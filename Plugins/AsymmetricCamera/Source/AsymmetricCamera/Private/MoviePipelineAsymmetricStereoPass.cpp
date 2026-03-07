// Custom MRQ render pass for stereo rendering with AsymmetricCamera

#include "MoviePipelineAsymmetricStereoPass.h"
#include "AsymmetricCameraComponent.h"
#include "MoviePipeline.h"
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

DEFINE_LOG_CATEGORY_STATIC(LogAsymmetricStereoPass, Log, All);

namespace
{
	FString GetFFmpegCodecString(EFFmpegVideoCodec InCodec)
	{
		switch (InCodec)
		{
		case EFFmpegVideoCodec::H264: return TEXT("libx264");
		case EFFmpegVideoCodec::H265: return TEXT("libx265");
		default:                      return TEXT("libx264");
		}
	}

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

	FString GetFFmpegPixFmtForCodec(EFFmpegVideoCodec /*InCodec*/)
	{
		return TEXT("yuv420p");
	}

	FString GetFFmpegQualityArgs(EFFmpegVideoCodec /*InCodec*/, int32 InCRF)
	{
		return FString::Printf(TEXT("-crf %d"), InCRF);
	}

	FString GetStereoMetadataArgs(EFFmpegVideoCodec InCodec, EAsymmetricStereoLayout InLayout)
	{
		switch (InCodec)
		{
		case EFFmpegVideoCodec::H264:
			{
				// H.264 Frame Packing Arrangement SEI: 3=side-by-side, 4=top-bottom
				const int32 FramePackingType = (InLayout == EAsymmetricStereoLayout::SideBySide) ? 3 : 4;
				return FString::Printf(TEXT("-x264-params frame-packing=%d"), FramePackingType);
			}
		case EFFmpegVideoCodec::H265:
			{
				// x265 has no frame-packing CLI param; use MKV container stereo_mode metadata
				const TCHAR* StereoMode = (InLayout == EAsymmetricStereoLayout::SideBySide)
					? TEXT("side_by_side_left") : TEXT("top_bottom_left");
				return FString::Printf(TEXT("-metadata:s:v stereo_mode=%s"), StereoMode);
			}
		default:
			return FString();
		}
	}

	FString GetOutputFormat(EFFmpegVideoCodec InCodec, EFFmpegOutputFormat InFormat)
	{
		// H.265 forces MKV container for stereo_mode metadata support
		if (InCodec == EFFmpegVideoCodec::H265)
		{
			return TEXT("mkv");
		}
		return GetFFmpegFormatString(InFormat);
	}

