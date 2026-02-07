// 非对称相机 Actor，放到关卡里就能用

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AsymmetricCameraActor.generated.h"

class UAsymmetricCameraComponent;
class UAsymmetricScreenComponent;

/**
 * 非对称/离轴投影相机 Actor
 * 组件层级: Root (SceneComponent) -> Screen (屏幕组件) + Camera (相机组件)
 *
 * - Root: 整个系统的原点，移动/旋转它来调整整体位置
 * - Screen: 定义物理投影平面（编辑器里可见）
 * - Camera: 眼睛位置，用于投影计算（也可以用 TrackedActor 代替）
 */
UCLASS()
class ASYMMETRICCAMERA_API AAsymmetricCameraActor : public AActor
{
	GENERATED_BODY()

public:
	AAsymmetricCameraActor();

	virtual bool ShouldTickIfViewportsOnly() const override { return true; }

	/** 根组件 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USceneComponent* RootComp;

	/** 屏幕组件，定义投影平面 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UAsymmetricScreenComponent* ScreenComponent;

	/** 非对称相机组件，负责投影计算 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UAsymmetricCameraComponent* AsymmetricCamera;
};
