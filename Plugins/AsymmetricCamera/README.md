# Asymmetric Camera - UE5 非对称投影相机插件

适用于 Unreal Engine 5.4 的离轴投影插件，可用于 CAVE 系统、投影映射、多屏显示和头部追踪显示。

## 功能

- 直接覆盖玩家相机投影矩阵，无需 Render Target 或 SceneCapture
- 编辑器中可直观拖拽调整屏幕位置、朝向和大小
- 屏幕边框、视锥线、眼球、近裁切面、标签均可独立开关
- 支持立体渲染（左眼/右眼）
- 支持蓝图

## 使用方法

1. 在关卡中放置 `AsymmetricCameraActor`
2. 选中 Actor，在 Details 面板调整 **Screen** 组件的位置/旋转和 ScreenWidth/ScreenHeight
3. 按 Play，玩家相机投影自动被覆盖
4. （可选）设置 `Tracked Actor` 实现头部追踪

## 组件结构

```
AAsymmetricCameraActor
└── Root (SceneComponent - 整体原点，拖拽移动/旋转整个系统)
    ├── Screen (AsymmetricScreenComponent - 投影屏幕平面)
    └── Camera (AsymmetricCameraComponent - 眼睛位置 + 投影计算)
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

## 编译

```bash
QuickBuild.bat
```
