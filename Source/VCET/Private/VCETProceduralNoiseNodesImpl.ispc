// Copyright Zundle. MIT License.

// Noise functions below are ported from the Procedural Noise Collection, whose license is below
//
// MIT License
//
// Copyright (c) 2026 @lumiey
// https://fragcoord.xyz/s/pxmcvnpc
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//
// Changes from the original GLSL:
// - A seed is mixed into every hash so octaves can be decorrelated
// - Feature point offsets in Worley use a 2D/3D hash instead of a broadcasted scalar hash
// - Screen-space derivative based smoothing (fwidth) is replaced by explicit inputs
// - All noises are remapped to output roughly [-1, 1]
// - 3D variants are added for Voronoi, Blue, HilbertBlue, Crater, Gabor, Curl, Scratch,
//   Wavelet, Erosion, Paper, Stone and Wool

#define FORCEINLINE inline

///////////////////////////////////////////////////////////////////////////////
// Minimal vector library
///////////////////////////////////////////////////////////////////////////////

struct float2
{
	float x;
	float y;
};

struct float3
{
	float x;
	float y;
	float z;
};

FORCEINLINE float2 MakeFloat2(const float X, const float Y)
{
	float2 Result;
	Result.x = X;
	Result.y = Y;
	return Result;
}
FORCEINLINE float3 MakeFloat3(const float X, const float Y, const float Z)
{
	float3 Result;
	Result.x = X;
	Result.y = Y;
	Result.z = Z;
	return Result;
}

FORCEINLINE float2 operator+(const float2 A, const float2 B) { return MakeFloat2(A.x + B.x, A.y + B.y); }
FORCEINLINE float2 operator-(const float2 A, const float2 B) { return MakeFloat2(A.x - B.x, A.y - B.y); }
FORCEINLINE float2 operator*(const float2 A, const float2 B) { return MakeFloat2(A.x * B.x, A.y * B.y); }
FORCEINLINE float2 operator+(const float2 A, const float B) { return MakeFloat2(A.x + B, A.y + B); }
FORCEINLINE float2 operator-(const float2 A, const float B) { return MakeFloat2(A.x - B, A.y - B); }
FORCEINLINE float2 operator*(const float2 A, const float B) { return MakeFloat2(A.x * B, A.y * B); }
FORCEINLINE float2 operator*(const float A, const float2 B) { return MakeFloat2(A * B.x, A * B.y); }
FORCEINLINE float2 operator+(const float A, const float2 B) { return MakeFloat2(A + B.x, A + B.y); }
FORCEINLINE float2 operator-(const float A, const float2 B) { return MakeFloat2(A - B.x, A - B.y); }
FORCEINLINE float2 operator/(const float2 A, const float B) { return MakeFloat2(A.x / B, A.y / B); }

FORCEINLINE float3 operator+(const float3 A, const float3 B) { return MakeFloat3(A.x + B.x, A.y + B.y, A.z + B.z); }
FORCEINLINE float3 operator-(const float3 A, const float3 B) { return MakeFloat3(A.x - B.x, A.y - B.y, A.z - B.z); }
FORCEINLINE float3 operator*(const float3 A, const float3 B) { return MakeFloat3(A.x * B.x, A.y * B.y, A.z * B.z); }
FORCEINLINE float3 operator+(const float3 A, const float B) { return MakeFloat3(A.x + B, A.y + B, A.z + B); }
FORCEINLINE float3 operator-(const float3 A, const float B) { return MakeFloat3(A.x - B, A.y - B, A.z - B); }
FORCEINLINE float3 operator*(const float3 A, const float B) { return MakeFloat3(A.x * B, A.y * B, A.z * B); }
FORCEINLINE float3 operator*(const float A, const float3 B) { return MakeFloat3(A * B.x, A * B.y, A * B.z); }
FORCEINLINE float3 operator+(const float A, const float3 B) { return MakeFloat3(A + B.x, A + B.y, A + B.z); }
FORCEINLINE float3 operator-(const float A, const float3 B) { return MakeFloat3(A - B.x, A - B.y, A - B.z); }
FORCEINLINE float3 operator/(const float3 A, const float B) { return MakeFloat3(A.x / B, A.y / B, A.z / B); }

FORCEINLINE float dot(const float2 A, const float2 B)
{
	return A.x * B.x + A.y * B.y;
}
FORCEINLINE float dot(const float3 A, const float3 B)
{
	return A.x * B.x + A.y * B.y + A.z * B.z;
}
FORCEINLINE float length(const float2 A)
{
	return sqrt(dot(A, A));
}
FORCEINLINE float length(const float3 A)
{
	return sqrt(dot(A, A));
}
FORCEINLINE float2 normalize(const float2 A)
{
	return A / length(A);
}
FORCEINLINE float3 normalize(const float3 A)
{
	return A / length(A);
}
FORCEINLINE float3 cross(const float3 A, const float3 B)
{
	return MakeFloat3(
		A.y * B.z - A.z * B.y,
		A.z * B.x - A.x * B.z,
		A.x * B.y - A.y * B.x);
}
FORCEINLINE float2 floor(const float2 A)
{
	return MakeFloat2(floor(A.x), floor(A.y));
}
FORCEINLINE float3 floor(const float3 A)
{
	return MakeFloat3(floor(A.x), floor(A.y), floor(A.z));
}
FORCEINLINE float2 abs(const float2 A)
{
	return MakeFloat2(abs(A.x), abs(A.y));
}
FORCEINLINE float3 abs(const float3 A)
{
	return MakeFloat3(abs(A.x), abs(A.y), abs(A.z));
}

FORCEINLINE float FCLerp(const float A, const float B, const float Alpha)
{
	return A + Alpha * (B - A);
}
FORCEINLINE float FCMax3(const float A, const float B, const float C)
{
	return max(A, max(B, C));
}
FORCEINLINE bool FCIsFinite(const float Value)
{
	// False for NaN and infinity
	return abs(Value) <= 3.402823e38f;
}
FORCEINLINE float FCFract(const float Value)
{
	return Value - floor(Value);
}
FORCEINLINE float2 FCFract(const float2 Value)
{
	return Value - floor(Value);
}
FORCEINLINE float3 FCFract(const float3 Value)
{
	return Value - floor(Value);
}

// GLSL-style smoothstep, supports Edge0 > Edge1
FORCEINLINE float FCSmoothstep(const float Edge0, const float Edge1, const float Value)
{
	const float Alpha = clamp((Value - Edge0) / (Edge1 - Edge0), 0.f, 1.f);
	return Alpha * Alpha * (3.f - 2.f * Alpha);
}

FORCEINLINE float2 FCClamp01(const float2 Value)
{
	return MakeFloat2(clamp(Value.x, 0.f, 1.f), clamp(Value.y, 0.f, 1.f));
}
FORCEINLINE float3 FCClamp01(const float3 Value)
{
	return MakeFloat3(clamp(Value.x, 0.f, 1.f), clamp(Value.y, 0.f, 1.f), clamp(Value.z, 0.f, 1.f));
}

///////////////////////////////////////////////////////////////////////////////
// Buffer helpers
///////////////////////////////////////////////////////////////////////////////

FORCEINLINE float LoadFloatImpl(const uniform float Data[], const uniform bool bConstant, const varying int32 Index)
{
	return bConstant ? Data[0] : Data[Index];
}

#define Load_float(Name, Index) LoadFloatImpl(Array ## Name, bConst ## Name, Index)

#define Load_float2(Name, Index) MakeFloat2(Load_float(Name ## _X, Index), Load_float(Name ## _Y, Index))

#define Load_float3(Name, Index) MakeFloat3(Load_float(Name ## _X, Index), Load_float(Name ## _Y, Index), Load_float(Name ## _Z, Index))

#define Input_float(Name) \
	const uniform float Array ## Name[], \
	const uniform bool bConst ## Name

