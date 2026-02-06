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
#include "VoxelMetadata.h"
#include "VoxelFloatMetadata.h"
#include "VoxelLinearColorMetadata.h"
#include "VoxelNormalMetadata.h"
#include "EngineUtils.h"

// Metadata type enum for async task
enum class EMetadataType : uint8 { None, Float, LinearColor, Normal };

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
    BakeLayer(true, CloudMetadata, CloudTexture, CloudRadius, W, H);
}

void USphericalTextureBaker::ForceRebakeLand()
{
    if (!bEnableLandLayer || bIsBakingLand) return;
    CreateRT(LandTexture, LandRenderTarget, LandTextureWidth, LandTextureHeight);
    int32 W = LandRenderTarget ? LandRenderTarget->SizeX : LandTextureWidth;
    int32 H = LandRenderTarget ? LandRenderTarget->SizeY : LandTextureHeight;
    BakeLayer(false, LandMetadata, LandTexture, LandRadius, W, H);
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

void USphericalTextureBaker::BakeLayer(bool bCloud, UVoxelMetadata* Meta, UTextureRenderTarget2D* RT, float Radius, int32 W, int32 H)
{
    if (!GetWorld() || !VolumeLayer.IsValid() || !RT) return;
    if (bCloud) bIsBakingCloud = true; else bIsBakingLand = true;
    
    const int32 N = W * H;
    
    // Detect metadata type
    EMetadataType MetaType = EMetadataType::None;
    TOptional<FVoxelFloatMetadataRef> FloatRef;
    TOptional<FVoxelLinearColorMetadataRef> ColorRef;
    TOptional<FVoxelNormalMetadataRef> NormalRef;
    
    if (Meta)
    {
        if (auto* FloatMeta = Cast<UVoxelFloatMetadata>(Meta))
        {
            MetaType = EMetadataType::Float;
            FloatRef = FVoxelFloatMetadataRef(FloatMeta);
        }
        else if (auto* ColorMeta = Cast<UVoxelLinearColorMetadata>(Meta))
        {
            MetaType = EMetadataType::LinearColor;
            ColorRef = FVoxelLinearColorMetadataRef(ColorMeta);
        }
        else if (auto* NormalMeta = Cast<UVoxelNormalMetadata>(Meta))
        {
            MetaType = EMetadataType::Normal;
            NormalRef = FVoxelNormalMetadataRef(NormalMeta);
        }
    }
    
    FVoxelWeakStackLayer WL(VolumeLayer);
    TSharedPtr<FVoxelLayers> Layers = FVoxelLayers::Get(GetWorld());
    TSharedRef<FVoxelSurfaceTypeTable> STT = FVoxelSurfaceTypeTable::Get();
    if (!Layers) { if (bCloud) bIsBakingCloud = false; else bIsBakingLand = false; return; }
    
    TWeakObjectPtr<USphericalTextureBaker> WThis(this);
    TWeakObjectPtr<UTextureRenderTarget2D> WRT = RT;
    FVector Ctr = SphereCenter;
    bool bRemap = bRemapNegativeToPositive, bInv = bInvertResult, bNorm = bAutoNormalize;
    float Mult = ResultMultiplier;
    
    struct FBakeResult
    {
        TArray<FLinearColor> Colors;
        EMetadataType Type = EMetadataType::None;
    };
    
    Voxel::AsyncTask([WL, Layers, STT, W, H, Radius, Ctr, bRemap, bInv, bNorm, Mult, N, MetaType, FloatRef, ColorRef, NormalRef]() -> TVoxelFuture<FBakeResult>
    {
        VOXEL_FUNCTION_COUNTER();
        FBakeResult Result;
        Result.Type = MetaType;
        Result.Colors.SetNum(N);
        
        // Generate spherical positions
        FVoxelDoubleVectorBuffer Pos;
        Pos.Allocate(N);
        constexpr double Pi = 3.14159265358979323846, Pi2 = Pi * 2.0;
        
        for (int32 Y = 0; Y < H; Y++)
        {
            for (int32 X = 0; X < W; X++)
            {
                int32 I = Y * W + X;
                double U = double(X) / W, V = double(Y) / (H - 1);
                double Lon = U * Pi2 - Pi, Lat = V * Pi;
                double SLat = FMath::Sin(Lat), CLat = FMath::Cos(Lat);
                double SLon = FMath::Sin(Lon), CLon = FMath::Cos(Lon);
                Pos.X.Set(I, Radius * SLat * CLon + Ctr.X);
                Pos.Y.Set(I, Radius * SLat * SLon + Ctr.Y);
                Pos.Z.Set(I, Radius * CLat + Ctr.Z);
            }
        }
        
        FVoxelQuery Query(0, *Layers, *STT, FVoxelDependencyCollector::Null);
        
        if (MetaType == EMetadataType::Float && FloatRef.IsSet())
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
        else if (MetaType == EMetadataType::LinearColor && ColorRef.IsSet())
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
        else if (MetaType == EMetadataType::Normal && NormalRef.IsSet())
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
        
    }).Then_GameThread([WThis, WRT, W, H, bCloud](const FBakeResult& Result)
    {
        auto* This = WThis.Get();
        auto* RT = WRT.Get();
        if (!This || !RT) return;
        
        if (Result.Colors.Num() > 0)
        {
            This->WriteColor(RT, Result.Colors, W, H);
        }
        
        if (bCloud) { This->bIsBakingCloud = false; This->OnCloudBakeComplete.Broadcast(); }
        else { This->bIsBakingLand = false; This->OnLandBakeComplete.Broadcast(); }
    });
}

void USphericalTextureBaker::WriteGrayscale(UTextureRenderTarget2D* RT, const TArray<float>& V, int32 W, int32 H)
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

void USphericalTextureBaker::WriteColor(UTextureRenderTarget2D* RT, const TArray<FLinearColor>& C, int32 W, int32 H)
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
