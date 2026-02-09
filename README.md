# Asymmetric Camera — UE5 Off-Axis Projection Plugin

**[中文文档](README_CN.md)**

An off-axis / asymmetric frustum projection plugin for **Unreal Engine 5.4**, designed for CAVE systems, projection mapping, multi-display setups, and head-tracked displays.

## Features

- **Zero-overhead projection override** — directly overrides the player camera projection matrix via `ISceneViewExtension`, no Render Target or SceneCapture needed
- **Interactive editor visualization** — drag to adjust screen position, orientation, and size with real-time frustum preview
- **Granular debug toggles** — screen outline, frustum lines, eye handle, near plane, and labels can be toggled independently
- **Stereoscopic rendering** — built-in eye separation for stereo left/right eye output
- **Blueprint support** — all parameters exposed to Blueprint
- **External data input** — supports importing calibration data from Max/Maya and referencing scene Actor transforms

## Quick Start

1. Place an `AsymmetricCameraActor` in your level
2. Select the Actor and adjust the **Screen** component's Transform and `ScreenWidth` / `ScreenHeight` in the Details panel
3. Press Play — the player camera projection is automatically overridden
4. *(Optional)* Set `Tracked Actor` for head tracking

## Component Hierarchy

```text
AAsymmetricCameraActor
└── Root (SceneComponent)
    ├── Screen (AsymmetricScreenComponent — projection screen plane)
    └── Camera (AsymmetricCameraComponent — eye position + projection calculation)
```

## Parameters

### Camera Component (`AsymmetricCameraComponent`)

| Parameter | Description |
| --------- | ----------- |
| `bUseAsymmetricProjection` | Enable / disable off-axis projection |
| `NearClip` | Near clipping plane distance (default 20) |
| `FarClip` | Far clipping plane distance (0 = infinite) |
| `EyeSeparation` | Inter-ocular distance for stereo rendering (0 = mono) |
| `EyeOffset` | Left eye −1 / Center 0 / Right eye 1 |
| `bMatchViewportAspectRatio` | Auto-match screen aspect ratio to prevent stretching |
| `TrackedActor` | Actor whose position is used as the eye position |
| `ScreenComponent` | Reference to the screen component |

### Screen Component (`AsymmetricScreenComponent`)

| Parameter | Description |
| --------- | ----------- |
| `ScreenWidth` | Screen width in world units (default 160) |
| `ScreenHeight` | Screen height in world units (default 90) |

The screen plane lies on the component's local YZ plane with the normal along +X. Use the component's Transform to control position and orientation.

### Debug Visualization

| Parameter | Description |
| --------- | ----------- |
| `bShowDebugFrustum` | Master toggle for editor debug visualization |
| `bShowScreenOutline` | Screen border + diagonal + normal arrow (green) |
| `bShowFrustumLines` | Lines from eye to screen corners (yellow) |
| `bShowEyeHandle` | Eye position sphere (yellow) |
| `bShowNearPlane` | Near clipping plane rectangle (red) |
| `bShowLabels` | Corner labels and screen info text |
| `bShowDebugInGame` | Show debug lines during gameplay |

## How It Works

Implements Robert Kooima's **Generalized Perspective Projection** algorithm:

1. On `BeginPlay`, `UAsymmetricCameraComponent` registers an `FAsymmetricViewExtension` (`FWorldSceneViewExtension`)
2. Each frame, UE5 calls `SetupViewProjectionMatrix()` on the extension
3. The extension computes the off-axis projection matrix (UE5 reversed-Z format) and screen-aligned view rotation matrix
4. Both are written into `FSceneViewProjectionData`, directly overriding the player camera — no RT, no SceneCapture, no extra rendering pass

## Build

```bash
# Quick build (auto-detects UE5 path)
QuickBuild.bat
```

## References

- [Robert Kooima: Generalized Perspective Projection](http://csc.lsu.edu/~kooima/articles/genperspective/)
- [GitHub: fweidner/UE4-Plugin-OffAxis](https://github.com/fweidner/UE4-Plugin-OffAxis)

## License

Apache License 2.0
