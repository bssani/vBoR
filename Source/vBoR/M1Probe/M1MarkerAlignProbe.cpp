#include "M1MarkerAlignProbe.h"

#include "Algo/AllOf.h"
#include "Components/InputComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CoreDelegates.h"
#include "MRUKPassthroughCameraAccess.h"
#include "MRUtilityKitSubsystem.h"
#include "../Interaction/VBoRAlignPointComponent.h"
#include "../Interaction/VBoRHandRigPawn.h"
#include "../Interaction/VBoRVehicleAlignmentComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogM1Marker, Log, All);

bool SolveM1Marker(const FVector2f (&Pixels)[4], double SideMm, const FM1CameraModel& Model,
	const FTransform& PawnWorld, FM1MarkerObservation& Out);
void DetectM1Markers(FM1CameraFrame& Frame, TFunctionRef<void(int32, const FVector2f (&)[4])> Visit);

namespace
{
template <typename SampleType, typename Projection>
double M1Median(const TArray<SampleType>& Samples, Projection Project)
{
	TArray<double> Values;
	Values.Reserve(Samples.Num());
	for (const auto& Sample : Samples)
	{
		Values.Add(Project(Sample));
	}
	Values.Sort();
	const int32 Middle = Values.Num() / 2;
	return (Values[(Values.Num() - 1) / 2] + Values[Middle]) * 0.5;
}

FVector M1MedianCenter(const TArray<FM1MarkerObservation>& Samples)
{
	return FVector(
		M1Median(Samples, [](const auto& S) { return S.CenterStage.X; }),
		M1Median(Samples, [](const auto& S) { return S.CenterStage.Y; }),
		M1Median(Samples, [](const auto& S) { return S.CenterStage.Z; }));
}

double M1SpreadMm(const TArray<FM1MarkerObservation>& Samples, const FVector& CenterStage)
{
	TArray<double> DistancesMm;
	DistancesMm.Reserve(Samples.Num());
	for (const auto& Sample : Samples)
	{
		DistancesMm.Add(FVector::Distance(Sample.CenterStage, CenterStage) * 10);
	}
	DistancesMm.Sort();
	// Nearest rank (M1 rev 2)
	return DistancesMm[FMath::CeilToInt32(0.8 * DistancesMm.Num()) - 1];
}
}

AM1MarkerAlignProbe::AM1MarkerAlignProbe()
{
	PrimaryActorTick.bCanEverTick = true;
}

void AM1MarkerAlignProbe::BeginPlay()
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

	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		if (auto* Candidate = It->FindComponentByClass<UVBoRVehicleAlignmentComponent>())
		{
			if (Alignment.IsValid())
			{
				Alignment.Reset();
				Publish(TEXT("Multiple vehicle alignment components"));
				break;
			}
			Alignment = Candidate;
		}
	}

	if (Alignment.IsValid())
	{ Alignment->OnPointRecord.AddUObject(this, &AM1MarkerAlignProbe::HandleAlignmentRecord); }

	if (APlayerController* Controller = UGameplayStatics::GetPlayerController(this, 0))
	{
		EnableInput(Controller);
		InputComponent->BindKey(EKeys::M, IE_Pressed, this, &AM1MarkerAlignProbe::ToggleCapture);
		InputComponent->BindKey(EKeys::T, IE_Pressed, this, &AM1MarkerAlignProbe::StartTip);
		InputComponent->BindKey(EKeys::G, IE_Pressed, this, &AM1MarkerAlignProbe::ApplyFit);
		InputComponent->BindKey(EKeys::H, IE_Pressed, this, &AM1MarkerAlignProbe::RestorePawn);
		InputComponent->BindKey(EKeys::S, IE_Pressed, this, &AM1MarkerAlignProbe::SaveFit);
	}
	RecenterHandle = FCoreDelegates::VRHeadsetRecenter.AddUObject(this, &AM1MarkerAlignProbe::CancelForRecenter);
	Publish(TEXT("M: Record or cancel   T: Compare fingertip   G: Preview fit   H: Restore   S: Save fit"));
	bCameraStarted = StartCamera();
	if (!bCameraStarted)
	{
		Publish(TEXT("Camera start failed — check Link settings (Passthrough over Link, camera API permission)"));
	}
}