	/** Resolve FFmpeg executable: user override > bundled binary > system PATH */
	FString ResolveFFmpegPath(const FFilePath& UserPath)
	{
		if (!UserPath.FilePath.IsEmpty() && FPaths::FileExists(UserPath.FilePath))
		{
			return UserPath.FilePath;
		}

		FString ModulePath = FPaths::GetPath(FModuleManager::Get().GetModuleFilename(TEXT("AsymmetricCamera")));
		FString PluginRoot  = FPaths::GetPath(FPaths::GetPath(ModulePath));
		FString BundledPath = FPaths::Combine(PluginRoot, TEXT("ThirdParty"), TEXT("FFmpeg"), TEXT("Win64"), TEXT("ffmpeg.exe"));
		FPaths::NormalizeFilename(BundledPath);

		if (FPaths::FileExists(BundledPath))
		{
			UE_LOG(LogAsymmetricStereoPass, Log, TEXT("Using bundled FFmpeg: %s"), *BundledPath);
			return BundledPath;
		}

		UE_LOG(LogAsymmetricStereoPass, Log, TEXT("No bundled FFmpeg at %s, falling back to system PATH."), *BundledPath);
		return TEXT("ffmpeg");
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

	if (!IsTemplate())
	{
		FString ModulePath  = FPaths::GetPath(FModuleManager::Get().GetModuleFilename(TEXT("AsymmetricCamera")));
		FString PluginRoot  = FPaths::GetPath(FPaths::GetPath(ModulePath));
		FString BundledPath = FPaths::Combine(PluginRoot, TEXT("ThirdParty"), TEXT("FFmpeg"), TEXT("Win64"), TEXT("ffmpeg.exe"));
		FPaths::NormalizeFilename(BundledPath);
		if (FPaths::FileExists(BundledPath))
		{
			FFmpegPath.FilePath = BundledPath;
		}
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// MRQ lifecycle
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
// Export lifecycle
// ─────────────────────────────────────────────────────────────────────────────

void UMoviePipelineAsymmetricStereoPass::BuildCompositeQueue()
{
	// MRQ provides GetOutputDataParams() which returns the complete output manifest
	// after all files have been written to disk. This is called from BeginExportImpl.
	//
	// Structure:
	//   FMoviePipelineOutputData
	//     .ShotData[]                           — one entry per shot
	//       .Shot                               — UMoviePipelineExecutorShot*
	//       .RenderPassData[PassIdentifier]
	//         .FilePaths[]                      — absolute paths, all frames
	//
	// We collect LeftEye and RightEye paths per shot, sort them (guarantees frame
	// order regardless of starting frame number or gaps), and build FShotCompositeRecord.

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
		// Use OuterName (shot section name) when available, fall back to index
		Record.ShotName = ShotOutput.Shot.IsValid() && !ShotOutput.Shot->OuterName.IsEmpty()
			? ShotOutput.Shot->OuterName
			: FString::Printf(TEXT("shot%02d"), ShotIdx);

		// Iterate all render passes for this shot and classify by eye name
		for (const auto& PassPair : ShotOutput.RenderPassData)
		{
			for (const FString& FilePath : PassPair.Value.FilePaths)
			{
				const FString FileName = FPaths::GetCleanFilename(FilePath);

				if (FileName.Contains(TEXT("LeftEye")))
				{
					Record.LeftEyePaths.Add(FilePath);
					if (Record.OutputDir.IsEmpty())
					{
						Record.OutputDir = FPaths::GetPath(FilePath);
					}
				}
				else if (FileName.Contains(TEXT("RightEye")))
				{
					Record.RightEyePaths.Add(FilePath);
					if (Record.OutputDir.IsEmpty())
					{
						Record.OutputDir = FPaths::GetPath(FilePath);
					}
				}
			}
		}

		// Sort guarantees ascending frame order regardless of number format or start offset
		Record.LeftEyePaths.Sort();
		Record.RightEyePaths.Sort();

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
		}

		++CurrentCompositeIndex;
	}

	// Launch the next shot if any remain
	if (CurrentCompositeIndex < CompositeQueue.Num())
	{
		LaunchFFmpegForShot(CompositeQueue[CurrentCompositeIndex]);
		return false;
	}

	// All shots done — clean up temporary concat list files
	for (const FString& TempFile : TempConcatFiles)
	{
		IFileManager::Get().Delete(*TempFile);
	}
	TempConcatFiles.Reset();

	bExportFinished = true;
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Multi-camera / stereo eye overrides
// ─────────────────────────────────────────────────────────────────────────────

int32 UMoviePipelineAsymmetricStereoPass::GetNumCamerasToRender() const
{
	return (StereoLayout != EAsymmetricStereoLayout::None) ? 2 : 1;
}

int32 UMoviePipelineAsymmetricStereoPass::GetCameraIndexForRenderPass(const int32 InCameraIndex) const
{
	// The base class returns -1 when bRenderAllCameras is false, which causes
	// CameraIndex to be -1 in GetCameraInfo, making both eyes render as left eye.
	// We always pass through the actual camera index so left (0) and right (1) are distinct.
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
	// Always get base camera data from PlayerCameraManager (single camera path)
	UE::MoviePipeline::FImagePassCameraViewData OutCameraData =
		UMoviePipelineImagePassBase::GetCameraInfo(InOutSampleState, OptPayload);

	if (StereoLayout == EAsymmetricStereoLayout::None)
	{
		return OutCameraData;
	}

	const int32 CameraIndex = InOutSampleState.OutputState.CameraIndex;
	const int32 EyeIdx      = GetEyeIndex(CameraIndex);
	const float EyeSign     = (EyeIdx == 0) ? -1.0f : 1.0f;

	// Calculate eye offset along screen right vector
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

	// Apply asymmetric off-axis projection if available.
	// ComponentEyeSeparation must be 0; IPD is controlled by this pass's EyeSeparation only.
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
	// Use the single-camera (PlayerCameraManager) path to avoid accessing the empty
	// SidecarCameras array which causes a crash in the deferred pass base.
	UMoviePipelineImagePassBase::BlendPostProcessSettings(InView, InOutSampleState, OptPayload);
}

#if WITH_EDITOR
FText UMoviePipelineAsymmetricStereoPass::GetDisplayText() const
{
	return NSLOCTEXT("MovieRenderPipeline", "AsymmetricStereoPass_DisplayName", "Asymmetric Stereo Pass");
}
#endif

// ─────────────────────────────────────────────────────────────────────────────
// Private helpers
// ─────────────────────────────────────────────────────────────────────────────

int32 UMoviePipelineAsymmetricStereoPass::GetEyeIndex(const int32 InCameraIndex) const
{
	return bSwapEyes ? (1 - InCameraIndex) : InCameraIndex;
}

FString UMoviePipelineAsymmetricStereoPass::WriteConcatList(
	const TArray<FString>& FilePaths, const FString& ListFilePath) const
{
	// FFmpeg concat demuxer format — one line per file, single-quoted path.
	// This approach is immune to frame number format, starting offset, and gaps.
	TArray<FString> Lines;
	Lines.Reserve(FilePaths.Num());
	for (const FString& Path : FilePaths)
	{
		FString Normalized = Path;
		FPaths::NormalizeFilename(Normalized);
		// Escape embedded single quotes for the concat list format
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

	// Write concat demuxer list files — one for each eye.
	// FFmpeg will read exactly the files listed, in order, regardless of their
	// frame numbers. No %04d pattern, no -start_number guessing needed.
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
		// Derive output extension from first left-eye file
		const FString Extension = FPaths::GetExtension(Record.LeftEyePaths[0], /*bIncludeDot=*/true);
		// %05d in the output path tells FFmpeg to write a numbered sequence
		OutputPath = FPaths::Combine(Record.OutputDir,
			FString::Printf(TEXT("stereo_%s_%s_%%05d%s"), *LayoutName, *Record.ShotName, *Extension));

		Args = FString::Printf(
			TEXT("-y -f concat -safe 0 -i \"%s\" -f concat -safe 0 -i \"%s\""
			     " -filter_complex \"[0:v][1:v]%s=inputs=2\" \"%s\""),
			*LeftListPath, *RightListPath, *FilterName, *OutputPath);
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

		Args = FString::Printf(
			TEXT("-y -f concat -safe 0 -framerate %s -i \"%s\""
			     " -f concat -safe 0 -framerate %s -i \"%s\""
			     " -filter_complex \"[0:v][1:v]%s=inputs=2\""
			     " -c:v %s %s -pix_fmt %s %s \"%s\""),
			*FrameRateStr, *LeftListPath,
			*FrameRateStr, *RightListPath,
			*FilterName,
			*Codec, *QualityArgs, *PixFmt, *StereoArgs,
			*OutputPath);
	}

	UE_LOG(LogAsymmetricStereoPass, Log,
		TEXT("Launching FFmpeg for shot '%s':\n  %s %s"), *Record.ShotName, *FFmpegExe, *Args);

	ActiveFFmpegProcess = FPlatformProcess::CreateProc(
		*FFmpegExe, *Args,
		/*bLaunchDetached=*/false,
		/*bLaunchHidden=*/false,
		/*bLaunchReallyHidden=*/false,
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
