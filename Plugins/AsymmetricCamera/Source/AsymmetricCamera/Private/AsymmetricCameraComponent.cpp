// 非对称相机组件实现

#include "AsymmetricCameraComponent.h"
#include "AsymmetricScreenComponent.h"
#include "AsymmetricViewExtension.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "SceneViewExtension.h"

UAsymmetricCameraComponent::UAsymmetricCameraComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	bUseAsymmetricProjection = true;
	EyeSeparation = 0.0f;
	EyeOffset = 0.0f;
	bShowDebugFrustum = true;
	bShowScreenOutline = true;
	bShowFrustumLines = true;
	bShowEyeHandle = true;
	bShowNearPlane = true;
	bShowLabels = true;
	bShowDebugInGame = false;
	bMatchViewportAspectRatio = true;
	TrackedActor = nullptr;
	ScreenComponent = nullptr;
	NearClip = 20.0f;
	FarClip = 0.0f; // 0 = 无限远（UE5 默认）
	bUseExternalData = false;
	ExternalEyePosition = FVector::ZeroVector;
	ExternalScreenBL = FVector(100.0f, -80.0f, -45.0f);
	ExternalScreenBR = FVector(100.0f,  80.0f, -45.0f);
	ExternalScreenTL = FVector(100.0f, -80.0f,  45.0f);
	ExternalScreenTR = FVector(100.0f,  80.0f,  45.0f);
}

void UAsymmetricCameraComponent::BeginPlay()
{
	Super::BeginPlay();

	// 没设屏幕组件的话自动找一个
	if (!ScreenComponent)
	{
		AActor* Owner = GetOwner();
		if (Owner)
		{
			ScreenComponent = Owner->FindComponentByClass<UAsymmetricScreenComponent>();
		}
	}

	// 注册视图扩展，用来覆盖玩家相机投影
	if (bUseAsymmetricProjection)
	{
		ViewExtension = FSceneViewExtensions::NewExtension<FAsymmetricViewExtension>(GetWorld(), this);
	}
}

void UAsymmetricCameraComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ViewExtension.Reset();
	Super::EndPlay(EndPlayReason);
}

void UAsymmetricCameraComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (bShowDebugInGame)
	{
		DrawDebugVisualization();
	}
}

FVector UAsymmetricCameraComponent::GetEyePosition() const
{
	if (bUseExternalData)
	{
		return ExternalEyePosition;
	}
	if (TrackedActor)
	{
		return TrackedActor->GetActorLocation();
	}
	return GetComponentLocation();
}

