// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SceneViewExtension.h"

class UAsymmetricCameraComponent;

/**
 * Scene view extension that overrides the player camera's projection matrix
 * with an off-axis asymmetric projection. This is the high-performance path
 * that directly modifies the main camera's projection without any Render Target.
 */
class FAsymmetricViewExtension : public FWorldSceneViewExtension
{
public:
	FAsymmetricViewExtension(const FAutoRegister& AutoRegister, UWorld* InWorld, UAsymmetricCameraComponent* InComponent);

	// ISceneViewExtension interface
	virtual void SetupViewFamily(FSceneViewFamily& InViewFamily) override {}
	virtual void SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView) override {}
	virtual void BeginRenderViewFamily(FSceneViewFamily& InViewFamily) override {}
	virtual void SetupViewProjectionMatrix(FSceneViewProjectionData& InOutProjectionData) override;

private:
	TWeakObjectPtr<UAsymmetricCameraComponent> CameraComponent;
};