void AM1MarkerAlignProbe::EndPlay(EEndPlayReason::Type Reason)
{
	if (Alignment.IsValid()) { Alignment->OnPointRecord.RemoveAll(this); }
	FCoreDelegates::VRHeadsetRecenter.Remove(RecenterHandle);
	StopCamera();
	if (UMRUKSubsystem* Subsystem = WorldLockSubsystem.Get())
	{
		Subsystem->EnableWorldLock = bPreviousWorldLock;
	}
	Super::EndPlay(Reason);
}

void AM1MarkerAlignProbe::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const double Now = GetWorld()->GetRealTimeSeconds();
	if (bCapturing && Now >= CaptureStart + MarkerSessionSeconds)
	{
		FinishCapture(Now);
	}
	if (bCameraStarted && (!bModelChecked || bCapturing))
	{
		FM1CameraFrame Frame;
		if (ReadCameraFrame(Frame, bCapturing))
		{
			ProcessFrame(Frame, Now);
		}
	}
	if (bSamplingTip)
	{
		ProcessTip(Now);
	}
	UpdateVisuals();
}

void AM1MarkerAlignProbe::ToggleCapture()
{
	if (bCapturing)
	{
		bCapturing = false;
		Markers.Reset();
		Orders.Reset();
		Publish(TEXT("Marker capture canceled"));
		return;
	}
	StartCapture(GetWorld()->GetRealTimeSeconds());
}

void AM1MarkerAlignProbe::StartCapture(double Now)
{
	if (!bCameraStarted)
	{
		bCameraStarted = StartCamera();
		if (!bCameraStarted)
		{
			Publish(TEXT("Camera start failed — check Link settings (Passthrough over Link, camera API permission)"));
			return;
		}
	}
	if (bModelChecked && !bModelValid)
	{
		Publish(TEXT("Camera model mismatch — capture unavailable"));
		return;
	}
	if (!Alignment.IsValid() || !IsValid(ReadPawn()))
	{
		Publish(TEXT("Vehicle alignment or player pawn unavailable"));
		return;
	}
	if (GetMarkerArrowWorld(*Alignment->GetOwner()).IsZero())
	{
		Publish(TEXT("Marker arrow direction invalid"));
		return;
	}

	const FString ProfileFailure = Alignment->ValidateBuckProfiles();
	if (!ProfileFailure.IsEmpty())
	{
		Publish(TEXT("Buck profiles invalid — ") + ProfileFailure);
		return;
	}
	TArray<UVBoRAlignPointComponent*> Points;
	TMap<int32, int32> BuckIndices;
	if (Alignment->Bucks.IsEmpty())
	{
		Alignment->GetOwner()->GetComponents(Points);
		Points.Sort([](const UVBoRAlignPointComponent& A, const UVBoRAlignPointComponent& B) { return A.Order < B.Order; });
		if (Points.Num() != 4)
		{
			Publish(TEXT("Expected four vehicle alignment points"));
			return;
		}
		for (int32 Index = 0; Index < Points.Num(); ++Index)
		{
			const int32 Order = Points[Index]->Order;
			if (Order < 1 || Order >= 50 || (Index > 0 && Order == Points[Index - 1]->Order))
			{
				Publish(TEXT("Expected distinct alignment point orders from 1 to 49"));
				return;
			}
		}
	}
	else
	{
		for (int32 Index = 0; Index < Alignment->Bucks.Num(); ++Index)
		{
			const auto& Buck = Alignment->Bucks[Index];
			TArray<UVBoRAlignPointComponent*> BuckPoints;
			Alignment->FindAlignPoints(BuckPoints, Buck.Name);
			if (BuckPoints.Num() != 4)
			{
				Publish(FString::Printf(TEXT("Expected four vehicle alignment points for buck %s"), *Buck.Name));
				return;
			}
			for (const auto* Point : BuckPoints) { BuckIndices.Add(Buck.FirstMarkerId + Point->Order - 1, Index); }
		}
	}
	Markers.Reset();
	Orders.Reset();
	for (auto* Point : Points)
	{
		Orders.Add(Point->Order);
		Markers.Add(Point->Order).Point = Point;
	}
	MarkerBuckIndices = MoveTemp(BuckIndices);
	IgnoredMarkerCounts.Reset();
	SessionBuck.Reset();
	Fit.Reset();
	LastCameraRotation.Reset();
	if (Camera)
	{
		LastTimestamp = Camera->GetTimestamp();
	}
	CaptureStart = Now;
	bCapturing = true;
	Publish(TEXT("Marker capture started"));
}

