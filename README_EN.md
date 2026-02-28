# Asymmetric Camera — UE5 Off-Axis Projection Plugin

**[中文文档](README.md)**

An off-axis / asymmetric frustum projection plugin for **Unreal Engine 5.4**, designed for CAVE systems, projection mapping, multi-display setups, and head-tracked displays.

## Features

- **Zero-overhead projection override** — directly overrides the player camera projection matrix via `ISceneViewExtension`, no Render Target or SceneCapture needed
- **Interactive editor visualization** — drag to adjust screen position, orientation, and size with real-time frustum preview
- **Granular debug toggles** — screen outline, frustum lines, eye handle, near plane, and labels can be toggled independently
- **Stereoscopic rendering** — built-in eye separation for stereo left/right eye output
- **MRQ stereo rendering** — custom render pass that renders both eyes per frame and auto-composites SBS/TB image sequences or video (bundled FFmpeg)
- **Blueprint support** — all parameters exposed to Blueprint
- **External data input** — supports importing calibration data from Max/Maya and referencing scene Actor transforms
- **MRQ (Movie Render Queue) support** — asymmetric projection works with offline rendering; can follow a CineCameraActor driven by Sequencer; motion blur is fully supported with per-eye transform tracking

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
| `StereoLayout` | Stereo layout mode: Mono / Side by Side / Top Bottom |
| `bMatchViewportAspectRatio` | Auto-match screen aspect ratio to prevent stretching |
| `bEnableMRQSupport` | Apply asymmetric projection during MRQ offline rendering |
| `TrackedActor` | Actor whose position is used as the eye position |
| `bFollowTargetCamera` | Sync owner actor Transform to a target camera each frame |
| `TargetCamera` | Target camera actor to follow (typically CineCameraActor) |
| `ScreenComponent` | Reference to the screen component (auto-detected on same actor) |

### External Data Input

Enable `bUseExternalData` to bypass the Screen component and define the projection screen via world-space coordinates or Actor references (e.g. calibration data from Max/Maya).

| Parameter | Description |
| --------- | ----------- |
| `bUseExternalData` | Use external data instead of ScreenComponent |
| `ExternalEyeActor` | Actor reference for eye position (overrides `ExternalEyePosition`) |
| `ExternalEyePosition` | Eye position in world space |
| `ExternalScreenBLActor` | Actor reference for screen bottom-left corner |
| `ExternalScreenBL` | Screen bottom-left corner in world space |
| `ExternalScreenBRActor` | Actor reference for screen bottom-right corner |
| `ExternalScreenBR` | Screen bottom-right corner in world space |
| `ExternalScreenTLActor` | Actor reference for screen top-left corner |
| `ExternalScreenTL` | Screen top-left corner in world space |
| `ExternalScreenTRActor` | Actor reference for screen top-right corner |
| `ExternalScreenTR` | Screen top-right corner in world space |

> Priority: Actor reference > FVector coordinate > TrackedActor/ScreenComponent fallback.

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
| `bShowStereoFrustums` | Show both left (cyan) and right (magenta) eye frustums simultaneously to verify stereo parallax (requires EyeSeparation > 0) |
| `bShowDebugInGame` | Show debug lines during gameplay |

## Blueprint API

### AsymmetricCameraComponent — Blueprint Functions

| Function | Description |
| -------- | ----------- |
| `GetEyePosition()` | Returns the effective eye world position (priority: ExternalEyeActor > ExternalEyePosition > TrackedActor > component location) |
| `GetEffectiveScreenCorners()` | Returns the four screen corners currently in use (external data or ScreenComponent) |
| `SetExternalData(Eye, BL, BR, TL, TR)` | Set all external data points at once |
| `CalculateOffAxisProjection()` | Manually compute the off-axis projection matrix and view rotation |

### AsymmetricScreenComponent — Blueprint Functions

| Function | Description |
| -------- | ----------- |
| `GetScreenSize()` | Returns screen size as `FVector2D(ScreenWidth, ScreenHeight)` |
| `SetScreenSize(NewSize)` | Sets screen size in world units |
| `GetScreenCornersWorld(BL, BR, TL, TR)` | Returns the four screen corners in world space (bottom-left, bottom-right, top-left, top-right) |
| `GetScreenCornersLocal(BL, BR, TL, TR)` | Returns the four screen corners in the owner Actor's local space |

## How It Works

Implements Robert Kooima's **Generalized Perspective Projection** algorithm:

1. On `BeginPlay`, `UAsymmetricCameraComponent` registers an `FAsymmetricViewExtension` (`FWorldSceneViewExtension`)
2. Each frame, UE5 calls `SetupViewProjectionMatrix()` on the extension — used during gameplay
3. The extension computes the off-axis projection matrix (UE5 reversed-Z format) and screen-aligned view rotation matrix
4. Both are written into `FSceneViewProjectionData`, directly overriding the player camera — no RT, no SceneCapture, no extra rendering pass
5. For MRQ offline rendering, the extension overrides the projection in `SetupView()` (since MRQ does not call `SetupViewProjectionMatrix`); this path uses `InView.ViewLocation` as the eye position so that eye offsets already applied by `MoviePipelineAsymmetricStereoPass` are correctly respected

**Motion blur:** Each eye maintains its own previous-frame eye position and view rotation, preventing the two eyes from contaminating each other's velocity buffer during stereo rendering.

## MRQ Rendering Workflow

