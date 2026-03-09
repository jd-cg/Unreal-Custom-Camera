// MRQ 立体渲染 Pass 实现 — 每帧渲染左右眼，并调用 FFmpeg 合成 SBS/TB 输出

#include "MoviePipelineAsymmetricStereoPass.h"
#include "AsymmetricCameraComponent.h"
#include "MoviePipeline.h"
#include "MoviePipelineQueue.h"
#include "MoviePipelineOutputSetting.h"
#include "MoviePipelinePrimaryConfig.h"
#include "MovieRenderPipelineDataTypes.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/PlatformProcess.h"
#include "HAL/FileManager.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"

DEFINE_LOG_CATEGORY_STATIC(LogAsymmetricStereoPass, Log, All);

namespace
{
	// 获取 FFmpeg 编码器名称字符串
	FString GetFFmpegCodecString(EFFmpegVideoCodec InCodec)
	{
		switch (InCodec)
		{
		case EFFmpegVideoCodec::H264: return TEXT("libx264");
		case EFFmpegVideoCodec::H265: return TEXT("libx265");
		default:                      return TEXT("libx264");
		}
	}

	// 获取输出容器格式字符串（H.265 强制 MKV）
	FString GetFFmpegFormatString(EFFmpegOutputFormat InFormat)
	{
		switch (InFormat)
		{
		case EFFmpegOutputFormat::MP4: return TEXT("mp4");
		case EFFmpegOutputFormat::MOV: return TEXT("mov");
		case EFFmpegOutputFormat::MKV: return TEXT("mkv");
		case EFFmpegOutputFormat::AVI: return TEXT("avi");
		default:                       return TEXT("mp4");
		}
	}

	// 获取像素格式（固定 yuv420p，兼容最广）
	FString GetFFmpegPixFmtForCodec(EFFmpegVideoCodec /*InCodec*/)
	{
		return TEXT("yuv420p");
	}

	// 获取视频质量参数字符串
	// -preset slow：慢速编码，相同 CRF 下压缩率更高、质量更好
	FString GetFFmpegQualityArgs(EFFmpegVideoCodec /*InCodec*/, int32 InCRF)
	{
		return FString::Printf(TEXT("-crf %d -preset slow"), InCRF);
	}

	// 获取立体 3D 元数据参数
	// H.264：嵌入 Frame Packing SEI；H.265：使用 MKV 容器 stereo_mode 元数据
	FString GetStereoMetadataArgs(EFFmpegVideoCodec InCodec, EAsymmetricStereoLayout InLayout)
	{
		switch (InCodec)
		{
		case EFFmpegVideoCodec::H264:
			{
				// Frame Packing Arrangement SEI: type 3=SBS, type 4=TB，VLC/PotPlayer 可自动识别
				const int32 FramePackingType = (InLayout == EAsymmetricStereoLayout::SideBySide) ? 3 : 4;
				return FString::Printf(TEXT("-x264-params frame-packing=%d"), FramePackingType);
			}
		case EFFmpegVideoCodec::H265:
			{
				// x265 不支持 frame-packing CLI 参数，改用 MKV 容器的 stereo_mode 元数据
				const TCHAR* StereoMode = (InLayout == EAsymmetricStereoLayout::SideBySide)
					? TEXT("side_by_side_left") : TEXT("top_bottom_left");
				return FString::Printf(TEXT("-metadata:s:v stereo_mode=%s"), StereoMode);
			}
		default:
			return FString();
		}
	}

	// 获取最终输出格式（H.265 强制使用 MKV 以支持 stereo_mode 元数据）
	FString GetOutputFormat(EFFmpegVideoCodec InCodec, EFFmpegOutputFormat InFormat)
	{
		if (InCodec == EFFmpegVideoCodec::H265)
		{
			return TEXT("mkv");
		}
		return GetFFmpegFormatString(InFormat);
	}

