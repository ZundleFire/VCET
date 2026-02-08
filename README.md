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

### Volume Texture Baker (3D)
Bakes Voxel volume layer data to **3D Volume Render Targets** for advanced volumetric effects.
- True 3D volumetric textures for ray-marched clouds
- Box region or Spherical Shell sampling modes
- Configurable resolution (up to 512³)
- Perfect for volumetric clouds, fog, and density fields

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

### Volume Texture Baker (3D)
Create true 3D volumetric textures for advanced effects like ray-marched clouds.

1. Add the **VCET Volume Texture Baker** component to an actor
2. Configure `VolumeLayer` to your Voxel volume layer
3. Choose region type:
   - **Box Mode**: Set `BoxCenter` and `BoxExtent` for a rectangular region
   - **Spherical Mode**: Enable `bUseSphericalRegion`, set `SphereCenter`, `InnerRadius`, `OuterRadius`
4. Set resolution (`VolumeResolutionX/Y/Z`) - default 128x128x64
5. Assign `Metadata` for density/color data
6. Call `ForceRebake()` to bake, or enable `bBakeOnBeginPlay`

**Blueprint Functions:**
- `ForceRebake()` - Bake the volume texture
- `GetVolumeTexture()` - Get the output volume texture
- `RequestGlobalRebake()` - Trigger all volume bakers in the world

**Volume Region Modes:**

| Mode | Use Case | Parameters |
|------|----------|------------|
| **Box** | Flat worlds, local fog volumes | `BoxCenter`, `BoxExtent` |
| **Spherical Shell** | Planetary atmospheres, cloud layers | `SphereCenter`, `InnerRadius`, `OuterRadius` |

**Performance Notes:**
- 128³ = ~2M voxels, ~8MB texture
- 256³ = ~16M voxels, ~64MB texture
- 512³ = ~134M voxels, ~512MB texture
- Bake time scales linearly with voxel count

## License
MIT License - See LICENSE file

## Contributing

We welcome community contributions! VCET is designed to be extensible and community-driven.

**Ways to Contribute:**
- Report bugs via [GitHub Issues](https://github.com/ZundleFire/VCET/issues)
- Suggest features via [GitHub Issues](https://github.com/ZundleFire/VCET/issues)
- Improve documentation
- Submit pull requests

**Getting Started:**
- Read [CONTRIBUTING.md](CONTRIBUTING.md) for guidelines
- Check [ARCHITECTURE.md](ARCHITECTURE.md) to understand the codebase
- Look for issues labeled `good-first-issue`

**Ideas for Contributions:**
- Cylindrical texture baker
- Cubemap texture baker
- Additional metadata types
- Performance optimizations
- Example projects/tutorials

## Resources

- **Documentation**: See [CONTRIBUTING.md](CONTRIBUTING.md) and [ARCHITECTURE.md](ARCHITECTURE.md)
- **Issues**: [GitHub Issues](https://github.com/ZundleFire/VCET/issues)
- **Voxel Plugin**: [voxelplugin.com](https://voxelplugin.com/)
