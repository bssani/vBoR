#include "VBoRVehicleAlignmentComponent.h"
#include "VBoRAlignPointComponent.h"
#include "VBoREyePointComponent.h"
#include "VBoRViewpointComponent.h"
#include "VBoRHandRigPawn.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "GameFramework/Pawn.h"
#include "HeadMountedDisplayFunctionLibrary.h"
#include "IXRTrackingSystem.h"
#include "HAL/PlatformTime.h"

FString UVBoRVehicleAlignmentComponent::FindAlignPoints(TArray<UVBoRAlignPointComponent*>& Out) const
{
	GetOwner()->GetComponents(Out);
	Out.Sort([](const UVBoRAlignPointComponent& A, const UVBoRAlignPointComponent& B) { return A.Order < B.Order; });
	if (Out.Num() < 2) { return TEXT("Alignment points missing on the car"); }
	for (int32 I = 0; I < Out.Num(); ++I)
	{
		if (Out[I]->Order < 1) { return TEXT("Alignment points missing on the car"); }
		if (I > 0 && Out[I]->Order == Out[I-1]->Order) { return TEXT("Alignment point order duplicated"); }
	}
	return FString();
}

USceneComponent* UVBoRVehicleAlignmentComponent::DriverSeatReference(FString& Failure) const
{
	TArray<UVBoRViewpointComponent*> Views; GetOwner()->GetComponents(Views);
	UVBoRViewpointComponent* Driver = nullptr;
	for (auto* View : Views)
	{
		if (!View->ComponentHasTag(TEXT("Car.DriverView"))) { continue; }
		if (Driver) { Failure = TEXT("Multiple Car.DriverView points"); return nullptr; }
		Driver = View;
	}
	return Driver ? static_cast<USceneComponent*>(Driver) : BaseEye.Get();
}

bool UVBoRVehicleAlignmentComponent::FitCaptured(const TArray<FVBoRAlignPointSample>& Samples, FVBoRPointFit& Fit, FString& Failure) const
{
	TArray<UVBoRAlignPointComponent*> Points;
	Failure = FindAlignPoints(Points);
	if (!Failure.IsEmpty() || Samples.Num() != Points.Num()) { Failure = TEXT("alignment points changed"); return false; }
	TArray<FVector> Stage, World;
	for (auto* Point : Points)
	{
		const auto* Sample = Samples.FindByPredicate([Point](const FVBoRAlignPointSample& P) { return P.Order == Point->Order; });
		if (!Sample) { Failure = TEXT("alignment points changed"); return false; }
		Stage.Add(Sample->StageCm); World.Add(Point->GetComponentLocation());
	}
	if (!FVBoRBuckAlignment::FitPoints(Stage,World,FloorWorldZCm,Fit)) { Failure = TEXT("Points too close"); return false; }
	return true;
}

bool UVBoRVehicleAlignmentComponent::BasePawn(FTransform& Out, FString& Failure) const
{
	DriverSeatReference(Failure);
	if (!Failure.IsEmpty()) { return false; }
	if (Saved.Version == 2)
	{
		FVBoRPointFit Fit;
		if (!FitCaptured(Saved.Points,Fit,Failure)) { return false; }
		if (Fit.MaxXYCm > PointFitLimitCm) { Failure = TEXT("alignment points moved"); return false; }
		Out = Fit.Pawn; return true;
	}
	if (!BaseEye) { Failure = TEXT("BaseEye missing"); return false; }
	Out = bHeightFromFloor ? FVBoRBuckAlignment::CalculatePawnOnFloor(Saved.Capture,BaseEye->GetComponentTransform(),BaseEye->GetComponentTransform(),FloorWorldZCm)
		: FVBoRBuckAlignment::CalculatePawn(Saved.Capture,BaseEye->GetComponentTransform());
	return true;
}

bool UVBoRVehicleAlignmentComponent::CanReadPointTip() const { return Cast<AVBoRHandRigPawn>(AlignmentPawn) != nullptr; }
bool UVBoRVehicleAlignmentComponent::ReadPointTip(FVector& WorldTip) const
{
	const auto* HandPawn = Cast<AVBoRHandRigPawn>(AlignmentPawn);
	return HandPawn && HandPawn->ReadRawIndexTip(PointHand,WorldTip);
}
double UVBoRVehicleAlignmentComponent::PointTime() const { return FPlatformTime::Seconds(); }
void UVBoRVehicleAlignmentComponent::ReadPointEnvironment(int32& Origin, FVector2D& PlayArea) const
{
	Origin = GEngine && GEngine->XRSystem.IsValid() ? static_cast<int32>(GEngine->XRSystem->GetTrackingOrigin()) : INDEX_NONE;
	PlayArea = UHeadMountedDisplayFunctionLibrary::GetPlayAreaBounds(EHMDTrackingOrigin::Stage);
}

