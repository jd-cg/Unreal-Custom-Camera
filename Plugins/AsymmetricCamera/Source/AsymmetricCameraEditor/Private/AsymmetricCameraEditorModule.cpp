// AsymmetricCamera 编辑器模块实现

#include "AsymmetricCameraEditorModule.h"
#include "AsymmetricCameraComponentVisualizer.h"
#include "AsymmetricCameraComponent.h"
#include "UnrealEdGlobals.h"
#include "Editor/UnrealEdEngine.h"

#define LOCTEXT_NAMESPACE "FAsymmetricCameraEditorModule"

void FAsymmetricCameraEditorModule::StartupModule()
{
	// 注册组件可视化器
	if (GUnrealEd)
	{
		TSharedPtr<FAsymmetricCameraComponentVisualizer> Visualizer = MakeShareable(new FAsymmetricCameraComponentVisualizer());

		if (Visualizer.IsValid())
		{
			GUnrealEd->RegisterComponentVisualizer(UAsymmetricCameraComponent::StaticClass()->GetFName(), Visualizer);
			Visualizer->OnRegister();
		}
	}
}

void FAsymmetricCameraEditorModule::ShutdownModule()
{
	// 反注册组件可视化器
	if (GUnrealEd)
	{
		GUnrealEd->UnregisterComponentVisualizer(UAsymmetricCameraComponent::StaticClass()->GetFName());
	}
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FAsymmetricCameraEditorModule, AsymmetricCameraEditor)
