#include "M1aCameraSpike.h"

#include "Components/InputComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "MRUKPassthroughCameraAccess.h"
#include "MRUtilityKitSubsystem.h"
#include "../Interaction/VBoRHandRigPawn.h"

#if defined(WITH_OPENCV) && WITH_OPENCV
#include "Algo/Sort.h"
#include "IXRTrackingSystem.h"
#include "OpenCVHelper.h"
#include "RenderingThread.h"
#include "RHICommandList.h"
#include "PreOpenCVHeaders.h"
#include "opencv2/calib3d.hpp"
#include "PostOpenCVHeaders.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogM1a, Log, All);

namespace
{
void ShowM1aMessage(const FString& Message, int32 Key = INDEX_NONE, float Seconds = 5.0f)
{
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(Key, Seconds, FColor::Cyan, Message);
	}
}

#if defined(WITH_OPENCV) && WITH_OPENCV
bool SolveMarkerCorners(const FVector2f (&Pixels)[4], double SideMm, const FTransform& CameraPose,
	const FVector2D& FocalPx, const FVector2D& PrincipalPx, FVector& Center, FVector (&Corners)[4])
{
	if (!FMath::IsFinite(SideMm) || SideMm <= 0.0 || FocalPx.ContainsNaN() ||
		FocalPx.X <= 0.0 || FocalPx.Y <= 0.0 || PrincipalPx.ContainsNaN())
	{
		return false;
	}

	const double HalfSideCm = SideMm / 20.0;
	// IPPE square order: top left, top right, bottom right, bottom left
	const std::vector<cv::Point3d> ObjectCorners = {
		{-HalfSideCm, HalfSideCm, 0}, {HalfSideCm, HalfSideCm, 0},
		{HalfSideCm, -HalfSideCm, 0}, {-HalfSideCm, -HalfSideCm, 0},
	};
	const std::vector<cv::Point2d> ImageCorners = {
		{Pixels[0].X, Pixels[0].Y}, {Pixels[1].X, Pixels[1].Y},
		{Pixels[2].X, Pixels[2].Y}, {Pixels[3].X, Pixels[3].Y},
	};
	const cv::Matx33d Intrinsics(FocalPx.X, 0, PrincipalPx.X, 0, FocalPx.Y, PrincipalPx.Y, 0, 0, 1);
	cv::Vec3d Rotation;
	cv::Vec3d Translation;
	if (!cv::solvePnP(ObjectCorners, ImageCorners, Intrinsics, cv::noArray(),
		Rotation, Translation, false, cv::SOLVEPNP_IPPE_SQUARE))
	{
		return false;
	}

	// OpenCV (right, down, forward) to MRUK projection axes (forward, right, down)
	const auto ToPoseSpace = [&CameraPose](const cv::Vec3d& Point)
	{
		return CameraPose.TransformPosition(FVector(Point[2], Point[0], Point[1]));
	};
	Center = ToPoseSpace(Translation);
	if (Translation[2] <= 0.0 || Center.ContainsNaN())
	{
		return false;
	}

	cv::Matx33d MarkerRotation;
	cv::Rodrigues(Rotation, MarkerRotation);
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Corners); ++Index)
	{
		Corners[Index] = ToPoseSpace(MarkerRotation * cv::Vec3d(ObjectCorners[Index]) + Translation);
		if (Corners[Index].ContainsNaN())
		{
			return false;
		}
	}
	return true;
}

