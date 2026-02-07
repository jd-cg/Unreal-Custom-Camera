// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AsymmetricCameraActor.generated.h"

class UAsymmetricCameraComponent;
class UAsymmetricScreenComponent;

/**
 * Actor that provides asymmetric/off-axis camera projection.
 * Hierarchy: Root (SceneComponent) -> Screen (AsymmetricScreenComponent) + Camera (AsymmetricCameraComponent)
 *
 * - Root: origin of the system, move/rotate to reposition everything
 * - Screen: defines the physical projection plane (visible mesh in editor)
 * - Camera: eye position for projection calculation (or uses TrackedActor)
 */
UCLASS()
class ASYMMETRICCAMERA_API AAsymmetricCameraActor : public AActor
{
	GENERATED_BODY()

public:
	AAsymmetricCameraActor();

	virtual bool ShouldTickIfViewportsOnly() const override { return true; }

	/** Root scene component */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USceneComponent* RootComp;

	/** Screen component defining the projection plane */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UAsymmetricScreenComponent* ScreenComponent;

	/** Asymmetric camera component for projection calculation */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UAsymmetricCameraComponent* AsymmetricCamera;
};
