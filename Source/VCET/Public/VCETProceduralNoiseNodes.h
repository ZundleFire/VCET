// Copyright Zundle. MIT License.

#pragma once

#include "VoxelMinimal.h"
#include "VoxelNode.h"
#include "Buffer/VoxelFloatBuffers.h"
#include "VCETProceduralNoiseNodes.generated.h"

// Noise types ported from the Procedural Noise Collection by @lumiey (MIT)
// https://fragcoord.xyz/s/pxmcvnpc
UENUM(BlueprintType, DisplayName = "Procedural Noise Type 2D")
enum class EVoxelProceduralNoiseType2D : uint8
{
	Default = 200,
	Perlin = 0 UMETA(ToolTip = "Classic gradient noise"),
	Simplex = 1 UMETA(ToolTip = "Simplex gradient noise"),
	Value = 2 UMETA(ToolTip = "Smoothly interpolated per-cell random values"),
	Worley = 3 UMETA(ToolTip = "Inverted distance to closest feature point (cellular F1)"),
	Voronoi = 4 UMETA(ToolTip = "Smoothly blended per-cell random values, uses Voronoi Smoothness"),
	Blue = 5 UMETA(ToolTip = "High-pass filtered white noise, one value per unit cell"),
	HilbertBlue = 6 UMETA(ToolTip = "Low-discrepancy noise following a Hilbert curve, one value per unit cell"),
	Crater = 7 UMETA(ToolTip = "Overlapping radial rings, similar to impact craters"),
	Gabor = 8 UMETA(ToolTip = "Interpolated randomly oriented sine kernels"),
	Curl = 9 UMETA(ToolTip = "Magnitude of the curl of Perlin noise"),
	Scratch = 10 UMETA(ToolTip = "Layered thin wavy lines, similar to surface scratches. Uses Scratch Smoothness"),
	Wavelet = 11 UMETA(ToolTip = "Rotated sine wavelets, uses Wavelet Phase to animate"),
	Erosion = 12 UMETA(ToolTip = "Perlin noise with slope-following gullies carved in, similar to hydraulic erosion"),
	Paper = 13 UMETA(ToolTip = "Fibrous multi-octave gradient length, similar to paper grain"),
	Stone = 14 UMETA(ToolTip = "Gradient-warped fractal Perlin, similar to stone surfaces"),
	Wool = 15 UMETA(ToolTip = "Max of absolute averaged Perlin gradients, similar to felted wool"),
	InterleavedGradient = 16 UMETA(ToolTip = "Interleaved gradient noise, a fast dither-style noise"),
};

// Noise types ported from the Procedural Noise Collection by @lumiey (MIT)
// https://fragcoord.xyz/s/pxmcvnpc
// The inherently 2D types (Voronoi, HilbertBlue, Crater, Gabor, Scratch, Wavelet, Erosion, Paper, Stone, Wool)
// use 3D extensions of the original algorithms
UENUM(BlueprintType, DisplayName = "Procedural Noise Type 3D")
enum class EVoxelProceduralNoiseType3D : uint8
{
	Default = 200,
	Perlin = 0 UMETA(ToolTip = "Classic gradient noise"),
	Simplex = 1 UMETA(ToolTip = "Simplex gradient noise"),
	Value = 2 UMETA(ToolTip = "Smoothly interpolated per-cell random values"),
	Worley = 3 UMETA(ToolTip = "Inverted distance to closest feature point (cellular F1)"),
	Voronoi = 4 UMETA(ToolTip = "Smoothly blended per-cell random values, uses Voronoi Smoothness"),
	Blue = 5 UMETA(ToolTip = "High-pass filtered white noise, one value per unit cell"),
	HilbertBlue = 6 UMETA(ToolTip = "Low-discrepancy noise following a 3D Hilbert curve, one value per unit cell"),
	Crater = 7 UMETA(ToolTip = "Overlapping radial shells, similar to impact craters"),
	Gabor = 8 UMETA(ToolTip = "Interpolated randomly oriented sine kernels"),
	Curl = 9 UMETA(ToolTip = "Magnitude of the gradient of Perlin noise"),
	Scratch = 10 UMETA(ToolTip = "Layered thin wavy strands, similar to surface scratches. Uses Scratch Smoothness"),
	Wavelet = 11 UMETA(ToolTip = "Rotated sine wavelets, uses Wavelet Phase to animate"),
	Erosion = 12 UMETA(ToolTip = "Perlin noise with slope-following gullies carved in, similar to hydraulic erosion"),
	Paper = 13 UMETA(ToolTip = "Fibrous multi-octave gradient length, similar to paper grain"),
	Stone = 14 UMETA(ToolTip = "Gradient-warped fractal Perlin, similar to stone surfaces"),
	Wool = 15 UMETA(ToolTip = "Max of absolute averaged Perlin gradients, similar to felted wool"),
	InterleavedGradient = 16 UMETA(ToolTip = "Interleaved gradient noise, a fast dither-style noise"),
};

