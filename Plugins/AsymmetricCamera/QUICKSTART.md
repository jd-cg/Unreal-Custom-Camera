# Quick Start Guide - 快速开始

## Installation Steps / 安装步骤

### 1. Plugin is Already Created / 插件已创建
The plugin is located at:
```
D:\Ue\ProjWorkspace\MyCustomCam\Plugins\AsymmetricCamera\
```

### 2. Generate Project Files / 生成项目文件

**Option A: Using Windows Context Menu (推荐)**
- Right-click on `MyCustomCam.uproject`
- Select "Generate Visual Studio project files"

**Option B: Manual Command / 手动命令**
```bash
"<UE5_PATH>\Engine\Build\BatchFiles\GenerateProjectFiles.bat" -project="D:\Ue\ProjWorkspace\MyCustomCam\MyCustomCam.uproject" -game -engine
```

Replace `<UE5_PATH>` with your Unreal Engine 5.4 installation path.
例如: `C:\Program Files\Epic Games\UE_5.4`

### 3. Compile the Project / 编译项目

**Option A: Visual Studio**
1. Open `MyCustomCam.sln`
2. Set build configuration to "Development Editor"
3. Build the solution (F7)

**Option B: Unreal Editor (Hot Reload)**
1. Open the project in Unreal Editor
2. The editor will detect new C++ code and prompt to compile
3. Click "Yes" to compile

### 4. Enable the Plugin / 启用插件

1. Open the project in Unreal Editor
2. Go to **Edit → Plugins**
3. Search for "Asymmetric Camera"
4. Check the box to enable it
5. Restart the editor when prompted

## Quick Test / 快速测试

### Test the Visualizer / 测试可视化

1. In the Level Editor, go to **Window → Place Actors**
2. Search for "AsymmetricCameraActor"
3. Drag it into your level
4. Select the actor
5. You should see:
   - **Green lines**: Screen boundary / 投影屏幕边界
   - **Yellow lines**: Frustum from camera to screen / 视锥体
   - **Red arrow**: Screen normal / 屏幕法线

### Enable Debug View / 启用调试视图

1. Select the AsymmetricCameraActor
2. In the Details panel, find:
   ```
   Asymmetric Camera → bShowDebugFrustum
   ```
3. Check the box
4. Play the level (Alt+P)
5. You'll see debug visualization in game view

## File Structure / 文件结构

```
Plugins/AsymmetricCamera/
├── AsymmetricCamera.uplugin          # Plugin descriptor
├── README.md                         # English documentation
├── 使用指南.md                        # Chinese documentation
├── Resources/                        # Plugin resources
└── Source/
    ├── AsymmetricCamera/             # Runtime module
    │   ├── Public/
    │   │   ├── AsymmetricCameraModule.h
    │   │   ├── AsymmetricCameraComponent.h
    │   │   └── AsymmetricCameraActor.h
    │   ├── Private/
    │   │   ├── AsymmetricCameraModule.cpp
    │   │   ├── AsymmetricCameraComponent.cpp
    │   │   └── AsymmetricCameraActor.cpp
    │   └── AsymmetricCamera.Build.cs
    └── AsymmetricCameraEditor/       # Editor module
        ├── Public/
        │   ├── AsymmetricCameraEditorModule.h
        │   └── AsymmetricCameraComponentVisualizer.h
        ├── Private/
        │   ├── AsymmetricCameraEditorModule.cpp
        │   └── AsymmetricCameraComponentVisualizer.cpp
        └── AsymmetricCameraEditor.Build.cs
```

## Troubleshooting / 故障排除

### Compilation Errors / 编译错误

**Missing UE5 Headers:**
- Make sure you have UE 5.4 installed
- Verify the engine path in `MyCustomCam.uproject`

**Plugin Not Found:**
- Check that the plugin is in `Plugins/AsymmetricCamera/`
- Regenerate project files

### Editor Issues / 编辑器问题

**Plugin Won't Enable:**
- Check the Output Log for errors
- Try cleaning: Delete `Binaries/`, `Intermediate/`, `Saved/` folders and rebuild

**No Visualization in Editor:**
- Make sure the actor is selected
- Or enable `bShowDebugFrustum`
- Check that Component Visualizers are enabled in Editor Preferences

### Runtime Issues / 运行时问题

**Black Screen / No Render:**
- Check that RenderTarget is created (auto-create should be enabled)
- Verify the tracked actor is set correctly
- Enable debug visualization to check frustum setup

## Next Steps / 下一步

1. ✅ Plugin compiled and enabled
2. ✅ Test visualization working
3. 📖 Read the full documentation: `README.md` (English) or `使用指南.md` (中文)
4. 🎮 Create your first asymmetric camera setup
5. 🚀 Explore advanced features (stereo rendering, CAVE systems, etc.)

## Support / 支持

If you encounter issues:
- Check the documentation files
- Enable `bShowDebugFrustum` for visual debugging
- Review the reference implementations listed in README.md

Good luck with your asymmetric camera project! / 祝你的非对称相机项目顺利！