	/** 解析 FFmpeg 可执行文件路径。
	 *  始终转换为绝对路径，处理编辑器 FFilePath 属性可能存储相对路径的情况。
	 *  路径为空时回退到系统 PATH 中的 ffmpeg。 */
	FString ResolveFFmpegPath(const FFilePath& UserPath)
	{
		if (UserPath.FilePath.IsEmpty())
		{
			UE_LOG(LogAsymmetricStereoPass, Warning,
				TEXT("FFmpegPath is empty — falling back to system PATH. "
				     "Set an absolute path in the pass settings to avoid this."));
			return TEXT("ffmpeg");
		}

		// 确保路径始终是绝对路径，与编辑器存储方式无关
		FString Resolved = UserPath.FilePath;
		if (FPaths::IsRelative(Resolved))
		{
			Resolved = FPaths::ConvertRelativePathToFull(Resolved);
			UE_LOG(LogAsymmetricStereoPass, Warning,
				TEXT("FFmpegPath was relative, resolved to absolute: %s"), *Resolved);
		}

		FPaths::NormalizeFilename(Resolved);
		UE_LOG(LogAsymmetricStereoPass, Log, TEXT("Using FFmpeg: %s"), *Resolved);
		return Resolved;
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// Construction
// ─────────────────────────────────────────────────────────────────────────────

UMoviePipelineAsymmetricStereoPass::UMoviePipelineAsymmetricStereoPass()
	: UMoviePipelineDeferredPassBase()
{
	PassIdentifier = FMoviePipelinePassIdentifier("AsymmetricStereo");
	StereoLayout   = EAsymmetricStereoLayout::SideBySide;
	EyeSeparation  = 6.4f;
	bSwapEyes      = false;
	CompositeMode  = EAsymmetricCompositeMode::ImageSequence;
	VideoCodec     = EFFmpegVideoCodec::H264;
	CompositeQuality = 18;
	OutputFormat   = EFFmpegOutputFormat::MP4;
	bDeleteSourceAfterComposite = true;
	bDebugSaveConcatFiles = false;
	// FFmpegPath 默认留空，用户必须在 Pass 设置里填写绝对路径（或留空使用系统 PATH）
}

// ─────────────────────────────────────────────────────────────────────────────
// MRQ 生命周期
// ─────────────────────────────────────────────────────────────────────────────

void UMoviePipelineAsymmetricStereoPass::SetupImpl(const MoviePipeline::FMoviePipelineRenderPassInitSettings& InPassInitSettings)
{
	Super::SetupImpl(InPassInitSettings);

	CachedCameraComponent = nullptr;
	UWorld* World = GetWorld();
	if (World)
	{
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			UAsymmetricCameraComponent* Comp = It->FindComponentByClass<UAsymmetricCameraComponent>();
			if (Comp)
			{
				CachedCameraComponent = Comp;
				UE_LOG(LogAsymmetricStereoPass, Log, TEXT("Found AsymmetricCameraComponent on actor: %s"), *It->GetName());
				break;
			}
		}
	}

	if (!CachedCameraComponent.IsValid())
	{
		UE_LOG(LogAsymmetricStereoPass, Warning, TEXT("No AsymmetricCameraComponent found in scene. Stereo eye offset will use camera right vector only."));
	}

	// Reset composite state for this render session
	CompositeQueue.Reset();
	TempConcatFiles.Reset();
	CurrentCompositeIndex = 0;
	bExportFinished = true;
}

void UMoviePipelineAsymmetricStereoPass::TeardownImpl()
{
	CachedCameraComponent = nullptr;
	Super::TeardownImpl();
}

// ─────────────────────────────────────────────────────────────────────────────
// 导出生命周期（渲染完成后调用 FFmpeg 合成）
// ─────────────────────────────────────────────────────────────────────────────

void UMoviePipelineAsymmetricStereoPass::BuildCompositeQueue()
{
	// MRQ 渲染完成后，通过 GetOutputDataParams() 拿到完整的输出文件清单。
	// 数据结构：
	//   FMoviePipelineOutputData
	//     .ShotData[]                           — 每个 Shot 一条记录
	//       .Shot                               — UMoviePipelineExecutorShot*
	//       .RenderPassData[PassIdentifier]
	//         .FilePaths[]                      — 绝对路径，所有帧
	//
	// 按 Shot 收集 LeftEye 和 RightEye 文件路径，排序后构建 FShotCompositeRecord。
	// 排序保证帧顺序正确，不依赖起始帧号或文件名格式。

	UMoviePipeline* Pipeline = GetPipeline();
	if (!Pipeline)
	{
		return;
	}

	const FMoviePipelineOutputData OutputData = Pipeline->GetOutputDataParams();
	const FFrameRate EffectiveFrameRate = Pipeline->GetPipelinePrimaryConfig()
		->GetEffectiveFrameRate(Pipeline->GetTargetSequence());

	for (int32 ShotIdx = 0; ShotIdx < OutputData.ShotData.Num(); ++ShotIdx)
	{
		const FMoviePipelineShotOutputData& ShotOutput = OutputData.ShotData[ShotIdx];

		FShotCompositeRecord Record;
		Record.FrameRate = EffectiveFrameRate;
		// 用 Shot Section 名称作为输出文件名的一部分，空格替换为下划线
		const UMoviePipelineExecutorShot* Shot = ShotOutput.Shot.Get();
		FString RawShotName = (Shot && !Shot->OuterName.IsEmpty())
			? Shot->OuterName
			: FString::Printf(TEXT("shot%02d"), ShotIdx);
		RawShotName.ReplaceInline(TEXT(" "), TEXT("_"));
		Record.ShotName = RawShotName;

		// Iterate all render passes for this shot and classify by eye name
		for (const auto& PassPair : ShotOutput.RenderPassData)
		{
			for (const FString& FilePath : PassPair.Value.FilePaths)
			{
				const FString FileName = FPaths::GetCleanFilename(FilePath);

				if (FileName.Contains(TEXT("LeftEye")))
				{
					Record.LeftEyePaths.Add(FilePath);
				}
				else if (FileName.Contains(TEXT("RightEye")))
				{
					Record.RightEyePaths.Add(FilePath);
				}
			}
		}

		// OutputDir 取两眼目录的父目录（如 .../CamTest/11/），
		// 避免 concat 列表文件和 ffmpeg 日志落在某一眼的子目录下导致路径找不到。
		if (Record.LeftEyePaths.Num() > 0)
		{
			const FString LeftEyeDir = FPaths::GetPath(Record.LeftEyePaths[0]);
			Record.OutputDir = FPaths::GetPath(LeftEyeDir); // 上一级
		}
		else if (Record.RightEyePaths.Num() > 0)
		{
			const FString RightEyeDir = FPaths::GetPath(Record.RightEyePaths[0]);
			Record.OutputDir = FPaths::GetPath(RightEyeDir);
		}

		// Sort guarantees ascending frame order regardless of number format or start offset
		// 排序保证帧升序，无论文件名格式或起始帧号
		Record.LeftEyePaths.Sort();
		Record.RightEyePaths.Sort();

		// 从第一个左眼文件名中提取起始帧号（用于 ImageSequence 输出帧号对齐）。
		// MRQ 文件名末尾固定为 .NNNNN.ext 格式（ZeroPadFrameNumbers 控制位数，默认 4 位）。
		if (Record.LeftEyePaths.Num() > 0)
		{
			const FString FirstFile = FPaths::GetBaseFilename(Record.LeftEyePaths[0]);
			// 找最后一段纯数字（帧号部分），例如 "LeftEye.0015" → "0015"
			int32 DotIdx = INDEX_NONE;
			FirstFile.FindLastChar(TEXT('.'), DotIdx);
			if (DotIdx != INDEX_NONE)
			{
				const FString FramePart = FirstFile.Mid(DotIdx + 1);
				if (FramePart.IsNumeric())
				{
					Record.StartFrameNumber = FCString::Atoi(*FramePart);
				}
			}
		}

		if (Record.LeftEyePaths.Num() > 0 && Record.RightEyePaths.Num() > 0)
		{
			UE_LOG(LogAsymmetricStereoPass, Log,
				TEXT("Shot '%s': %d left + %d right eye frames queued for composite."),
				*Record.ShotName, Record.LeftEyePaths.Num(), Record.RightEyePaths.Num());
			CompositeQueue.Add(MoveTemp(Record));
		}
		else
		{
			UE_LOG(LogAsymmetricStereoPass, Warning,
				TEXT("Shot '%s': missing LeftEye or RightEye files (left=%d, right=%d). "
				     "Make sure {camera_name} is included in the MRQ output filename template."),
				*Record.ShotName, Record.LeftEyePaths.Num(), Record.RightEyePaths.Num());
		}
	}
}

void UMoviePipelineAsymmetricStereoPass::BeginExportImpl()
{
	if (CompositeMode == EAsymmetricCompositeMode::Disabled || StereoLayout == EAsymmetricStereoLayout::None)
	{
		return;
	}

	BuildCompositeQueue();

	if (CompositeQueue.Num() == 0)
	{
		UE_LOG(LogAsymmetricStereoPass, Warning, TEXT("No stereo shot pairs found — skipping FFmpeg composite."));
		bExportFinished = true;
		return;
	}

	bExportFinished = false;
	CurrentCompositeIndex = 0;
	LaunchFFmpegForShot(CompositeQueue[CurrentCompositeIndex]);
}

bool UMoviePipelineAsymmetricStereoPass::HasFinishedExportingImpl()
{
	if (bExportFinished)
	{
		return true;
	}

	// Still waiting for current FFmpeg process
	if (ActiveFFmpegProcess.IsValid() && FPlatformProcess::IsProcRunning(ActiveFFmpegProcess))
	{
		return false;
	}

	// Current process has finished — check result and advance queue
	if (ActiveFFmpegProcess.IsValid())
	{
		int32 ReturnCode = 0;
		FPlatformProcess::GetProcReturnCode(ActiveFFmpegProcess, &ReturnCode);
		FPlatformProcess::CloseProc(ActiveFFmpegProcess);
		ActiveFFmpegProcess.Reset();

		const FShotCompositeRecord& FinishedRecord = CompositeQueue[CurrentCompositeIndex];
		if (ReturnCode == 0)
		{
			UE_LOG(LogAsymmetricStereoPass, Log, TEXT("FFmpeg composite succeeded for shot '%s'."), *FinishedRecord.ShotName);
			if (bDeleteSourceAfterComposite)
			{
				DeleteSourceFiles(FinishedRecord);
			}
		}
		else
		{
			UE_LOG(LogAsymmetricStereoPass, Error,
				TEXT("FFmpeg exited with code %d for shot '%s', keeping source files."),
				ReturnCode, *FinishedRecord.ShotName);

			// Print the FFmpeg log file contents to make the error visible in Output Log
			const FString FFmpegLogPath = FPaths::Combine(FinishedRecord.OutputDir,
				FString::Printf(TEXT("_ffmpeg_log_%s.txt"), *FinishedRecord.ShotName));
			FString FFmpegOutput;
			if (FFileHelper::LoadFileToString(FFmpegOutput, *FFmpegLogPath) && !FFmpegOutput.IsEmpty())
			{
				UE_LOG(LogAsymmetricStereoPass, Error, TEXT("FFmpeg output:\n%s"), *FFmpegOutput);
			}
			else
			{
				UE_LOG(LogAsymmetricStereoPass, Warning, TEXT("FFmpeg log file not found or empty: %s"), *FFmpegLogPath);
			}
		}

		++CurrentCompositeIndex;
	}

	// Launch the next shot if any remain
	if (CurrentCompositeIndex < CompositeQueue.Num())
	{
		LaunchFFmpegForShot(CompositeQueue[CurrentCompositeIndex]);
		return false;
	}

	// Clean up or retain temp files based on debug setting
	if (TempConcatFiles.Num() > 0)
	{
		if (bDebugSaveConcatFiles)
		{
			UE_LOG(LogAsymmetricStereoPass, Log, TEXT("Debug mode: concat/log files retained for inspection:"));
			for (const FString& TempFile : TempConcatFiles)
			{
				UE_LOG(LogAsymmetricStereoPass, Log, TEXT("  %s"), *TempFile);
			}
		}
		else
		{
			for (const FString& TempFile : TempConcatFiles)
			{
				IFileManager::Get().Delete(*TempFile, /*bRequireExists=*/false);
			}
		}
	}

	// 显示完成通知
	if (CompositeQueue.Num() > 0)
	{
		FNotificationInfo Info(FText::FromString(
			FString::Printf(TEXT("立体合成完成：%d 个 Shot 已处理"), CompositeQueue.Num())));
		Info.ExpireDuration = 5.0f;
		Info.bUseLargeFont = false;
		Info.bFireAndForget = true;
		FSlateNotificationManager::Get().AddNotification(Info);

		UE_LOG(LogAsymmetricStereoPass, Log, TEXT("Stereo composite finished: %d shot(s) processed."), CompositeQueue.Num());
	}

	bExportFinished = true;
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// 多相机 / 立体眼别覆写
// ─────────────────────────────────────────────────────────────────────────────

int32 UMoviePipelineAsymmetricStereoPass::GetNumCamerasToRender() const
{
	return (StereoLayout != EAsymmetricStereoLayout::None) ? 2 : 1;
}

int32 UMoviePipelineAsymmetricStereoPass::GetCameraIndexForRenderPass(const int32 InCameraIndex) const
{
	// 基类在 bRenderAllCameras=false 时返回 -1，会导致两眼都按左眼渲染。
	// 直接透传实际索引，确保左眼（0）和右眼（1）各自独立。
	return InCameraIndex;
}

FString UMoviePipelineAsymmetricStereoPass::GetCameraName(const int32 InCameraIndex) const
{
	if (StereoLayout == EAsymmetricStereoLayout::None)
	{
		return Super::GetCameraName(InCameraIndex);
	}

	const int32 EyeIdx = GetEyeIndex(InCameraIndex);
	return (EyeIdx == 0) ? TEXT("LeftEye") : TEXT("RightEye");
}

FString UMoviePipelineAsymmetricStereoPass::GetCameraNameOverride(const int32 InCameraIndex) const
{
	return GetCameraName(InCameraIndex);
}

UE::MoviePipeline::FImagePassCameraViewData UMoviePipelineAsymmetricStereoPass::GetCameraInfo(
	FMoviePipelineRenderPassMetrics& InOutSampleState, IViewCalcPayload* OptPayload) const
{
	// 始终从 PlayerCameraManager 获取基础相机数据（单相机路径）
	UE::MoviePipeline::FImagePassCameraViewData OutCameraData =
		UMoviePipelineImagePassBase::GetCameraInfo(InOutSampleState, OptPayload);

	if (StereoLayout == EAsymmetricStereoLayout::None)
	{
		return OutCameraData;
	}

	const int32 CameraIndex = InOutSampleState.OutputState.CameraIndex;
	const int32 EyeIdx      = GetEyeIndex(CameraIndex);
	const float EyeSign     = (EyeIdx == 0) ? -1.0f : 1.0f;

	// 沿屏幕右方向计算眼睛偏移量
	FVector EyeOffset;
	if (CachedCameraComponent.IsValid())
	{
		FVector ScreenBL, ScreenBR, ScreenTL, ScreenTR;
		CachedCameraComponent->GetEffectiveScreenCorners(ScreenBL, ScreenBR, ScreenTL, ScreenTR);
		const FVector ScreenRight = (ScreenBR - ScreenBL).GetSafeNormal();
		EyeOffset = ScreenRight * EyeSign * (EyeSeparation * 0.5f);
	}
	else
	{
		const FVector RightVector = FRotationMatrix(OutCameraData.ViewInfo.Rotation).GetScaledAxis(EAxis::Y);
		EyeOffset = RightVector * EyeSign * (EyeSeparation * 0.5f);
	}

	OutCameraData.ViewInfo.Location += EyeOffset;

	// 应用非对称离轴投影（如果有 AsymmetricCameraComponent）。
	// ComponentEyeSeparation 必须为 0，IPD 只由本 Pass 的 EyeSeparation 控制。
	if (CachedCameraComponent.IsValid() && CachedCameraComponent->bUseAsymmetricProjection)
	{
		FVector    EyePosition = OutCameraData.ViewInfo.Location;
		FRotator   ProjViewRotation;
		FMatrix    ProjectionMatrix;

		if (CachedCameraComponent->CalculateOffAxisProjection(EyePosition, ProjViewRotation, ProjectionMatrix))
		{
			OutCameraData.bUseCustomProjectionMatrix = true;
			OutCameraData.CustomProjectionMatrix     = ProjectionMatrix;
		}
	}

	return OutCameraData;
}

void UMoviePipelineAsymmetricStereoPass::BlendPostProcessSettings(
	FSceneView* InView, FMoviePipelineRenderPassMetrics& InOutSampleState, IViewCalcPayload* OptPayload)
{
	// 使用单相机（PlayerCameraManager）路径，避免访问空的 SidecarCameras 数组导致崩溃。
	UMoviePipelineImagePassBase::BlendPostProcessSettings(InView, InOutSampleState, OptPayload);
}

#if WITH_EDITOR
FText UMoviePipelineAsymmetricStereoPass::GetDisplayText() const
{
	return NSLOCTEXT("MovieRenderPipeline", "AsymmetricStereoPass_DisplayName", "Asymmetric Stereo Pass");
}
#endif

// ─────────────────────────────────────────────────────────────────────────────
// 私有辅助函数
// ─────────────────────────────────────────────────────────────────────────────

int32 UMoviePipelineAsymmetricStereoPass::GetEyeIndex(const int32 InCameraIndex) const
{
	return bSwapEyes ? (1 - InCameraIndex) : InCameraIndex;
}

FString UMoviePipelineAsymmetricStereoPass::WriteConcatList(
	const TArray<FString>& FilePaths, const FString& ListFilePath) const
{
	// FFmpeg concat demuxer 格式：每行一个文件，路径用单引号括起来。
	// 这种方式对帧号格式、起始帧和文件名间隔均无要求。
	TArray<FString> Lines;
	Lines.Reserve(FilePaths.Num());
	for (const FString& Path : FilePaths)
	{
		FString Normalized = Path;
		FPaths::NormalizeFilename(Normalized);
		// 转义路径中的单引号（concat list 格式要求）
		Normalized.ReplaceInline(TEXT("'"), TEXT("'\\''"));
		Lines.Add(FString::Printf(TEXT("file '%s'"), *Normalized));
	}

	const FString Content = FString::Join(Lines, TEXT("\n")) + TEXT("\n");
	if (FFileHelper::SaveStringToFile(Content, *ListFilePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		return ListFilePath;
	}

	UE_LOG(LogAsymmetricStereoPass, Error, TEXT("Failed to write concat list: %s"), *ListFilePath);
	return FString();
}

void UMoviePipelineAsymmetricStereoPass::LaunchFFmpegForShot(const FShotCompositeRecord& Record)
{
	const FString FFmpegExe = ResolveFFmpegPath(FFmpegPath);

	// 写入左右眼 concat 列表文件。
	// FFmpeg 按列表中的顺序读取，不依赖帧号模式，不需要 -start_number。
	const FString LeftListPath  = FPaths::Combine(Record.OutputDir,
		FString::Printf(TEXT("_concat_left_%s.txt"),  *Record.ShotName));
	const FString RightListPath = FPaths::Combine(Record.OutputDir,
		FString::Printf(TEXT("_concat_right_%s.txt"), *Record.ShotName));

	if (WriteConcatList(Record.LeftEyePaths,  LeftListPath).IsEmpty() ||
		WriteConcatList(Record.RightEyePaths, RightListPath).IsEmpty())
	{
		UE_LOG(LogAsymmetricStereoPass, Error,
			TEXT("Failed to write concat lists for shot '%s', skipping."), *Record.ShotName);
		return;
	}

	TempConcatFiles.Add(LeftListPath);
	TempConcatFiles.Add(RightListPath);

	const FString FilterName   = (StereoLayout == EAsymmetricStereoLayout::SideBySide) ? TEXT("hstack") : TEXT("vstack");
	const FString LayoutName   = (StereoLayout == EAsymmetricStereoLayout::SideBySide) ? TEXT("SBS")    : TEXT("TB");

	// Exact fractional frame rate string, e.g. "24000/1001" for 23.976 fps
	const FString FrameRateStr = FString::Printf(TEXT("%d/%d"),
		Record.FrameRate.Numerator, Record.FrameRate.Denominator);

	FString Args;
	FString OutputPath;

	if (CompositeMode == EAsymmetricCompositeMode::ImageSequence)
	{
		// 从第一个左眼文件推导输出扩展名，保持和源文件格式一致
		const FString Extension = FPaths::GetExtension(Record.LeftEyePaths[0], /*bIncludeDot=*/true);
		OutputPath = FPaths::Combine(Record.OutputDir,
			FString::Printf(TEXT("stereo_%s_%s_%%05d%s"), *LayoutName, *Record.ShotName, *Extension));

		// JPEG 用 -q:v 1（质量最高，1-31 越小越好）；PNG/EXR 本身无损，不需要质量参数。
		const FString ExtLower = Extension.ToLower();
		const bool bIsJpeg = ExtLower == TEXT(".jpg") || ExtLower == TEXT(".jpeg");
		const FString QualityFlag = bIsJpeg ? TEXT("-q:v 1") : TEXT("");

		// concat demuxer 不支持 -framerate，帧率用 -r 在输出端指定。
		// -start_number 让输出帧序号与源文件帧号对齐（支持自定义起始帧）。
		Args = FString::Printf(
			TEXT("-y -f concat -safe 0 -i \"%s\" -f concat -safe 0 -i \"%s\""
			     " -filter_complex \"[0:v][1:v]%s=inputs=2\" -r %s %s -start_number %d \"%s\""),
			*LeftListPath, *RightListPath, *FilterName, *FrameRateStr, *QualityFlag,
			Record.StartFrameNumber, *OutputPath);
	}
	else
	{
		const FString Codec       = GetFFmpegCodecString(VideoCodec);
		const FString Fmt         = GetOutputFormat(VideoCodec, OutputFormat);
		const FString PixFmt      = GetFFmpegPixFmtForCodec(VideoCodec);
		const FString QualityArgs = GetFFmpegQualityArgs(VideoCodec, CompositeQuality);
		const FString StereoArgs  = GetStereoMetadataArgs(VideoCodec, StereoLayout);
		OutputPath = FPaths::Combine(Record.OutputDir,
			FString::Printf(TEXT("stereo_%s_%s.%s"), *LayoutName, *Record.ShotName, *Fmt));

		// concat demuxer 不支持 -framerate，帧率用 -r 在输出端指定
		Args = FString::Printf(
			TEXT("-y -f concat -safe 0 -i \"%s\""
			     " -f concat -safe 0 -i \"%s\""
			     " -filter_complex \"[0:v][1:v]%s=inputs=2\""
			     " -r %s -c:v %s %s -pix_fmt %s %s \"%s\""),
			*LeftListPath,
			*RightListPath,
			*FilterName,
			*FrameRateStr,
			*Codec, *QualityArgs, *PixFmt, *StereoArgs,
			*OutputPath);
	}

	// Log concat list contents to Output Log when debug mode is on
	if (bDebugSaveConcatFiles)
	{
		FString LeftContent, RightContent;
		FFileHelper::LoadFileToString(LeftContent,  *LeftListPath);
		FFileHelper::LoadFileToString(RightContent, *RightListPath);
		UE_LOG(LogAsymmetricStereoPass, Log, TEXT("Left concat list (%s):\n%s"),  *LeftListPath,  *LeftContent);
		UE_LOG(LogAsymmetricStereoPass, Log, TEXT("Right concat list (%s):\n%s"), *RightListPath, *RightContent);
	}

	// FFmpeg stdout/stderr 重定向到日志文件，方便排查错误。合成完成后根据调试开关决定是否删除。
	// 通过临时 bat 文件执行，彻底规避 cmd /c 引号嵌套解析问题（Args 内部含多处双引号路径）。
	const FString FFmpegLogPath = FPaths::Combine(Record.OutputDir,
		FString::Printf(TEXT("_ffmpeg_log_%s.txt"), *Record.ShotName));
	TempConcatFiles.Add(FFmpegLogPath);
	const FString BatPath = FPaths::Combine(Record.OutputDir,
		FString::Printf(TEXT("_ffmpeg_run_%s.bat"), *Record.ShotName));

	// bat 文件中 % 需要转义为 %%，否则 cmd 会把 %0 当作批处理参数导致路径破坏。
	// 例如 stereo_TB_%05d.jpeg 在 bat 中必须写成 stereo_TB_%%05d.jpeg。
	FString ArgsEscaped = Args;
	ArgsEscaped.ReplaceInline(TEXT("%"), TEXT("%%"));

	const FString BatContent = FString::Printf(
		TEXT("@echo off\r\n\"%s\" %s > \"%s\" 2>&1\r\n"),
		*FFmpegExe, *ArgsEscaped, *FFmpegLogPath);

	if (!FFileHelper::SaveStringToFile(BatContent, *BatPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		UE_LOG(LogAsymmetricStereoPass, Error,
			TEXT("Failed to write FFmpeg bat file: %s"), *BatPath);
		return;
	}
	TempConcatFiles.Add(BatPath);

	UE_LOG(LogAsymmetricStereoPass, Log,
		TEXT("Launching FFmpeg for shot '%s':\n  %s %s"), *Record.ShotName, *FFmpegExe, *Args);
	UE_LOG(LogAsymmetricStereoPass, Log, TEXT("FFmpeg output will be written to: %s"), *FFmpegLogPath);

	const FString CmdExe = TEXT("cmd.exe");
	const FString CmdArgs = FString::Printf(TEXT("/c \"%s\""), *BatPath);

	ActiveFFmpegProcess = FPlatformProcess::CreateProc(
		*CmdExe, *CmdArgs,
		/*bLaunchDetached=*/false,
		/*bLaunchHidden=*/true,
		/*bLaunchReallyHidden=*/true,
		nullptr, 0, nullptr, nullptr);

	if (!ActiveFFmpegProcess.IsValid())
	{
		UE_LOG(LogAsymmetricStereoPass, Error,
			TEXT("Failed to launch FFmpeg for shot '%s'. "
			     "Check FFmpegPath or run ThirdParty/FFmpeg/download_ffmpeg.ps1."),
			*Record.ShotName);
	}
}

void UMoviePipelineAsymmetricStereoPass::DeleteSourceFiles(const FShotCompositeRecord& Record) const
{
	int32 Deleted = 0;
	for (const FString& Path : Record.LeftEyePaths)
	{
		if (IFileManager::Get().Delete(*Path)) { ++Deleted; }
	}
	for (const FString& Path : Record.RightEyePaths)
	{
		if (IFileManager::Get().Delete(*Path)) { ++Deleted; }
	}
	UE_LOG(LogAsymmetricStereoPass, Log,
		TEXT("Deleted %d source eye files for shot '%s'."), Deleted, *Record.ShotName);
}
