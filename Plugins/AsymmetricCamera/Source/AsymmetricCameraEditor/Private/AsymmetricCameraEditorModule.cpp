// Copyright Epic Games, Inc. All Rights Reserved.

#include "AsymmetricCameraEditorModule.h"
#include "AsymmetricCameraComponentVisualizer.h"
#include "AsymmetricCameraComponent.h"
#include "UnrealEdGlobals.h"
#include "Editor/UnrealEdEngine.h"

#define LOCTEXT_NAMESPACE "FAsymmetricCameraEditorModule"

void FAsymmetricCameraEditorModule::StartupModule()
{
	// Register component visualizer
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
	// Unregister component visualizer
	if (GUnrealEd)
	{
		GUnrealEd->UnregisterComponentVisualizer(UAsymmetricCameraComponent::StaticClass()->GetFName());
	}
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FAsymmetricCameraEditorModule, AsymmetricCameraEditor)
