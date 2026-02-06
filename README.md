# VCET - Voxel Community Extra Tools

Community-made extra tools for the [Voxel Plugin](https://voxelplugin.com/).

## Features

### Spherical Texture Baker
Bakes Voxel volume layer data to equirectangular render targets for **spherical/planetary worlds**.
- Sample at configurable radii (cloud altitude, surface, etc.)
- Auto-detects metadata type (Float, LinearColor, Normal)
- Multiple layers (Cloud, Land)

### Planar Texture Baker
Bakes Voxel volume layer data to flat render targets for **non-spherical/flat worlds**.
- Sample at configurable heights and world bounds
- Auto-detects metadata type (Float, LinearColor, Normal)
- Multiple layers (Primary, Secondary)

### Cloud Animation System
Realistic animated cloud movement for spherical planets.
- **Latitude-based wind patterns**: Trade winds, westerlies, polar easterlies
- **Flowmap distortion**: Local swirling for storms and cyclones
- **Curl noise turbulence**: Small-scale detail movement
- **Coriolis rotation**: Proper cyclone rotation per hemisphere
- **Material Parameter Collection integration**: Easy material setup

## Requirements
- Unreal Engine 5.7+
- [Voxel Plugin 2.0p8+](https://voxelplugin.com/)

## Installation

### As a Git Submodule (Recommended)
This is the recommended method as it allows easy updates and version tracking.

```bash
# Navigate to your project's Plugins folder
cd YourProject/Plugins

# Add VCET as a submodule
git submodule add https://github.com/ZundleFire/VCET.git VCET

# Commit the submodule addition
cd ..
git add .gitmodules Plugins/VCET
git commit -m "Add VCET as submodule"
```

### Cloning a Project with VCET Submodule
If you're cloning a project that already uses VCET as a submodule:

```bash
# Clone with submodules
git clone --recurse-submodules https://github.com/YourUsername/YourProject.git

# Or if already cloned, initialize submodules
git submodule update --init --recursive
```

### Updating VCET
To update VCET to the latest version:

```bash
cd Plugins/VCET
git pull origin main
cd ../..
git add Plugins/VCET
git commit -m "Update VCET submodule"
```

### Manual Installation
1. Download or clone this repository
2. Copy the `VCET` folder to your project's `Plugins` directory

## Usage

### Supported Metadata Types
Both bakers automatically detect the metadata type and write to appropriate channels:

| Metadata Type | Output Channels | Description |
|---------------|-----------------|-------------|
| **Float Metadata** | R channel only | Single value written to red channel |
| **Linear Color Metadata** | RGBA channels | Full color with alpha |
| **Normal Metadata** | RGB channels | Normal vectors remapped to 0-1 |
| **None (null)** | Grayscale (RGB) | Distance field sampled as grayscale |

### Spherical Texture Baker
Use for planetary/spherical worlds with equirectangular projection.

1. Add the **VCET Spherical Texture Baker** component to an actor
2. Configure `VolumeLayer` to your Voxel volume layer
3. Set `SphereCenter` and layer radii (CloudRadius, LandRadius)
4. Assign `CloudMetadata` / `LandMetadata` to your metadata asset
5. Call `ForceRebake()` to bake, or enable `bBakeOnBeginPlay`

**Blueprint Functions:**
- `ForceRebake()` - Bake all enabled layers
- `ForceRebakeCloud()` - Bake cloud layer only
- `ForceRebakeLand()` - Bake land layer only
- `RequestGlobalRebake()` - Trigger all bakers in the world

### Planar Texture Baker
Use for flat/non-spherical worlds with top-down projection.

1. Add the **VCET Planar Texture Baker** component to an actor
2. Configure `VolumeLayer` to your Voxel volume layer
3. Set `WorldCenter`, `WorldSize`, and layer heights (PrimaryHeight, SecondaryHeight)
4. Assign `PrimaryMetadata` / `SecondaryMetadata` to your metadata asset
5. Call `ForceRebake()` to bake, or enable `bBakeOnBeginPlay`

**Blueprint Functions:**
- `ForceRebake()` - Bake all enabled layers
- `ForceRebakePrimary()` - Bake primary layer only
- `ForceRebakeSecondary()` - Bake secondary layer only
- `RequestGlobalRebake()` - Trigger all bakers in the world

### Cloud Animation Component
For animated cloud movement on spherical planets.

**Setup:**
1. Create a **Material Parameter Collection** with these scalar parameters:
   - `CloudTime`, `EquatorWindSpeed`, `PolarWindSpeed`, `WindReversalLatitude`
   - `TurbulenceStrength`, `TurbulenceScale`, `TurbulenceSpeed`
   - `FlowPhase1`, `FlowPhase2`, `FlowBlendFactor`
2. Add the **VCET Cloud Animation** component to your sky sphere or game mode
3. Assign your MPC to `CloudMPC`
4. Configure `AnimParams` for your planet's wind patterns

**In Your Cloud Material:**
Use the HLSL functions from `Shaders/SphericalCloudAnimation.ush` in a Custom node:

```hlsl
// Get animated UV
float2 AnimatedUV = VCET_AnimateCloudUV(
    UV,
    CloudTime,           // From MPC
    EquatorWindSpeed,    // From MPC
    PolarWindSpeed,      // From MPC
    0.15,                // Wind reversal latitude
    TurbulenceStrength,  // From MPC
    TurbulenceScale      // From MPC
);

// Sample cloud texture
return CloudTexture.Sample(CloudSampler, AnimatedUV);
```

**Animation Parameters:**
| Parameter | Description | Default |
|-----------|-------------|---------|
| `EquatorWindSpeed` | UV/s at equator | 0.02 |
| `PolarWindSpeed` | UV/s at poles | 0.005 |
| `WindReversalLatitude` | Where westerlies start | 0.3 |
| `TurbulenceStrength` | Curl noise intensity | 0.03 |
| `TurbulenceScale` | Curl noise frequency | 2.0 |

## License
MIT License - See LICENSE file

## Contributing
Contributions welcome! Please open issues or pull requests.
