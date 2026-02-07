// Copyright Epic Games, Inc. All Rights Reserved.

#include "VolumeTextureBaker.h"
#include "Engine/TextureRenderTargetVolume.h"
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
#include "EngineUtils.h"

// Metadata type enum for async task
enum class EVolumeMetadataType : uint8 { None, Float, LinearColor };

UVolumeTextureBaker::UVolumeTextureBaker()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UVolumeTextureBaker::BeginPlay()
{
    Super::BeginPlay();
    if (bBakeOnBeginPlay)
    {
        ForceRebake();
    }
}

void UVolumeTextureBaker::RequestGlobalRebake(UObject* WorldContextObject)
{
    if (!WorldContextObject) return;
    UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
    if (!World) return;
    
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        TArray<UVolumeTextureBaker*> Bakers;
        (*It)->GetComponents<UVolumeTextureBaker>(Bakers);
        for (auto* Baker : Bakers)
        {
            if (Baker && !Baker->IsBaking())
            {
                Baker->ForceRebake();
            }
        }
    }
}

void UVolumeTextureBaker::ForceRebake()
{
    if (bIsBaking) return;
    CreateVolumeRT();
    BakeVolume();
}

void UVolumeTextureBaker::CreateVolumeRT()
{
    // Use external RT if provided
    if (VolumeRenderTarget)
    {
        VolumeTexture = VolumeRenderTarget;
        return;
    }
    
    // Create new volume RT if needed
    if (!VolumeTexture || 
        VolumeTexture->SizeX != VolumeResolutionX || 
        VolumeTexture->SizeY != VolumeResolutionY)
    {
        VolumeTexture = NewObject<UTextureRenderTargetVolume>(this);
        VolumeTexture->Init(VolumeResolutionX, VolumeResolutionY, VolumeResolutionZ, 
                           bUseHDR ? PF_FloatRGBA : PF_B8G8R8A8);
        VolumeTexture->UpdateResourceImmediate(true);
    }
}