bool ReadM1aPixels(UMRUKPassthroughCameraAccessTexture& Texture, FIntPoint Size, TArray<FColor>& Pixels)
{
	FTextureResource* Resource = Texture.GetResource();
	ENQUEUE_RENDER_COMMAND(M1aReadPixels)([Resource, Size, &Pixels](FRHICommandListImmediate& RHICmdList)
	{
		FRHITexture* RhiTexture = Resource ? Resource->GetTexture2DRHI() : nullptr;
		if (RhiTexture && RhiTexture->GetSizeXY() == Size)
		{
			FReadSurfaceDataFlags Flags;
			Flags.SetLinearToGamma(false);
			RHICmdList.ReadSurfaceData(RhiTexture, FIntRect(FIntPoint::ZeroValue, Size), Pixels, Flags);
		}
	});
	// Keep the MRUK pose and pixel readback on the same frame (M1 D3-D4)
	FlushRenderingCommands();
	return Pixels.Num() == Size.X * Size.Y;
}
#endif
}

AM1aCameraSpike::AM1aCameraSpike()
{
	PrimaryActorTick.bCanEverTick = true;
}

void AM1aCameraSpike::BeginPlay()
{
	Super::BeginPlay();
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		WorldLockSubsystem = GameInstance->GetSubsystem<UMRUKSubsystem>();
		if (UMRUKSubsystem* Subsystem = WorldLockSubsystem.Get())
		{
			bPreviousWorldLock = Subsystem->EnableWorldLock;
			Subsystem->EnableWorldLock = false;
		}
	}
	if (APlayerController* Controller = UGameplayStatics::GetPlayerController(this, 0))
	{
		EnableInput(Controller);
		InputComponent->BindKey(EKeys::M, IE_Pressed, this, &AM1aCameraSpike::ToggleCamera);
		InputComponent->BindKey(EKeys::T, IE_Pressed, this, &AM1aCameraSpike::StartTipSample);
	}
	ShowM1aMessage(TEXT("M: Toggle left camera   T: Record reference point with left index tip"));
}

void AM1aCameraSpike::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopCamera();
	if (UMRUKSubsystem* Subsystem = WorldLockSubsystem.Get())
	{
		Subsystem->EnableWorldLock = bPreviousWorldLock;
	}
	Super::EndPlay(EndPlayReason);
}

void AM1aCameraSpike::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const double Now = GetWorld()->GetRealTimeSeconds();
	if (Camera)
	{
		TickCamera(Now);
	}
	if (bSamplingTip)
	{
		TickTipSample(Now);
	}
	if (bHasMarker)
	{
		if (const APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0))
		{
			const FVector CenterWorld = Pawn->GetActorTransform().TransformPosition(MarkerStageCm);
			DrawDebugSphere(GetWorld(), CenterWorld, 1.0, 12, FColor::Green);
		}
	}
}

