#include "M1MarkerAlignProbe.h"

#include "Engine/Engine.h"
#include "MRUKPassthroughCameraAccess.h"

#if defined(WITH_OPENCV) && WITH_OPENCV
#include "OpenCVHelper.h"
#include "RenderingThread.h"
#include "RHICommandList.h"
#include "TextureResource.h"
#include "PreOpenCVHeaders.h"
#include "opencv2/calib3d.hpp"
#include "PostOpenCVHeaders.h"
#endif

FVector2D FM1CameraModel::Project(const FVector& World) const
{
	const FVector Local = Pose.InverseTransformPosition(World);
	return PrincipalPx + FocalPx * FVector2D(Local.Y / Local.X, -Local.Z / Local.X);
}

FVector FM1CameraModel::Ray(const FVector2D& Pixel) const
{
	const FVector2D Slope = (Pixel - PrincipalPx) / FocalPx;
	return Pose.TransformVectorNoScale(FVector(1, Slope.X, -Slope.Y)).GetSafeNormal();
}

bool SolveM1Marker(const FVector2f (&Pixels)[4], double SideMm, const FM1CameraModel& Model,
	const FTransform& PawnWorld, FM1MarkerObservation& Out)
{
#if defined(WITH_OPENCV) && WITH_OPENCV
	if (!FMath::IsFinite(SideMm) || SideMm <= 0)
	{
		return false;
	}
	const double Half = SideMm / 20;
	// Marker corners (TL, TR, BR, BL), in cm
	const std::vector<cv::Point3d> Object = {
		{-Half, Half, 0}, {Half, Half, 0}, {Half, -Half, 0}, {-Half, -Half, 0},
	};
	std::vector<cv::Point2d> Image;
	for (int32 Index = 0; Index < 4; ++Index)
	{
		Image.emplace_back(Pixels[Index].X, Pixels[Index].Y);
	}
	const cv::Matx33d Intrinsics(Model.FocalPx.X, 0, Model.PrincipalPx.X,
		0, Model.FocalPx.Y, Model.PrincipalPx.Y, 0, 0, 1);
	cv::Vec3d Rotation;
	cv::Vec3d Translation;
	if (!cv::solvePnP(Object, Image, Intrinsics, cv::noArray(), Rotation, Translation, false, cv::SOLVEPNP_ITERATIVE)
		|| Translation[2] <= 0)
	{
		return false;
	}

	// OpenCV right/down/forward to UE forward/right/up (M1a rev 3)
	const auto ToUE = [](const cv::Vec3d& Point) { return FVector(Point[2], Point[0], -Point[1]); };
	cv::Matx33d MarkerRotation;
	cv::Rodrigues(Rotation, MarkerRotation);
	Out.CenterStage = PawnWorld.InverseTransformPosition(Model.Pose.TransformPosition(ToUE(Translation)));
	const FVector Arrow = PawnWorld.InverseTransformVectorNoScale(
		Model.Pose.TransformVectorNoScale(ToUE(MarkerRotation * cv::Vec3d(0, 1, 0))));
	const FVector Normal = PawnWorld.InverseTransformVectorNoScale(
		Model.Pose.TransformVectorNoScale(ToUE(MarkerRotation * cv::Vec3d(0, 0, 1))));
	Out.ArrowYawDeg = Arrow.Rotation().Yaw;
	Out.TiltDeg = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FMath::Abs(Normal.Z), 0.0, 1.0)));
	Out.ReprojectionPx = 0;
	Out.SidePx = TNumericLimits<double>::Max();
	for (int32 Index = 0; Index < 4; ++Index)
	{
		const FVector Local = ToUE(MarkerRotation * cv::Vec3d(Object[Index]) + Translation);
		if (Local.X <= 0)
		{
			return false;
		}
		const FVector2D Projected = Model.Project(Model.Pose.TransformPosition(Local));
		Out.ReprojectionPx += FVector2D::Distance(Projected, FVector2D(Pixels[Index])) / 4;
		Out.SidePx = FMath::Min(Out.SidePx, double(FVector2f::Distance(Pixels[Index], Pixels[(Index + 1) % 4])));
	}
	return !Out.CenterStage.ContainsNaN() && !Arrow.ContainsNaN() && !Normal.ContainsNaN() &&
		FMath::IsFinite(Out.ReprojectionPx);
