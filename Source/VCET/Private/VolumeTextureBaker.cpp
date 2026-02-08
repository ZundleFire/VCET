// Copyright Epic Games, Inc. All Rights Reserved.

#include "VolumeTextureBaker.h"
#include "Engine/TextureRenderTargetVolume.h"
#include "Engine/VolumeTexture.h"
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
#include "UObject/SavePackage.h"
#include "Misc/PackageName.h"
#include "AssetRegistry/AssetRegistryModule.h"

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
    
    // UE5's UTextureRenderTargetVolume always creates PF_FloatRGBA regardless of Init() parameter
    // So we explicitly use PF_FloatRGBA to match what will actually be created
    const EPixelFormat VolumeFormat = PF_FloatRGBA;
    const int32 Size = VolumeResolution;
    
    // Create new volume RT if needed or if size changed
    if (!VolumeTexture || 
        VolumeTexture->SizeX != Size || 
        VolumeTexture->SizeY != Size)
    {
        VolumeTexture = NewObject<UTextureRenderTargetVolume>(this);
        VolumeTexture->Init(Size, Size, Size, VolumeFormat);
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
    
    // Auto-detect metadata type
    bool bHasFloatMetadata = false;
    bool bHasColorMetadata = false;
    TOptional<FVoxelFloatMetadataRef> FloatRef;
    TOptional<FVoxelLinearColorMetadataRef> ColorRef;
    
    if (Metadata)
    {
        if (auto* FloatMeta = Cast<UVoxelFloatMetadata>(Metadata))
        {
            bHasFloatMetadata = true;
            FloatRef = FVoxelFloatMetadataRef(FloatMeta);
            UE_LOG(LogTemp, Log, TEXT("VolumeTextureBaker: Using Float metadata (grayscale)"));
        }
        else if (auto* ColorMeta = Cast<UVoxelLinearColorMetadata>(Metadata))
        {
            bHasColorMetadata = true;
            ColorRef = FVoxelLinearColorMetadataRef(ColorMeta);
            UE_LOG(LogTemp, Log, TEXT("VolumeTextureBaker: Using LinearColor metadata (RGBA)"));
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
        TArray<FLinearColor> ColorData;  // Always use color data (RGBA)
        bool bIsGrayscale = false;       // True if source was float metadata
    };
    
    Voxel::AsyncTask([WeakLayer, Layers, STT, Size, TotalVoxels,
                     VolCtr, VolSize, bRemap, bNorm, bInvert, Mult,
                     bHasFloatMetadata, bHasColorMetadata, FloatRef, ColorRef]() -> TVoxelFuture<FBakeResult>
    {
        VOXEL_FUNCTION_COUNTER();
        FBakeResult Result;
        Result.ColorData.SetNum(TotalVoxels);
        
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
        
        if (bHasColorMetadata && ColorRef.IsSet())
        {
            // Sample LinearColor metadata (full RGBA)
            Result.bIsGrayscale = false;
            
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
                            Result.ColorData[i] = (*CB)[i];
                        }
                    }
                }
            }
        }
        else if (bHasFloatMetadata && FloatRef.IsSet())
        {
            // Sample float metadata (grayscale - write to all RGB channels)
            Result.bIsGrayscale = true;
            
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
                            const float Val = (*FB)[i];
                            Result.ColorData[i] = FLinearColor(Val, Val, Val, 1.0f);
                        }
                    }
                }
            }
        }
        else
        {
            // Sample distance field directly (grayscale)
            Result.bIsGrayscale = true;
            
            auto Dist = Query.SampleVolumeLayer(WeakLayer, Positions);
            for (int32 i = 0; i < TotalVoxels; i++)
            {
                const float Val = Dist[i];
                Result.ColorData[i] = FLinearColor(Val, Val, Val, 1.0f);
            }
        }
        
        // Process the data
        if (Result.bIsGrayscale)
        {
            // For grayscale data, apply processing to RGB (leave A=1.0)
            float MinV = FLT_MAX, MaxV = -FLT_MAX;
            
            // First pass: remap and find min/max
            for (int32 i = 0; i < TotalVoxels; i++)
            {
                float Val = Result.ColorData[i].R;  // Use R channel for grayscale
                if (bRemap) Val = (Val + 1.f) * 0.5f;
                Val *= Mult;
                if (bInvert) Val = 1.f - Val;
                
                MinV = FMath::Min(MinV, Val);
                MaxV = FMath::Max(MaxV, Val);
                
                // Write to RGB (keep A=1.0)
                Result.ColorData[i] = FLinearColor(Val, Val, Val, 1.0f);
            }
            
            // Second pass: normalize if requested
            if (bNorm && MaxV > MinV)
            {
                const float Range = MaxV - MinV;
                for (int32 i = 0; i < TotalVoxels; i++)
                {
                    const float NormVal = (Result.ColorData[i].R - MinV) / Range;
                    Result.ColorData[i] = FLinearColor(NormVal, NormVal, NormVal, 1.0f);
                }
            }
            else
            {
                // Just clamp to 0-1
                for (int32 i = 0; i < TotalVoxels; i++)
                {
                    const float ClampVal = FMath::Clamp(Result.ColorData[i].R, 0.f, 1.f);
                    Result.ColorData[i] = FLinearColor(ClampVal, ClampVal, ClampVal, 1.0f);
                }
            }
        }
        else
        {
            // For color data, apply processing per-channel
            for (int32 i = 0; i < TotalVoxels; i++)
            {
                FLinearColor& Color = Result.ColorData[i];
                
                // Process each channel
                if (bRemap)
                {
                    Color.R = (Color.R + 1.f) * 0.5f;
                    Color.G = (Color.G + 1.f) * 0.5f;
                    Color.B = (Color.B + 1.f) * 0.5f;
                    Color.A = (Color.A + 1.f) * 0.5f;
                }
                
                Color.R *= Mult;
                Color.G *= Mult;
                Color.B *= Mult;
                Color.A *= Mult;
                
                if (bInvert)
                {
                    Color.R = 1.f - Color.R;
                    Color.G = 1.f - Color.G;
                    Color.B = 1.f - Color.B;
                    Color.A = 1.f - Color.A;
                }
                
                // Clamp to 0-1
                Color.R = FMath::Clamp(Color.R, 0.f, 1.f);
                Color.G = FMath::Clamp(Color.G, 0.f, 1.f);
                Color.B = FMath::Clamp(Color.B, 0.f, 1.f);
                Color.A = FMath::Clamp(Color.A, 0.f, 1.f);
            }
            
            
            // Note: Auto-normalize doesn't make sense for color data, skip it
        }
        
        return Result;
        
    }).Then_GameThread([WeakThis](const FBakeResult& Result)
    {
        UVolumeTextureBaker* This = WeakThis.Get();
        if (!This) return;
        
        if (Result.ColorData.Num() > 0)
        {
            // Cache the color data for static texture creation
            This->CachedColorData = Result.ColorData;
            
            // Write to render target
            This->WriteToVolumeRT(Result.ColorData);
        }
        
        // Create static asset if requested
        This->CreateStaticAssetIfNeeded();
        
        This->bIsBaking = false;
        This->OnBakeComplete.Broadcast();
    });
}

