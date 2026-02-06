# VCET - Voxel Community Extra Tools

Community-made extra tools for the [Voxel Plugin](https://voxelplugin.com/).

## Features

### Spherical Texture Baker
Bakes Voxel volume layer data to equirectangular render targets for **spherical/planetary worlds**.
- Sample at configurable radii (cloud altitude, surface, etc.)
- Supports RGBA via Linear Color Metadata or grayscale
- Multiple layers (Cloud, Land)

### Planar Texture Baker
Bakes Voxel volume layer data to flat render targets for **non-spherical/flat worlds**.
- Sample at configurable heights
- Configurable world bounds
- Supports RGBA via Linear Color Metadata or grayscale
- Multiple layers (Primary, Secondary)

## Installation

### As a Git Submodule (Recommended)
```bash
cd YourProject/Plugins
git submodule add https://github.com/ZundleFire/VCET.git VCET
```

### Manual
1. Download or clone this repository
2. Copy the `VCET` folder to your project's `Plugins` directory

## Requirements
- Unreal Engine 5.x
- [Voxel Plugin](https://voxelplugin.com/) (Pro or Free)

## Usage

### Spherical Texture Baker
1. Add the **Spherical Texture Baker** component to an actor
2. Configure `VolumeLayer` to your Voxel volume layer
3. Set `SphereCenter` and layer radii
4. Optionally assign `ColorMetadata` for RGBA output
5. Call `ForceRebake()` or bind to events

### Planar Texture Baker
1. Add the **Planar Texture Baker** component to an actor
2. Configure `VolumeLayer` to your Voxel volume layer
3. Set `WorldCenter`, `WorldSize`, and layer heights
4. Optionally assign `ColorMetadata` for RGBA output
5. Call `ForceRebake()` or bind to events

## License
MIT License - See LICENSE file

## Contributing
Contributions welcome! Please open issues or pull requests.
