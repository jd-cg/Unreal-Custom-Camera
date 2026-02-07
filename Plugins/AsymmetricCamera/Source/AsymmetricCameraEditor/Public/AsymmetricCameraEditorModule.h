// AsymmetricCamera 编辑器模块，注册组件可视化器

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FAsymmetricCameraEditorModule : public IModuleInterface
{
public:
	// IModuleInterface 接口
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
