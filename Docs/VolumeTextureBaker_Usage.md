# VCET Volume Texture Baker

## Overview

The Volume Texture Baker component bakes Voxel Plugin VOLUME layer data into 3D volume textures (UTextureRenderTargetVolume) for use in volumetric rendering effects.

## Features

- ✅ Bakes voxel volume data to true 3D textures
- ✅ Automatic format detection and adaptation
- ✅ Supports distance field or float metadata sampling
- ✅ Configurable sampling region and resolution
- ✅ Processing options: remap, normalize, invert
- ✅ Blueprint and C++ friendly
- ✅ Async baking with completion events

## Quick Start

### 1. Add Component
Add the **VCET Volume Texture Baker** component to any Actor in your scene.

### 2. Configure Voxel Layer
- Set `VolumeLayer` to your Voxel VOLUME layer
- Optionally set `Metadata` to sample specific float metadata (or leave empty for distance field)

### 3. Set Sampling Region
- `VolumeCenter`: World-space center of the sampling region
- `VolumeSize`: Dimensions of the region (e.g., 50000×50000×50000 cm)

### 4. Set Resolution
- `VolumeResolution`: Grid resolution (e.g., 64, 128, 256)
  - Higher = more detail, more memory
  - 128³ is typical for volumetric clouds (16MB)

### 5. Bake
- Call `ForceRebake()` in Blueprint/C++
- Or enable `bBakeOnBeginPlay` to auto-bake on level start

### 6. Use in Materials
- Access the baked texture via `GetVolumeTexture()`
- Use in material as a Volume Texture sample

## Technical Details

### Output Format
- **Always PF_FloatRGBA** (RGBA 16-bit float)
- 8 bytes per voxel
- This is a UE5 limitation - volume render targets ignore format parameters

### Memory Usage
```
Memory = Resolution³ × 8 bytes

Examples:
- 32³  = 256KB
- 64³  = 2MB
- 128³ = 16MB
- 256³ = 128MB
```

### Processing Options

**bRemapNegativeToPositive** (default: true)
- Remaps distance field values from (-1,1) to (0,1)
- Useful for distance fields where negative = inside

**bAutoNormalize** (default: true)
- Normalizes values to 0-1 range based on min/max
- Ensures full dynamic range utilization

**bInvertResult** (default: false)
- Inverts values: output = 1 - input
- Useful for inverting solid/empty

**ResultMultiplier** (default: 1.0)
- Scales values before clamping
- Useful for adjusting density strength

## Blueprint Examples

### Basic Baking
```cpp
// Get the baker component
UVolumeTextureBaker* Baker = GetComponentByClass(UVolumeTextureBaker::StaticClass());

// Trigger a bake
Baker->ForceRebake();

// Bind to completion event
Baker->OnBakeComplete.AddDynamic(this, &AMyActor::OnBakeFinished);
```

### Check Baking Status
```cpp
if (!Baker->IsBaking())
{
    // Safe to rebake
    Baker->ForceRebake();
}
```

### Get Baked Texture
```cpp
UTextureRenderTargetVolume* VolumeTexture = Baker->GetVolumeTexture();

// Set on material instance
MaterialInstance->SetTextureParameterValue("CloudDensityTexture", VolumeTexture);
```

### Global Rebake
```cpp
// Rebake ALL volume texture bakers in the world
UVolumeTextureBaker::RequestGlobalRebake(GetWorld());
```

## Material Usage

### Sample in Material

1. Create a **Material Parameter Collection** (recommended)
   - Add a Texture parameter for your volume texture
   - Update it after baking

2. In your material:
   - Add a `VolumeTextureSampleParameter3D` node
   - Connect UVW coordinates (0-1 range)
   - Sample the density

### Volumetric Cloud Example
```
// In material:
WorldPosition → VolumeSpaceTransform → VolumeTextureSample → Density

Where VolumeSpaceTransform:
- Translate by -VolumeCenter
- Scale by 1/VolumeSize
- Add 0.5 to center in 0-1 space
```

## Use Cases

