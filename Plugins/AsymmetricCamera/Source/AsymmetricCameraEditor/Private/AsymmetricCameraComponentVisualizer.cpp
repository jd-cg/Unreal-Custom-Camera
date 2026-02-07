// 编辑器组件可视化器实现

#include "AsymmetricCameraComponentVisualizer.h"
#include "AsymmetricCameraComponent.h"
#include "AsymmetricScreenComponent.h"
#include "SceneManagement.h"
#include "EditorViewportClient.h"
#include "ScopedTransaction.h"
#include "CanvasItem.h"
#include "CanvasTypes.h"
#include "Engine/Font.h"

IMPLEMENT_HIT_PROXY(HEyePositionProxy, HComponentVisProxy);

FAsymmetricCameraComponentVisualizer::FAsymmetricCameraComponentVisualizer()
	: bEyeSelected(false)
{
}

FAsymmetricCameraComponentVisualizer::~FAsymmetricCameraComponentVisualizer()
{
}

void FAsymmetricCameraComponentVisualizer::DrawVisualization(
	const UActorComponent* Component, const FSceneView* View,
	FPrimitiveDrawInterface* PDI)
{
	const UAsymmetricCameraComponent* CameraComponent =
		Cast<const UAsymmetricCameraComponent>(Component);
	if (!CameraComponent || !CameraComponent->bShowDebugFrustum)
	{
		return;
	}

	if (CameraComponent->bShowScreenOutline)
	{
		DrawScreenOutline(CameraComponent, PDI);
	}
	DrawFrustum(CameraComponent, PDI);
}

void FAsymmetricCameraComponentVisualizer::DrawScreenOutline(
	const UAsymmetricCameraComponent* CameraComponent,
	FPrimitiveDrawInterface* PDI) const
{
	if (!CameraComponent || !CameraComponent->ScreenComponent)
	{
		return;
	}

	FVector PA, PB, PC, PD;
	CameraComponent->ScreenComponent->GetScreenCornersWorld(PA, PB, PC, PD);

	const FLinearColor ScreenColor = FLinearColor::Green;
	const FLinearColor NormalColor = FLinearColor::Red;
	const float Thickness = 2.0f;
	const uint8 DepthPriority = SDPG_Foreground;

	PDI->SetHitProxy(nullptr);

	// 屏幕边框
	PDI->DrawLine(PA, PB, ScreenColor, DepthPriority, Thickness);
	PDI->DrawLine(PB, PD, ScreenColor, DepthPriority, Thickness);
	PDI->DrawLine(PD, PC, ScreenColor, DepthPriority, Thickness);
	PDI->DrawLine(PC, PA, ScreenColor, DepthPriority, Thickness);

	// 对角线
	const FLinearColor DiagonalColor = FLinearColor(0.0f, 0.5f, 1.0f);
	PDI->DrawLine(PA, PD, DiagonalColor, DepthPriority, Thickness * 0.5f);
	PDI->DrawLine(PB, PC, DiagonalColor, DepthPriority, Thickness * 0.5f);

	// 从屏幕中心画法线箭头
	FVector ScreenCenterPos = (PA + PB + PC + PD) * 0.25f;
	FVector Normal = FVector::CrossProduct(PB - PA, PC - PA).GetSafeNormal();
	FVector NormalEnd = ScreenCenterPos + Normal * 50.0f;
	PDI->DrawLine(ScreenCenterPos, NormalEnd, NormalColor, DepthPriority, Thickness * 1.5f);

	const float ArrowSize = 10.0f;
	FVector ArrowBase = NormalEnd - Normal * ArrowSize;
	FVector RightVec = FVector::CrossProduct(Normal, FVector::UpVector).GetSafeNormal();
	if (RightVec.SizeSquared() < 0.01f)
	{
		RightVec = FVector::CrossProduct(Normal, FVector::ForwardVector).GetSafeNormal();
	}
	FVector UpVec = FVector::CrossProduct(RightVec, Normal).GetSafeNormal();

	PDI->DrawLine(NormalEnd, ArrowBase + RightVec * ArrowSize * 0.3f, NormalColor, DepthPriority, Thickness);
	PDI->DrawLine(NormalEnd, ArrowBase - RightVec * ArrowSize * 0.3f, NormalColor, DepthPriority, Thickness);
	PDI->DrawLine(NormalEnd, ArrowBase + UpVec * ArrowSize * 0.3f, NormalColor, DepthPriority, Thickness);
	PDI->DrawLine(NormalEnd, ArrowBase - UpVec * ArrowSize * 0.3f, NormalColor, DepthPriority, Thickness);
}

