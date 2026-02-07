// 编辑器组件可视化器，在视口里画视锥和眼睛手柄

#pragma once

#include "CoreMinimal.h"
#include "ComponentVisualizer.h"

class UAsymmetricCameraComponent;

/** 眼睛位置的点击代理 */
struct HEyePositionProxy : public HComponentVisProxy
{
	DECLARE_HIT_PROXY();

	HEyePositionProxy(const UActorComponent* InComponent)
		: HComponentVisProxy(InComponent, HPP_Wireframe)
	{
	}
};

/**
 * AsymmetricCameraComponent 的编辑器可视化器。
 * 画投影视锥和眼睛位置手柄。
 * 屏幕的可视化由 UAsymmetricScreenComponent 的 mesh 负责。
 */
class FAsymmetricCameraComponentVisualizer : public FComponentVisualizer
{
public:
	FAsymmetricCameraComponentVisualizer();
	virtual ~FAsymmetricCameraComponentVisualizer();

	// FComponentVisualizer 接口
	virtual void DrawVisualization(const UActorComponent* Component, const FSceneView* View, FPrimitiveDrawInterface* PDI) override;
	virtual bool VisProxyHandleClick(FEditorViewportClient* InViewportClient, HComponentVisProxy* VisProxy, const FViewportClick& Click) override;
	virtual void EndEditing() override;
	virtual bool GetWidgetLocation(const FEditorViewportClient* ViewportClient, FVector& OutLocation) const override;
	virtual bool HandleInputDelta(FEditorViewportClient* ViewportClient, FViewport* Viewport, FVector& DeltaTranslate, FRotator& DeltaRotate, FVector& DeltaScale) override;
	virtual void DrawVisualizationHUD(const UActorComponent* Component, const FViewport* Viewport, const FSceneView* View, FCanvas* Canvas) override;

private:
	void DrawFrustum(const UAsymmetricCameraComponent* CameraComponent, FPrimitiveDrawInterface* PDI) const;
	void DrawScreenOutline(const UAsymmetricCameraComponent* CameraComponent, FPrimitiveDrawInterface* PDI) const;

	/** 眼睛手柄是否被选中 */
	bool bEyeSelected;

	/** 正在编辑的组件 */
	TWeakObjectPtr<UAsymmetricCameraComponent> SelectedComponent;
};