void UVolumeTextureBaker::WriteToVolumeRT(const TArray<FLinearColor>& ColorData)
{
    if (!VolumeTexture || ColorData.Num() == 0) return;
    
    const int32 Size = VolumeResolution;
    const int32 TotalVoxels = Size * Size * Size;
    
    if (ColorData.Num() != TotalVoxels)
    {
        UE_LOG(LogTemp, Error, TEXT("VolumeTextureBaker: Data size mismatch! Expected %d, got %d"), TotalVoxels, ColorData.Num());
        return;
    }
    
    // Query the ACTUAL format from the render target
    const EPixelFormat ActualFormat = VolumeTexture->GetFormat();
    const int32 BytesPerPixel = GPixelFormats[ActualFormat].BlockBytes;
    
    UE_LOG(LogTemp, Log, TEXT("VolumeTextureBaker: Detected format %d (%s) with %d bytes per pixel"), 
        (int)ActualFormat, GPixelFormats[ActualFormat].Name, BytesPerPixel);
    
    // Prepare data buffer based on actual format
    TSharedPtr<TArray<uint8>> DataPtr = MakeShared<TArray<uint8>>();
    
    // Convert color data to match the texture's actual format
    if (ActualFormat == PF_FloatRGBA || ActualFormat == PF_A32B32G32R32F)
    {
        // RGBA 16-bit float or 32-bit float
        const bool bIs16Bit = (ActualFormat == PF_FloatRGBA);
        DataPtr->SetNumUninitialized(TotalVoxels * BytesPerPixel);
        
        if (bIs16Bit)
        {
            FFloat16* Dest = reinterpret_cast<FFloat16*>(DataPtr->GetData());
            for (int32 i = 0; i < TotalVoxels; i++)
            {
                const FLinearColor& Color = ColorData[i];
                Dest[i * 4 + 0] = FFloat16(Color.R); // R
                Dest[i * 4 + 1] = FFloat16(Color.G); // G
                Dest[i * 4 + 2] = FFloat16(Color.B); // B
                Dest[i * 4 + 3] = FFloat16(Color.A); // A
            }
        }
        else
        {
            float* Dest = reinterpret_cast<float*>(DataPtr->GetData());
            for (int32 i = 0; i < TotalVoxels; i++)
            {
                const FLinearColor& Color = ColorData[i];
                Dest[i * 4 + 0] = Color.R; // R
                Dest[i * 4 + 1] = Color.G; // G
                Dest[i * 4 + 2] = Color.B; // B
                Dest[i * 4 + 3] = Color.A; // A
            }
        }
    }
    else if (ActualFormat == PF_B8G8R8A8)
    {
        // BGRA 8-bit
        DataPtr->SetNumUninitialized(TotalVoxels * 4);
        uint8* Dest = DataPtr->GetData();
        for (int32 i = 0; i < TotalVoxels; i++)
        {
            const FLinearColor& Color = ColorData[i];
            Dest[i * 4 + 0] = static_cast<uint8>(FMath::Clamp(Color.B * 255.f, 0.f, 255.f)); // B
            Dest[i * 4 + 1] = static_cast<uint8>(FMath::Clamp(Color.G * 255.f, 0.f, 255.f)); // G
            Dest[i * 4 + 2] = static_cast<uint8>(FMath::Clamp(Color.R * 255.f, 0.f, 255.f)); // R
            Dest[i * 4 + 3] = static_cast<uint8>(FMath::Clamp(Color.A * 255.f, 0.f, 255.f)); // A
        }
    }
    else if (ActualFormat == PF_R16F)
    {
        // Single channel 16-bit float - use R channel only
        DataPtr->SetNumUninitialized(TotalVoxels * sizeof(FFloat16));
        FFloat16* Dest = reinterpret_cast<FFloat16*>(DataPtr->GetData());
        for (int32 i = 0; i < TotalVoxels; i++)
        {
            Dest[i] = FFloat16(ColorData[i].R);
        }
    }
    else if (ActualFormat == PF_G8)
    {
        // Single channel 8-bit - use R channel only
        DataPtr->SetNumUninitialized(TotalVoxels);
        uint8* Dest = DataPtr->GetData();
        for (int32 i = 0; i < TotalVoxels; i++)
        {
            Dest[i] = static_cast<uint8>(FMath::Clamp(ColorData[i].R * 255.f, 0.f, 255.f));
        }
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("VolumeTextureBaker: Unsupported texture format %d (%s)!"), 
            (int)ActualFormat, GPixelFormats[ActualFormat].Name);
        return;
    }
    
    UE_LOG(LogTemp, Log, TEXT("VolumeTextureBaker: Prepared %d voxels, %d bytes total"), 
        TotalVoxels, DataPtr->Num());
    
    // Get the texture
    UTextureRenderTargetVolume* RT = VolumeTexture;
    if (!RT) return;

    // Make sure the resource is initialized
    RT->UpdateResourceImmediate(true);

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

        // Get the ACTUAL pixel format from the texture - this is what D3D12 allocated
        const EPixelFormat ActualFormat = Texture->GetFormat();
        const FPixelFormatInfo& FormatInfo = GPixelFormats[ActualFormat];
        
        // Calculate what D3D12 expects for row pitch based on the actual texture format
        // This accounts for block compression and other format-specific requirements
        const uint32 ActualBytesPerPixel = FormatInfo.BlockBytes;
        const uint32 BlockSizeX = FormatInfo.BlockSizeX;
        const uint32 BlockSizeY = FormatInfo.BlockSizeY;
        
        // Calculate row pitch accounting for block sizes
        const uint32 NumBlocksX = FMath::DivideAndRoundUp((uint32)Size, BlockSizeX);
        const uint32 NumBlocksY = FMath::DivideAndRoundUp((uint32)Size, BlockSizeY);
        const uint32 DestRowPitch = NumBlocksX * ActualBytesPerPixel;
        
        // Our source data pitch (tightly packed)
        const uint32 SourceRowPitch = Size * BytesPerPixel;
        const uint32 SourceSlicePitch = SourceRowPitch * Size;
        
        
        
        // Log format info for debugging (should now match perfectly)
        UE_LOG(LogTemp, Log, TEXT("VolumeTextureBaker: ActualFormat=%d ActualBPP=%d SourceBPP=%d DestRowPitch=%d SourceRowPitch=%d"),
            (int)ActualFormat, ActualBytesPerPixel, BytesPerPixel, DestRowPitch, SourceRowPitch);
        
        // Check if we need format conversion or can upload directly
        const bool bNeedsConversion = (ActualBytesPerPixel != BytesPerPixel) || (DestRowPitch != SourceRowPitch);
        
        if (!bNeedsConversion)
        {
            // Perfect match - upload directly slice by slice
            for (int32 Z = 0; Z < Size; ++Z)
            {
                const uint8* SliceData = DataPtr->GetData() + Z * SourceSlicePitch;
                
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
                    DestRowPitch,       // Source row pitch (what D3D12 expects)
                    DestRowPitch * NumBlocksY,     // Source depth pitch
                    SliceData
                );
                PRAGMA_ENABLE_DEPRECATION_WARNINGS
            }
        }
        else
        {
            // Need to reformat data to match destination - create aligned buffer per slice
            TArray<uint8> ConvertedSliceData;
            const uint32 DestSliceSize = DestRowPitch * NumBlocksY;
            ConvertedSliceData.SetNumZeroed(DestSliceSize);
            
            for (int32 Z = 0; Z < Size; ++Z)
            {
                const uint8* SourceSliceData = DataPtr->GetData() + Z * SourceSlicePitch;
                
                // Copy/convert each row
                if (ActualBytesPerPixel == BytesPerPixel)
                {
                    // Same bytes per pixel, just different pitch - copy with padding
                    for (int32 Y = 0; Y < Size; ++Y)
                    {
                        FMemory::Memcpy(
                            ConvertedSliceData.GetData() + Y * DestRowPitch,
                            SourceSliceData + Y * SourceRowPitch,
                            SourceRowPitch
                        );
                    }
                }
                else
                {
                    // Format mismatch - need conversion (shouldn't happen if Init() uses matching format)
                    UE_LOG(LogTemp, Error, TEXT("VolumeTextureBaker: Format mismatch! Expected %d BPP but got %d BPP"), 
                        ActualBytesPerPixel, BytesPerPixel);
                    return;
                }
                
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
                    DestRowPitch,       // Source row pitch (what D3D12 expects)
                    DestSliceSize,      // Source depth pitch
                    ConvertedSliceData.GetData()
                );
                PRAGMA_ENABLE_DEPRECATION_WARNINGS
            }
        }
    });
}

