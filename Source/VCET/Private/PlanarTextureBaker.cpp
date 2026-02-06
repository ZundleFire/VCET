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
#include "VoxelMetadata.h"
#include "VoxelFloatMetadata.h"
#include "VoxelLinearColorMetadata.h"
#include "VoxelNormalMetadata.h"
#include "EngineUtils.h"

// Metadata type enum for async task
enum class EPlanarMetadataType : uint8 { None, Float, LinearColor, Normal };

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
    BakeLayer(true, PrimaryMetadata, PrimaryTexture, PrimaryHeight, W, H);
}

void UPlanarTextureBaker::ForceRebakeSecondary()
{
    if (!bEnableSecondaryLayer || bIsBakingSecondary) return;
    CreateRT(SecondaryTexture, SecondaryRenderTarget, SecondaryTextureWidth, SecondaryTextureHeight);
    int32 W = SecondaryRenderTarget ? SecondaryRenderTarget->SizeX : SecondaryTextureWidth;
    int32 H = SecondaryRenderTarget ? SecondaryRenderTarget->SizeY : SecondaryTextureHeight;
    BakeLayer(false, SecondaryMetadata, SecondaryTexture, SecondaryHeight, W, H);
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

void UPlanarTextureBaker::BakeLayer(bool bPrimary, UVoxelMetadata* Meta, UTextureRenderTarget2D* RT, float SampleZ, int32 W, int32 H)
{
    if (!GetWorld() || !VolumeLayer.IsValid() || !RT) return;
    if (bPrimary) bIsBakingPrimary = true; else bIsBakingSecondary = true;
    
    const int32 N = W * H;
    
    // Detect metadata type
    EPlanarMetadataType MetaType = EPlanarMetadataType::None;
    TOptional<FVoxelFloatMetadataRef> FloatRef;
    TOptional<FVoxelLinearColorMetadataRef> ColorRef;
    TOptional<FVoxelNormalMetadataRef> NormalRef;
    
    if (Meta)
    {
        if (auto* FloatMeta = Cast<UVoxelFloatMetadata>(Meta))
        {
            MetaType = EPlanarMetadataType::Float;
            FloatRef = FVoxelFloatMetadataRef(FloatMeta);
        }
        else if (auto* ColorMeta = Cast<UVoxelLinearColorMetadata>(Meta))
        {
            MetaType = EPlanarMetadataType::LinearColor;
            ColorRef = FVoxelLinearColorMetadataRef(ColorMeta);
        }
        else if (auto* NormalMeta = Cast<UVoxelNormalMetadata>(Meta))
        {
            MetaType = EPlanarMetadataType::Normal;
            NormalRef = FVoxelNormalMetadataRef(NormalMeta);
        }
    }
    
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
    
    struct FBakeResult
    {
        TArray<FLinearColor> Colors;
        EPlanarMetadataType Type = EPlanarMetadataType::None;
    };
    
    Voxel::AsyncTask([WL, Layers, STT, W, H, SampleZ, Ctr, Sz, bRemap, bInv, bNorm, Mult, N, MetaType, FloatRef, ColorRef, NormalRef]() -> TVoxelFuture<FBakeResult>
    {
        VOXEL_FUNCTION_COUNTER();
        FBakeResult Result;
        Result.Type = MetaType;
        Result.Colors.SetNum(N);
        
        // Generate planar positions
        FVoxelDoubleVectorBuffer Pos;
        Pos.Allocate(N);
        
        double MinX = Ctr.X - Sz.X * 0.5, MaxX = Ctr.X + Sz.X * 0.5;
        double MinY = Ctr.Y - Sz.Y * 0.5, MaxY = Ctr.Y + Sz.Y * 0.5;
        
        for (int32 Y = 0; Y < H; Y++)
        {
            for (int32 X = 0; X < W; X++)
            {
                int32 I = Y * W + X;
                double U = double(X) / (W - 1), V = double(Y) / (H - 1);
                Pos.X.Set(I, FMath::Lerp(MinX, MaxX, U));
                Pos.Y.Set(I, FMath::Lerp(MinY, MaxY, V));
                Pos.Z.Set(I, SampleZ + Ctr.Z);
            }
        }
        
        FVoxelQuery Query(0, *Layers, *STT, FVoxelDependencyCollector::Null);
        
        if (MetaType == EPlanarMetadataType::Float && FloatRef.IsSet())
        {
            // Float metadata ? R channel only
            TVoxelMap<FVoxelMetadataRef, TSharedRef<FVoxelBuffer>> MetaBuffers;
            const FVoxelMetadataRef& Ref = FloatRef.GetValue();
            if (Ref.IsValid()) MetaBuffers.Add_EnsureNew(Ref, Ref.MakeDefaultBuffer(N));
            Query.SampleVolumeLayer(WL, Pos, {}, MetaBuffers);
            
            if (Ref.IsValid())
            {
                if (auto* Buf = MetaBuffers.Find(Ref))
                {
                    if (auto* FB = static_cast<const FVoxelFloatBuffer*>(&Buf->Get()))
                    {
                        for (int32 i = 0; i < FMath::Min(FB->Num(), N); i++)
                        {
                            float Val = (*FB)[i];
                            if (bRemap) Val = (Val + 1.f) * 0.5f;
                            Val *= Mult;
                            if (bInv) Val = 1.f - Val;
                            Result.Colors[i] = FLinearColor(Val, 0.f, 0.f, 1.f); // R channel only
                        }
                    }
                }
            }
        }
        else if (MetaType == EPlanarMetadataType::LinearColor && ColorRef.IsSet())
        {
            // Linear Color metadata ? RGBA channels
            TVoxelMap<FVoxelMetadataRef, TSharedRef<FVoxelBuffer>> MetaBuffers;
            const FVoxelMetadataRef& Ref = ColorRef.GetValue();
            if (Ref.IsValid()) MetaBuffers.Add_EnsureNew(Ref, Ref.MakeDefaultBuffer(N));
            Query.SampleVolumeLayer(WL, Pos, {}, MetaBuffers);
            
            if (Ref.IsValid())
            {
                if (auto* Buf = MetaBuffers.Find(Ref))
                {
                    if (auto* CB = static_cast<const FVoxelLinearColorBuffer*>(&Buf->Get()))
                    {
                        for (int32 i = 0; i < FMath::Min(CB->Num(), N); i++)
                        {
                            Result.Colors[i] = (*CB)[i]; // All RGBA channels
                        }
                    }
                }
            }
        }
        else if (MetaType == EPlanarMetadataType::Normal && NormalRef.IsSet())
        {
            // Normal metadata ? RGB channels (no alpha modification)
            TVoxelMap<FVoxelMetadataRef, TSharedRef<FVoxelBuffer>> MetaBuffers;
            const FVoxelMetadataRef& Ref = NormalRef.GetValue();
            if (Ref.IsValid()) MetaBuffers.Add_EnsureNew(Ref, Ref.MakeDefaultBuffer(N));
            Query.SampleVolumeLayer(WL, Pos, {}, MetaBuffers);
            
            if (Ref.IsValid())
            {
                if (auto* Buf = MetaBuffers.Find(Ref))
                {
                    if (auto* NB = static_cast<const FVoxelVectorBuffer*>(&Buf->Get()))
                    {
                        for (int32 i = 0; i < FMath::Min(NB->Num(), N); i++)
                        {
                            FVector3f Normal = (*NB)[i];
                            // Remap normal from [-1,1] to [0,1] for texture storage
                            Result.Colors[i] = FLinearColor(
                                Normal.X * 0.5f + 0.5f,
                                Normal.Y * 0.5f + 0.5f,
                                Normal.Z * 0.5f + 0.5f,
                                1.f
                            );
                        }
                    }
                }
            }
        }
        else
        {
            // No metadata - sample distance field as grayscale
            auto Dist = Query.SampleVolumeLayer(WL, Pos);
            float MinV = FLT_MAX, MaxV = -FLT_MAX;
            
            for (int32 i = 0; i < N; i++)
            {
                float Val = Dist[i];
                if (bRemap) Val = (Val + 1.f) * 0.5f;
                Val *= Mult;
                if (bInv) Val = 1.f - Val;
                Result.Colors[i] = FLinearColor(Val, Val, Val, 1.f);
                MinV = FMath::Min(MinV, Val);
                MaxV = FMath::Max(MaxV, Val);
            }
            
            // Auto-normalize grayscale
            if (bNorm && MaxV > MinV)
            {
                float Range = MaxV - MinV;
                for (int32 i = 0; i < N; i++)
                {
                    float Val = (Result.Colors[i].R - MinV) / Range;
                    Result.Colors[i] = FLinearColor(Val, Val, Val, 1.f);
                }
            }
            else
            {
                for (int32 i = 0; i < N; i++)
                {
                    float Val = FMath::Clamp(Result.Colors[i].R, 0.f, 1.f);
                    Result.Colors[i] = FLinearColor(Val, Val, Val, 1.f);
                }
            }
        }
        
        return Result;
        
    }).Then_GameThread([WThis, WRT, W, H, bPrimary](const FBakeResult& Result)
    {
        auto* This = WThis.Get();
        auto* RT = WRT.Get();
        if (!This || !RT) return;
        
        if (Result.Colors.Num() > 0)
        {
            This->WriteColor(RT, Result.Colors, W, H);
        }
        
        if (bPrimary) { This->bIsBakingPrimary = false; This->OnPrimaryBakeComplete.Broadcast(); }
        else { This->bIsBakingSecondary = false; This->OnSecondaryBakeComplete.Broadcast(); }
    });
}

void UPlanarTextureBaker::WriteGrayscale(UTextureRenderTarget2D* RT, const TArray<float>& V, int32 W, int32 H)
{
    if (!RT || V.Num() != W * H) return;
    TArray<FColor> Px;
    Px.SetNum(W * H);
    for (int32 i = 0; i < V.Num(); i++)
    {
        uint8 B = uint8(FMath::Clamp(V[i], 0.f, 1.f) * 255.f);
        Px[i] = FColor(B, B, B, 255);
    }
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
    TArray<FColor> Px;
    Px.SetNum(W * H);
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
