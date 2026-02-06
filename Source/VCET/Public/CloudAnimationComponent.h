// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SphericalCloudAnimator.h"
#include "CloudAnimationComponent.generated.h"

class UMaterialParameterCollection;

/**
 * Component that automatically updates cloud animation parameters each frame.
 * Attach to your sky sphere, weather manager, or game mode actor.
 * 
 * This component updates a Material Parameter Collection with animated values
 * that can be used in your volumetric cloud material for realistic movement.
 */
UCLASS(ClassGroup=(VCET), meta=(BlueprintSpawnableComponent), DisplayName="VCET Cloud Animation")
class VCET_API UCloudAnimationComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UCloudAnimationComponent();

    // === Material Parameter Collection ===
    
    /**
     * The Material Parameter Collection to update with animation values.
     * Create an MPC with these scalar parameters:
     * - CloudTime
     * - EquatorWindSpeed, PolarWindSpeed, WindReversalLatitude
     * - FlowmapStrength, FlowmapSpeed
     * - TurbulenceStrength, TurbulenceScale, TurbulenceSpeed
     * - FlowPhase1, FlowPhase2, FlowBlendFactor
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cloud Animation")
    TObjectPtr<UMaterialParameterCollection> CloudMPC;

    // === Animation Parameters ===
    
    /** Cloud animation parameters */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cloud Animation")
    FSphericalCloudAnimParams AnimParams;

    // === Time Control ===
    
    /** Use game time or custom time */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Time")
    bool bUseGameTime = true;
    
    /** Custom time value (used when bUseGameTime is false) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Time", meta = (EditCondition = "!bUseGameTime"))
    float CustomTime = 0.0f;
    
    /** Time scale multiplier */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Time")
    float TimeScale = 1.0f;
    
    /** Pause animation */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Time")
    bool bPaused = false;

    // === Advanced ===
    
    /** Update frequency (updates per second). 0 = every frame */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Advanced", meta = (ClampMin = "0", ClampMax = "120"))
    float UpdateFrequency = 0.0f;

    // === Functions ===
    
    /** Get the current animation time */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "VCET|Cloud Animation")
    float GetCurrentTime() const { return CurrentTime; }
    
    /** Set custom time (also sets bUseGameTime to false) */
    UFUNCTION(BlueprintCallable, Category = "VCET|Cloud Animation")
    void SetCustomTime(float NewTime);
    
    /** Force an immediate update of the MPC */
    UFUNCTION(BlueprintCallable, Category = "VCET|Cloud Animation")
    void ForceUpdate();

protected:
    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
    float CurrentTime = 0.0f;
    float TimeSinceLastUpdate = 0.0f;
    
    void UpdateMPC();
};