void UVolumeTextureBaker::BakeVolume()
{
    if (!GetWorld() || !VolumeLayer.IsValid() || !VolumeTexture)
    {
        return;
    }
    
    bIsBaking = true;
    
    const int32 SizeX = VolumeResolutionX;
    const int32 SizeY = VolumeResolutionY;
    const int32 SizeZ = VolumeResolutionZ;
    const int32 TotalVoxels = SizeX * SizeY * SizeZ;
    
    // Detect metadata type
    EVolumeMetadataType MetaType = EVolumeMetadataType::None;
    TOptional<FVoxelFloatMetadataRef> FloatRef;
    TOptional<FVoxelLinearColorMetadataRef> ColorRef;
    
    if (Metadata)
    {
        if (auto* FloatMeta = Cast<UVoxelFloatMetadata>(Metadata))
        {
            MetaType = EVolumeMetadataType::Float;
            FloatRef = FVoxelFloatMetadataRef(FloatMeta);
        }
        else if (auto* ColorMeta = Cast<UVoxelLinearColorMetadata>(Metadata))
        {
            MetaType = EVolumeMetadataType::LinearColor;
            ColorRef = FVoxelLinearColorMetadataRef(ColorMeta);
        }
    }
    
    FVoxelWeakStackLayer WeakLayer(VolumeLayer);
    TSharedPtr<FVoxelLayers> Layers = FVoxelLayers::Get(GetWorld());
    TSharedRef<FVoxelSurfaceTypeTable> STT = FVoxelSurfaceTypeTable::Get();
    
    if (!Layers)
    {
        bIsBaking = false;
        return;
    }
    
    // Capture parameters for async task
    TWeakObjectPtr<UVolumeTextureBaker> WeakThis(this);
    
    const bool bSpherical = bUseSphericalRegion;
    const FVector BoxCtr = BoxCenter;
    const FVector BoxExt = BoxExtent;
    const FVector SphCtr = SphereCenter;
    const float RadiusInner = InnerRadius;
    const float RadiusOuter = OuterRadius;
    const bool bRemap = bRemapNegativeToPositive;
    const bool bNorm = bAutoNormalize;
    const bool bInvert = bInvertResult;
    const float Mult = ResultMultiplier;
    
    struct FBakeResult
    {
        TArray<FLinearColor> VoxelData;
    };
    
    Voxel::AsyncTask([WeakLayer, Layers, STT, SizeX, SizeY, SizeZ, TotalVoxels,
                     bSpherical, BoxCtr, BoxExt, SphCtr, RadiusInner, RadiusOuter,
                     bRemap, bNorm, bInvert, Mult,
                     MetaType, FloatRef, ColorRef]() -> TVoxelFuture<FBakeResult>
    {
        VOXEL_FUNCTION_COUNTER();
        FBakeResult Result;
        Result.VoxelData.SetNum(TotalVoxels);
        
        // Generate 3D sample positions
        FVoxelDoubleVectorBuffer Positions;
        Positions.Allocate(TotalVoxels);
        
        for (int32 Z = 0; Z < SizeZ; Z++)
        {
            for (int32 Y = 0; Y < SizeY; Y++)
            {
                for (int32 X = 0; X < SizeX; X++)
                {
                    const int32 Index = X + Y * SizeX + Z * SizeX * SizeY;
                    
                    // Normalized UVW coordinates (0-1)
                    const double U = double(X) / double(SizeX - 1);
                    const double V = double(Y) / double(SizeY - 1);
                    const double W = double(Z) / double(SizeZ - 1);
                    
                    double WorldX, WorldY, WorldZ;
                    
                    if (bSpherical)
                    {
                        // Spherical shell: W = radius blend, UV = spherical coords
                        const double Radius = FMath::Lerp(RadiusInner, RadiusOuter, W);
                        const double Theta = U * 2.0 * PI; // Longitude (0 to 2PI)
                        const double Phi = V * PI;          // Latitude (0 to PI)
                        
                        const double SinPhi = FMath::Sin(Phi);
                        WorldX = SphCtr.X + Radius * SinPhi * FMath::Cos(Theta);
                        WorldY = SphCtr.Y + Radius * SinPhi * FMath::Sin(Theta);
                        WorldZ = SphCtr.Z + Radius * FMath::Cos(Phi);
                    }
                    else
                    {
                        // Box region: Simple linear interpolation
                        const FVector MinCorner = BoxCtr - BoxExt * 0.5;
                        const FVector MaxCorner = BoxCtr + BoxExt * 0.5;
                        
                        WorldX = FMath::Lerp(MinCorner.X, MaxCorner.X, U);
                        WorldY = FMath::Lerp(MinCorner.Y, MaxCorner.Y, V);
                        WorldZ = FMath::Lerp(MinCorner.Z, MaxCorner.Z, W);
                    }
                    
                    Positions.X.Set(Index, WorldX);
                    Positions.Y.Set(Index, WorldY);
                    Positions.Z.Set(Index, WorldZ);
                }
            }
        }
        
        // Query voxel data
        FVoxelQuery Query(0, *Layers, *STT, FVoxelDependencyCollector::Null);
        
        if (MetaType == EVolumeMetadataType::Float && FloatRef.IsSet())
        {
            TVoxelMap<FVoxelMetadataRef, TSharedRef<FVoxelBuffer>> MetaBuffers;
            const FVoxelMetadataRef& Ref = FloatRef.GetValue();
            if (Ref.IsValid())
            {
                MetaBuffers.Add_EnsureNew(Ref, Ref.MakeDefaultBuffer(TotalVoxels));
            }
            Query.SampleVolumeLayer(WeakLayer, Positions, {}, MetaBuffers);
            
            if (Ref.IsValid())
            {
                if (auto* Buf = MetaBuffers.Find(Ref))
                {
                    if (auto* FB = static_cast<const FVoxelFloatBuffer*>(&Buf->Get()))
                    {
                        for (int32 i = 0; i < FMath::Min(FB->Num(), TotalVoxels); i++)
                        {
                            float Val = (*FB)[i];
                            if (bRemap) Val = (Val + 1.f) * 0.5f;
                            Val *= Mult;
                            if (bInvert) Val = 1.f - Val;
                            Result.VoxelData[i] = FLinearColor(Val, Val, Val, 1.f);
                        }
                    }
                }
            }
        }
        else if (MetaType == EVolumeMetadataType::LinearColor && ColorRef.IsSet())
        {
            TVoxelMap<FVoxelMetadataRef, TSharedRef<FVoxelBuffer>> MetaBuffers;
            const FVoxelMetadataRef& Ref = ColorRef.GetValue();
            if (Ref.IsValid())
            {
                MetaBuffers.Add_EnsureNew(Ref, Ref.MakeDefaultBuffer(TotalVoxels));
            }
            Query.SampleVolumeLayer(WeakLayer, Positions, {}, MetaBuffers);
            
            if (Ref.IsValid())
            {
                if (auto* Buf = MetaBuffers.Find(Ref))
                {
                    if (auto* CB = static_cast<const FVoxelLinearColorBuffer*>(&Buf->Get()))
                    {
                        for (int32 i = 0; i < FMath::Min(CB->Num(), TotalVoxels); i++)
                        {
                            Result.VoxelData[i] = (*CB)[i];
                        }
                    }
                }
            }
        }
        else
        {
            // No metadata - sample distance field
            auto Dist = Query.SampleVolumeLayer(WeakLayer, Positions);
            float MinV = FLT_MAX, MaxV = -FLT_MAX;
            
            for (int32 i = 0; i < TotalVoxels; i++)
            {
                float Val = Dist[i];
                if (bRemap) Val = (Val + 1.f) * 0.5f;
                Val *= Mult;
                if (bInvert) Val = 1.f - Val;
                Result.VoxelData[i] = FLinearColor(Val, Val, Val, 1.f);
                MinV = FMath::Min(MinV, Val);
                MaxV = FMath::Max(MaxV, Val);
            }
            
            // Auto-normalize
            if (bNorm && MaxV > MinV)
            {
                const float Range = MaxV - MinV;
                for (int32 i = 0; i < TotalVoxels; i++)
                {
                    float Val = (Result.VoxelData[i].R - MinV) / Range;
                    Result.VoxelData[i] = FLinearColor(Val, Val, Val, 1.f);
                }
            }
        }
        
        return Result;
        
    }).Then_GameThread([WeakThis](const FBakeResult& Result)
    {
        UVolumeTextureBaker* This = WeakThis.Get();
        if (!This) return;
        
        if (Result.VoxelData.Num() > 0)
        {
            This->WriteToVolumeRT(Result.VoxelData);
        }
        
        This->bIsBaking = false;
        This->OnBakeComplete.Broadcast();
    });
}

