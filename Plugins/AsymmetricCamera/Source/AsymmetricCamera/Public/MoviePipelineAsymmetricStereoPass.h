// Custom MRQ render pass for stereo rendering with AsymmetricCamera

#pragma once

#include "CoreMinimal.h"
#include "MoviePipelineDeferredPasses.h"
#include "AsymmetricStereoTypes.h"
#include "Misc/FrameRate.h"
#include "MoviePipelineAsymmetricStereoPass.generated.h"

class UAsymmetricCameraComponent;

/**
 * Per-shot record: exact file paths extracted from MRQ output data in BeginExportImpl.
 * No directory scanning or filename pattern guessing required.
 */
struct FShotCompositeRecord
{
	TArray<FString> LeftEyePaths;   // Absolute paths, sorted in frame order
	TArray<FString> RightEyePaths;  // Absolute paths, sorted in frame order
	FString         OutputDir;
	FFrameRate      FrameRate;
	FString         ShotName;
};

/**
 * MRQ render pass that renders left and right eye views for stereo output.
 * Uses the multi-camera mechanism to render each eye as a separate "camera",
 * then optionally calls FFmpeg to composite SBS/TB output after rendering.
 *
 * Usage:
 * 1. Add this pass to your MRQ job (replaces default Deferred Rendering)
 * 2. Set StereoLayout and EyeSeparation
 * 3. Include {camera_name} in output filename to distinguish LeftEye/RightEye
 * 4. Set CompositeMode to ImageSequence or Video to auto-merge with FFmpeg after render
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

	/** FFmpeg composite mode: Disabled keeps separate eye sequences, ImageSequence outputs one merged image per frame, Video outputs a merged video file */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stereo|FFmpeg",
		meta = (EditCondition = "StereoLayout != EAsymmetricStereoLayout::None",
			ToolTip = "Disabled=保留左右眼分离序列；Image Sequence=每帧输出一张合并图；Video=输出合并视频文件"))
	EAsymmetricCompositeMode CompositeMode;

	/** Path to FFmpeg executable. Must be an absolute path (e.g. D:/tools/ffmpeg/bin/ffmpeg.exe).
	 *  Falls back to system PATH if left empty. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stereo|FFmpeg",
		meta = (EditCondition = "CompositeMode != EAsymmetricCompositeMode::Disabled && StereoLayout != EAsymmetricStereoLayout::None",
			FilePathFilter = "exe",
			ToolTip = "FFmpeg 可执行文件绝对路径，例如 D:/tools/ffmpeg/bin/ffmpeg.exe。留空则使用系统 PATH 中的 ffmpeg。"))
	FFilePath FFmpegPath;

	/** Video codec for composite output */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stereo|FFmpeg",
		meta = (EditCondition = "CompositeMode == EAsymmetricCompositeMode::Video && StereoLayout != EAsymmetricStereoLayout::None",
			ToolTip = "合成视频编码器。H.264 兼容性最好，H.265 压缩率更高"))
	EFFmpegVideoCodec VideoCodec;

	/** CRF quality (0=lossless, 23=default, 51=worst). Lower is better. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stereo|FFmpeg",
		meta = (ClampMin = "0", ClampMax = "51",
			EditCondition = "CompositeMode == EAsymmetricCompositeMode::Video && StereoLayout != EAsymmetricStereoLayout::None",
			ToolTip = "视频质量 CRF 值。0=无损，18=高质量（推荐），23=默认，51=最差。数值越小质量越高、文件越大"))
	int32 CompositeQuality;

	/** Output container format */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stereo|FFmpeg",
		meta = (EditCondition = "CompositeMode == EAsymmetricCompositeMode::Video && StereoLayout != EAsymmetricStereoLayout::None",
			ToolTip = "输出容器格式。MP4 兼容性最好，MOV 适合 Apple 生态，MKV 支持更多编码格式"))
	EFFmpegOutputFormat OutputFormat;

	/** Delete left/right eye source sequences after successful composite */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stereo|FFmpeg",
		meta = (EditCondition = "CompositeMode != EAsymmetricCompositeMode::Disabled && StereoLayout != EAsymmetricStereoLayout::None",
			ToolTip = "合成成功后自动删除左右眼源图片序列，仅保留合成结果"))
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

	// Post-finalize export — runs after all files are written to disk
	virtual void BeginExportImpl() override;
	virtual bool HasFinishedExportingImpl() override;

#if WITH_EDITOR
	virtual FText GetDisplayText() const override;
#endif

private:
	/** Cached reference to the AsymmetricCameraComponent in the scene */
	UPROPERTY(Transient)
	TWeakObjectPtr<UAsymmetricCameraComponent> CachedCameraComponent;

	/** Get the eye index accounting for bSwapEyes */
	int32 GetEyeIndex(const int32 InCameraIndex) const;

	// ── FFmpeg composite queue ───────────────────────────────────────────────

	/** Build CompositeQueue by inspecting MRQ output data (called from BeginExportImpl) */
	void BuildCompositeQueue();

	/** Launch FFmpeg for the shot at CompositeQueue[CurrentCompositeIndex] */
	void LaunchFFmpegForShot(const FShotCompositeRecord& Record);

	/** Write a concat demuxer list file; returns the path on success, empty string on failure */
	FString WriteConcatList(const TArray<FString>& FilePaths, const FString& ListFilePath) const;

	/** Delete source eye files for a completed shot record */
	void DeleteSourceFiles(const FShotCompositeRecord& Record) const;

	/** Per-shot records built in BeginExportImpl from MRQ output data */
	TArray<FShotCompositeRecord> CompositeQueue;

	/** Handle to the running FFmpeg process */
	FProcHandle ActiveFFmpegProcess;

	/** Index into CompositeQueue of the shot currently being composited */
	int32 CurrentCompositeIndex = 0;

	/** Whether all FFmpeg exports have completed */
	bool bExportFinished = true;

	/** Temporary concat list files written to disk; cleaned up after all shots finish */
	TArray<FString> TempConcatFiles;
};