FVector AM1MarkerAlignProbe::GetMarkerArrowWorld(const AActor& Car) const
{
	if (MarkerArrowLocal.ContainsNaN())
	{
		return FVector::ZeroVector;
	}
	return Car.GetActorTransform().TransformVectorNoScale(MarkerArrowLocal).GetSafeNormal2D();
}

bool AM1MarkerAlignProbe::CheckModel(const FM1CameraFrame& Frame)
{
	double MaxError = 0;
	for (const FVector Local : {FVector(70, 3, 8), FVector(82, -17, -12), FVector(95, 11, 18)})
	{
		const FVector2D Expected = Frame.Model.Project(Frame.Model.Pose.TransformPosition(Local));
		// MRUK treats local +Z as image down (M1 rev 1)
		const FVector Reflected(Local.X, Local.Y, -Local.Z);
		const FVector2D Actual = ProjectMruk(Frame.Model.Pose.TransformPosition(Reflected)) *
			FVector2D(Frame.Size.X, Frame.Size.Y);
		const double Error = FVector2D::Distance(Expected, Actual);
		MaxError = FMath::IsFinite(Error) ? FMath::Max(MaxError, Error) : TNumericLimits<double>::Max();
	}
	const bool bValid = MaxError <= 0.5;
	Publish(bValid ? FString::Printf(TEXT("Camera model check passed (%.3f px)"), MaxError) :
		FString::Printf(TEXT("Camera model mismatch %.3g px (MRUK projection convention changed?)"), MaxError));
	return bValid;
}

void AM1MarkerAlignProbe::ProcessFrame(FM1CameraFrame& Frame, double Now)
{
	if (Frame.Timestamp <= LastTimestamp)
	{
		return;
	}
	const double Interval = (Frame.Timestamp - LastTimestamp).GetTotalSeconds();
	LastTimestamp = Frame.Timestamp;
	if (!bModelChecked)
	{
		bModelChecked = true;
		bModelValid = CheckModel(Frame);
		if (!bModelValid)
		{
			bCapturing = false;
			return;
		}
	}
	const APawn* Pawn = ReadPawn();
	if (!bCapturing || !bModelValid || !IsValid(Pawn))
	{
		return;
	}
	const FQuat Rotation = Frame.Model.Pose.GetRotation();
	const bool bFastHead = LastCameraRotation.IsSet() &&
		FMath::RadiansToDegrees(LastCameraRotation->AngularDistance(Rotation)) > MarkerMaxHeadDegPerSec * Interval;
	LastCameraRotation = Rotation;
	const FTransform PawnWorld = Pawn->GetActorTransform();
	DetectM1Markers(Frame, [this, &Frame, &PawnWorld, bFastHead](int32 Id, const FVector2f (&Corners)[4])
	{
		const FMarker* Marker = Markers.Find(Id);
		if (!Marker && !MarkerBuckIndices.Contains(Id)) { return; }
		if (Marker && Marker->bComplete) { return; }
		if (!Marker && !SessionBuck.IsEmpty())
		{
			RecordObservation(Id, nullptr, bFastHead);
			return;
		}
		FM1MarkerObservation Observation;
		const bool bValid = !bFastHead && SolveM1Marker(Corners, MarkerSizeMm, Frame.Model, PawnWorld, Observation);
		RecordObservation(Id, bValid ? &Observation : nullptr, bFastHead);
	});
	if (!Orders.IsEmpty() && Algo::AllOf(Orders, [this](int32 Id) { return Markers[Id].bComplete; }))
	{
		FinishCapture(Now);
	}
}

