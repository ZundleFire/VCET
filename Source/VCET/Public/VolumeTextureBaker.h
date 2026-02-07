// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/VolumeTexture.h"
#include "Engine/TextureRenderTargetVolume.h"
#include "VoxelMinimal.h"
#include "VoxelStackLayer.h"
#include "VolumeTextureBaker.generated.h"

class UVoxelMetadata;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnVolumeTextureBaked);

/** Volume texture format for output */
UENUM(BlueprintType)
enum class EVolumeTextureFormat : uint8
{
    /** Single channel 8-bit (R8) - Best for cloud density */
    Grayscale8 UMETA(DisplayName = "Grayscale 8-bit (R8)"),
    
    /** Single channel 16-bit float (R16F) - HDR density */
    Grayscale16F UMETA(DisplayName = "Grayscale 16-bit Float (R16F)"),
    
    /** Full color 8-bit (RGBA8) */
    Color8 UMETA(DisplayName = "Color 8-bit (RGBA8)"),
    
    /** Full color 16-bit float (RGBA16F) - HDR color */
    Color16F UMETA(DisplayName = "Color 16-bit Float (RGBA16F)")
};

/**
 * Bakes Voxel VOLUME layer data to 3D Volume Textures.
 * Creates true volumetric textures for advanced effects like:
 * - Volumetric clouds with ray marching (use Grayscale format)
 * - 3D fog/mist volumes
 * - Density fields for particle effects
 * 
 * The output is a seamless 3D cube texture, similar to UE5's built-in
 * volumetric cloud textures.
 */
UCLASS(ClassGroup=(VCET), meta=(BlueprintSpawnableComponent), DisplayName="VCET Volume Texture Baker")
class VCET_API UVolumeTextureBaker : public UActorComponent
{
    GENERATED_BODY()

public:
    UVolumeTextureBaker();

    // === Voxel Configuration ===
    
    /** The Voxel VOLUME layer to query */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel")
    FVoxelStackVolumeLayer VolumeLayer;
    
    /** 
     * Metadata to sample (optional).
     * - Float Metadata ? density value
     * - Linear Color Metadata ? RGBA (requires Color format)
     * - None ? samples distance field directly
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel")
    TObjectPtr<UVoxelMetadata> Metadata;

    // === Volume Region ===
    
    /** Center of the sampling region (world space) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Volume Region")
    FVector VolumeCenter = FVector::ZeroVector;
    
    /** Size of the sampling region - this will be mapped to the 3D texture cube */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Volume Region")
    FVector VolumeSize = FVector(50000.0f, 50000.0f, 50000.0f);

    // === Volume Texture Settings ===
    
    /** External volume render target. Leave null to auto-create. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Volume Texture")
    TObjectPtr<UTextureRenderTargetVolume> VolumeRenderTarget;
    
    /** 
     * Volume texture resolution (cubic). 
     * Common values: 32, 64, 128, 256
     * UE5 cloud textures typically use 128.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Volume Texture", meta = (ClampMin = "4", ClampMax = "256"))
    int32 VolumeResolution = 128;
    
    /** 
     * Texture format. Use Grayscale for cloud density (like UE5 clouds).
     * Grayscale8 is most memory-efficient.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Volume Texture")
    EVolumeTextureFormat TextureFormat = EVolumeTextureFormat::Grayscale8;

    // === Processing ===
    
    /** Remap values from (-1,1) to (0,1) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Processing")
    bool bRemapNegativeToPositive = true;
    
    /** Auto-normalize values to 0-1 range */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Processing")
    bool bAutoNormalize = true;
    
    /** Invert the result (1 - value) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Processing")
    bool bInvertResult = false;
    
    /** Result multiplier */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Processing", meta = (ClampMin = "0.01", ClampMax = "100.0"))
    float ResultMultiplier = 1.0f;

    // === Lifecycle ===
    
    /** Bake on BeginPlay */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lifecycle")
    bool bBakeOnBeginPlay = false;

    // === Output ===
    
    /** The baked volume texture (read-only) */
    UPROPERTY(BlueprintReadOnly, Category = "Output")
    TObjectPtr<UTextureRenderTargetVolume> VolumeTexture;
    
    /** Called when baking completes */
    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnVolumeTextureBaked OnBakeComplete;

    // === Functions ===
    
    /** Force a rebake of the volume texture */
    UFUNCTION(BlueprintCallable, Category = "VCET|Volume Texture")
    void ForceRebake();
    
    /** Get the output volume texture */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "VCET|Volume Texture")
    UTextureRenderTargetVolume* GetVolumeTexture() const { return VolumeTexture; }
    
    /** Check if currently baking */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "VCET|Volume Texture")
    bool IsBaking() const { return bIsBaking; }
    
    /** Trigger all volume texture bakers in world to rebake */
    UFUNCTION(BlueprintCallable, Category = "VCET|Volume Texture", meta = (WorldContext = "WorldContextObject"))
    static void RequestGlobalRebake(UObject* WorldContextObject);

protected:
    virtual void BeginPlay() override;

private:
    bool bIsBaking = false;
    
    
    EPixelFormat GetPixelFormat() const;
    void CreateVolumeRT();
    void BakeVolume();
    void WriteToVolumeRT(const TArray<float>& DensityData);
};
