// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialParameterCollection.h"
#include "SphericalCloudAnimator.generated.h"

/**
 * Parameters for spherical cloud animation.
 * Use with Material Parameter Collection to drive cloud movement.
 */
USTRUCT(BlueprintType)
struct VCET_API FSphericalCloudAnimParams
{
    GENERATED_BODY()

    /** Base rotation speed at equator (UV units per second) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind")
    float EquatorWindSpeed = 0.02f;

    /** Rotation speed at poles (UV units per second) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind")
    float PolarWindSpeed = 0.005f;

    /** Latitude where wind reverses direction (0-1, where 0.5 is equator) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind", meta = (ClampMin = "0", ClampMax = "1"))
    float WindReversalLatitude = 0.3f;

    /** Strength of flowmap distortion */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Swirl")
    float FlowmapStrength = 0.1f;

    /** Speed of flowmap animation cycle */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Swirl")
    float FlowmapSpeed = 0.1f;

    /** Strength of curl noise turbulence */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turbulence")
    float TurbulenceStrength = 0.03f;

    /** Scale of curl noise pattern */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turbulence")
    float TurbulenceScale = 2.0f;

    /** Speed of turbulence animation */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turbulence")
    float TurbulenceSpeed = 0.01f;

    FSphericalCloudAnimParams()
    {
    }
};

/**
 * Result of spherical cloud UV animation calculation.
 */
USTRUCT(BlueprintType)
struct VCET_API FSphericalCloudUVResult
{
    GENERATED_BODY()

    /** Primary animated UV */
    UPROPERTY(BlueprintReadOnly, Category = "Result")
    FVector2D UV1 = FVector2D::ZeroVector;

    /** Secondary animated UV for flowmap blending */
    UPROPERTY(BlueprintReadOnly, Category = "Result")
    FVector2D UV2 = FVector2D::ZeroVector;

    /** Blend factor between UV1 and UV2 (for flowmap animation) */
    UPROPERTY(BlueprintReadOnly, Category = "Result")
    float BlendFactor = 0.0f;
};

/**
 * Blueprint Function Library for spherical cloud animation.
 * 
 * Use these functions to calculate animated UVs for cloud textures on spherical planets.
 * The results can be passed to materials via Material Parameter Collection or dynamic material instances.
 * 
 * For best results, use the Material Function "MF_SphericalCloudAnim" which implements
 * all animation in the shader for per-pixel accuracy.
 */
UCLASS()
class VCET_API USphericalCloudAnimatorLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    /**
     * Calculate animated UV coordinates for spherical cloud texture.
     * Call this per-frame and pass results to your material.
     * 
     * @param InputUV - Original UV coordinates (typically from equirectangular projection)
     * @param Time - Current world time in seconds
     * @param Params - Animation parameters
     * @return Animated UV result with blend information
     */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "VCET|Cloud Animation")
    static FSphericalCloudUVResult CalculateAnimatedCloudUV(
        FVector2D InputUV,
        float Time,
        const FSphericalCloudAnimParams& Params);

    /**
     * Get the wind direction multiplier based on latitude.
     * Simulates trade winds (east at equator) and westerlies (west at mid-latitudes).
     * 
     * @param Latitude - Normalized latitude (0 = south pole, 0.5 = equator, 1 = north pole)
     * @param ReversalLatitude - Latitude band where wind reverses (distance from equator)
     * @return Wind direction multiplier (-1 to 1)
     */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "VCET|Cloud Animation")
    static float GetWindDirectionAtLatitude(float Latitude, float ReversalLatitude = 0.3f);

    /**
     * Get wind speed multiplier based on latitude.
     * Faster at equator, slower at poles (Coriolis effect).
     * 
     * @param Latitude - Normalized latitude (0 = south pole, 0.5 = equator, 1 = north pole)
     * @return Speed multiplier (0-1)
     */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "VCET|Cloud Animation")
    static float GetWindSpeedAtLatitude(float Latitude);

    /**
     * Apply flowmap distortion to UV coordinates.
     * Uses two-phase blending to prevent texture stretching.
     * 
     * @param InputUV - Original UV coordinates
     * @param FlowDirection - Flow direction from flowmap (typically RG channels, remapped to -1,1)
     * @param Time - Current time
     * @param Strength - Distortion strength
     * @param Speed - Animation speed
     * @param OutUV1 - First phase UV
     * @param OutUV2 - Second phase UV
     * @param OutBlend - Blend factor between phases
     */
    UFUNCTION(BlueprintCallable, Category = "VCET|Cloud Animation")
    static void ApplyFlowmapDistortion(
        FVector2D InputUV,
        FVector2D FlowDirection,
        float Time,
        float Strength,
        float Speed,
        FVector2D& OutUV1,
        FVector2D& OutUV2,
        float& OutBlend);

    /**
     * Generate HLSL code for the cloud animation material function.
     * Useful for creating custom material functions.
     */
    UFUNCTION(BlueprintCallable, Category = "VCET|Cloud Animation|Utility")
    static FString GetCloudAnimationHLSL();

    /**
     * Update a Material Parameter Collection with cloud animation values.
     * Call this per-frame from your game mode or weather system.
     * 
     * @param WorldContext - World context object
     * @param Collection - The MPC to update
     * @param Time - Current world time
     * @param Params - Animation parameters
     */
    UFUNCTION(BlueprintCallable, Category = "VCET|Cloud Animation", meta = (WorldContext = "WorldContext"))
    static void UpdateCloudAnimationMPC(
        UObject* WorldContext,
        UMaterialParameterCollection* Collection,
        float Time,
        const FSphericalCloudAnimParams& Params);
};
