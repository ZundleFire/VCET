// Copyright Zundle. MIT License.

#include "VCETProceduralNoiseNodes.h"
#include "VCETProceduralNoiseNodesImpl.ispc.generated.h"
#include "VoxelBufferAccessor.h"

FORCEINLINE ispc::EProceduralNoise2D GetISPCNoise(const EVoxelProceduralNoiseType2D Noise)
{
	switch (Noise)
	{
	default: ensure(false);

#define CASE(Name) case EVoxelProceduralNoiseType2D::Name: return ispc::ProceduralNoise2D_ ## Name;

	case EVoxelProceduralNoiseType2D::Default:
	CASE(Perlin);
	CASE(Simplex);
	CASE(Value);
	CASE(Worley);
	CASE(Voronoi);
	CASE(Blue);
	CASE(HilbertBlue);
	CASE(Crater);
	CASE(Gabor);
	CASE(Curl);
	CASE(Scratch);
	CASE(Wavelet);
	CASE(Erosion);
	CASE(Paper);
	CASE(Stone);
	CASE(Wool);
	CASE(InterleavedGradient);

#undef CASE
	}
}

FORCEINLINE ispc::EProceduralNoise3D GetISPCNoise(const EVoxelProceduralNoiseType3D Noise)
{
	switch (Noise)
	{
	default: ensure(false);

#define CASE(Name) case EVoxelProceduralNoiseType3D::Name: return ispc::ProceduralNoise3D_ ## Name;

	case EVoxelProceduralNoiseType3D::Default:
	CASE(Perlin);
	CASE(Simplex);
	CASE(Value);
	CASE(Worley);
	CASE(Voronoi);
	CASE(Blue);
	CASE(HilbertBlue);
	CASE(Crater);
	CASE(Gabor);
	CASE(Curl);
	CASE(Scratch);
	CASE(Wavelet);
	CASE(Erosion);
	CASE(Paper);
	CASE(Stone);
	CASE(Wool);
	CASE(InterleavedGradient);

#undef CASE
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelNode_ProceduralNoise2D::Compute(const FVoxelGraphQuery Query) const
{
	const TValue<FVoxelVector2DBuffer> Positions = PositionPin.Get(Query);
	const TValue<FVoxelFloatBuffer> Amplitudes = AmplitudePin.Get(Query);
	const TValue<FVoxelFloatBuffer> FeatureScales = FeatureScalePin.Get(Query);
	const TValue<FVoxelFloatBuffer> Lacunarities = LacunarityPin.Get(Query);
	const TValue<FVoxelFloatBuffer> Gains = GainPin.Get(Query);
	const TValue<FVoxelFloatBuffer> VoronoiSmoothnesses = VoronoiSmoothnessPin.Get(Query);
	const TValue<FVoxelFloatBuffer> WaveletPhases = WaveletPhasePin.Get(Query);
	const TValue<FVoxelFloatBuffer> ScratchSmoothnesses = ScratchSmoothnessPin.Get(Query);
	const TValue<int32> NumOctaves = NumOctavesPin.Get(Query);
	const TValue<FVoxelSeed> Seed = SeedPin.Get(Query);
	const TValue<EVoxelProceduralNoiseType2D> DefaultNoiseType = DefaultNoiseTypePin.Get(Query);
	const TVoxelArray<TValue<EVoxelProceduralNoiseType2D>> OctaveTypes = OctaveTypePins.Get(Query);
	const TVoxelArray<TValue<FVoxelFloatBuffer>> OctaveStrengths = OctaveStrengthPins.Get(Query);

	VOXEL_GRAPH_WAIT(Positions, Amplitudes, FeatureScales, Lacunarities, Gains, VoronoiSmoothnesses, WaveletPhases, ScratchSmoothnesses, NumOctaves, Seed, DefaultNoiseType, OctaveTypes, OctaveStrengths)
	{
		const int32 Num = ComputeVoxelBuffersNum(Positions, Amplitudes, FeatureScales, Lacunarities, Gains, VoronoiSmoothnesses, WaveletPhases, ScratchSmoothnesses);
		const int32 SafeNumOctaves = FMath::Clamp(NumOctaves, 1, 255);

		TVoxelInlineArray<ispc::FProceduralOctave2D, 16> Octaves;
		Octaves.Reserve(SafeNumOctaves);

		for (int32 Index = 0; Index < SafeNumOctaves; Index++)
		{
			ispc::FProceduralOctave2D& Octave = Octaves.Emplace_GetRef(ispc::FProceduralOctave2D{});

			if (OctaveTypes.IsValidIndex(Index) &&
				OctaveTypes[Index] != EVoxelProceduralNoiseType2D::Default)
			{
				Octave.Type = GetISPCNoise(OctaveTypes[Index]);
			}
			else
			{
				Octave.Type = GetISPCNoise(DefaultNoiseType);
			}

			if (OctaveStrengths.IsValidIndex(Index))
			{
				const FVoxelFloatBuffer& Strength = *OctaveStrengths[Index];

				if (Strength.IsConstant())
				{
					Octave.bStrengthIsConstant = true;
					Octave.StrengthConstant = Strength.GetConstant();
				}
				else
				{
					if (Strength.Num() != Num)
					{
						RaiseBufferError();
						return;
					}

					Octave.bStrengthIsConstant = false;
					Octave.StrengthArray = Strength.GetData();
				}
			}
			else
			{
				Octave.bStrengthIsConstant = true;
				Octave.StrengthConstant = 1.f;
				Octave.StrengthArray = nullptr;
			}
		}

		VOXEL_SCOPE_COUNTER_FORMAT("ProceduralNoise2D Num=%d", Num);
		FVoxelNodeStatScope StatScope(*this, Num);

		FVoxelFloatBuffer ReturnValue;
		ReturnValue.Allocate(Num);

		ispc::VoxelNode_ProceduralNoise2D(
			Positions->X.GetData(),
			Positions->X.IsConstant(),
			Positions->Y.GetData(),
			Positions->Y.IsConstant(),
			Amplitudes->GetData(),
			Amplitudes->IsConstant(),
			FeatureScales->GetData(),
			FeatureScales->IsConstant(),
			Lacunarities->GetData(),
			Lacunarities->IsConstant(),
			Gains->GetData(),
			Gains->IsConstant(),
			VoronoiSmoothnesses->GetData(),
			VoronoiSmoothnesses->IsConstant(),
			WaveletPhases->GetData(),
			WaveletPhases->IsConstant(),
			ScratchSmoothnesses->GetData(),
			ScratchSmoothnesses->IsConstant(),
			Octaves.GetData(),
			Octaves.Num(),
			Seed,
			ReturnValue.GetData(),
			Num);

		ValuePin.Set(Query, MoveTemp(ReturnValue));
	};
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelNode_ProceduralNoise3D::Compute(const FVoxelGraphQuery Query) const
{
	const TValue<FVoxelVectorBuffer> Positions = PositionPin.Get(Query);
	const TValue<FVoxelFloatBuffer> Amplitudes = AmplitudePin.Get(Query);
	const TValue<FVoxelFloatBuffer> FeatureScales = FeatureScalePin.Get(Query);
	const TValue<FVoxelFloatBuffer> Lacunarities = LacunarityPin.Get(Query);
	const TValue<FVoxelFloatBuffer> Gains = GainPin.Get(Query);
	const TValue<FVoxelFloatBuffer> VoronoiSmoothnesses = VoronoiSmoothnessPin.Get(Query);
	const TValue<FVoxelFloatBuffer> WaveletPhases = WaveletPhasePin.Get(Query);
	const TValue<FVoxelFloatBuffer> ScratchSmoothnesses = ScratchSmoothnessPin.Get(Query);
	const TValue<int32> NumOctaves = NumOctavesPin.Get(Query);
	const TValue<FVoxelSeed> Seed = SeedPin.Get(Query);
	const TValue<EVoxelProceduralNoiseType3D> DefaultNoiseType = DefaultNoiseTypePin.Get(Query);
	const TVoxelArray<TValue<EVoxelProceduralNoiseType3D>> OctaveTypes = OctaveTypePins.Get(Query);
	const TVoxelArray<TValue<FVoxelFloatBuffer>> OctaveStrengths = OctaveStrengthPins.Get(Query);

	VOXEL_GRAPH_WAIT(Positions, Amplitudes, FeatureScales, Lacunarities, Gains, VoronoiSmoothnesses, WaveletPhases, ScratchSmoothnesses, NumOctaves, Seed, DefaultNoiseType, OctaveTypes, OctaveStrengths)
	{
		const int32 Num = ComputeVoxelBuffersNum(Positions, Amplitudes, FeatureScales, Lacunarities, Gains, VoronoiSmoothnesses, WaveletPhases, ScratchSmoothnesses);
		const int32 SafeNumOctaves = FMath::Clamp(NumOctaves, 1, 255);

		TVoxelInlineArray<ispc::FProceduralOctave3D, 16> Octaves;
		Octaves.Reserve(SafeNumOctaves);

		for (int32 Index = 0; Index < SafeNumOctaves; Index++)
		{
			ispc::FProceduralOctave3D& Octave = Octaves.Emplace_GetRef(ispc::FProceduralOctave3D{});

			if (OctaveTypes.IsValidIndex(Index) &&
				OctaveTypes[Index] != EVoxelProceduralNoiseType3D::Default)
			{
				Octave.Type = GetISPCNoise(OctaveTypes[Index]);
			}
			else
			{
				Octave.Type = GetISPCNoise(DefaultNoiseType);
			}

			if (OctaveStrengths.IsValidIndex(Index))
			{
				const FVoxelFloatBuffer& Strength = *OctaveStrengths[Index];

				if (Strength.IsConstant())
				{
					Octave.bStrengthIsConstant = true;
					Octave.StrengthConstant = Strength.GetConstant();
				}
				else
				{
					if (Strength.Num() != Num)
					{
						RaiseBufferError();
						return;
					}

					Octave.bStrengthIsConstant = false;
					Octave.StrengthArray = Strength.GetData();
				}
			}
			else
			{
				Octave.bStrengthIsConstant = true;
				Octave.StrengthConstant = 1.f;
				Octave.StrengthArray = nullptr;
			}
		}

		VOXEL_SCOPE_COUNTER_FORMAT("ProceduralNoise3D Num=%d", Num);
		FVoxelNodeStatScope StatScope(*this, Num);

		FVoxelFloatBuffer ReturnValue;
		ReturnValue.Allocate(Num);

		ispc::VoxelNode_ProceduralNoise3D(
			Positions->X.GetData(),
			Positions->X.IsConstant(),
			Positions->Y.GetData(),
			Positions->Y.IsConstant(),
			Positions->Z.GetData(),
			Positions->Z.IsConstant(),
			Amplitudes->GetData(),
			Amplitudes->IsConstant(),
			FeatureScales->GetData(),
			FeatureScales->IsConstant(),
			Lacunarities->GetData(),
			Lacunarities->IsConstant(),
			Gains->GetData(),
			Gains->IsConstant(),
			VoronoiSmoothnesses->GetData(),
			VoronoiSmoothnesses->IsConstant(),
			WaveletPhases->GetData(),
			WaveletPhases->IsConstant(),
			ScratchSmoothnesses->GetData(),
			ScratchSmoothnesses->IsConstant(),
			Octaves.GetData(),
			Octaves.Num(),
			Seed,
			ReturnValue.GetData(),
			Num);

		ValuePin.Set(Query, MoveTemp(ReturnValue));
	};
}
