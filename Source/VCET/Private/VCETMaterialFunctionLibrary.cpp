// Copyright Epic Games, Inc. All Rights Reserved.

#include "VCETMaterialFunctionLibrary.h"

FString UVCETMaterialFunctionLibrary::GetCloudAnimationCustomNodeCode()
{
    return TEXT(R"(
// VCET Spherical Cloud Animation
// Inputs: UV (float2), Time (float), EquatorSpeed (float), PolarSpeed (float), TurbStrength (float), TurbScale (float)
// Output Type: CMOT Float 2

float Latitude = UV.y;
float DistFromEquator = abs(Latitude - 0.5);

// Wind direction: Trade winds (east) near equator, Westerlies (west) at mid-latitudes
float WindDir = 1.0;
if (DistFromEquator > 0.15 && DistFromEquator < 0.35)
    WindDir = -1.0;

// Wind speed: Faster at equator due to Coriolis effect
float SpeedMult = 1.0 - pow(DistFromEquator * 2.0, 2.0);
float WindSpeed = lerp(PolarSpeed, EquatorSpeed, SpeedMult);

// Base wind offset
float2 Offset = float2(Time * WindSpeed * WindDir, 0);

// Turbulence (curl noise approximation)
float TurbPhase = Time * 0.01;
float2 Turb;
Turb.x = sin(UV.x * TurbScale * 6.28318 + TurbPhase) * TurbStrength;
Turb.y = cos(UV.y * TurbScale * 6.28318 + TurbPhase * 1.3) * TurbStrength * 0.5;

// Final animated UV
float2 AnimatedUV = UV + Offset + Turb;
AnimatedUV.x = frac(AnimatedUV.x);  // Wrap longitude
AnimatedUV.y = saturate(AnimatedUV.y);  // Clamp latitude

return AnimatedUV;
)");
}

FString UVCETMaterialFunctionLibrary::GetMaterialSetupInstructions()
{
    return TEXT(R"(
=== VCET SPHERICAL CLOUD ANIMATION SETUP ===

STEP 1: Create Material Parameter Collection
-------------------------------------------
1. Right-click in Content Browser > Materials > Material Parameter Collection
2. Name it "MPC_CloudAnimation"
3. Add these Scalar Parameters:
   - CloudTime (default: 0)
   - EquatorWindSpeed (default: 0.02)
   - PolarWindSpeed (default: 0.005)
   - TurbulenceStrength (default: 0.03)
   - TurbulenceScale (default: 2.0)

STEP 2: Add Cloud Animation Component
-------------------------------------
1. Add "VCET Cloud Animation" component to your sky sphere or game mode
2. Assign your MPC_CloudAnimation to the CloudMPC property
3. The component will update CloudTime automatically each frame

STEP 3: In Your Cloud Material
------------------------------
1. Add a "Custom" node
2. Set Output Type to "CMOT Float 2"
3. Add these inputs:
   - UV (Vector2) - connect your cloud texture UVs
   - Time (Scalar) - connect Collection Parameter "CloudTime"
   - EquatorSpeed (Scalar) - connect Collection Parameter "EquatorWindSpeed"
   - PolarSpeed (Scalar) - connect Collection Parameter "PolarWindSpeed"
   - TurbStrength (Scalar) - connect Collection Parameter "TurbulenceStrength"
   - TurbScale (Scalar) - connect Collection Parameter "TurbulenceScale"
4. Paste the HLSL code from GetCloudAnimationCustomNodeCode()
5. Connect the Custom node output to your cloud texture's UV input

STEP 4: For Volumetric Clouds
-----------------------------
If using UE5 Volumetric Clouds:
1. In your Volumetric Cloud material, find the Extinction/Density texture sample
2. Replace its UV input with the animated UV from the Custom node
3. The clouds will now animate with realistic wind patterns!

ALTERNATIVE: Two-Phase Blending (Smoother)
------------------------------------------
For smoother animation without stretching:
1. Sample your cloud texture TWICE with the animated UV
2. Offset the second sample slightly: AnimatedUV + float2(0.1, 0)
3. Lerp between them using: abs(frac(Time * 0.1) - 0.5) * 2.0
)");
}
