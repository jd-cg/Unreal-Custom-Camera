// Custom MRQ render pass for stereo rendering with AsymmetricCamera

#include "MoviePipelineAsymmetricStereoPass.h"
#include "AsymmetricCameraComponent.h"
#include "MoviePipeline.h"
#include "MoviePipelineOutputSetting.h"
#include "MovieRenderPipelineDataTypes.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Misc/Paths.h"
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

	FString GetFFmpegPixFmtForCodec(EFFmpegVideoCodec InCodec)
	{
		return TEXT("yuv420p");
	}

	FString GetFFmpegQualityArgs(EFFmpegVideoCodec InCodec, int32 InCRF)
	{
		return FString::Printf(TEXT("-crf %d"), InCRF);
	}

	FString GetFramePackingArgs(EFFmpegVideoCodec InCodec, EAsymmetricStereoLayout InLayout)
	{
		// H.264/H.265 Frame Packing Arrangement SEI: 3=side-by-side, 4=top-bottom
		const int32 FramePackingType = (InLayout == EAsymmetricStereoLayout::SideBySide) ? 3 : 4;

		switch (InCodec)
		{
		case EFFmpegVideoCodec::H264:
			return FString::Printf(TEXT("-x264-params frame-packing=%d"), FramePackingType);
		case EFFmpegVideoCodec::H265:
			return FString::Printf(TEXT("-x265-params frame-packing=%d"), FramePackingType);
		default:
			return FString();
		}
	}
}

UMoviePipelineAsymmetricStereoPass::UMoviePipelineAsymmetricStereoPass()
	: UMoviePipelineDeferredPassBase()
{
	PassIdentifier = FMoviePipelinePassIdentifier("AsymmetricStereo");
	StereoLayout = EAsymmetricStereoLayout::SideBySide;
	EyeSeparation = 6.4f;
	bSwapEyes = false;
	CompositeMode = EAsymmetricCompositeMode::ImageSequence;
	VideoCodec = EFFmpegVideoCodec::H264;
	CompositeQuality = 18;
	OutputFormat = EFFmpegOutputFormat::MP4;
	bDeleteSourceAfterComposite = true;

	// Set default FFmpegPath to bundled binary (relative to plugin)
	if (!IsTemplate())
	{
		FString ModulePath = FPaths::GetPath(FModuleManager::Get().GetModuleFilename(TEXT("AsymmetricCamera")));
		FString PluginRoot = FPaths::GetPath(FPaths::GetPath(ModulePath));
		FString BundledPath = FPaths::Combine(PluginRoot, TEXT("ThirdParty"), TEXT("FFmpeg"), TEXT("Win64"), TEXT("ffmpeg.exe"));
		FPaths::NormalizeFilename(BundledPath);
		if (FPaths::FileExists(BundledPath))
		{
			FFmpegPath.FilePath = BundledPath;
		}
	}
}

void UMoviePipelineAsymmetricStereoPass::SetupImpl(const MoviePipeline::FMoviePipelineRenderPassInitSettings& InPassInitSettings)
{
	Super::SetupImpl(InPassInitSettings);

	// Find AsymmetricCameraComponent in the world
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
}

void UMoviePipelineAsymmetricStereoPass::TeardownImpl()
{
	// Cache output directory and framerate while the shot is still valid.
	// BeginExportImpl runs after all shots are done, so GetCurrentShotIndex is no longer valid there.
	if (CompositeMode != EAsymmetricCompositeMode::Disabled && StereoLayout != EAsymmetricStereoLayout::None)
	{
		UMoviePipelineOutputSetting* OutputSetting = GetPipeline()->FindOrAddSettingForShot<UMoviePipelineOutputSetting>(
			GetPipeline()->GetActiveShotList()[GetPipeline()->GetCurrentShotIndex()]);

		if (OutputSetting)
		{
			CachedOutputDir = OutputSetting->OutputDirectory.Path;
			CachedOutputDir.ReplaceInline(TEXT("{project_dir}"), *FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()));
			FPaths::NormalizeDirectoryName(CachedOutputDir);
			CachedFrameRate = OutputSetting->bUseCustomFrameRate ? OutputSetting->OutputFrameRate.Numerator : 24;
		}
	}

	CachedCameraComponent = nullptr;
	Super::TeardownImpl();
}