#define Input_float2(Name) \
	Input_float(Name ## _X), \
	Input_float(Name ## _Y)

#define Input_float3(Name) \
	Input_float(Name ## _X), \
	Input_float(Name ## _Y), \
	Input_float(Name ## _Z)

///////////////////////////////////////////////////////////////////////////////
// Seeded ports of the fi hashes
///////////////////////////////////////////////////////////////////////////////

FORCEINLINE uint32 FCHashBase(const uint32 Seed, const float2 Position)
{
	const uint32 X = intbits(Position.x * 141421356.f);
	const uint32 Y = intbits(Position.y * 2718281828.f);
	return X ^ Y ^ Seed;
}
FORCEINLINE uint32 FCHashBase(const uint32 Seed, const float3 Position)
{
	const uint32 X = intbits(Position.x * 141421356.f);
	const uint32 Y = intbits(Position.y * 2718281828.f);
	const uint32 Z = intbits(Position.z * 1618033988.f);
	return X ^ Y ^ Z ^ Seed;
}

// Signed conversion is much faster than uint32 to float in ISPC
FORCEINLINE float FCUintToFloat01(const uint32 Value)
{
	return (float)((int32)Value) * (1.f / 4294967296.f) + 0.5f;
}

FORCEINLINE float FCHash12(const uint32 Seed, const float2 Position)
{
	return FCUintToFloat01(FCHashBase(Seed, Position) * 3141592653u);
}
FORCEINLINE float2 FCHash22(const uint32 Seed, const float2 Position)
{
	const uint32 Hash = FCHashBase(Seed, Position);
	return MakeFloat2(
		FCUintToFloat01(Hash * 3141592653u),
		FCUintToFloat01(Hash * 1618033988u));
}
FORCEINLINE float3 FCHash32(const uint32 Seed, const float2 Position)
{
	const uint32 Hash = FCHashBase(Seed, Position);
	return MakeFloat3(
		FCUintToFloat01(Hash * 1732050807u),
		FCUintToFloat01(Hash * 2645751311u),
		FCUintToFloat01(Hash * 3316624790u));
}
FORCEINLINE float FCHash13(const uint32 Seed, const float3 Position)
{
	return FCUintToFloat01(FCHashBase(Seed, Position) * 3141592653u);
}
FORCEINLINE float3 FCHash33(const uint32 Seed, const float3 Position)
{
	const uint32 Hash = FCHashBase(Seed, Position);
	return MakeFloat3(
		FCUintToFloat01(Hash * 1732050807u),
		FCUintToFloat01(Hash * 2645751311u),
		FCUintToFloat01(Hash * 3316624790u));
}

// Pseudo random offset used to seed noises whose formulation has no hash to mix a seed into
FORCEINLINE float2 FCSeedOffset2(const uint32 Seed)
{
	const uint32 Hash = (Seed ^ 0x9E3779B9u) * 3141592653u;
	return MakeFloat2(
		(float)((int32)(Hash & 0xffffu)) * (1024.f / 65535.f),
		(float)((int32)((Hash >> 16) & 0xffffu)) * (1024.f / 65535.f));
}
FORCEINLINE float3 FCSeedOffset3(const uint32 Seed)
{
	const uint32 Hash = (Seed ^ 0x9E3779B9u) * 3141592653u;
	const uint32 TenBits = (1u << 10) - 1;
	return MakeFloat3(
		(float)((int32)((Hash >> 0) & TenBits)),
		(float)((int32)((Hash >> 10) & TenBits)),
		(float)((int32)((Hash >> 20) & TenBits)));
}

///////////////////////////////////////////////////////////////////////////////
// 2D noises
///////////////////////////////////////////////////////////////////////////////

FORCEINLINE float FCValue2D(const uint32 Seed, const float2 Position)
{
	const float2 Cell = floor(Position);
	float2 Alpha = Position - Cell;
	Alpha = Alpha * Alpha * (3.f - 2.f * Alpha);

	const float Result = FCLerp(
		FCLerp(FCHash12(Seed, Cell), FCHash12(Seed, Cell + MakeFloat2(1.f, 0.f)), Alpha.x),
		FCLerp(FCHash12(Seed, Cell + MakeFloat2(0.f, 1.f)), FCHash12(Seed, Cell + MakeFloat2(1.f, 1.f)), Alpha.x),
		Alpha.y);

	return Result * 2.f - 1.f;
}

// Returns the raw gradient noise value, roughly [-0.7, 0.7]
FORCEINLINE float FCPerlin2D_Raw(const uint32 Seed, const float2 Position)
{
	const float2 Cell = floor(Position);
	const float2 Local = Position - Cell;
	const float2 Alpha = Local * Local * Local * (10.f + Local * (6.f * Local - 15.f));

	const float NoiseA = dot(normalize(FCHash22(Seed, Cell + MakeFloat2(0.f, 0.f)) - 0.5f), Local - MakeFloat2(0.f, 0.f));
	const float NoiseB = dot(normalize(FCHash22(Seed, Cell + MakeFloat2(1.f, 0.f)) - 0.5f), Local - MakeFloat2(1.f, 0.f));
	const float NoiseC = dot(normalize(FCHash22(Seed, Cell + MakeFloat2(0.f, 1.f)) - 0.5f), Local - MakeFloat2(0.f, 1.f));
	const float NoiseD = dot(normalize(FCHash22(Seed, Cell + MakeFloat2(1.f, 1.f)) - 0.5f), Local - MakeFloat2(1.f, 1.f));

	return FCLerp(FCLerp(NoiseA, NoiseB, Alpha.x), FCLerp(NoiseC, NoiseD, Alpha.x), Alpha.y);
}
FORCEINLINE float FCPerlin2D(const uint32 Seed, const float2 Position)
{
	return FCPerlin2D_Raw(Seed, Position) * 1.4f;
}

// Gradient noise with analytic derivative, from https://iquilezles.org/articles/gradientnoise/
FORCEINLINE float FCPerlinDeriv2D(const uint32 Seed, const float2 Position, varying float2* OutGradient)
{
	const float2 Cell = floor(Position);
	const float2 Local = Position - Cell;

	const float2 Alpha = Local * Local * Local * (Local * (Local * 6.f - 15.f) + 10.f);
	const float2 DeltaAlpha = 30.f * Local * Local * (Local * (Local - 2.f) + 1.f);

	const float2 GradientA = FCHash22(Seed, Cell + MakeFloat2(0.f, 0.f)) * 2.f - 1.f;
	const float2 GradientB = FCHash22(Seed, Cell + MakeFloat2(1.f, 0.f)) * 2.f - 1.f;
	const float2 GradientC = FCHash22(Seed, Cell + MakeFloat2(0.f, 1.f)) * 2.f - 1.f;
	const float2 GradientD = FCHash22(Seed, Cell + MakeFloat2(1.f, 1.f)) * 2.f - 1.f;

	const float ValueA = dot(GradientA, Local - MakeFloat2(0.f, 0.f));
	const float ValueB = dot(GradientB, Local - MakeFloat2(1.f, 0.f));
	const float ValueC = dot(GradientC, Local - MakeFloat2(0.f, 1.f));
	const float ValueD = dot(GradientD, Local - MakeFloat2(1.f, 1.f));

	*OutGradient =
		GradientA +
		Alpha.x * (GradientB - GradientA) +
		Alpha.y * (GradientC - GradientA) +
		Alpha.x * Alpha.y * (GradientA - GradientB - GradientC + GradientD) +
		DeltaAlpha * (MakeFloat2(Alpha.y, Alpha.x) * (ValueA - ValueB - ValueC + ValueD) + MakeFloat2(ValueB, ValueC) - ValueA);

	return
		ValueA +
		Alpha.x * (ValueB - ValueA) +
		Alpha.y * (ValueC - ValueA) +
		Alpha.x * Alpha.y * (ValueA - ValueB - ValueC + ValueD);
}

FORCEINLINE float FCSimplex2D(const uint32 Seed, const float2 Position)
{
	const float2 Cell = floor(Position + (Position.x + Position.y) * 0.366025f);
	const float2 PositionA = Position - Cell + (Cell.x + Cell.y) * 0.211324f;

	const float Mask = PositionA.x >= PositionA.y ? 1.f : 0.f;
	const float2 PositionB = PositionA - MakeFloat2(Mask, 1.f - Mask) + 0.211324f;
	const float2 PositionC = PositionA - 0.577351f;

	float AlphaA = max(0.5f - dot(PositionA, PositionA), 0.f);
	float AlphaB = max(0.5f - dot(PositionB, PositionB), 0.f);
	float AlphaC = max(0.5f - dot(PositionC, PositionC), 0.f);

	AlphaA = AlphaA * AlphaA * AlphaA * AlphaA;
	AlphaB = AlphaB * AlphaB * AlphaB * AlphaB;
	AlphaC = AlphaC * AlphaC * AlphaC * AlphaC;

	const float Result =
		AlphaA * dot(PositionA, FCHash22(Seed, Cell) - 0.5f) +
		AlphaB * dot(PositionB, FCHash22(Seed, Cell + MakeFloat2(Mask, 1.f - Mask)) - 0.5f) +
		AlphaC * dot(PositionC, FCHash22(Seed, Cell + 1.f) - 0.5f);

	return Result * 140.f;
}

FORCEINLINE float FCWorley2D(const uint32 Seed, const float2 Position)
{
	const float2 Cell = floor(Position);
	const float2 Local = Position - Cell;

	float Distance = 1e6f;
	for (uniform int32 IndexX = -1; IndexX <= 1; IndexX++)
	{
		for (uniform int32 IndexY = -1; IndexY <= 1; IndexY++)
		{
			const float2 Offset = MakeFloat2(IndexX, IndexY);
			const float2 Delta = Local - Offset - FCHash22(Seed, Cell + Offset);
			Distance = min(Distance, dot(Delta, Delta));
		}
	}

	return (1.f - sqrt(Distance)) * 2.f - 1.f;
}

FORCEINLINE float FCVoronoi2D(const uint32 Seed, const float2 Position, const float Smoothness)
{
	const float Sharpness = 1.f / max(Smoothness, 0.001f);

	const float2 Cell = floor(Position);
	const float2 Local = Position - Cell;

	float ValueSum = 0.f;
	float WeightSum = 0.f;
	for (uniform int32 IndexX = -1; IndexX <= 1; IndexX++)
	{
		for (uniform int32 IndexY = -1; IndexY <= 1; IndexY++)
		{
			const float2 Offset = MakeFloat2(IndexX, IndexY);
			const float3 Random = FCHash32(Seed, Cell + Offset);
			const float Distance = length(Offset - Local + MakeFloat2(Random.x, Random.y));
			const float Weight = pow(FCSmoothstep(1.414f, 0.f, Distance), Sharpness);
			ValueSum += Random.z * Weight;
			WeightSum += Weight;
		}
	}

	return ValueSum / max(WeightSum, 1e-6f) * 2.f - 1.f;
}

// High-pass filtered white noise, from https://www.shadertoy.com/view/tllcR2
FORCEINLINE float FCBlue2D(const uint32 Seed, const float2 Position)
{
	const float2 Cell = floor(Position);

	float Sum = 0.f;
	for (uniform int32 IndexX = -1; IndexX <= 1; IndexX++)
	{
		for (uniform int32 IndexY = -1; IndexY <= 1; IndexY++)
		{
			Sum += FCHash12(Seed, Cell + MakeFloat2(IndexX, IndexY));
		}
	}

	return (0.9f * (1.125f * FCHash12(Seed, Cell) - Sum / 8.f) + 0.5f) * 2.f - 1.f;
}

// Hilbert curve based low-discrepancy noise, modified from https://www.shadertoy.com/view/3tB3z3
FORCEINLINE float FCHilbertBlue2D(const uint32 Seed, const float2 Position)
{
	const uint32 SeedHash = (Seed ^ 0x9E3779B9u) * 3141592653u;

	int32 X = ((int32)floor(Position.x) + (int32)(SeedHash & 511u)) & 511;
	int32 Y = ((int32)floor(Position.y) + (int32)((SeedHash >> 9) & 511u)) & 511;

	int32 Result = 0;
	for (uniform int32 Mask = 512 >> 1; Mask > 0; Mask >>= 1)
	{
		const int32 RegionX = (X & Mask) != 0 ? 1 : 0;
		const int32 RegionY = (Y & Mask) != 0 ? 1 : 0;
		Result += Mask * Mask * ((RegionX << 1) | (RegionX ^ RegionY));

		const int32 Flip = ((X ^ Y) * (1 - RegionY)) ^ ((Mask - 1) * (RegionX & (1 - RegionY)));
		X ^= Flip;
		Y ^= Flip;
	}
	Result = Result & 262143;

	// Compute in double: fract of golden ratio times a large index needs more than float precision
	const double GoldenIndex = 0.6180339887498948482d * (double)Result;
	return (float)(GoldenIndex - floor(GoldenIndex)) * 2.f - 1.f;
}

// Impact crater rings, modified from https://www.shadertoy.com/view/XsGBDt
FORCEINLINE float FCCrater2D(const uint32 Seed, const float2 Position)
{
	const float2 Cell = floor(Position);
	const float2 Local = Position - Cell;

	float ValueSum = 0.f;
	float WeightSum = 0.f;
	for (uniform int32 IndexX = -2; IndexX <= 2; IndexX++)
	{
		for (uniform int32 IndexY = -2; IndexY <= 2; IndexY++)
		{
			const float2 Offset = MakeFloat2(IndexX, IndexY);
			const float2 Random = FCHash22(Seed, Cell + Offset);
			const float Distance = length(Local - Offset - Random);
			const float Weight = exp(-4.f * Distance);
			ValueSum += Weight * sin(6.28318530718f * sqrt(max(Distance, 0.06f)));
			WeightSum += Weight;
		}
	}

	return abs(ValueSum / max(WeightSum, 1e-6f)) * 2.f - 1.f;
}

FORCEINLINE float FCGabor2D(const uint32 Seed, const float2 Position)
{
	const uniform float Frequency = 8.f;

	const float2 Cell = floor(Position);
	float2 Alpha = Position - Cell;
	Alpha = Alpha * Alpha * (3.f - 2.f * Alpha);

	return FCLerp(
		FCLerp(
			sin(Frequency * dot(Position, FCHash22(Seed, Cell + MakeFloat2(0.f, 0.f)))),
			sin(Frequency * dot(Position, FCHash22(Seed, Cell + MakeFloat2(1.f, 0.f)))),
			Alpha.x),
		FCLerp(
			sin(Frequency * dot(Position, FCHash22(Seed, Cell + MakeFloat2(0.f, 1.f)))),
			sin(Frequency * dot(Position, FCHash22(Seed, Cell + MakeFloat2(1.f, 1.f)))),
			Alpha.x),
		Alpha.y);
}

// Magnitude of the finite-difference curl of gradient noise
FORCEINLINE float FCCurl2D(const uint32 Seed, const float2 Position)
{
	const uniform float Epsilon = 0.1f;

	const float2 CurlValue = MakeFloat2(
		FCPerlin2D_Raw(Seed, Position + MakeFloat2(Epsilon, 0.f)) - FCPerlin2D_Raw(Seed, Position - MakeFloat2(Epsilon, 0.f)),
		FCPerlin2D_Raw(Seed, Position + MakeFloat2(0.f, Epsilon)) - FCPerlin2D_Raw(Seed, Position - MakeFloat2(0.f, Epsilon))) / Epsilon * 0.5f * 0.7f;

	return length(CurlValue) / 1.414f * 2.f - 1.f;
}

// Single layer of thin wavy lines, inspired from https://www.shadertoy.com/view/4syXRD
FORCEINLINE float FCScratchLayer2D(const uint32 Seed, const float2 Position, const float Smoothness)
{
	const uniform float Thickness = 0.02f;
	const uniform float Wavyness = 0.5f;

	const float2 Cell = floor(Position);
	const float2 Random = FCHash22(Seed, Cell) * MakeFloat2(3104.f, 554.f);

	float2 Local = (Position - Cell) * 2.f - 1.f;

	const float SinAngle = sin(Random.x + Random.y);
	const float CosAngle = cos(Random.x + Random.y);
	Local = Local * CosAngle + MakeFloat2(-Local.y, Local.x) * SinAngle;
	Local = Local + sin(Random.x - Random.y);

	float Line = abs(Local.x - cos(Random.x + Local.y * 1.57f) * Wavyness);
	Line = FCSmoothstep(Thickness + Smoothness, Thickness - Smoothness, Line);
	Line *= Local.y * 0.5f + 0.5f;

	return Line;
}
FORCEINLINE float FCScratch2D(const uint32 Seed, const float2 Position, const float Smoothness)
{
	float2 Local = Position;
	float Width = max(Smoothness, 0.001f);

	float Scratches = 0.f;
	for (uniform int32 Index = 0; Index < 8; Index++)
	{
		Scratches = max(Scratches, FCScratchLayer2D(Seed, Local, Width));
		Local = MakeFloat2(
			Local.x + 0.7f * Local.y,
			-0.7f * Local.x + Local.y) - 12.31f;
		Width *= 1.22f;
	}

	return Scratches * 2.f - 1.f;
}

// Rotated sine wavelets, from https://www.shadertoy.com/view/wsBfzK
FORCEINLINE float FCWavelet2D(const uint32 Seed, const float2 Position, const float Phase)
{
	const uniform float Scale = 1.24f;

	float2 Local = Position + FCSeedOffset2(Seed);

	float Value = 0.f;
	float Frequency = 1.f;
	float WeightSum = 0.f;
	for (uniform int32 Index = 0; Index < 4; Index++)
	{
		const float2 Scaled = Local * Frequency;

		float2 Random = FCFract(floor(Scaled) * MakeFloat2(123.34f, 233.53f));
		Random = Random + dot(Random, Random + 23.234f);
		const float Angle = FCFract(Random.x * Random.y) * 1e3f;

		const float2 Centered = FCFract(Scaled) - 0.5f;
		const float SinAngle = sin(Angle);
		const float CosAngle = cos(Angle);
		const float2 Rotated = MakeFloat2(
			Centered.x * CosAngle + Centered.y * SinAngle,
			-Centered.x * SinAngle + Centered.y * CosAngle);

		Value += sin(Rotated.x * 10.f + Phase) * FCSmoothstep(0.25f, 0.f, dot(Rotated, Rotated)) / Frequency;

		Local = MakeFloat2(
			0.54f * Local.x - 0.84f * Local.y,
			0.84f * Local.x + 0.54f * Local.y) + (uniform float)Index;

		WeightSum += 1.f / Frequency;
		Frequency *= Scale;
	}

	return Value / WeightSum;
}

FORCEINLINE float3 FCGullies2D(const uint32 Seed, const float2 Position, const float2 Slope)
{
	const float2 SideDirection = MakeFloat2(-Slope.y, Slope.x) * 3.14159265f;

	const float2 Cell = floor(Position);
	const float2 Local = Position - Cell;

	float2 HeightSlope = MakeFloat2(0.f, 0.f);
	float WeightSum = 0.f;
	for (uniform int32 IndexX = -1; IndexX <= 2; IndexX++)
	{
		for (uniform int32 IndexY = -1; IndexY <= 2; IndexY++)
		{
			const float2 Offset = MakeFloat2(IndexX, IndexY);
			const float2 Delta = Local - Offset - FCHash22(Seed, Cell + Offset) + 0.5f;
			const float DistanceSquared = dot(Delta, Delta);
			const float Weight = max(0.f, exp(-DistanceSquared * 2.f) - 0.01111f);
			WeightSum += Weight;

			const float Along = dot(Delta, SideDirection);
			HeightSlope = HeightSlope + MakeFloat2(cos(Along), -sin(Along)) * Weight;
		}
	}

	WeightSum = max(WeightSum, 1e-6f);
	return MakeFloat3(
		HeightSlope.x,
		HeightSlope.y * SideDirection.x,
		HeightSlope.y * SideDirection.y) / WeightSum;
}
// Gradient noise with slope-following gullies, modified from https://www.shadertoy.com/view/sf23W1
FORCEINLINE float FCErosion2D(const uint32 Seed, const float2 Position)
{
	float2 Gradient;
	float Value = FCPerlinDeriv2D(Seed, Position, &Gradient);

	float Strength = 0.25f;
	float Frequency = 8.f;
	float Total = 1.f;
	for (uniform int32 Index = 0; Index < 4; Index++)
	{
		const float SlopeLengthSquared = max(dot(Gradient, Gradient), 1e-12f);
		const float3 Gully = FCGullies2D(Seed, Position * Frequency, Gradient * pow(SlopeLengthSquared, -0.25f));

		Value += Gully.x * Strength;
		Gradient = Gradient + MakeFloat2(Gully.y, Gully.z) * Strength * Frequency;

		Total += Strength;
		Strength *= 0.5f;
		Frequency *= 2.f;
	}

	return Value / Total;
}

FORCEINLINE float FCPaper2D(const uint32 Seed, const float2 Position)
{
	float2 Local = Position;

	float2 Sum = MakeFloat2(0.f, 0.f);
	float WeightSum = 0.f;
	float Weight = 1.f;
	for (uniform int32 Index = 0; Index < 10; Index++)
	{
		float2 Gradient;
		FCPerlinDeriv2D(Seed, Local, &Gradient);

		Sum = Sum + FCClamp01(Gradient * 0.5f + 0.5f) * Weight;
		WeightSum += Weight;
		Weight *= 0.8f;
		Local = Local * 2.f;
	}
	Sum = Sum / WeightSum;

	return (length(Sum) / 1.414f * 0.6f + 0.4f) * 2.f - 1.f;
}

FORCEINLINE float FCStone2D(const uint32 Seed, const float2 Position)
{
	float2 WarpGradient = MakeFloat2(0.f, 0.f);
	{
		float2 Local = Position;
		float Weight = 1.f;
		for (uniform int32 Index = 0; Index < 6; Index++)
		{
			float2 Gradient;
			FCPerlinDeriv2D(Seed, Local, &Gradient);

			WarpGradient = WarpGradient + Gradient * Weight;
			Weight *= 0.5f;
			Local = Local * 2.f;
		}
	}

	float2 Local = Position + WarpGradient * 0.4f;

	float Sum = 0.f;
	float WeightSum = 0.f;
	float Weight = 1.f;
	for (uniform int32 Index = 0; Index < 6; Index++)
	{
		Sum += (FCPerlin2D_Raw(Seed, Local) * 0.7f + 0.5f) * Weight;
		WeightSum += Weight;
		Weight *= 0.5f;
		Local = Local * 2.f;
	}

	return Sum / WeightSum * 2.f - 1.f;
}

FORCEINLINE float FCWool2D(const uint32 Seed, const float2 Position)
{
	float2 Local = Position;

	float2 Sum = MakeFloat2(0.f, 0.f);
	float WeightSum = 0.f;
	float Weight = 1.f;
	for (uniform int32 Index = 0; Index < 6; Index++)
	{
		float2 Gradient;
		FCPerlinDeriv2D(Seed, Local, &Gradient);

		Sum = Sum + Gradient * Weight;
		WeightSum += Weight;
		Weight *= 0.5f;
		Local = Local * 2.f;
	}
	Sum = Sum / WeightSum;

	return max(abs(Sum.x), abs(Sum.y)) * 2.f - 1.f;
}

// Interleaved gradient noise
FORCEINLINE float FCInterleavedGradient2D(const uint32 Seed, const float2 Position)
{
	const float2 Local = Position + FCSeedOffset2(Seed);
	return FCFract(52.9829189f * FCFract(dot(Local, MakeFloat2(0.06711056f, 0.00583715f)))) * 2.f - 1.f;
}

///////////////////////////////////////////////////////////////////////////////
// 3D noises
///////////////////////////////////////////////////////////////////////////////

FORCEINLINE float FCValue3D(const uint32 Seed, const float3 Position)
{
	const float3 Cell = floor(Position);
	float3 Alpha = Position - Cell;
	Alpha = Alpha * Alpha * (3.f - 2.f * Alpha);

	const float Result = FCLerp(
		FCLerp(
			FCLerp(FCHash13(Seed, Cell + MakeFloat3(0.f, 0.f, 0.f)), FCHash13(Seed, Cell + MakeFloat3(1.f, 0.f, 0.f)), Alpha.x),
			FCLerp(FCHash13(Seed, Cell + MakeFloat3(0.f, 1.f, 0.f)), FCHash13(Seed, Cell + MakeFloat3(1.f, 1.f, 0.f)), Alpha.x),
			Alpha.y),
		FCLerp(
			FCLerp(FCHash13(Seed, Cell + MakeFloat3(0.f, 0.f, 1.f)), FCHash13(Seed, Cell + MakeFloat3(1.f, 0.f, 1.f)), Alpha.x),
			FCLerp(FCHash13(Seed, Cell + MakeFloat3(0.f, 1.f, 1.f)), FCHash13(Seed, Cell + MakeFloat3(1.f, 1.f, 1.f)), Alpha.x),
			Alpha.y),
		Alpha.z);

	return Result * 2.f - 1.f;
}

// Returns the raw gradient noise value, roughly [-0.7, 0.7]
FORCEINLINE float FCPerlin3D_Raw(const uint32 Seed, const float3 Position)
{
	const float3 Cell = floor(Position);
	const float3 Local = Position - Cell;
	const float3 Alpha = Local * Local * Local * (10.f + Local * (6.f * Local - 15.f));

#define FC_CORNER(X, Y, Z) dot(normalize(FCHash33(Seed, Cell + MakeFloat3(X, Y, Z)) - 0.5f), Local - MakeFloat3(X, Y, Z))
	const float NoiseA = FC_CORNER(0.f, 0.f, 0.f);
	const float NoiseB = FC_CORNER(1.f, 0.f, 0.f);
	const float NoiseC = FC_CORNER(0.f, 1.f, 0.f);
	const float NoiseD = FC_CORNER(1.f, 1.f, 0.f);
	const float NoiseE = FC_CORNER(0.f, 0.f, 1.f);
	const float NoiseF = FC_CORNER(1.f, 0.f, 1.f);
	const float NoiseG = FC_CORNER(0.f, 1.f, 1.f);
	const float NoiseH = FC_CORNER(1.f, 1.f, 1.f);
#undef FC_CORNER

	const float LayerA = FCLerp(FCLerp(NoiseA, NoiseB, Alpha.x), FCLerp(NoiseC, NoiseD, Alpha.x), Alpha.y);
	const float LayerB = FCLerp(FCLerp(NoiseE, NoiseF, Alpha.x), FCLerp(NoiseG, NoiseH, Alpha.x), Alpha.y);

	return FCLerp(LayerA, LayerB, Alpha.z);
}
FORCEINLINE float FCPerlin3D(const uint32 Seed, const float3 Position)
{
	return FCPerlin3D_Raw(Seed, Position) * 1.4f;
}

// Gradient noise with analytic derivative, from https://iquilezles.org/articles/gradientnoise/
FORCEINLINE float FCPerlinDeriv3D(const uint32 Seed, const float3 Position, varying float3* OutGradient)
{
	const float3 Cell = floor(Position);
	const float3 Local = Position - Cell;

	const float3 Alpha = Local * Local * Local * (Local * (Local * 6.f - 15.f) + 10.f);
	const float3 DeltaAlpha = 30.f * Local * Local * (Local * (Local - 2.f) + 1.f);

	const float3 GradientA = FCHash33(Seed, Cell + MakeFloat3(0.f, 0.f, 0.f)) * 2.f - 1.f;
	const float3 GradientB = FCHash33(Seed, Cell + MakeFloat3(1.f, 0.f, 0.f)) * 2.f - 1.f;
	const float3 GradientC = FCHash33(Seed, Cell + MakeFloat3(0.f, 1.f, 0.f)) * 2.f - 1.f;
	const float3 GradientD = FCHash33(Seed, Cell + MakeFloat3(1.f, 1.f, 0.f)) * 2.f - 1.f;
	const float3 GradientE = FCHash33(Seed, Cell + MakeFloat3(0.f, 0.f, 1.f)) * 2.f - 1.f;
	const float3 GradientF = FCHash33(Seed, Cell + MakeFloat3(1.f, 0.f, 1.f)) * 2.f - 1.f;
	const float3 GradientG = FCHash33(Seed, Cell + MakeFloat3(0.f, 1.f, 1.f)) * 2.f - 1.f;
	const float3 GradientH = FCHash33(Seed, Cell + MakeFloat3(1.f, 1.f, 1.f)) * 2.f - 1.f;

	const float ValueA = dot(GradientA, Local - MakeFloat3(0.f, 0.f, 0.f));
	const float ValueB = dot(GradientB, Local - MakeFloat3(1.f, 0.f, 0.f));
	const float ValueC = dot(GradientC, Local - MakeFloat3(0.f, 1.f, 0.f));
	const float ValueD = dot(GradientD, Local - MakeFloat3(1.f, 1.f, 0.f));
	const float ValueE = dot(GradientE, Local - MakeFloat3(0.f, 0.f, 1.f));
	const float ValueF = dot(GradientF, Local - MakeFloat3(1.f, 0.f, 1.f));
	const float ValueG = dot(GradientG, Local - MakeFloat3(0.f, 1.f, 1.f));
	const float ValueH = dot(GradientH, Local - MakeFloat3(1.f, 1.f, 1.f));

	const float CornerSum = -ValueA + ValueB + ValueC - ValueD + ValueE - ValueF - ValueG + ValueH;
	const float3 GradientCornerSum =
		-1.f * GradientA + GradientB + GradientC - GradientD + GradientE - GradientF - GradientG + GradientH;

	const float3 AlphaYZX = MakeFloat3(Alpha.y, Alpha.z, Alpha.x);
	const float3 AlphaZXY = MakeFloat3(Alpha.z, Alpha.x, Alpha.y);

	*OutGradient =
		GradientA +
		Alpha.x * (GradientB - GradientA) +
		Alpha.y * (GradientC - GradientA) +
		Alpha.z * (GradientE - GradientA) +
		Alpha.x * Alpha.y * (GradientA - GradientB - GradientC + GradientD) +
		Alpha.y * Alpha.z * (GradientA - GradientC - GradientE + GradientG) +
		Alpha.z * Alpha.x * (GradientA - GradientB - GradientE + GradientF) +
		Alpha.x * Alpha.y * Alpha.z * GradientCornerSum +
		DeltaAlpha * (
			MakeFloat3(ValueB, ValueC, ValueE) - ValueA +
			AlphaYZX * MakeFloat3(ValueA - ValueB - ValueC + ValueD, ValueA - ValueC - ValueE + ValueG, ValueA - ValueB - ValueE + ValueF) +
			AlphaZXY * MakeFloat3(ValueA - ValueB - ValueE + ValueF, ValueA - ValueB - ValueC + ValueD, ValueA - ValueC - ValueE + ValueG) +
			AlphaYZX * AlphaZXY * CornerSum);

	return
		ValueA +
		Alpha.x * (ValueB - ValueA) +
		Alpha.y * (ValueC - ValueA) +
		Alpha.z * (ValueE - ValueA) +
		Alpha.x * Alpha.y * (ValueA - ValueB - ValueC + ValueD) +
		Alpha.y * Alpha.z * (ValueA - ValueC - ValueE + ValueG) +
		Alpha.z * Alpha.x * (ValueA - ValueB - ValueE + ValueF) +
		Alpha.x * Alpha.y * Alpha.z * CornerSum;
}

FORCEINLINE float FCSimplex3D(const uint32 Seed, const float3 Position)
{
	const float3 Skewed = floor(Position + (Position.x + Position.y + Position.z) * (1.f / 3.f));
	const float3 PositionA = Position - Skewed + (Skewed.x + Skewed.y + Skewed.z) * (1.f / 6.f);

	const float EdgeX = PositionA.x >= PositionA.y ? 1.f : 0.f;
	const float EdgeY = PositionA.y >= PositionA.z ? 1.f : 0.f;
	const float EdgeZ = PositionA.z >= PositionA.x ? 1.f : 0.f;

	// e = step(0, x - x.yzx); i1 = e * (1 - e.zxy); i2 = 1 - e.zxy * (1 - e)
	const float3 Edge = MakeFloat3(EdgeX, EdgeY, EdgeZ);
	const float3 Corner1 = Edge * MakeFloat3(1.f - EdgeZ, 1.f - EdgeX, 1.f - EdgeY);
	const float3 Corner2 = MakeFloat3(1.f, 1.f, 1.f) - (MakeFloat3(EdgeZ, EdgeX, EdgeY) * (MakeFloat3(1.f, 1.f, 1.f) - Edge));

	const float3 PositionB = PositionA - Corner1 + (1.f / 6.f);
	const float3 PositionC = PositionA - Corner2 + (1.f / 3.f);
	const float3 PositionD = PositionA - 0.5f;

	float AlphaA = max(0.6f - dot(PositionA, PositionA), 0.f);
	float AlphaB = max(0.6f - dot(PositionB, PositionB), 0.f);
	float AlphaC = max(0.6f - dot(PositionC, PositionC), 0.f);
	float AlphaD = max(0.6f - dot(PositionD, PositionD), 0.f);

	AlphaA = AlphaA * AlphaA * AlphaA * AlphaA;
	AlphaB = AlphaB * AlphaB * AlphaB * AlphaB;
	AlphaC = AlphaC * AlphaC * AlphaC * AlphaC;
	AlphaD = AlphaD * AlphaD * AlphaD * AlphaD;

	const float Result =
		AlphaA * dot(FCHash33(Seed, Skewed) - 0.5f, PositionA) +
		AlphaB * dot(FCHash33(Seed, Skewed + Corner1) - 0.5f, PositionB) +
		AlphaC * dot(FCHash33(Seed, Skewed + Corner2) - 0.5f, PositionC) +
		AlphaD * dot(FCHash33(Seed, Skewed + 1.f) - 0.5f, PositionD);

	return Result * 52.f;
}

FORCEINLINE float FCWorley3D(const uint32 Seed, const float3 Position)
{
	const float3 Cell = floor(Position);
	const float3 Local = Position - Cell;

	float Distance = 1e6f;
	for (uniform int32 IndexX = -1; IndexX <= 1; IndexX++)
	{
		for (uniform int32 IndexY = -1; IndexY <= 1; IndexY++)
		{
			for (uniform int32 IndexZ = -1; IndexZ <= 1; IndexZ++)
			{
				const float3 Offset = MakeFloat3(IndexX, IndexY, IndexZ);
				const float3 Delta = Local - Offset - FCHash33(Seed, Cell + Offset);
				Distance = min(Distance, dot(Delta, Delta));
			}
		}
	}

	return (1.f - sqrt(Distance)) * 2.f - 1.f;
}

FORCEINLINE float FCVoronoi3D(const uint32 Seed, const float3 Position, const float Smoothness)
{
	const float Sharpness = 1.f / max(Smoothness, 0.001f);

	const float3 Cell = floor(Position);
	const float3 Local = Position - Cell;

	float ValueSum = 0.f;
	float WeightSum = 0.f;
	for (uniform int32 IndexX = -1; IndexX <= 1; IndexX++)
	{
		for (uniform int32 IndexY = -1; IndexY <= 1; IndexY++)
		{
			for (uniform int32 IndexZ = -1; IndexZ <= 1; IndexZ++)
			{
				const float3 Offset = MakeFloat3(IndexX, IndexY, IndexZ);
				const float3 Random = FCHash33(Seed, Cell + Offset);
				const float Value = FCHash13(Seed ^ 0x9E3779B9u, Cell + Offset);
				const float Distance = length(Offset - Local + Random);
				const float Weight = pow(FCSmoothstep(1.732f, 0.f, Distance), Sharpness);
				ValueSum += Value * Weight;
				WeightSum += Weight;
			}
		}
	}

	return ValueSum / max(WeightSum, 1e-6f) * 2.f - 1.f;
}

// High-pass filtered white noise, 3D extension of https://www.shadertoy.com/view/tllcR2
FORCEINLINE float FCBlue3D(const uint32 Seed, const float3 Position)
{
	const float3 Cell = floor(Position);

	float Sum = 0.f;
	for (uniform int32 IndexX = -1; IndexX <= 1; IndexX++)
	{
		for (uniform int32 IndexY = -1; IndexY <= 1; IndexY++)
		{
			for (uniform int32 IndexZ = -1; IndexZ <= 1; IndexZ++)
			{
				Sum += FCHash13(Seed, Cell + MakeFloat3(IndexX, IndexY, IndexZ));
			}
		}
	}

	return (0.9f * (1.125f * FCHash13(Seed, Cell) - Sum / 26.f) + 0.5f) * 2.f - 1.f;
}

// 3D Hilbert curve index using Skilling's transpose algorithm, 6 bits per axis (64x64x64 grid)
FORCEINLINE float FCHilbertBlue3D(const uint32 Seed, const float3 Position)
{
	const uint32 SeedHash = (Seed ^ 0x9E3779B9u) * 3141592653u;

	int32 X = ((int32)floor(Position.x) + (int32)(SeedHash & 63u)) & 63;
	int32 Y = ((int32)floor(Position.y) + (int32)((SeedHash >> 6) & 63u)) & 63;
	int32 Z = ((int32)floor(Position.z) + (int32)((SeedHash >> 12) & 63u)) & 63;

	// Axes to transpose (John Skilling, "Programming the Hilbert curve")
	for (uniform int32 Mask = 64 >> 1; Mask > 1; Mask >>= 1)
	{
		const uniform int32 Lower = Mask - 1;

		if ((X & Mask) != 0)
		{
			X ^= Lower;
		}
		else
		{
			const int32 Swap = X & Lower;
			X ^= Swap;
		}

		if ((Y & Mask) != 0)
		{
			X ^= Lower;
		}
		else
		{
			const int32 Swap = (X ^ Y) & Lower;
			X ^= Swap;
			Y ^= Swap;
		}

		if ((Z & Mask) != 0)
		{
			X ^= Lower;
		}
		else
		{
			const int32 Swap = (X ^ Z) & Lower;
			X ^= Swap;
			Z ^= Swap;
		}
	}

	// Gray encode
	Y ^= X;
	Z ^= Y;

	int32 Gray = 0;
	for (uniform int32 Mask = 64 >> 1; Mask > 1; Mask >>= 1)
	{
		if ((Z & Mask) != 0)
		{
			Gray ^= Mask - 1;
		}
	}
	X ^= Gray;
	Y ^= Gray;
	Z ^= Gray;

	// Interleave the transposed bits into a single index
	int32 Result = 0;
	for (uniform int32 Bit = 5; Bit >= 0; Bit--)
	{
		Result = (Result << 1) | ((X >> Bit) & 1);
		Result = (Result << 1) | ((Y >> Bit) & 1);
		Result = (Result << 1) | ((Z >> Bit) & 1);
	}

	// Compute in double: fract of golden ratio times a large index needs more than float precision
	const double GoldenIndex = 0.6180339887498948482d * (double)Result;
	return (float)(GoldenIndex - floor(GoldenIndex)) * 2.f - 1.f;
}

// Impact crater shells, 3D extension of https://www.shadertoy.com/view/XsGBDt
FORCEINLINE float FCCrater3D(const uint32 Seed, const float3 Position)
{
	const float3 Cell = floor(Position);
	const float3 Local = Position - Cell;

	float ValueSum = 0.f;
	float WeightSum = 0.f;
	for (uniform int32 IndexX = -1; IndexX <= 1; IndexX++)
	{
		for (uniform int32 IndexY = -1; IndexY <= 1; IndexY++)
		{
			for (uniform int32 IndexZ = -1; IndexZ <= 1; IndexZ++)
			{
				const float3 Offset = MakeFloat3(IndexX, IndexY, IndexZ);
				const float3 Random = FCHash33(Seed, Cell + Offset);
				const float Distance = length(Local - Offset - Random);
				const float Weight = exp(-4.f * Distance);
				ValueSum += Weight * sin(6.28318530718f * sqrt(max(Distance, 0.06f)));
				WeightSum += Weight;
			}
		}
	}

	return abs(ValueSum / max(WeightSum, 1e-6f)) * 2.f - 1.f;
}

FORCEINLINE float FCGabor3D(const uint32 Seed, const float3 Position)
{
	const uniform float Frequency = 8.f;

	const float3 Cell = floor(Position);
	float3 Alpha = Position - Cell;
	Alpha = Alpha * Alpha * (3.f - 2.f * Alpha);

#define FC_CORNER(X, Y, Z) sin(Frequency * dot(Position, FCHash33(Seed, Cell + MakeFloat3(X, Y, Z))))
	const float LayerA = FCLerp(
		FCLerp(FC_CORNER(0.f, 0.f, 0.f), FC_CORNER(1.f, 0.f, 0.f), Alpha.x),
		FCLerp(FC_CORNER(0.f, 1.f, 0.f), FC_CORNER(1.f, 1.f, 0.f), Alpha.x),
		Alpha.y);
	const float LayerB = FCLerp(
		FCLerp(FC_CORNER(0.f, 0.f, 1.f), FC_CORNER(1.f, 0.f, 1.f), Alpha.x),
		FCLerp(FC_CORNER(0.f, 1.f, 1.f), FC_CORNER(1.f, 1.f, 1.f), Alpha.x),
		Alpha.y);
#undef FC_CORNER

	return FCLerp(LayerA, LayerB, Alpha.z);
}

// Magnitude of the finite-difference gradient of gradient noise
FORCEINLINE float FCCurl3D(const uint32 Seed, const float3 Position)
{
	const uniform float Epsilon = 0.1f;

	const float3 CurlValue = MakeFloat3(
		FCPerlin3D_Raw(Seed, Position + MakeFloat3(Epsilon, 0.f, 0.f)) - FCPerlin3D_Raw(Seed, Position - MakeFloat3(Epsilon, 0.f, 0.f)),
		FCPerlin3D_Raw(Seed, Position + MakeFloat3(0.f, Epsilon, 0.f)) - FCPerlin3D_Raw(Seed, Position - MakeFloat3(0.f, Epsilon, 0.f)),
		FCPerlin3D_Raw(Seed, Position + MakeFloat3(0.f, 0.f, Epsilon)) - FCPerlin3D_Raw(Seed, Position - MakeFloat3(0.f, 0.f, Epsilon))) / Epsilon * 0.5f * 0.7f;

	return length(CurlValue) / 1.732f * 2.f - 1.f;
}

// Single layer of thin wavy strands, 3D extension of https://www.shadertoy.com/view/4syXRD
FORCEINLINE float FCScratchLayer3D(const uint32 Seed, const float3 Position, const float Smoothness)
{
	const uniform float Thickness = 0.02f;
	const uniform float Wavyness = 0.5f;

	const float3 Cell = floor(Position);
	const float3 Random = FCHash33(Seed, Cell) * MakeFloat3(3104.f, 554.f, 1234.f);

	float3 Local = (Position - Cell) * 2.f - 1.f;

	// Random orientation: rotate in XY then in YZ
	{
		const float SinAngle = sin(Random.x + Random.y);
		const float CosAngle = cos(Random.x + Random.y);
		Local = MakeFloat3(
			Local.x * CosAngle - Local.y * SinAngle,
			Local.x * SinAngle + Local.y * CosAngle,
			Local.z);
	}
	{
		const float SinAngle = sin(Random.y + Random.z);
		const float CosAngle = cos(Random.y + Random.z);
		Local = MakeFloat3(
			Local.x,
			Local.y * CosAngle - Local.z * SinAngle,
			Local.y * SinAngle + Local.z * CosAngle);
	}
	Local = Local + sin(Random.x - Random.y);

	// A wavy strand along Y: constrain both X and Z to make a line instead of a plane
	const float LineX = abs(Local.x - cos(Random.x + Local.y * 1.57f) * Wavyness);
	const float LineZ = abs(Local.z - sin(Random.z + Local.y * 1.57f) * Wavyness);

	float Line = FCSmoothstep(Thickness + Smoothness, Thickness - Smoothness, max(LineX, LineZ));
	Line *= Local.y * 0.5f + 0.5f;

	return Line;
}
FORCEINLINE float FCScratch3D(const uint32 Seed, const float3 Position, const float Smoothness)
{
	float3 Local = Position;
	float Width = max(Smoothness, 0.001f);

	float Scratches = 0.f;
	for (uniform int32 Index = 0; Index < 8; Index++)
	{
		Scratches = max(Scratches, FCScratchLayer3D(Seed, Local, Width));

		Local = MakeFloat3(
			Local.x + 0.7f * Local.y,
			-0.7f * Local.x + Local.y,
			Local.z);
		Local = MakeFloat3(
			Local.x,
			Local.y + 0.7f * Local.z,
			-0.7f * Local.y + Local.z) - 12.31f;

		Width *= 1.22f;
	}

	return Scratches * 2.f - 1.f;
}

// Rotated sine wavelets, 3D extension of https://www.shadertoy.com/view/wsBfzK
FORCEINLINE float FCWavelet3D(const uint32 Seed, const float3 Position, const float Phase)
{
	const uniform float Scale = 1.24f;

	float3 Local = Position + FCSeedOffset3(Seed);

	float Value = 0.f;
	float Frequency = 1.f;
	float WeightSum = 0.f;
	for (uniform int32 Index = 0; Index < 4; Index++)
	{
		const float3 Scaled = Local * Frequency;

		float3 Random = FCFract(floor(Scaled) * MakeFloat3(123.34f, 233.53f, 314.15f));
		Random = Random + dot(Random, Random + 23.234f);
		const float AngleA = FCFract(Random.x * Random.y) * 1e3f;
		const float AngleB = FCFract(Random.y * Random.z) * 1e3f;

		float3 Rotated = FCFract(Scaled) - 0.5f;
		{
			const float SinAngle = sin(AngleA);
			const float CosAngle = cos(AngleA);
			Rotated = MakeFloat3(
				Rotated.x * CosAngle + Rotated.y * SinAngle,
				-Rotated.x * SinAngle + Rotated.y * CosAngle,
				Rotated.z);
		}
		{
			const float SinAngle = sin(AngleB);
			const float CosAngle = cos(AngleB);
			Rotated = MakeFloat3(
				Rotated.x,
				Rotated.y * CosAngle + Rotated.z * SinAngle,
				-Rotated.y * SinAngle + Rotated.z * CosAngle);
		}

		Value += sin(Rotated.x * 10.f + Phase) * FCSmoothstep(0.25f, 0.f, dot(Rotated, Rotated)) / Frequency;

		Local = MakeFloat3(
			0.54f * Local.x - 0.84f * Local.y,
			0.84f * Local.x + 0.54f * Local.y,
			Local.z);
		Local = MakeFloat3(
			Local.x,
			0.54f * Local.y - 0.84f * Local.z,
			0.84f * Local.y + 0.54f * Local.z) + (uniform float)Index;

		WeightSum += 1.f / Frequency;
		Frequency *= Scale;
	}

	return Value / WeightSum;
}

struct FGullies3DResult
{
	float Value;
	float3 Gradient;
};

// 3D extension of the erosion gullies: carve perpendicular to the local slope
FORCEINLINE FGullies3DResult FCGullies3D(const uint32 Seed, const float3 Position, const float3 Slope)
{
	// Stable direction perpendicular to the slope: cross with the axis least aligned with it
	const float3 AbsSlope = abs(Slope);
	float3 Axis;
	if (AbsSlope.x <= AbsSlope.y && AbsSlope.x <= AbsSlope.z)
	{
		Axis = MakeFloat3(1.f, 0.f, 0.f);
	}
	else if (AbsSlope.y <= AbsSlope.z)
	{
		Axis = MakeFloat3(0.f, 1.f, 0.f);
	}
	else
	{
		Axis = MakeFloat3(0.f, 0.f, 1.f);
	}
	const float3 SideDirection = cross(Slope, Axis) * 3.14159265f;

	const float3 Cell = floor(Position);
	const float3 Local = Position - Cell;

	float2 HeightSlope = MakeFloat2(0.f, 0.f);
	float WeightSum = 0.f;
	for (uniform int32 IndexX = -1; IndexX <= 2; IndexX++)
	{
		for (uniform int32 IndexY = -1; IndexY <= 2; IndexY++)
		{
			for (uniform int32 IndexZ = -1; IndexZ <= 2; IndexZ++)
			{
				const float3 Offset = MakeFloat3(IndexX, IndexY, IndexZ);
				const float3 Delta = Local - Offset - FCHash33(Seed, Cell + Offset) + 0.5f;
				const float DistanceSquared = dot(Delta, Delta);
				const float Weight = max(0.f, exp(-DistanceSquared * 2.f) - 0.01111f);
				WeightSum += Weight;

				const float Along = dot(Delta, SideDirection);
				HeightSlope = HeightSlope + MakeFloat2(cos(Along), -sin(Along)) * Weight;
			}
		}
	}

	WeightSum = max(WeightSum, 1e-6f);

	FGullies3DResult Result;
	Result.Value = HeightSlope.x / WeightSum;
	Result.Gradient = SideDirection * (HeightSlope.y / WeightSum);
	return Result;
}
// Gradient noise with slope-following gullies, 3D extension of https://www.shadertoy.com/view/sf23W1
FORCEINLINE float FCErosion3D(const uint32 Seed, const float3 Position)
{
	float3 Gradient;
	float Value = FCPerlinDeriv3D(Seed, Position, &Gradient);

	float Strength = 0.25f;
	float Frequency = 8.f;
	float Total = 1.f;
	for (uniform int32 Index = 0; Index < 4; Index++)
	{
		const float SlopeLengthSquared = max(dot(Gradient, Gradient), 1e-12f);
		const FGullies3DResult Gully = FCGullies3D(Seed, Position * Frequency, Gradient * pow(SlopeLengthSquared, -0.25f));

		Value += Gully.Value * Strength;
		Gradient = Gradient + Gully.Gradient * (Strength * Frequency);

		Total += Strength;
		Strength *= 0.5f;
		Frequency *= 2.f;
	}

	return Value / Total;
}

FORCEINLINE float FCPaper3D(const uint32 Seed, const float3 Position)
{
	float3 Local = Position;

	float3 Sum = MakeFloat3(0.f, 0.f, 0.f);
	float WeightSum = 0.f;
	float Weight = 1.f;
	for (uniform int32 Index = 0; Index < 10; Index++)
	{
		float3 Gradient;
		FCPerlinDeriv3D(Seed, Local, &Gradient);

		Sum = Sum + FCClamp01(Gradient * 0.5f + 0.5f) * Weight;
		WeightSum += Weight;
		Weight *= 0.8f;
		Local = Local * 2.f;
	}
	Sum = Sum / WeightSum;

	return (length(Sum) / 1.732f * 0.6f + 0.4f) * 2.f - 1.f;
}

FORCEINLINE float FCStone3D(const uint32 Seed, const float3 Position)
{
	float3 WarpGradient = MakeFloat3(0.f, 0.f, 0.f);
	{
		float3 Local = Position;
		float Weight = 1.f;
		for (uniform int32 Index = 0; Index < 6; Index++)
		{
			float3 Gradient;
			FCPerlinDeriv3D(Seed, Local, &Gradient);

			WarpGradient = WarpGradient + Gradient * Weight;
			Weight *= 0.5f;
			Local = Local * 2.f;
		}
	}

	float3 Local = Position + WarpGradient * 0.4f;

	float Sum = 0.f;
	float WeightSum = 0.f;
	float Weight = 1.f;
	for (uniform int32 Index = 0; Index < 6; Index++)
	{
		Sum += (FCPerlin3D_Raw(Seed, Local) * 0.7f + 0.5f) * Weight;
		WeightSum += Weight;
		Weight *= 0.5f;
		Local = Local * 2.f;
	}

	return Sum / WeightSum * 2.f - 1.f;
}

FORCEINLINE float FCWool3D(const uint32 Seed, const float3 Position)
{
	float3 Local = Position;

	float3 Sum = MakeFloat3(0.f, 0.f, 0.f);
	float WeightSum = 0.f;
	float Weight = 1.f;
	for (uniform int32 Index = 0; Index < 6; Index++)
	{
		float3 Gradient;
		FCPerlinDeriv3D(Seed, Local, &Gradient);

		Sum = Sum + Gradient * Weight;
		WeightSum += Weight;
		Weight *= 0.5f;
		Local = Local * 2.f;
	}
	Sum = Sum / WeightSum;

	return FCMax3(abs(Sum.x), abs(Sum.y), abs(Sum.z)) * 2.f - 1.f;
}

// Interleaved gradient noise
FORCEINLINE float FCInterleavedGradient3D(const uint32 Seed, const float3 Position)
{
	const float3 Local = Position + FCSeedOffset3(Seed);
	return FCFract(52.9829189f * FCFract(dot(Local, MakeFloat3(0.06711056f, 0.00583715f, 0.00974572f)))) * 2.f - 1.f;
}

///////////////////////////////////////////////////////////////////////////////
// Kernels
///////////////////////////////////////////////////////////////////////////////

enum EProceduralNoise2D
{
	ProceduralNoise2D_Perlin,
	ProceduralNoise2D_Simplex,
	ProceduralNoise2D_Value,
	ProceduralNoise2D_Worley,
	ProceduralNoise2D_Voronoi,
	ProceduralNoise2D_Blue,
	ProceduralNoise2D_HilbertBlue,
	ProceduralNoise2D_Crater,
	ProceduralNoise2D_Gabor,
	ProceduralNoise2D_Curl,
	ProceduralNoise2D_Scratch,
	ProceduralNoise2D_Wavelet,
	ProceduralNoise2D_Erosion,
	ProceduralNoise2D_Paper,
	ProceduralNoise2D_Stone,
	ProceduralNoise2D_Wool,
	ProceduralNoise2D_InterleavedGradient,
};

enum EProceduralNoise3D
{
	ProceduralNoise3D_Perlin,
	ProceduralNoise3D_Simplex,
	ProceduralNoise3D_Value,
	ProceduralNoise3D_Worley,
	ProceduralNoise3D_Voronoi,
	ProceduralNoise3D_Blue,
	ProceduralNoise3D_HilbertBlue,
	ProceduralNoise3D_Crater,
	ProceduralNoise3D_Gabor,
	ProceduralNoise3D_Curl,
	ProceduralNoise3D_Scratch,
	ProceduralNoise3D_Wavelet,
	ProceduralNoise3D_Erosion,
	ProceduralNoise3D_Paper,
	ProceduralNoise3D_Stone,
	ProceduralNoise3D_Wool,
	ProceduralNoise3D_InterleavedGradient,
};

struct FProceduralOctave2D
{
	EProceduralNoise2D Type;
	bool bStrengthIsConstant;
	float StrengthConstant;
	const float* StrengthArray;
};

struct FProceduralOctave3D
{
	EProceduralNoise3D Type;
	bool bStrengthIsConstant;
	float StrengthConstant;
	const float* StrengthArray;
};

export void VoxelNode_ProceduralNoise2D(
	Input_float2(Position),
	Input_float(Amplitude),
	Input_float(FeatureScale),
	Input_float(Lacunarity),
	Input_float(Gain),
	Input_float(VoronoiSmoothness),
	Input_float(WaveletPhase),
	Input_float(ScratchSmoothness),
	const uniform FProceduralOctave2D Octaves[],
	const uniform int32 NumOctaves,
	const uniform int32 InSeed,
	uniform float ReturnValue[],
	const uniform int32 Num)
{
	foreach (Index = 0 ... Num)
	{
		const varying float Lacunarity = Load_float(Lacunarity, Index);
		const varying float Gain = Load_float(Gain, Index);
		const varying float VoronoiSmoothness = Load_float(VoronoiSmoothness, Index);
		const varying float WaveletPhase = Load_float(WaveletPhase, Index);
		const varying float ScratchSmoothness = Load_float(ScratchSmoothness, Index);

		varying float Sum = 0.f;
		varying float AmplitudeSum = 0.f;

		varying float Amplitude = 1.f;
		varying float2 Position = Load_float2(Position, Index) / Load_float(FeatureScale, Index);
		uniform int32 Seed = InSeed;

		for (uniform int32 OctaveIndex = 0; OctaveIndex < NumOctaves; OctaveIndex++)
		{
			const uniform FProceduralOctave2D Octave = Octaves[OctaveIndex];
			const uniform uint32 OctaveSeed = (uniform uint32)Seed;

			varying float Noise;
			switch (Octave.Type)
			{
			default:
			case ProceduralNoise2D_Perlin:
			{
				Noise = FCPerlin2D(OctaveSeed, Position);
			}
			break;
			case ProceduralNoise2D_Simplex:
			{
				Noise = FCSimplex2D(OctaveSeed, Position);
			}
			break;
			case ProceduralNoise2D_Value:
			{
				Noise = FCValue2D(OctaveSeed, Position);
			}
			break;
			case ProceduralNoise2D_Worley:
			{
				Noise = FCWorley2D(OctaveSeed, Position);
			}
			break;
			case ProceduralNoise2D_Voronoi:
			{
				Noise = FCVoronoi2D(OctaveSeed, Position, VoronoiSmoothness);
			}
			break;
			case ProceduralNoise2D_Blue:
			{
				Noise = FCBlue2D(OctaveSeed, Position);
			}
			break;
			case ProceduralNoise2D_HilbertBlue:
			{
				Noise = FCHilbertBlue2D(OctaveSeed, Position);
			}
			break;
			case ProceduralNoise2D_Crater:
			{
				Noise = FCCrater2D(OctaveSeed, Position);
			}
			break;
			case ProceduralNoise2D_Gabor:
			{
				Noise = FCGabor2D(OctaveSeed, Position);
			}
			break;
			case ProceduralNoise2D_Curl:
			{
				Noise = FCCurl2D(OctaveSeed, Position);
			}
			break;
			case ProceduralNoise2D_Scratch:
			{
				Noise = FCScratch2D(OctaveSeed, Position, ScratchSmoothness);
			}
			break;
			case ProceduralNoise2D_Wavelet:
			{
				Noise = FCWavelet2D(OctaveSeed, Position, WaveletPhase);
			}
			break;
			case ProceduralNoise2D_Erosion:
			{
				Noise = FCErosion2D(OctaveSeed, Position);
			}
			break;
			case ProceduralNoise2D_Paper:
			{
				Noise = FCPaper2D(OctaveSeed, Position);
			}
			break;
			case ProceduralNoise2D_Stone:
			{
				Noise = FCStone2D(OctaveSeed, Position);
			}
			break;
			case ProceduralNoise2D_Wool:
			{
				Noise = FCWool2D(OctaveSeed, Position);
			}
			break;
			case ProceduralNoise2D_InterleavedGradient:
			{
				Noise = FCInterleavedGradient2D(OctaveSeed, Position);
			}
			break;
			}

			const varying float Strength = Octave.bStrengthIsConstant ? Octave.StrengthConstant : Octave.StrengthArray[Index];

			const varying float NewSum = Sum + Noise * Strength * Amplitude;
			Sum = select(FCIsFinite(NewSum), NewSum, Sum);

			AmplitudeSum = AmplitudeSum + abs(Strength * Amplitude);
			Amplitude = Amplitude * Gain;
			Position = Position * Lacunarity;
			Seed = (Seed * 196314165) + 907633515;
		}

		const varying float BaseAmplitude = Load_float(Amplitude, Index);
		ReturnValue[Index] = Sum / (AmplitudeSum == 0.f ? 1.f : AmplitudeSum) * BaseAmplitude;
	}
}

export void VoxelNode_ProceduralNoise3D(
	Input_float3(Position),
	Input_float(Amplitude),
	Input_float(FeatureScale),
	Input_float(Lacunarity),
	Input_float(Gain),
	Input_float(VoronoiSmoothness),
	Input_float(WaveletPhase),
	Input_float(ScratchSmoothness),
	const uniform FProceduralOctave3D Octaves[],
	const uniform int32 NumOctaves,
	const uniform int32 InSeed,
	uniform float ReturnValue[],
	const uniform int32 Num)
{
	foreach (Index = 0 ... Num)
	{
		const varying float Lacunarity = Load_float(Lacunarity, Index);
		const varying float Gain = Load_float(Gain, Index);
		const varying float VoronoiSmoothness = Load_float(VoronoiSmoothness, Index);
		const varying float WaveletPhase = Load_float(WaveletPhase, Index);
		const varying float ScratchSmoothness = Load_float(ScratchSmoothness, Index);

		varying float Sum = 0.f;
		varying float AmplitudeSum = 0.f;

		varying float Amplitude = 1.f;
		varying float3 Position = Load_float3(Position, Index) / Load_float(FeatureScale, Index);
		uniform int32 Seed = InSeed;

		for (uniform int32 OctaveIndex = 0; OctaveIndex < NumOctaves; OctaveIndex++)
		{
			const uniform FProceduralOctave3D Octave = Octaves[OctaveIndex];
			const uniform uint32 OctaveSeed = (uniform uint32)Seed;

			varying float Noise;
			switch (Octave.Type)
			{
			default:
			case ProceduralNoise3D_Perlin:
			{
				Noise = FCPerlin3D(OctaveSeed, Position);
			}
			break;
			case ProceduralNoise3D_Simplex:
			{
				Noise = FCSimplex3D(OctaveSeed, Position);
			}
			break;
			case ProceduralNoise3D_Value:
			{
				Noise = FCValue3D(OctaveSeed, Position);
			}
			break;
			case ProceduralNoise3D_Worley:
			{
				Noise = FCWorley3D(OctaveSeed, Position);
			}
			break;
			case ProceduralNoise3D_Voronoi:
			{
				Noise = FCVoronoi3D(OctaveSeed, Position, VoronoiSmoothness);
			}
			break;
			case ProceduralNoise3D_Blue:
			{
				Noise = FCBlue3D(OctaveSeed, Position);
			}
			break;
			case ProceduralNoise3D_HilbertBlue:
			{
				Noise = FCHilbertBlue3D(OctaveSeed, Position);
			}
			break;
			case ProceduralNoise3D_Crater:
			{
				Noise = FCCrater3D(OctaveSeed, Position);
			}
			break;
			case ProceduralNoise3D_Gabor:
			{
				Noise = FCGabor3D(OctaveSeed, Position);
			}
			break;
			case ProceduralNoise3D_Curl:
			{
				Noise = FCCurl3D(OctaveSeed, Position);
			}
			break;
			case ProceduralNoise3D_Scratch:
			{
				Noise = FCScratch3D(OctaveSeed, Position, ScratchSmoothness);
			}
			break;
			case ProceduralNoise3D_Wavelet:
			{
				Noise = FCWavelet3D(OctaveSeed, Position, WaveletPhase);
			}
			break;
			case ProceduralNoise3D_Erosion:
			{
				Noise = FCErosion3D(OctaveSeed, Position);
			}
			break;
			case ProceduralNoise3D_Paper:
			{
				Noise = FCPaper3D(OctaveSeed, Position);
			}
			break;
			case ProceduralNoise3D_Stone:
			{
				Noise = FCStone3D(OctaveSeed, Position);
			}
			break;
			case ProceduralNoise3D_Wool:
			{
				Noise = FCWool3D(OctaveSeed, Position);
			}
			break;
			case ProceduralNoise3D_InterleavedGradient:
			{
				Noise = FCInterleavedGradient3D(OctaveSeed, Position);
			}
			break;
			}

			const varying float Strength = Octave.bStrengthIsConstant ? Octave.StrengthConstant : Octave.StrengthArray[Index];

			const varying float NewSum = Sum + Noise * Strength * Amplitude;
			Sum = select(FCIsFinite(NewSum), NewSum, Sum);

			AmplitudeSum = AmplitudeSum + abs(Strength * Amplitude);
			Amplitude = Amplitude * Gain;
			Position = Position * Lacunarity;
			Seed = (Seed * 196314165) + 907633515;
		}

		const varying float BaseAmplitude = Load_float(Amplitude, Index);
		ReturnValue[Index] = Sum / (AmplitudeSum == 0.f ? 1.f : AmplitudeSum) * BaseAmplitude;
	}
}
