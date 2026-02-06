// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/TextureRenderTarget2D.h"
#include "VoxelMinimal.h"
#include "VoxelStackLayer.h"
#include "VoxelQueryBlueprintLibrary.h"
#include "PlanarTextureBaker.generated.h"

class UVoxelMetadata;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnPlanarTextureBaked);

/**
 * Bakes Voxel VOLUME layer data to flat/planar render targets.
 * Samples 3D positions on a horizontal plane and writes to 2D textures.
 * 
 * Use for flat/non-spherical worlds to bake clouds, terrain colors, etc.
 * 
 * Features:
 * - Multiple layers at different heights (Primary, Secondary)
 * - Configurable world bounds (XY area)
 * - Auto-detects metadata type and writes appropriate channels:
 *   - Float Metadata ? R channel (grayscale)
 *   - Linear Color Metadata ? RGBA channels
 *   - Normal Metadata ? RGB channels
 * - External or auto-created render targets
 */
UCLASS(ClassGroup=(VCET), meta=(BlueprintSpawnableComponent), DisplayName="VCET Planar Texture Baker")
class VCET_API UPlanarTextureBaker : public UActorComponent
{
    GENERATED_BODY()

public:
    UPlanarTextureBaker();

    // === Voxel ===
    
    /** The Voxel VOLUME layer to query */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel")
    FVoxelStackVolumeLayer VolumeLayer;

    // === Shared ===
    
    /** World center point (XY center of sampling area) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shared")
    FVector WorldCenter = FVector::ZeroVector;
    
    /** Size of sampling area (X and Y extent from center) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shared")
    FVector2D WorldSize = FVector2D(100000.0f, 100000.0f);
    
    /** Bake on BeginPlay */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shared")
    bool bBakeOnBeginPlay = false;

    // === Primary Layer ===
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Primary Layer")
    bool bEnablePrimaryLayer = true;
    
    /** 
     * Metadata to sample. Auto-detects type:
     * - Float ? R channel only
     * - Linear Color ? RGBA channels
     * - Normal ? RGB channels
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Primary Layer", meta = (EditCondition = "bEnablePrimaryLayer"))
    TObjectPtr<UVoxelMetadata> PrimaryMetadata;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Primary Layer", meta = (EditCondition = "bEnablePrimaryLayer"))
    TObjectPtr<UTextureRenderTarget2D> PrimaryRenderTarget;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Primary Layer", meta = (EditCondition = "bEnablePrimaryLayer"))
    float PrimaryHeight = 10000.0f;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Primary Layer", meta = (ClampMin = "16", ClampMax = "4096", EditCondition = "bEnablePrimaryLayer"))
    int32 PrimaryTextureWidth = 512;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Primary Layer", meta = (ClampMin = "16", ClampMax = "4096", EditCondition = "bEnablePrimaryLayer"))
    int32 PrimaryTextureHeight = 512;
    
    UPROPERTY(BlueprintReadOnly, Category = "Primary Layer|Output")
    TObjectPtr<UTextureRenderTarget2D> PrimaryTexture;
    
    UPROPERTY(BlueprintAssignable, Category = "Primary Layer")
    FOnPlanarTextureBaked OnPrimaryBakeComplete;

    // === Secondary Layer ===
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Secondary Layer")
    bool bEnableSecondaryLayer = false;
    
    /** 
     * Metadata to sample. Auto-detects type:
     * - Float ? R channel only
     * - Linear Color ? RGBA channels
     * - Normal ? RGB channels
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Secondary Layer", meta = (EditCondition = "bEnableSecondaryLayer"))
    TObjectPtr<UVoxelMetadata> SecondaryMetadata;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Secondary Layer", meta = (EditCondition = "bEnableSecondaryLayer"))
    TObjectPtr<UTextureRenderTarget2D> SecondaryRenderTarget;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Secondary Layer", meta = (EditCondition = "bEnableSecondaryLayer"))
    float SecondaryHeight = 0.0f;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Secondary Layer", meta = (ClampMin = "16", ClampMax = "4096", EditCondition = "bEnableSecondaryLayer"))
    int32 SecondaryTextureWidth = 512;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Secondary Layer", meta = (ClampMin = "16", ClampMax = "4096", EditCondition = "bEnableSecondaryLayer"))
    int32 SecondaryTextureHeight = 512;
    
    UPROPERTY(BlueprintReadOnly, Category = "Secondary Layer|Output")
    TObjectPtr<UTextureRenderTarget2D> SecondaryTexture;
    
    UPROPERTY(BlueprintAssignable, Category = "Secondary Layer")
    FOnPlanarTextureBaked OnSecondaryBakeComplete;

    // === Processing ===
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Processing")
    bool bRemapNegativeToPositive = true;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Processing")
    bool bAutoNormalize = true;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Processing")
    bool bInvertResult = false;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Processing", meta = (ClampMin = "0.1", ClampMax = "100.0"))
    float ResultMultiplier = 1.0f;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Processing")
    bool bUseHDR = false;

    // === Functions ===
    
    UFUNCTION(BlueprintCallable, Category = "VCET|Planar Texture")
    void ForceRebake();
    
    UFUNCTION(BlueprintCallable, Category = "VCET|Planar Texture")
    void ForceRebakePrimary();
    
    UFUNCTION(BlueprintCallable, Category = "VCET|Planar Texture")
    void ForceRebakeSecondary();
    
    UFUNCTION(BlueprintCallable, Category = "VCET|Planar Texture", meta = (WorldContext = "WorldContextObject"))
    static void RequestGlobalRebake(UObject* WorldContextObject);
    
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "VCET|Planar Texture")
    UTextureRenderTarget2D* GetPrimaryTexture() const { return PrimaryTexture; }
    
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "VCET|Planar Texture")
    UTextureRenderTarget2D* GetSecondaryTexture() const { return SecondaryTexture; }
    
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "VCET|Planar Texture")
    bool IsBaking() const { return bIsBakingPrimary || bIsBakingSecondary; }

protected:
    virtual void BeginPlay() override;
    
private:
    bool bIsBakingPrimary = false;
    bool bIsBakingSecondary = false;
    
    void CreateRT(TObjectPtr<UTextureRenderTarget2D>& Out, UTextureRenderTarget2D* External, int32 W, int32 H);
    void BakeLayer(bool bPrimary, UVoxelMetadata* Meta, UTextureRenderTarget2D* RT, float Z, int32 W, int32 H);
    void WriteGrayscale(UTextureRenderTarget2D* RT, const TArray<float>& V, int32 W, int32 H);
    void WriteColor(UTextureRenderTarget2D* RT, const TArray<FLinearColor>& C, int32 W, int32 H);
};
