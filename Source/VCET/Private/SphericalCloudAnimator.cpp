// Copyright Epic Games, Inc. All Rights Reserved.

#include "SphericalCloudAnimator.h"
#include "Kismet/KismetMaterialLibrary.h"
#include "Materials/MaterialParameterCollectionInstance.h"

FSphericalCloudUVResult USphericalCloudAnimatorLibrary::CalculateAnimatedCloudUV(
    FVector2D InputUV,
    float Time,
    const FSphericalCloudAnimParams& Params)
{
    FSphericalCloudUVResult Result;
    
    // Calculate latitude (0 at poles, 1 at equator when using abs)
    float Latitude = InputUV.Y; // 0 = south, 0.5 = equator, 1 = north
    
    // Get wind parameters
    float WindDir = GetWindDirectionAtLatitude(Latitude, Params.WindReversalLatitude);
    float WindSpeedMult = GetWindSpeedAtLatitude(Latitude);
    float WindSpeed = FMath::Lerp(Params.PolarWindSpeed, Params.EquatorWindSpeed, WindSpeedMult);
    
    // Apply base wind offset
    FVector2D BaseOffset;
    BaseOffset.X = Time * WindSpeed * WindDir;
    BaseOffset.Y = 0.0f;
    
    // Simple turbulence approximation (would be better with actual curl noise texture)
    float TurbPhase = Time * Params.TurbulenceSpeed;
    FVector2D TurbOffset;
    TurbOffset.X = FMath::Sin(InputUV.X * Params.TurbulenceScale * 6.28f + TurbPhase) * Params.TurbulenceStrength;
    TurbOffset.Y = FMath::Cos(InputUV.Y * Params.TurbulenceScale * 6.28f + TurbPhase * 1.3f) * Params.TurbulenceStrength * 0.5f;
    
    // Calculate flowmap phase for two-phase blending
    float Phase = FMath::Frac(Time * Params.FlowmapSpeed);
    float Phase2 = FMath::Frac(Phase + 0.5f);
    
    Result.UV1 = InputUV + BaseOffset + TurbOffset * Phase;
    Result.UV2 = InputUV + BaseOffset + TurbOffset * Phase2;
    Result.BlendFactor = FMath::Abs(Phase - 0.5f) * 2.0f;
    
    // Wrap UVs
    Result.UV1.X = FMath::Frac(Result.UV1.X);
    Result.UV1.Y = FMath::Clamp(Result.UV1.Y, 0.0f, 1.0f);
    Result.UV2.X = FMath::Frac(Result.UV2.X);
    Result.UV2.Y = FMath::Clamp(Result.UV2.Y, 0.0f, 1.0f);
    
    return Result;
}

float USphericalCloudAnimatorLibrary::GetWindDirectionAtLatitude(float Latitude, float ReversalLatitude)
{
    // Distance from equator (0-0.5)
    float DistFromEquator = FMath::Abs(Latitude - 0.5f);
    
    // Trade winds near equator blow east (+1)
    // Westerlies at mid-latitudes blow west (-1)
    // Polar easterlies at high latitudes blow east (+1)
    
    if (DistFromEquator < ReversalLatitude)
    {
        // Trade winds zone - blow east
        return 1.0f;
    }
    else if (DistFromEquator < ReversalLatitude * 2.0f)
    {
        // Westerlies zone - blow west
        return -1.0f;
    }
    else
    {
        // Polar zone - blow east but weaker
        return 0.5f;
    }
}

float USphericalCloudAnimatorLibrary::GetWindSpeedAtLatitude(float Latitude)
{
    // Faster at equator, slower at poles
    float DistFromEquator = FMath::Abs(Latitude - 0.5f) * 2.0f; // 0 at equator, 1 at poles
    return 1.0f - DistFromEquator * DistFromEquator; // Quadratic falloff
}

void USphericalCloudAnimatorLibrary::ApplyFlowmapDistortion(
    FVector2D InputUV,
    FVector2D FlowDirection,
    float Time,
    float Strength,
    float Speed,
    FVector2D& OutUV1,
    FVector2D& OutUV2,
    float& OutBlend)
{
    float Phase = FMath::Frac(Time * Speed);
    float Phase2 = FMath::Frac(Phase + 0.5f);
    
    OutUV1 = InputUV + FlowDirection * Phase * Strength;
    OutUV2 = InputUV + FlowDirection * Phase2 * Strength;
    OutBlend = FMath::Abs(Phase - 0.5f) * 2.0f;
}