void AM1aCameraSpike::ToggleCamera()
{
#if defined(WITH_OPENCV) && WITH_OPENCV
	if (Camera)
	{
		StopCamera();
		ShowM1aMessage(TEXT("Left camera stopped"));
		return;
	}

	auto* Subsystem = GEngine->GetEngineSubsystem<UMRUKPassthroughCameraAccessSubsystem>();
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
		UE_LOG(LogM1a, Warning, TEXT("Camera start failed: no supported 4:3 resolution for left eye"));
		ShowM1aMessage(TEXT("No left camera 4:3 resolution. Check Link camera access."));
		return;
	}

	Camera = NewObject<UMRUKPassthroughCameraAccess>(this);
	Camera->CameraEye = Eye;
	Camera->ResolutionWidth = Resolution.X;
	Camera->ResolutionHeight = Resolution.Y;
	if (!Camera->Play())
	{
		UE_LOG(LogM1a, Warning, TEXT("Camera Play returned false (left eye, requested %dx%d)"), Resolution.X, Resolution.Y);
		ShowM1aMessage(TEXT("Camera start failed. Check Link camera permissions."));
		Camera = nullptr;
		return;
	}

	UE_LOG(LogM1a, Log, TEXT("Camera Play succeeded (left eye, requested %dx%d, actual %dx%d)"),
		Resolution.X, Resolution.Y, Camera->ResolutionWidth, Camera->ResolutionHeight);
	const FMRUKCameraIntrinsics Intrinsics = Camera->GetCameraIntrinsics();
	LensOffset = FTransform(Intrinsics.LensOffsetOrientation, Intrinsics.LensOffsetPosition);
	UE_LOG(LogM1a, Log, TEXT("Camera intrinsics: focal px %s | principal px %s | sensor px %s | lens offset cm %s | lens orientation %s"),
		*Intrinsics.FocalLength.ToString(), *Intrinsics.PrincipalPoint.ToString(), *Intrinsics.SensorResolution.ToString(),
		*Intrinsics.LensOffsetPosition.ToString(), *Intrinsics.LensOffsetOrientation.ToString());
	if (Camera->ResolutionWidth <= 0 || Camera->ResolutionHeight <= 0 ||
		Camera->ResolutionWidth * 3 != Camera->ResolutionHeight * 4)
	{
		UE_LOG(LogM1a, Warning, TEXT("Camera returned an invalid or non-4:3 resolution"));
		ShowM1aMessage(TEXT("Camera returned an invalid or non-4:3 resolution"));
		StopCamera();
		return;
	}

	// Ignore the last MRUK timestamp from a previous camera run
	LastTimestamp = Camera->GetTimestamp();
	CameraTexture = NewObject<UMRUKPassthroughCameraAccessTexture>(this);
	CameraTexture->PassthroughCameraAccess = Camera;
	CameraTexture->UpdateResource();
	FlushRenderingCommands();

	FramesRead = 0;
	FramesWithMarkers = 0;
	GoodMarkerFrames.Reset();
	IgnoredMarkerCounts.Reset();
	bSawNonBlackFrame = false;
	bHasStagePoseInInterval = false;
	bHasMarker = false;
	NextSummary = GetWorld()->GetRealTimeSeconds() + 1.0;
	ShowM1aMessage(TEXT("Left camera started"));
#else
	UE_LOG(LogM1a, Warning, TEXT("Camera spike requires OpenCV on Win64"));
	ShowM1aMessage(TEXT("Camera spike requires OpenCV on Win64"));
#endif
}

void AM1aCameraSpike::StopCamera()
{
#if defined(WITH_OPENCV) && WITH_OPENCV
	if (Camera)
	{
		// Finish MRUK's pending upload before releasing its CPU buffer
		FlushRenderingCommands();
		if (CameraTexture)
		{
			CameraTexture->UnregisterTexture();
			CameraTexture = nullptr;
		}
		Camera->Stop();
		Camera = nullptr;
		UE_LOG(LogM1a, Log, TEXT("Left camera stopped"));
	}
#endif
}

