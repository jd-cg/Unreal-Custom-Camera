// 非对称相机 Actor 实现

#include "AsymmetricCameraActor.h"
#include "AsymmetricCameraComponent.h"
#include "AsymmetricScreenComponent.h"

AAsymmetricCameraActor::AAsymmetricCameraActor()
{
	PrimaryActorTick.bCanEverTick = false;

	// 根组件，整个系统的原点
	RootComp = CreateDefaultSubobject<USceneComponent>(TEXT("RootComponent"));
	RootComponent = RootComp;

	// 屏幕组件，定义投影平面
	ScreenComponent = CreateDefaultSubobject<UAsymmetricScreenComponent>(TEXT("Screen"));
	ScreenComponent->SetupAttachment(RootComp);

	// 相机组件，眼睛位置 + 投影计算
	AsymmetricCamera = CreateDefaultSubobject<UAsymmetricCameraComponent>(TEXT("AsymmetricCamera"));
	AsymmetricCamera->SetupAttachment(RootComp);
	AsymmetricCamera->ScreenComponent = ScreenComponent;
}
