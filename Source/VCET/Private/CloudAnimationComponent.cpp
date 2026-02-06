// Copyright Epic Games, Inc. All Rights Reserved.

#include "CloudAnimationComponent.h"
#include "Materials/MaterialParameterCollection.h"

UCloudAnimationComponent::UCloudAnimationComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

void UCloudAnimationComponent::BeginPlay()
{
    Super::BeginPlay();
    
    if (bUseGameTime)
    {
        CurrentTime = GetWorld()->GetTimeSeconds();
    }
    else
    {
        CurrentTime = CustomTime;
    }
    
    // Initial update
    ForceUpdate();
}

void UCloudAnimationComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    
    if (bPaused)
    {
        return;
    }
    
    // Update time
    if (bUseGameTime)
    {
        CurrentTime = GetWorld()->GetTimeSeconds() * TimeScale;
    }
    else
    {
        CurrentTime = CustomTime * TimeScale;
    }
    
    // Check update frequency
    if (UpdateFrequency > 0.0f)
    {
        TimeSinceLastUpdate += DeltaTime;
        float UpdateInterval = 1.0f / UpdateFrequency;
        
        if (TimeSinceLastUpdate < UpdateInterval)
        {
            return;
        }
        
        TimeSinceLastUpdate = FMath::Fmod(TimeSinceLastUpdate, UpdateInterval);
    }
    
    UpdateMPC();
}

void UCloudAnimationComponent::SetCustomTime(float NewTime)
{
    bUseGameTime = false;
    CustomTime = NewTime;
}

void UCloudAnimationComponent::ForceUpdate()
{
    UpdateMPC();
}

void UCloudAnimationComponent::UpdateMPC()
{
    if (!CloudMPC)
    {
        return;
    }
    
    USphericalCloudAnimatorLibrary::UpdateCloudAnimationMPC(
        this,
        CloudMPC,
        CurrentTime,
        AnimParams
    );
}
