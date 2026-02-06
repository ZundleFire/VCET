// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "VCETMaterialFunctionLibrary.generated.h"

class UMaterialFunction;

/**
 * Utility functions for creating VCET material functions.
 */
UCLASS()
class VCET_API UVCETMaterialFunctionLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    /**
     * Get the Custom HLSL code for spherical cloud animation.
     * Copy this into a Custom node in your material.
     */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "VCET|Material")
    static FString GetCloudAnimationCustomNodeCode();

    /**
     * Get instructions for setting up the cloud animation in a material.
     */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "VCET|Material")
    static FString GetMaterialSetupInstructions();
};