void UMoviePipelineAsymmetricStereoPass::BeginExportImpl()
{
	// Called after all files have been finalized and written to disk.
	// This is the correct place to run FFmpeg composite (not TeardownImpl,
	// which runs before ProcessOutstandingFinishedFrames writes files).
	if (CompositeMode != EAsymmetricCompositeMode::Disabled && StereoLayout != EAsymmetricStereoLayout::None)
	{
		bExportFinished = false;
		RunFFmpegComposite();
	}
}

bool UMoviePipelineAsymmetricStereoPass::HasFinishedExportingImpl()
{
	if (bExportFinished)
	{
		return true;
	}

	// Check if the FFmpeg process is still running
	if (ActiveFFmpegProcess.IsValid())
	{
		if (FPlatformProcess::IsProcRunning(ActiveFFmpegProcess))
		{
			return false;
		}

		// Process finished - check return code
		int32 ReturnCode = 0;
		FPlatformProcess::GetProcReturnCode(ActiveFFmpegProcess, &ReturnCode);

		if (ReturnCode == 0 && bDeleteSourceAfterComposite)
		{
			UE_LOG(LogAsymmetricStereoPass, Log, TEXT("FFmpeg composite succeeded. Deleting source sequences..."));

			TArray<FString> LeftToDelete;
			IFileManager::Get().FindFiles(LeftToDelete, *FPaths::Combine(CachedOutputDir, TEXT("*LeftEye*")), true, false);
			for (const FString& File : LeftToDelete)
			{
				IFileManager::Get().Delete(*FPaths::Combine(CachedOutputDir, File));
			}

			TArray<FString> RightToDelete;
			IFileManager::Get().FindFiles(RightToDelete, *FPaths::Combine(CachedOutputDir, TEXT("*RightEye*")), true, false);
			for (const FString& File : RightToDelete)
			{
				IFileManager::Get().Delete(*FPaths::Combine(CachedOutputDir, File));
			}

			UE_LOG(LogAsymmetricStereoPass, Log, TEXT("Deleted %d left + %d right eye source files."), LeftToDelete.Num(), RightToDelete.Num());
		}
		else if (ReturnCode != 0)
		{
			UE_LOG(LogAsymmetricStereoPass, Error, TEXT("FFmpeg exited with code %d, keeping source files."), ReturnCode);
		}

		FPlatformProcess::CloseProc(ActiveFFmpegProcess);
		ActiveFFmpegProcess.Reset();
	}

	bExportFinished = true;
	return true;
}

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
	// We temporarily force NumCameras=1 logic by calling the grandparent
	UE::MoviePipeline::FImagePassCameraViewData OutCameraData = UMoviePipelineImagePassBase::GetCameraInfo(InOutSampleState, OptPayload);

	if (StereoLayout == EAsymmetricStereoLayout::None)
	{
		return OutCameraData;
	}

	// Determine which eye we're rendering
	const int32 CameraIndex = InOutSampleState.OutputState.CameraIndex;
	const int32 EyeIdx = GetEyeIndex(CameraIndex);
	const float EyeSign = (EyeIdx == 0) ? -1.0f : 1.0f;

	// Calculate eye offset along screen right vector (consistent with CalculateOffAxisProjection)
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
		// No AsymmetricCameraComponent — fall back to camera right vector
		const FVector RightVector = FRotationMatrix(OutCameraData.ViewInfo.Rotation).GetScaledAxis(EAxis::Y);
		EyeOffset = RightVector * EyeSign * (EyeSeparation * 0.5f);
	}

	OutCameraData.ViewInfo.Location += EyeOffset;

	// If we have an AsymmetricCameraComponent, use its projection
	if (CachedCameraComponent.IsValid() && CachedCameraComponent->bUseAsymmetricProjection)
	{
		FVector EyePosition = OutCameraData.ViewInfo.Location;
		FRotator ProjViewRotation;
		FMatrix ProjectionMatrix;

		// Temporarily set EyeOffset on the component for projection calculation
		UAsymmetricCameraComponent* Comp = CachedCameraComponent.Get();
		const float OrigEyeOffset = Comp->EyeOffset;
		Comp->EyeOffset = EyeSign;

		if (Comp->CalculateOffAxisProjection(EyePosition, ProjViewRotation, ProjectionMatrix))
		{
			OutCameraData.bUseCustomProjectionMatrix = true;
			OutCameraData.CustomProjectionMatrix = ProjectionMatrix;
		}

		// Restore original EyeOffset
		Comp->EyeOffset = OrigEyeOffset;
	}

	return OutCameraData;
}