void FAsymmetricCameraComponentVisualizer::DrawFrustum(
	const UAsymmetricCameraComponent* CameraComponent,
	FPrimitiveDrawInterface* PDI) const
{
	if (!CameraComponent || !CameraComponent->ScreenComponent)
	{
		return;
	}

	FVector EyePosition = CameraComponent->GetEyePosition();

	FVector PA, PB, PC, PD;
	CameraComponent->ScreenComponent->GetScreenCornersWorld(PA, PB, PC, PD);

	const FLinearColor FrustumColor = FLinearColor::Yellow;
	const FLinearColor SelectedColor = FLinearColor(1.0f, 0.8f, 0.0f);
	const float FrustumThickness = 1.5f;
	const uint8 DepthPriority = SDPG_World;

	// 从眼睛到屏幕四角的视锥线
	if (CameraComponent->bShowFrustumLines)
	{
		PDI->DrawLine(EyePosition, PA, FrustumColor, DepthPriority, FrustumThickness);
		PDI->DrawLine(EyePosition, PB, FrustumColor, DepthPriority, FrustumThickness);
		PDI->DrawLine(EyePosition, PC, FrustumColor, DepthPriority, FrustumThickness);
		PDI->DrawLine(EyePosition, PD, FrustumColor, DepthPriority, FrustumThickness);
	}

	// 眼睛位置手柄（带点击代理）
	if (CameraComponent->bShowEyeHandle)
	{
		PDI->SetHitProxy(new HEyePositionProxy(CameraComponent));

		bool bIsSelected = (SelectedComponent.Get() == CameraComponent && bEyeSelected);
		float PointSize = bIsSelected ? 15.0f : 10.0f;
		FLinearColor PointColor = bIsSelected ? SelectedColor : FrustumColor;

		DrawWireSphere(PDI, EyePosition, PointColor, 8.0f, 16, SDPG_Foreground, FrustumThickness);
		PDI->DrawPoint(EyePosition, PointColor, PointSize, SDPG_Foreground);

		PDI->SetHitProxy(nullptr);
	}

	// 近裁切面可视化
	if (CameraComponent->bShowNearPlane)
	{
		FVector ScreenCenterPos = (PA + PB + PC + PD) * 0.25f;
		float DistanceToScreen = (ScreenCenterPos - EyePosition).Size();

		if (CameraComponent->NearClip < DistanceToScreen)
		{
			float NearRatio = CameraComponent->NearClip / DistanceToScreen;
			FVector NearPA = EyePosition + (PA - EyePosition) * NearRatio;
			FVector NearPB = EyePosition + (PB - EyePosition) * NearRatio;
			FVector NearPC = EyePosition + (PC - EyePosition) * NearRatio;
			FVector NearPD = EyePosition + (PD - EyePosition) * NearRatio;

			const FLinearColor NearPlaneColor = FLinearColor::Red;
			PDI->DrawLine(NearPA, NearPB, NearPlaneColor, DepthPriority, 1.0f);
			PDI->DrawLine(NearPB, NearPD, NearPlaneColor, DepthPriority, 1.0f);
			PDI->DrawLine(NearPD, NearPC, NearPlaneColor, DepthPriority, 1.0f);
			PDI->DrawLine(NearPC, NearPA, NearPlaneColor, DepthPriority, 1.0f);
		}
	}
}

bool FAsymmetricCameraComponentVisualizer::VisProxyHandleClick(
	FEditorViewportClient* InViewportClient,
	HComponentVisProxy* VisProxy,
	const FViewportClick& Click)
{
	if (!VisProxy || !VisProxy->Component.IsValid())
	{
		return false;
	}

	const UAsymmetricCameraComponent* CameraComp =
		Cast<const UAsymmetricCameraComponent>(VisProxy->Component.Get());
	if (!CameraComp)
	{
		return false;
	}

	if (HEyePositionProxy* EyeProxy = HitProxyCast<HEyePositionProxy>(VisProxy))
	{
		bEyeSelected = true;
		SelectedComponent = const_cast<UAsymmetricCameraComponent*>(CameraComp);
		return true;
	}

	return false;
}

void FAsymmetricCameraComponentVisualizer::EndEditing()
{
	bEyeSelected = false;
	SelectedComponent.Reset();
}

