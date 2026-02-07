// 投影屏幕组件，定义投影平面的位置、朝向和大小

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "AsymmetricScreenComponent.generated.h"

/**
 * 投影屏幕组件，用于非对称/离轴投影。
 * 通过位置、旋转和尺寸来定义一块物理投影屏幕。
 * 编辑器里的线框可视化由 ComponentVisualizer 负责。
 *
 * 屏幕平面在组件局部空间的 YZ 平面上，法线朝 +X，
 * 和 nDisplay 的约定一致。
 */
UCLASS(ClassGroup = Camera, meta = (BlueprintSpawnableComponent))
class ASYMMETRICCAMERA_API UAsymmetricScreenComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UAsymmetricScreenComponent();

	/** 屏幕宽度（世界单位） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Asymmetric Camera|Screen", meta = (ClampMin = "1.0"))
	float ScreenWidth;

	/** 屏幕高度（世界单位） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Asymmetric Camera|Screen", meta = (ClampMin = "1.0"))
	float ScreenHeight;

	/** 获取屏幕尺寸，返回 (宽, 高) */
	UFUNCTION(BlueprintCallable, Category = "Asymmetric Camera|Screen")
	FVector2D GetScreenSize() const;

	/** 设置屏幕尺寸（世界单位） */
	UFUNCTION(BlueprintCallable, Category = "Asymmetric Camera|Screen")
	void SetScreenSize(const FVector2D& NewSize);

	/**
	 * 获取屏幕四角的世界坐标。
	 * 顺序：左下、右下、左上、右上。
	 */
	UFUNCTION(BlueprintCallable, Category = "Asymmetric Camera|Screen")
	void GetScreenCornersWorld(FVector& OutBottomLeft, FVector& OutBottomRight, FVector& OutTopLeft, FVector& OutTopRight) const;

	/**
	 * 获取屏幕四角在所属 Actor 局部空间的坐标。
	 * 顺序：左下、右下、左上、右上。
	 */
	UFUNCTION(BlueprintCallable, Category = "Asymmetric Camera|Screen")
	void GetScreenCornersLocal(FVector& OutBottomLeft, FVector& OutBottomRight, FVector& OutTopLeft, FVector& OutTopRight) const;
};
