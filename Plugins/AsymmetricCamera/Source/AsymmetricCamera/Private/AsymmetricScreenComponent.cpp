// 投影屏幕组件实现

#include "AsymmetricScreenComponent.h"

UAsymmetricScreenComponent::UAsymmetricScreenComponent()
{
	// 默认：屏幕在根组件前方 100 单位，160x90（16:9）
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
	// 屏幕四角在组件局部空间（nDisplay 约定）：
	// 屏幕在 YZ 平面上，法线朝 +X
	// Y = 水平方向（宽），Z = 垂直方向（高）
	const float HW = ScreenWidth * 0.5f;
	const float HH = ScreenHeight * 0.5f;

	const FVector LocalBL(0.0f, -HW, -HH); // 左下
	const FVector LocalBR(0.0f,  HW, -HH); // 右下
	const FVector LocalTL(0.0f, -HW,  HH); // 左上
	const FVector LocalTR(0.0f,  HW,  HH); // 右上

	// 用位置 + 旋转变换到世界空间（不用缩放）
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
