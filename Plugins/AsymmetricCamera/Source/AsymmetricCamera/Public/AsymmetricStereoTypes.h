// Stereo rendering types for AsymmetricCamera plugin

#pragma once

#include "CoreMinimal.h"
#include "AsymmetricStereoTypes.generated.h"

/** Stereo layout mode for asymmetric camera rendering */
UENUM(BlueprintType)
enum class EAsymmetricStereoLayout : uint8
{
	None        UMETA(DisplayName = "Mono"),
	SideBySide  UMETA(DisplayName = "Side by Side"),
	TopBottom   UMETA(DisplayName = "Top / Bottom")
};

/** FFmpeg composite output mode */
UENUM(BlueprintType)
enum class EAsymmetricCompositeMode : uint8
{
	Disabled        UMETA(DisplayName = "Disabled"),        // Keep separate LeftEye/RightEye sequences
	ImageSequence   UMETA(DisplayName = "Image Sequence"),  // One merged SBS/TB image per frame
	Video           UMETA(DisplayName = "Video")            // Merged video file
};

/** FFmpeg video codec for stereo composite output */
UENUM(BlueprintType)
enum class EFFmpegVideoCodec : uint8
{
	H264        UMETA(DisplayName = "H.264 (libx264)"),
	H265        UMETA(DisplayName = "H.265 (libx265)")
};

/** FFmpeg output container format */
UENUM(BlueprintType)
enum class EFFmpegOutputFormat : uint8
{
	MP4         UMETA(DisplayName = "MP4"),
	MOV         UMETA(DisplayName = "MOV"),
	MKV         UMETA(DisplayName = "MKV"),
	AVI         UMETA(DisplayName = "AVI")
};