void AM1MarkerAlignProbe::RecordObservation(int32 Id, const FM1MarkerObservation* Observation, bool bFastHead)
{
	FMarker* Marker = Markers.Find(Id);
	const bool bAccepted = !bFastHead && Observation && Observation->ReprojectionPx <= MarkerReprojLimitPx &&
		Observation->SidePx >= MarkerMinSidePx;
	if (!Marker)
	{
		const int32* Index = MarkerBuckIndices.Find(Id);
		if (!Index || !Alignment.IsValid() || !Alignment->Bucks.IsValidIndex(*Index)) { return; }
		if (!SessionBuck.IsEmpty())
		{
			++IgnoredMarkerCounts.FindOrAdd(Id);
			return;
		}
		if (!bAccepted) { return; }
		const auto& Buck = Alignment->Bucks[*Index];
		TArray<UVBoRAlignPointComponent*> Points;
		if (!Alignment->FindAlignPoints(Points, Buck.Name).IsEmpty() || Points.Num() != 4) { return; }
		SessionBuck = Buck.Name;
		for (auto* Point : Points)
		{
			const int32 MarkerId = Buck.FirstMarkerId + Point->Order - 1;
			Orders.Add(MarkerId);
			Markers.Add(MarkerId).Point = Point;
		}
		Marker = Markers.Find(Id);
		Publish(TEXT("Marker capture started — buck ") + SessionBuck);
	}
	if (!Marker || Marker->bComplete) { return; }
	if (!bAccepted)
	{
		++Marker->Rejected;
		return;
	}
	for (const auto& Sample : Marker->Samples)
	{
		Marker->MaxPairMm = FMath::Max(Marker->MaxPairMm,
			FVector::Distance(Sample.CenterStage, Observation->CenterStage) * 10);
	}
	Marker->Samples.Add(*Observation);
	Marker->bComplete = Marker->Samples.Num() >= FMath::Max(1, MarkerMinFrames);
	Marker->CenterStage = Observation->CenterStage;
	if (Marker->bComplete)
	{
		Marker->CenterStage = M1MedianCenter(Marker->Samples);
		Marker->SpreadMm = M1SpreadMm(Marker->Samples, Marker->CenterStage);
		Marker->bRejected = Marker->SpreadMm > MarkerSpreadLimitMm;
	}
}

