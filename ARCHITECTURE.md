# VCET Architecture

This document describes the internal architecture of VCET to help contributors understand how the plugin works.

## Overview

VCET provides texture baking components that sample Voxel Plugin volume layers and write the results to render targets. The plugin is designed to be:

- **Async-first**: Sampling happens on background threads
- **Metadata-agnostic**: Auto-detects Float, LinearColor, and Normal metadata
- **Modular**: Easy to add new baker types

## Core Components

### 1. Spherical Texture Baker (`USphericalTextureBaker`)

**Purpose**: Bakes volumetric data to equirectangular (lat/long) textures for spherical planets.

**Key Methods:**
- `ForceRebake()` - Triggers baking of all enabled layers
- `BakeLayer()` - Core baking logic (async)
- `CreateRT()` - Creates or assigns render targets

**Baking Flow:**
```
1. Generate spherical position buffer (lat/long ? 3D positions)
2. Detect metadata type (Float/LinearColor/Normal)
3. Create appropriate metadata refs
4. Async: Query voxel layer with positions
5. Async: Sample metadata buffers
6. Async: Process values (remap, normalize, etc.)
7. GameThread: Write to render target via RHI
8. Broadcast completion delegate
```

**Thread Safety:**
- Component state (`bIsBaking`) modified on GameThread only
- Heavy sampling done in Voxel async tasks
- RHI commands enqueued from GameThread

### 2. Planar Texture Baker (`UPlanarTextureBaker`)

**Purpose**: Bakes volumetric data to flat top-down textures.

**Key Differences from Spherical:**
- Uses planar XY grid instead of spherical coordinates
- Configurable world bounds (`WorldCenter`, `WorldSize`)
- Multiple height layers for clouds/ground

**Position Generation:**
```cpp
// Spherical: Sphere surface coordinates
Pos = Radius * (SinLat*CosLon, SinLat*SinLon, CosLat)

// Planar: Flat XY grid at fixed Z
Pos = (Lerp(MinX, MaxX, U), Lerp(MinY, MaxY, V), SampleHeight)
```

## Data Flow

### High-Level Pipeline

```
User Component (GameThread)
    ?
Generate Position Buffer
    ?
Voxel Async Task (Worker Thread)
    ??? Query Volume Layer
    ??? Sample Metadata
    ??? Process Values
    ?
Return to GameThread
    ?
Write to RenderTarget (RHI Thread)
    ?
Broadcast Completion
```

### Metadata Detection

```cpp
enum class EMetadataType { None, Float, LinearColor, Normal };

if (Meta)
{
    if (Cast<UVoxelFloatMetadata>(Meta))
        Type = Float;  // R channel only
    else if (Cast<UVoxelLinearColorMetadata>(Meta))
        Type = LinearColor;  // RGBA channels
    else if (Cast<UVoxelNormalMetadata>(Meta))
        Type = Normal;  // RGB channels (remapped to 0-1)
}
else
{
    Type = None;  // Sample distance field as grayscale
}
```

## Memory Management

### Render Target Creation

```cpp
void CreateRT(TObjectPtr<UTextureRenderTarget2D>& Out, 
              UTextureRenderTarget2D* External, 
              int32 W, int32 H)
{
    // Use external RT if provided
    if (External) { Out = External; return; }
    
    // Create new RT if needed
    if (!Out)
    {
        Out = NewObject<UTextureRenderTarget2D>(this);
        Out->RenderTargetFormat = bUseHDR ? RTF_RGBA16f : RTF_RGBA8;
        Out->InitAutoFormat(W, H);
        Out->UpdateResourceImmediate(true);
    }
}
```

**Memory Considerations:**
- 512x256 RGBA8 texture = ~512KB
- 512x256 RGBA16f texture = ~1MB
- Large textures (4096x2048) can use ~64MB
- External RTs allow user to control lifetime

### Async Buffer Lifecycle

```
1. Allocate on Worker Thread:
   FVoxelDoubleVectorBuffer Pos;
   Pos.Allocate(TotalSamples);

2. Sample on Worker Thread:
   FVoxelFloatBuffer Dist = Query.SampleVolumeLayer(Layer, Pos);

3. Process on Worker Thread:
   Result.Colors.SetNum(N);

4. Transfer to GameThread (via TSharedPtr):
   auto Data = MakeShared<TArray<FColor>>(MoveTemp(Pixels));

5. Enqueue RHI Command:
   ENQUEUE_RENDER_COMMAND(WriteTexture)([Data]{ ... });

6. Buffers cleaned up automatically via shared pointers
```

## Extension Points

### Module Organization Strategy

**Current Structure:**
VCET uses a **single-module architecture** for simplicity:
```
VCET/Source/VCET/  (Runtime module)
```

**When to keep features in the main VCET module:**
- ? New baker types (they share common patterns)
- ? New metadata support (minimal code)
- ? Utilities that complement existing features
- ? Performance optimizations
- ? Bug fixes

**When to create a separate module:**