// Generates multi-octave height noise from a collection of stylized procedural noises
USTRUCT(Category = "Noise")
struct VCET_API FVoxelNode_ProceduralNoise2D : public FVoxelNode
{
	GENERATED_BODY()
	GENERATED_VOXEL_NODE_BODY()

public:
	// Position at which to calculate output noise
	VOXEL_INPUT_PIN(FVoxelVector2DBuffer, Position, nullptr, PositionPin);
	// Height difference of the lowest and highest point of the noise's largest octave
	VOXEL_INPUT_PIN(FVoxelFloatBuffer, Amplitude, 10000.f);
	// Amount of space the noise will take to tile in the world, a divisor for position
	VOXEL_INPUT_PIN(FVoxelFloatBuffer, FeatureScale, 100000.f);
	// A factor for how much smaller each octave's feature scale is compared to last octave's
	VOXEL_INPUT_PIN(FVoxelFloatBuffer, Lacunarity, 2.f);
	// A factor for how much smaller each octave's amplitude is compared to last octave's
	VOXEL_INPUT_PIN(FVoxelFloatBuffer, Gain, 0.5f);
	// Edge smoothness of the cell blending when using Voronoi noise
	VOXEL_INPUT_PIN(FVoxelFloatBuffer, VoronoiSmoothness, 1.f);
	// Phase offset of the wavelets when using Wavelet noise, can be used to animate it
	VOXEL_INPUT_PIN(FVoxelFloatBuffer, WaveletPhase, 0.f);
	// Edge smoothness of the lines when using Scratch noise
	VOXEL_INPUT_PIN(FVoxelFloatBuffer, ScratchSmoothness, 0.05f);
	// Amount of layers this noise should have
	VOXEL_INPUT_PIN(int32, NumOctaves, 10);
	// Used to randomize the output noise
	VOXEL_INPUT_PIN(FVoxelSeed, Seed, nullptr);
	// Default noise type
	VOXEL_INPUT_PIN(EVoxelProceduralNoiseType2D, DefaultNoiseType, EVoxelProceduralNoiseType2D::Perlin, ShowInDetail);
	// Noise type to use for generating a given octave
	VOXEL_VARIADIC_INPUT_PIN(EVoxelProceduralNoiseType2D, OctaveType, EVoxelProceduralNoiseType2D::Default, 0, ShowInDetail);
	// Multiplier for the amplitude of a given octave
	VOXEL_VARIADIC_INPUT_PIN(FVoxelFloatBuffer, OctaveStrength, 1.f, 0, ShowInDetail);

	// Result of all octaves being added together
	VOXEL_OUTPUT_PIN(FVoxelFloatBuffer, Value);

	//~ Begin FVoxelNode Interface
	virtual void Compute(FVoxelGraphQuery Query) const override;
	//~ End FVoxelNode Interface
};

// Generates multi-octave density noise from a collection of stylized procedural noises
USTRUCT(Category = "Noise")
struct VCET_API FVoxelNode_ProceduralNoise3D : public FVoxelNode
{
	GENERATED_BODY()
	GENERATED_VOXEL_NODE_BODY()

public:
	// Position at which to calculate output noise
	VOXEL_INPUT_PIN(FVoxelVectorBuffer, Position, nullptr, PositionPin);
	// Height difference of the lowest and highest point of the noise's largest octave
	VOXEL_INPUT_PIN(FVoxelFloatBuffer, Amplitude, 10000.f);
	// Amount of space the noise will take to tile in the world, a divisor for position
	VOXEL_INPUT_PIN(FVoxelFloatBuffer, FeatureScale, 100000.f);
	// A factor for how much smaller each octave's feature scale is compared to last octave's
	VOXEL_INPUT_PIN(FVoxelFloatBuffer, Lacunarity, 2.f);
	// A factor for how much smaller each octave's amplitude is compared to last octave's
	VOXEL_INPUT_PIN(FVoxelFloatBuffer, Gain, 0.5f);
	// Edge smoothness of the cell blending when using Voronoi noise
	VOXEL_INPUT_PIN(FVoxelFloatBuffer, VoronoiSmoothness, 1.f);
	// Phase offset of the wavelets when using Wavelet noise, can be used to animate it
	VOXEL_INPUT_PIN(FVoxelFloatBuffer, WaveletPhase, 0.f);
	// Edge smoothness of the strands when using Scratch noise
	VOXEL_INPUT_PIN(FVoxelFloatBuffer, ScratchSmoothness, 0.05f);
	// Amount of layers this noise should have
	VOXEL_INPUT_PIN(int32, NumOctaves, 10);
	// Used to randomize the output noise
	VOXEL_INPUT_PIN(FVoxelSeed, Seed, nullptr);
	// Default noise type
	VOXEL_INPUT_PIN(EVoxelProceduralNoiseType3D, DefaultNoiseType, EVoxelProceduralNoiseType3D::Perlin, ShowInDetail);
	// Noise type to use for generating a given octave
	VOXEL_VARIADIC_INPUT_PIN(EVoxelProceduralNoiseType3D, OctaveType, EVoxelProceduralNoiseType3D::Default, 0, ShowInDetail);
	// Multiplier for the amplitude of a given octave
	VOXEL_VARIADIC_INPUT_PIN(FVoxelFloatBuffer, OctaveStrength, 1.f, 0, ShowInDetail);

	// Result of all octaves being added together
	VOXEL_OUTPUT_PIN(FVoxelFloatBuffer, Value);

	//~ Begin FVoxelNode Interface
	virtual void Compute(FVoxelGraphQuery Query) const override;
	//~ End FVoxelNode Interface
};