1. Place an `AsymmetricCameraActor` in the level and configure the screen
2. Place a `CineCameraActor` and animate it with Sequencer
3. On the AsymmetricCamera component, enable **Follow Target Camera** and set **Target Camera** to the CineCameraActor
4. Enable **Enable MRQ Support** (on by default)
5. Render with Movie Render Queue — the output uses asymmetric projection while the animation is driven by the CineCameraActor

> **Note:**
>
> - Motion blur is fully supported — the plugin tracks previous-frame camera transforms independently per eye to ensure correct velocity buffer calculation.
> - MRQ high-resolution tiling is not compatible with asymmetric projection. Set tiling to 1×1.

## MRQ Motion Blur Setup

Motion blur requires three things to work together:

### 1. MRQ Anti-Aliasing Settings

Add an **Anti-Aliasing** setting to the MRQ Job:

| Parameter | Recommended | Description |
| --------- | ----------- | ----------- |
| `Temporal Sample Count` | 8 – 32 | Number of temporal samples; higher = smoother motion blur |
| `Spatial Sample Count` | 1 | Keep at 1 in most cases |
| `Override Anti Aliasing` | Checked | Force override the renderer's AA setting |
| `Anti Aliasing Method` | `None` | MRQ accumulates frames itself; disable engine AA to avoid double-blurring |

### 2. Camera / Post Process Motion Blur

Enable motion blur on the `CineCameraActor` or a Post Process Volume in the scene:

| Parameter | Recommended | Description |
| --------- | ----------- | ----------- |
| Motion Blur → **Amount** | 0.5 (default) | Blur strength; 0 = disabled |
| Motion Blur → **Max** | 5.0 (default) | Maximum blur distance (% of screen) |
| Motion Blur → **Target FPS** | 0 | 0 = automatically derived from output frame rate |

> If Amount is 0 or Motion Blur is disabled in the post process settings, MRQ temporal samples will not produce any motion blur.

### 3. Plugin Side (No Extra Configuration Needed)

The plugin automatically writes `InView.PreviousViewTransform` in `SetupView()` each frame, providing the correct previous-frame camera transform for the velocity buffer. No additional plugin parameters need to be set.

> **Note:** The **first frame** of a sequence will have no motion blur (no previous frame data is available). This is an inherent limitation of offline rendering.

## MRQ Stereo Rendering

The plugin provides an `Asymmetric Stereo Pass` for rendering stereo video (Side-by-Side or Top-Bottom) via MRQ.

### Setup

1. In the MRQ Job's **Render Pass** section, remove the default "Deferred Rendering" and add **Asymmetric Stereo Pass**
2. Configure the pass parameters (see table below)
3. Include `{camera_name}` in the output filename template (auto-filled with LeftEye / RightEye)
4. After rendering, the plugin automatically composites SBS/TB output according to `CompositeMode` — image sequence or video (default: `Image Sequence`)

**Pass parameters:**

| Parameter | Description |
| --------- | ----------- |
| `StereoLayout` | Side by Side (LR) or Top / Bottom (TB) |
| `EyeSeparation` | Inter-ocular distance in centimeters (default 6.4) |
| `bSwapEyes` | Swap left and right eye output |
| `CompositeMode` | `Disabled` (keep separate sequences) / `Image Sequence` (one merged image per frame, **default**) / `Video` (merged video file) |
| `FFmpegPath` | Path to FFmpeg executable; leave empty to use the bundled one (file picker supported) |
| `VideoCodec` | Video encoder: H.264 / H.265 (`Video` mode only) |
| `CompositeQuality` | CRF quality value: 0=lossless, 18=recommended, 51=worst (`Video` mode only) |
| `OutputFormat` | Output format: MP4 / MOV / MKV / AVI; H.265 forces MKV (`Video` mode only) |
| `bDeleteSourceAfterComposite` | Auto-delete left/right eye source sequences after successful composite (default: on) |

> **Stereo 3D metadata (`Video` mode):**
>
> - H.264 output embeds a Frame Packing Arrangement SEI in the bitstream (type 3=SBS / type 4=TB), recognized automatically by VLC, PotPlayer, etc.
> - H.265 output forces MKV container and uses `stereo_mode` container metadata (x265 has no frame-packing CLI parameter).

### Composite Output Naming

| Mode | Example output filename |
| ---- | ----------------------- |
| `Image Sequence` | `stereo_SBS.0000.jpeg`, `stereo_TB.0000.png` |
| `Video` (SBS) | `stereo_SBS.mp4` |
| `Video` (TB) | `stereo_TB.mkv` (H.265) |

### FFmpeg

The plugin bundles ffmpeg.exe under `ThirdParty/FFmpeg/Win64/` (managed via Git LFS). You can also set `FFmpegPath` to point to an FFmpeg installation on your system.

## Git LFS

This project uses Git LFS to manage large binary files (e.g. the bundled ffmpeg.exe). Make sure Git LFS is installed before cloning:

```bash
git lfs install
git clone <repo-url>
```

If you've already cloned without LFS:

```bash
git lfs pull
```

## Build

```bash
# Quick build (auto-detects UE5 path)
QuickBuild.bat

# If QuickBuild fails, open the project directly in Unreal Editor
# and compile from there (Ctrl+Alt+F11)
start "" "MyCustomCam.uproject"
```

## References

- [Robert Kooima: Generalized Perspective Projection](http://csc.lsu.edu/~kooima/articles/genperspective/)

## License

Apache License 2.0
