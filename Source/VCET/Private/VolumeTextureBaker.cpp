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

EPixelFormat UVolumeTextureBaker::GetPixelFormat() const
{
    switch (TextureFormat)
    {
        case EVolumeTextureFormat::Grayscale8:
            return PF_G8;
        case EVolumeTextureFormat::Grayscale16F:
            return PF_R16F;
        case EVolumeTextureFormat::Color8:
            return PF_B8G8R8A8;
        case EVolumeTextureFormat::Color16F:
            return PF_FloatRGBA;
        default:
            return PF_G8;
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
    
    const EPixelFormat PixelFormat = GetPixelFormat();
    const int32 Size = VolumeResolution;
    
    // Create new volume RT if needed or if size/format changed
    if (!VolumeTexture || 
        VolumeTexture->SizeX != Size || 
        VolumeTexture->SizeY != Size ||
        VolumeTexture->GetFormat() != PixelFormat)
    {
        VolumeTexture = NewObject<UTextureRenderTargetVolume>(this);
        VolumeTexture->Init(Size, Size, Size, PixelFormat);
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
    
    const int32 Size = VolumeResolution;
    const int32 TotalVoxels = Size * Size * Size;
    
    // Check for float metadata
    bool bHasFloatMetadata = false;
    TOptional<FVoxelFloatMetadataRef> FloatRef;
    
    if (Metadata)
    {
        if (auto* FloatMeta = Cast<UVoxelFloatMetadata>(Metadata))
        {
            bHasFloatMetadata = true;
            FloatRef = FVoxelFloatMetadataRef(FloatMeta);
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
    
    const FVector VolCtr = VolumeCenter;
    const FVector VolSize = VolumeSize;
    const bool bRemap = bRemapNegativeToPositive;
    const bool bNorm = bAutoNormalize;
    const bool bInvert = bInvertResult;
    const float Mult = ResultMultiplier;
    
    struct FBakeResult
    {
        TArray<float> DensityData;
    };
    
    Voxel::AsyncTask([WeakLayer, Layers, STT, Size, TotalVoxels,
                     VolCtr, VolSize, bRemap, bNorm, bInvert, Mult,
                     bHasFloatMetadata, FloatRef]() -> TVoxelFuture<FBakeResult>
    {
        VOXEL_FUNCTION_COUNTER();
        FBakeResult Result;
        Result.DensityData.SetNum(TotalVoxels);
        
        // Generate 3D sample positions for a cubic volume
        FVoxelDoubleVectorBuffer Positions;
        Positions.Allocate(TotalVoxels);
        
        const FVector HalfSize = VolSize * 0.5;
        const FVector MinCorner = VolCtr - HalfSize;
        
        for (int32 Z = 0; Z < Size; Z++)
        {
            for (int32 Y = 0; Y < Size; Y++)
            {
                for (int32 X = 0; X < Size; X++)
                {
                    const int32 Index = X + Y * Size + Z * Size * Size;
                    
                    // Normalized UVW coordinates (0-1)
                    const double U = (double(X) + 0.5) / double(Size);
                    const double V = (double(Y) + 0.5) / double(Size);
                    const double W = (double(Z) + 0.5) / double(Size);
                    
                    // World position within the sampling volume
                    const double WorldX = MinCorner.X + U * VolSize.X;
                    const double WorldY = MinCorner.Y + V * VolSize.Y;
                    const double WorldZ = MinCorner.Z + W * VolSize.Z;
                    
                    Positions.X.Set(Index, WorldX);
                    Positions.Y.Set(Index, WorldY);
                    Positions.Z.Set(Index, WorldZ);
                }
            }
        }
        
        // Query voxel data
        FVoxelQuery Query(0, *Layers, *STT, FVoxelDependencyCollector::Null);
        
        if (bHasFloatMetadata && FloatRef.IsSet())
        {
            // Sample float metadata (density)
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
                            Result.DensityData[i] = (*FB)[i];
                        }
                    }
                }
            }
        }
        else
        {
            // Sample distance field directly
            auto Dist = Query.SampleVolumeLayer(WeakLayer, Positions);
            for (int32 i = 0; i < TotalVoxels; i++)
            {
                Result.DensityData[i] = Dist[i];
            }
        }
        
        // Process the data
        float MinV = FLT_MAX, MaxV = -FLT_MAX;
        
        // First pass: remap and find min/max
        for (int32 i = 0; i < TotalVoxels; i++)
        {
            float Val = Result.DensityData[i];
            if (bRemap) Val = (Val + 1.f) * 0.5f;
            Val *= Mult;
            if (bInvert) Val = 1.f - Val;
            Result.DensityData[i] = Val;
            MinV = FMath::Min(MinV, Val);
            MaxV = FMath::Max(MaxV, Val);
        }
        
        // Second pass: normalize if requested
        if (bNorm && MaxV > MinV)
        {
            const float Range = MaxV - MinV;
            for (int32 i = 0; i < TotalVoxels; i++)
            {
                Result.DensityData[i] = (Result.DensityData[i] - MinV) / Range;
            }
        }
        else
        {
            // Just clamp to 0-1
            for (int32 i = 0; i < TotalVoxels; i++)
            {
                Result.DensityData[i] = FMath::Clamp(Result.DensityData[i], 0.f, 1.f);
            }
        }
        
        return Result;
        
    }).Then_GameThread([WeakThis](const FBakeResult& Result)
    {
        UVolumeTextureBaker* This = WeakThis.Get();
        if (!This) return;
        
        if (Result.DensityData.Num() > 0)
        {
            This->WriteToVolumeRT(Result.DensityData);
        }
        
        This->bIsBaking = false;
        This->OnBakeComplete.Broadcast();
    });
}

