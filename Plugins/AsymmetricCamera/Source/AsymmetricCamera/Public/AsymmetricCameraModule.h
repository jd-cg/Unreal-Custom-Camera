// AsymmetricCamera 运行时模块

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FAsymmetricCameraModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
