// Copyright Epic Games, Inc. All Rights Reserved.

#include "SphericalTextureBaker.h"
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

USphericalTextureBaker::USphericalTextureBaker() { PrimaryComponentTick.bCanEverTick = false; }

void USphericalTextureBaker::BeginPlay()
{
    Super::BeginPlay();
    if (bBakeOnBeginPlay) ForceRebake();
}

void USphericalTextureBaker::RequestGlobalRebake(UObject* Ctx)
{
    if (!Ctx) return;
    UWorld* W = GEngine->GetWorldFromContextObject(Ctx, EGetWorldErrorMode::LogAndReturnNull);
    if (!W) return;
    for (TActorIterator<AActor> It(W); It; ++It)
    {
        TArray<USphericalTextureBaker*> B;
        (*It)->GetComponents<USphericalTextureBaker>(B);
        for (auto* Baker : B) if (Baker && !Baker->IsBaking()) Baker->ForceRebake();
    }
}

void USphericalTextureBaker::ForceRebake()
{
    if (bEnableCloudLayer) ForceRebakeCloud();
    if (bEnableLandLayer) ForceRebakeLand();
}

void USphericalTextureBaker::ForceRebakeCloud()
{
    if (!bEnableCloudLayer || bIsBakingCloud) return;
    CreateRT(CloudTexture, CloudRenderTarget, CloudTextureWidth, CloudTextureHeight);
    int32 W = CloudRenderTarget ? CloudRenderTarget->SizeX : CloudTextureWidth;
    int32 H = CloudRenderTarget ? CloudRenderTarget->SizeY : CloudTextureHeight;
    BakeLayer(true, CloudColorMetadata, CloudTexture, CloudRadius, W, H);
}

void USphericalTextureBaker::ForceRebakeLand()
{
    if (!bEnableLandLayer || bIsBakingLand) return;
    CreateRT(LandTexture, LandRenderTarget, LandTextureWidth, LandTextureHeight);
    int32 W = LandRenderTarget ? LandRenderTarget->SizeX : LandTextureWidth;
    int32 H = LandRenderTarget ? LandRenderTarget->SizeY : LandTextureHeight;
    BakeLayer(false, LandColorMetadata, LandTexture, LandRadius, W, H);
}

void USphericalTextureBaker::CreateRT(TObjectPtr<UTextureRenderTarget2D>& Out, UTextureRenderTarget2D* Ext, int32 W, int32 H)
{
    if (Ext) { Out = Ext; return; }
    if (Out) return;
    Out = NewObject<UTextureRenderTarget2D>(this);
    Out->RenderTargetFormat = bUseHDR ? RTF_RGBA16f : RTF_RGBA8;
    Out->InitAutoFormat(W, H);
    Out->UpdateResourceImmediate(true);
}

void USphericalTextureBaker::BakeLayer(bool bCloud, UVoxelLinearColorMetadata* Meta, UTextureRenderTarget2D* RT, float Radius, int32 W, int32 H)
{
    if (!GetWorld() || !VolumeLayer.IsValid() || !RT) return;
    if (bCloud) bIsBakingCloud = true; else bIsBakingLand = true;
    
    const int32 N = W * H;
    const bool bColor = Meta != nullptr;
    
    FVoxelWeakStackLayer WL(VolumeLayer);
    TSharedPtr<FVoxelLayers> Layers = FVoxelLayers::Get(GetWorld());
    TSharedRef<FVoxelSurfaceTypeTable> STT = FVoxelSurfaceTypeTable::Get();
    if (!Layers) { if (bCloud) bIsBakingCloud = false; else bIsBakingLand = false; return; }
    
    TWeakObjectPtr<USphericalTextureBaker> WThis(this);
    TWeakObjectPtr<UTextureRenderTarget2D> WRT = RT;
    FVector Ctr = SphereCenter;
    bool bRemap = bRemapNegativeToPositive, bInv = bInvertResult, bNorm = bAutoNormalize;
    float Mult = ResultMultiplier;
    
    TOptional<FVoxelLinearColorMetadataRef> CR;
    if (bColor && Meta) CR = FVoxelLinearColorMetadataRef(Meta);
    
    struct FR { TArray<FLinearColor> C; TArray<float> V; bool bC = false; };
    
    Voxel::AsyncTask([WL, Layers, STT, W, H, Radius, Ctr, bRemap, bInv, bNorm, Mult, N, bColor, CR]() -> TVoxelFuture<FR>
    {
        VOXEL_FUNCTION_COUNTER();
        FR R; R.bC = bColor;
        FVoxelDoubleVectorBuffer P; P.Allocate(N);
        constexpr double Pi = 3.14159265358979323846, Pi2 = Pi * 2.0;
        
        for (int32 Y = 0; Y < H; Y++)
            for (int32 X = 0; X < W; X++)
            {
                int32 I = Y * W + X;
                double U = double(X) / W, V = double(Y) / (H - 1);
                double Lon = U * Pi2 - Pi, Lat = V * Pi;
                double SLat = FMath::Sin(Lat), CLat = FMath::Cos(Lat);
                double SLon = FMath::Sin(Lon), CLon = FMath::Cos(Lon);
                P.X.Set(I, Radius * SLat * CLon + Ctr.X);
                P.Y.Set(I, Radius * SLat * SLon + Ctr.Y);
                P.Z.Set(I, Radius * CLat + Ctr.Z);
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
    }).Then_GameThread([WThis, WRT, W, H, bCloud](const FR& R)
    {
        auto* T = WThis.Get(); auto* RT = WRT.Get();
        if (!T || !RT) return;
        if (R.bC && R.C.Num() > 0) T->WriteColor(RT, R.C, W, H);
        else if (R.V.Num() > 0) T->WriteGrayscale(RT, R.V, W, H);
        if (bCloud) { T->bIsBakingCloud = false; T->OnCloudBakeComplete.Broadcast(); }
        else { T->bIsBakingLand = false; T->OnLandBakeComplete.Broadcast(); }
    });
}

void USphericalTextureBaker::WriteGrayscale(UTextureRenderTarget2D* RT, const TArray<float>& V, int32 W, int32 H)
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

void USphericalTextureBaker::WriteColor(UTextureRenderTarget2D* RT, const TArray<FLinearColor>& C, int32 W, int32 H)
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
