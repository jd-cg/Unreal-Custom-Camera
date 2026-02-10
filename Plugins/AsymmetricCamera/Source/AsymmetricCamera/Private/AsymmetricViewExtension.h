// 场景视图扩展，用来覆盖玩家相机的投影矩阵

#pragma once

#include "CoreMinimal.h"
#include "SceneViewExtension.h"

class UAsymmetricCameraComponent;

/**
 * 场景视图扩展，把玩家相机的投影矩阵替换成离轴非对称投影。
 * 高性能路径：直接改主相机的投影，不需要 Render Target。
 */
class FAsymmetricViewExtension : public FWorldSceneViewExtension
{
public:
	FAsymmetricViewExtension(const FAutoRegister& AutoRegister, UWorld* InWorld, UAsymmetricCameraComponent* InComponent);

	// ISceneViewExtension 接口
	virtual void SetupViewFamily(FSceneViewFamily& InViewFamily) override {}
	virtual void SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView) override;
	virtual void BeginRenderViewFamily(FSceneViewFamily& InViewFamily) override {}
	virtual void SetupViewProjectionMatrix(FSceneViewProjectionData& InOutProjectionData) override;

private:
	TWeakObjectPtr<UAsymmetricCameraComponent> CameraComponent;
};