void UVolumeTextureBaker::WriteToVolumeRT(const TArray<FLinearColor>& VoxelData)
{
    if (!VolumeTexture || VoxelData.Num() == 0) return;
    
    const int32 SizeX = VolumeResolutionX;
    const int32 SizeY = VolumeResolutionY;
    const int32 SizeZ = VolumeResolutionZ;
    
    // Get the texture's pixel format to determine bytes per pixel
    const EPixelFormat PixelFormat = VolumeTexture->GetFormat();
    const int32 BytesPerPixel = GPixelFormats[PixelFormat].BlockBytes;
    
    // Convert to appropriate pixel format based on texture format
    TSharedPtr<TArray<uint8>> DataPtr;
    
    if (PixelFormat == PF_FloatRGBA || PixelFormat == PF_A32B32G32R32F)
    {
        // HDR format - use FFloat16Color or full float
        TArray<FFloat16Color> HdrData;
        HdrData.SetNum(VoxelData.Num());
        for (int32 i = 0; i < VoxelData.Num(); i++)
        {
            HdrData[i] = FFloat16Color(VoxelData[i]);
        }
        
        DataPtr = MakeShared<TArray<uint8>>();
        DataPtr->SetNumUninitialized(HdrData.Num() * sizeof(FFloat16Color));
        FMemory::Memcpy(DataPtr->GetData(), HdrData.GetData(), DataPtr->Num());
    }
    else
    {
        // LDR format - use FColor (BGRA8)
        TArray<FColor> LdrData;
        LdrData.SetNum(VoxelData.Num());
        for (int32 i = 0; i < VoxelData.Num(); i++)
        {
            LdrData[i] = VoxelData[i].ToFColor(false);
        }
        
        DataPtr = MakeShared<TArray<uint8>>();
        DataPtr->SetNumUninitialized(LdrData.Num() * sizeof(FColor));
        FMemory::Memcpy(DataPtr->GetData(), LdrData.GetData(), DataPtr->Num());
    }
    
    // Get the texture RHI resource reference while on game thread
    UTextureRenderTargetVolume* RT = VolumeTexture;
    if (!RT) return;
    
    // Make sure the resource is initialized
    RT->UpdateResourceImmediate(true);
    
    // Enqueue render command directly (we're already on game thread from Then_GameThread)
    ENQUEUE_RENDER_COMMAND(UpdateVolumeTexture)(
        [RT, DataPtr, SizeX, SizeY, SizeZ, BytesPerPixel](FRHICommandListImmediate& RHICmdList)
    {
        if (!RT || !IsValid(RT)) return;
        
        FTextureRenderTargetResource* Resource = RT->GetRenderTargetResource();
        if (!Resource) return;
        
        FRHITexture* Texture = Resource->GetRenderTargetTexture();
        if (!Texture) return;
        
        const uint32 SourcePitch = SizeX * BytesPerPixel;
        const uint32 SourceDepthPitch = SourcePitch * SizeY;
        
        // Update entire volume at once
        FUpdateTextureRegion3D Region(0, 0, 0, 0, 0, 0, SizeX, SizeY, SizeZ);
        
        PRAGMA_DISABLE_DEPRECATION_WARNINGS
        RHIUpdateTexture3D(
            Texture,
            0, // Mip index
            Region,
            SourcePitch,
            SourceDepthPitch,
            DataPtr->GetData()
        );
        PRAGMA_ENABLE_DEPRECATION_WARNINGS
    });
}
