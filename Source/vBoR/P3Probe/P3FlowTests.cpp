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
#include "Rig/IsdkGrabInteractionRigComponent.h"
#include "Rig/IsdkRayInteractionRigComponent.h"
#include "Interaction/IsdkGrabberComponent.h"

struct FP3FlowTestAccess
{
	static AVBoRPokeButton* Confirm(AVBoRScenarioRunner* Runner) { return Runner->ConfirmButton; }
	static AVBoRPokeButton* Action(AVBoRScenarioRunner* Runner) { return Runner->ActionButton; }
	static FText Slate(AVBoRScenarioRunner* Runner) { return Runner->PanelText->GetText(); }
	static void Key(AVBoRScenarioRunner* Runner) { Runner->KeyConfirm(); }
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
	struct FP3World
	{
		UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
		APawn* Pawn;
		APlayerController* Controller;
		AP3VehicleMock* Car;
		AVBoRScenarioRunner* Runner;
		FVBoRBuckSavedAlignment Saved;
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
		}
		~FP3World() { World->EndPlay(EEndPlayReason::Quit); GEngine->DestroyWorldContext(World); World->DestroyWorld(false); }
		void Apply() { Car->Alignment->InitializeAlignment(Pawn, &Saved, 3, Saved.PlayAreaCm); }
		void Steps(std::initializer_list<FVBoRScenarioStep> Steps)
		{
			Runner->Scenario->Steps.Reset(); for (const auto& Step : Steps) { Runner->Scenario->Steps.Add(Step); } Runner->InitializeFlow();
		}
		void Command(EVBoRFlowCommand Action, EVBoRCommandSource Source = EVBoRCommandSource::Operator)
		{ Runner->QueueCommand({Action, Source, Runner->GetToken()}); Runner->ProcessFlow(0); }
		void FinishSwitch() { Car->Alignment->AdvanceSwitch(Car->Alignment->FadeSeconds); Car->Alignment->AdvanceSwitch(Car->Alignment->FadeSeconds); }
		FVBoRDoorMotion& DoorMotion() { return const_cast<FVBoRDoorMotion&>(Car->Door->GetMotion()); }
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP3DuplicateTest, "vBoR.P3Flow.E1E2DuplicateAndStale", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP3DuplicateTest::RunTest(const FString&)
{
	FP3World E; E.Steps({P3Step(TEXT("02")),P3Step(TEXT("12")),P3Step(TEXT("19"))});
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
	TestTrue(TEXT("View request actor remains visible in panel"),E.Runner->GetPanelText().Contains(TEXT("시점 명령 진행자")));
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
	TestTrue(TEXT("E13 capture instructions visible"),E.Runner->GetPanelText().Contains(TEXT("P0-c")));
	E.Command(EVBoRFlowCommand::IgnoreAlignment); TestTrue(TEXT("E13 O starts with explicit ignored status"),E.Runner->IsRunning() && E.Runner->bAlignmentIgnored);
	TestTrue(TEXT("E13 ignore in panel"),E.Runner->GetPanelText().Contains(TEXT("정렬 무시 예")));
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
#endif