FString USphericalCloudAnimatorLibrary::GetCloudAnimationHLSL()
{
    return TEXT(R"(
// ============================================
// VCET Spherical Cloud Animation - HLSL
// ============================================
// Copy this into a Custom node or Material Function

// Inputs:
// UV - Equirectangular UV coordinates
// Time - World time seconds
// EquatorSpeed - Wind speed at equator
// PolarSpeed - Wind speed at poles  
// FlowmapStrength - Flowmap distortion strength
// TurbulenceStrength - Curl noise strength
// TurbulenceScale - Curl noise frequency

// Get latitude from V coordinate
float Latitude = UV.y;
float DistFromEquator = abs(Latitude - 0.5);

// Wind direction based on latitude (trade winds, westerlies, polar easterlies)
float WindDir = 1.0;
if (DistFromEquator > 0.15 && DistFromEquator < 0.35)
    WindDir = -1.0; // Westerlies
    
// Wind speed (faster at equator)
float SpeedMult = 1.0 - pow(DistFromEquator * 2.0, 2.0);
float WindSpeed = lerp(PolarSpeed, EquatorSpeed, SpeedMult);

// Base wind offset
float2 Offset = float2(Time * WindSpeed * WindDir, 0);

// Turbulence (simplified curl noise approximation)
float TurbPhase = Time * 0.01;
float2 TurbOffset;
TurbOffset.x = sin(UV.x * TurbulenceScale * 6.28 + TurbPhase) * TurbulenceStrength;
TurbOffset.y = cos(UV.y * TurbulenceScale * 6.28 + TurbPhase * 1.3) * TurbulenceStrength * 0.5;

// Two-phase flowmap blending
float Phase1 = frac(Time * 0.1);
float Phase2 = frac(Phase1 + 0.5);
float BlendFactor = abs(Phase1 - 0.5) * 2.0;

float2 UV1 = UV + Offset + TurbOffset * Phase1;
float2 UV2 = UV + Offset + TurbOffset * Phase2;

// Wrap U coordinate (longitude), clamp V (latitude)
UV1.x = frac(UV1.x);
UV2.x = frac(UV2.x);
UV1.y = saturate(UV1.y);
UV2.y = saturate(UV2.y);

// Sample cloud texture with both UVs and blend
float4 Cloud1 = CloudTexture.Sample(CloudSampler, UV1);
float4 Cloud2 = CloudTexture.Sample(CloudSampler, UV2);
float4 FinalCloud = lerp(Cloud1, Cloud2, BlendFactor);

return FinalCloud;
)");
}

void USphericalCloudAnimatorLibrary::UpdateCloudAnimationMPC(
    UObject* WorldContext,
    UMaterialParameterCollection* Collection,
    float Time,
    const FSphericalCloudAnimParams& Params)
{
    if (!WorldContext || !Collection)
    {
        return;
    }
    
    UWorld* World = GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::LogAndReturnNull);
    if (!World)
    {
        return;
    }
    
    // Set scalar parameters
    UKismetMaterialLibrary::SetScalarParameterValue(World, Collection, FName("CloudTime"), Time);
    UKismetMaterialLibrary::SetScalarParameterValue(World, Collection, FName("EquatorWindSpeed"), Params.EquatorWindSpeed);
    UKismetMaterialLibrary::SetScalarParameterValue(World, Collection, FName("PolarWindSpeed"), Params.PolarWindSpeed);
    UKismetMaterialLibrary::SetScalarParameterValue(World, Collection, FName("WindReversalLatitude"), Params.WindReversalLatitude);
    UKismetMaterialLibrary::SetScalarParameterValue(World, Collection, FName("FlowmapStrength"), Params.FlowmapStrength);
    UKismetMaterialLibrary::SetScalarParameterValue(World, Collection, FName("FlowmapSpeed"), Params.FlowmapSpeed);
    UKismetMaterialLibrary::SetScalarParameterValue(World, Collection, FName("TurbulenceStrength"), Params.TurbulenceStrength);
    UKismetMaterialLibrary::SetScalarParameterValue(World, Collection, FName("TurbulenceScale"), Params.TurbulenceScale);
    UKismetMaterialLibrary::SetScalarParameterValue(World, Collection, FName("TurbulenceSpeed"), Params.TurbulenceSpeed);
    
    // Pre-calculate some values for the shader
    float Phase1 = FMath::Frac(Time * Params.FlowmapSpeed);
    float Phase2 = FMath::Frac(Phase1 + 0.5f);
    float BlendFactor = FMath::Abs(Phase1 - 0.5f) * 2.0f;
    
    UKismetMaterialLibrary::SetScalarParameterValue(World, Collection, FName("FlowPhase1"), Phase1);
    UKismetMaterialLibrary::SetScalarParameterValue(World, Collection, FName("FlowPhase2"), Phase2);
    UKismetMaterialLibrary::SetScalarParameterValue(World, Collection, FName("FlowBlendFactor"), BlendFactor);
}
