// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "AsymmetricScreenComponent.generated.h"

/**
 * Screen component for asymmetric/off-axis projection.
 * Represents a physical projection screen defined by position, rotation, and size.
 * Visualization is handled by the editor component visualizer (wireframe outline).
 *
 * The screen plane lies in the component's local YZ plane with normal along +X,
 * matching nDisplay's convention.
 */
UCLASS(ClassGroup = Camera, meta = (BlueprintSpawnableComponent))
class ASYMMETRICCAMERA_API UAsymmetricScreenComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UAsymmetricScreenComponent();

	/** Screen width in world units */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Asymmetric Camera|Screen", meta = (ClampMin = "1.0"))
	float ScreenWidth;

	/** Screen height in world units */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Asymmetric Camera|Screen", meta = (ClampMin = "1.0"))
	float ScreenHeight;

	/** Get the screen size as (Width, Height). */
	UFUNCTION(BlueprintCallable, Category = "Asymmetric Camera|Screen")
	FVector2D GetScreenSize() const;

	/** Set the screen size in world units. */
	UFUNCTION(BlueprintCallable, Category = "Asymmetric Camera|Screen")
	void SetScreenSize(const FVector2D& NewSize);

	/**
	 * Get the four screen corners in world space.
	 * Order: BottomLeft, BottomRight, TopLeft, TopRight.
	 */
	UFUNCTION(BlueprintCallable, Category = "Asymmetric Camera|Screen")
	void GetScreenCornersWorld(FVector& OutBottomLeft, FVector& OutBottomRight, FVector& OutTopLeft, FVector& OutTopRight) const;

	/**
	 * Get the four screen corners in the owning actor's local space.
	 * Order: BottomLeft, BottomRight, TopLeft, TopRight.
	 */
	UFUNCTION(BlueprintCallable, Category = "Asymmetric Camera|Screen")
	void GetScreenCornersLocal(FVector& OutBottomLeft, FVector& OutBottomRight, FVector& OutTopLeft, FVector& OutTopRight) const;
};