void AM1MarkerAlignProbe::FinishCapture(double Now)
{
	bCapturing = false;
	Publish(FString::Printf(TEXT("Marker session %.2f s"), Now - CaptureStart));
	if (!MarkerBuckIndices.IsEmpty())
	{
		if (SessionBuck.IsEmpty()) { Publish(TEXT("Incomplete — no buck identified")); return; }
		if (Alignment.IsValid())
		{
			TArray<int32> Ids;
			IgnoredMarkerCounts.GetKeys(Ids);
			Ids.Sort();
			for (int32 Id : Ids)
			{
				const int32 Index = MarkerBuckIndices[Id];
				if (!Alignment->Bucks.IsValidIndex(Index)) { continue; }
				Publish(FString::Printf(TEXT("Ignored markers from buck %s: ID %d ×%d"),
					*Alignment->Bucks[Index].Name, Id, IgnoredMarkerCounts[Id]));
			}
		}
	}
	bool bReady = true;
	TArray<FVector> Stage;
	TArray<FVector> World;
	for (int32 Id : Orders)
	{
		const FMarker& Marker = Markers[Id];
		const auto* Point = Marker.Point.Get();
		const FString Label = Point && !Point->Label.IsEmpty() ? FString::Printf(TEXT(" (%s)"), *Point->Label) : FString();
		double ErrorSum = 0;
		for (const auto& Sample : Marker.Samples)
		{
			ErrorSum += Sample.ReprojectionPx;
		}
		const double SpreadMm = !Marker.bComplete && !Marker.Samples.IsEmpty() ?
			M1SpreadMm(Marker.Samples, M1MedianCenter(Marker.Samples)) : Marker.SpreadMm;
		Publish(FString::Printf(TEXT("Marker %d%s | spread %.2f mm | max pair %.2f mm | accepted %d | rejected %d | mean reprojection %s"),
			Id, *Label, SpreadMm, Marker.MaxPairMm, Marker.Samples.Num(), Marker.Rejected,
			Marker.Samples.IsEmpty() ? TEXT("unavailable") :
			*FString::Printf(TEXT("%.2f px"), ErrorSum / Marker.Samples.Num())));
		if (!Marker.bComplete)
		{
			Publish(FString::Printf(TEXT("Incomplete — missing ID %d (%d/%d frames)"), Id, Marker.Samples.Num(), MarkerMinFrames));
			bReady = false;
		}
		else if (Marker.bRejected)
		{
			Publish(FString::Printf(TEXT("Marker %d rejected: spread %.2f mm"), Id, Marker.SpreadMm));
			bReady = false;
		}
		if (Point)
		{
			Stage.Add(Marker.CenterStage);
			World.Add(Point->GetComponentLocation());
		}
	}
	if (!bReady)
	{
		return;
	}

	const auto* Settings = Alignment.Get();
	const auto* Pawn = ReadPawn();
	FVBoRPointFit Candidate;
	if (!Settings || !IsValid(Pawn) || Stage.Num() != Orders.Num() ||
		!FVBoRBuckAlignment::FitPoints(Stage, World, Settings->FloorWorldZCm, Candidate))
	{
		Publish(TEXT("Marker fit unavailable"));
		return;
	}
	const AActor& Car = *Settings->GetOwner();
	const FVector CarFrontWorld = GetMarkerArrowWorld(Car);
	if (CarFrontWorld.IsZero())
	{
		Publish(TEXT("Marker arrow direction invalid"));
		return;
	}
	const FVector CarFrontStage = Candidate.Pawn.InverseTransformVectorNoScale(CarFrontWorld);
	const double CarYaw = CarFrontStage.Rotation().Yaw;
	bool bArrowRejected = false;
	for (int32 Index = 0; Index < Orders.Num(); ++Index)
	{
		const int32 Id = Orders[Index];
		FMarker& Marker = Markers[Id];
		Candidate.HeightCm[Index] -= MarkerHeightOffsetCm;
		Publish(FString::Printf(TEXT("Marker %d | XY %.2f cm | height %+.2f cm"),
			Id, Candidate.XYCm[Index], Candidate.HeightCm[Index]));
		const FVector MeasuredWorld = Candidate.Pawn.TransformPosition(Stage[Index]);
		const FVector Residual = Car.GetActorTransform().InverseTransformVectorNoScale(MeasuredWorld - World[Index]);
		Publish(FString::Printf(TEXT("Marker %d | Stage (%.2f, %.2f, %.2f) cm | car-local residual X %+.2f cm | Y %+.2f cm"),
			Id, Stage[Index].X, Stage[Index].Y, Stage[Index].Z, Residual.X, Residual.Y), false);

		const double Tilt = M1Median(Marker.Samples, [](const auto& S) { return S.TiltDeg; });
		if (Tilt > 60)
		{
			Publish(FString::Printf(TEXT("Marker %d arrow check skipped (tilt %.1f deg)"), Id, Tilt));
			continue;
		}
		// Unwrap around a measured arrow before taking its median (wrap at 180 deg)
		const double AnchorYaw = Marker.Samples[0].ArrowYawDeg;
		const double ArrowYaw = AnchorYaw + M1Median(Marker.Samples, [AnchorYaw](const auto& S)
		{
			return FMath::FindDeltaAngleDegrees(AnchorYaw, S.ArrowYawDeg);
		});
		const double Error = FMath::Abs(FMath::FindDeltaAngleDegrees(CarYaw, FRotator::NormalizeAxis(ArrowYaw)));
		if (Error > MarkerYawLimitDeg)
		{
			Marker.bRejected = true;
			bArrowRejected = true;
			Publish(FString::Printf(TEXT("Marker %d arrow %.0f deg off car front — markers swapped or rotated?"), Id, Error));
		}
	}
	for (int32 A = 0; A < Orders.Num(); ++A)
	{
		for (int32 B = A + 1; B < Orders.Num(); ++B)
		{
			const double DistanceDelta = FVector::Dist2D(Stage[A], Stage[B]) - FVector::Dist2D(World[A], World[B]);
			Publish(FString::Printf(TEXT("Marker pair %d-%d | XY distance delta %+.2f cm"),
				Orders[A], Orders[B], DistanceDelta), false);
		}
	}
	if (bArrowRejected)
	{
		return;
	}

	const FTransform Baseline = OriginalPawnPose.IsSet() && PreviewPawn.Get() == Pawn ?
		OriginalPawnPose.GetValue() : Pawn->GetActorTransform();
	const FVector Delta = Candidate.Pawn.GetLocation() - Baseline.GetLocation();
	const double DeltaYaw = FMath::FindDeltaAngleDegrees(Baseline.Rotator().Yaw, Candidate.Pawn.Rotator().Yaw);
	const double At70 = 70 * FMath::Abs(FMath::Tan(FMath::DegreesToRadians(DeltaYaw)));
	const FString State = StaticEnum<EVBoRAlignmentState>()->GetNameStringByValue(static_cast<int64>(Settings->State));
	Publish(FString::Printf(TEXT("Max XY %.2f cm / %.2f cm%s"), Candidate.MaxXYCm, Settings->PointFitLimitCm,
		Candidate.MaxXYCm > Settings->PointFitLimitCm ? TEXT(" (over limit)") : TEXT("")));
	Publish(FString::Printf(TEXT("Marker fit vs pawn (%s) | dX %+.2f cm | dY %+.2f cm | dYaw %+.2f deg | at 70 cm %.2f cm"),
		*State, Delta.X, Delta.Y, DeltaYaw, At70));
	Fit = MoveTemp(Candidate);
}

