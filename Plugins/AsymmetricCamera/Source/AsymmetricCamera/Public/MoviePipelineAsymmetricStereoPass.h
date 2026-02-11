// Custom MRQ render pass for stereo rendering with AsymmetricCamera

#pragma once

#include "CoreMinimal.h"
#include "MoviePipelineDeferredPasses.h"
#include "AsymmetricStereoTypes.h"
#include "MoviePipelineAsymmetricStereoPass.generated.h"

class UAsymmetricCameraComponent;

/**
 * MRQ render pass that renders left and right eye views for stereo output.
 * Uses the multi-camera mechanism to render each eye as a separate "camera",
 * then optionally calls FFmpeg to composite SBS/TB output after rendering.
 *
 * Usage:
 * 1. Add this pass to your MRQ job (replaces default Deferred Rendering)
 * 2. Set StereoLayout and EyeSeparation
 * 3. Include {camera_name} in output filename to distinguish LeftEye/RightEye
 * 4. Enable bAutoComposite to auto-merge with FFmpeg after render
 */
UCLASS(BlueprintType)
class ASYMMETRICCAMERA_API UMoviePipelineAsymmetricStereoPass : public UMoviePipelineDeferredPassBase
{
	GENERATED_BODY()

public:
	UMoviePipelineAsymmetricStereoPass();

	/** Stereo layout mode */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stereo", meta = (ToolTip = "立体布局模式。Mono=单目，Side by Side=左右并排，Top/Bottom=上下排列"))
	EAsymmetricStereoLayout StereoLayout;

	/** Inter-ocular distance in centimeters */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stereo", meta = (ClampMin = "0.0", EditCondition = "StereoLayout != EAsymmetricStereoLayout::None", ToolTip = "双眼间距（厘米）。默认 6.4cm 模拟人眼瞳距"))
	float EyeSeparation;

	/** Swap left and right eye output */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stereo", meta = (EditCondition = "StereoLayout != EAsymmetricStereoLayout::None", ToolTip = "交换左右眼输出。如果发现左右眼反了可以开启"))
	bool bSwapEyes;

	/** Automatically call FFmpeg to composite SBS/TB after rendering */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stereo|FFmpeg", meta = (EditCondition = "StereoLayout != EAsymmetricStereoLayout::None", ToolTip = "渲染完成后自动调用 FFmpeg 合成左右眼为一个视频文件"))
	bool bAutoComposite;

	/** Path to FFmpeg executable. Leave empty to use bundled or system PATH. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stereo|FFmpeg", meta = (EditCondition = "bAutoComposite && StereoLayout != EAsymmetricStereoLayout::None", FilePathFilter = "exe", ToolTip = "FFmpeg 可执行文件路径。留空则自动使用插件自带的 FFmpeg 或系统 PATH 中的 FFmpeg"))
	FFilePath FFmpegPath;

	/** Video codec for composite output */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stereo|FFmpeg", meta = (EditCondition = "bAutoComposite && StereoLayout != EAsymmetricStereoLayout::None", ToolTip = "合成视频编码器。H.264 兼容性最好，H.265 压缩率更高，ProRes 适合后期剪辑"))
	EFFmpegVideoCodec VideoCodec;

	/** CRF quality (0=lossless, 23=default, 51=worst). Lower is better. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stereo|FFmpeg", meta = (ClampMin = "0", ClampMax = "51", EditCondition = "bAutoComposite && StereoLayout != EAsymmetricStereoLayout::None", ToolTip = "视频质量 CRF 值。0=无损，18=高质量（推荐），23=默认，51=最差。数值越小质量越高、文件越大"))
	int32 CompositeQuality;

	/** Output container format */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stereo|FFmpeg", meta = (EditCondition = "bAutoComposite && StereoLayout != EAsymmetricStereoLayout::None", ToolTip = "输出容器格式。MP4 兼容性最好，MOV 适合 Apple 生态，MKV 支持更多编码格式"))
	EFFmpegOutputFormat OutputFormat;

	/** Delete left/right eye source sequences after successful composite */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stereo|FFmpeg", meta = (EditCondition = "bAutoComposite && StereoLayout != EAsymmetricStereoLayout::None", ToolTip = "合成成功后自动删除左右眼源图片序列，仅保留合成视频"))
	bool bDeleteSourceAfterComposite;

protected:
	// UMoviePipelineDeferredPassBase overrides
	virtual void SetupImpl(const MoviePipeline::FMoviePipelineRenderPassInitSettings& InPassInitSettings) override;
	virtual void TeardownImpl() override;

	virtual int32 GetNumCamerasToRender() const override;
	virtual int32 GetCameraIndexForRenderPass(const int32 InCameraIndex) const override;
	virtual FString GetCameraName(const int32 InCameraIndex) const override;
	virtual FString GetCameraNameOverride(const int32 InCameraIndex) const override;
	virtual UE::MoviePipeline::FImagePassCameraViewData GetCameraInfo(FMoviePipelineRenderPassMetrics& InOutSampleState, IViewCalcPayload* OptPayload = nullptr) const override;
	virtual void BlendPostProcessSettings(FSceneView* InView, FMoviePipelineRenderPassMetrics& InOutSampleState, IViewCalcPayload* OptPayload = nullptr) override;

	// UMoviePipelineSetting overrides - post-finalize export (runs after all files are written to disk)
	virtual void BeginExportImpl() override;
	virtual bool HasFinishedExportingImpl() override;

#if WITH_EDITOR
	virtual FText GetDisplayText() const override;
#endif

private:
	/** Cached reference to the AsymmetricCameraComponent in the scene */
	UPROPERTY(Transient)
	TWeakObjectPtr<UAsymmetricCameraComponent> CachedCameraComponent;

	/** Run FFmpeg to composite left/right eye sequences */
	void RunFFmpegComposite();

	/** Get the eye index accounting for bSwapEyes */
	int32 GetEyeIndex(const int32 InCameraIndex) const;

	/** Handle to the running FFmpeg process (for async tracking) */
	FProcHandle ActiveFFmpegProcess;

	/** Whether the FFmpeg export has completed */
	bool bExportFinished = true;

	/** Cached output directory (resolved during TeardownImpl while shot is still valid) */
	FString CachedOutputDir;

	/** Cached output framerate */
	int32 CachedFrameRate = 24;
};