void AM1aCameraSpike::TickCamera(double Now)
{
#if defined(WITH_OPENCV) && WITH_OPENCV
	const FDateTime Timestamp = Camera->GetTimestamp();
	if (Camera->GetCameraPlayState() == EMRUKCameraPlayState::Playing && Timestamp != LastTimestamp)
	{
		LastTimestamp = Timestamp;
		const FIntPoint Size(Camera->ResolutionWidth, Camera->ResolutionHeight);
		TArray<FColor> Pixels;
		if (!ReadM1aPixels(*CameraTexture, Size, Pixels))
		{
			UE_LOG(LogM1a, Warning, TEXT("ReadSurfaceData failed: expected %dx%d, received %d pixels"), Size.X, Size.Y, Pixels.Num());
			ShowM1aMessage(TEXT("Camera texture readback failed"));
			StopCamera();
			return;
		}
		++FramesRead;
		bSawNonBlackFrame |= Pixels.ContainsByPredicate([](const FColor& Pixel)
		{
			return Pixel.R != 0 || Pixel.G != 0 || Pixel.B != 0;
		});
		FQuat CameraMrukOrientation;
		FVector CameraMrukCm;
		Camera->GetCameraPose(CameraMrukCm, CameraMrukOrientation);
		const FTransform CameraMruk(CameraMrukOrientation, CameraMrukCm);

		FQuat HmdStageOrientation;
		FVector HmdStageCm;
		const auto& XR = GEngine->XRSystem;
		// Current UE pose for Stage (M1a rev 1, hold head still during measurement)
		const bool bHasStagePose = CameraMrukOrientation.IsNormalized() && !CameraMrukCm.ContainsNaN()
			&& XR.IsValid() && XR->GetCurrentPose(IXRTrackingSystem::HMDDeviceId, HmdStageOrientation, HmdStageCm)
			&& HmdStageOrientation.IsNormalized() && !HmdStageCm.ContainsNaN();
		FTransform CameraStage;
		if (bHasStagePose)
		{
			const FTransform HmdStage(HmdStageOrientation, HmdStageCm);
			CameraStage = LensOffset * HmdStage;
			const FTransform HmdMruk = LensOffset.Inverse() * CameraMruk;
			MrukToStage = HmdMruk.Inverse() * HmdStage;
			CameraStageCm = CameraStage.GetLocation();
			bHasStagePoseInInterval = true;
		}

		TArray<FArucoMarker> Markers;
		FOpenCVHelper::IdentifyArucoMarkers(Pixels, Size, EArucoDictionary::DICT_4X4_50, Markers);
		const FArucoMarker* Marker = nullptr;
		for (const FArucoMarker& Candidate : Markers)
		{
			if (Candidate.MarkerID < 1 || Candidate.MarkerID > 4)
			{
				++IgnoredMarkerCounts.FindOrAdd(Candidate.MarkerID);
			}
			else if (!Marker)
			{
				Marker = &Candidate;
			}
		}
		if (Marker)
		{
			++FramesWithMarkers;
			if (bHasStagePose)
			{
				FVector Origin, TopLeft, BottomRight;
				Camera->ViewportPointToWorldSpaceRay(FVector2D(0, 0), Origin, TopLeft);
				Camera->ViewportPointToWorldSpaceRay(FVector2D(1, 1), Origin, BottomRight);
				TopLeft = CameraMrukOrientation.UnrotateVector(TopLeft);
				BottomRight = CameraMrukOrientation.UnrotateVector(BottomRight);

				// MRUK rays include the sensor crop (M1 D5)
				const FVector2D TopLeftSlope(TopLeft.Y / TopLeft.X, TopLeft.Z / TopLeft.X);
				const FVector2D BottomRightSlope(BottomRight.Y / BottomRight.X, BottomRight.Z / BottomRight.X);
				const FVector2D ImageSize(Size.X, Size.Y);
				const FVector2D FocalPx = ImageSize / (BottomRightSlope - TopLeftSlope);
				const FVector2D PrincipalPx = -TopLeftSlope * FocalPx;

				FVector CenterMruk;
				FVector CornersMruk[4];
				if (SolveMarkerCorners(Marker->Corners, MarkerSizeMm, CameraMruk,
					FocalPx, PrincipalPx, CenterMruk, CornersMruk))
				{
					double ErrorPx = 0.0;
					for (int32 Index = 0; Index < UE_ARRAY_COUNT(CornersMruk); ++Index)
					{
						const FVector2D ProjectedPx = Camera->WorldToViewportPoint(CornersMruk[Index]) * ImageSize;
						ErrorPx += FVector2D::Distance(ProjectedPx, FVector2D(Marker->Corners[Index]));
					}
					if (FMath::IsFinite(ErrorPx))
					{
						bHasMarker = true;
						MarkerId = Marker->MarkerID;
						const FVector CenterCamera = CameraMruk.InverseTransformPosition(CenterMruk);
						MarkerStageCm = CameraStage.TransformPosition(CenterCamera);
						ReprojectionPx = ErrorPx / UE_ARRAY_COUNT(CornersMruk);
						if (ReprojectionPx <= 1.0)
						{
							GoodMarkerFrames.Add(FGoodMarkerFrame{MarkerStageCm, CenterCamera.Size()});
						}
					}
				}
			}
		}
	}

	if (Now < NextSummary)
	{
		return;
	}
	const FString MarkerText = bHasMarker
		? FString::Printf(TEXT("last marker %d Stage cm %s | reprojection %.2f px"), MarkerId, *MarkerStageCm.ToString(), ReprojectionPx)
		: TEXT("no marker pose");
	const FString CameraText = bHasStagePoseInInterval ? CameraStageCm.ToString() : TEXT("unavailable");
	FString Summary = FString::Printf(TEXT("Frames %d | recognized %d | %s | camera Stage cm %s"),
		FramesRead, FramesWithMarkers, *MarkerText, *CameraText);
	if (bHasReferencePoint)
	{
		const int32 GoodFrameCount = GoodMarkerFrames.Num();
		if (GoodFrameCount == 0)
		{
			Summary += TEXT(" | marker minus reference: no good frame");
		}
		else
		{
			const auto Median = [this, GoodFrameCount](const auto& Project)
			{
				Algo::SortBy(GoodMarkerFrames, Project);
				return (Project(GoodMarkerFrames[(GoodFrameCount - 1) / 2]) +
					Project(GoodMarkerFrames[GoodFrameCount / 2])) * 0.5;
			};
			const double X = Median([](const FGoodMarkerFrame& Frame) { return Frame.CenterStageCm.X; });
			const double Y = Median([](const FGoodMarkerFrame& Frame) { return Frame.CenterStageCm.Y; });
			const double Z = Median([](const FGoodMarkerFrame& Frame) { return Frame.CenterStageCm.Z; });
			const double DistanceCm = Median([](const FGoodMarkerFrame& Frame) { return Frame.CameraDistanceCm; });
			const FVector DeltaMm = (FVector(X, Y, Z) - ReferencePointStageCm) * 10.0;
			Summary += FString::Printf(TEXT(" | marker minus reference: XY %.1f mm | Z %+.1f mm | 3D %.1f mm | delta mm %s | good frames %d | camera-marker distance %.1f cm"),
				DeltaMm.Size2D(), DeltaMm.Z, DeltaMm.Size(), *DeltaMm.ToString(), GoodFrameCount, DistanceCm);
		}
	}
	FString Diagnostics = TEXT(" | D unavailable");
	if (bHasStagePoseInInterval)
	{
		const FRotator Rotation = MrukToStage.Rotator();
		Diagnostics = FString::Printf(TEXT(" | D cm %s Yaw %.3f Pitch %.3f Roll %.3f deg"),
			*MrukToStage.GetLocation().ToString(), Rotation.Yaw, Rotation.Pitch, Rotation.Roll);
	}
	if (const APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0))
	{
		Diagnostics += FString::Printf(TEXT(" | Pawn cm %s Yaw %.3f deg"),
			*Pawn->GetActorLocation().ToString(), Pawn->GetActorRotation().Yaw);
	}
	else
	{
		Diagnostics += TEXT(" | Pawn unavailable");
	}
	for (const auto& Entry : IgnoredMarkerCounts)
	{
		Diagnostics += FString::Printf(TEXT(" | ignored id %d count %d"), Entry.Key, Entry.Value);
	}
	UE_LOG(LogM1a, Log, TEXT("%s%s"), *Summary, *Diagnostics);
	ShowM1aMessage(Summary, 0x4d3161, 1.1f);
	if (FramesRead > 0 && !bSawNonBlackFrame)
	{
		UE_LOG(LogM1a, Warning, TEXT("All %d camera frames in this interval were black"), FramesRead);
		ShowM1aMessage(TEXT("Camera returned only black frames"));
		StopCamera();
	}
	FramesRead = 0;
	FramesWithMarkers = 0;
	GoodMarkerFrames.Reset();
	IgnoredMarkerCounts.Reset();
	bSawNonBlackFrame = false;
	bHasStagePoseInInterval = false;
	NextSummary = Now + 1.0;
