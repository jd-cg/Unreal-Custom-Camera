// MRQ 立体渲染 Pass — 每帧渲染左右眼，并调用 FFmpeg 合成 SBS/TB 图片序列或视频

#pragma once

#include "CoreMinimal.h"
#include "MoviePipelineDeferredPasses.h"
#include "AsymmetricStereoTypes.h"
#include "Misc/FrameRate.h"
#include "Misc/Paths.h"
#include "MoviePipelineAsymmetricStereoPass.generated.h"

class UAsymmetricCameraComponent;

/**
 * 每个 Shot 的合成记录：从 MRQ 输出数据中提取的精确文件路径列表。
 * 在 BeginExportImpl 中从 GetOutputDataParams() 构建，不需要扫描目录或猜测文件名模式。
 */
struct FShotCompositeRecord
{
	TArray<FString> LeftEyePaths;   // 左眼帧文件绝对路径，按帧序排序
	TArray<FString> RightEyePaths;  // 右眼帧文件绝对路径，按帧序排序
	FString         OutputDir;      // 输出目录
	FFrameRate      FrameRate;      // 序列帧率（精确分数形式）
	FString         ShotName;       // Shot 名称（用于输出文件命名）
	int32           StartFrameNumber = 0; // 起始帧号（用于 ImageSequence 输出对齐）
};

