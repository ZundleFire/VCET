# Contributing to VCET

Thank you for your interest in contributing to VCET (Voxel Community Extra Tools)! This document provides guidelines for contributing.

## Code of Conduct

Be respectful and constructive. We're all here to make great tools for the Voxel Plugin community.

## How Can I Contribute?

### Reporting Bugs

Before creating bug reports, please check existing issues. When creating a bug report, include:

- **Clear title** describing the issue
- **Unreal Engine version** (e.g., 5.7)
- **Voxel Plugin version** (e.g., 2.0p8)
- **Steps to reproduce** the issue
- **Expected vs actual behavior**
- **Screenshots or logs** if applicable

### Suggesting Features

Feature requests are welcome! Please:

- **Check existing issues** for duplicates
- **Describe the use case** - why is this feature needed?
- **Provide examples** of how it would work
- **Consider performance** implications for large worlds

### Contributing Code

#### What to Contribute

**Good First Contributions:**
- Documentation improvements
- Bug fixes
- Performance optimizations
- New metadata type support
- Additional texture baker features

**Ideas for New Features:**
- Cylindrical texture baker
- Cubemap texture baker
- Animated texture baking
- Distance field bakers
- Mesh bakers from voxel data

#### Development Setup

1. **Fork the repository** on GitHub
2. **Clone your fork**:
   ```bash
   git clone https://github.com/YourUsername/VCET.git
   cd VCET
   ```

3. **Create a feature branch**:
   ```bash
   git checkout -b feature/your-feature-name
   ```

4. **Add VCET to a test project** as a submodule to test your changes:
   ```bash
   cd YourTestProject/Plugins
   git submodule add /path/to/your/VCET VCET
   ```

#### Code Style

**C++ Guidelines:**
- Follow Unreal Engine coding standards
- Use descriptive variable names
- Add comments for complex logic
- Keep functions focused and single-purpose

**Naming Conventions:**
```cpp
// Classes: Pascal case with prefix
class VCET_API UMyNewBaker : public UActorComponent

// Functions: Pascal case
void ForceRebake();

// Variables: Camel case with 'b' prefix for bools
bool bEnableFeature = false;
float SampleRadius = 100.0f;

// UPROPERTY macros for Blueprint exposure
UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VCET")
```

**File Organization:**
```
VCET/
??? Source/VCET/
?   ??? Public/          # Header files (.h)
?   ?   ??? MyFeature.h
?   ??? Private/         # Implementation files (.cpp)
?   ?   ??? MyFeature.cpp
?   ??? VCET.Build.cs    # Build configuration
??? Shaders/             # HLSL shader files (if needed)
??? README.md
```

### Module Organization

**When to add to the existing VCET module:**
- ? New baker types (Cylindrical, Cubemap, etc.)
- ? New metadata type support
- ? Small utility classes
- ? Performance improvements
- ? Bug fixes

**When to create a new module:**
- ?? Feature requires unique dependencies not in VCET.Build.cs
- ?? Experimental features users may want to disable
- ?? Large subsystems (e.g., material function library, editor tools)

**Creating a New Module:**
```
VCET/
??? Source/
?   ??? VCET/              # Core module (always loaded)
?   ?   ??? VCET.Build.cs
?   ??? VCETAdvanced/      # Optional module
?       ??? VCETAdvanced.Build.cs
?       ??? Public/
?       ??? Private/
```

Update `VCET.uplugin`:
```json
"Modules": [
    {
        "Name": "VCET",
        "Type": "Runtime",
        "LoadingPhase": "Default"
    },
    {
        "Name": "VCETAdvanced",
        "Type": "Runtime",
        "LoadingPhase": "Default"
    }
]
```

**Recommendation for Contributors:**
Start with the main VCET module. Only create a new module if:
1. Your feature has dependencies that would bloat VCET for all users
2. The maintainers suggest it during code review
3. You're adding editor-only tools (use `"Type": "Editor"`)

This keeps the plugin simple for most contributors while allowing growth.

#### Adding a New Baker

Template for creating a new texture baker:

1. **Create header file** `Public/MyNewBaker.h`:
```cpp
// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/TextureRenderTarget2D.h"
#include "VoxelMinimal.h"
#include "VoxelStackLayer.h"
#include "MyNewBaker.generated.h"

UCLASS(ClassGroup=(VCET), meta=(BlueprintSpawnableComponent), DisplayName="VCET My New Baker")
class VCET_API UMyNewBaker : public UActorComponent
{
    GENERATED_BODY()

public:
    UMyNewBaker();

    /** The Voxel VOLUME layer to query */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel")
    FVoxelStackVolumeLayer VolumeLayer;

    /** Force a rebake */
    UFUNCTION(BlueprintCallable, Category = "VCET|My New Baker")
    void ForceRebake();

protected:
    virtual void BeginPlay() override;
    
private:
    void BakeTexture();
};
```

2. **Create implementation** `Private/MyNewBaker.cpp`:
```cpp
// Copyright Epic Games, Inc. All Rights Reserved.

#include "MyNewBaker.h"

UMyNewBaker::UMyNewBaker()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UMyNewBaker::BeginPlay()
{
    Super::BeginPlay();
}

void UMyNewBaker::ForceRebake()
{
    BakeTexture();
}

void UMyNewBaker::BakeTexture()
{
    // Your baking logic here
}
```

3. **Update documentation** in README.md

#### Testing Your Changes

Before submitting:

- ? **Build successfully** in both Development and Shipping configurations
- ? **Test in-editor** with the Voxel Plugin
- ? **Test at runtime** in PIE and packaged builds
- ? **Check for memory leaks** with large textures
- ? **Verify Blueprint functionality** if exposed
- ? **Update documentation** if adding features

#### Commit Guidelines

Use clear, descriptive commit messages:

```bash
# Good commits
git commit -m "Add cylindrical texture baker"
git commit -m "Fix memory leak in spherical baker"
git commit -m "Improve performance of metadata detection"

# Bad commits
git commit -m "Fixed stuff"
git commit -m "Update"
```

#### Pull Request Process

1. **Update documentation** in README.md
2. **Ensure all tests pass** (build successfully)
3. **Create pull request** with:
   - Clear title describing the change
   - Description of what changed and why
   - Reference to any related issues
   - Screenshots/videos for visual changes

**PR Template:**
```markdown
## Description
Brief description of changes

## Type of Change
- [ ] Bug fix
- [ ] New feature
- [ ] Performance improvement
- [ ] Documentation update

## Testing
- Tested in UE 5.7 with Voxel Plugin 2.0p8
- Tested with [describe test scenario]

## Screenshots
(if applicable)

## Checklist
- [ ] Code builds without errors
- [ ] Follows code style guidelines
- [ ] Updated documentation
- [ ] Tested in-editor and at runtime
```

## Community

- **Discussions**: Use GitHub Discussions for questions and ideas
- **Issues**: Use GitHub Issues for bugs and feature requests
- **Discord**: (If you create one, link it here)

## License

By contributing, you agree that your contributions will be licensed under the MIT License.

## Questions?

Open an issue or start a discussion on GitHub!

---

**Thank you for contributing to VCET! ??**
