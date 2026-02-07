// AsymmetricCamera 运行时模块实现

#include "AsymmetricCameraModule.h"

#define LOCTEXT_NAMESPACE "FAsymmetricCameraModule"

void FAsymmetricCameraModule::StartupModule()
{
	// 模块加载时执行，具体时机由 .uplugin 配置决定
}

void FAsymmetricCameraModule::ShutdownModule()
{
	// 模块卸载时的清理工作
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FAsymmetricCameraModule, AsymmetricCamera)