FString UVBoRVehicleAlignmentComponent::CapturePointRejection() const
{
	if (bSwitching) { return TEXT("View transition in progress"); }
	if (!IsBaseViewpoint()) { return TEXT("Return to the driver seat first"); }
	if (!CanReadPointTip()) { return TEXT("Hand tracking pawn required"); }
	TArray<UVBoRAlignPointComponent*> Points; const FString Failure = FindAlignPoints(Points);
	if (!Failure.IsEmpty()) { return Failure; }
	if (bPointCapturing) { return TEXT("Capturing a point"); }
	if (bPointSession && CapturedPoints.Num() >= Points.Num()) { return TEXT("All points captured — save or cancel"); }
	return FString();
}

bool UVBoRVehicleAlignmentComponent::CapturePoint()
{
	if (!CapturePointRejection().IsEmpty() || !FMath::IsFinite(PointCaptureSeconds) || PointCaptureSeconds <= 0) { return false; }
	if (!bPointSession) { CapturedPoints.Reset(); ReadPointEnvironment(PointOrigin,PointPlayArea); }
	bPointSession = true; bPointCapturing = true; PointWindow.Reset(); PointElapsed = 0; LastPointTime = PointTime();
	ShowPoints(true); PointResult(TEXT("Capturing a point")); return true;
}

void UVBoRVehicleAlignmentComponent::AdvancePointCapture()
{
	if (!bPointCapturing) { return; }
	int32 Origin; FVector2D Area; ReadPointEnvironment(Origin,Area);
	FVector WorldTip;
	if (Origin != EHMDTrackingOrigin::Stage || Origin != PointOrigin || !AlignmentPawn ||
		!FMath::IsNearlyEqual(GetWorld()->GetWorldSettings()->WorldToMeters,100.f) || !ReadPointTip(WorldTip) || WorldTip.ContainsNaN())
	{ bPointCapturing = false; PointResult(TEXT("Point canceled — hand not tracked or Stage changed")); return; }
	const double Now = PointTime(), Dt = Now - LastPointTime; LastPointTime = Now;
	if (!FMath::IsFinite(Dt) || Dt <= 0) { return; }
	// 원본 손끝 world → Stage. HomeShift 와 현재 정렬이 기록에 섞이지 않게 함 (P4h D2, cm)
	PointWindow.Add(AlignmentPawn->GetActorTransform().InverseTransformPosition(WorldTip)); PointElapsed += Dt;
	if (PointElapsed + UE_DOUBLE_SMALL_NUMBER < PointCaptureSeconds) { return; }
	bPointCapturing = false;
	FVBoRAlignPointSample Sample;
	for (int32 I = 0; I < PointWindow.Num(); ++I)
	{
		Sample.StageCm += PointWindow[I];
		for (int32 J = I + 1; J < PointWindow.Num(); ++J)
		{ Sample.SpreadMm = FMath::Max(Sample.SpreadMm,FVector::Distance(PointWindow[I],PointWindow[J]) * 10); }
	}
	Sample.Frames = PointWindow.Num(); Sample.StageCm /= Sample.Frames;
	if (Sample.SpreadMm > PointSpreadLimitMm)
	{ PointResult(FString::Printf(TEXT("Point rejected — spread %.2f mm over limit %.2f mm"),Sample.SpreadMm,PointSpreadLimitMm)); return; }
	TArray<UVBoRAlignPointComponent*> Points; const FString Failure = FindAlignPoints(Points);
	if (!Failure.IsEmpty() || !Points.IsValidIndex(CapturedPoints.Num())) { PointResult(TEXT("Point canceled — alignment points changed")); return; }
	Sample.Order = Points[CapturedPoints.Num()]->Order; CapturedPoints.Add(Sample);
	PointResult(FString::Printf(TEXT("Point %d captured — spread %.2f mm, %d frames"),Sample.Order,Sample.SpreadMm,Sample.Frames));
}

