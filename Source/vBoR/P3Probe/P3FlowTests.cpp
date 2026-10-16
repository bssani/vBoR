#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "P3VehicleMock.h"
#include "P3FlowGameMode.h"
#include "../Interaction/VBoRScenarioRunner.h"
#include "../Interaction/VBoRParticipantZones.h"
#include "../Interaction/VBoRVehicleAlignmentComponent.h"
#include "../Interaction/VBoREyePointComponent.h"
#include "../Interaction/VBoRViewpointComponent.h"
#include "../Interaction/VBoRHingedDoor.h"
#include "../Interaction/VBoRPokeButton.h"
#include "Components/InputComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/WorldSettings.h"
#include "Interaction/IsdkPokeInteractable.h"
#include "Interaction/Surfaces/IsdkPointablePlane.h"
#include "Misc/CoreDelegates.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBorder.h"
#include "Styling/CoreStyle.h"
#include "XRTrackingSystemBase.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/StaticMesh.h"
#include "Rig/IsdkGrabInteractionRigComponent.h"
#include "Rig/IsdkRayInteractionRigComponent.h"
#include "Interaction/IsdkGrabberComponent.h"
#include "../P2Probe/P2dTestFixture.h"
#include "IsdkFunctionLibrary.h"

struct FP3FlowTestAccess
{
	static AVBoRPokeButton* Confirm(AVBoRScenarioRunner* Runner) { return Runner->ConfirmButton; }
	static AVBoRPokeButton* Action(AVBoRScenarioRunner* Runner) { return Runner->ActionButton; }
	static FText Slate(AVBoRScenarioRunner* Runner) { return Runner->PanelText->GetText(); }
	static void Key(AVBoRScenarioRunner* Runner) { Runner->KeyConfirm(); }
	static double Lock(AVBoRScenarioRunner* Runner) { return Runner->ConfirmLockRemaining; }
	static void Toggle(AVBoRScenarioRunner* Runner) { Runner->KeyOperatorInfo(); }
	static void NewInputFrame(AVBoRScenarioRunner* Runner) { Runner->LiveFrame = MAX_uint64; }
	static void Replace(AVBoRScenarioRunner* Runner) { Runner->PlacePanel(); }
	static TSharedPtr<SBorder> Border(AVBoRScenarioRunner* Runner) { return Runner->PanelBorder; }
	static TSharedPtr<STextBlock> Text(AVBoRScenarioRunner* Runner) { return Runner->PanelText; }
	static UWidgetComponent* Label(AVBoRScenarioRunner* Runner, int32 I) { return Runner->ButtonLabels[I]; }
	static UWidgetComponent* OperatorPanel(AVBoRScenarioRunner* Runner) { return Runner->OperatorPanel; }
	static TSharedPtr<STextBlock> LabelText(AVBoRScenarioRunner* Runner, int32 I) { return Runner->ButtonLabelTexts[I]; }
	static const TArray<TObjectPtr<UStaticMeshComponent>>& Plates(AP3VehicleMock* Car) { return Car->Plates; }
};

