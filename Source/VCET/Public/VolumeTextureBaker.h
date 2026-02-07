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

/**
 * Bakes Voxel VOLUME layer data to 3D Volume Render Targets.
 * Creates true volumetric textures for advanced effects like:
 * - Volumetric clouds with ray marching
 * - 3D fog/mist volumes
 * - Density fields for particle effects
 * 
 * Supports both Box (AABB) and Spherical Shell sampling regions.
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
     * Metadata to sample. Auto-detects type:
     * - Float ? R channel only (density)
     * - Linear Color ? RGBA channels
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel")
    TObjectPtr<UVoxelMetadata> Metadata;

    // === Volume Region ===
    
    /** Use spherical shell instead of box region */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Volume Region")
    bool bUseSphericalRegion = false;
    
    // --- Box Region ---
    
    /** Center of the box sampling region (world space) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Volume Region|Box", meta = (EditCondition = "!bUseSphericalRegion"))
    FVector BoxCenter = FVector::ZeroVector;
    
    /** Size of the box sampling region (world units) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Volume Region|Box", meta = (EditCondition = "!bUseSphericalRegion"))
    FVector BoxExtent = FVector(100000.0f, 100000.0f, 20000.0f);
    
    // --- Spherical Shell Region ---
    
    /** Center of the sphere (typically planet center) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Volume Region|Spherical", meta = (EditCondition = "bUseSphericalRegion"))
    FVector SphereCenter = FVector::ZeroVector;
    
    /** Inner radius of the shell (e.g., planet surface) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Volume Region|Spherical", meta = (EditCondition = "bUseSphericalRegion"))
    float InnerRadius = 637100.0f;
    
    /** Outer radius of the shell (e.g., atmosphere top) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Volume Region|Spherical", meta = (EditCondition = "bUseSphericalRegion"))
    float OuterRadius = 650000.0f;

    // === Volume Texture Settings ===
    
    /** External volume render target. Leave null to auto-create. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Volume Texture")
    TObjectPtr<UTextureRenderTargetVolume> VolumeRenderTarget;
    
    /** Volume texture resolution X (width) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Volume Texture", meta = (ClampMin = "4", ClampMax = "512"))
    int32 VolumeResolutionX = 128;
    
    /** Volume texture resolution Y (height) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Volume Texture", meta = (ClampMin = "4", ClampMax = "512"))
    int32 VolumeResolutionY = 128;
    
    /** Volume texture resolution Z (depth) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Volume Texture", meta = (ClampMin = "4", ClampMax = "512"))
    int32 VolumeResolutionZ = 64;
    
    /** Use HDR format (RGBA16f) instead of RGBA8 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Volume Texture")
    bool bUseHDR = false;

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
    
    void CreateVolumeRT();
    void BakeVolume();
    void WriteToVolumeRT(const TArray<FLinearColor>& VoxelData);
};
