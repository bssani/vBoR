#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "../Interaction/VBoRBuckAlignment.h"

#include "P4hTestFixture.h"
#include "P3VehicleMock.h"
#include "../P0Probe/P0BuckBench.h"
#include "../Interaction/VBoRAlignPointComponent.h"
#include "../Interaction/VBoREyePointComponent.h"
#include "../Interaction/VBoRViewpointComponent.h"
#include "../Interaction/VBoRScenarioRunner.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Components/InputComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/WorldSettings.h"
#include "HeadMountedDisplayTypes.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Misc/CoreDelegates.h"
#include "HAL/FileManager.h"

struct FP4hTestAccess
{
	static void BeforeStart(AVBoRScenarioRunner* R) { R->bRunning = false; R->bFreeMode = false; }
	static void ClearFrame(AVBoRScenarioRunner* R) { R->LiveFrame = MAX_uint64; }
	static void BenchLoad(AP0BuckBench* B, const FString& Path, APawn* Pawn)
	{ B->SavePath = Path; B->AlignmentPawn = Pawn; FVBoRBuckHeadFrame F; F.TrackingOrigin = EHMDTrackingOrigin::Stage; F.PlayAreaCm = FVector2D(300,400); B->LoadAndApply(F); }
	static bool BenchHasSaved(AP0BuckBench* B) { return B->bHasSaved; }
	static FString BenchState(AP0BuckBench* B) { return B->State; }
	static void BenchSave(AP0BuckBench* B)
	{
		double Now = 100;
		B->CaptureClock = [&Now] { return Now; };
		FVBoRBuckHeadFrame F; F.bTracked = true; F.TrackingOrigin = EHMDTrackingOrigin::Stage;
		F.PlayAreaCm = FVector2D(300,400); F.PositionCm = FVector(0,0,115);
		B->CurrentFrame = F; B->StartSave(); Now += B->SaveHoldSeconds; B->ProcessFrame(B->SaveHoldSeconds,F); B->ReleaseSave();
		B->CaptureClock = [] { return 0.; };
	}
};

