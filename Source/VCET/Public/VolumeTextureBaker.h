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
 * Bakes Voxel VOLUME layer data to 3D Volume Textures for volumetric rendering.
 * 
 * Creates true volumetric textures (UTextureRenderTargetVolume) suitable for:
 * - Volumetric clouds with ray marching
 * - 3D fog/mist volumes
 * - Density fields for particle effects
 * - Volumetric lighting and atmospherics
 * 
 * TECHNICAL NOTES:
 * - Output format: Always PF_FloatRGBA (RGBA 16-bit float, 8 bytes per voxel)
 *   This is a limitation of UE5's UTextureRenderTargetVolume which ignores format parameters
 * - Memory usage: Resolution^3 * 8 bytes (e.g., 128³ = 16MB)
 * - The output is a seamless 3D cube texture, similar to UE5's built-in volumetric cloud textures
 * 
 * WORKFLOW:
 * 1. Set VolumeLayer to your voxel volume layer (distance field or metadata)
 * 2. Position VolumeCenter and VolumeSize to define sampling region
 * 3. Set VolumeResolution (32-256, typically 128 for clouds)
 * 4. Call ForceRebake() or enable bBakeOnBeginPlay
 * 5. Use the output VolumeTexture in your materials (Material Parameter Collection recommended)
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
     * - Float Metadata ? grayscale density (written to RGB channels)
     * - LinearColor Metadata ? full RGBA color data
     * - None ? samples distance field directly (grayscale)
     * 
     * Automatically detects metadata type and formats data appropriately.
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
     * Volume texture resolution (cubic grid: N×N×N voxels).
     * Common values: 32, 64, 128, 256
     * UE5 cloud textures typically use 128.
     * Memory usage: N³ × 8 bytes (e.g., 128³ = 16MB)
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Volume Texture", meta = (ClampMin = "4", ClampMax = "256"))
    int32 VolumeResolution = 128;

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
    
    // === Asset Creation ===
    
    /** 
     * Automatically create a static UVolumeTexture asset after baking completes.
     * The asset will be saved to disk and can be used independently of this component.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Asset Creation")
    bool bCreateStaticAsset = false;
    
    /** 
     * Package path where the static texture asset will be saved.
     * Example: "/Game/Textures/Volumes" will save to Content/Textures/Volumes/
     * Leave empty to use "/Game/VCET/Volumes"
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Asset Creation", meta = (EditCondition = "bCreateStaticAsset"))
    FString AssetOutputPath = TEXT("/Game/VCET/Volumes");
    
    /** 
     * Base name for the created asset.
     * Will automatically append numbers to avoid overwrites (e.g., "CloudVolume_001", "CloudVolume_002")
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Asset Creation", meta = (EditCondition = "bCreateStaticAsset"))
    FString AssetBaseName = TEXT("VolumeTexture");

    // === Output ===
    
    /** The baked volume texture render target (read-only) */
    UPROPERTY(BlueprintReadOnly, Category = "Output")
    TObjectPtr<UTextureRenderTargetVolume> VolumeTexture;
    
    /** The last created static volume texture asset (if bCreateStaticAsset is enabled) */
    UPROPERTY(BlueprintReadOnly, Category = "Output")
    TObjectPtr<UVolumeTexture> StaticVolumeTexture;
    
    /** Called when baking completes */
    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnVolumeTextureBaked OnBakeComplete;

    // === Functions ===
    
    /** Force a rebake of the volume texture */
    UFUNCTION(BlueprintCallable, Category = "VCET|Volume Texture")
    void ForceRebake();
    
    /** Get the output volume render target */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "VCET|Volume Texture")
    UTextureRenderTargetVolume* GetVolumeTexture() const { return VolumeTexture; }
    
    /** Get the last created static volume texture asset */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "VCET|Volume Texture")
    UVolumeTexture* GetStaticVolumeTexture() const { return StaticVolumeTexture; }
    
    /** Check if currently baking */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "VCET|Volume Texture")
    bool IsBaking() const { return bIsBaking; }
    
    /** Manually create a static volume texture asset from the current render target */
    UFUNCTION(BlueprintCallable, Category = "VCET|Volume Texture")
    UVolumeTexture* CreateStaticTexture();
    
    /** Trigger all volume texture bakers in world to rebake */
    UFUNCTION(BlueprintCallable, Category = "VCET|Volume Texture", meta = (WorldContext = "WorldContextObject"))
    static void RequestGlobalRebake(UObject* WorldContextObject);

protected:
    virtual void BeginPlay() override;

private:
    bool bIsBaking = false;
    
    // Cached color data from last bake (used for creating static textures)
    TArray<FLinearColor> CachedColorData;
    
    void CreateVolumeRT();
    void BakeVolume();
    void WriteToVolumeRT(const TArray<FLinearColor>& ColorData);
    void CreateStaticAssetIfNeeded();
    FString GetUniqueAssetName(const FString& PackagePath, const FString& BaseName);
};
