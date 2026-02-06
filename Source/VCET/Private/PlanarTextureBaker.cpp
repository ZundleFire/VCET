// Copyright Epic Games, Inc. All Rights Reserved.

#include "PlanarTextureBaker.h"
#include "Engine/TextureRenderTarget2D.h"
#include "TextureResource.h"
#include "RenderingThread.h"
#include "VoxelQuery.h"
#include "VoxelLayers.h"
#include "Surface/VoxelSurfaceTypeTable.h"
#include "Buffer/VoxelDoubleBuffers.h"
#include "Buffer/VoxelFloatBuffers.h"
#include "VoxelLinearColorMetadata.h"
#include "EngineUtils.h"

UPlanarTextureBaker::UPlanarTextureBaker() { PrimaryComponentTick.bCanEverTick = false; }

void UPlanarTextureBaker::BeginPlay()
{
    Super::BeginPlay();
    if (bBakeOnBeginPlay) ForceRebake();
}

void UPlanarTextureBaker::RequestGlobalRebake(UObject* Ctx)
{
    if (!Ctx) return;
    UWorld* W = GEngine->GetWorldFromContextObject(Ctx, EGetWorldErrorMode::LogAndReturnNull);
    if (!W) return;
    for (TActorIterator<AActor> It(W); It; ++It)
    {
        TArray<UPlanarTextureBaker*> B;
        (*It)->GetComponents<UPlanarTextureBaker>(B);
        for (auto* Baker : B) if (Baker && !Baker->IsBaking()) Baker->ForceRebake();
    }
}

void UPlanarTextureBaker::ForceRebake()
{
    if (bEnablePrimaryLayer) ForceRebakePrimary();
    if (bEnableSecondaryLayer) ForceRebakeSecondary();
}

void UPlanarTextureBaker::ForceRebakePrimary()
{
    if (!bEnablePrimaryLayer || bIsBakingPrimary) return;
    CreateRT(PrimaryTexture, PrimaryRenderTarget, PrimaryTextureWidth, PrimaryTextureHeight);
    int32 W = PrimaryRenderTarget ? PrimaryRenderTarget->SizeX : PrimaryTextureWidth;
    int32 H = PrimaryRenderTarget ? PrimaryRenderTarget->SizeY : PrimaryTextureHeight;
    BakeLayer(true, PrimaryColorMetadata, PrimaryTexture, PrimaryHeight, W, H);
}

void UPlanarTextureBaker::ForceRebakeSecondary()
{
    if (!bEnableSecondaryLayer || bIsBakingSecondary) return;
    CreateRT(SecondaryTexture, SecondaryRenderTarget, SecondaryTextureWidth, SecondaryTextureHeight);
    int32 W = SecondaryRenderTarget ? SecondaryRenderTarget->SizeX : SecondaryTextureWidth;
    int32 H = SecondaryRenderTarget ? SecondaryRenderTarget->SizeY : SecondaryTextureHeight;
    BakeLayer(false, SecondaryColorMetadata, SecondaryTexture, SecondaryHeight, W, H);
}

void UPlanarTextureBaker::CreateRT(TObjectPtr<UTextureRenderTarget2D>& Out, UTextureRenderTarget2D* Ext, int32 W, int32 H)
{
    if (Ext) { Out = Ext; return; }
    if (Out) return;
    Out = NewObject<UTextureRenderTarget2D>(this);
    Out->RenderTargetFormat = bUseHDR ? RTF_RGBA16f : RTF_RGBA8;
    Out->InitAutoFormat(W, H);
    Out->UpdateResourceImmediate(true);
}

