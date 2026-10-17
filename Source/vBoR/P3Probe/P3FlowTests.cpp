#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "P3VehicleMock.h"
#include "P3FlowGameMode.h"
#include "../Interaction/VBoRScenarioRunner.h"
#include "../Interaction/VBoRFloorRoute.h"
#include "Components/SplineComponent.h"
#include "Components/ArrowComponent.h"
#include "Components/BoxComponent.h"
#include "EngineUtils.h"
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
#include "Misc/OutputDevice.h"
#include "Misc/OutputDeviceRedirector.h"

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

struct FP3bTestAccess
{
	static const TArray<TObjectPtr<UStaticMeshComponent>>& Pieces(AVBoRFloorRoute* R) { return R->Pieces; }
	static const TArray<TObjectPtr<UMaterialInstanceDynamic>>& Materials(AVBoRFloorRoute* R) { return R->ArrowMaterials; }
	static bool Arrived(AVBoRScenarioRunner* R) { return R->bRouteArrived; }
	static double Dwell(AVBoRScenarioRunner* R) { return R->RouteDwellSeconds; }
	static UArrowComponent* Arrow(AVBoRScenarioRunner* R) { return R->MarkerArrow; }
	static const TArray<TObjectPtr<UStaticMeshComponent>>& Ring(AVBoRScenarioRunner* R) { return R->MarkerRing; }
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
		FVector2D PlayArea = FVector2D(10000,10000);
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
		virtual FVector2D GetPlayAreaBounds(EHMDTrackingOrigin::Type) const override { return PlayArea; }
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
	AVBoRFloorRoute* P3bRoute(FP3World& E, const TCHAR* Tag, const TArray<FVector>& Points = {FVector(0,-300,0),FVector(400,-300,0)})
	{
		AVBoRFloorRoute* R = E.World->SpawnActorDeferred<AVBoRFloorRoute>(AVBoRFloorRoute::StaticClass(),FTransform::Identity);
		R->Tags.Add(Tag); R->Spline->SetSplinePoints(Points,ESplineCoordinateSpace::Local);
		R->FinishSpawning(FTransform::Identity); return R;
	}
	FVBoRScenarioStep P3bStep(const TCHAR* Id, const TCHAR* Tag)
	{
		auto Step = P3Step(Id); Step.RouteTag = Tag; Step.MarkerTag = TEXT("Car.DriverSeat"); return Step;
	}
	struct FP3bRecords : FOutputDevice
	{
		TArray<FString> Lines;
		FP3bRecords() { GLog->AddOutputDevice(this); }
		~FP3bRecords() { GLog->FlushThreadedLogs(); GLog->RemoveOutputDevice(this); }
		virtual void Serialize(const TCHAR* Text, ELogVerbosity::Type, const FName& Category) override
		{ if (Category == TEXT("LogP3Flow")) { Lines.Add(Text); } }
		int32 Count(const TCHAR* Text) { GLog->FlushThreadedLogs(); return Lines.FilterByPredicate([Text](const FString& Line) { return Line.Contains(Text); }).Num(); }
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
	const FVector Start = E.Car->GetActorTransform().TransformPosition(FVector(E.Car->BodySizeCm.X/2 + E.Car->StartOutsideCm,0,150.5));
	E.Head(FTransform(FRotator::ZeroRotator, Start));
	E.Steps({P3Step(TEXT("10"), EVBoRStepRule::Seated, TEXT("Car.DriverSeat"))}); E.Runner->ProcessFlow(0);
	TestTrue(TEXT("Start marker location in eye coordinates and mm"), E.Runner->LastRecord.Contains(TEXT("시작 위치")) && E.Runner->LastRecord.Contains(E.Car->Alignment->DescribeHead(true, Start)));
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
	TestTrue(TEXT("Seat re-entry places once facing seat front"), E.Runner->Panel->GetComponentLocation().Equals(Eye + FVector(70, 0, 0)));
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
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP3bVisibilityTest, "vBoR.P3b.E1E3E7CurrentRoute", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP3bVisibilityTest::RunTest(const FString&)
{
	FP3World E; auto* A = P3bRoute(E,TEXT("Route.A")); auto* B = P3bRoute(E,TEXT("Route.B"));
	TestTrue(TEXT("Before flow both hidden"), A->IsHidden() && B->IsHidden());
	E.Steps({P3bStep(TEXT("07"),TEXT("Route.A")),P3bStep(TEXT("08"),TEXT("Route.B")),P3bStep(TEXT("09"),TEXT(""))});
	TestTrue(TEXT("Only current route visible"), !A->IsHidden() && B->IsHidden() && E.Car->DriverDoorRoute->IsHidden());
	TestTrue(TEXT("Down arrow above destination"), FP3bTestAccess::Arrow(E.Runner)->GetComponentLocation().Equals(FVector(400,-300,160)) && FP3bTestAccess::Arrow(E.Runner)->GetForwardVector().Equals(-FVector::ZAxisVector,0.01));
	for (auto& Part : FP3bTestAccess::Ring(E.Runner)) { TestTrue(TEXT("Marker ring hidden during route"), Part->bHiddenInGame); }
	E.Command(EVBoRFlowCommand::Next); TestTrue(TEXT("Next route only"), A->IsHidden() && !B->IsHidden());
	E.Command(EVBoRFlowCommand::Previous); TestTrue(TEXT("Previous restores route"), !A->IsHidden() && B->IsHidden());
	E.Head(FTransform(FRotator::ZeroRotator,FVector(400,-300,160))); E.Runner->ProcessFlow(0.5);
	TestTrue(TEXT("Arrived route hidden"), A->IsHidden());
	E.Command(EVBoRFlowCommand::Restart); TestTrue(TEXT("Restart resets arrival"), !A->IsHidden() && !FP3bTestAccess::Arrived(E.Runner));
	E.Command(EVBoRFlowCommand::Next); E.Command(EVBoRFlowCommand::Next);
	TestTrue(TEXT("No route step hides all"), A->IsHidden() && B->IsHidden());
	TestFalse(TEXT("No route uses existing marker"), FP3bTestAccess::Ring(E.Runner)[0]->bHiddenInGame);
	E.Command(EVBoRFlowCommand::Restart); E.Command(EVBoRFlowCommand::EnterFreeMode);
	TestTrue(TEXT("Free mode hides routes and marker"), A->IsHidden() && B->IsHidden() && FP3bTestAccess::Arrow(E.Runner)->bHiddenInGame);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP3bShapeTest, "vBoR.P3b.E2E3E4RouteGeometryAndPulse", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP3bShapeTest::RunTest(const FString&)
{
	FP3World E; auto* R = P3bRoute(E,TEXT("Route.Shape"));
	const auto& Parts = FP3bTestAccess::Pieces(R); const auto& Materials = FP3bTestAccess::Materials(R);
	TestEqual(TEXT("Nine arrows from 25 to 329 cm"), Materials.Num(),9);
	TestEqual(TEXT("Two legs each and 48 ring segments"), Parts.Num(),66);
	for (int32 I = 0; I < Materials.Num(); ++I)
	{
		FBox Bounds(ForceInit);
		for (int32 J = 0; J < 2; ++J) { Bounds += Parts[I*2+J]->Bounds.GetBox(); }
		TestTrue(TEXT("Arrow center, width 32, length 22, floor +1 cm"), Bounds.GetCenter().Equals(FVector(25+38*I,-300,1),0.02) && Bounds.GetSize().Equals(FVector(22,32,0.2),0.02));
		const FVector Tip = Parts[I*2]->GetComponentTransform().TransformPosition(FVector(-50,0,0));
		const FVector Tail = (Parts[I*2]->GetComponentTransform().TransformPosition(FVector(50,0,0)) + Parts[I*2+1]->GetComponentTransform().TransformPosition(FVector(50,0,0)))*0.5;
		TestTrue(TEXT("V points along spline within one degree"), FVector::DotProduct((Tip-Tail).GetSafeNormal(),FVector::XAxisVector) >= FMath::Cos(FMath::DegreesToRadians(1.0)));
		TestEqual(TEXT("Stripe width 8 cm"), Parts[I*2]->GetRelativeScale3D().Y * 100,8.0);
		TestEqual(TEXT("No collision"), Parts[I*2]->GetCollisionEnabled(),ECollisionEnabled::NoCollision);
	}
	for (int32 I = 18; I < Parts.Num(); ++I)
	{
		const FTransform T = Parts[I]->GetComponentTransform();
		for (double X : {-50.0,50.0})
		{ const FVector P = T.TransformPosition(FVector(X,0,0)); TestTrue(TEXT("Ring centerline radius 30 at +1 cm"), FMath::IsNearlyEqual(FVector::Dist2D(P,FVector(400,-300,0)),30.0,0.01) && FMath::IsNearlyEqual(P.Z,1.0)); }
		TestEqual(TEXT("Ring stripe 4 cm"), Parts[I]->GetRelativeScale3D().Y * 100,4.0);
		TestTrue(TEXT("Ring sRGB yellow"), CastChecked<UMaterialInstanceDynamic>(Parts[I]->GetMaterial(0))->K2_GetVectorParameterValue(TEXT("ColorParam")).Equals(FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("e7a91f")))));
	}
	R->SetRouteVisible(true); R->Tick(0.79f);
	const FLinearColor Before = Materials[0]->K2_GetVectorParameterValue(TEXT("ColorParam"));
	R->Tick(0.14f);
	TestTrue(TEXT("Pulse advances one arrow after 0.14 s"), Materials[1]->K2_GetVectorParameterValue(TEXT("ColorParam")).Equals(Before,0.001));
	TestTrue(TEXT("Previous arrow dim at 35 percent"), Materials[0]->K2_GetVectorParameterValue(TEXT("ColorParam")).Equals(FLinearColor(R->ArrowColor.R*0.35,R->ArrowColor.G*0.35,R->ArrowColor.B*0.35,1),0.001));
	TestTrue(TEXT("Bright sRGB cyan"), Before.Equals(FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("0fa7c4")))));
	// 곡선에서도 화살표 전방을 따로 계산함 (방향 부호 오류 방지, P3b E2)
	R = P3bRoute(E,TEXT("Route.Curve"),{FVector(0,-300,0),FVector(100,-250,0),FVector(200,-300,0)});
	for (int32 I = 0; I < FP3bTestAccess::Materials(R).Num(); ++I)
	{
		const auto& P = FP3bTestAccess::Pieces(R); const FVector Tip = P[I*2]->GetComponentTransform().TransformPosition(FVector(-50,0,0));
		const FVector Tail = (P[I*2]->GetComponentTransform().TransformPosition(FVector(50,0,0))+P[I*2+1]->GetComponentTransform().TransformPosition(FVector(50,0,0)))*0.5;
		const FVector Direction = R->Spline->GetDirectionAtDistanceAlongSpline(25+38*I,ESplineCoordinateSpace::World).GetSafeNormal2D();
		TestTrue(TEXT("Curved spline arrow direction"), FVector::DotProduct((Tip-Tail).GetSafeNormal2D(),Direction) > FMath::Cos(FMath::DegreesToRadians(1.0)));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP3bArrivalTest, "vBoR.P3b.E5E6ArrivalAndLock", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP3bArrivalTest::RunTest(const FString&)
{
	FP3World E; auto* R = P3bRoute(E,TEXT("Route.Arrival"));
	E.Steps({P3bStep(TEXT("07"),TEXT("Route.Arrival")),P3Step(TEXT("08"))});
	E.Head(FTransform(FRotator(0,90,0),FVector(400,-300,160))); E.Runner->ProcessFlow(0.4);
	TestFalse(TEXT("0.4 s not arrived"), FP3bTestAccess::Arrived(E.Runner));
	E.HeadXR->bTracked = false; E.Runner->ProcessFlow(5); TestEqual(TEXT("Untracked frame paused"), FP3bTestAccess::Dwell(E.Runner),0.4);
	E.HeadXR->bTracked = true; E.Head(FTransform(FRotator::ZeroRotator,FVector(400,-331,160))); E.Runner->ProcessFlow(0.1);
	TestEqual(TEXT("Tracked exit resets dwell"), FP3bTestAccess::Dwell(E.Runner),0.0);
	E.Head(FTransform(FRotator(0,90,0),FVector(400,-300,160))); E.Runner->ProcessFlow(0.4); E.Runner->ProcessFlow(0.1);
	TestTrue(TEXT("0.5 s hides route but does not complete"), FP3bTestAccess::Arrived(E.Runner) && R->IsHidden() && E.Runner->GetStepIndex()==0);
	TestFalse(TEXT("Original marker returns"), FP3bTestAccess::Ring(E.Runner)[0]->bHiddenInGame);
	TestTrue(TEXT("Arrival panel uses end tangent despite head yaw"), E.Runner->Panel->GetComponentLocation().Equals(FVector(470,-300,160),0.01) && E.Runner->Panel->GetForwardVector().Equals(-FVector::XAxisVector,0.01));
	TestTrue(TEXT("Arrival button same direction"), FP3FlowTestAccess::Confirm(E.Runner)->GetActorLocation().Equals(FVector(445,-312,130),0.01));
	const FVector Fixed = E.Runner->Panel->GetComponentLocation();
	E.Head(FTransform(FRotator::ZeroRotator,FVector(400,-340,160))); E.Runner->ProcessFlow(0.1);
	E.Head(FTransform(FRotator::ZeroRotator,FVector(400,-300,160))); E.Runner->ProcessFlow(0.2);
	TestTrue(TEXT("No re-light or re-placement on ring re-entry"), R->IsHidden() && E.Runner->Panel->GetComponentLocation().Equals(Fixed));
	E.Runner->QueueCommand({EVBoRFlowCommand::Confirm,EVBoRCommandSource::Participant,E.Runner->GetToken()}); E.Runner->ProcessFlow(0);
	TestEqual(TEXT("Arrival confirm rejected at 0.3 s"),E.Runner->GetStepIndex(),0);
	E.Runner->ProcessFlow(0.4); E.Runner->QueueCommand({EVBoRFlowCommand::Confirm,EVBoRCommandSource::Participant,E.Runner->GetToken()}); E.Runner->ProcessFlow(0);
	TestEqual(TEXT("Arrival confirm accepted at 0.7 s"),E.Runner->GetStepIndex(),1);
	E.Command(EVBoRFlowCommand::Previous); TestTrue(TEXT("Returning to arrived step starts route again"),!R->IsHidden() && !FP3bTestAccess::Arrived(E.Runner));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP3bBoundsTest, "vBoR.P3b.E8PlayAreaAndNotices", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP3bBoundsTest::RunTest(const FString&)
{
	FP3World E; auto* R = P3bRoute(E,TEXT("Route.Bounds"));
	for (bool bUnknown : {false,true})
	{
		E.HeadXR->PlayArea = bUnknown ? FVector2D::ZeroVector : FVector2D(200,250);
		FP3bRecords Records; E.Steps({P3bStep(TEXT("07"),TEXT("Route.Bounds"))});
		TestTrue(TEXT("Unsafe or unknown route hidden"),R->IsHidden());
		const FString Expected = bUnknown ? TEXT("Floor route hidden — play area unknown") : TEXT("Floor route hidden — outside the play area");
		TestEqual(TEXT("English operator notice"),E.Runner->GetOperatorStatus().FloorRouteNotice,Expected);
		TestTrue(TEXT("Korean VR notice"),E.Runner->GetOperatorText().Contains(bUnknown ? TEXT("바닥 경로 숨김 — 영역 모름") : TEXT("바닥 경로 숨김 — 플레이 영역 밖")));
		TestFalse(TEXT("Hidden route restores original marker"),FP3bTestAccess::Ring(E.Runner)[0]->bHiddenInGame);
		E.Head(FTransform(FRotator::ZeroRotator,FVector(400,-300,160))); E.Runner->ProcessFlow(2);
		TestFalse(TEXT("Disabled route has no arrival event"),FP3bTestAccess::Arrived(E.Runner));
		TestEqual(TEXT("Notice recorded once per entry"),Records.Count(TEXT("Floor route hidden")),1);
	}
	E.HeadXR->PlayArea = FVector2D(10000,10000); E.Steps({P3bStep(TEXT("07"),TEXT("Route.Bounds"))});
	TestTrue(TEXT("Safe route visible, notice cleared"),!R->IsHidden() && E.Runner->GetOperatorStatus().FloorRouteNotice.IsEmpty());
	// Pawn 의 Stage 회전·이동을 직접 적용해 월드 경계와 혼동하지 않는지 확인 (P3b D6-b)
	E.Pawn->SetActorLocationAndRotation(FVector(200,-300,0),FRotator(0,90,0)); E.HeadXR->PlayArea=FVector2D(200,1000);
	E.Steps({P3bStep(TEXT("07"),TEXT("Route.Bounds"))}); TestFalse(TEXT("Rotated Stage wide axis accepts world-X route"),R->IsHidden());
	E.HeadXR->PlayArea=FVector2D(1000,200); E.Steps({P3bStep(TEXT("07"),TEXT("Route.Bounds"))}); TestTrue(TEXT("Swapping Stage axes rejects route"),R->IsHidden());
	E.Pawn->SetActorLocationAndRotation(FVector(200,-300,0),FRotator::ZeroRotator); E.HeadXR->PlayArea=FVector2D(510,1000);
	E.Steps({P3bStep(TEXT("07"),TEXT("Route.Bounds"))}); TestTrue(TEXT("End ring outside inset rejected even if spline inside"),R->IsHidden());
	E.HeadXR->PlayArea=FVector2D(520,1000); E.Steps({P3bStep(TEXT("07"),TEXT("Route.Bounds"))}); TestFalse(TEXT("Ring exactly at 30 cm inset accepted"),R->IsHidden());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP3bValidationTest, "vBoR.P3b.E9FootprintAndRouteValidation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP3bValidationTest::RunTest(const FString&)
{
	auto Check = [this](const TCHAR* Kind, auto Setup)
	{
		FP3World E; const FName Tag(TEXT("Route.Invalid")); Setup(E);
		E.Steps({P3bStep(TEXT("07"),*Tag.ToString())});
		TestTrue(Kind,!E.Runner->IsRunning() && E.Runner->ValidationProblems.Contains(TEXT("07")) && E.Runner->ValidationProblems.Contains(Tag.ToString()));
	};
	Check(TEXT("Path crosses footprint"),[](FP3World& E) { P3bRoute(E,TEXT("Route.Invalid"),{FVector(-400,0,0),FVector(400,0,0)}); });
	Check(TEXT("Ring crosses footprint though endpoint outside"),[](FP3World& E) { P3bRoute(E,TEXT("Route.Invalid"),{FVector(0,-300,0),FVector(0,-130,0)}); });
	Check(TEXT("Arrival panel crosses footprint"),[](FP3World& E) { P3bRoute(E,TEXT("Route.Invalid"),{FVector(400,0,0),FVector(300,0,0)}); });
	Check(TEXT("No footprint"),[](FP3World& E) { E.Car->Footprint->ComponentTags.Reset(); P3bRoute(E,TEXT("Route.Invalid")); });
	Check(TEXT("Duplicate footprint"),[](FP3World& E) { auto* B=NewObject<UBoxComponent>(E.Car); B->ComponentTags.Add(TEXT("Car.Footprint")); B->RegisterComponent(); P3bRoute(E,TEXT("Route.Invalid")); });
	Check(TEXT("Missing route tag"),[](FP3World&) {});
	Check(TEXT("Duplicate route tag"),[](FP3World& E) { P3bRoute(E,TEXT("Route.Invalid")); P3bRoute(E,TEXT("Route.Invalid")); });
	Check(TEXT("Only one spline point"),[](FP3World& E) { P3bRoute(E,TEXT("Route.Invalid"),{FVector(0,-300,0)}); });
	Check(TEXT("Wrong actor type"),[](FP3World& E) { E.Car->Tags.Add(TEXT("Route.Invalid")); });
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP3bMockTest, "vBoR.P3b.E10MockDefaults", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP3bMockTest::RunTest(const FString&)
{
	FP3World E; auto* R = E.Car->DriverDoorRoute.Get();
	TestNotNull(TEXT("Mock route spawned in BeginPlay"),R); if (!R) { return false; }
	int32 Routes=0,Footprints=0,Starts=0;
	for (TActorIterator<AActor> It(E.World); It; ++It)
	{
		if (It->ActorHasTag(TEXT("Route.ToDriverDoor"))) { ++Routes; }
		TArray<USceneComponent*> Components; It->GetComponents(Components);
		for (auto* C : Components) { Footprints += C->ComponentHasTag(TEXT("Car.Footprint")); Starts += C->ComponentHasTag(TEXT("Car.Start")); }
	}
	TestTrue(TEXT("One of each required tag"),Routes==1 && Footprints==1 && Starts==1);
	TestTrue(TEXT("Footprint body plus 20 cm, no collision"),E.Car->Footprint->GetUnscaledBoxExtent().Equals(FVector(255,112.5,72.5)) && E.Car->Footprint->GetCollisionEnabled()==ECollisionEnabled::NoCollision);
	E.Steps({P3bStep(TEXT("07"),TEXT("Route.ToDriverDoor"))});
	TestTrue(TEXT("Default route passes footprint and synthetic large room"),E.Runner->IsRunning() && E.Runner->ValidationProblems.IsEmpty() && !R->IsHidden());
	const FVector End = R->Spline->GetLocationAtSplinePoint(3,ESplineCoordinateSpace::World);
	TestTrue(TEXT("Default end near outside handle and outside footprint"),FVector::Dist2D(End,E.Car->GetActorTransform().TransformPosition(E.Car->OutsideHandlePivotCm)) <= 70 && FMath::Abs(End.Y) > E.Car->Footprint->GetScaledBoxExtent().Y);
	TestTrue(TEXT("End tangent is rearward"),R->Spline->GetDirectionAtDistanceAlongSpline(R->Spline->GetSplineLength(),ESplineCoordinateSpace::World).Equals(-FVector::XAxisVector,0.01));
	UVBoRScenarioAsset* Defaults = NewObject<UVBoRScenarioAsset>();
	for (const auto& Step : Defaults->Steps) { TestTrue(TEXT("Constructor routes remain unassigned"),Step.RouteTag.IsNone()); }
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP3bFacingTest, "vBoR.P3b.E11ViewpointFacing", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP3bFacingTest::RunTest(const FString&)
{
	FP3World E;
	E.Head(FTransform(FRotator(0,30,0),FVector(400,-300,160))); E.Steps({P3Step(TEXT("12"))});
	TestTrue(TEXT("Standing preserves head yaw"),E.Runner->Panel->GetComponentLocation().Equals(FVector(400,-300,160)+FRotator(0,30,0).Vector()*70,0.01));
	const FVector Driver = E.Car->DriverEye->GetComponentLocation(); E.Car->Seat->SampleHead(true,Driver,1);
	E.Head(FTransform(FRotator(0,90,0),Driver)); E.Steps({P3Step(TEXT("12"))});
	TestTrue(TEXT("Seated step uses driver front"),E.Runner->Panel->GetComponentLocation().Equals(Driver+FVector(70,0,0),0.01));
	for (const auto Action : {EVBoRFlowCommand::Passenger,EVBoRFlowCommand::Rear})
	{
		E.Car->Seat->SampleHead(true,Driver+FVector(0,-100,0),0);
		auto* View = Action==EVBoRFlowCommand::Passenger ? E.Car->PassengerView.Get() : E.Car->RearView.Get();
		View->SetRelativeRotation(FRotator(0,25,0)); E.Command(Action); E.FinishSwitch();
		const FTransform Head(FRotator(0,55,0),View->GetComponentLocation()); E.Head(Head); FP3FlowTestAccess::Replace(E.Runner);
		TestTrue(TEXT("Viewpoint getter uses active seat"),E.Car->Alignment->GetCurrentEye()==View);
		TestTrue(TEXT("Away view ignores head yaw on placement"),E.Runner->Panel->GetComponentLocation().Equals(Head.GetLocation()+FRotator(0,25,0).Vector()*70,0.01));
		E.Steps({P3Step(TEXT("12"))}); TestTrue(TEXT("Away step start uses seat yaw"),E.Runner->Panel->GetComponentLocation().Equals(Head.GetLocation()+FRotator(0,25,0).Vector()*70,0.01));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP3bOperatorPlateTest, "vBoR.P3b.E12OperatorPlateOcclusion", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP3bOperatorPlateTest::RunTest(const FString&)
{
	FP3World E; E.Steps({P3Step(TEXT("12"))});
	for (const auto Action : {EVBoRFlowCommand::ReturnBase,EVBoRFlowCommand::Passenger,EVBoRFlowCommand::Rear})
	{
		if (Action!=EVBoRFlowCommand::ReturnBase) { E.Command(Action); E.FinishSwitch(); }
		const FVector Eye = E.Car->Alignment->GetCurrentEye()->GetComponentLocation(); E.Head(FTransform(FRotator::ZeroRotator,Eye));
		E.Car->Seat->SampleHead(true,Eye,1); FP3FlowTestAccess::Replace(E.Runner);
		for (bool bInfo : {false,true})
		{
			if (bInfo) { FP3FlowTestAccess::Toggle(E.Runner); }
			E.Runner->ProcessFlow(0); auto* Plate=FP3FlowTestAccess::OperatorPanel(E.Runner);
			const FVector2D Size=Plate->GetDrawSize(); const FBox LocalBox(FVector(-0.1,-Size.X/2,-Size.Y/2),FVector(0.1,Size.X/2,Size.Y/2));
			const FTransform Transform=Plate->GetComponentTransform(); const FVector LocalEye=Transform.InverseTransformPosition(Eye);
			for (const FVector Target : {E.Runner->Panel->GetComponentLocation(),FP3FlowTestAccess::Confirm(E.Runner)->GetActorLocation(),FP3FlowTestAccess::Action(E.Runner)->GetActorLocation(),FP3FlowTestAccess::Label(E.Runner,0)->GetComponentLocation(),FP3FlowTestAccess::Label(E.Runner,1)->GetComponentLocation()})
			{
				const FVector LocalTarget=Transform.InverseTransformPosition(Target);
				TestFalse(TEXT("Eye segment to guidance/buttons/names misses info rectangle"),FMath::LineBoxIntersection(LocalBox,LocalEye,LocalTarget,LocalTarget-LocalEye));
			}
			for (const auto& CarPlate : FP3FlowTestAccess::Plates(E.Car))
			{ const FVector Target=Plate->GetComponentLocation(); TestFalse(TEXT("Info plate center visible past car plates"),FMath::LineBoxIntersection(CarPlate->Bounds.GetBox(),Eye,Target,Target-Eye)); }
			if (bInfo)
			{
				TestTrue(TEXT("Info beside guidance at same distance and height with gap"),FMath::IsNearlyEqual(Plate->GetComponentLocation().X,E.Runner->Panel->GetComponentLocation().X,0.01) && FMath::IsNearlyEqual(Plate->GetComponentLocation().Z,E.Runner->Panel->GetComponentLocation().Z,0.01) && FMath::Abs(Plate->GetComponentLocation().Y-E.Runner->Panel->GetComponentLocation().Y)>=52-0.01);
				FP3FlowTestAccess::Toggle(E.Runner);
			}
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP3bStartTest, "vBoR.P3b.E13StartMarkerRecords", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP3bStartTest::RunTest(const FString&)
{
	FP3World E; FP3bRecords Records;
	const FVector Start=E.Car->GetActorTransform().TransformPosition(FVector(485,0,0.5));
	E.Head(FTransform(FRotator::ZeroRotator,Start+FVector(51,0,160))); E.Steps({P3Step(TEXT("01")),P3Step(TEXT("02"))}); E.Runner->ProcessFlow(0.1);
	TestEqual(TEXT("Outside start marker does not record"),Records.Count(TEXT("시작 위치 |")),0);
	E.Head(FTransform(FRotator::ZeroRotator,Start+FVector(0,0,99))); E.Runner->ProcessFlow(0.1); TestEqual(TEXT("Headset held low does not record"),Records.Count(TEXT("시작 위치 |")),0);
	E.Head(FTransform(FRotator::ZeroRotator,Start+FVector(50,0,100))); E.HeadXR->bTracked=false; E.Runner->ProcessFlow(1);
	TestEqual(TEXT("Untracked start ignored"),Records.Count(TEXT("시작 위치 |")),0);
	E.HeadXR->bTracked=true; E.Runner->ProcessFlow(0.1); E.Runner->ProcessFlow(0.1);
	TestEqual(TEXT("Threshold 50 cm / 100 cm records once"),Records.Count(TEXT("시작 위치 |")),1);
	E.Command(EVBoRFlowCommand::Next); TestEqual(TEXT("Visited marker has no fallback"),Records.Count(TEXT("시작 표시를 거치지 않음")),0);
	E.Head(FTransform(FRotator::ZeroRotator,FVector(0,0,160))); E.Steps({P3Step(TEXT("01")),P3Step(TEXT("02"))}); E.Command(EVBoRFlowCommand::Next);
	TestEqual(TEXT("Leaving first step records fallback once"),Records.Count(TEXT("시작 표시를 거치지 않음")),1);
	for (const auto& Line : Records.Lines) { if (Line.Contains(TEXT("시작 표시를 거치지 않음"))) { TestTrue(TEXT("Fallback includes coordinates"),Line.Contains(TEXT("mm"))); } }
	TArray<USceneComponent*> Components; E.Car->GetComponents(Components);
	for (auto* C : Components) { C->ComponentTags.Remove(TEXT("Car.Start")); }
	E.Steps({P3Step(TEXT("01"))}); E.Runner->ProcessFlow(0.1); TestEqual(TEXT("No marker keeps first tracked frame behavior"),Records.Count(TEXT("시작 위치 |")),2);
	return true;
}
#endif
