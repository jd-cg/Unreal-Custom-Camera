// Copyright Epic Games, Inc. All Rights Reserved.

#include "AsymmetricCameraActor.h"
#include "AsymmetricCameraComponent.h"
#include "AsymmetricScreenComponent.h"

AAsymmetricCameraActor::AAsymmetricCameraActor()
{
	PrimaryActorTick.bCanEverTick = false;

	// Root component — origin of the entire system
	RootComp = CreateDefaultSubobject<USceneComponent>(TEXT("RootComponent"));
	RootComponent = RootComp;

	// Screen component — defines the physical projection plane
	ScreenComponent = CreateDefaultSubobject<UAsymmetricScreenComponent>(TEXT("Screen"));
	ScreenComponent->SetupAttachment(RootComp);

	// Camera component — eye position, projection calculation
	AsymmetricCamera = CreateDefaultSubobject<UAsymmetricCameraComponent>(TEXT("AsymmetricCamera"));
	AsymmetricCamera->SetupAttachment(RootComp);
	AsymmetricCamera->ScreenComponent = ScreenComponent;
}
