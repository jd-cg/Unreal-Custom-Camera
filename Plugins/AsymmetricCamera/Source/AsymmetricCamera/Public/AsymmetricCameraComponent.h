// 非对称相机组件，核心投影计算逻辑

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "AsymmetricCameraComponent.generated.h"

class FAsymmetricViewExtension;
class UAsymmetricScreenComponent;

/**
 * 离轴/非对称视锥投影相机组件
 * 基于 Robert Kooima 的 "Generalized Perspective Projection" 算法，
 * 和 nDisplay 的投影计算方式对齐。
 *
 * 高性能路径：通过 ISceneViewExtension 直接覆盖玩家相机的投影矩阵，
 * 不需要 Render Target 或 SceneCapture。
 *
 * 需要一个同级的 UAsymmetricScreenComponent 来定义投影屏幕。
 * 眼睛位置 = 组件的世界坐标（设了 TrackedActor 就用它的位置）。
 */
UCLASS(ClassGroup = Camera, meta = (BlueprintSpawnableComponent), hideCategories = (Mobility))
class ASYMMETRICCAMERA_API UAsymmetricCameraComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UAsymmetricCameraComponent();

	/** 开关：是否启用离轴投影 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Asymmetric Camera")
	bool bUseAsymmetricProjection;

	/** 近裁切面距离 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Asymmetric Camera", meta = (ClampMin = "0.01"))
	float NearClip;

	/** 远裁切面距离（0 = 无限远，和 UE5 默认一样） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Asymmetric Camera", meta = (ClampMin = "0.0"))
	float FarClip;

	/** 双眼间距，用于立体渲染（0 = 单眼） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Asymmetric Camera|Stereo", meta = (ClampMin = "0.0"))
	float EyeSeparation;

	/** 渲染哪只眼（左 = -1，中 = 0，右 = 1） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Asymmetric Camera|Stereo", meta = (ClampMin = "-1.0", ClampMax = "1.0"))
	float EyeOffset;

	/** 自动调整视口比例匹配屏幕宽高比（防止画面拉伸）。
	 *  CAVE/多屏系统里屏幕比例本身就对的话可以关掉。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Asymmetric Camera")
	bool bMatchViewportAspectRatio;

	/** 开关：MRQ（Movie Render Queue）离线渲染时也应用非对称投影。
	 *  开启后可将此组件挂到 CineCameraActor 上，配合 Sequencer + MRQ 使用。
	 *  MRQ 的 tiling 高分辨率渲染与非对称投影不兼容，建议 tiling 设为 1x1。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Asymmetric Camera")
	bool bEnableMRQSupport;

	/** 总开关：编辑器视口里显示调试可视化 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Asymmetric Camera|Debug")
	bool bShowDebugFrustum;

	/** 显示屏幕边框线、对角线和法线箭头 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Asymmetric Camera|Debug", meta = (EditCondition = "bShowDebugFrustum"))
	bool bShowScreenOutline;

	/** 显示从眼睛到屏幕四角的视锥线 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Asymmetric Camera|Debug", meta = (EditCondition = "bShowDebugFrustum"))
	bool bShowFrustumLines;

	/** 显示眼睛位置的球形手柄 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Asymmetric Camera|Debug", meta = (EditCondition = "bShowDebugFrustum"))
	bool bShowEyeHandle;

	/** 显示近裁切面可视化 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Asymmetric Camera|Debug", meta = (EditCondition = "bShowDebugFrustum"))
	bool bShowNearPlane;

	/** 显示角标签（BL/BR/TL/TR）、眼睛标签和屏幕信息文字 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Asymmetric Camera|Debug", meta = (EditCondition = "bShowDebugFrustum"))
	bool bShowLabels;

	/** 运行时也显示调试可视化（DrawDebugLine） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Asymmetric Camera|Debug")
	bool bShowDebugInGame;

	/** 可选：跟踪的 Actor，用它的位置作为眼睛位置。不设就用组件自身的世界坐标。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Asymmetric Camera|Tracking")
	AActor* TrackedActor;

	/** 开关：Owner Actor 的 Transform 完全跟随此相机。
	 *  用于 MRQ 渲染场景：Sequencer 驱动电影相机动画，非对称相机自动同步位置和旋转。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Asymmetric Camera|Tracking")
	bool bFollowTargetCamera;

	/** 要跟随的目标相机 Actor（通常是 CineCameraActor）。
	 *  开启 bFollowTargetCamera 后，Owner Actor 每帧同步此相机的 Transform。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Asymmetric Camera|Tracking", meta = (EditCondition = "bFollowTargetCamera"))
	AActor* TargetCamera;

	/** 关联的屏幕组件，定义投影平面。
	 *  自动查找同 Actor 上的 AsymmetricScreenComponent，也可手动指定。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Asymmetric Camera")
	TObjectPtr<UAsymmetricScreenComponent> ScreenComponent;

	// ---- 外部数据输入（对接 Max/Maya 等外部工具） ----

	/** 开关：使用外部数据代替 ScreenComponent */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Asymmetric Camera|External")
	bool bUseExternalData;

	/** 外部眼睛位置 Actor（设了就用它的位置，否则用 ExternalEyePosition） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Asymmetric Camera|External", meta = (EditCondition = "bUseExternalData"))
	AActor* ExternalEyeActor;

	/** 外部眼睛位置（世界坐标，ExternalEyeActor 没设时生效） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Asymmetric Camera|External", meta = (EditCondition = "bUseExternalData"))
	FVector ExternalEyePosition;

	/** 外部屏幕左下角 Actor（设了就用它的位置，否则用 ExternalScreenBL） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Asymmetric Camera|External", meta = (EditCondition = "bUseExternalData"))
	AActor* ExternalScreenBLActor;

	/** 外部屏幕左下角（世界坐标） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Asymmetric Camera|External", meta = (EditCondition = "bUseExternalData"))
	FVector ExternalScreenBL;

	/** 外部屏幕右下角 Actor */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Asymmetric Camera|External", meta = (EditCondition = "bUseExternalData"))
	AActor* ExternalScreenBRActor;

	/** 外部屏幕右下角（世界坐标） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Asymmetric Camera|External", meta = (EditCondition = "bUseExternalData"))
	FVector ExternalScreenBR;

	/** 外部屏幕左上角 Actor */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Asymmetric Camera|External", meta = (EditCondition = "bUseExternalData"))
	AActor* ExternalScreenTLActor;

	/** 外部屏幕左上角（世界坐标） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Asymmetric Camera|External", meta = (EditCondition = "bUseExternalData"))
	FVector ExternalScreenTL;

	/** 外部屏幕右上角 Actor */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Asymmetric Camera|External", meta = (EditCondition = "bUseExternalData"))
	AActor* ExternalScreenTRActor;

	/** 外部屏幕右上角（世界坐标） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Asymmetric Camera|External", meta = (EditCondition = "bUseExternalData"))
	FVector ExternalScreenTR;

	/** 一次性设置全部外部数据 */
	UFUNCTION(BlueprintCallable, Category = "Asymmetric Camera|External")
	void SetExternalData(const FVector& EyePos, const FVector& BL, const FVector& BR, const FVector& TL, const FVector& TR);

	/** 获取当前生效的屏幕四角（根据模式自动选择数据源） */
	UFUNCTION(BlueprintCallable, Category = "Asymmetric Camera")
	void GetEffectiveScreenCorners(FVector& OutBL, FVector& OutBR, FVector& OutTL, FVector& OutTR) const;

	/**
	 * 获取眼睛的世界坐标。
	 * 优先级：TrackedActor > 组件自身位置
	 */
	UFUNCTION(BlueprintCallable, Category = "Asymmetric Camera")
	FVector GetEyePosition() const;

	/**
	 * 计算离轴投影。
	 * @param EyePosition - 眼睛世界坐标
	 * @param OutViewRotation - 屏幕朝向（用于构建 ViewRotationMatrix）
	 * @param OutProjectionMatrix - 输出的投影矩阵（UE5 reversed-Z 格式）
	 * @return 计算成功返回 true
	 */
	UFUNCTION(BlueprintCallable, Category = "Asymmetric Camera")
	bool CalculateOffAxisProjection(const FVector& EyePosition, FRotator& OutViewRotation, FMatrix& OutProjectionMatrix);

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void OnRegister() override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	/** 运行时画调试线 */
	void DrawDebugVisualization() const;

	/** 场景视图扩展，用来覆盖玩家相机投影 */
	TSharedPtr<FAsymmetricViewExtension, ESPMode::ThreadSafe> ViewExtension;
};
