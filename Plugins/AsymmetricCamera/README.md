# Asymmetric Camera Plugin for Unreal Engine 5.4

An off-axis / asymmetric frustum camera plugin for Unreal Engine 5.4, useful for VR, CAVE systems, projection mapping, and advanced rendering scenarios.

## Features

- **Off-Axis Projection**: Implements generalized perspective projection based on Robert Kooima's algorithm
- **Flexible Screen Definition**: Define projection screens using 3D corner points or plane parameters
- **Stereoscopic Support**: Built-in eye separation for stereo rendering
- **Real-time Tracking**: Can track actor positions (e.g., player camera) for dynamic projection updates
- **Debug Visualization**: Visual debugging tools to see the projection frustum and screen boundaries
- **Blueprint Friendly**: Full Blueprint support with easy-to-use nodes

## Installation

1. Copy the `AsymmetricCamera` folder to your project's `Plugins` directory
2. If the `Plugins` directory doesn't exist, create it in your project root
3. Regenerate project files (right-click .uproject → Generate Visual Studio project files)
4. Compile the project
5. Enable the plugin in Edit → Plugins → Rendering → Asymmetric Camera

## Quick Start

### Method 1: Using AsymmetricCameraActor

1. Drag `AsymmetricCameraActor` into your level
2. Configure the projection screen:
   - Set `Projection Screen` → `Bottom Left`, `Bottom Right`, `Top Left` to define your screen corners
   - Or use `Set Screen From Plane` function to define by center, size, and normal
3. Set `Tracked Actor` to your player camera or pawn for head tracking
4. The actor will render to its `Render Target`, which you can display on a material

### Method 2: Using AsymmetricCameraComponent

Add `AsymmetricCameraComponent` to any actor and use `Calculate Off Axis Projection` to get custom projection matrices.

## Configuration

### Projection Screen Setup

The projection screen is defined by three corner points:
- **Bottom Left**: Lower-left corner of the screen
- **Bottom Right**: Lower-right corner of the screen
- **Top Left**: Upper-left corner of the screen
- The top-right corner is calculated automatically

**Coordinate Modes:**
- **Relative Coordinates** (`bUseRelativeScreenCoordinates = true`): Screen corners are relative to the component
- **World Coordinates** (`bUseRelativeScreenCoordinates = false`): Screen corners are in world space

### Example: 2m × 1.5m Screen

```cpp
// In local space (100cm forward, 2m wide, 1.5m tall)
BottomLeft = (100, -100, -75)
BottomRight = (100, 100, -75)
TopLeft = (100, -100, 75)
```

Or in Blueprint:
```
Set Screen From Plane
  Center: (100, 0, 0)
  Width: 200
  Height: 150
  Normal: (1, 0, 0)
```

### Stereoscopic Rendering

For stereo/VR applications:
- Set `Eye Separation` to the interpupillary distance (typically 6.4 cm = 6.4 units)
- Set `Eye Offset` to -1 for left eye, +1 for right eye, 0 for center

## Use Cases

### CAVE / Powerwall Systems
Configure multiple `AsymmetricCameraActor` instances, one for each projection surface, with head tracking to create an immersive virtual environment.

### Projection Mapping
Define projection screens that match physical surfaces for architectural visualization or art installations.

### Off-Center Displays
Create asymmetric projections for multi-monitor setups or unusual display configurations.

### Fishtank VR
Implement desktop VR with head tracking for a 3D "window" effect on a standard monitor.

## API Reference

### UAsymmetricCameraComponent

**Main Functions:**
- `CalculateOffAxisProjection(EyePosition, OutProjectionMatrix, OutViewMatrix)`: Calculate projection matrices
- `SetScreenCorners(BottomLeft, BottomRight, TopLeft)`: Set screen definition
- `SetScreenFromPlane(Center, Width, Height, Normal)`: Set screen from plane parameters
- `GetScreenCorners(...)`: Get current screen corner positions

**Properties:**
- `bUseAsymmetricProjection`: Enable/disable off-axis projection
- `ProjectionScreen`: Screen definition (corners, near/far clip)
- `EyeSeparation`: Distance between eyes for stereo rendering
- `bShowDebugFrustum`: Show debug visualization

### AAsymmetricCameraActor

**Properties:**
- `RenderTarget`: Output texture
- `TrackedActor`: Actor to track for eye position
- `bUpdateProjectionEveryFrame`: Continuous updates
- `bAutoCreateRenderTarget`: Auto-create render target on begin play

**Functions:**
- `UpdateProjection()`: Manually update projection
- `SetRenderTarget(NewRenderTarget)`: Change output render target

## Technical Details

### Algorithm

This plugin implements Robert Kooima's "Generalized Perspective Projection" algorithm, which:

1. Defines a projection screen as a plane in 3D space
2. Calculates an orthonormal basis for the screen
3. Projects screen corners relative to the eye position
4. Constructs an asymmetric frustum projection matrix

The algorithm ensures that rendered perspective matches the physical relationship between the viewer's eyes and the display surface.

### Performance

- Projection matrix calculation is lightweight (suitable for real-time head tracking)
- Uses standard UE5 SceneCaptureComponent2D for rendering
- Debug visualization has minimal overhead when disabled

## References

- Robert Kooima: "Generalized Perspective Projection"
- [GitHub: fweidner/UE4-Plugin-OffAxis](https://github.com/fweidner/UE4-Plugin-OffAxis)
- [Medium: Off-axis projection in Unity](https://medium.com/try-creative-tech/off-axis-projection-in-unity-1572d826541e)

## License

Copyright Epic Games, Inc. All Rights Reserved.

## Troubleshooting

**Q: The projection looks wrong**
- Verify screen corner positions are correct (enable `bShowDebugFrustum`)
- Check that near/far clip planes are appropriate
- Ensure tracked actor position is in front of the screen

**Q: Performance issues**
- Reduce render target resolution
- Disable `bUpdateProjectionEveryFrame` if eye position is static
- Check scene capture settings (show flags, capture sources)

**Q: Screen not visible in debug mode**
- Check that the component is enabled
- Verify world/relative coordinate mode matches your setup
- Ensure actor is visible in viewport
