# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Language

Always respond in Chinese (中文). Code comments remain in English.

## Project Overview

MyCustomCam is an Unreal Engine 5.4 project focused on custom camera development. Includes the **Asymmetric Camera Plugin** for off-axis/asymmetric frustum projection (similar to Unity's off-center projection), useful for VR, CAVE systems, and projection mapping.

## Project Structure

- **Source/MyCustomCam/**: Core game module with minimal boilerplate
  - `MyCustomCam.Build.cs`: Module dependencies (Core, CoreUObject, Engine, InputCore, EnhancedInput)
  - `MyCustomCam.h/.cpp`: Primary game module implementation
- **Source/MyCustomCam.Target.cs**: Game build target configuration
- **Source/MyCustomCamEditor.Target.cs**: Editor build target configuration
- **Config/**: Engine and project configuration files
  - `DefaultEngine.ini`: Rendering (Lumen, Virtual Shadow Maps, DX12/SM6), audio, and project settings
  - `DefaultInput.ini`: Enhanced Input system configuration
  - `DefaultGame.ini`: Game-specific settings (default map: `/Game/NewMap`)
- **Content/**: Content assets (currently contains NewMap.umap)
- **Plugins/AsymmetricCamera/**: Custom camera plugin for asymmetric frustum projection
  - Implements off-axis projection based on Robert Kooima's algorithm
  - Includes editor visualizer for frustum debugging
  - See `Plugins/AsymmetricCamera/README.md` for full documentation

## Build System

**Quick build using batch script (recommended):**
```bash
# Quick build (auto-detects UE5 path)
QuickBuild.bat

# Or use the full build manager
BuildPlugin.bat
```

**Build the project manually:**
```bash
# From Visual Studio (recommended for C++ development)
# Open MyCustomCam.sln and build from IDE

# Or use Unreal Build Tool directly
"%UE5_ROOT%\Engine\Build\BatchFiles\Build.bat" MyCustomCamEditor Win64 Development -Project="%CD%\MyCustomCam.uproject"
```

**Generate project files (after modifying .Build.cs or adding new modules):**
```bash
"%UE5_ROOT%\Engine\Build\BatchFiles\GenerateProjectFiles.bat" -project="%CD%\MyCustomCam.uproject" -game -engine
```

**Hot reload:**
- Use Ctrl+Alt+F11 in Unreal Editor for live C++ compilation
- Or compile from Visual Studio while editor is open

## Running the Project

**Launch editor:**
```bash
"%UE5_ROOT%\Engine\Binaries\Win64\UnrealEditor.exe" "%CD%\MyCustomCam.uproject"
```

**Package for shipping:**
```bash
"%UE5_ROOT%\Engine\Build\BatchFiles\RunUAT.bat" BuildCookRun -project="%CD%\MyCustomCam.uproject" -platform=Win64 -configuration=Shipping -cook -stage -package
```

## Key Technical Details

**Unreal Engine Version:** 5.4
- Using BuildSettingsVersion.V5
- EngineIncludeOrderVersion.Unreal5_4
- PCHUsageMode: UseExplicitOrSharedPCHs

**Rendering Configuration:**
- Default RHI: DirectX 12
- Shader Model: SM6 (PCD3D_SM6)
- Lumen enabled (r.DynamicGlobalIlluminationMethod=1, r.ReflectionMethod=1)
- Virtual Shadow Maps enabled (r.Shadow.Virtual.Enable=1)
- Mesh distance fields generated (r.GenerateMeshDistanceFields=True)
- Hardware targeting: Desktop Maximum performance

**Input System:**
- Using Enhanced Input system (DefaultPlayerInputClass and DefaultInputComponentClass)
- To add input mappings, create Input Mapping Contexts and Input Actions in the Content Browser
- Bind inputs in C++ using UEnhancedInputComponent

**Enabled Plugins:**
- ModelingToolsEditorMode (Editor-only)
- **AsymmetricCamera** (Runtime + Editor) - Custom off-axis projection camera

## Adding New C++ Classes

When adding new gameplay classes:
1. Add the class files to `Source/MyCustomCam/`
2. If adding new module dependencies, update `MyCustomCam.Build.cs` PublicDependencyModuleNames or PrivateDependencyModuleNames
3. Regenerate project files if module structure changes
4. Common modules to add:
   - "Slate", "SlateCore" for UI
   - "OnlineSubsystem" for networking features
   - "AIModule" for AI functionality
   - "UMG" for widget blueprints

## Module Dependencies

Current dependencies:
- **Public:** Core, CoreUObject, Engine, InputCore, EnhancedInput
- **Private:** None

The module is configured with standard dependency setup for a basic game project.

## Asymmetric Camera Plugin

### Overview
The project includes a custom plugin that implements off-axis/asymmetric frustum projection, similar to Unity's off-center projection feature. This is useful for:
- VR and CAVE systems
- Projection mapping
- Multi-display setups
- Head-tracked displays

### Key Features
- **Zero-overhead Projection Override**: Directly overrides the player camera's projection matrix via `ISceneViewExtension::SetupViewProjectionMatrix` — no Render Target or SceneCapture needed
- **Editor Visualization**: Real-time frustum visualization in editor viewport (enabled by default)
  - Green lines: Screen boundary
  - Yellow lines: Frustum from camera to screen
  - Red arrow: Screen normal direction
  - Orange lines: Near clipping plane
- **Flexible Screen Definition**: Define projection screens using 3D corner points or plane parameters
- **Stereoscopic Support**: Built-in eye separation for stereo rendering
- **Blueprint Integration**: Full Blueprint support with easy-to-use nodes
- **Real-time Tracking**: Can track actor positions for dynamic projection updates

### Quick Usage
1. Add `AsymmetricCameraActor` to your level
2. Frustum visualization is shown by default (`bShowDebugFrustum = true`)
3. Configure `Projection Screen` corners in Details panel
4. Set `Tracked Actor` on the AsymmetricCamera component for head tracking (optional)
5. Press Play — the player camera's projection is automatically overridden

### Files and Documentation
- **Plugin Source**: `Plugins/AsymmetricCamera/`
- **Documentation**:
  - `README.md` - English documentation
  - `使用指南.md` - Chinese user guide
  - `QUICKSTART.md` - Quick start guide
- **Build Scripts**:
  - `BuildPlugin.bat` - Full build management menu
  - `QuickBuild.bat` - Quick compile script

### Architecture
The plugin consists of two modules:
- **AsymmetricCamera** (Runtime): Core projection calculation and components
  - `UAsymmetricCameraComponent`: Calculates off-axis projection matrices, manages `ISceneViewExtension`
  - `AAsymmetricCameraActor`: Minimal actor wrapping the component for easy level placement
  - `FAsymmetricViewExtension`: Overrides player camera projection via `SetupViewProjectionMatrix`
- **AsymmetricCameraEditor** (Editor): Editor visualization
  - `FAsymmetricCameraComponentVisualizer`: Draws frustum in viewport

### How It Works (High-Performance Path)

1. On `BeginPlay`, `UAsymmetricCameraComponent` registers an `FAsymmetricViewExtension` (inherits `FWorldSceneViewExtension`)
2. Each frame, UE5 calls `SetupViewProjectionMatrix(FSceneViewProjectionData&)` on the extension
3. The extension computes the off-axis projection matrix (Kooima algorithm, UE5 reversed-Z format) and view rotation matrix (screen-aligned)
4. Both are written into `FSceneViewProjectionData`, directly overriding the player camera — no RT, no SceneCapture, no extra rendering pass

### Algorithm
Implements Robert Kooima's "Generalized Perspective Projection" algorithm, which creates a custom projection matrix based on:

1. 3D screen plane definition (corner points)
2. Eye/camera position in world space
3. Near and far clipping planes

The projection matrix is produced in **UE5 reversed-Z format** (row-vector convention, `M[2][3]=1`, near maps to Z=1, far maps to Z=0). The view rotation matrix aligns world space to screen-aligned rendering coordinates (X=right, Y=up, Z=forward into screen).

### References
- [Robert Kooima: Generalized Perspective Projection](http://csc.lsu.edu/~kooima/articles/genperspective/)
- [GitHub: fweidner/UE4-Plugin-OffAxis](https://github.com/fweidner/UE4-Plugin-OffAxis)
- [Medium: Off-axis projection in Unity](https://medium.com/try-creative-tech/off-axis-projection-in-unity-1572d826541e)