| Feature Type | Example | Module Strategy |
|--------------|---------|-----------------|
| **Core Bakers** | Cylindrical, Cubemap | Main VCET module |
| **Material Tools** | Material function library, shader nodes | `VCETMaterials` module |
| **Editor Tools** | Custom editors, visualizers | `VCETEditor` module (Editor type) |
| **Advanced Features** | Flowmap generation, animation baking | `VCETAdvanced` module |
| **Experimental** | Research features, beta systems | `VCETExperimental` module |

**Multi-Module Example:**
```
VCET/
??? Source/
?   ??? VCET/              # Core: Texture bakers (always loaded)
?   ??? VCETMaterials/     # Material functions and shaders
?   ??? VCETEditor/        # Editor-only tools
?   ??? VCETAdvanced/      # Advanced features with extra deps
??? VCET.uplugin           # Declares all modules
```

**Benefits of Single Module (Current):**
- ? Simpler for contributors
- ? Faster compile times (one module to build)
- ? Easier dependency management
- ? Less configuration

**When Multi-Module Makes Sense:**
- New feature adds 5+ MB of dependencies
- Feature is editor-only (no runtime cost)
- Feature is experimental and users may want to disable it
- Different loading phases needed (e.g., PostConfigInit vs Default)

**Recommendation:**
Keep new bakers and core features in the main VCET module. Only split into modules if suggested during code review or if your feature truly needs isolation.

### Adding a New Baker Type

To add a new baker (e.g., Cylindrical, Cubemap):

1. **Create Component Class**:
```cpp
UCLASS(ClassGroup=(VCET), meta=(BlueprintSpawnableComponent))
class VCET_API UMyNewBaker : public UActorComponent
{
    // Follow same pattern as USphericalTextureBaker
};
```

2. **Implement Position Generation**:
```cpp
void GeneratePositions(FVoxelDoubleVectorBuffer& OutPos, ...)
{
    // Convert UV/parametric coords ? 3D world positions
}
```

3. **Reuse Core Baking Logic**:
```cpp
// Detection, sampling, processing is the same!
EMetadataType MetaType = DetectMetadataType(Meta);
Query.SampleVolumeLayer(Layer, Positions, MetaBuffers);
ProcessResults(MetaType, Buffer);
```

4. **Add to Build.cs** (if needed)
5. **Document in README.md**

### Adding Metadata Support

To support a new metadata type:

```cpp
// 1. Add to enum
enum class EMetadataType { ..., MyNewType };

// 2. Add detection
else if (auto* MyMeta = Cast<UVoxelMyNewMetadata>(Meta))
{
    MetaType = EMetadataType::MyNewType;
    MyRef = FVoxelMyNewMetadataRef(MyMeta);
}

// 3. Add sampling
if (MetaType == EMetadataType::MyNewType && MyRef.IsSet())
{
    // Sample and convert to FLinearColor
}
```

## Performance Considerations

### Sampling Costs

| Texture Size | Sample Count | Est. Time (Release) |
|--------------|--------------|---------------------|
| 256x128      | 32,768       | ~50ms               |
| 512x256      | 131,072      | ~150ms              |
| 1024x512     | 524,288      | ~500ms              |
| 2048x1024    | 2,097,152    | ~2000ms             |

**Bottleneck**: Voxel query sampling (scales with sample count)

### Optimization Strategies

1. **Batch Updates**: Don't rebake every frame
2. **LOD Textures**: Bake lower-res versions for distance
3. **Async Everything**: Never block GameThread
4. **External RTs**: Reuse render targets when possible
5. **HDR Only When Needed**: RGBA8 is 2x smaller than RGBA16f

## Testing

### Unit Test Checklist

- [ ] Metadata detection works for all types
- [ ] Position generation wraps correctly (longitude)
- [ ] Position generation clamps correctly (latitude)
- [ ] Null metadata falls back to distance field
- [ ] External render targets are respected
- [ ] Multiple layers can be baked simultaneously
- [ ] Memory is freed after baking

### Integration Test Checklist

- [ ] Works with Voxel Plugin volume layers
- [ ] Works with Voxel noise graphs
- [ ] Works in PIE
- [ ] Works in packaged builds
- [ ] Works on all platforms (Win64, Linux, etc.)
- [ ] No crashes with invalid inputs

## Common Issues

### "Cannot sample VolumeLayer"
- Ensure VolumeLayer is valid
- Check that Voxel Plugin is initialized
- Verify FVoxelLayers::Get() returns valid pointer

### "Render target is black"
- Check metadata assignment
- Verify voxel graph outputs to the metadata
- Ensure sample positions are correct
- Check Processing options (normalize, remap, etc.)

### "Memory leak"
- Ensure TSharedPtr is used for buffer transfers
- Check that RenderTargets are not being recreated every frame
- Use external RTs for long-lived textures

## Contributing Guidelines

See [CONTRIBUTING.md](CONTRIBUTING.md) for:
- Code style
- Testing requirements
- PR process
- Community guidelines