void AM1MarkerAlignProbe::SaveFit()
{
	if (!Fit.IsSet() || !Alignment.IsValid())
	{
		Publish(TEXT("No marker fit to save"));
		return;
	}
	TArray<FVBoRAlignPointSample> Samples;
	for (int32 Id : Orders)
	{
		const FMarker& Marker = Markers[Id];
		const auto* Point = Marker.Point.Get();
		if (!Point) { Publish(TEXT("Marker save rejected — alignment points changed")); return; }
		FVBoRAlignPointSample& Sample = Samples.AddDefaulted_GetRef();
		Sample.Order = Point->Order;
		Sample.StageCm = Marker.CenterStage;
		Sample.SpreadMm = Marker.SpreadMm;
		Sample.Frames = Marker.Samples.Num();
	}
	FString Failure;
	if (!Alignment->SaveMarkerPoints(SessionBuck, Samples, Failure))
	{
		Publish(TEXT("Marker save rejected — ") + Failure);
		return;
	}
	const auto* AnchorSwitch = IConsoleManager::Get().FindConsoleVariable(TEXT("vBoR.AnchorAlignment"));
	// AnchorResult supplies wait/success/failure lines; don't invite an early EndPlay
	if (!AnchorSwitch || AnchorSwitch->GetInt() == 0)
	{ Publish(SessionBuck.IsEmpty() ? TEXT("Saved alignment — open L_Main") : TEXT("Saved for buck ") + SessionBuck + TEXT(" — open L_Main")); }
}