#endif
}

void AM1aCameraSpike::StartTipSample()
{
	bSamplingTip = true;
	TipSampleEnd = GetWorld()->GetRealTimeSeconds() + 1.0;
	TipSumStageCm = FVector::ZeroVector;
	TipSamples = 0;
	ShowM1aMessage(TEXT("Hold left index tip at the reference point"), INDEX_NONE, 1.0f);
}

void AM1aCameraSpike::TickTipSample(double Now)
{
	const AVBoRHandRigPawn* Pawn = Cast<AVBoRHandRigPawn>(UGameplayStatics::GetPlayerPawn(this, 0));
	FVector WorldTip;
	if (!Pawn || !Pawn->ReadRawIndexTip(0, WorldTip))
	{
		bSamplingTip = false;
		UE_LOG(LogM1a, Log, TEXT("T canceled: left index tracking unavailable"));
		ShowM1aMessage(TEXT("Reference point recording canceled: left index tracking unavailable"));
		return;
	}

	TipSumStageCm += Pawn->GetActorTransform().InverseTransformPosition(WorldTip);
	++TipSamples;
	if (Now >= TipSampleEnd)
	{
		bSamplingTip = false;
		ReferencePointStageCm = TipSumStageCm / TipSamples;
		bHasReferencePoint = true;
		const FString Result = FString::Printf(TEXT("T: reference point Stage cm %s | %d samples"),
			*ReferencePointStageCm.ToString(), TipSamples);
		UE_LOG(LogM1a, Log, TEXT("%s"), *Result);
		ShowM1aMessage(Result);
	}
}