namespace
{
	constexpr auto P4hFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;
	using PB = EVBoROperatorButton;
	struct FP4hWorld
	{
		UWorld* World = UWorld::CreateWorld(EWorldType::Game,false);
		APawn* Pawn;
		AP3VehicleMock* Car;
		UP4hTestAlignment* A;
		AVBoRScenarioRunner* Runner;
		FString Path;
		FP4hWorld()
		{
			GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
			auto* PC = World->SpawnActor<APlayerController>(); Pawn = World->SpawnActor<APawn>();
			Pawn->SetRootComponent(NewObject<USceneComponent>(Pawn)); Pawn->GetRootComponent()->RegisterComponent(); PC->Possess(Pawn);
			Car = World->SpawnActor<AP3VehicleMock>(); Car->Alignment->DestroyComponent();
			A = NewObject<UP4hTestAlignment>(Car); Car->AddInstanceComponent(A); Car->Alignment = A; A->BaseEye = Car->DriverEye;
			A->Origin = EHMDTrackingOrigin::Stage;
			Path = FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("Automation"),FGuid::NewGuid().ToString()+TEXT("-P4h.json")); A->TestPath = Path; A->RegisterComponent();
			Runner = World->SpawnActor<AVBoRScenarioRunner>(); Runner->Scenario = NewObject<UVBoRScenarioAsset>(Runner);
			Runner->Scenario->Steps.Reset(); FVBoRScenarioStep Step; Step.StepId = TEXT("01"); Step.ConfirmBy = EVBoRConfirmBy::Either; Runner->Scenario->Steps.Add(Step);
			World->InitializeActorsForPlay(FURL()); World->GetWorldSettings()->NotifyBeginPlay(); Runner->InitializeFlow();
		}
		~FP4hWorld()
		{ World->EndPlay(EEndPlayReason::Quit); GEngine->DestroyWorldContext(World); World->DestroyWorld(false); IFileManager::Get().Delete(*Path); IFileManager::Get().Delete(*(Path+TEXT(".candidate"))); }
		void Button(PB B) { FP4hTestAccess::ClearFrame(Runner); Runner->PressOperatorButton(B); Runner->ProcessFlow(0); }
		void Sample(double Dt) { A->Now += Dt; A->AdvancePointCapture(); }
		void CaptureAll(double Height = 0)
		{
			for (int32 I = A->GetCapturedPoints().Num(); I < Car->AlignPoints.Num(); ++I)
			{ A->StageTip = Car->AlignPoints[I]->GetComponentLocation() + FVector(0,0,Height); Button(PB::CapturePoint); Sample(A->PointCaptureSeconds); }
		}
		void FinishFade() { A->AdvanceSwitch(A->FadeSeconds); A->AdvanceSwitch(A->FadeSeconds); Runner->ProcessFlow(0); }
		FVBoRBuckSavedAlignment SavedPoints() const
		{
			FVBoRBuckSavedAlignment S; S.Version = 2; S.SavedAt = TEXT("2026-10-20T00:00:00Z"); S.TrackingOrigin = A->Origin; S.PlayAreaCm = A->Area;
			for (UVBoRAlignPointComponent* P : Car->AlignPoints) { FVBoRAlignPointSample V; V.Order = P->Order; V.StageCm = P->GetComponentLocation(); V.Frames = 15; S.Points.Add(V); }
			return S;
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP4hSolve, "vBoR.P4h.E1E2RigidFit", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP4hSolve::RunTest(const FString&)
{
	const TArray<FVector> Stage = {FVector(4,-60,50), FVector(4,-14,50), FVector(-44,-14,50), FVector(-44,-60,50)};
	for (double Yaw : {30., -30., -120., 179.})
	{
		const FTransform Expected(FRotator(0,Yaw,0), FVector(123,-456,7));
		TArray<FVector> World; for (const FVector& P : Stage) { World.Add(Expected.TransformPosition(P)); }
		FVBoRPointFit Fit;
		TestTrue(TEXT("Solves exact points"), FVBoRBuckAlignment::FitPoints(Stage,World,7,Fit));
		TestTrue(TEXT("Yaw sign and translation"), Fit.Pawn.Equals(Expected,0.0001));
		TestTrue(TEXT("No XY residual"), Fit.MaxXYCm < 0.01);
		TestEqual(TEXT("Floor height"), Fit.Pawn.GetLocation().Z, 7.);
		World[0].X += 1; World[1].X -= 1;
		TestTrue(TEXT("Noisy fit"), FVBoRBuckAlignment::FitPoints(Stage,World,7,Fit));
		TestTrue(TEXT("Reports residual"), Fit.MaxXYCm > 0.1 && Fit.MaxXYCm < 2);
	}
	TArray<FVector> Two = {Stage[0], Stage[1]}; FVBoRPointFit Fit;
	TestTrue(TEXT("Two points"), FVBoRBuckAlignment::FitPoints(Two,Two,0,Fit));
	TestEqual(TEXT("XY span cm"), Fit.SpanCm, 46.);
	Two[1] = Two[0]; TestFalse(TEXT("Coincident points cannot set Yaw"), FVBoRBuckAlignment::FitPoints(Two,Two,0,Fit));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP4hCapture,"vBoR.P4h.E3E4StageAndCaptureWindow",P4hFlags)
bool FP4hCapture::RunTest(const FString&)
{
	for (int32 Fps : {15,90})
	{
		FP4hWorld W; W.Pawn->SetActorTransform(FTransform(FRotator(0,75,0),FVector(100,-200,0))); W.A->MoveHomeShift(FVector2D(100,0));
		W.A->StageTip = FVector(5,7,50); W.Button(PB::CapturePoint);
		for (int32 I = 0; I < Fps-1; ++I) { W.Sample(1./Fps); }
		TestTrue(TEXT("Still capturing before one second"),W.A->IsCapturingPoint()); W.Sample(1./Fps);
		TestEqual(TEXT("One point"),W.A->GetCapturedPoints().Num(),1);
		if (!W.A->GetCapturedPoints().IsEmpty())
		{
			const auto& P = W.A->GetCapturedPoints()[0]; TestTrue(TEXT("Stage ignores alignment and HomeShift"),P.StageCm.Equals(W.A->StageTip,0.0001));
			TestEqual(TEXT("Frame count"),P.Frames,Fps);
		}
		W.Button(PB::CapturePoint); W.Sample(0.3); W.A->bTracked = false; W.Sample(0.1);
		TestFalse(TEXT("Loss cancels window"),W.A->IsCapturingPoint()); TestEqual(TEXT("Keeps earlier points"),W.A->GetCapturedPoints().Num(),1);
		TestTrue(TEXT("Loss reason"),W.A->PointStatus().Contains(TEXT("not tracked")));
		W.A->bTracked = true; W.Button(PB::CapturePoint); W.Sample(0.5); W.A->StageTip.X += 2; W.Sample(0.5);
		TestEqual(TEXT("Spread rejects current point"),W.A->GetCapturedPoints().Num(),1);
		TestTrue(TEXT("Spread reported"),W.A->PointStatus().Contains(TEXT("spread")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP4hStore,"vBoR.P4h.E5VersionedPersistence",P4hFlags)
bool FP4hStore::RunTest(const FString&)
{
	FP4hWorld W; FString Why; auto S = W.SavedPoints(); FVBoRBuckSavedAlignment Loaded;
	TestTrue(TEXT("Write points"),FVBoRBuckAlignmentStore::Save(W.Path,S,Why)); TestTrue(TEXT("Load points"),FVBoRBuckAlignmentStore::Load(W.Path,Loaded,Why));
	TestEqual(TEXT("Version 2"),Loaded.Version,2); TestEqual(TEXT("Point count"),Loaded.Points.Num(),4);
	for (int32 I = 0; I < Loaded.Points.Num(); ++I)
	{
		TestEqual(TEXT("Order"),Loaded.Points[I].Order,S.Points[I].Order); TestEqual(TEXT("Frames"),Loaded.Points[I].Frames,S.Points[I].Frames);
		TestEqual(TEXT("Spread"),Loaded.Points[I].SpreadMm,S.Points[I].SpreadMm); TestTrue(TEXT("Stage roundtrip"),Loaded.Points[I].StageCm.Equals(S.Points[I].StageCm));
	}
	FVBoRBuckSavedAlignment Head; Head.SavedAt = S.SavedAt; Head.TrackingOrigin = S.TrackingOrigin; Head.PlayAreaCm = S.PlayAreaCm; Head.Capture.UsedFrames = 15;
	TestTrue(TEXT("Head overwrites points"),FVBoRBuckAlignmentStore::Save(W.Path,Head,Why)); TestTrue(TEXT("Head read"),FVBoRBuckAlignmentStore::Load(W.Path,Loaded,Why)); TestEqual(TEXT("Version 1"),Loaded.Version,1);
	TestTrue(TEXT("Points overwrite head"),FVBoRBuckAlignmentStore::Save(W.Path,S,Why));
	FString Text; FFileHelper::LoadFileToString(Text,*W.Path); const FString Good = Text;
	Text.ReplaceInline(TEXT("\"version\": 2"),TEXT("\"version\": 99")); Text.ReplaceInline(TEXT("\"version\":2"),TEXT("\"version\":99"));
	FFileHelper::SaveStringToFile(Text,*W.Path); TestFalse(TEXT("Unknown version rejected"),FVBoRBuckAlignmentStore::Load(W.Path,Loaded,Why));
	FFileHelper::SaveStringToFile(TEXT("{broken"),*W.Path); TestFalse(TEXT("Broken JSON"),FVBoRBuckAlignmentStore::Load(W.Path,Loaded,Why));
	FFileHelper::SaveStringToFile(Good,*W.Path); S.Points[1].Order = S.Points[0].Order;
	TestFalse(TEXT("Invalid values rejected"),FVBoRBuckAlignmentStore::Save(W.Path,S,Why));
	TestTrue(TEXT("Previous save retained"),FVBoRBuckAlignmentStore::Load(W.Path,Loaded,Why)); TestEqual(TEXT("Still four points"),Loaded.Points.Num(),4);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP4hBench,"vBoR.P4h.E6BenchRejectsPoints",P4hFlags)
bool FP4hBench::RunTest(const FString&)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game,false); GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	auto* Pawn = World->SpawnActor<APawn>(); Pawn->SetRootComponent(NewObject<USceneComponent>(Pawn)); Pawn->GetRootComponent()->RegisterComponent(); Pawn->SetActorLocation(FVector(23,45,6));
	auto* Bench = World->SpawnActor<AP0BuckBench>();
	const FString Path = FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("Automation"),FGuid::NewGuid().ToString()+TEXT("-P4hBench.json"));
	FVBoRBuckSavedAlignment S; S.Version = 2; S.SavedAt = TEXT("2026-10-20T00:00:00Z"); S.TrackingOrigin = EHMDTrackingOrigin::Stage; S.PlayAreaCm = FVector2D(300,400);
	S.Points = {{1,FVector(0,0,50),0,15},{2,FVector(50,0,50),0,15}}; FString Why;
	TestTrue(TEXT("Write v2"),FVBoRBuckAlignmentStore::Save(Path,S,Why)); const auto Before = Pawn->GetActorTransform();
	FP4hTestAccess::BenchLoad(Bench,Path,Pawn); TestFalse(TEXT("No bench saved head"),FP4hTestAccess::BenchHasSaved(Bench));
	TestTrue(TEXT("Pawn retained"),Pawn->GetActorTransform().Equals(Before)); TestTrue(TEXT("Point reason"),FP4hTestAccess::BenchState(Bench).Contains(TEXT("point alignment")));
	FP4hTestAccess::BenchSave(Bench); TestTrue(TEXT("Read new bench save"),FVBoRBuckAlignmentStore::Load(Path,S,Why)); TestEqual(TEXT("Bench wrote v1"),S.Version,1);
	GEngine->DestroyWorldContext(World); World->DestroyWorld(false); IFileManager::Get().Delete(*Path);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP4hApply,"vBoR.P4h.E7ApplyAndCarChanges",P4hFlags)
bool FP4hApply::RunTest(const FString&)
{
	FP4hWorld W; auto S = W.SavedPoints(); W.A->BaseEye = nullptr;
	W.A->InitializeAlignment(W.Pawn,&S,W.A->Origin,W.A->Area); TestTrue(TEXT("v2 needs no BaseEye"),W.A->State == EVBoRAlignmentState::Applied);
	W.Car->SetActorTransform(FTransform(FRotator(0,30,0),FVector(100,200,9))); W.A->FloorWorldZCm = 7;
	W.A->InitializeAlignment(W.Pawn,&S,W.A->Origin,W.A->Area);
	for (int32 I = 0; I < S.Points.Num(); ++I)
	{ TestTrue(TEXT("Current car points fitted"),FVector::Dist2D(W.Pawn->GetActorTransform().TransformPosition(S.Points[I].StageCm),W.Car->AlignPoints[I]->GetComponentLocation()) < 0.01); }
	TestEqual(TEXT("Always floor Z"),W.Pawn->GetActorLocation().Z,7.);
	auto Before = W.Pawn->GetActorTransform(); W.Car->AlignPoints[0]->Order = 99;
	W.A->InitializeAlignment(W.Pawn,&S,W.A->Origin,W.A->Area); TestEqual(TEXT("Order reason"),W.A->Reason,FString(TEXT("alignment points changed"))); TestTrue(TEXT("Order retains Pawn"),W.Pawn->GetActorTransform().Equals(Before));
	W.Car->AlignPoints[0]->Order = 1; auto Less = S; Less.Points.Pop(); W.A->InitializeAlignment(W.Pawn,&Less,W.A->Origin,W.A->Area); TestTrue(TEXT("Count needs check"),W.A->State == EVBoRAlignmentState::NeedsCheck);
	W.Car->AlignPoints[0]->AddLocalOffset(FVector(30,0,0)); W.A->InitializeAlignment(W.Pawn,&S,W.A->Origin,W.A->Area); TestEqual(TEXT("Moved point reason"),W.A->Reason,FString(TEXT("alignment points moved"))); TestTrue(TEXT("Moved retains Pawn"),W.Pawn->GetActorTransform().Equals(Before));
	W.Car->AlignPoints[0]->AddLocalOffset(FVector(-30,0,0)); W.A->InitializeAlignment(W.Pawn,&S,0,W.A->Area); TestTrue(TEXT("Origin blocks apply"),W.Pawn->GetActorTransform().Equals(Before));
	W.A->InitializeAlignment(W.Pawn,&S,W.A->Origin,FVector2D(100,100)); TestTrue(TEXT("Guardian needs check"),W.A->State == EVBoRAlignmentState::NeedsCheck);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP4hFitGate,"vBoR.P4h.E8FitSpanAndHeight",P4hFlags)
bool FP4hFitGate::RunTest(const FString&)
{
	FP4hWorld W; W.CaptureAll(5); TestTrue(TEXT("Height warning"),W.A->PointStatus().Contains(TEXT("height warning"))); TestTrue(TEXT("Height allows save"),W.Runner->EvaluateOperatorButton(PB::SavePoints).bAccepted);
	W.A->PointMinSpanCm = 1000; TestEqual(TEXT("Span rejects"),W.A->SavePointsRejection(),FString(TEXT("Points too close"))); W.A->PointMinSpanCm = 20;
	W.Car->AlignPoints[0]->AddLocalOffset(FVector(20,0,0)); TestTrue(TEXT("Fit reason has value"),W.A->SavePointsRejection().StartsWith(TEXT("Fit "))); TestFalse(TEXT("Save button rejects"),W.Runner->EvaluateOperatorButton(PB::SavePoints).bAccepted);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP4hSeats,"vBoR.P4h.E9SeatReference",P4hFlags)
bool FP4hSeats::RunTest(const FString&)
{
	for (int32 Version : {1,2})
	{
		FP4hWorld W; auto S = W.SavedPoints(); S.Version = Version; S.Capture.PositionCm = FVector(10,20,115); S.Capture.YawDeg = 30; S.Capture.UsedFrames = 15;
		auto* Driver = NewObject<UVBoRViewpointComponent>(W.Car); W.Car->AddInstanceComponent(Driver); Driver->SetupAttachment(W.Car->GetRootComponent()); Driver->ComponentTags.Add(TEXT("Car.DriverView")); Driver->RegisterComponent(); Driver->SetRelativeLocation(FVector(-30,-37,40));
		W.Car->PassengerView->SetRelativeRotation(FRotator(0,20,0)); W.A->InitializeAlignment(W.Pawn,&S,W.A->Origin,W.A->Area); auto Base = W.Pawn->GetActorTransform();
		TestTrue(TEXT("Seat request"),W.A->SwitchViewpoint(W.Car->PassengerView)); W.FinishFade();
		TestTrue(TEXT("SgRP difference"),W.Pawn->GetActorLocation().Equals(Base.GetLocation()+W.Car->PassengerView->GetComponentLocation()-Driver->GetComponentLocation(),0.0001)); TestTrue(TEXT("Ignores view Yaw"),W.Pawn->GetActorQuat().Equals(Base.GetRotation(),0.0001));
		TestTrue(TEXT("Driver request is base"),W.A->SwitchViewpoint(Driver)); W.FinishFade(); TestTrue(TEXT("At base"),W.A->IsBaseViewpoint()); TestTrue(TEXT("Exact base"),W.Pawn->GetActorTransform().Equals(Base,0.0001));
		auto* Duplicate = NewObject<UVBoRViewpointComponent>(W.Car); W.Car->AddInstanceComponent(Duplicate); Duplicate->ComponentTags.Add(TEXT("Car.DriverView")); Duplicate->RegisterComponent();
		W.A->InitializeAlignment(W.Pawn,&S,W.A->Origin,W.A->Area); TestTrue(TEXT("Duplicate driver needs check"),W.A->State == EVBoRAlignmentState::NeedsCheck); TestTrue(TEXT("Duplicate driver reason"),W.A->Reason.Contains(TEXT("Car.DriverView"))); TestTrue(TEXT("No move"),W.Pawn->GetActorTransform().Equals(Base));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP4hCommands,"vBoR.P4h.E10CommandRules",P4hFlags)
bool FP4hCommands::RunTest(const FString&)
{
	FP4hWorld W;
	TestEqual(TEXT("Save needs all"),W.Runner->EvaluateOperatorButton(PB::SavePoints).Reason,FString(TEXT("Capture all points first")));
	TestEqual(TEXT("Cancel needs session"),W.Runner->EvaluateOperatorButton(PB::CancelPoints).Reason,FString(TEXT("No point alignment in progress")));
	W.A->bHandPawn = false; TestEqual(TEXT("Requires hand pawn"),W.Runner->EvaluateOperatorButton(PB::CapturePoint).Reason,FString(TEXT("Hand tracking pawn required"))); W.A->bHandPawn = true;
	TestTrue(TEXT("P key handled"),W.Runner->HandleOperatorKey(EKeys::P)); W.Runner->ProcessFlow(0); TestTrue(TEXT("Capturing"),W.A->IsCapturingPoint());
	TestEqual(TEXT("Cannot capture twice"),W.Runner->EvaluateOperatorButton(PB::CapturePoint).Reason,FString(TEXT("Capturing a point")));
	TestEqual(TEXT("Reapply blocked"),W.Runner->EvaluateOperatorButton(PB::Reapply).Reason,FString(TEXT("Point alignment in progress")));
	FP4hTestAccess::ClearFrame(W.Runner); TestTrue(TEXT("X key handled"),W.Runner->HandleOperatorKey(EKeys::X)); W.Runner->ProcessFlow(0); TestFalse(TEXT("X clears session"),W.A->IsPointAligning());
	W.CaptureAll(); TestEqual(TEXT("All points captured"),W.Runner->EvaluateOperatorButton(PB::CapturePoint).Reason,FString(TEXT("All points captured — save or cancel")));
	FCoreDelegates::VRHeadsetRecenter.Broadcast(); TestEqual(TEXT("Recenter discards points"),W.A->GetCapturedPoints().Num(),0); TestTrue(TEXT("Recenter reason"),W.A->PointStatus().Contains(TEXT("Recenter — points cleared"))); TestTrue(TEXT("Recenter needs check"),W.A->State == EVBoRAlignmentState::NeedsCheck);
	W.Button(PB::CapturePoint); W.Button(PB::Ignore); TestTrue(TEXT("O starts"),W.Runner->IsRunning()); TestFalse(TEXT("O clears points"),W.A->IsPointAligning()); TestFalse(TEXT("O hides points"),W.A->ArePointsVisible());
	TestEqual(TEXT("Running capture rejected"),W.Runner->EvaluateOperatorButton(PB::CapturePoint).Reason,FString(TEXT("Before the flow starts only")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP4hCommandSeats,"vBoR.P4h.E10SeatAndSourceGates",P4hFlags)
bool FP4hCommandSeats::RunTest(const FString&)
{
	FP4hWorld W; auto S = W.SavedPoints(); W.A->InitializeAlignment(W.Pawn,&S,W.A->Origin,W.A->Area);
	TestTrue(TEXT("Begin seat transition"),W.A->SwitchViewpoint(W.Car->PassengerView));
	TestEqual(TEXT("Transition first"),W.Runner->EvaluateOperatorButton(PB::CapturePoint).Reason,FString(TEXT("View transition in progress")));
	W.FinishFade(); TestEqual(TEXT("Seat gate"),W.Runner->EvaluateOperatorButton(PB::CapturePoint).Reason,FString(TEXT("Return to the driver seat first")));
	W.A->ReturnToBase(); W.FinishFade();
	FVBoRFlowCommand C{EVBoRFlowCommand::CapturePoint,EVBoRCommandSource::Participant,W.Runner->GetToken()};
	TestEqual(TEXT("Participant cannot capture"),W.Runner->EvaluateCommand(C).Reason,FString(TEXT("Operator only")));
	W.CaptureAll(); W.A->RequestReapply();
	// 점 정렬 중 다시 적용 거절을 직접 확인함 (P4h D7)
	TestFalse(TEXT("Direct reapply also blocked"),W.A->IsSwitching());
	W.Button(PB::SavePoints); TestTrue(TEXT("Save starts fade"),W.A->IsSwitching());
	// 저장 직후는 점 정렬이 끝나므로 Save 는 전체 기록 조건으로 거절됨
	TestEqual(TEXT("Save no longer in session"),W.Runner->EvaluateOperatorButton(PB::SavePoints).Reason,FString(TEXT("Capture all points first")));
	W.FinishFade(); TestTrue(TEXT("Fresh P after save allowed"),W.Runner->EvaluateOperatorButton(PB::CapturePoint).bAccepted);
	W.Button(PB::CapturePoint); TestEqual(TEXT("Fresh capture starts at point one"),W.A->GetCapturedPoints().Num(),0);
	W.Button(PB::CancelPoints); W.Button(PB::Ignore); W.Button(PB::Free);
	TestEqual(TEXT("Free mode capture rejected"),W.Runner->EvaluateOperatorButton(PB::CapturePoint).Reason,FString(TEXT("Before the flow starts only")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP4hSettings,"vBoR.P4h.E11PointConfiguration",P4hFlags)
bool FP4hSettings::RunTest(const FString&)
{
	FP4hWorld W;
	W.Car->AlignPoints[1]->Order = 1; TestEqual(TEXT("Duplicate order"),W.Runner->EvaluateOperatorButton(PB::CapturePoint).Reason,FString(TEXT("Alignment point order duplicated")));
	for (int32 I = 3; I >= 0; --I)
	{
		W.Car->AlignPoints[I]->DestroyComponent();
		if (I <= 1) { TestEqual(TEXT("Zero or one point"),W.Runner->EvaluateOperatorButton(PB::CapturePoint).Reason,FString(TEXT("Alignment points missing on the car"))); }
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP4hSaveFlow,"vBoR.P4h.E12SaveFadeWithoutStart",P4hFlags)
bool FP4hSaveFlow::RunTest(const FString&)
{
	FP4hWorld W; W.CaptureAll(); FP4hTestAccess::ClearFrame(W.Runner); W.Runner->HandleOperatorKey(EKeys::S); W.Runner->ProcessFlow(0);
	TestTrue(TEXT("Save fades"),W.A->IsSwitching()); TestFalse(TEXT("Save does not start"),W.Runner->IsRunning());
	TestEqual(TEXT("Transition rejects P"),W.Runner->EvaluateOperatorButton(PB::CapturePoint).Reason,FString(TEXT("View transition in progress")));
	W.FinishFade(); TestTrue(TEXT("Applied"),W.A->State == EVBoRAlignmentState::Applied); TestFalse(TEXT("Still before flow"),W.Runner->IsRunning()); TestTrue(TEXT("Visible after save"),W.A->ArePointsVisible());
	W.A->StageTip = W.Car->AlignPoints[0]->GetComponentLocation()+FVector(3,4,2); TestTrue(TEXT("Nearest XY 5 cm"),W.A->PointStatus().Contains(TEXT("Nearest point 1: XY 5.00 cm, height +2.00 cm")));
	W.Button(PB::Reapply); W.FinishFade(); TestTrue(TEXT("A starts"),W.Runner->IsRunning()); TestFalse(TEXT("Flow hides points"),W.A->ArePointsVisible());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP4hWriteFailure,"vBoR.P4h.E12WriteFailureRetainsPoints",P4hFlags)
bool FP4hWriteFailure::RunTest(const FString&)
{
	FP4hWorld W; W.CaptureAll();
	// 파일을 부모 경로로 써서 쓰기 실패를 재현함 (권한·OS 상태에 의존하지 않게, P4h E12)
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(W.Path),true); FFileHelper::SaveStringToFile(TEXT("block parent"),*W.Path); W.A->TestPath = W.Path / TEXT("blocked.json");
	W.Button(PB::SavePoints); TestTrue(TEXT("Failed save keeps session"),W.A->IsPointAligning()); TestEqual(TEXT("Keeps all points"),W.A->GetCapturedPoints().Num(),4);
	TestFalse(TEXT("No fade"),W.A->IsSwitching()); TestTrue(TEXT("Reason shown"),W.Runner->GetOperatorStatus().LastCommand.Contains(TEXT("write failed")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP4hMock,"vBoR.P4h.E13E14MockMarkers",P4hFlags)
bool FP4hMock::RunTest(const FString&)
{
	FP4hWorld W; const TArray<FVector> Expected = {FVector(4,-60,50),FVector(4,-14,50),FVector(-44,-14,50),FVector(-44,-60,50)};
	TestEqual(TEXT("Four points"),W.Car->AlignPoints.Num(),4);
	for (int32 I = 0; I < W.Car->AlignPoints.Num(); ++I)
	{
		auto* P = W.Car->AlignPoints[I].Get(); TestEqual(TEXT("Order"),P->Order,I+1); TestTrue(TEXT("Mock label"),P->Label.StartsWith(TEXT("Mock seat"))); TestTrue(TEXT("Position cm"),P->GetRelativeLocation().Equals(Expected[I]));
		TestEqual(TEXT("No tags"),P->ComponentTags.Num(),0); TestFalse(TEXT("Hidden by default"),P->GetVisibleFlag());
		TArray<USceneComponent*> Children; P->GetChildrenComponents(true,Children);
		for (auto* Child : Children)
		{
			TestEqual(TEXT("No child tags"),Child->ComponentTags.Num(),0);
			if (const auto* Primitive = Cast<UPrimitiveComponent>(Child)) { TestTrue(TEXT("No collision"),Primitive->GetCollisionEnabled() == ECollisionEnabled::NoCollision); }
		}
	}
	W.Button(PB::CapturePoint); for (UVBoRAlignPointComponent* P : W.Car->AlignPoints) { TestTrue(TEXT("Shown during capture"),P->GetVisibleFlag()); }
	W.Button(PB::CancelPoints); for (UVBoRAlignPointComponent* P : W.Car->AlignPoints) { TestFalse(TEXT("Hidden on cancel"),P->GetVisibleFlag()); }
	return true;
}
#endif