void AM1MarkerAlignProbe::HandleAlignmentRecord(const FString& Line)
{
	Publish(Line);
}

void AM1MarkerAlignProbe::CancelForRecenter()
{
	if (bCapturing)
	{
		bCapturing = false;
		Publish(TEXT("Recenter — marker capture canceled"));
	}
	bSamplingTip = false;
	Markers.Reset();
	Orders.Reset();
	Fit.Reset();
}

void AM1MarkerAlignProbe::ApplyFit()
{
	APawn* Pawn = ReadPawn();
	if (!Fit.IsSet() || !IsValid(Pawn))
	{
		Publish(TEXT("No marker fit"));
		return;
	}
	if (PreviewPawn.Get() != Pawn || !OriginalPawnPose.IsSet())
	{
		OriginalPawnPose = Pawn->GetActorTransform();
		PreviewPawn = Pawn;
	}
	Pawn->SetActorTransform(Fit->Pawn, false, nullptr, ETeleportType::TeleportPhysics);
	if (Alignment.IsValid())
	{
		TArray<UVBoRAlignPointComponent*> Points;
		Alignment->GetOwner()->GetComponents(Points);
		for (auto* Point : Points)
		{
			Point->ShowPoint(Alignment->Bucks.IsEmpty() || Point->Buck == SessionBuck);
		}
	}
	Publish(TEXT("Marker fit applied temporarily"));
}

void AM1MarkerAlignProbe::RestorePawn()
{
	if (!OriginalPawnPose.IsSet())
	{
		return;
	}
	if (APawn* Pawn = PreviewPawn.Get())
	{
		Pawn->SetActorTransform(OriginalPawnPose.GetValue(), false, nullptr, ETeleportType::TeleportPhysics);
	}
	OriginalPawnPose.Reset();
	PreviewPawn.Reset();
	if (Alignment.IsValid())
	{
		TArray<UVBoRAlignPointComponent*> Points;
		Alignment->GetOwner()->GetComponents(Points);
		for (auto* Point : Points)
		{
			Point->ShowPoint(Alignment->ArePointsVisible() &&
				(Alignment->Bucks.IsEmpty() || Point->Buck == Alignment->ActiveBuckName()));
		}
	}
	Publish(TEXT("Previous pawn pose restored"));
}

APawn* AM1MarkerAlignProbe::ReadPawn() const
{
	return UGameplayStatics::GetPlayerPawn(this, 0);
}

bool AM1MarkerAlignProbe::ReadTip(FVector& World) const
{
	const auto* Pawn = Cast<AVBoRHandRigPawn>(ReadPawn());
	return Pawn && Alignment.IsValid() && Pawn->ReadRawIndexTip(Alignment->PointHand, World);
}

void AM1MarkerAlignProbe::StartTip()
{
	TipSumStage = FVector::ZeroVector;
	TipCount = 0;
	TipEnd = GetWorld()->GetRealTimeSeconds() + 1;
	bSamplingTip = true;
	Publish(TEXT("Hold index fingertip on the marker center for 1 s"));
}