#else
	return false;
#endif
}

void DetectM1Markers(FM1CameraFrame& Frame, TFunctionRef<void(int32, const FVector2f (&)[4])> Visit)
{
#if defined(WITH_OPENCV) && WITH_OPENCV
	if (Frame.Pixels.Num() != Frame.Size.X * Frame.Size.Y || Frame.Pixels.IsEmpty())
	{
		return;
	}
	TArray<FArucoMarker> Markers;
	FOpenCVHelper::IdentifyArucoMarkers(Frame.Pixels, Frame.Size, EArucoDictionary::DICT_4X4_50, Markers);
	TSet<int32> Seen;
	for (const auto& Marker : Markers)
	{
		if (!Seen.Contains(Marker.MarkerID))
		{
			Seen.Add(Marker.MarkerID);
			Visit(Marker.MarkerID, Marker.Corners);
		}
	}
#endif
}

bool AM1MarkerAlignProbe::StartCamera()
{
#if defined(WITH_OPENCV) && WITH_OPENCV
	auto* Subsystem = GEngine ? GEngine->GetEngineSubsystem<UMRUKPassthroughCameraAccessSubsystem>() : nullptr;
	if (!Subsystem)
	{
		return false;
	}
	FIntPoint Resolution = FIntPoint::ZeroValue;
	constexpr EMRUKCameraEye Eye = EMRUKCameraEye::Left;
	for (const FVector2D& Candidate : Subsystem->GetSupportedResolutions(Eye))
	{
		if (Candidate == FVector2D(1280, 960))
		{
			Resolution = FIntPoint(1280, 960);
			break;
		}
		if (Resolution.X == 0 && Candidate.X > 0 && Candidate.Y > 0 && Candidate.X * 3 == Candidate.Y * 4)
		{
			Resolution = FIntPoint(static_cast<int32>(Candidate.X), static_cast<int32>(Candidate.Y));
		}
	}
	if (Resolution.X == 0)
	{
		return false;
	}

	Camera = NewObject<UMRUKPassthroughCameraAccess>(this);
	Camera->CameraEye = Eye;
	Camera->ResolutionWidth = Resolution.X;
	Camera->ResolutionHeight = Resolution.Y;
	if (!Camera->Play())
	{
		Camera = nullptr;
		return false;
	}
	if (Camera->ResolutionWidth <= 0 || Camera->ResolutionHeight <= 0 ||
		Camera->ResolutionWidth * 3 != Camera->ResolutionHeight * 4)
	{
		StopCamera();
		return false;
	}
	LastTimestamp = Camera->GetTimestamp();
	bModelChecked = false;
	bModelValid = false;
	CameraTexture = NewObject<UMRUKPassthroughCameraAccessTexture>(this);
	CameraTexture->PassthroughCameraAccess = Camera;
	CameraTexture->UpdateResource();
	FlushRenderingCommands();
	Publish(FString::Printf(TEXT("Left camera started (%dx%d)"), Camera->ResolutionWidth, Camera->ResolutionHeight));
	return true;
#else
	return false;
#endif
}

