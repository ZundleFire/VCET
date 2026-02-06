// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/TextureRenderTarget2D.h"
#include "VoxelMinimal.h"
#include "VoxelStackLayer.h"
#include "VoxelQueryBlueprintLibrary.h"
#include "SphericalTextureBaker.generated.h"

class UVoxelLinearColorMetadata;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSphericalTextureBaked);

/**
 * Bakes Voxel VOLUME layer data to equirectangular render targets.
 * Samples 3D positions on a sphere surface and writes to 2D textures.
 * 
 * Use for planetary/spherical worlds to bake clouds, terrain colors, etc.
 * 
 * Features:
 * - Multiple layers at different radii (Cloud, Land)
 * - RGBA output via Linear Color Metadata or grayscale
 * - External or auto-created render targets
 */
UCLASS(ClassGroup=(VCET), meta=(BlueprintSpawnableComponent), DisplayName="VCET Spherical Texture Baker")
class VCET_API USphericalTextureBaker : public UActorComponent
{
    GENERATED_BODY()

public:
    USphericalTextureBaker();

    // === Voxel ===
    
    /** The Voxel VOLUME layer to query */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel")
    FVoxelStackVolumeLayer VolumeLayer;

    // === Shared ===
    
    /** Center of the sphere */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shared")
    FVector SphereCenter = FVector::ZeroVector;
    
    /** Bake on BeginPlay */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shared")
    bool bBakeOnBeginPlay = false;

    // === Cloud Layer ===
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cloud Layer")
    bool bEnableCloudLayer = true;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cloud Layer", meta = (EditCondition = "bEnableCloudLayer"))
    TObjectPtr<UVoxelLinearColorMetadata> CloudColorMetadata;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cloud Layer", meta = (EditCondition = "bEnableCloudLayer"))
    TObjectPtr<UTextureRenderTarget2D> CloudRenderTarget;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cloud Layer", meta = (EditCondition = "bEnableCloudLayer"))
    float CloudRadius = 647100.0f;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cloud Layer", meta = (ClampMin = "16", ClampMax = "4096", EditCondition = "bEnableCloudLayer"))
    int32 CloudTextureWidth = 512;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cloud Layer", meta = (ClampMin = "16", ClampMax = "4096", EditCondition = "bEnableCloudLayer"))
    int32 CloudTextureHeight = 256;
    
    UPROPERTY(BlueprintReadOnly, Category = "Cloud Layer|Output")
    TObjectPtr<UTextureRenderTarget2D> CloudTexture;
    
    UPROPERTY(BlueprintAssignable, Category = "Cloud Layer")
    FOnSphericalTextureBaked OnCloudBakeComplete;

    // === Land Layer ===
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Land Layer")
    bool bEnableLandLayer = false;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Land Layer", meta = (EditCondition = "bEnableLandLayer"))
    TObjectPtr<UVoxelLinearColorMetadata> LandColorMetadata;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Land Layer", meta = (EditCondition = "bEnableLandLayer"))
    TObjectPtr<UTextureRenderTarget2D> LandRenderTarget;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Land Layer", meta = (EditCondition = "bEnableLandLayer"))
    float LandRadius = 637100.0f;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Land Layer", meta = (ClampMin = "16", ClampMax = "4096", EditCondition = "bEnableLandLayer"))
    int32 LandTextureWidth = 512;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Land Layer", meta = (ClampMin = "16", ClampMax = "4096", EditCondition = "bEnableLandLayer"))
    int32 LandTextureHeight = 256;
    
    UPROPERTY(BlueprintReadOnly, Category = "Land Layer|Output")
    TObjectPtr<UTextureRenderTarget2D> LandTexture;
    
    UPROPERTY(BlueprintAssignable, Category = "Land Layer")
    FOnSphericalTextureBaked OnLandBakeComplete;

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
    
    UFUNCTION(BlueprintCallable, Category = "VCET|Spherical Texture")
    void ForceRebake();
    
    UFUNCTION(BlueprintCallable, Category = "VCET|Spherical Texture")
    void ForceRebakeCloud();
    
    UFUNCTION(BlueprintCallable, Category = "VCET|Spherical Texture")
    void ForceRebakeLand();
    
    UFUNCTION(BlueprintCallable, Category = "VCET|Spherical Texture", meta = (WorldContext = "WorldContextObject"))
    static void RequestGlobalRebake(UObject* WorldContextObject);
    
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "VCET|Spherical Texture")
    UTextureRenderTarget2D* GetCloudTexture() const { return CloudTexture; }
    
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "VCET|Spherical Texture")
    UTextureRenderTarget2D* GetLandTexture() const { return LandTexture; }
    
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "VCET|Spherical Texture")
    bool IsBaking() const { return bIsBakingCloud || bIsBakingLand; }

protected:
    virtual void BeginPlay() override;
    
private:
    bool bIsBakingCloud = false;
    bool bIsBakingLand = false;
    
    void CreateRT(TObjectPtr<UTextureRenderTarget2D>& Out, UTextureRenderTarget2D* External, int32 W, int32 H);
    void BakeLayer(bool bCloud, UVoxelLinearColorMetadata* Meta, UTextureRenderTarget2D* RT, float R, int32 W, int32 H);
    void WriteGrayscale(UTextureRenderTarget2D* RT, const TArray<float>& V, int32 W, int32 H);
    void WriteColor(UTextureRenderTarget2D* RT, const TArray<FLinearColor>& C, int32 W, int32 H);
};