void UPlanarTextureBaker::BakeLayer(bool bPrimary, UVoxelLinearColorMetadata* Meta, UTextureRenderTarget2D* RT, float SampleZ, int32 W, int32 H)
{
    if (!GetWorld() || !VolumeLayer.IsValid() || !RT) return;
    if (bPrimary) bIsBakingPrimary = true; else bIsBakingSecondary = true;
    
    const int32 N = W * H;
    const bool bColor = Meta != nullptr;
    
    FVoxelWeakStackLayer WL(VolumeLayer);
    TSharedPtr<FVoxelLayers> Layers = FVoxelLayers::Get(GetWorld());
    TSharedRef<FVoxelSurfaceTypeTable> STT = FVoxelSurfaceTypeTable::Get();
    if (!Layers) { if (bPrimary) bIsBakingPrimary = false; else bIsBakingSecondary = false; return; }
    
    TWeakObjectPtr<UPlanarTextureBaker> WThis(this);
    TWeakObjectPtr<UTextureRenderTarget2D> WRT = RT;
    FVector Ctr = WorldCenter;
    FVector2D Sz = WorldSize;
    bool bRemap = bRemapNegativeToPositive, bInv = bInvertResult, bNorm = bAutoNormalize;
    float Mult = ResultMultiplier;
    
    TOptional<FVoxelLinearColorMetadataRef> CR;
    if (bColor && Meta) CR = FVoxelLinearColorMetadataRef(Meta);
    
    struct FR { TArray<FLinearColor> C; TArray<float> V; bool bC = false; };
    
    Voxel::AsyncTask([WL, Layers, STT, W, H, SampleZ, Ctr, Sz, bRemap, bInv, bNorm, Mult, N, bColor, CR]() -> TVoxelFuture<FR>
    {
        VOXEL_FUNCTION_COUNTER();
        FR R; R.bC = bColor;
        FVoxelDoubleVectorBuffer P; P.Allocate(N);
        
        double MinX = Ctr.X - Sz.X * 0.5, MaxX = Ctr.X + Sz.X * 0.5;
        double MinY = Ctr.Y - Sz.Y * 0.5, MaxY = Ctr.Y + Sz.Y * 0.5;
        
        for (int32 Y = 0; Y < H; Y++)
            for (int32 X = 0; X < W; X++)
            {
                int32 I = Y * W + X;
                double U = double(X) / (W - 1), V = double(Y) / (H - 1);
                P.X.Set(I, FMath::Lerp(MinX, MaxX, U));
                P.Y.Set(I, FMath::Lerp(MinY, MaxY, V));
                P.Z.Set(I, SampleZ + Ctr.Z);
            }
        
        FVoxelQuery Q(0, *Layers, *STT, FVoxelDependencyCollector::Null);
        
        if (bColor && CR.IsSet())
        {
            TVoxelMap<FVoxelMetadataRef, TSharedRef<FVoxelBuffer>> MB;
            const auto& Ref = CR.GetValue();
            if (Ref.IsValid()) MB.Add_EnsureNew(Ref, Ref.MakeDefaultBuffer(N));
            Q.SampleVolumeLayer(WL, P, {}, MB);
            R.C.SetNum(N);
            if (Ref.IsValid())
                if (auto* B = MB.Find(Ref))
                    if (auto* CB = static_cast<const FVoxelLinearColorBuffer*>(&B->Get()))
                        if (CB->Num() > 0)
                            for (int32 i = 0; i < FMath::Min(CB->Num(), N); i++) R.C[i] = (*CB)[i];
        }
        else
        {
            auto D = Q.SampleVolumeLayer(WL, P);
            R.V.SetNum(N);
            float Mi = FLT_MAX, Ma = -FLT_MAX;
            for (int32 i = 0; i < N; i++)
            {
                float v = D[i];
                if (bRemap) v = (v + 1.f) * 0.5f;
                v *= Mult;
                if (bInv) v = 1.f - v;
                R.V[i] = v;
                Mi = FMath::Min(Mi, v); Ma = FMath::Max(Ma, v);
            }
            if (bNorm && Ma > Mi) { float Rng = Ma - Mi; for (int32 i = 0; i < N; i++) R.V[i] = (R.V[i] - Mi) / Rng; }
            else for (int32 i = 0; i < N; i++) R.V[i] = FMath::Clamp(R.V[i], 0.f, 1.f);
        }
        return R;
    }).Then_GameThread([WThis, WRT, W, H, bPrimary](const FR& R)
    {
        auto* T = WThis.Get(); auto* RT = WRT.Get();
        if (!T || !RT) return;
        if (R.bC && R.C.Num() > 0) T->WriteColor(RT, R.C, W, H);
        else if (R.V.Num() > 0) T->WriteGrayscale(RT, R.V, W, H);
        if (bPrimary) { T->bIsBakingPrimary = false; T->OnPrimaryBakeComplete.Broadcast(); }
        else { T->bIsBakingSecondary = false; T->OnSecondaryBakeComplete.Broadcast(); }
    });
}

