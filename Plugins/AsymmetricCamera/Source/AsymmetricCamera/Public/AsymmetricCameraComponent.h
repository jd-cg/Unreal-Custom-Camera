// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "AsymmetricCameraComponent.generated.h"

class FAsymmetricViewExtension;
class UAsymmetricScreenComponent;

/**
 * Camera component that provides off-axis/asymmetric frustum projection.
 * Based on Robert Kooima's "Generalized Perspective Projection" algorithm,
 * aligned with nDisplay's projection calculation.
 *
 * High-performance path: directly overrides the player camera's projection matrix
 * via ISceneViewExtension. No Render Target or SceneCapture needed.
 *
 * Requires a sibling UAsymmetricScreenComponent to define the projection screen.
 * Eye position = this component's world location (or TrackedActor if set).
 */
UCLASS(ClassGroup = Camera, meta = (BlueprintSpawnableComponent), hideCategories = (Mobility))
class ASYMMETRICCAMERA_API UAsymmetricCameraComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UAsymmetricCameraComponent();

	/** Enable/Disable off-axis projection */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Asymmetric Camera")
	bool bUseAsymmetricProjection;

	/** Near clipping plane distance */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Asymmetric Camera", meta = (ClampMin = "0.01"))
	float NearClip;

	/** Far clipping plane distance (0 = infinite, matching UE5 default) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Asymmetric Camera", meta = (ClampMin = "0.0"))
	float FarClip;

	/** Eye separation for stereoscopic rendering (0 for mono) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Asymmetric Camera|Stereo", meta = (ClampMin = "0.0"))
	float EyeSeparation;

	/** Which eye to render (left = -1, center = 0, right = 1) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Asymmetric Camera|Stereo", meta = (ClampMin = "-1.0", ClampMax = "1.0"))
	float EyeOffset;

	/** Automatically adjust viewport to match screen aspect ratio (prevents stretching).
	 *  Disable for CAVE/multi-display setups where screen aspect ratio matches the physical display. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Asymmetric Camera")
	bool bMatchViewportAspectRatio;

	/** Master switch: debug visualization in editor viewport */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Asymmetric Camera|Debug")
	bool bShowDebugFrustum;

	/** Show screen wireframe outline, diagonals, and normal arrow */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Asymmetric Camera|Debug", meta = (EditCondition = "bShowDebugFrustum"))
	bool bShowScreenOutline;

	/** Show frustum lines from eye to screen corners */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Asymmetric Camera|Debug", meta = (EditCondition = "bShowDebugFrustum"))
	bool bShowFrustumLines;

	/** Show eye position sphere handle */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Asymmetric Camera|Debug", meta = (EditCondition = "bShowDebugFrustum"))
	bool bShowEyeHandle;

	/** Show near clipping plane visualization */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Asymmetric Camera|Debug", meta = (EditCondition = "bShowDebugFrustum"))
	bool bShowNearPlane;

	/** Show corner labels (BL/BR/TL/TR), eye label, and screen info text */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Asymmetric Camera|Debug", meta = (EditCondition = "bShowDebugFrustum"))
	bool bShowLabels;

	/** Show debug visualization during gameplay (DrawDebugLine) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Asymmetric Camera|Debug")
	bool bShowDebugInGame;

	/** Optional: Actor to track for eye position. If not set, uses this component's world location. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Asymmetric Camera|Tracking")
	AActor* TrackedActor;

	/** Reference to the screen component that defines the projection plane */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Asymmetric Camera")
	UAsymmetricScreenComponent* ScreenComponent;

	/**
	 * Get the eye position in world space.
	 * Priority: TrackedActor > this component's world location
	 */
	UFUNCTION(BlueprintCallable, Category = "Asymmetric Camera")
	FVector GetEyePosition() const;

	/**
	 * Calculate the off-axis projection.
	 * @param EyePosition - Eye position in world space
	 * @param OutViewRotation - Screen orientation as FRotator (for ViewRotationMatrix construction)
	 * @param OutProjectionMatrix - The resulting projection matrix (UE5 reversed-Z)
	 * @return true if successful
	 */
	UFUNCTION(BlueprintCallable, Category = "Asymmetric Camera")
	bool CalculateOffAxisProjection(const FVector& EyePosition, FRotator& OutViewRotation, FMatrix& OutProjectionMatrix);

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	/** Draw debug visualization during gameplay */
	void DrawDebugVisualization() const;

	/** Scene view extension for overriding player camera projection */
	TSharedPtr<FAsymmetricViewExtension, ESPMode::ThreadSafe> ViewExtension;
};