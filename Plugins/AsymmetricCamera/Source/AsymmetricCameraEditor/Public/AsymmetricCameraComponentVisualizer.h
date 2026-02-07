// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ComponentVisualizer.h"

class UAsymmetricCameraComponent;

/** Hit proxy for the eye position handle */
struct HEyePositionProxy : public HComponentVisProxy
{
	DECLARE_HIT_PROXY();

	HEyePositionProxy(const UActorComponent* InComponent)
		: HComponentVisProxy(InComponent, HPP_Wireframe)
	{
	}
};

/**
 * Editor visualizer for AsymmetricCameraComponent.
 * Draws the projection frustum and eye position handle.
 * Screen visualization is handled by the UAsymmetricScreenComponent mesh.
 */
class FAsymmetricCameraComponentVisualizer : public FComponentVisualizer
{
public:
	FAsymmetricCameraComponentVisualizer();
	virtual ~FAsymmetricCameraComponentVisualizer();

	// FComponentVisualizer interface
	virtual void DrawVisualization(const UActorComponent* Component, const FSceneView* View, FPrimitiveDrawInterface* PDI) override;
	virtual bool VisProxyHandleClick(FEditorViewportClient* InViewportClient, HComponentVisProxy* VisProxy, const FViewportClick& Click) override;
	virtual void EndEditing() override;
	virtual bool GetWidgetLocation(const FEditorViewportClient* ViewportClient, FVector& OutLocation) const override;
	virtual bool HandleInputDelta(FEditorViewportClient* ViewportClient, FViewport* Viewport, FVector& DeltaTranslate, FRotator& DeltaRotate, FVector& DeltaScale) override;
	virtual void DrawVisualizationHUD(const UActorComponent* Component, const FViewport* Viewport, const FSceneView* View, FCanvas* Canvas) override;

private:
	void DrawFrustum(const UAsymmetricCameraComponent* CameraComponent, FPrimitiveDrawInterface* PDI) const;
	void DrawScreenOutline(const UAsymmetricCameraComponent* CameraComponent, FPrimitiveDrawInterface* PDI) const;

	/** Whether the eye position handle is selected */
	bool bEyeSelected;

	/** The component being edited */
	TWeakObjectPtr<UAsymmetricCameraComponent> SelectedComponent;
};