void UPlanarTextureBaker::WriteGrayscale(UTextureRenderTarget2D* RT, const TArray<float>& V, int32 W, int32 H)
{
    if (!RT || V.Num() != W * H) return;
    TArray<FColor> Px; Px.SetNum(W * H);
    for (int32 i = 0; i < V.Num(); i++) { uint8 B = uint8(FMath::Clamp(V[i], 0.f, 1.f) * 255.f); Px[i] = FColor(B, B, B, 255); }
    auto Data = MakeShared<TArray<FColor>>(MoveTemp(Px));
    TWeakObjectPtr<UTextureRenderTarget2D> WRT = RT;
    AsyncTask(ENamedThreads::GameThread, [WRT, Data, W, H]()
    {
        if (auto* R = WRT.Get(); R && IsValid(R))
        {
            R->UpdateResourceImmediate(true);
            if (auto* Res = R->GameThread_GetRenderTargetResource())
            {
                FUpdateTextureRegion2D Rgn(0, 0, 0, 0, W, H);
                ENQUEUE_RENDER_COMMAND(Write)([Res, Data, W, H, Rgn](FRHICommandListImmediate&)
                {
                    FRHITexture* Tex = Res->GetRenderTargetTexture();
                    if (Tex)
                    {
                        PRAGMA_DISABLE_DEPRECATION_WARNINGS
                        RHIUpdateTexture2D(Tex, 0, Rgn, W * sizeof(FColor), reinterpret_cast<const uint8*>(Data->GetData()));
                        PRAGMA_ENABLE_DEPRECATION_WARNINGS
                    }
                });
            }
        }
    });
}

void UPlanarTextureBaker::WriteColor(UTextureRenderTarget2D* RT, const TArray<FLinearColor>& C, int32 W, int32 H)
{
    if (!RT || C.Num() != W * H) return;
    TArray<FColor> Px; Px.SetNum(W * H);
    for (int32 i = 0; i < C.Num(); i++) Px[i] = C[i].ToFColor(false);
    auto Data = MakeShared<TArray<FColor>>(MoveTemp(Px));
    TWeakObjectPtr<UTextureRenderTarget2D> WRT = RT;
    AsyncTask(ENamedThreads::GameThread, [WRT, Data, W, H]()
    {
        if (auto* R = WRT.Get(); R && IsValid(R))
        {
            R->UpdateResourceImmediate(true);
            if (auto* Res = R->GameThread_GetRenderTargetResource())
            {
                FUpdateTextureRegion2D Rgn(0, 0, 0, 0, W, H);
                ENQUEUE_RENDER_COMMAND(WriteC)([Res, Data, W, H, Rgn](FRHICommandListImmediate&)
                {
                    FRHITexture* Tex = Res->GetRenderTargetTexture();
                    if (Tex)
                    {
                        PRAGMA_DISABLE_DEPRECATION_WARNINGS
                        RHIUpdateTexture2D(Tex, 0, Rgn, W * sizeof(FColor), reinterpret_cast<const uint8*>(Data->GetData()));
                        PRAGMA_ENABLE_DEPRECATION_WARNINGS
                    }
                });
            }
        }
    });
}