void UMoviePipelineAsymmetricStereoPass::BlendPostProcessSettings(FSceneView* InView, FMoviePipelineRenderPassMetrics& InOutSampleState, IViewCalcPayload* OptPayload)
{
	// Our stereo eyes are virtual offsets from the player camera, not sidecar cameras
	// from Sequencer. Always use the single-camera (PlayerCameraManager) path to avoid
	// accessing the empty SidecarCameras array which causes a crash.
	UMoviePipelineImagePassBase::BlendPostProcessSettings(InView, InOutSampleState, OptPayload);
}

#if WITH_EDITOR
FText UMoviePipelineAsymmetricStereoPass::GetDisplayText() const
{
	return NSLOCTEXT("MovieRenderPipeline", "AsymmetricStereoPass_DisplayName", "Asymmetric Stereo Pass");
}
#endif

int32 UMoviePipelineAsymmetricStereoPass::GetEyeIndex(const int32 InCameraIndex) const
{
	// CameraIndex 0 = left eye, 1 = right eye (unless swapped)
	return bSwapEyes ? (1 - InCameraIndex) : InCameraIndex;
}

void UMoviePipelineAsymmetricStereoPass::RunFFmpegComposite()
{
	// Resolve FFmpeg path: user override > bundled binary > system PATH
	FString ResolvedFFmpegPath = FFmpegPath.FilePath;
	if (ResolvedFFmpegPath.IsEmpty())
	{
		FString ModulePath = FPaths::GetPath(FModuleManager::Get().GetModuleFilename(TEXT("AsymmetricCamera")));
		FString PluginRoot = FPaths::GetPath(FPaths::GetPath(ModulePath));
		FString BundledPath = FPaths::Combine(PluginRoot, TEXT("ThirdParty"), TEXT("FFmpeg"), TEXT("Win64"), TEXT("ffmpeg.exe"));
		FPaths::NormalizeFilename(BundledPath);

		if (FPaths::FileExists(BundledPath))
		{
			ResolvedFFmpegPath = BundledPath;
			UE_LOG(LogAsymmetricStereoPass, Log, TEXT("Using bundled FFmpeg: %s"), *ResolvedFFmpegPath);
		}
		else
		{
			ResolvedFFmpegPath = TEXT("ffmpeg");
			UE_LOG(LogAsymmetricStereoPass, Log, TEXT("No bundled FFmpeg found at %s, falling back to system PATH."), *BundledPath);
		}
	}

	// Use cached output directory (resolved during TeardownImpl while shot was still valid)
	if (CachedOutputDir.IsEmpty())
	{
		UE_LOG(LogAsymmetricStereoPass, Warning, TEXT("No cached output directory, skipping FFmpeg composite."));
		bExportFinished = true;
		return;
	}

	const FString& OutputDir = CachedOutputDir;

	UE_LOG(LogAsymmetricStereoPass, Log, TEXT("Output directory: %s"), *OutputDir);

	// Scan the output directory for LeftEye files to determine the actual naming pattern
	TArray<FString> LeftEyeFiles;
	IFileManager::Get().FindFiles(LeftEyeFiles, *FPaths::Combine(OutputDir, TEXT("*LeftEye*")), true, false);
	LeftEyeFiles.Sort();

	if (LeftEyeFiles.Num() == 0)
	{
		UE_LOG(LogAsymmetricStereoPass, Warning, TEXT("No LeftEye files found in %s, skipping FFmpeg composite."), *OutputDir);
		bExportFinished = true;
		return;
	}

	// Extract the naming pattern from the first file
	// Example: "seq_001.LeftEye.0000.jpeg" -> prefix="seq_001.LeftEye.", ext=".jpeg", pad=4
	const FString& FirstFile = LeftEyeFiles[0];
	const FString Extension = FPaths::GetExtension(FirstFile, true);

	FString BaseName = FPaths::GetBaseFilename(FirstFile);
	int32 LastDot = INDEX_NONE;
	BaseName.FindLastChar(TEXT('.'), LastDot);
	if (LastDot == INDEX_NONE)
	{
		UE_LOG(LogAsymmetricStereoPass, Warning, TEXT("Cannot parse frame number from filename: %s"), *FirstFile);
		bExportFinished = true;
		return;
	}

	FString FrameStr = BaseName.Mid(LastDot + 1);
	FString Prefix = BaseName.Left(LastDot + 1);
	const int32 ZeroPad = FrameStr.Len();

	// Build FFmpeg sequence pattern: "seq_001.LeftEye.%04d.jpeg"
	FString LeftSeqPattern = FString::Printf(TEXT("%s%%0%dd%s"), *Prefix, ZeroPad, *Extension);
	FString RightSeqPattern = LeftSeqPattern.Replace(TEXT("LeftEye"), TEXT("RightEye"));

	FString LeftInputPath = FPaths::Combine(OutputDir, LeftSeqPattern);
	FString RightInputPath = FPaths::Combine(OutputDir, RightSeqPattern);

	// Use cached framerate from TeardownImpl
	const int32 FrameRate = CachedFrameRate;

	const FString FilterName = (StereoLayout == EAsymmetricStereoLayout::SideBySide) ? TEXT("hstack") : TEXT("vstack");
	const FString LayoutName = (StereoLayout == EAsymmetricStereoLayout::SideBySide) ? TEXT("SBS") : TEXT("TB");

	FString Args;
	FString OutputPath;

	if (CompositeMode == EAsymmetricCompositeMode::ImageSequence)
	{
		// Output merged image sequence — same format as input (png/jpeg/exr etc.)
		FString OutputPattern = FString::Printf(TEXT("stereo_%s.%%0%dd%s"), *LayoutName, ZeroPad, *Extension);
		OutputPath = FPaths::Combine(OutputDir, OutputPattern);

		Args = FString::Printf(
			TEXT("-y -i \"%s\" -i \"%s\" -filter_complex \"[0:v][1:v]%s=inputs=2\" \"%s\""),
			*LeftInputPath, *RightInputPath, *FilterName, *OutputPath
		);
	}
	else
	{
		// Output video file
		const FString Codec = GetFFmpegCodecString(VideoCodec);
		const FString Fmt = GetFFmpegFormatString(OutputFormat);
		const FString PixFmt = GetFFmpegPixFmtForCodec(VideoCodec);
		const FString QualityArgs = GetFFmpegQualityArgs(VideoCodec, CompositeQuality);
		const FString FramePackingArgs = GetFramePackingArgs(VideoCodec, StereoLayout);
		OutputPath = FPaths::Combine(OutputDir, FString::Printf(TEXT("stereo_%s.%s"), *LayoutName, *Fmt));

		Args = FString::Printf(
			TEXT("-y -framerate %d -i \"%s\" -framerate %d -i \"%s\" -filter_complex \"[0:v][1:v]%s=inputs=2\" -c:v %s %s -pix_fmt %s %s \"%s\""),
			FrameRate, *LeftInputPath,
			FrameRate, *RightInputPath,
			*FilterName,
			*Codec,
			*QualityArgs,
			*PixFmt,
			*FramePackingArgs,
			*OutputPath
		);
	}

	UE_LOG(LogAsymmetricStereoPass, Log, TEXT("Running FFmpeg composite: %s %s"), *ResolvedFFmpegPath, *Args);

	// Launch FFmpeg asynchronously. HasFinishedExportingImpl() will poll for completion
	// and handle source file deletion when the process exits.
	ActiveFFmpegProcess = FPlatformProcess::CreateProc(
		*ResolvedFFmpegPath,
		*Args,
		false,  // bLaunchDetached
		false,  // bLaunchHidden
		false,  // bLaunchReallyHidden
		nullptr, 0, nullptr, nullptr);

	if (ActiveFFmpegProcess.IsValid())
	{
		UE_LOG(LogAsymmetricStereoPass, Log, TEXT("FFmpeg process launched. Output will be: %s"), *OutputPath);
	}
	else
	{
		UE_LOG(LogAsymmetricStereoPass, Error, TEXT("Failed to launch FFmpeg. Check FFmpegPath or run ThirdParty/FFmpeg/download_ffmpeg.ps1."));
		bExportFinished = true;
	}
}
