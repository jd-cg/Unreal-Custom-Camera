# Asymmetric Camera — UE5 非对称投影相机插件

**[English](README.md)**

适用于 **Unreal Engine 5.4** 的离轴投影插件，可用于 CAVE 系统、投影映射、多屏显示和头部追踪显示。

## 功能

- **零开销投影覆盖** — 通过 `ISceneViewExtension` 直接覆盖玩家相机投影矩阵，无需 Render Target 或 SceneCapture
- **编辑器交互可视化** — 可直观拖拽调整屏幕位置、朝向和大小，实时预览视锥
- **精细调试开关** — 屏幕边框、视锥线、眼球、近裁切面、标签均可独立开关
- **立体渲染** — 内置眼间距，支持左眼/右眼立体输出
- **蓝图支持** — 所有参数均暴露给蓝图
- **外部数据输入** — 支持导入 Max/Maya 标定数据，可引用场景 Actor 的 Transform

## 快速开始

1. 在关卡中放置 `AsymmetricCameraActor`
2. 选中 Actor，在 Details 面板调整 **Screen** 组件的位置/旋转和 `ScreenWidth` / `ScreenHeight`
3. 按 Play，玩家相机投影自动被覆盖
4. （可选）设置 `Tracked Actor` 实现头部追踪

## 组件结构

```text
AAsymmetricCameraActor
└── Root (SceneComponent)
    ├── Screen (AsymmetricScreenComponent — 投影屏幕平面)
    └── Camera (AsymmetricCameraComponent — 眼睛位置 + 投影计算)
```

## 参数说明

### Camera 组件 (AsymmetricCameraComponent)

| 参数 | 说明 |
| ---- | ---- |
| `bUseAsymmetricProjection` | 启用/禁用离轴投影 |
| `NearClip` | 近裁切面距离，默认 20 |
| `FarClip` | 远裁切面距离，0 表示无限远 |
| `EyeSeparation` | 立体渲染眼间距，0 为单目 |
| `EyeOffset` | 左眼 -1 / 中心 0 / 右眼 1 |
| `bMatchViewportAspectRatio` | 自动匹配屏幕宽高比，防止画面拉伸 |
| `TrackedActor` | 追踪目标 Actor，用作眼睛位置 |
| `ScreenComponent` | 引用的屏幕组件 |

### 外部数据输入

启用 `bUseExternalData` 后可绕过 Screen 组件，通过世界坐标或 Actor 引用定义投影屏幕（如 Max/Maya 标定数据）。

| 参数 | 说明 |
| ---- | ---- |
| `bUseExternalData` | 使用外部数据代替 ScreenComponent |
| `ExternalEyeActor` | 外部眼睛位置 Actor 引用（优先于 `ExternalEyePosition`） |
| `ExternalEyePosition` | 外部眼睛位置（世界坐标） |
| `ExternalScreenBLActor` | 外部屏幕左下角 Actor 引用 |
| `ExternalScreenBL` | 外部屏幕左下角（世界坐标） |
| `ExternalScreenBRActor` | 外部屏幕右下角 Actor 引用 |
| `ExternalScreenBR` | 外部屏幕右下角（世界坐标） |
| `ExternalScreenTLActor` | 外部屏幕左上角 Actor 引用 |
| `ExternalScreenTL` | 外部屏幕左上角（世界坐标） |
| `ExternalScreenTRActor` | 外部屏幕右上角 Actor 引用 |
| `ExternalScreenTR` | 外部屏幕右上角（世界坐标） |

> 优先级：Actor 引用 > FVector 坐标 > TrackedActor/ScreenComponent 回退。

### Screen 组件 (AsymmetricScreenComponent)

| 参数 | 说明 |
| ---- | ---- |
| `ScreenWidth` | 屏幕宽度，世界单位，默认 160 |
| `ScreenHeight` | 屏幕高度，世界单位，默认 90 |

屏幕平面在组件本地 YZ 平面，法线沿 +X 方向。通过组件的 Transform 控制位置和朝向。

### 调试开关

| 参数 | 说明 |
| ---- | ---- |
| `bShowDebugFrustum` | 主开关：编辑器调试可视化 |
| `bShowScreenOutline` | 屏幕边框 + 对角线 + 法线箭头（绿色） |
| `bShowFrustumLines` | 眼睛到屏幕四角的连线（黄色） |
| `bShowEyeHandle` | 眼球位置球体（黄色） |
| `bShowNearPlane` | 近裁切面矩形（红色） |
| `bShowLabels` | 角点标签和屏幕信息文字 |
| `bShowDebugInGame` | 游戏运行时显示调试线 |

## 蓝图 API

| 函数 | 说明 |
| ---- | ---- |
| `GetEyePosition()` | 获取当前生效的眼睛世界坐标（优先级：ExternalEyeActor > ExternalEyePosition > TrackedActor > 组件自身位置） |
| `GetEffectiveScreenCorners()` | 获取当前生效的屏幕四角坐标（外部数据或 ScreenComponent） |
| `SetExternalData(Eye, BL, BR, TL, TR)` | 一次性设置全部外部数据 |
| `CalculateOffAxisProjection()` | 手动计算离轴投影矩阵和视图旋转矩阵 |

## 工作原理

基于 Robert Kooima 的 **广义透视投影 (Generalized Perspective Projection)** 算法：

1. `BeginPlay` 时，`UAsymmetricCameraComponent` 注册 `FAsymmetricViewExtension`（`FWorldSceneViewExtension`）
2. 每帧 UE5 调用扩展的 `SetupViewProjectionMatrix()`
3. 扩展计算离轴投影矩阵（UE5 reversed-Z 格式）和屏幕对齐的视图旋转矩阵
4. 两者写入 `FSceneViewProjectionData`，直接覆盖玩家相机 — 无 RT、无 SceneCapture、无额外渲染 Pass

## 编译

```bash
# 快速编译（自动检测 UE5 路径）
QuickBuild.bat

# 如果 QuickBuild 无法编译，可直接打开项目在编辑器中编译（Ctrl+Alt+F11）
start "" "MyCustomCam.uproject"
```

## 参考资料

- [Robert Kooima: Generalized Perspective Projection](http://csc.lsu.edu/~kooima/articles/genperspective/)
- [GitHub: fweidner/UE4-Plugin-OffAxis](https://github.com/fweidner/UE4-Plugin-OffAxis)

## 许可证

Apache License 2.0
