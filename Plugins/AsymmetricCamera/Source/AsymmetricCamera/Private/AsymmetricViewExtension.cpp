// 场景视图扩展实现，在这里覆盖投影矩阵

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

	// 用屏幕旋转构建 ViewRotationMatrix，和 LocalPlayer.cpp:1244 一样：
	//   FInverseRotationMatrix(ViewRotation) * SwizzleMatrix
	static const FMatrix SwizzleMatrix(
		FPlane(0, 0, 1, 0),
		FPlane(1, 0, 0, 0),
		FPlane(0, 1, 0, 0),
		FPlane(0, 0, 0, 1)
	);
	const FMatrix ViewRotationMatrix = FInverseRotationMatrix(ViewRotation) * SwizzleMatrix;

	// 调试日志（只打前 3 帧）
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

	// 约束视口比例匹配屏幕宽高比（防止拉伸）
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
			// 左右黑边（Pillarbox）
			NewH = ViewH;
			NewW = FMath::RoundToInt(static_cast<float>(ViewH) * ScreenAspect);
			OffX += (ViewW - NewW) / 2;
		}
		else
		{
			// 上下黑边（Letterbox）
			NewW = ViewW;
			NewH = FMath::RoundToInt(static_cast<float>(ViewW) / ScreenAspect);
			OffY += (ViewH - NewH) / 2;
		}

		const FIntRect ConstrainedRect(OffX, OffY, OffX + NewW, OffY + NewH);
		InOutProjectionData.SetConstrainedViewRectangle(ConstrainedRect);
	}
}

void FAsymmetricViewExtension::SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView)
{
	// MRQ (Movie Render Queue) does not call SetupViewProjectionMatrix,
	// so we apply the asymmetric projection here for offline renders.
	if (!InView.bIsOfflineRender)
	{
		return;
	}

	if (!CameraComponent.IsValid()
		|| !CameraComponent->bUseAsymmetricProjection
		|| !CameraComponent->bEnableMRQSupport)
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

	// Set previous frame transform for motion blur support.
	// On the first frame we use current data (no motion blur for frame 0).
	if (bHasPreviousViewData)
	{
		InView.PreviousViewTransform = FTransform(PrevViewRotation.Quaternion(), PrevEyePosition);
	}

	// Cache current frame data for next frame
	PrevEyePosition = EyePosition;
	PrevViewRotation = ViewRotation;
	bHasPreviousViewData = true;

	InView.UpdateProjectionMatrix(ProjectionMatrix);

	InView.ViewLocation = EyePosition;
	InView.ViewRotation = ViewRotation;
	InView.UpdateViewMatrix();
}
