// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class VCET : ModuleRules
{
    public VCET(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "VoxelCore",
            "VoxelGraph",
            "Voxel",
            "RenderCore",
            "RHI"
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "Projects",
            "MaterialShaderQualitySettings"
        });

        // Editor-only dependencies for material expressions
        if (Target.bBuildEditor)
        {
            PrivateDependencyModuleNames.AddRange(new string[]
            {
                "UnrealEd",
                "MaterialEditor"
            });
        }
    }
}