void UVolumeTextureBaker::CreateStaticAssetIfNeeded()
{
    if (!bCreateStaticAsset || !VolumeTexture)
    {
        return;
    }
    
    StaticVolumeTexture = CreateStaticTexture();
}

UVolumeTexture* UVolumeTextureBaker::CreateStaticTexture()
{
    if (!VolumeTexture)
    {
        UE_LOG(LogTemp, Error, TEXT("VolumeTextureBaker: Cannot create static texture - no render target available"));
        return nullptr;
    }
    
    if (CachedColorData.Num() == 0)
    {
        UE_LOG(LogTemp, Error, TEXT("VolumeTextureBaker: Cannot create static texture - no cached data available. Run ForceRebake() first."));
        return nullptr;
    }
    
    const int32 Size = VolumeResolution;
    const int32 TotalVoxels = Size * Size * Size;
    
    if (CachedColorData.Num() != TotalVoxels)
    {
        UE_LOG(LogTemp, Error, TEXT("VolumeTextureBaker: Cached data size mismatch! Expected %d, got %d"), TotalVoxels, CachedColorData.Num());
        return nullptr;
    }
    
    // Ensure output path is valid
    FString PackagePath = AssetOutputPath;
    if (PackagePath.IsEmpty())
    {
        PackagePath = TEXT("/Game/VCET/Volumes");
    }
    
    // Remove trailing slash
    PackagePath.RemoveFromEnd(TEXT("/"));
    
    // Get unique asset name
    FString UniqueName = GetUniqueAssetName(PackagePath, AssetBaseName);
    FString PackageName = PackagePath + TEXT("/") + UniqueName;
    
    UE_LOG(LogTemp, Log, TEXT("VolumeTextureBaker: Creating static volume texture at %s"), *PackageName);
    
    // Create package
    UPackage* Package = CreatePackage(*PackageName);
    if (!Package)
    {
        UE_LOG(LogTemp, Error, TEXT("VolumeTextureBaker: Failed to create package %s"), *PackageName);
        return nullptr;
    }
    
    Package->FullyLoad();
    
    // Create the volume texture
    UVolumeTexture* VolumeTextureAsset = NewObject<UVolumeTexture>(Package, *UniqueName, RF_Public | RF_Standalone);
    if (!VolumeTextureAsset)
    {
        UE_LOG(LogTemp, Error, TEXT("VolumeTextureBaker: Failed to create UVolumeTexture object"));
        return nullptr;
    }
    
    // Initialize source data - using RGBA16F format (Float16 per channel)
    VolumeTextureAsset->Source.Init(Size, Size, Size, 1, TSF_RGBA16F);
    
    // Convert cached FLinearColor data to FFloat16Color for the texture
    TArray<FFloat16Color> Float16Data;
    Float16Data.SetNumUninitialized(TotalVoxels);
    
    for (int32 i = 0; i < TotalVoxels; i++)
    {
        const FLinearColor& Color = CachedColorData[i];
        Float16Data[i] = FFloat16Color(Color);
    }
    
    // Copy data to texture source
    uint8* DestData = VolumeTextureAsset->Source.LockMip(0);
    FMemory::Memcpy(DestData, Float16Data.GetData(), TotalVoxels * sizeof(FFloat16Color));
    VolumeTextureAsset->Source.UnlockMip(0);
    
    // Set texture properties
    VolumeTextureAsset->SRGB = false;
    VolumeTextureAsset->CompressionSettings = TC_HDR;
    VolumeTextureAsset->MipGenSettings = TMGS_NoMipmaps;
    VolumeTextureAsset->AddressMode = TA_Clamp;
    
    // Update the texture
    VolumeTextureAsset->UpdateResource();
    
    // Mark package as dirty
    Package->MarkPackageDirty();
    
    // Save the package
    FString FilePath = FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension());
    FSavePackageArgs SaveArgs;
    SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
    SaveArgs.SaveFlags = SAVE_NoError;
    
    if (UPackage::SavePackage(Package, VolumeTextureAsset, *FilePath, SaveArgs))
    {
        UE_LOG(LogTemp, Log, TEXT("VolumeTextureBaker: Successfully saved static texture to %s"), *FilePath);
        
        // Notify asset registry
        FAssetRegistryModule::AssetCreated(VolumeTextureAsset);
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("VolumeTextureBaker: Failed to save package to %s"), *FilePath);
    }
    
    return VolumeTextureAsset;
}

FString UVolumeTextureBaker::GetUniqueAssetName(const FString& PackagePath, const FString& BaseName)
{
    FString UniqueName = BaseName;
    int32 Suffix = 1;
    
    // Check if asset exists and increment suffix until we find a unique name
    while (true)
    {
        FString TestPackageName = PackagePath + TEXT("/") + UniqueName;
        
        // Check if package exists
        if (!FPackageName::DoesPackageExist(TestPackageName))
        {
            break;
        }
        
        // Try next suffix
        UniqueName = FString::Printf(TEXT("%s_%03d"), *BaseName, Suffix);
        Suffix++;
        
        // Safety check to avoid infinite loop
        if (Suffix > 999)
        {
            UE_LOG(LogTemp, Warning, TEXT("VolumeTextureBaker: Reached maximum suffix count (999), using timestamp"));
            UniqueName = FString::Printf(TEXT("%s_%lld"), *BaseName, FDateTime::Now().GetTicks());
            break;
        }
    }
    
    return UniqueName;
}