bool UAsymmetricCameraComponent::CalculateOffAxisProjection(
	const FVector& EyePosition,
	FRotator& OutViewRotation,
	FMatrix& OutProjectionMatrix)
{
	if (!bUseAsymmetricProjection)
	{
		return false;
	}

	// 拿屏幕四角（世界坐标）
	FVector WorldBL, WorldBR, WorldTL, WorldTR;
	GetEffectiveScreenCorners(WorldBL, WorldBR, WorldTL, WorldTR);

	if (bUseExternalData)
	{
		// 外部模式：从四角推导屏幕朝向
		FVector ScreenRight = (WorldBR - WorldBL).GetSafeNormal();
		FVector ScreenUp = (WorldTL - WorldBL).GetSafeNormal();
		FVector ScreenNormal = FVector::CrossProduct(ScreenRight, ScreenUp).GetSafeNormal();
		OutViewRotation = ScreenNormal.Rotation();
	}
	else
	{
		if (!ScreenComponent)
		{
			return false;
		}
		// 组件模式：直接用屏幕组件的旋转
		OutViewRotation = ScreenComponent->GetComponentRotation();
	}

	// 转到 Actor 局部空间
	const AActor* Owner = GetOwner();
	const FTransform LocalSpace = (Owner ? Owner->GetActorTransform() : FTransform::Identity);

	const FVector PA = LocalSpace.InverseTransformPositionNoScale(WorldBL); // 左下
	const FVector PB = LocalSpace.InverseTransformPositionNoScale(WorldBR); // 右下
	const FVector PC = LocalSpace.InverseTransformPositionNoScale(WorldTL); // 左上

	// 眼睛位置转到 Actor 局部空间
	FVector PE = LocalSpace.InverseTransformPositionNoScale(EyePosition);

	// 立体偏移：沿屏幕右方向偏移
	if (FMath::Abs(EyeSeparation) > SMALL_NUMBER)
	{
		FVector VR = (PB - PA).GetSafeNormal();
		PE += VR * (EyeOffset * EyeSeparation * 0.5f);
	}

	// 屏幕的正交基
	FVector VR = (PB - PA).GetSafeNormal(); // 右
	FVector VU = (PC - PA).GetSafeNormal(); // 上
	// 屏幕法线：叉积取反（nDisplay 约定，左手系）
	FVector VN = -FVector::CrossProduct(VR, VU).GetSafeNormal();

	// 眼睛到屏幕三个角的向量
	const FVector VA = PA - PE;
	const FVector VB = PB - PE;
	const FVector VC = PC - PE;

	// 眼睛到屏幕平面的距离
	const float Distance = -FVector::DotProduct(VA, VN);
	static const float MinScreenDistance = 10.0f;
	const float SafeDistance = (FMath::Abs(Distance) < MinScreenDistance) ? MinScreenDistance : Distance;

	// 把屏幕范围投影到近裁切面上
	const float Near = NearClip;
	const float Far = FarClip;
	const float NearOverDist = Near / SafeDistance;

	const float Left   = FVector::DotProduct(VR, VA) * NearOverDist;
	const float Right  = FVector::DotProduct(VR, VB) * NearOverDist;
	const float Bottom = FVector::DotProduct(VU, VA) * NearOverDist;
	const float Top    = FVector::DotProduct(VU, VC) * NearOverDist;

	// 用 nDisplay 的 MakeProjectionMatrix 公式构建投影矩阵：
	// 1) 标准左手系偏心投影
	// 2) 乘以 flipZ 得到 UE5 reversed-Z
	const float mx = 2.0f * Near / (Right - Left);
	const float my = 2.0f * Near / (Top - Bottom);
	const float ma = -(Right + Left) / (Right - Left);
	const float mb = -(Top + Bottom) / (Top - Bottom);

	// 支持无限远平面（Far <= 0 或 Far == Near）
	const bool bInfiniteFar = (Far <= 0.0f) || FMath::IsNearlyEqual(Near, Far);
	const float mc = bInfiniteFar ? (1.0f - SMALL_NUMBER) : (Far / (Far - Near));
	const float md = bInfiniteFar ? (-Near * (1.0f - SMALL_NUMBER)) : (-(Far * Near) / (Far - Near));
	const float me = 1.0f;

	// 标准左手系投影矩阵
	const FMatrix StandardLHS(
		FPlane(mx, 0.0f, 0.0f, 0.0f),
		FPlane(0.0f, my, 0.0f, 0.0f),
		FPlane(ma, mb, mc, me),
		FPlane(0.0f, 0.0f, md, 0.0f));

	// flipZ：反转 Z 轴，转成 UE5 的 reversed-Z
	static const FMatrix FlipZ(
		FPlane(1.0f, 0.0f, 0.0f, 0.0f),
		FPlane(0.0f, 1.0f, 0.0f, 0.0f),
		FPlane(0.0f, 0.0f, -1.0f, 0.0f),
		FPlane(0.0f, 0.0f, 1.0f, 1.0f));

	OutProjectionMatrix = StandardLHS * FlipZ;
	return true;
}

void UAsymmetricCameraComponent::GetEffectiveScreenCorners(
	FVector& OutBL, FVector& OutBR, FVector& OutTL, FVector& OutTR) const
{
	if (bUseExternalData)
	{
		OutBL = ExternalScreenBL;
		OutBR = ExternalScreenBR;
		OutTL = ExternalScreenTL;
		OutTR = ExternalScreenTR;
	}
	else if (ScreenComponent)
	{
		ScreenComponent->GetScreenCornersWorld(OutBL, OutBR, OutTL, OutTR);
	}
	else
	{
		OutBL = OutBR = OutTL = OutTR = GetComponentLocation();
	}
}