namespace
{
	FVBoRScenarioStep P3Step(const TCHAR* Id, EVBoRStepRule Rule = EVBoRStepRule::Confirm, const TCHAR* Tag = TEXT(""), EVBoRConfirmBy By = EVBoRConfirmBy::Either)
	{
		FVBoRScenarioStep Step; Step.StepId = Id; Step.Instruction = FText::FromString(TEXT("차량을 확인하세요."));
		Step.Rule = Rule; Step.TargetTag = Tag; Step.ConfirmBy = By; return Step;
	}
	void P3Poke(AVBoRPokeButton* Button)
	{
		FIsdkInteractionPointerEvent Event; Event.Type = EIsdkPointerEventType::Select; Event.Interactable = Button->Poke.Get();
		// ISDK pointable → 기존 버튼 → 진행기 delegate 경로로 보냄 (P3 E4·E19)
		Button->Poke->GetInteractionPointerEventDelegate().Broadcast(Event);
	}
	// 실제 XR 대신 합성 Stage 자세를 공급함. world cache 는 일부러 갱신하지 않음 (R1-5·R1-7)
	class FP3TrackedHead : public FXRTrackingSystemBase
	{
	public:
		FP3TrackedHead() : FXRTrackingSystemBase(nullptr) {}
		FTransform LocalHead;
		bool bTracked = true;
		virtual FName GetSystemName() const override { return TEXT("P3SyntheticHead"); }
		virtual int32 GetXRSystemFlags() const override { return 0; }
		virtual bool EnumerateTrackedDevices(TArray<int32>& Out, EXRTrackedDeviceType) override { Out.Add(0); return true; }
		virtual bool IsTracking(int32 Device) override { return bTracked && Device == 0; }
		virtual bool GetCurrentPose(int32 Device, FQuat& Rotation, FVector& Position) override
		{ Rotation = LocalHead.GetRotation(); Position = LocalHead.GetLocation(); return IsTracking(Device); }
		virtual float GetWorldToMetersScale() const override { return 100; }
		virtual void ResetOrientationAndPosition(float) override {}
		virtual bool IsHeadTrackingAllowed() const override { return true; }
	};
	struct FP3World
	{
		UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
		APawn* Pawn;
		APlayerController* Controller;
		AP3VehicleMock* Car;
		AVBoRScenarioRunner* Runner;
		FVBoRBuckSavedAlignment Saved;
		TSharedPtr<IXRTrackingSystem, ESPMode::ThreadSafe> OriginalXR;
		TSharedPtr<FP3TrackedHead, ESPMode::ThreadSafe> HeadXR;
		FP3World()
		{
			GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
			Controller = World->SpawnActor<APlayerController>();
			Pawn = World->SpawnActor<APawn>(); Pawn->SetRootComponent(NewObject<USceneComponent>(Pawn)); Pawn->GetRootComponent()->RegisterComponent(); Controller->Possess(Pawn);
			Car = World->SpawnActor<AP3VehicleMock>();
			Runner = World->SpawnActor<AVBoRScenarioRunner>(); Runner->Scenario = NewObject<UVBoRScenarioAsset>(Runner);
			World->InitializeActorsForPlay(FURL()); World->GetWorldSettings()->NotifyBeginPlay();
			Saved.SavedAt = TEXT("2026-10-13T00:00:00Z"); Saved.TrackingOrigin = 3; Saved.PlayAreaCm = FVector2D(300,400);
			Saved.Capture.PositionCm = FVector(10,20,115); Saved.Capture.YawDeg = 30;
			Apply();
			OriginalXR = GEngine->XRSystem; HeadXR = MakeShared<FP3TrackedHead, ESPMode::ThreadSafe>(); GEngine->XRSystem = HeadXR;
			Head(Car->DriverEye->GetComponentTransform());
		}
		~FP3World() { World->EndPlay(EEndPlayReason::Quit); GEngine->DestroyWorldContext(World); World->DestroyWorld(false); GEngine->XRSystem = OriginalXR; }
		void Head(const FTransform& WorldHead) { HeadXR->LocalHead = WorldHead.GetRelativeTransform(Pawn->GetActorTransform()); }
		void Ready() { Runner->ProcessFlow(0); Runner->ProcessFlow(FP3FlowTestAccess::Lock(Runner) + 0.01); }
		void Apply() { Car->Alignment->InitializeAlignment(Pawn, &Saved, 3, Saved.PlayAreaCm); }
		void Steps(std::initializer_list<FVBoRScenarioStep> Steps)
		{
			Runner->Scenario->Steps.Reset(); for (const auto& Step : Steps) { Runner->Scenario->Steps.Add(Step); } Runner->InitializeFlow();
		}
		void Command(EVBoRFlowCommand Action, EVBoRCommandSource Source = EVBoRCommandSource::Operator)
		{ if (Action == EVBoRFlowCommand::Confirm) { Ready(); } Runner->QueueCommand({Action, Source, Runner->GetToken()}); Runner->ProcessFlow(0); }
		void FinishSwitch() { Car->Alignment->AdvanceSwitch(Car->Alignment->FadeSeconds); Car->Alignment->AdvanceSwitch(Car->Alignment->FadeSeconds); }
		FVBoRDoorMotion& DoorMotion() { return const_cast<FVBoRDoorMotion&>(Car->Door->GetMotion()); }
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP2dP3FlowTest, "vBoR.P2d.P3.E12InsideCloseOpenOutsideClose", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP2dP3FlowTest::RunTest(const FString&)
{
	FP3World E;
	E.Steps({P3Step(TEXT("11"),EVBoRStepRule::DoorLatched,TEXT("Car.DriverDoor"),EVBoRConfirmBy::None),
		P3Step(TEXT("18"),EVBoRStepRule::DoorOpened,TEXT("Car.DriverDoor"),EVBoRConfirmBy::None),
		P3Step(TEXT("20"),EVBoRStepRule::DoorLatched,TEXT("Car.DriverDoor"),EVBoRConfirmBy::None)});
	AVBoRHingedDoor* Door = E.Car->Door; AVBoRAxisPart* Lever = E.Car->InsideAxisHandle;
	FVBoRDoorMotion& Motion = E.DoorMotion();
	Door->Unlock(); Motion.AngleDeg = 50; Motion.bLatchArmed = true;
	FP2dTestAccess::Apply(Door,0,Motion.Metrics.Unlocks,0);
	auto Hand = [](double Angle)
	{
		FVBoRDoorHand Input; Input.bTracked = true;
		for (FVector& P : Input.Points) { P = FQuat(FVector::ZAxisVector,FMath::DegreesToRadians(Angle)).RotateVector(FVector(85,-9,50)); }
		return Input;
	};
	auto PullInside = [&]()
	{
		FP2dTestAccess::PalmOpen(Door,Lever,0);
		const FTransform Rest = Lever->AxisPivot->GetComponentTransform();
		FP2dTestAccess::Begin(Lever,0,UIsdkFunctionLibrary::MakeGrabPoseStruct(11,Rest.TransformPosition(FVector(0,0,-2)),FQuat::Identity));
		const FVector Pulled = FQuat(FVector::XAxisVector,FMath::DegreesToRadians(25.0)).RotateVector(FVector(0,0,-2));
		FP2dTestAccess::Move(Lever,UIsdkFunctionLibrary::MakeGrabPoseStruct(11,Rest.TransformPosition(Pulled),FQuat::Identity));
		return FP2dTestAccess::Transfer(Door,Lever,Hand(Motion.AngleDeg));
	};
	TestTrue(TEXT("11 starts inside lever hold"), PullInside());
	FVBoRDoorHand Hands[2] = {Hand(0),{}}; FP2dTestAccess::Advance(Door,Hands,1); E.Runner->ProcessFlow(0);
	TestEqual(TEXT("11 closes and advances to 18"), E.Runner->GetStepIndex(),1);
	TestTrue(TEXT("11 relatches"), Motion.bLocked); Lever->Tick(1);
	TestTrue(TEXT("18 pulls actual inside lever"), PullInside());
	TestEqual(TEXT("18 unlock source is inside"), Motion.Metrics.LastUnlockHandle,1);
	Hands[0] = Hand(50); FP2dTestAccess::Advance(Door,Hands,1); E.Runner->ProcessFlow(0);
	TestEqual(TEXT("18 opens and advances to 20"), E.Runner->GetStepIndex(),2);
	Motion.ReleaseGrab(true);
	Hands[0] = {}; FP2dTestAccess::Advance(Door,Hands,0.1);
	const FQuat R(FVector::ZAxisVector,FMath::DegreesToRadians(50.0));
	Hands[0] = Hand(50); for (FVector& P : Hands[0].Points) { P = R.RotateVector(FVector(80,6,50)); }
	Hands[0].PalmNormal = R.RotateVector(FVector(0,-1,0)); FP2dTestAccess::Advance(Door,Hands,0.1);
	for (FVector& P : Hands[0].Points) { P = FVector(80,0,50); }
	FP2dTestAccess::Advance(Door,Hands,1); E.Runner->ProcessFlow(0);
	TestTrue(TEXT("20 outside push closes and relatches"), Motion.bLocked && Motion.AngleDeg == 0);
	TestTrue(TEXT("20 ends flow"), E.Runner->IsFinished());
	TestEqual(TEXT("No skipped stages"), E.Runner->SkipCount,0); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP3DuplicateTest, "vBoR.P3Flow.E1E2DuplicateAndStale", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP3DuplicateTest::RunTest(const FString&)
{
	FP3World E; E.Steps({P3Step(TEXT("02")),P3Step(TEXT("12")),P3Step(TEXT("19"))});
	E.Ready();
	const auto Token = E.Runner->GetToken();
	P3Poke(FP3FlowTestAccess::Confirm(E.Runner)); FP3FlowTestAccess::Key(E.Runner); E.Runner->ProcessFlow(0);
	TestEqual(TEXT("E1 actual poke plus key advances exactly once"), E.Runner->GetStepIndex(), 1);
	TestEqual(TEXT("E1 second command stale"), E.Runner->StaleDiscards, 1);
	TestTrue(TEXT("E1 participant result distinct from operator"), E.Runner->LastCompletion.Contains(TEXT("참가자 확인")));
	E.Runner->QueueCommand({EVBoRFlowCommand::Confirm, EVBoRCommandSource::Operator, Token}); E.Runner->ProcessFlow(0);
	TestEqual(TEXT("E2 delayed command never completes new step"), E.Runner->GetStepIndex(), 1);
	E.Steps({P3Step(TEXT("08"),EVBoRStepRule::DoorOpened,TEXT("Car.DriverDoor"),EVBoRConfirmBy::None),P3Step(TEXT("12"))});
	const auto RuleToken = E.Runner->GetToken(); E.DoorMotion().AngleDeg = 55; E.Runner->ProcessFlow(0);
	E.Runner->QueueCommand({EVBoRFlowCommand::Confirm,EVBoRCommandSource::Operator,RuleToken}); E.Runner->ProcessFlow(0);
	TestEqual(TEXT("E2 command from automatic-completion step stale"), E.Runner->GetStepIndex(),1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP3StateRuleTest, "vBoR.P3Flow.E3E4StateAndButtonBaseline", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP3StateRuleTest::RunTest(const FString&)
{
	FP3World E; E.DoorMotion().AngleDeg = 50;
	E.Steps({P3Step(TEXT("08"),EVBoRStepRule::DoorOpened,TEXT("Car.DriverDoor"),EVBoRConfirmBy::None),P3Step(TEXT("12"))});
	E.Runner->ProcessFlow(0); TestEqual(TEXT("E3 already-open completes first evaluation"),E.Runner->GetStepIndex(),1);
	E.Steps({P3Step(TEXT("12")),P3Step(TEXT("13"),EVBoRStepRule::ButtonPressed,TEXT("Car.ReachButton"),EVBoRConfirmBy::None),P3Step(TEXT("19"))});
	P3Poke(E.Car->ReachButton); E.Command(EVBoRFlowCommand::Confirm); E.Runner->ProcessFlow(0);
	TestEqual(TEXT("E4 pre-entry presses excluded"),E.Runner->GetStepIndex(),1);
	P3Poke(E.Car->ReachButton); E.Runner->ProcessFlow(0);
	TestEqual(TEXT("E4 fresh ISDK press completes"),E.Runner->GetStepIndex(),2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP3PermissionTest, "vBoR.P3Flow.E5E6ConfirmAuthorityAndRule", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP3PermissionTest::RunTest(const FString&)
{
	FP3World E; E.Steps({P3Step(TEXT("09"),EVBoRStepRule::Confirm,TEXT(""),EVBoRConfirmBy::Operator),P3Step(TEXT("10"),EVBoRStepRule::Seated,TEXT("Car.DriverSeat")),P3Step(TEXT("12"))});
	TestTrue(TEXT("Operator-only button hidden and inactive"), FP3FlowTestAccess::Confirm(E.Runner)->IsHidden() && !FP3FlowTestAccess::Confirm(E.Runner)->Poke->IsActive());
	E.Command(EVBoRFlowCommand::Confirm,EVBoRCommandSource::Participant);
	TestEqual(TEXT("E5 participant cannot confirm operator step"),E.Runner->GetStepIndex(),0);
	TestEqual(TEXT("E5 authority refusal recorded"),E.Runner->PermissionDiscards,1);
	E.Command(EVBoRFlowCommand::Confirm); TestEqual(TEXT("E5 operator succeeds"),E.Runner->GetStepIndex(),1);
	TestTrue(TEXT("E5 operator label"),E.Runner->LastCompletion.Contains(TEXT("진행자 확인")));
	E.Command(EVBoRFlowCommand::Confirm); TestEqual(TEXT("E6 unsatisfied seat never completes"),E.Runner->GetStepIndex(),1);
	TestEqual(TEXT("E6 rule refusal recorded"),E.Runner->RuleDiscards,1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP3SeatTest, "vBoR.P3Flow.E7TrackedSeatDwell", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP3SeatTest::RunTest(const FString&)
{
	FP3World E; E.Steps({P3Step(TEXT("10"),EVBoRStepRule::Seated,TEXT("Car.DriverSeat")),P3Step(TEXT("12"))});
	const FVector Eye = E.Car->DriverEye->GetComponentLocation();
	E.Car->Seat->SampleHead(true,Eye,1.4); E.Command(EVBoRFlowCommand::Confirm); TestEqual(TEXT("E7 1.4 insufficient"),E.Runner->GetStepIndex(),0);
	E.Car->Seat->SampleHead(false,FVector::ZeroVector,10); E.Car->Seat->SampleHead(false,FVector::ZeroVector,10);
	TestTrue(TEXT("E7 invalid time excluded"),FMath::IsNearlyEqual(E.Car->Seat->DwellSeconds,1.4));
	TestEqual(TEXT("E7 one pause episode"),E.Car->Seat->TrackingPauses,1);
	E.Car->Seat->SampleHead(true,Eye,10); TestTrue(TEXT("Recovery interval excluded"),FMath::IsNearlyEqual(E.Car->Seat->DwellSeconds,1.4));
	E.Car->Seat->SampleHead(true,Eye,0.1); E.Command(EVBoRFlowCommand::Confirm); TestEqual(TEXT("E7 1.5 satisfies"),E.Runner->GetStepIndex(),1);
	E.Car->Seat->SampleHead(true,Eye+FVector(0,0,26),1); TestEqual(TEXT("Standing outside top resets"),E.Car->Seat->DwellSeconds,0.0);
	E.Car->Seat->SampleHead(true,Eye,0.5); TestEqual(TEXT("Re-entry starts new dwell"),E.Car->Seat->DwellSeconds,0.5);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP3ApproachTest, "vBoR.P3Flow.E8E9IndependentApproachHysteresis", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP3ApproachTest::RunTest(const FString&)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game,false);
	AActor* Actor = World->SpawnActor<AActor>(); auto* Zone = NewObject<UVBoRApproachZoneComponent>(Actor); Actor->SetRootComponent(Zone); Zone->RegisterComponent(); Zone->SetBoxExtent(FVector(100,50,10));
	Zone->SampleHead(true,FVector(99,0,180),1);
	for (int32 I=0; I<6; ++I) { Zone->SampleHead(true,FVector(I%2 ? 90:110,0,180),2+I); }
	TestEqual(TEXT("E8 one enter"),Zone->Approaches,1); TestEqual(TEXT("E8 no boundary departure"),Zone->Departures,0);
	Zone->SampleHead(false,FVector(400,0,0),10); TestTrue(TEXT("Tracking loss preserves near state"),Zone->IsParticipantNear());
	Zone->SampleHead(true,FVector(131,0,180),11); TestEqual(TEXT("E9 no runner required for departure"),Zone->Departures,1);
	Zone->SampleHead(true,FVector(99,0,180),12); TestEqual(TEXT("E9 re-enter independently"),Zone->Approaches,2);
	TestEqual(TEXT("Timestamps retained"),Zone->LastDepartureSeconds,11.0);
	World->DestroyWorld(false); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP3ViewTest, "vBoR.P3Flow.E10E11ViewpointFadeAndExactTransform", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP3ViewTest::RunTest(const FString&)
{
	FP3World E; E.Steps({P3Step(TEXT("12"))});
	const FTransform Before=E.Pawn->GetActorTransform();
	E.Command(EVBoRFlowCommand::Passenger); E.Command(EVBoRFlowCommand::Rear); E.Command(EVBoRFlowCommand::ReturnBase);
	FP3FlowTestAccess::Toggle(E.Runner);
	TestTrue(TEXT("View request actor remains visible in operator block"),E.Runner->GetOperatorText().Contains(TEXT("시점 명령 진행자")));
	TestTrue(TEXT("E10 fade first, pawn unchanged"),E.Pawn->GetActorTransform().Equals(Before));
	TestEqual(TEXT("E10 concurrent switches discarded"),E.Car->Alignment->SwitchDiscards,2);
	E.FinishSwitch(); TestTrue(TEXT("E10 first target retained"),E.Car->Alignment->IsAtViewpoint(E.Car->PassengerView));
	for (UVBoRViewpointComponent* View : {E.Car->RearView.Get(),static_cast<UVBoRViewpointComponent*>(nullptr),E.Car->PassengerView.Get()})
	{
		if (View) { E.Car->Alignment->SwitchViewpoint(View); } else { E.Car->Alignment->ReturnToBase(); }
		E.FinishSwitch();
		const auto Expected = FVBoRBuckAlignment::CalculatePawn(E.Saved.Capture, View ? View->GetComponentTransform() : E.Car->DriverEye->GetComponentTransform());
		TestTrue(TEXT("E11 exact position <0.1mm"),E.Pawn->GetActorLocation().Equals(Expected.GetLocation(),0.01));
		TestTrue(TEXT("E11 exact yaw <0.01deg"),FMath::Abs(FMath::FindDeltaAngleDegrees(double(E.Pawn->GetActorRotation().Yaw),double(Expected.Rotator().Yaw)))<0.01);
	}
	E.Runner->ProcessFlow(50); TestFalse(TEXT("Observation never finishes merely by arrival/time"),E.Runner->IsFinished());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP3ExitTest, "vBoR.P3Flow.E12ReturnBeforeExit", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP3ExitTest::RunTest(const FString&)
{
	FP3World E; E.Car->Alignment->SwitchViewpoint(E.Car->PassengerView); E.FinishSwitch(); E.DoorMotion().AngleDeg=55;
	auto Step=P3Step(TEXT("18"),EVBoRStepRule::DoorOpened,TEXT("Car.DriverDoor"),EVBoRConfirmBy::None); Step.bRequireBaseViewpoint=true;
	E.Steps({Step,P3Step(TEXT("19"))}); E.Runner->ProcessFlow(0);
	TestEqual(TEXT("E12 open door alone insufficient while away"),E.Runner->GetStepIndex(),0);
	TestTrue(TEXT("E12 return instruction visible"),E.Runner->GetPanelText().Contains(TEXT("운전석으로 돌아가세요")));
	TestFalse(TEXT("E12 action button visible"),FP3FlowTestAccess::Action(E.Runner)->IsHidden());
	E.Command(EVBoRFlowCommand::Next); TestEqual(TEXT("E12 skip also requires base viewpoint"),E.Runner->GetStepIndex(),0);
	P3Poke(FP3FlowTestAccess::Action(E.Runner)); E.Runner->ProcessFlow(0); E.FinishSwitch(); E.Runner->ProcessFlow(0);
	TestEqual(TEXT("E12 base return allows completion"),E.Runner->GetStepIndex(),1); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP3AlignmentGateTest, "vBoR.P3Flow.E13E14AlignmentGateAndRecenter", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP3AlignmentGateTest::RunTest(const FString&)
{
	FP3World E; E.Car->Alignment->InitializeAlignment(E.Pawn,nullptr,3,E.Saved.PlayAreaCm); E.Steps({P3Step(TEXT("12"))});
	TestFalse(TEXT("E13 missing saved value blocks start"),E.Runner->IsRunning());
	TestTrue(TEXT("E13 capture instructions visible in operator block"),E.Runner->GetOperatorText().Contains(TEXT("P0-c")));
	E.Command(EVBoRFlowCommand::IgnoreAlignment); TestTrue(TEXT("E13 O starts with explicit ignored status"),E.Runner->IsRunning() && E.Runner->bAlignmentIgnored);
	FP3FlowTestAccess::Toggle(E.Runner);
	TestTrue(TEXT("E13 ignore in operator block"),E.Runner->GetOperatorText().Contains(TEXT("정렬 무시 예")));
	E.Apply(); E.Runner->bAlignmentIgnored=false; const FTransform Before=E.Pawn->GetActorTransform();
	E.Car->Alignment->InitializeAlignment(E.Pawn,&E.Saved,0,E.Saved.PlayAreaCm); E.Runner->InitializeFlow();
	TestFalse(TEXT("E13 different origin blocks start"),E.Runner->IsRunning()); TestTrue(TEXT("Different origin retains Pawn"),E.Pawn->GetActorTransform().Equals(Before));
	E.Apply(); E.Runner->InitializeFlow(); FCoreDelegates::VRHeadsetRecenter.Broadcast();
	TestTrue(TEXT("E14 recenter needs check"),E.Car->Alignment->State==EVBoRAlignmentState::NeedsCheck);
	TestTrue(TEXT("E14 recenter leaves pawn"),E.Pawn->GetActorTransform().Equals(Before));
	TestTrue(TEXT("E14 flow continues"),E.Runner->IsRunning()); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP3TagsTest, "vBoR.P3Flow.E15InvalidAndAmbiguousTags", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP3TagsTest::RunTest(const FString&)
{
	FP3World E;
	auto Wrong=P3Step(TEXT("08"),EVBoRStepRule::DoorOpened,TEXT("Car.ReachButton")); Wrong.MarkerTag=TEXT("Missing.Marker");
	E.Steps({Wrong,P3Step(TEXT("10"),EVBoRStepRule::Seated,TEXT("Missing.Seat"))});
	TestFalse(TEXT("E15 invalid tags block start"),E.Runner->IsRunning());
	TestTrue(TEXT("E15 all problems reported"),E.Runner->ValidationProblems.Contains(TEXT("Car.ReachButton")) && E.Runner->ValidationProblems.Contains(TEXT("Missing.Marker")) && E.Runner->ValidationProblems.Contains(TEXT("Missing.Seat")));
	E.Command(EVBoRFlowCommand::IgnoreAlignment); TestFalse(TEXT("O cannot bypass invalid tags"),E.Runner->IsRunning());
	AActor* Duplicate=E.World->SpawnActor<AActor>(); Duplicate->Tags.Add(TEXT("Car.DriverDoor"));
	E.Steps({P3Step(TEXT("08"),EVBoRStepRule::DoorOpened,TEXT("Car.DriverDoor"))});
	TestTrue(TEXT("Ambiguous tags reported"),E.Runner->ValidationProblems.Contains(TEXT("2개 발견")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP3DataTest, "vBoR.P3Flow.E16E18E19DataOrderCompletionAndSlate", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP3DataTest::RunTest(const FString&)
{
	FP3World E; auto First=P3Step(TEXT("19")); First.Instruction=FText::FromString(TEXT("문구 수정 확인 — 실내를 둘러보세요."));
	E.Steps({First,P3Step(TEXT("02")),P3Step(TEXT("12"))});
	TestTrue(TEXT("E19 actual Slate Korean text follows asset"),FP3FlowTestAccess::Slate(E.Runner).ToString().Contains(First.Instruction.ToString()));
	TestTrue(TEXT("E19 instruction exact"),E.Runner->GetInstructionText().EqualTo(First.Instruction));
	const FVector FixedPanel=E.Runner->Panel->GetComponentLocation(); E.Pawn->SetActorLocation(FVector(400,0,0)); E.Runner->ProcessFlow(0);
	TestTrue(TEXT("Panel remains fixed during step"),E.Runner->Panel->GetComponentLocation().Equals(FixedPanel));
	E.Command(EVBoRFlowCommand::Confirm); TestTrue(TEXT("E16 edited order followed"),E.Runner->GetPanelText().Contains(TEXT("02 · 2/3")));
	E.Command(EVBoRFlowCommand::Confirm); E.Command(EVBoRFlowCommand::Confirm); TestTrue(TEXT("E18 final finished"),E.Runner->IsFinished());
	const auto Token=E.Runner->GetToken(); E.Command(EVBoRFlowCommand::Confirm); TestEqual(TEXT("E18 no post-end advancement"),E.Runner->GetToken().Generation,Token.Generation);
	TestTrue(TEXT("E18 final Korean text"),FP3FlowTestAccess::Slate(E.Runner).ToString().Contains(TEXT("안내 완료")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP3OperatorTest, "vBoR.P3Flow.E17OperatorPreviousSkipRestart", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP3OperatorTest::RunTest(const FString&)
{
	FP3World E; E.Steps({P3Step(TEXT("02")),P3Step(TEXT("12")),P3Step(TEXT("19"))});
	E.Command(EVBoRFlowCommand::Next); TestTrue(TEXT("E17 skip actor recorded"),E.Runner->LastCompletion.Contains(TEXT("진행자 건너뛰기")));
	const auto Old=E.Runner->GetToken(); E.Command(EVBoRFlowCommand::Previous); TestEqual(TEXT("E17 previous revisits prior step"),E.Runner->GetStepIndex(),0);
	E.Command(EVBoRFlowCommand::Next); E.Runner->QueueCommand({EVBoRFlowCommand::Confirm,EVBoRCommandSource::Operator,Old}); E.Runner->ProcessFlow(0);
	TestEqual(TEXT("Revisited index does not revive old generation"),E.Runner->GetStepIndex(),1);
	E.Car->Alignment->SwitchViewpoint(E.Car->PassengerView); E.FinishSwitch(); E.DoorMotion().AngleDeg=55; E.DoorMotion().bLocked=false;
	E.Command(EVBoRFlowCommand::Restart); E.FinishSwitch(); E.Runner->ProcessFlow(0);
	TestEqual(TEXT("E17 reset to first step"),E.Runner->GetStepIndex(),0); TestTrue(TEXT("E17 reset to base"),E.Car->Alignment->IsBaseViewpoint());
	TestTrue(TEXT("E17 door closed and locked"),E.DoorMotion().bLocked && E.DoorMotion().AngleDeg==0);
	TestEqual(TEXT("E17 retries counted"),E.Runner->RetryCount,1); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP3AssemblyTest, "vBoR.P3Flow.MockAndHandRig", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP3AssemblyTest::RunTest(const FString&)
{
	FP3World E;
	TestTrue(TEXT("Default scenario 14 steps validates"),E.Runner->InitializeFlow()); TestEqual(TEXT("Default step count"),E.Runner->Scenario->Steps.Num(),14);
	TestTrue(TEXT("Car stays fixed by Stage alignment"),E.Car->GetActorTransform().Equals(FTransform::Identity));
	TestTrue(TEXT("Driver eye default"),E.Car->DriverEye->GetRelativeLocation().Equals(FVector(-20,-37,115)));
	TestTrue(TEXT("Door hinge faces rear, outside -Y"),E.Car->Door->GetActorForwardVector().Equals(FVector(-1,0,0),0.001) && E.Car->Door->GetActorRightVector().Equals(FVector(0,-1,0),0.001));
	TestEqual(TEXT("Reach button no enlargement"),E.Car->ReachButton->HitAreaScale,1.0f);
	TestTrue(TEXT("Panel Slate, world space"),E.Runner->Panel->GetSlateWidget().IsValid() && E.Runner->Panel->GetWidgetSpace()==EWidgetSpace::World);
	AVBoRPokeButton* Confirm=FP3FlowTestAccess::Confirm(E.Runner);
	TestTrue(TEXT("Panel button front faces viewer"),Confirm->GetActorUpVector().Equals(E.Runner->Panel->GetForwardVector(),0.001));
	TestTrue(TEXT("Plane uses ISDK normal"),Confirm->Plane->GetNormal().GetSafeNormal().Equals(Confirm->GetActorUpVector(),0.001));
	TestTrue(TEXT("P3 mode uses independent palm opt-in pawn"),GetDefault<AP3FlowGameMode>()->DefaultPawnClass==AP3PalmGrabPawn::StaticClass());
	AP3PalmGrabPawn* Pawn=E.World->SpawnActor<AP3PalmGrabPawn>();
	for (int32 H=0; H<2; ++H)
	{
		auto* Rig=Pawn->GetHandRig(H); TestTrue(TEXT("Palm rig enabled"),Rig->GetGrabInteraction()->IsActive());
		TestFalse(TEXT("Ray inactive"),Rig->GetRayInteraction()->IsActive()); TestFalse(TEXT("Distance forbidden"),Rig->GetGrabber()->IsGrabDetectionTypeAllowed(EIsdkGrabDetectorType::DistanceGrab));
	}
	TestFalse(TEXT("P0 still poke-only"),GetDefault<AVBoRHandRigPawn>()->bAllowPalmGrab);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP3ConfirmLockTest, "vBoR.P3Flow.R1ConfirmTimeLock", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP3ConfirmLockTest::RunTest(const FString&)
{
	FP3World E; E.Steps({P3Step(TEXT("02")), P3Step(TEXT("12")), P3Step(TEXT("19"))});
	TestTrue(TEXT("Initial confirmation hidden and poke inactive"), FP3FlowTestAccess::Confirm(E.Runner)->IsHidden() && !FP3FlowTestAccess::Confirm(E.Runner)->Poke->IsActive());
	E.Ready(); P3Poke(FP3FlowTestAccess::Confirm(E.Runner)); E.Runner->ProcessFlow(0);
	TestEqual(TEXT("A completed by actual poke"), E.Runner->GetStepIndex(), 1);
	E.Runner->ProcessFlow(0.3); FP3FlowTestAccess::NewInputFrame(E.Runner);
	P3Poke(FP3FlowTestAccess::Confirm(E.Runner)); E.Runner->ProcessFlow(0);
	TestEqual(TEXT("B not completed by next-frame double poke"), E.Runner->GetStepIndex(), 1);
	TestTrue(TEXT("Lock reason logged"), E.Runner->LastRecord.Contains(TEXT("단계 시작 직후")));
	TestTrue(TEXT("Confirmation stays hidden and inactive"), FP3FlowTestAccess::Confirm(E.Runner)->IsHidden() && !FP3FlowTestAccess::Confirm(E.Runner)->Poke->IsActive());
	E.Runner->ProcessFlow(0.4); FP3FlowTestAccess::NewInputFrame(E.Runner); FP3FlowTestAccess::Key(E.Runner); E.Runner->ProcessFlow(0);
	TestEqual(TEXT("B completed at 0.7 s"), E.Runner->GetStepIndex(), 2);
	E.Command(EVBoRFlowCommand::Next); TestTrue(TEXT("Next is available during lock"), E.Runner->IsFinished());
	E.Steps({P3Step(TEXT("01"), EVBoRStepRule::Approach, TEXT("Car.Approach"), EVBoRConfirmBy::None), P3Step(TEXT("02"))});
	E.Car->Approach->SampleHead(true, E.Car->DriverEye->GetComponentLocation(), 0); E.Runner->ProcessFlow(0);
	E.Runner->ProcessFlow(0.1); FP3FlowTestAccess::NewInputFrame(E.Runner); FP3FlowTestAccess::Key(E.Runner);
	E.Runner->ProcessFlow(1.0);
	TestEqual(TEXT("Late Space received at 0.1 s rejected even after long processing frame"), E.Runner->GetStepIndex(), 1);
	TestTrue(TEXT("Operator lock rejection counted"), E.Runner->ConfirmLockDiscards == 2);
	E.Command(EVBoRFlowCommand::Previous); TestEqual(TEXT("Previous works during lock"), E.Runner->GetStepIndex(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP3PanelTextTest, "vBoR.P3Flow.R1ParticipantAndOperatorText", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP3PanelTextTest::RunTest(const FString&)
{
	FP3World E; E.Steps({P3Step(TEXT("12"))}); E.Ready();
	const FString Text = E.Runner->GetPanelText();
	TestTrue(TEXT("Participant instruction present"), Text.Contains(TEXT("차량을 확인하세요.")));
	for (const TCHAR* Word : {TEXT("버림"), TEXT("건너뛰기"), TEXT("정렬"), TEXT("generation"), TEXT("근사 형상")})
	{ TestFalse(TEXT("No operator diagnostics on participant panel"), Text.Contains(Word)); }
	TestTrue(TEXT("Step key always visible"), E.Runner->GetOperatorText().Contains(TEXT("Space")));
	TestFalse(TEXT("Full operator diagnostics default off"), E.Runner->GetOperatorText().Contains(TEXT("버림")));
	bool bF1Bound = false;
	for (const auto& Binding : E.Runner->InputComponent->KeyBindings) { bF1Bound |= Binding.Chord.Key == EKeys::I; }
	TestTrue(TEXT("F1 is bound"), bF1Bound);
	FP3FlowTestAccess::Toggle(E.Runner); TestTrue(TEXT("F1 reveals diagnostics"), E.Runner->GetOperatorText().Contains(TEXT("버림")));
	FP3FlowTestAccess::Toggle(E.Runner); TestFalse(TEXT("Second F1 hides diagnostics"), E.Runner->GetOperatorText().Contains(TEXT("버림")));
	E.Steps({P3Step(TEXT("09"), EVBoRStepRule::Confirm, TEXT(""), EVBoRConfirmBy::Operator)}); E.Ready();
	TestTrue(TEXT("Participant waits without operator wording"), E.Runner->GetPanelText().Contains(TEXT("잠시 기다려 주세요")) && !E.Runner->GetPanelText().Contains(TEXT("진행자")));
	TestTrue(TEXT("Operator wait line shown with F1 off"), E.Runner->GetOperatorText().Contains(TEXT("진행자 확인 대기 — Space")));
	TestFalse(TEXT("Operator wait component visible with F1 off"), FP3FlowTestAccess::OperatorPanel(E.Runner)->bHiddenInGame);
	E.Steps({P3Step(TEXT("13"), EVBoRStepRule::ButtonPressed, TEXT("Car.ReachButton"), EVBoRConfirmBy::None)});
	TestTrue(TEXT("No-confirm rule wait shows skip key"), E.Runner->GetOperatorText().Contains(TEXT("→ 건너뛰기")));
	E.Command(EVBoRFlowCommand::Passenger); E.FinishSwitch(); E.Runner->ProcessFlow(0);
	TestTrue(TEXT("Away viewpoint shows Home"), E.Runner->GetOperatorText().Contains(TEXT("Home 운전석으로")));
	E.Car->Alignment->InitializeAlignment(E.Pawn, nullptr, 3, E.Saved.PlayAreaCm); E.Steps({P3Step(TEXT("12"))});
	TestTrue(TEXT("Preparation text separated from always-visible start failure"), E.Runner->GetPanelText() == TEXT("준비 중입니다") && E.Runner->GetOperatorText().Contains(TEXT("정렬 필요")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP3PanelLayoutTest, "vBoR.P3Flow.R1EyeLevelAndReadableStyle", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP3PanelLayoutTest::RunTest(const FString&)
{
	FP3World E;
	for (double Height : {120.0, 160.0}) for (double Yaw : {0.0, 90.0})
	{
		const FTransform Head(FRotator(0, Yaw, 0), FVector(0, 0, Height)); E.Head(Head); E.Steps({P3Step(TEXT("12"))});
		const FVector PanelOffset = Head.InverseTransformPosition(E.Runner->Panel->GetComponentLocation());
		TestTrue(TEXT("Panel 70 cm ahead at eye level"), FMath::IsNearlyEqual(PanelOffset.X, 70.0) && PanelOffset.Z >= -5 && PanelOffset.Z <= 0.001);
		for (AVBoRPokeButton* Button : {FP3FlowTestAccess::Confirm(E.Runner), FP3FlowTestAccess::Action(E.Runner)})
		{
			const FVector Offset = Head.InverseTransformPosition(Button->GetActorLocation());
			TestTrue(TEXT("Button ahead 40-50 cm and below 25-35 cm"), Offset.X >= 40 && Offset.X <= 50 && Offset.Z >= -35 && Offset.Z <= -25);
		}
	}
	E.Ready(); TestFalse(TEXT("Confirm name appears with button"), FP3FlowTestAccess::Label(E.Runner, 0)->bHiddenInGame);
	TestEqual(TEXT("Confirm name text"), FP3FlowTestAccess::LabelText(E.Runner, 0)->GetText().ToString(), FString(TEXT("확인")));
	E.Steps({P3Step(TEXT("09"), EVBoRStepRule::Confirm, TEXT(""), EVBoRConfirmBy::Operator)});
	TestTrue(TEXT("Confirm name hides with button"), FP3FlowTestAccess::Label(E.Runner, 0)->bHiddenInGame);
	E.Steps({P3Step(TEXT("14"), EVBoRStepRule::Viewpoint, TEXT("Car.PassengerView"))});
	TestFalse(TEXT("Action name appears with button"), FP3FlowTestAccess::Label(E.Runner, 1)->bHiddenInGame);
	TestEqual(TEXT("Action name text"), FP3FlowTestAccess::LabelText(E.Runner, 1)->GetText().ToString(), FString(TEXT("동승석으로 이동")));
	TestTrue(TEXT("Black text"), FP3FlowTestAccess::Text(E.Runner)->GetColorAndOpacity().GetSpecifiedColor() == FLinearColor::Black);
	TestEqual(TEXT("Large bold font gives about 2 cm glyphs"), FP3FlowTestAccess::Text(E.Runner)->GetFont().Size, 40.0f);
	const auto Border = FP3FlowTestAccess::Border(E.Runner);
	TestTrue(TEXT("Solid white brush explicitly selected"), Border->GetBorderImage() == FCoreStyle::Get().GetBrush("WhiteBrush"));
	const FLinearColor Background = Border->GetBorderBackgroundColor().GetSpecifiedColor();
	TestTrue(TEXT("Light opaque background"), Background.R > 0.9 && Background.G > 0.9 && Background.B > 0.9 && Background.A == 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP3HeadLogTest, "vBoR.P3Flow.R1TrackedHeadPositionLog", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP3HeadLogTest::RunTest(const FString&)
{
	FP3World E; const FVector Eye = E.Car->DriverEye->GetComponentLocation();
	E.Head(FTransform(FRotator::ZeroRotator, Eye + FVector(0, -100, 35)));
	E.Steps({P3Step(TEXT("10"), EVBoRStepRule::Seated, TEXT("Car.DriverSeat"))}); E.Runner->ProcessFlow(0);
	TestTrue(TEXT("Start location in eye coordinates and mm"), E.Runner->LastRecord.Contains(TEXT("시작 위치")) && E.Runner->LastRecord.Contains(TEXT("Y=-1000.00 Z=350.00 mm")));
	E.Head(FTransform(FRotator::ZeroRotator, Eye)); E.Car->Seat->SampleHead(true, Eye, 1.5); E.Runner->ProcessFlow(0);
	TestTrue(TEXT("First satisfied seat records near-zero head"), E.Runner->LastRecord.Contains(TEXT("착석 위치")) && E.Runner->LastRecord.Contains(TEXT("X=0.00 Y=0.00 Z=0.00 mm")));
	const FString SeatRecord = E.Runner->LastRecord; E.Runner->ProcessFlow(0.1); TestEqual(TEXT("Seat location recorded once"), E.Runner->LastRecord, SeatRecord);
	FP3FlowTestAccess::Toggle(E.Runner); E.Runner->ProcessFlow(0.9);
	TestFalse(TEXT("F1 does not log earlier than 1 s"), E.Runner->LastRecord.Contains(TEXT("머리 위치")));
	E.Runner->ProcessFlow(0.1); TestTrue(TEXT("F1 logs at 1 s"), E.Runner->LastRecord.Contains(TEXT("머리 위치")));
	const FString TrackedRecord = E.Runner->LastRecord; E.HeadXR->bTracked = false; E.Runner->ProcessFlow(5);
	TestEqual(TEXT("Tracking loss produces no position log"), E.Runner->LastRecord, TrackedRecord);
	TestTrue(TEXT("Untracked head has no coordinates"), E.Car->Alignment->DescribeHead(false, Eye).IsEmpty());
	E.HeadXR->bTracked = true; E.Command(EVBoRFlowCommand::Passenger); E.FinishSwitch();
	FTransform Head; TestTrue(TEXT("Reads relocated tracked head"), E.Car->Alignment->ReadWorldHead(Head));
	TestTrue(TEXT("Uses active viewpoint eye"), E.Car->Alignment->DescribeHead(true, Head.GetLocation()).Contains(TEXT("PassengerView")));
	FP3FlowTestAccess::Toggle(E.Runner); E.Runner->ProcessFlow(5); TestFalse(TEXT("F1 off stops periodic position log"), E.Runner->LastRecord.Contains(TEXT("머리 위치")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP3ReachPlacementTest, "vBoR.P3Flow.R1ReachButtonAboveDashboard", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP3ReachPlacementTest::RunTest(const FString&)
{
	FP3World E;
	auto Check = [&]()
	{
		const FBox Dashboard = FP3FlowTestAccess::Plates(E.Car)[11]->Bounds.GetBox();
		const FBox Button = E.Car->ReachButton->ButtonMesh->Bounds.GetBox();
		TestTrue(TEXT("Button bottom on dashboard top"), FMath::IsNearlyEqual(Button.Min.Z, Dashboard.Max.Z, 0.1));
		TestTrue(TEXT("Button XY bounds inside top"), Button.Min.X >= Dashboard.Min.X - 0.01 && Button.Max.X <= Dashboard.Max.X + 0.01 && Button.Min.Y >= Dashboard.Min.Y - 0.01 && Button.Max.Y <= Dashboard.Max.Y + 0.01);
		TestTrue(TEXT("Visible size remains 4x4x1 cm"), Button.GetSize().Equals(FVector(4, 4, 1), 0.01));
		TestEqual(TEXT("No reach hit-area enlargement"), E.Car->ReachButton->HitAreaScale, 1.0f);
	};
	Check(); E.Car->DashboardSizeCm = FVector(40, 100, 20); E.Car->DashboardCenterCm = FVector(65, 10, 95);
	E.Car->ReachButtonCm = FVector(-100, 1000, -500); E.Car->OnConstruction(E.Car->GetActorTransform()); Check();
	UMaterialInstanceDynamic* Material = Cast<UMaterialInstanceDynamic>(E.Car->ReachButton->ButtonMesh->GetMaterial(0));
	TestTrue(TEXT("Reach button has distinct base material"), Material && E.Car->ReachButton->ButtonMesh->GetMaterial(0) != FP3FlowTestAccess::Plates(E.Car)[11]->GetMaterial(0));
	if (Material) { TestTrue(TEXT("Blue color parameter exists and is set"), Material->K2_GetVectorParameterValue(TEXT("ColorParam")).Equals(FLinearColor(0.05, 0.25, 1, 1))); }
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP3RepositionTest, "vBoR.P3Flow.R1EventPanelRepositionAndOcclusion", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP3RepositionTest::RunTest(const FString&)
{
	FP3World E; const FVector Eye = E.Car->DriverEye->GetComponentLocation();
	E.Head(FTransform(FRotator::ZeroRotator, Eye + FVector(0, -100, 45))); E.Steps({P3Step(TEXT("12")), P3Step(TEXT("19"))});
	const FVector StandingPanel = E.Runner->Panel->GetComponentLocation(); E.Head(FTransform(FRotator::ZeroRotator, Eye));
	E.Car->Seat->SampleHead(true, Eye, 0.9); E.Runner->ProcessFlow(0); TestTrue(TEXT("No placement before seat threshold"), E.Runner->Panel->GetComponentLocation().Equals(StandingPanel));
	E.Car->Seat->SampleHead(true, Eye, 0.1); E.Runner->ProcessFlow(0); const FVector SeatedPanel = E.Runner->Panel->GetComponentLocation();
	TestTrue(TEXT("One placement at 1 s seated"), SeatedPanel.Equals(Eye + FVector(70, 0, 0)));
	E.Head(FTransform(FRotator(0, 90, 0), Eye)); E.Car->Seat->SampleHead(true, Eye, 2); E.Runner->ProcessFlow(2);
	TestTrue(TEXT("Continuous seat and 90 deg look do not follow head"), E.Runner->Panel->GetComponentLocation().Equals(SeatedPanel));
	E.Car->Seat->SampleHead(true, Eye + FVector(0, -100, 45), 0.1); E.Runner->ProcessFlow(0);
	E.Car->Seat->SampleHead(true, Eye, 1); E.Runner->ProcessFlow(0);
	TestTrue(TEXT("Seat re-entry places once at new yaw"), E.Runner->Panel->GetComponentLocation().Equals(Eye + FVector(0, 70, 0)));
	E.Runner->ProcessFlow(0.3); E.Runner->QueueCommand({EVBoRFlowCommand::Confirm, EVBoRCommandSource::Participant, E.Runner->GetToken()}); E.Runner->ProcessFlow(0);
	TestEqual(TEXT("Reposition confirmation refused at 0.3 s"), E.Runner->GetStepIndex(), 0);
	E.Runner->ProcessFlow(0.4); E.Runner->QueueCommand({EVBoRFlowCommand::Confirm, EVBoRCommandSource::Participant, E.Runner->GetToken()}); E.Runner->ProcessFlow(0);
	TestEqual(TEXT("Reposition confirmation allowed at 0.7 s"), E.Runner->GetStepIndex(), 1);
	const FVector FixedPanel = E.Runner->Panel->GetComponentLocation(); E.HeadXR->bTracked = false;
	E.Command(EVBoRFlowCommand::Passenger); E.FinishSwitch();
	TestTrue(TEXT("Untracked event defers placement"), E.Runner->Panel->GetComponentLocation().Equals(FixedPanel) && E.Runner->Panel->bHiddenInGame);
	E.Head(FTransform(FRotator::ZeroRotator, Eye)); E.HeadXR->bTracked = true; E.Runner->ProcessFlow(0);
	TestTrue(TEXT("First tracked frame performs deferred placement"), E.Runner->Panel->GetComponentLocation().Equals(Eye + FVector(70, 0, 0)) && !E.Runner->Panel->bHiddenInGame);
	// Stage capture 와 현재 머리를 같게 둠 (전환 후 각 eye 위치를 직접 검산, R1-7)
	E.HeadXR->LocalHead = FTransform(FRotator(0, E.Saved.Capture.YawDeg, 0), E.Saved.Capture.PositionCm);
	for (EVBoRFlowCommand Command : {EVBoRFlowCommand::Passenger, EVBoRFlowCommand::Rear, EVBoRFlowCommand::ReturnBase})
	{
		E.Command(Command); E.Car->Alignment->AdvanceSwitch(E.Car->Alignment->FadeSeconds);
		FTransform Head; E.Car->Alignment->ReadWorldHead(Head);
		TestTrue(TEXT("Placed at moved head while still fading"), E.Car->Alignment->IsSwitching() && E.Runner->Panel->GetComponentLocation().Equals(Head.GetLocation() + FVector(70, 0, 0), 0.01));
		E.Car->Alignment->AdvanceSwitch(E.Car->Alignment->FadeSeconds);
		for (const FVector Target : {E.Runner->Panel->GetComponentLocation(), FP3FlowTestAccess::Confirm(E.Runner)->GetActorLocation(), FP3FlowTestAccess::Action(E.Runner)->GetActorLocation(), FP3FlowTestAccess::OperatorPanel(E.Runner)->GetComponentLocation()})
		{
			for (const auto& Plate : FP3FlowTestAccess::Plates(E.Car))
			{ TestFalse(FString::Printf(TEXT("Eye segment clear of %s"), *Plate->GetName()), FMath::LineBoxIntersection(Plate->Bounds.GetBox(), Head.GetLocation(), Target, Target - Head.GetLocation())); }
		}
	}
	// 단계 시작도 같은 배치·가림 규칙을 따름 (E22)
	for (USceneComponent* View : {static_cast<USceneComponent*>(E.Car->DriverEye.Get()), static_cast<USceneComponent*>(E.Car->PassengerView.Get()), static_cast<USceneComponent*>(E.Car->RearView.Get())})
	{
		E.Head(View->GetComponentTransform()); E.Steps({P3Step(TEXT("12"))});
		const FVector Start = View->GetComponentLocation();
		for (const FVector Target : {E.Runner->Panel->GetComponentLocation(), FP3FlowTestAccess::Confirm(E.Runner)->GetActorLocation(), FP3FlowTestAccess::Action(E.Runner)->GetActorLocation()})
		{ for (const auto& Plate : FP3FlowTestAccess::Plates(E.Car)) { TestFalse(TEXT("Step-start eye segment clear"), FMath::LineBoxIntersection(Plate->Bounds.GetBox(), Start, Target, Target - Start)); } }
	}
	return true;
}
#endif