#if defined(WITH_OPENCV) && WITH_OPENCV && WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FM1aPoseFromCornersTest, "vBoR.M1a.PoseFromCorners",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FM1aPoseFromCornersTest::RunTest(const FString& Parameters)
{
	const FVector2D FocalPx(880.0, 915.0);
	const FVector2D PrincipalPx(628.0, 493.0);
	const FVector LocalCenter(85.0, 9.0, -7.0);
	const FTransform Cameras[] = {
		FTransform(FRotator::ZeroRotator, FVector(8.0, -12.0, 105.0)),
		FTransform(FRotator(-28.0, 37.0, 19.0), FVector(-44.0, 25.0, 142.0)),
	};
	const FQuat MarkerTilt = FRotator(10.0, -20.0, 8.0).Quaternion();
	const FVector CornerOffsets[] = {
		FVector(0, -3, -3), FVector(0, 3, -3), FVector(0, 3, 3), FVector(0, -3, 3),
	};

	for (int32 PoseIndex = 0; PoseIndex < UE_ARRAY_COUNT(Cameras); ++PoseIndex)
	{
		FVector2f Pixels[4];
		for (int32 CornerIndex = 0; CornerIndex < UE_ARRAY_COUNT(Pixels); ++CornerIndex)
		{
			const FVector Point = LocalCenter + MarkerTilt.RotateVector(CornerOffsets[CornerIndex]);
			Pixels[CornerIndex] = FVector2f(
				PrincipalPx.X + FocalPx.X * Point.Y / Point.X,
				PrincipalPx.Y + FocalPx.Y * Point.Z / Point.X);
		}

		const FTransform& Camera = Cameras[PoseIndex];
		FVector Center;
		FVector StageCorners[4];
		if (TestTrue(FString::Printf(TEXT("Solve pose %d"), PoseIndex),
			SolveMarkerCorners(Pixels, 60.0, Camera, FocalPx, PrincipalPx, Center, StageCorners)))
		{
			const double ErrorMm = FVector::Distance(Center, Camera.TransformPosition(LocalCenter)) * 10.0;
			TestTrue(FString::Printf(TEXT("Pose %d center error %.3f mm <= 1 mm"), PoseIndex, ErrorMm), ErrorMm <= 1.0);
		}
	}
	return true;
}
#endif
