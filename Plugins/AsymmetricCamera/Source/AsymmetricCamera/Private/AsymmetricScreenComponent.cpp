// Copyright Epic Games, Inc. All Rights Reserved.

#include "AsymmetricScreenComponent.h"

UAsymmetricScreenComponent::UAsymmetricScreenComponent()
{
	// Default: screen 100 units ahead of root, 160 wide x 90 tall (16:9)
	SetRelativeLocation(FVector(100.0f, 0.0f, 0.0f));
	ScreenWidth = 160.0f;
	ScreenHeight = 90.0f;
}

FVector2D UAsymmetricScreenComponent::GetScreenSize() const
{
	return FVector2D(ScreenWidth, ScreenHeight);
}

void UAsymmetricScreenComponent::SetScreenSize(const FVector2D& NewSize)
{
	ScreenWidth = FMath::Max(NewSize.X, 1.0f);
	ScreenHeight = FMath::Max(NewSize.Y, 1.0f);
}

void UAsymmetricScreenComponent::GetScreenCornersWorld(
	FVector& OutBottomLeft, FVector& OutBottomRight,
	FVector& OutTopLeft, FVector& OutTopRight) const
{
	// Screen corners in component local space (nDisplay convention):
	// Screen lies in YZ plane, normal along +X.
	// Y = horizontal (width), Z = vertical (height)
	const float HW = ScreenWidth * 0.5f;
	const float HH = ScreenHeight * 0.5f;

	const FVector LocalBL(0.0f, -HW, -HH); // Left-Bottom
	const FVector LocalBR(0.0f,  HW, -HH); // Right-Bottom
	const FVector LocalTL(0.0f, -HW,  HH); // Left-Top
	const FVector LocalTR(0.0f,  HW,  HH); // Right-Top

	// Transform to world space using position + rotation (not scale)
	const FVector ScreenLoc = GetComponentLocation();
	const FQuat ScreenQuat = GetComponentQuat();

	OutBottomLeft  = ScreenLoc + ScreenQuat.RotateVector(LocalBL);
	OutBottomRight = ScreenLoc + ScreenQuat.RotateVector(LocalBR);
	OutTopLeft     = ScreenLoc + ScreenQuat.RotateVector(LocalTL);
	OutTopRight    = ScreenLoc + ScreenQuat.RotateVector(LocalTR);
}

void UAsymmetricScreenComponent::GetScreenCornersLocal(
	FVector& OutBottomLeft, FVector& OutBottomRight,
	FVector& OutTopLeft, FVector& OutTopRight) const
{
	FVector WorldBL, WorldBR, WorldTL, WorldTR;
	GetScreenCornersWorld(WorldBL, WorldBR, WorldTL, WorldTR);

	const AActor* Owner = GetOwner();
	if (Owner)
	{
		const FTransform ActorTransform = Owner->GetActorTransform();
		OutBottomLeft  = ActorTransform.InverseTransformPositionNoScale(WorldBL);
		OutBottomRight = ActorTransform.InverseTransformPositionNoScale(WorldBR);
		OutTopLeft     = ActorTransform.InverseTransformPositionNoScale(WorldTL);
		OutTopRight    = ActorTransform.InverseTransformPositionNoScale(WorldTR);
	}
	else
	{
		OutBottomLeft  = WorldBL;
		OutBottomRight = WorldBR;
		OutTopLeft     = WorldTL;
		OutTopRight    = WorldTR;
	}
}