### Volumetric Clouds
- Resolution: 128³ or 256³
- Use distance field or float metadata for density
- Enable `bRemapNegativeToPositive` and `bAutoNormalize`
- Sample in volumetric material with ray marching

### 3D Fog/Mist
- Resolution: 64³ or 128³
- Use float metadata for density variation
- Combine with height fog in post-process

### Particle Density Fields
- Resolution: 32³ or 64³
- Use as velocity/density source for Niagara particles
- Update periodically if voxel data changes

### Volumetric Lighting
- Resolution: 32³ or 64³
- Use as light scattering/absorption volume
- Cheaper than real-time sampling

## Performance Tips

1. **Choose appropriate resolution**
   - Start low (64³) and increase if needed
   - Remember: memory scales cubically!

2. **Bake once at startup**
   - Enable `bBakeOnBeginPlay` for static scenes
   - Or call `ForceRebake()` after level load

3. **Avoid frequent rebaking**
   - Baking is async but still expensive
   - Check `IsBaking()` before triggering new bakes
   - For dynamic effects, consider smaller update regions

4. **External render targets**
   - Provide your own `VolumeRenderTarget` to reuse across bakers
   - Useful for managing memory explicitly

## Troubleshooting

### Texture is all black/white
- Check `VolumeCenter` and `VolumeSize` cover your voxel data
- Verify your VolumeLayer is valid and has data
- Try disabling `bAutoNormalize` to see raw values
- Check metadata is being used correctly

### Out of memory
- Reduce `VolumeResolution` (try 64³ or 32³)
- Consider multiple smaller volumes instead of one large one

### Baking never completes
- Check Output Log for errors
- Verify VolumeLayer exists in the scene
- Ensure voxel data is accessible

### Format mismatch errors
- This should no longer occur with the fixed version
- If it does, report it - the baker auto-detects format now

## API Reference

### Properties

| Property | Type | Default | Description |
|----------|------|---------|-------------|
| `VolumeLayer` | FVoxelStackVolumeLayer | - | Voxel VOLUME layer to sample |
| `Metadata` | UVoxelMetadata* | null | Optional float metadata to sample |
| `VolumeCenter` | FVector | (0,0,0) | World-space center of sampling region |
| `VolumeSize` | FVector | (50k,50k,50k) | Size of sampling region |
| `VolumeRenderTarget` | UTextureRenderTargetVolume* | null | External volume texture (optional) |
| `VolumeResolution` | int32 | 128 | Cubic grid resolution (4-256) |
| `bRemapNegativeToPositive` | bool | true | Remap (-1,1) to (0,1) |
| `bAutoNormalize` | bool | true | Normalize to full 0-1 range |
| `bInvertResult` | bool | false | Invert values (1-x) |
| `ResultMultiplier` | float | 1.0 | Scale values before clamp |
| `bBakeOnBeginPlay` | bool | false | Auto-bake on level start |

### Functions

| Function | Description |
|----------|-------------|
| `ForceRebake()` | Trigger a baking operation |
| `GetVolumeTexture()` | Get the output volume texture |
| `IsBaking()` | Check if currently baking |
| `RequestGlobalRebake(World)` | Static: Rebake all bakers in world |

### Events

| Event | Description |
|-------|-------------|
| `OnBakeComplete` | Fired when baking finishes |

## Version History

### v1.1 (Current)
- ✅ Fixed D3D12 row pitch alignment crash
- ✅ Removed non-functional format enum (UE5 limitation)
- ✅ Added automatic format detection
- ✅ Improved documentation

### v1.0
- Initial release

## Known Limitations

1. **Format locked to PF_FloatRGBA**
   - UE5's UTextureRenderTargetVolume ignores format parameters
   - Always creates RGBA 16-bit float (8 bytes/voxel)
   - Cannot create grayscale or 8-bit formats currently

2. **No streaming**
   - Entire volume must fit in memory
   - Consider multiple smaller volumes for large worlds

3. **Static baking**
   - Not designed for every-frame updates
   - For dynamic effects, consider smaller update regions or lower resolutions

## Support

For issues or questions, check:
- Output Log for detailed error messages
- This documentation
- VCET plugin source code (well-commented)