FString UVBoRVehicleAlignmentComponent::SavePointsRejection() const
{
	TArray<UVBoRAlignPointComponent*> Points; FindAlignPoints(Points);
	if (!bPointSession || bPointCapturing || CapturedPoints.Num() != Points.Num()) { return TEXT("Capture all points first"); }
	FVBoRPointFit Fit; FString Failure;
	if (!FitCaptured(CapturedPoints,Fit,Failure)) { return Failure; }
	if (Fit.SpanCm < PointMinSpanCm) { return TEXT("Points too close"); }
	if (Fit.MaxXYCm > PointFitLimitCm) { return FString::Printf(TEXT("Fit %.2f cm over limit %.2f cm"),Fit.MaxXYCm,PointFitLimitCm); }
	if (bSwitching) { return TEXT("View transition in progress"); }
	return FString();
}

bool UVBoRVehicleAlignmentComponent::SavePoints(FString& Failure)
{
	Failure = SavePointsRejection(); if (!Failure.IsEmpty()) { return false; }
	FVBoRBuckSavedAlignment Candidate; Candidate.Version = 2; Candidate.SavedAt = FDateTime::UtcNow().ToIso8601();
	Candidate.TrackingOrigin = PointOrigin; Candidate.PlayAreaCm = PointPlayArea; Candidate.Points = CapturedPoints;
	if (!FVBoRBuckAlignmentStore::Save(SavedAlignmentPath(),Candidate,Failure)) { PointResult(Failure); return false; }
	bPointSession = false; bPointCapturing = false;
	PointResult(TEXT("Points saved — check the markers, then A to start"));
	return RequestReapply();
}

void UVBoRVehicleAlignmentComponent::CancelPoints(const FString& Message)
{
	CapturedPoints.Reset(); PointWindow.Reset(); bPointCapturing = false; bPointSession = false;
	ShowPoints(false); PointResult(Message);
}
void UVBoRVehicleAlignmentComponent::FlowStarted()
{ if (bPointsVisible || bPointSession || !CapturedPoints.IsEmpty()) { CancelPoints(TEXT("Flow started — points hidden")); } }
void UVBoRVehicleAlignmentComponent::PointResult(const FString& Message)
{ LastPointResult = Message; OnPointRecord.Broadcast(Message); }
void UVBoRVehicleAlignmentComponent::ShowPoints(bool bShow)
{
	bPointsVisible = bShow; TArray<UVBoRAlignPointComponent*> Points; GetOwner()->GetComponents(Points);
	for (auto* Point : Points) { Point->ShowPoint(bShow); }
}

FString UVBoRVehicleAlignmentComponent::PointStatus() const
{
	TArray<UVBoRAlignPointComponent*> Points; const FString Failure = FindAlignPoints(Points);
	FString Text = LastPointResult;
	if (bPointSession && Points.IsValidIndex(CapturedPoints.Num()))
	{ Text = FString::Printf(TEXT("Point %d/%d — %s\n"),CapturedPoints.Num()+1,Points.Num(),*Points[CapturedPoints.Num()]->Label) + Text; }
	FVBoRPointFit Fit; FString FitFailure;
	if (!CapturedPoints.IsEmpty() && FitCaptured(CapturedPoints,Fit,FitFailure))
	{
		for (int32 I = 0; I < Points.Num(); ++I)
		{ Text += FString::Printf(TEXT("\nPoint %d: XY %.2f cm, height %+.2f cm%s"),Points[I]->Order,Fit.XYCm[I],Fit.HeightCm[I],FMath::Abs(Fit.HeightCm[I]) > PointHeightWarnCm ? TEXT(" — height warning") : TEXT("")); }
		Text += FString::Printf(TEXT("\nMax XY %.2f cm / limit %.2f cm"),Fit.MaxXYCm,PointFitLimitCm);
	}
	if (bPointsVisible)
	{
		FVector Tip;
		if (ReadPointTip(Tip) && !Tip.ContainsNaN() && !Points.IsEmpty())
		{
			const UVBoRAlignPointComponent* Nearest = Points[0];
			for (auto* Point : Points)
			{ if (FVector::DistSquared(Tip,Point->GetComponentLocation()) < FVector::DistSquared(Tip,Nearest->GetComponentLocation())) { Nearest = Point; } }
			Text += FString::Printf(TEXT("\nNearest point %d: XY %.2f cm, height %+.2f cm"),Nearest->Order,FVector::Dist2D(Tip,Nearest->GetComponentLocation()),Tip.Z - Nearest->GetComponentLocation().Z);
		}
		else { Text += TEXT("\nNearest point: hand not tracked"); }
	}
	return Text;
}