void UAsymmetricCameraComponent::SetExternalData(
	const FVector& EyePos, const FVector& BL, const FVector& BR,
	const FVector& TL, const FVector& TR)
{
	bUseExternalData = true;
	ExternalEyePosition = EyePos;
	ExternalScreenBL = BL;
	ExternalScreenBR = BR;
	ExternalScreenTL = TL;
	ExternalScreenTR = TR;
}

void UAsymmetricCameraComponent::DrawDebugVisualization() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	FVector PA, PB, PC, PD;
	GetEffectiveScreenCorners(PA, PB, PC, PD);

	// 外部模式下没有 ScreenComponent 也能画
	if (!bUseExternalData && !ScreenComponent)
	{
		return;
	}

	// 画屏幕边框
	if (bShowScreenOutline)
	{
		DrawDebugLine(World, PA, PB, FColor::Green, false, -1.0f, 0, 2.0f);
		DrawDebugLine(World, PB, PD, FColor::Green, false, -1.0f, 0, 2.0f);
		DrawDebugLine(World, PD, PC, FColor::Green, false, -1.0f, 0, 2.0f);
		DrawDebugLine(World, PC, PA, FColor::Green, false, -1.0f, 0, 2.0f);

		// 从屏幕中心画法线箭头
		FVector ScreenCenterWorld = (PA + PB + PC + PD) * 0.25f;
		FVector Normal = FVector::CrossProduct(PB - PA, PC - PA).GetSafeNormal();
		DrawDebugLine(World, ScreenCenterWorld, ScreenCenterWorld + Normal * 50.0f, FColor::Red, false, -1.0f, 0, 3.0f);
	}

	FVector EyePos = GetEyePosition();

	// 画眼睛位置
	if (bShowEyeHandle)
	{
		DrawDebugSphere(World, EyePos, 10.0f, 8, FColor::Yellow, false, -1.0f, 0, 2.0f);
	}

	// 画从眼睛到屏幕四角的视锥线
	if (bShowFrustumLines)
	{
		DrawDebugLine(World, EyePos, PA, FColor::Yellow, false, -1.0f, 0, 1.0f);
		DrawDebugLine(World, EyePos, PB, FColor::Yellow, false, -1.0f, 0, 1.0f);
		DrawDebugLine(World, EyePos, PC, FColor::Yellow, false, -1.0f, 0, 1.0f);
		DrawDebugLine(World, EyePos, PD, FColor::Yellow, false, -1.0f, 0, 1.0f);
	}

	// 画近裁切面
	if (bShowNearPlane)
	{
		FVector ScreenCenterPos = (PA + PB + PC + PD) * 0.25f;
		float DistanceToScreen = (ScreenCenterPos - EyePos).Size();

		if (NearClip < DistanceToScreen)
		{
			float NearRatio = NearClip / DistanceToScreen;
			FVector NearPA = EyePos + (PA - EyePos) * NearRatio;
			FVector NearPB = EyePos + (PB - EyePos) * NearRatio;
			FVector NearPC = EyePos + (PC - EyePos) * NearRatio;
			FVector NearPD = EyePos + (PD - EyePos) * NearRatio;

			DrawDebugLine(World, NearPA, NearPB, FColor::Red, false, -1.0f, 0, 1.0f);
			DrawDebugLine(World, NearPB, NearPD, FColor::Red, false, -1.0f, 0, 1.0f);
			DrawDebugLine(World, NearPD, NearPC, FColor::Red, false, -1.0f, 0, 1.0f);
			DrawDebugLine(World, NearPC, NearPA, FColor::Red, false, -1.0f, 0, 1.0f);
		}
	}
}

#if WITH_EDITOR
void UAsymmetricCameraComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (NearClip <= 0.0f)
	{
		NearClip = 0.01f;
	}
	if (FarClip > 0.0f && FarClip <= NearClip)
	{
		FarClip = NearClip + 100.0f;
	}
}
#endif
