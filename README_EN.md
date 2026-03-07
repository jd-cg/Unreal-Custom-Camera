# Asymmetric Camera — UE5 Off-Axis Projection Plugin

**[中文文档](README.md)**

An off-axis / asymmetric frustum projection plugin for **Unreal Engine 5.4 / 5.5 / 5.6**, designed for CAVE systems, projection mapping, multi-display setups, and head-tracked displays.

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

Motion blur requires three things to work together.

### 1. MRQ Anti-Aliasing Settings

Add an **Anti-Aliasing** setting to the MRQ Job:

| Parameter | Default | Recommended | Description |
| --------- | ------- | ----------- | ----------- |
| `Temporal Sample Count` | 1 | 8 – 32 | **Must be > 1 to produce motion blur**; higher = smoother |
| `Spatial Sample Count` | 1 | 1 | Keep at default |
| `Override Anti Aliasing` | Unchecked | Checked | Force override the renderer's AA setting |
| `Anti Aliasing Method` | — | `None` | MRQ accumulates frames itself; disable engine AA to avoid double-blurring |

### 1b. MRQ Console Variables Setting

Add a **Console Variables** setting to the MRQ Job and include the following:

| Console Variable | Value | Description |
| ---------------- | ----- | ----------- |
| `r.MotionBlurQuality` | `4` | Motion blur render quality: 0 = off, 4 = maximum. Some MRQ configurations reset this to 0 at runtime — set it explicitly to ensure motion blur is active |

> Without this variable, motion blur may not appear in MRQ renders at all, even if the Post Process Volume and temporal sample count are correctly configured.

### 2. Post Process Volume Motion Blur

Because AsymmetricCamera overrides the camera projection via `ISceneViewExtension`, motion blur should be configured through a **Post Process Volume in the scene** rather than on the CineCameraActor — a PPV applies reliably to any camera view.

**Steps:**

1. Place a **Post Process Volume** in the scene (Place Actors → Volumes → Post Process Volume)
2. In Details, check **Infinite Extent (Unbound)** to cover the whole scene
3. Expand **Rendering Features** → **Motion Blur**
4. **Check the checkbox next to each parameter** before setting its value (unchecked = no override, the value is ignored)

| Parameter | Default | Recommended | Description |
| --------- | ------- | ----------- | ----------- |
| Motion Blur → **Amount** ✅ | 0.5 | 0.5 | Blur strength; **must be > 0** |
| Motion Blur → **Max** ✅ | 5.0 | 5.0 | Maximum blur distance (% of screen) |
| Motion Blur → **Target FPS** ✅ | 0 | 0 | 0 = derived automatically from output frame rate |

> If the Amount checkbox is unchecked or its value is 0, MRQ temporal samples will not produce any motion blur.

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
| `FFmpegPath` | Path to FFmpeg executable. Click `...` to browse, or type an absolute path (e.g. `D:/tools/ffmpeg/bin/ffmpeg.exe`). Leave empty to use `ffmpeg` from the system PATH |
| `VideoCodec` | Video encoder: H.264 / H.265 (`Video` mode only) |
| `CompositeQuality` | CRF quality value: 0=lossless, 18=recommended, 51=worst (`Video` mode only) |
| `OutputFormat` | Output format: MP4 / MOV / MKV / AVI; H.265 forces MKV (`Video` mode only) |
| `bDeleteSourceAfterComposite` | Auto-delete left/right eye source sequences after successful composite (default: on) |
| `bDebugSaveConcatFiles` | Debug mode: keep concat list files and FFmpeg log files (`_concat_*.txt` / `_ffmpeg_log_*.txt`) on disk. Default off; enable when diagnosing composite failures |

> **Stereo 3D metadata (`Video` mode):**
>
> - H.264 output embeds a Frame Packing Arrangement SEI in the bitstream (type 3=SBS / type 4=TB), recognized automatically by VLC, PotPlayer, etc.
> - H.265 output forces MKV container and uses `stereo_mode` container metadata (x265 has no frame-packing CLI parameter).

### Composite Output Naming

Output filenames include the layout mode and the shot name (taken from the Sequencer Shot Section name, e.g. `shot0000`):

| Mode | Example output filename |
| ---- | ----------------------- |
| `Image Sequence` | `stereo_SBS_shot0000_%05d.jpeg`, `stereo_TB_shot0001_%05d.png` |
| `Video` (SBS) | `stereo_SBS_shot0000.mp4` |
| `Video` (TB) | `stereo_TB_shot0000.mkv` (H.265) |

Multiple shots each produce their own composite output, processed serially in order.

### Composite Mechanism

Compositing uses FFmpeg's **concat demuxer** — a temporary list file explicitly names every input frame instead of relying on a frame-number pattern (`%04d`). This means:

- **Frame number agnostic** — Handle Frames, mid-sequence shots, or any starting frame number all work correctly
- **Filename format agnostic** — no requirement on separator characters or zero-padding width
- **Gap-safe** — partial re-renders or non-contiguous frames do not break compositing
- **Full multi-shot coverage** — every shot is composited independently; no shots are silently skipped
- **Exact frame rate** — passed as a `Numerator/Denominator` fraction (e.g. `24000/1001` for 23.976 fps)

The only requirement: the MRQ output filename template must include `{camera_name}` so that LeftEye and RightEye files can be distinguished.

### FFmpeg

Set `FFmpegPath` to the absolute path of your FFmpeg executable (e.g. `D:/tools/ffmpeg/bin/ffmpeg.exe`), or use the `...` file picker. Leave empty to fall back to `ffmpeg` on the system PATH (requires FFmpeg to be installed and in PATH).

> **Tip:** Pre-built Windows binaries are available at [ffmpeg.org](https://ffmpeg.org/download.html) or [gyan.dev](https://www.gyan.dev/ffmpeg/builds/).

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
# Quick build (auto-detects UE5 path, supports 5.4 / 5.5 / 5.6)
QuickBuild.bat

# If QuickBuild cannot find your engine installation, open the script
# in a text editor and set UE5_PATH manually:
#   set "UE5_PATH=C:\Your\Path\To\UE_5.X"

# Alternatively, open the project in Unreal Editor and compile there (Ctrl+Alt+F11)
start "" "MyCustomCam.uproject"
```

## Development Environment (VSCode IntelliSense)

The project is configured for clangd to provide accurate C++ completion and navigation.

**Prerequisite:** Install the VSCode [clangd extension](https://marketplace.visualstudio.com/items?itemName=llvm-vs-code-extensions.vscode-clangd) and **disable** the Microsoft C/C++ extension (the two conflict).

**Generate the compile database (run once, and again after modifying Build.cs):**

```bash
GenerateCompileCommands.bat
```

The script calls UBT in `GenerateClangDatabase` mode and copies the resulting `compile_commands.json` to the project root. Afterwards, run `clangd: Restart language server` in VSCode to apply.

> `compile_commands.json` is listed in `.gitignore` and is not committed. Each developer machine must generate it locally.

## References

- [Robert Kooima: Generalized Perspective Projection](http://csc.lsu.edu/~kooima/articles/genperspective/)

## License

Apache License 2.0