/**
 * MRQ 立体渲染 Pass，渲染左右眼视图并自动合成 SBS/TB 输出。
 * 使用多相机机制将左右眼各渲染为独立"相机"，渲染完成后调用 FFmpeg 合成。
 *
 * 使用方法：
 * 1. 在 MRQ Job 的渲染 Pass 中添加此 Pass（替换默认 Deferred Rendering）
 * 2. 设置 StereoLayout 和 EyeSeparation
 * 3. 输出文件名模板中包含 {camera_name} 以区分 LeftEye/RightEye
 * 4. 设置 CompositeMode=ImageSequence 或 Video，渲染完成后自动调用 FFmpeg 合成
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

	/** 双眼间距（厘米），默认 6.4cm 模拟人眼瞳距 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stereo", meta = (ClampMin = "0.0", EditCondition = "StereoLayout != EAsymmetricStereoLayout::None", ToolTip = "双眼间距（厘米）。默认 6.4cm 模拟人眼瞳距"))
	float EyeSeparation;

	/** 交换左右眼输出（发现左右反了时开启） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stereo", meta = (EditCondition = "StereoLayout != EAsymmetricStereoLayout::None", ToolTip = "交换左右眼输出。如果发现左右眼反了可以开启"))
	bool bSwapEyes;

	/** 合成模式：Disabled=保留分离序列，ImageSequence=每帧合并图片，Video=合并视频 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stereo|FFmpeg",
		meta = (EditCondition = "StereoLayout != EAsymmetricStereoLayout::None",
			ToolTip = "Disabled=保留左右眼分离序列；Image Sequence=每帧输出一张合并图；Video=输出合并视频文件"))
	EAsymmetricCompositeMode CompositeMode;

	/** FFmpeg 可执行文件路径。
	 *  点击 "..." 浏览选择，或直接输入绝对路径。
	 *  留空则使用系统 PATH 中的 ffmpeg。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stereo|FFmpeg",
		meta = (EditCondition = "CompositeMode != EAsymmetricCompositeMode::Disabled && StereoLayout != EAsymmetricStereoLayout::None",
			FilePathFilter = "exe",
			ToolTip = "FFmpeg 可执行文件路径。点击 ... 按钮浏览选择，或直接输入绝对路径，例如 D:/tools/ffmpeg/bin/ffmpeg.exe。留空则使用系统 PATH 中的 ffmpeg。"))
	FFilePath FFmpegPath;

	/** 视频编码器：H.264 兼容性最好，H.265 压缩率更高 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stereo|FFmpeg",
		meta = (EditCondition = "CompositeMode == EAsymmetricCompositeMode::Video && StereoLayout != EAsymmetricStereoLayout::None",
			ToolTip = "合成视频编码器。H.264 兼容性最好，H.265 压缩率更高"))
	EFFmpegVideoCodec VideoCodec;

	/** CRF 质量值（0=无损，18=高质量推荐，23=默认，51=最差），数值越小质量越高 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stereo|FFmpeg",
		meta = (ClampMin = "0", ClampMax = "51",
			EditCondition = "CompositeMode == EAsymmetricCompositeMode::Video && StereoLayout != EAsymmetricStereoLayout::None",
			ToolTip = "视频质量 CRF 值。0=无损，18=高质量（推荐），23=默认，51=最差。数值越小质量越高、文件越大"))
	int32 CompositeQuality;

	/** 输出容器格式（MP4/MOV/MKV/AVI，H.265 强制使用 MKV） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stereo|FFmpeg",
		meta = (EditCondition = "CompositeMode == EAsymmetricCompositeMode::Video && StereoLayout != EAsymmetricStereoLayout::None",
			ToolTip = "输出容器格式。MP4 兼容性最好，MOV 适合 Apple 生态，MKV 支持更多编码格式"))
	EFFmpegOutputFormat OutputFormat;

	/** 合成成功后自动删除左右眼源图片序列（默认开启） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stereo|FFmpeg",
		meta = (EditCondition = "CompositeMode != EAsymmetricCompositeMode::Disabled && StereoLayout != EAsymmetricStereoLayout::None",
			ToolTip = "合成成功后自动删除左右眼源图片序列，仅保留合成结果"))
	bool bDeleteSourceAfterComposite;

	/** Keep concat list files and FFmpeg log files on disk for debugging.
	 *  When disabled (default), these temporary files are deleted after composite. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stereo|FFmpeg",
		meta = (EditCondition = "CompositeMode != EAsymmetricCompositeMode::Disabled && StereoLayout != EAsymmetricStereoLayout::None",
			ToolTip = "调试模式：保留 concat 列表文件和 FFmpeg 日志文件（_concat_*.txt / _ffmpeg_log_*.txt）。默认关闭，出现合成问题时可开启排查。"))
	bool bDebugSaveConcatFiles;

protected:
	// UMoviePipelineDeferredPassBase 接口覆写
	virtual void SetupImpl(const MoviePipeline::FMoviePipelineRenderPassInitSettings& InPassInitSettings) override;
	virtual void TeardownImpl() override;

	virtual int32 GetNumCamerasToRender() const override;
	virtual int32 GetCameraIndexForRenderPass(const int32 InCameraIndex) const override;
	virtual FString GetCameraName(const int32 InCameraIndex) const override;
	virtual FString GetCameraNameOverride(const int32 InCameraIndex) const override;
	virtual UE::MoviePipeline::FImagePassCameraViewData GetCameraInfo(FMoviePipelineRenderPassMetrics& InOutSampleState, IViewCalcPayload* OptPayload = nullptr) const override;
	virtual void BlendPostProcessSettings(FSceneView* InView, FMoviePipelineRenderPassMetrics& InOutSampleState, IViewCalcPayload* OptPayload = nullptr) override;

	// 导出后处理 — 全部文件写入完成后执行 FFmpeg 合成
	virtual void BeginExportImpl() override;
	virtual bool HasFinishedExportingImpl() override;

#if WITH_EDITOR
	virtual FText GetDisplayText() const override;
#endif

private:
	/** 场景中 AsymmetricCameraComponent 的缓存引用 */
	UPROPERTY(Transient)
	TWeakObjectPtr<UAsymmetricCameraComponent> CachedCameraComponent;

	/** 获取考虑 bSwapEyes 后的实际眼别索引（0=左，1=右） */
	int32 GetEyeIndex(const int32 InCameraIndex) const;

	// ── FFmpeg 合成队列 ──────────────────────────────────────────────────────

	/** 从 MRQ 输出数据构建合成队列（在 BeginExportImpl 中调用） */
	void BuildCompositeQueue();

	/** 启动 FFmpeg 处理 CompositeQueue[CurrentCompositeIndex] 对应的 Shot */
	void LaunchFFmpegForShot(const FShotCompositeRecord& Record);

	/** 写入 concat demuxer 列表文件；成功返回路径，失败返回空字符串 */
	FString WriteConcatList(const TArray<FString>& FilePaths, const FString& ListFilePath) const;

	/** 删除已完成 Shot 的左右眼源文件 */
	void DeleteSourceFiles(const FShotCompositeRecord& Record) const;

	/** 每个 Shot 的合成记录，在 BeginExportImpl 中从 MRQ 输出数据构建 */
	TArray<FShotCompositeRecord> CompositeQueue;

	/** 当前正在运行的 FFmpeg 进程句柄 */
	FProcHandle ActiveFFmpegProcess;

	/** 当前正在合成的 Shot 在 CompositeQueue 中的索引 */
	int32 CurrentCompositeIndex = 0;

	/** 所有 FFmpeg 导出是否已完成 */
	bool bExportFinished = true;

	/** 写入磁盘的临时 concat 列表文件，所有 Shot 完成后统一清理 */
	TArray<FString> TempConcatFiles;
};