bool AM1MarkerAlignProbe::ReadCameraFrame(FM1CameraFrame& Frame, bool bReadPixels)
{
#if defined(WITH_OPENCV) && WITH_OPENCV
	if (!Camera || Camera->GetCameraPlayState() != EMRUKCameraPlayState::Playing)
	{
		return false;
	}
	Frame.Timestamp = Camera->GetTimestamp();
	if (Frame.Timestamp <= LastTimestamp)
	{
		return false;
	}

	Frame.Size = FIntPoint(Camera->ResolutionWidth, Camera->ResolutionHeight);
	FQuat Orientation = FQuat::Identity;
	FVector Position = FVector::ZeroVector;
	Camera->GetCameraPose(Position, Orientation);
	if (!Orientation.IsNormalized() || Position.ContainsNaN())
	{
		return false;
	}
	Frame.Model.Pose = FTransform(Orientation, Position);
	if (bModelChecked)
	{
		Frame.Model.FocalPx = CameraFocalPx;
		Frame.Model.PrincipalPx = CameraPrincipalPx;
	}
	else
	{
		FVector Origin, TopLeft, BottomRight;
		Camera->ViewportPointToWorldSpaceRay(FVector2D(0, 0), Origin, TopLeft);
		Camera->ViewportPointToWorldSpaceRay(FVector2D(1, 1), Origin, BottomRight);
		TopLeft = Orientation.UnrotateVector(TopLeft);
		BottomRight = Orientation.UnrotateVector(BottomRight);
		if (TopLeft.X <= 0 || BottomRight.X <= 0)
		{
			return false;
		}
		const FVector2D MinSlope(TopLeft.Y / TopLeft.X, TopLeft.Z / TopLeft.X);
		const FVector2D MaxSlope(BottomRight.Y / BottomRight.X, BottomRight.Z / BottomRight.X);
		const FVector2D Span = MaxSlope - MinSlope;
		// MRUK ray Z is image down (M1 rev 1)
		if (Span.ContainsNaN() || FMath::IsNearlyZero(Span.X) || FMath::IsNearlyZero(Span.Y))
		{
			return false;
		}
		Frame.Model.FocalPx = FVector2D(Frame.Size.X / Span.X, Frame.Size.Y / FMath::Abs(Span.Y));
		Frame.Model.PrincipalPx = FVector2D(-MinSlope.X * Frame.Model.FocalPx.X,
			-MinSlope.Y * Frame.Size.Y / Span.Y);
		CameraFocalPx = Frame.Model.FocalPx;
		CameraPrincipalPx = Frame.Model.PrincipalPx;
	}
	if (!bReadPixels)
	{
		return true;
	}

	FTextureResource* Resource = CameraTexture->GetResource();
	const FIntPoint Size = Frame.Size;
	ENQUEUE_RENDER_COMMAND(M1MarkerReadPixels)([Resource, Size, &Pixels = Frame.Pixels](FRHICommandListImmediate& RHICmdList)
	{
		FRHITexture* Texture = Resource ? Resource->GetTexture2DRHI() : nullptr;
		if (Texture && Texture->GetSizeXY() == Size)
		{
			FReadSurfaceDataFlags Flags;
			Flags.SetLinearToGamma(false);
			RHICmdList.ReadSurfaceData(Texture, FIntRect(FIntPoint::ZeroValue, Size), Pixels, Flags);
		}
	});
	// Read the upload for this timestamp before accepting another pose (M1 D3)
	FlushRenderingCommands();
	if (Frame.Pixels.Num() != Size.X * Size.Y)
	{
		Publish(FString::Printf(TEXT("Camera texture readback unavailable (%d pixels)"), Frame.Pixels.Num()));
	}
	return true;
#else
	return false;
#endif
}

FVector2D AM1MarkerAlignProbe::ProjectMruk(const FVector& World) const
{
	return Camera ? Camera->WorldToViewportPoint(World) : FVector2D::ZeroVector;
}

void AM1MarkerAlignProbe::StopCamera()
{
#if defined(WITH_OPENCV) && WITH_OPENCV
	if (Camera)
	{
		// Finish pending uploads before MRUK releases the image buffer (M1a)
		FlushRenderingCommands();
		if (CameraTexture)
		{
			CameraTexture->UnregisterTexture();
			CameraTexture = nullptr;
		}
		Camera->Stop();
		Camera = nullptr;
	}
#endif
	bCameraStarted = false;
}
