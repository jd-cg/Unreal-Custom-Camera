// Copyright Epic Games, Inc. All Rights Reserved.

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
	FarClip = 0.0f; // 0 = infinite far plane (UE5 default)
}

void UAsymmetricCameraComponent::BeginPlay()
{
	Super::BeginPlay();

	// Auto-find screen component if not set
	if (!ScreenComponent)
	{
		AActor* Owner = GetOwner();
		if (Owner)
		{
			ScreenComponent = Owner->FindComponentByClass<UAsymmetricScreenComponent>();
		}
	}

	// Register view extension to override the player camera projection
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
	if (!bUseAsymmetricProjection || !ScreenComponent)
	{
		return false;
	}

	// ViewRotation = screen component's world rotation (nDisplay convention)
	OutViewRotation = ScreenComponent->GetComponentRotation();

	// Get screen corners in actor local space (nDisplay convention)
	const AActor* Owner = GetOwner();
	const FTransform LocalSpace = (Owner ? Owner->GetActorTransform() : FTransform::Identity);

	const FVector2D ScreenSize = ScreenComponent->GetScreenSize();
	const float HW = ScreenSize.X * 0.5f;
	const float HH = ScreenSize.Y * 0.5f;

	// Screen corners in screen component local space (YZ plane, normal +X)
	const FVector ScreenLoc = LocalSpace.InverseTransformPositionNoScale(ScreenComponent->GetComponentLocation());
	const FQuat ScreenQuat = LocalSpace.InverseTransformRotation(ScreenComponent->GetComponentQuat());

	const FVector PA = ScreenLoc + ScreenQuat.RotateVector(FVector(0.0f, -HW, -HH)); // Left-Bottom
	const FVector PB = ScreenLoc + ScreenQuat.RotateVector(FVector(0.0f,  HW, -HH)); // Right-Bottom
	const FVector PC = ScreenLoc + ScreenQuat.RotateVector(FVector(0.0f, -HW,  HH)); // Left-Top

	// Eye position in actor local space
	FVector PE = LocalSpace.InverseTransformPositionNoScale(EyePosition);

	// Apply stereo eye offset along screen right vector
	if (FMath::Abs(EyeSeparation) > SMALL_NUMBER)
	{
		FVector VR = (PB - PA).GetSafeNormal();
		PE += VR * (EyeOffset * EyeSeparation * 0.5f);
	}

	// Compute orthonormal basis for the screen
	FVector VR = (PB - PA).GetSafeNormal(); // Right
	FVector VU = (PC - PA).GetSafeNormal(); // Up
	// Screen normal: negate cross product (nDisplay convention, left-handed)
	FVector VN = -FVector::CrossProduct(VR, VU).GetSafeNormal();

	// Vectors from eye to screen corners
	const FVector VA = PA - PE;
	const FVector VB = PB - PE;
	const FVector VC = PC - PE;

	// Distance from eye to screen plane
	const float Distance = -FVector::DotProduct(VA, VN);
	static const float MinScreenDistance = 10.0f;
	const float SafeDistance = (FMath::Abs(Distance) < MinScreenDistance) ? MinScreenDistance : Distance;

	// Project screen extents onto near plane
	const float Near = NearClip;
	const float Far = FarClip;
	const float NearOverDist = Near / SafeDistance;

	const float Left   = FVector::DotProduct(VR, VA) * NearOverDist;
	const float Right  = FVector::DotProduct(VR, VB) * NearOverDist;
	const float Bottom = FVector::DotProduct(VU, VA) * NearOverDist;
	const float Top    = FVector::DotProduct(VU, VC) * NearOverDist;

	// Build projection matrix using nDisplay's exact MakeProjectionMatrix formula:
	// 1) Standard LHS off-center projection
	// 2) Multiply by flipZ to get UE5 reversed-Z
	const float mx = 2.0f * Near / (Right - Left);
	const float my = 2.0f * Near / (Top - Bottom);
	const float ma = -(Right + Left) / (Right - Left);
	const float mb = -(Top + Bottom) / (Top - Bottom);

	// Support unlimited far plane (Far <= 0 or Far == Near)
	const bool bInfiniteFar = (Far <= 0.0f) || FMath::IsNearlyEqual(Near, Far);
	const float mc = bInfiniteFar ? (1.0f - SMALL_NUMBER) : (Far / (Far - Near));
	const float md = bInfiniteFar ? (-Near * (1.0f - SMALL_NUMBER)) : (-(Far * Near) / (Far - Near));
	const float me = 1.0f;

	// Standard LHS projection matrix
	const FMatrix StandardLHS(
		FPlane(mx, 0.0f, 0.0f, 0.0f),
		FPlane(0.0f, my, 0.0f, 0.0f),
		FPlane(ma, mb, mc, me),
		FPlane(0.0f, 0.0f, md, 0.0f));

	// flipZ: invert Z-axis for UE5's reversed-Z convention
	static const FMatrix FlipZ(
		FPlane(1.0f, 0.0f, 0.0f, 0.0f),
		FPlane(0.0f, 1.0f, 0.0f, 0.0f),
		FPlane(0.0f, 0.0f, -1.0f, 0.0f),
		FPlane(0.0f, 0.0f, 1.0f, 1.0f));

	OutProjectionMatrix = StandardLHS * FlipZ;
	return true;
}

void UAsymmetricCameraComponent::DrawDebugVisualization() const
{
	UWorld* World = GetWorld();
	if (!World || !ScreenComponent)
	{
		return;
	}

	FVector PA, PB, PC, PD;
	ScreenComponent->GetScreenCornersWorld(PA, PB, PC, PD);

	// Draw screen outline
	if (bShowScreenOutline)
	{
		DrawDebugLine(World, PA, PB, FColor::Green, false, -1.0f, 0, 2.0f);
		DrawDebugLine(World, PB, PD, FColor::Green, false, -1.0f, 0, 2.0f);
		DrawDebugLine(World, PD, PC, FColor::Green, false, -1.0f, 0, 2.0f);
		DrawDebugLine(World, PC, PA, FColor::Green, false, -1.0f, 0, 2.0f);

		// Normal arrow from screen center
		FVector ScreenCenterWorld = (PA + PB + PC + PD) * 0.25f;
		FVector Normal = FVector::CrossProduct(PB - PA, PC - PA).GetSafeNormal();
		DrawDebugLine(World, ScreenCenterWorld, ScreenCenterWorld + Normal * 50.0f, FColor::Red, false, -1.0f, 0, 3.0f);
	}

	FVector EyePos = GetEyePosition();

	// Draw eye position
	if (bShowEyeHandle)
	{
		DrawDebugSphere(World, EyePos, 10.0f, 8, FColor::Yellow, false, -1.0f, 0, 2.0f);
	}

	// Draw frustum lines from eye to corners
	if (bShowFrustumLines)
	{
		DrawDebugLine(World, EyePos, PA, FColor::Yellow, false, -1.0f, 0, 1.0f);
		DrawDebugLine(World, EyePos, PB, FColor::Yellow, false, -1.0f, 0, 1.0f);
		DrawDebugLine(World, EyePos, PC, FColor::Yellow, false, -1.0f, 0, 1.0f);
		DrawDebugLine(World, EyePos, PD, FColor::Yellow, false, -1.0f, 0, 1.0f);
	}

	// Draw near plane
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