void AM1MarkerAlignProbe::ProcessTip(double Now)
{
	const auto* Pawn = ReadPawn();
	FVector WorldTip;
	if (!IsValid(Pawn) || !ReadTip(WorldTip) || WorldTip.ContainsNaN())
	{
		bSamplingTip = false;
		Publish(TEXT("Fingertip comparison canceled: tracking unavailable"));
		return;
	}
	TipSumStage += Pawn->GetActorTransform().InverseTransformPosition(WorldTip);
	++TipCount;
	if (Now < TipEnd)
	{
		return;
	}
	bSamplingTip = false;

	const FVector StageTip = TipSumStage / TipCount;
	const FMarker* Nearest = nullptr;
	int32 NearestId = INDEX_NONE;
	for (int32 Id : Orders)
	{
		const FMarker& Marker = Markers[Id];
		if (!Marker.Samples.IsEmpty() && !Marker.bRejected &&
			(!Nearest || FVector::DistSquared(StageTip, Marker.CenterStage) < FVector::DistSquared(StageTip, Nearest->CenterStage)))
		{
			Nearest = &Marker;
			NearestId = Id;
		}
	}
	if (!Nearest)
	{
		Publish(TEXT("No accepted marker center for fingertip comparison"));
		return;
	}
	const FVector DeltaMm = (Nearest->CenterStage - StageTip) * 10;
	Publish(FString::Printf(TEXT("Marker %d minus fingertip | XY %.1f mm | height %+.1f mm | 3D %.1f mm"),
		NearestId, DeltaMm.Size2D(), DeltaMm.Z, DeltaMm.Size()));
}

void AM1MarkerAlignProbe::UpdateVisuals()
{
	const auto* Pawn = ReadPawn();
	if (!IsValid(Pawn))
	{
		return;
	}
	for (const auto& Entry : Markers)
	{
		const FMarker& Marker = Entry.Value;
		if (!Marker.Samples.IsEmpty())
		{
			const FColor Color = Marker.bRejected ? FColor::Red : (Marker.bComplete ? FColor::Green : FColor::Yellow);
			DrawDebugSphere(GetWorld(), Pawn->GetActorTransform().TransformPosition(Marker.CenterStage), 1, 12, Color);
		}
	}
	if (!OriginalPawnPose.IsSet() || PreviewPawn.Get() != Pawn || !Alignment.IsValid() || !GEngine)
	{
		return;
	}
	FVector Tip;
	const FString Check = ReadTip(Tip) && !Tip.ContainsNaN() ? DescribeCheckPoint(Tip) : FString();
	if (Check.IsEmpty())
	{
		GEngine->RemoveOnScreenDebugMessage(0x4d3143);
	}
	else
	{
		GEngine->AddOnScreenDebugMessage(0x4d3143, 0.1f, FColor::Cyan, Check);
	}
}

FString AM1MarkerAlignProbe::DescribeCheckPoint(const FVector& Tip) const
{
	TArray<USceneComponent*> Components;
	Alignment->GetOwner()->GetComponents(Components);
	const USceneComponent* Nearest = nullptr;
	double BestDistance = TNumericLimits<double>::Max();
	for (const auto* Component : Components)
	{
		if (Component->ComponentHasTag(TEXT("Align.Check")))
		{
			const double Distance = FVector::DistSquared(Tip, Component->GetComponentLocation());
			if (Distance < BestDistance)
			{
				Nearest = Component;
				BestDistance = Distance;
			}
		}
	}
	if (!Nearest)
	{
		return FString();
	}
	const FVector Delta = Tip - Nearest->GetComponentLocation();
	return FString::Printf(TEXT("%s | XY %.1f cm | height %+.1f cm"), *Nearest->GetName(), Delta.Size2D(), Delta.Z);
}

void AM1MarkerAlignProbe::Publish(const FString& Line, bool bShowOnScreen)
{
	UE_LOG(LogM1Marker, Log, TEXT("%s"), *Line);
	if (bShowOnScreen && GEngine)
	{
		GEngine->AddOnScreenDebugMessage(INDEX_NONE, 10, FColor::Cyan, Line);
	}
#if WITH_DEV_AUTOMATION_TESTS
	if (bCollectTestMessages)
	{
		TestMessages.Add(Line);
	}
#endif
}
