// Copyright Epic Games, Inc. All Rights Reserved.

#include "AsymmetricViewExtension.h"
#include "AsymmetricCameraComponent.h"
#include "AsymmetricScreenComponent.h"
#include "Math/InverseRotationMatrix.h"
#include "SceneView.h"

DEFINE_LOG_CATEGORY_STATIC(LogAsymmetricCamera, Log, All);

static int32 GAsymmetricDebugLogFrames = 0;

FAsymmetricViewExtension::FAsymmetricViewExtension(
	const FAutoRegister& AutoRegister,
	UWorld* InWorld,
	UAsymmetricCameraComponent* InComponent)
	: FWorldSceneViewExtension(AutoRegister, InWorld)
	, CameraComponent(InComponent)
{
}

void FAsymmetricViewExtension::SetupViewProjectionMatrix(FSceneViewProjectionData& InOutProjectionData)
{
	if (!CameraComponent.IsValid() || !CameraComponent->bUseAsymmetricProjection)
	{
		return;
	}

	FVector EyePosition = CameraComponent->GetEyePosition();

	FRotator ViewRotation;
	FMatrix ProjectionMatrix;

	if (!CameraComponent->CalculateOffAxisProjection(EyePosition, ViewRotation, ProjectionMatrix))
	{
		return;
	}

	// Build ViewRotationMatrix using UE5's standard formula:
	//   FInverseRotationMatrix(ViewRotation) * SwizzleMatrix
	// This matches LocalPlayer.cpp:1244 exactly.
	static const FMatrix SwizzleMatrix(
		FPlane(0, 0, 1, 0),
		FPlane(1, 0, 0, 0),
		FPlane(0, 1, 0, 0),
		FPlane(0, 0, 0, 1)
	);
	const FMatrix ViewRotationMatrix = FInverseRotationMatrix(ViewRotation) * SwizzleMatrix;

	// Debug log (first 3 frames only)
	if (GAsymmetricDebugLogFrames < 3)
	{
		GAsymmetricDebugLogFrames++;
		UE_LOG(LogAsymmetricCamera, Warning, TEXT("=== AsymmetricCamera Debug Frame %d ==="), GAsymmetricDebugLogFrames);
		UE_LOG(LogAsymmetricCamera, Warning, TEXT("  EyePosition: %s"), *EyePosition.ToString());
		UE_LOG(LogAsymmetricCamera, Warning, TEXT("  ViewRotation: %s"), *ViewRotation.ToString());
		UE_LOG(LogAsymmetricCamera, Warning, TEXT("  Original ViewOrigin: %s"), *InOutProjectionData.ViewOrigin.ToString());
		UE_LOG(LogAsymmetricCamera, Warning, TEXT("  ProjMatrix[0]: %.4f %.4f %.4f %.4f"), ProjectionMatrix.M[0][0], ProjectionMatrix.M[0][1], ProjectionMatrix.M[0][2], ProjectionMatrix.M[0][3]);
		UE_LOG(LogAsymmetricCamera, Warning, TEXT("  ProjMatrix[1]: %.4f %.4f %.4f %.4f"), ProjectionMatrix.M[1][0], ProjectionMatrix.M[1][1], ProjectionMatrix.M[1][2], ProjectionMatrix.M[1][3]);
		UE_LOG(LogAsymmetricCamera, Warning, TEXT("  ProjMatrix[2]: %.4f %.4f %.4f %.4f"), ProjectionMatrix.M[2][0], ProjectionMatrix.M[2][1], ProjectionMatrix.M[2][2], ProjectionMatrix.M[2][3]);
		UE_LOG(LogAsymmetricCamera, Warning, TEXT("  ProjMatrix[3]: %.4f %.4f %.4f %.4f"), ProjectionMatrix.M[3][0], ProjectionMatrix.M[3][1], ProjectionMatrix.M[3][2], ProjectionMatrix.M[3][3]);
		UE_LOG(LogAsymmetricCamera, Warning, TEXT("  ViewRect: %d,%d - %d,%d"), InOutProjectionData.ViewRect.Min.X, InOutProjectionData.ViewRect.Min.Y, InOutProjectionData.ViewRect.Max.X, InOutProjectionData.ViewRect.Max.Y);
	}

	InOutProjectionData.ViewOrigin = EyePosition;
	InOutProjectionData.ViewRotationMatrix = ViewRotationMatrix;
	InOutProjectionData.ProjectionMatrix = ProjectionMatrix;

	// Constrain viewport to match screen aspect ratio (prevents stretching)
	if (CameraComponent->bMatchViewportAspectRatio && CameraComponent->ScreenComponent)
	{
		const FVector2D ScreenSize = CameraComponent->ScreenComponent->GetScreenSize();
		const float ScreenW = ScreenSize.X;
		const float ScreenH = ScreenSize.Y;
		if (ScreenW <= SMALL_NUMBER || ScreenH <= SMALL_NUMBER)
		{
			return;
		}

		const float ScreenAspect = ScreenW / ScreenH;

		const FIntRect FullRect = InOutProjectionData.ViewRect;
		const int32 ViewW = FullRect.Width();
		const int32 ViewH = FullRect.Height();
		if (ViewW <= 0 || ViewH <= 0)
		{
			return;
		}

		const float ViewportAspect = static_cast<float>(ViewW) / static_cast<float>(ViewH);
		if (FMath::IsNearlyEqual(ScreenAspect, ViewportAspect, 0.001f))
		{
			return;
		}

		int32 NewW, NewH;
		int32 OffX = FullRect.Min.X;
		int32 OffY = FullRect.Min.Y;

		if (ScreenAspect < ViewportAspect)
		{
			// Pillarbox (black bars on sides)
			NewH = ViewH;
			NewW = FMath::RoundToInt(static_cast<float>(ViewH) * ScreenAspect);
			OffX += (ViewW - NewW) / 2;
		}
		else
		{
			// Letterbox (black bars top/bottom)
			NewW = ViewW;
			NewH = FMath::RoundToInt(static_cast<float>(ViewW) / ScreenAspect);
			OffY += (ViewH - NewH) / 2;
		}

		const FIntRect ConstrainedRect(OffX, OffY, OffX + NewW, OffY + NewH);
		InOutProjectionData.SetConstrainedViewRectangle(ConstrainedRect);
	}
}