bool FAsymmetricCameraComponentVisualizer::GetWidgetLocation(
	const FEditorViewportClient* ViewportClient,
	FVector& OutLocation) const
{
	if (!bEyeSelected || !SelectedComponent.IsValid())
	{
		return false;
	}

	OutLocation = SelectedComponent->GetEyePosition();
	return true;
}

bool FAsymmetricCameraComponentVisualizer::HandleInputDelta(
	FEditorViewportClient* ViewportClient,
	FViewport* Viewport,
	FVector& DeltaTranslate,
	FRotator& DeltaRotate,
	FVector& DeltaScale)
{
	if (!bEyeSelected || !SelectedComponent.IsValid())
	{
		return false;
	}

	if (DeltaTranslate.IsNearlyZero())
	{
		return true;
	}

	// 拖动眼睛位置（移动相机组件）
	const FScopedTransaction Transaction(FText::FromString(TEXT("Move Eye Position")));
	SelectedComponent->Modify();

	FVector CurrentLoc = SelectedComponent->GetComponentLocation();
	SelectedComponent->SetWorldLocation(CurrentLoc + DeltaTranslate);
	return true;
}

void FAsymmetricCameraComponentVisualizer::DrawVisualizationHUD(
	const UActorComponent* Component, const FViewport* Viewport,
	const FSceneView* View, FCanvas* Canvas)
{
	const UAsymmetricCameraComponent* CameraComponent =
		Cast<const UAsymmetricCameraComponent>(Component);
	if (!CameraComponent || !CameraComponent->bShowDebugFrustum
		|| !CameraComponent->bShowLabels
		|| !CameraComponent->ScreenComponent || !Canvas || !View)
	{
		return;
	}

	UFont* Font = GEngine->GetSmallFont();
	if (!Font)
	{
		return;
	}

	const float DPIScale = Canvas->GetDPIScale();

	auto WorldToCanvas = [&](const FVector& WorldPos, FVector2D& OutCanvasPos) -> bool
	{
		FVector2D PixelPos;
		if (View->WorldToPixel(WorldPos, PixelPos))
		{
			OutCanvasPos = PixelPos / DPIScale;
			return true;
		}
		return false;
	};

	FVector PA, PB, PC, PD;
	CameraComponent->ScreenComponent->GetScreenCornersWorld(PA, PB, PC, PD);
	FVector EyePosition = CameraComponent->GetEyePosition();

	// 角标签
	const TCHAR* CornerLabels[4] = { TEXT("BL"), TEXT("BR"), TEXT("TL"), TEXT("TR") };
	const FVector CornerPositions[4] = { PA, PB, PC, PD };

	for (int32 i = 0; i < 4; ++i)
	{
		FVector2D CanvasPos;
		if (WorldToCanvas(CornerPositions[i], CanvasPos))
		{
			FCanvasTextItem TextItem(
				FVector2D(CanvasPos.X + 6.0f, CanvasPos.Y - 10.0f),
				FText::FromString(CornerLabels[i]), Font, FLinearColor::Green);
			TextItem.EnableShadow(FLinearColor::Black);
			Canvas->DrawItem(TextItem);
		}
	}

	// 眼睛标签
	{
		FVector2D CanvasPos;
		if (WorldToCanvas(EyePosition, CanvasPos))
		{
			FCanvasTextItem TextItem(
				FVector2D(CanvasPos.X + 8.0f, CanvasPos.Y - 12.0f),
				FText::FromString(TEXT("Eye")), Font, FLinearColor::Yellow);
			TextItem.EnableShadow(FLinearColor::Black);
			Canvas->DrawItem(TextItem);
		}
	}

	// 屏幕信息
	{
		FVector ScreenCenterPos = (PA + PB + PC + PD) * 0.25f;
		FVector2D ScreenSize = CameraComponent->ScreenComponent->GetScreenSize();
		float Distance = (ScreenCenterPos - EyePosition).Size();

		FString InfoText = FString::Printf(TEXT("%.0f x %.0f  D=%.0f"),
			ScreenSize.X, ScreenSize.Y, Distance);

		FVector2D CanvasPos;
		if (WorldToCanvas(ScreenCenterPos, CanvasPos))
		{
			FCanvasTextItem TextItem(
				FVector2D(CanvasPos.X + 6.0f, CanvasPos.Y + 4.0f),
				FText::FromString(InfoText), Font, FLinearColor(0.7f, 1.0f, 0.7f));
			TextItem.EnableShadow(FLinearColor::Black);
			Canvas->DrawItem(TextItem);
		}
	}
}