void UVolumeTextureBaker::WriteToVolumeRT(const TArray<float>& DensityData)
{
    if (!VolumeTexture || DensityData.Num() == 0) return;
    
    const int32 Size = VolumeResolution;
    const int32 TotalVoxels = Size * Size * Size;
    
    if (DensityData.Num() != TotalVoxels)
    {
        UE_LOG(LogTemp, Error, TEXT("VolumeTextureBaker: Data size mismatch! Expected %d, got %d"), TotalVoxels, DensityData.Num());
        return;
    }
    
    const EPixelFormat PixelFormat = GetPixelFormat();
    const int32 BytesPerPixel = GPixelFormats[PixelFormat].BlockBytes;
    
    // Convert density data to the appropriate format
    TSharedPtr<TArray<uint8>> DataPtr = MakeShared<TArray<uint8>>();
    
    if (PixelFormat == PF_G8)
    {
        // Single channel 8-bit grayscale
        DataPtr->SetNumUninitialized(TotalVoxels);
        uint8* Dest = DataPtr->GetData();
        for (int32 i = 0; i < TotalVoxels; i++)
        {
            Dest[i] = static_cast<uint8>(FMath::Clamp(DensityData[i] * 255.f, 0.f, 255.f));
        }
    }
    else if (PixelFormat == PF_R16F)
    {
        // Single channel 16-bit float
        DataPtr->SetNumUninitialized(TotalVoxels * sizeof(FFloat16));
        FFloat16* Dest = reinterpret_cast<FFloat16*>(DataPtr->GetData());
        for (int32 i = 0; i < TotalVoxels; i++)
        {
            Dest[i] = FFloat16(DensityData[i]);
        }
    }
    else if (PixelFormat == PF_B8G8R8A8)
    {
        // BGRA 8-bit - write grayscale to all channels
        DataPtr->SetNumUninitialized(TotalVoxels * 4);
        uint8* Dest = DataPtr->GetData();
        for (int32 i = 0; i < TotalVoxels; i++)
        {
            const uint8 Val = static_cast<uint8>(FMath::Clamp(DensityData[i] * 255.f, 0.f, 255.f));
            Dest[i * 4 + 0] = Val; // B
            Dest[i * 4 + 1] = Val; // G
            Dest[i * 4 + 2] = Val; // R
            Dest[i * 4 + 3] = 255; // A
        }
    }
    else if (PixelFormat == PF_FloatRGBA)
    {
        // RGBA 16-bit float - write grayscale to all channels
        DataPtr->SetNumUninitialized(TotalVoxels * sizeof(FFloat16) * 4);
        FFloat16* Dest = reinterpret_cast<FFloat16*>(DataPtr->GetData());
        for (int32 i = 0; i < TotalVoxels; i++)
        {
            const FFloat16 Val(DensityData[i]);
            Dest[i * 4 + 0] = Val; // R
            Dest[i * 4 + 1] = Val; // G
            Dest[i * 4 + 2] = Val; // B
            Dest[i * 4 + 3] = FFloat16(1.0f); // A
        }
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("VolumeTextureBaker: Unsupported pixel format!"));
        return;
    }
    
    // Get the texture
    UTextureRenderTargetVolume* RT = VolumeTexture;
    if (!RT) return;

    // Make sure the resource is initialized
    RT->UpdateResourceImmediate(true);

    // Log upload parameters for debugging
    UE_LOG(LogTemp, Verbose, TEXT("VolumeTextureBaker: Format=%d BytesPerPixel=%d Size=%dx%dx%d TotalBytes=%d"),
        (int)PixelFormat, BytesPerPixel, Size, Size, Size, DataPtr->Num());

    // For single-slice uploads to avoid pitch issues, we'll upload one XY slice at a time
    const uint32 SliceSize = Size * Size * BytesPerPixel;
    
    // Enqueue render command
    ENQUEUE_RENDER_COMMAND(UpdateVolumeTextureSliced)(
        [RT, DataPtr, Size, BytesPerPixel, SliceSize](FRHICommandListImmediate& RHICmdList)
    {
        if (!RT || !IsValid(RT)) return;

        FTextureRenderTargetResource* Resource = RT->GetRenderTargetResource();
        if (!Resource) return;

        FRHITexture* Texture = Resource->GetRenderTargetTexture();
        if (!Texture) return;

        const uint32 RowPitch = Size * BytesPerPixel;
        const uint32 SlicePitch = RowPitch * Size;

        // Upload one Z-slice at a time with a 1-deep region to avoid pitch mismatches
        for (int32 Z = 0; Z < Size; ++Z)
        {
            const uint8* SliceData = DataPtr->GetData() + Z * SlicePitch;
            
            // Region for a single Z slice
            FUpdateTextureRegion3D Region(
                0, 0, Z,        // DestX, DestY, DestZ
                0, 0, 0,        // SourceX, SourceY, SourceZ
                Size, Size, 1   // Width, Height, Depth (1 slice)
            );

            PRAGMA_DISABLE_DEPRECATION_WARNINGS
            RHIUpdateTexture3D(
                Texture,
                0,              // Mip index
                Region,
                RowPitch,       // Source row pitch
                SlicePitch,     // Source depth pitch (for 1 slice this is row*height)
                SliceData
            );
            PRAGMA_ENABLE_DEPRECATION_WARNINGS
        }
    });
}
