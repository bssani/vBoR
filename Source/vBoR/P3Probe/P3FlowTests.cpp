#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "HAL/IConsoleManager.h"
#include "P3VehicleMock.h"
#include "../P1Probe/P1TrunkMock.h"
#include "../Interaction/VBoRBoxStand.h"
#include "../Interaction/VBoRCarryObject.h"
#include "../Interaction/VBoRLoadTarget.h"
#include "../Interaction/VBoRPoweredPanel.h"
#include "../Interaction/VBoRFoldingSeat.h"
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
#include "../Interaction/VBoRKeyTable.h"
#include "../Spectator/VBoRSpectatorModel.h"
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

struct FP4cTestAccess
{
	// 쥔 상태·키 이벤트만 합성함. 실제 잡기·버튼 필터는 P4a 시험 범위 (P4c §5)
	static void Hold(AVBoRKeyFob* Key, int32 Hand = 0) { Key->HoldingHand = Hand; }
	static FIntPoint Counts(AVBoRScenarioRunner* R, AVBoRKeyFob* Key) { return R->KeyCounts.FindRef(Key); }
	static bool Pending(AVBoRScenarioRunner* R) { return R->bStartHeadPending; }
	static bool Bound(AVBoRScenarioRunner* R, AVBoRKeyFob* Key)
	{ return Key->OnUnlockPressed.IsAlreadyBound(R, &AVBoRScenarioRunner::HandleKeyUnlock) || Key->OnLockPressed.IsAlreadyBound(R, &AVBoRScenarioRunner::HandleKeyLock); }
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP3bHomeRouteTest, "vBoR.P3b.E11HomeIgnorePlayArea", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP3bHomeRouteTest::RunTest(const FString&)
{
	IConsoleVariable* Variable = IConsoleManager::Get().FindConsoleVariable(TEXT("vBoR.RouteIgnorePlayArea"));
	if (!TestNotNull(TEXT("Home route switch registered"),Variable)) { return false; }
	const int32 Previous = Variable->GetInt();
	FP3World E; auto* R = P3bRoute(E,TEXT("Route.Bounds"));
	for (bool bUnknown : {false,true})
	{
		Variable->Set(1, ECVF_SetByConsole);
		E.HeadXR->PlayArea = bUnknown ? FVector2D::ZeroVector : FVector2D(200,250);
		FP3bRecords Records; E.Steps({P3bStep(TEXT("07"),TEXT("Route.Bounds"))});
		TestFalse(TEXT("Switch draws route outside or without play area"),R->IsHidden());
		TestEqual(TEXT("English operator notice says check is off"),E.Runner->GetOperatorStatus().FloorRouteNotice,FString(TEXT("Floor route shown — play area check off (home test)")));
		TestTrue(TEXT("Korean VR notice says check is off"),E.Runner->GetOperatorText().Contains(TEXT("바닥 경로 표시 — 영역 검사 끔 (집 시험)")));
		TestTrue(TEXT("Route marker replaces target ring"),FP3bTestAccess::Ring(E.Runner)[0]->bHiddenInGame);
		E.Runner->ProcessFlow(0.1); E.Runner->ProcessFlow(0.1);
		TestEqual(TEXT("Notice recorded once per entry, no per-frame requery"),Records.Count(TEXT("play area check off")),1);
		E.Head(FTransform(FRotator::ZeroRotator,FVector(400,-300,160))); E.Runner->ProcessFlow(0.6);
		TestTrue(TEXT("Arrival still hides route"),FP3bTestAccess::Arrived(E.Runner) && R->IsHidden());
	}
	Variable->Set(0, ECVF_SetByConsole); E.HeadXR->PlayArea = FVector2D(200,250); E.Steps({P3bStep(TEXT("07"),TEXT("Route.Bounds"))});
	TestTrue(TEXT("Switch off hides unsafe route again"),R->IsHidden() && E.Runner->GetOperatorStatus().FloorRouteNotice == TEXT("Floor route hidden — outside the play area"));
	Variable->Set(1, ECVF_SetByConsole); E.HeadXR->PlayArea = FVector2D(10000,10000); E.Steps({P3bStep(TEXT("07"),TEXT("Route.Bounds"))});
	TestTrue(TEXT("Safe route needs no notice even with switch on"),!R->IsHidden() && E.Runner->GetOperatorStatus().FloorRouteNotice.IsEmpty());
	Variable->Set(Previous, ECVF_SetByConsole);
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
	TestTrue(TEXT("End tangent is forward (P4d D4)"),R->Spline->GetDirectionAtDistanceAlongSpline(R->Spline->GetSplineLength(),ESplineCoordinateSpace::World).Equals(FVector::XAxisVector,0.01));
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP4cHeldTest, "vBoR.P4c.E1E2E3KeyHeld", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP4cHeldTest::RunTest(const FString&)
{
	FP3World E; AVBoRKeyFob* Key = E.Car->FrontKeyTable->Key;
	E.Steps({P3Step(TEXT("01"),EVBoRStepRule::KeyHeld,TEXT("Car.FrontKey"),EVBoRConfirmBy::None),
		P3Step(TEXT("02"),EVBoRStepRule::KeyUnlockAndLock,TEXT("Car.FrontKey"))});
	E.Runner->ProcessFlow(0); TestEqual(TEXT("E1 unheld waits"),E.Runner->GetStepIndex(),0);
	Key->OnUnlockPressed.Broadcast(Key); Key->OnLockPressed.Broadcast(Key);
	FP4cTestAccess::Hold(Key); E.Runner->ProcessFlow(0);
	TestEqual(TEXT("E1 held advances to 02"),E.Runner->GetStepIndex(),1);
	TestTrue(TEXT("E1 participant rule recorded"),E.Runner->LastCompletion.Contains(TEXT("참가자 규칙")));
	E.Command(EVBoRFlowCommand::Confirm); TestEqual(TEXT("E3 events before 02 ignored"),E.Runner->GetStepIndex(),1);
	Key->OnUnlockPressed.Broadcast(Key); Key->OnLockPressed.Broadcast(Key);
	E.Command(EVBoRFlowCommand::Previous);
	TestEqual(TEXT("E2/E3 entering 01 cannot finish in same frame"),E.Runner->GetStepIndex(),0);
	E.Runner->ProcessFlow(0); TestEqual(TEXT("E3 held key returns to 02 next frame"),E.Runner->GetStepIndex(),1);
	E.Command(EVBoRFlowCommand::Confirm); TestEqual(TEXT("E3 new baseline ignores earlier 02 presses"),E.Runner->GetStepIndex(),1);
	TestTrue(TEXT("E2 reinitialize already holding key"),E.Runner->InitializeFlow());
	TestEqual(TEXT("E2 initialization enters 01"),E.Runner->GetStepIndex(),0);
	E.Runner->ProcessFlow(0); TestEqual(TEXT("E2 next frame advances"),E.Runner->GetStepIndex(),1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP4cButtonsTest, "vBoR.P4c.E4E5KeyButtons", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP4cButtonsTest::RunTest(const FString&)
{
	for (bool bUnlockFirst : {false,true})
	{
		FP3World E; AVBoRKeyFob* Key = E.Car->FrontKeyTable->Key; AVBoRKeyFob* Rear = E.Car->RearKeyTable->Key;
		E.Steps({P3Step(TEXT("01"),EVBoRStepRule::KeyHeld,TEXT("Car.FrontKey"),EVBoRConfirmBy::None),
			P3Step(TEXT("02"),EVBoRStepRule::KeyUnlockAndLock,TEXT("Car.FrontKey")),P3Step(TEXT("03"))});
		Key->OnUnlockPressed.Broadcast(Key); Key->OnLockPressed.Broadcast(Key);
		FP4cTestAccess::Hold(Key); E.Runner->ProcessFlow(0);
		E.Command(EVBoRFlowCommand::Confirm); TestEqual(TEXT("E5 01 presses ignored"),E.Runner->GetStepIndex(),1);
		Rear->OnUnlockPressed.Broadcast(Rear); Rear->OnLockPressed.Broadcast(Rear);
		E.Command(EVBoRFlowCommand::Confirm); TestEqual(TEXT("E5 rear presses ignored"),E.Runner->GetStepIndex(),1);
		TestTrue(TEXT("E4 no confirmation button before both"),FP3FlowTestAccess::Confirm(E.Runner)->IsHidden());
		if (bUnlockFirst) { Key->OnUnlockPressed.Broadcast(Key); } else { Key->OnLockPressed.Broadcast(Key); }
		E.Command(EVBoRFlowCommand::Confirm); TestEqual(TEXT("E4 one kind waits"),E.Runner->GetStepIndex(),1);
		if (bUnlockFirst) { Key->OnLockPressed.Broadcast(Key); } else { Key->OnUnlockPressed.Broadcast(Key); }
		E.Runner->ProcessFlow(0);
		TestEqual(TEXT("E4 both require confirmation"),E.Runner->GetStepIndex(),1);
		TestFalse(TEXT("E4 participant confirmation visible"),FP3FlowTestAccess::Confirm(E.Runner)->IsHidden());
		E.Command(EVBoRFlowCommand::Confirm);
		TestEqual(TEXT("E4 Space advances after either order"),E.Runner->GetStepIndex(),2);
		TestTrue(TEXT("E4 operator confirmation recorded"),E.Runner->LastCompletion.Contains(TEXT("진행자 확인")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP4cBindingTest, "vBoR.P4c.E6KeyBindings", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP4cBindingTest::RunTest(const FString&)
{
	FP3World E; AVBoRKeyFob* Key = E.Car->FrontKeyTable->Key;
	E.Steps({P3Step(TEXT("02"),EVBoRStepRule::KeyUnlockAndLock,TEXT("Car.FrontKey"))});
	TestTrue(TEXT("E6 reinitialize"),E.Runner->InitializeFlow());
	Key->OnUnlockPressed.Broadcast(Key); Key->OnLockPressed.Broadcast(Key);
	TestEqual(TEXT("E6 one broadcast counted once"),FP4cTestAccess::Counts(E.Runner,Key),FIntPoint(1,1));
	E.Steps({P3Step(TEXT("02"),EVBoRStepRule::KeyUnlockAndLock,TEXT("Car.RearKey"))});
	TestFalse(TEXT("E6 old target unbound"),FP4cTestAccess::Bound(E.Runner,Key));
	E.Steps({P3Step(TEXT("02"),EVBoRStepRule::KeyUnlockAndLock,TEXT("Car.FrontKey"))});
	TestTrue(TEXT("E6 target bound"),FP4cTestAccess::Bound(E.Runner,Key));
	E.Runner->Destroy(); TestFalse(TEXT("E6 EndPlay unbound"),FP4cTestAccess::Bound(E.Runner,Key));
	Key->OnUnlockPressed.Broadcast(Key); Key->OnLockPressed.Broadcast(Key);
	TestEqual(TEXT("E6 EndPlay receives no events"),FP4cTestAccess::Counts(E.Runner,Key),FIntPoint::ZeroValue);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP4cValidationTest, "vBoR.P4c.E7KeyValidation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP4cValidationTest::RunTest(const FString&)
{
	FP3World E;
	for (EVBoRStepRule Rule : {EVBoRStepRule::KeyHeld,EVBoRStepRule::KeyUnlockAndLock})
	{
		for (const TCHAR* Tag : {TEXT("Car.DriverDoor"),TEXT("Car.MissingKey")})
		{
			E.Runner->Scenario->Steps = {P3Step(TEXT("01"),Rule,Tag)};
			TestFalse(TEXT("E7 invalid target blocks initialization"),E.Runner->InitializeFlow());
			TestFalse(TEXT("E7 flow stopped"),E.Runner->IsRunning());
			TestTrue(TEXT("E7 mismatch diagnostic"),E.Runner->ValidationProblems.Contains(TEXT("종류 불일치")));
			if (FName(Tag) == TEXT("Car.MissingKey")) { TestTrue(TEXT("E7 missing tag diagnostic"),E.Runner->ValidationProblems.Contains(TEXT("0개 발견"))); }
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP4cStartTest, "vBoR.P4c.E8StartAttempts", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP4cStartTest::RunTest(const FString&)
{
	FP3World E; FP3bRecords Records;
	const FTransform Outside(FRotator::ZeroRotator,FVector(0,0,160));
	const FTransform Start(FRotator::ZeroRotator,E.Car->GetActorTransform().TransformPosition(FVector(485,0,160.5)));
	E.Head(Outside); E.Steps({P3Step(TEXT("01")),P3Step(TEXT("02"))});
	E.Command(EVBoRFlowCommand::Next); E.Command(EVBoRFlowCommand::Previous); E.Command(EVBoRFlowCommand::Next);
	TestEqual(TEXT("E8.1 revisiting 01 does not repeat fallback"),Records.Count(TEXT("시작 표시를 거치지 않음")),1);
	TestFalse(TEXT("E8.1 revisiting 01 never rearms"),FP4cTestAccess::Pending(E.Runner));
	E.Runner->InitializeFlow(); E.Command(EVBoRFlowCommand::Previous);
	TestTrue(TEXT("E8.2 previous at 01 preserves pending"),FP4cTestAccess::Pending(E.Runner));
	TestEqual(TEXT("E8.2 previous at 01 no fallback"),Records.Count(TEXT("시작 표시를 거치지 않음")),1);
	E.Head(Start); E.Runner->ProcessFlow(0); E.Command(EVBoRFlowCommand::Previous);
	TestFalse(TEXT("E8.2 recorded start remains recorded"),FP4cTestAccess::Pending(E.Runner));
	E.Runner->ProcessFlow(0); TestEqual(TEXT("E8.2 previous no duplicate start"),Records.Count(TEXT("시작 위치 |")),1);
	E.Head(Outside); E.Runner->InitializeFlow(); E.Command(EVBoRFlowCommand::Restart);
	TestEqual(TEXT("E8.3 R discards attempt without fallback"),Records.Count(TEXT("시작 표시를 거치지 않음")),1);
	TestTrue(TEXT("E8.3 R rearms on base"),FP4cTestAccess::Pending(E.Runner));
	E.Head(Start); E.Runner->ProcessFlow(0); TestEqual(TEXT("E8.3 start after R recorded"),Records.Count(TEXT("시작 위치 |")),2);
	E.Head(Outside); E.Runner->InitializeFlow(); E.Command(EVBoRFlowCommand::Passenger); E.FinishSwitch();
	E.Head(Outside); E.Command(EVBoRFlowCommand::Restart);
	TestEqual(TEXT("E8.4 R at passenger no fallback"),Records.Count(TEXT("시작 표시를 거치지 않음")),1);
	TestFalse(TEXT("E8.4 return transition does not record old attempt"),FP4cTestAccess::Pending(E.Runner));
	E.FinishSwitch(); E.Head(Outside); E.Runner->ProcessFlow(0);
	TestEqual(TEXT("E8.4 returned to 01"),E.Runner->GetStepIndex(),0);
	TestTrue(TEXT("E8.4 return rearms start"),FP4cTestAccess::Pending(E.Runner));
	E.Head(Start); E.Runner->ProcessFlow(0); TestEqual(TEXT("E8.4 start after return recorded"),Records.Count(TEXT("시작 위치 |")),3);
	// 떠나는 입력 네 가지가 모두 같은 fallback 경로를 씀 (P4c D3)
	for (EVBoRFlowCommand Action : {EVBoRFlowCommand::Next,EVBoRFlowCommand::Confirm,EVBoRFlowCommand::EnterFreeMode})
	{
		E.Head(Outside); E.Runner->InitializeFlow(); const int32 Before = Records.Count(TEXT("시작 표시를 거치지 않음"));
		E.Command(Action); TestEqual(TEXT("E8 leave records once"),Records.Count(TEXT("시작 표시를 거치지 않음")),Before+1);
	}
	E.Head(Outside); E.Steps({P3Step(TEXT("01"),EVBoRStepRule::KeyHeld,TEXT("Car.FrontKey"),EVBoRConfirmBy::None),P3Step(TEXT("02"))});
	const int32 Before = Records.Count(TEXT("시작 표시를 거치지 않음")); FP4cTestAccess::Hold(E.Car->FrontKeyTable->Key); E.Runner->ProcessFlow(0);
	TestEqual(TEXT("E8 rule leave records once"),Records.Count(TEXT("시작 표시를 거치지 않음")),Before+1);
	E.Car->Alignment->State = EVBoRAlignmentState::NoSavedValue; TestFalse(TEXT("Alignment blocks new attempt"),E.Runner->InitializeFlow());
	E.Command(EVBoRFlowCommand::IgnoreAlignment); TestTrue(TEXT("O starts pending attempt"),FP4cTestAccess::Pending(E.Runner));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP4cAssetTest, "vBoR.P4c.E9ScenarioAsset", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP4cAssetTest::RunTest(const FString&)
{
	const auto* Asset = LoadObject<UVBoRScenarioAsset>(nullptr,TEXT("/Game/P3/DA_P3_Scenario.DA_P3_Scenario"));
	if (!TestNotNull(TEXT("E9 asset loads"),Asset)) { return false; }
	struct FExpected { const TCHAR* Id; const TCHAR* Text; EVBoRStepRule Rule; const TCHAR* Tag; EVBoRConfirmBy By; };
	const FExpected Expected[] = {
		{TEXT("01"),TEXT("테이블의 키를 집으세요."),EVBoRStepRule::KeyHeld,TEXT("Car.FrontKey"),EVBoRConfirmBy::None},
		{TEXT("02"),TEXT("키 버튼으로 외장 조명을 켜고 꺼 보세요."),EVBoRStepRule::KeyUnlockAndLock,TEXT("Car.FrontKey"),EVBoRConfirmBy::Either},
		{TEXT("03"),TEXT("차량 주변을 둘러보세요."),EVBoRStepRule::Confirm,TEXT(""),EVBoRConfirmBy::Either},
		{TEXT("04"),TEXT("리프트게이트를 열어보세요."),EVBoRStepRule::PoweredOpen,TEXT("Car.Liftgate"),EVBoRConfirmBy::None},
		{TEXT("05"),TEXT("박스를 적재해 보세요."),EVBoRStepRule::Confirm,TEXT(""),EVBoRConfirmBy::Operator},
		{TEXT("06"),TEXT("리프트게이트를 닫아보세요."),EVBoRStepRule::PoweredClosed,TEXT("Car.Liftgate"),EVBoRConfirmBy::None},
		{TEXT("07"),TEXT("도어 핸들을 조작해 보세요."),EVBoRStepRule::DoorUnlocked,TEXT("Car.DriverDoor"),EVBoRConfirmBy::None},
		{TEXT("08"),TEXT("문을 열어보세요."),EVBoRStepRule::DoorOpened,TEXT("Car.DriverDoor"),EVBoRConfirmBy::None},
		{TEXT("09"),TEXT("차량에 탑승해 보세요."),EVBoRStepRule::Confirm,TEXT(""),EVBoRConfirmBy::Operator},
		{TEXT("10"),TEXT("좌석에 앉아보세요."),EVBoRStepRule::Seated,TEXT("Car.DriverSeat"),EVBoRConfirmBy::Operator},
		{TEXT("11"),TEXT("문을 닫아보세요."),EVBoRStepRule::DoorLatched,TEXT("Car.DriverDoor"),EVBoRConfirmBy::None},
		{TEXT("12"),TEXT("실내를 둘러보세요."),EVBoRStepRule::Confirm,TEXT(""),EVBoRConfirmBy::Either},
		{TEXT("13"),TEXT("가장 먼 버튼까지 손가락을 뻗어보세요."),EVBoRStepRule::ButtonPressed,TEXT("Car.ReachButton"),EVBoRConfirmBy::None},
		{TEXT("14"),TEXT("동승석 위치에서 확인하세요."),EVBoRStepRule::Viewpoint,TEXT("Car.PassengerView"),EVBoRConfirmBy::Either},
		{TEXT("15"),TEXT("2열 위치에서 확인하세요."),EVBoRStepRule::Viewpoint,TEXT("Car.RearView"),EVBoRConfirmBy::Either},
		{TEXT("16"),TEXT("시트 폴딩 각도를 확인하세요."),EVBoRStepRule::Confirm,TEXT(""),EVBoRConfirmBy::Operator},
		{TEXT("18"),TEXT("하차를 위해 문을 열어보세요."),EVBoRStepRule::DoorOpened,TEXT("Car.DriverDoor"),EVBoRConfirmBy::None},
		{TEXT("19"),TEXT("차량에서 내려보세요."),EVBoRStepRule::Confirm,TEXT(""),EVBoRConfirmBy::Operator},
		{TEXT("20"),TEXT("문을 닫아보세요."),EVBoRStepRule::DoorLatched,TEXT("Car.DriverDoor"),EVBoRConfirmBy::None}
	};
	if (!TestEqual(TEXT("E9 nineteen steps"),Asset->Steps.Num(),static_cast<int32>(UE_ARRAY_COUNT(Expected)))) { return false; }
	for (int32 I = 0; I < Asset->Steps.Num(); ++I)
	{
		const auto& S = Asset->Steps[I]; const auto& X = Expected[I]; const FString Prefix = FString::Printf(TEXT("E9 %s "),X.Id);
		TestEqual(Prefix+TEXT("ID"),S.StepId,FName(X.Id)); TestEqual(Prefix+TEXT("instruction"),S.Instruction.ToString(),FString(X.Text));
		TestEqual(Prefix+TEXT("rule"),S.Rule,X.Rule); TestEqual(Prefix+TEXT("target"),S.TargetTag,FName(X.Tag)); TestEqual(Prefix+TEXT("confirm"),S.ConfirmBy,X.By);
		TestEqual(Prefix+TEXT("marker"),S.MarkerTag,I == 0 ? FName(TEXT("Car.FrontKey")) : NAME_None);
		TestEqual(Prefix+TEXT("route"),S.RouteTag,I == 2 ? FName(TEXT("Route.AroundCar")) : I == 6 ? FName(TEXT("Route.ToDriverDoor")) : NAME_None);
		TestEqual(Prefix+TEXT("base viewpoint"),S.bRequireBaseViewpoint,I == 16);
		TestEqual(Prefix+TEXT("door deg"),S.DoorOpenDeg,45.0f); TestEqual(Prefix+TEXT("seated seconds"),S.SeatedSeconds,1.5f);
		const FString TV = I == 0 ? TEXT("Pick up the key from the table.") : I == 1 ? TEXT("Turn the exterior lights on and off with the key.") : TEXT("");
		TestEqual(Prefix+TEXT("TV"),S.SpectatorText.ToString(),TV);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP4cIntegrationTest, "vBoR.P4c.E10E11E12MockAssetDefaults", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP4cIntegrationTest::RunTest(const FString&)
{
	FP3World E; E.Runner->Scenario = LoadObject<UVBoRScenarioAsset>(nullptr,TEXT("/Game/P3/DA_P3_Scenario.DA_P3_Scenario"));
	if (!TestNotNull(TEXT("E10 asset loads"),E.Runner->Scenario.Get())) { return false; }
	TestTrue(TEXT("E10 asset initializes against mock"),E.Runner->InitializeFlow());
	TestTrue(TEXT("E10 validation empty"),E.Runner->ValidationProblems.IsEmpty());
	TestEqual(TEXT("E10 starts 01"),E.Runner->GetStepIndex(),0);
	TestEqual(TEXT("E10 TV total"),VBoRSpectator::Banner(*E.Runner,FVBoRSpectatorLabels()).Total,19);
	for (const TCHAR* Tag : {TEXT("Car.FrontKey"),TEXT("Car.RearKey"),TEXT("Car.FrontKey.Unlock"),TEXT("Car.FrontKey.Lock"),TEXT("Car.RearKey.Unlock"),TEXT("Car.RearKey.Lock")})
	{
		int32 Count = 0;
		for (TActorIterator<AActor> It(E.World); It; ++It)
		{
			if (!It->ActorHasTag(Tag)) { continue; } ++Count;
			TestTrue(TEXT("E11 tags have expected actor type"),FString(Tag).EndsWith(TEXT("Key")) ? It->IsA<AVBoRKeyFob>() : It->IsA<AVBoRPokeButton>());
		}
		TestEqual(FString(TEXT("E11 unique "))+Tag,Count,1);
	}
	const auto* Defaults = NewObject<UVBoRScenarioAsset>();
	TestEqual(TEXT("E12 defaults still fourteen"),Defaults->Steps.Num(),14);
	TestEqual(TEXT("E12 default 01 still Approach"),Defaults->Steps[0].Rule,EVBoRStepRule::Approach);
	return true;
}

struct FP4dTestAccess
{
	static void Grab(AVBoRCarryObject* Box) { Box->BeginCarry(0, nullptr, Box->GetActorTransform()); }
	static void Release(AVBoRCarryObject* Box) { Box->ReleaseCarry(); }
	static void Pause(AVBoRCarryObject* Box, bool bPaused) { Box->Session.bPaused = bPaused; }
};

namespace
{
	int32 P4dBoxes(UWorld* World, AActor* Stand)
	{
		int32 Count = 0;
		for (TActorIterator<AVBoRCarryObject> It(World); It; ++It) { Count += It->GetOwner() == Stand; }
		return Count;
	}
	void P4dOpen(AVBoRPoweredPanel* Gate) { Gate->RequestOpen(); Gate->Tick(6); }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP4dMockTest, "vBoR.P4d.E1E10RearAssemblyAndRoutes", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP4dMockTest::RunTest(const FString&)
{
	FP3World E; const auto* T = E.Car->Trunk.Get(); const auto* S = E.Car->BoxStand.Get();
	if (!TestNotNull(TEXT("E1 trunk"),T) || !TestNotNull(TEXT("E1 stand"),S)) { return false; }
	TestTrue(TEXT("E1 rear opening faces +X"),T->GetActorLocation().Equals(FVector(-235,0,0)) && T->GetActorForwardVector().Equals(FVector::XAxisVector));
	for (const TCHAR* Tag : {TEXT("Car.Liftgate"),TEXT("Car.BoxStand")})
	{
		int32 Count = 0; for (TActorIterator<AActor> It(E.World); It; ++It) { Count += It->ActorHasTag(Tag); }
		TestEqual(TEXT("E1 one actor per tag"),Count,1);
	}
	TestTrue(TEXT("E1 loading region is trunk interior"),E.Car->LoadTarget->LoadingRegion->GetUnscaledBoxExtent().Equals(FVector(45,65,35)) && E.Car->LoadTarget->LoadingRegion->GetComponentLocation().Equals(FVector(-190,0,110)));
	TestFalse(TEXT("E1 guide hidden"),E.Car->LoadTarget->bShowGuide);
	TestEqual(TEXT("E1 one box"),P4dBoxes(E.World,E.Car->BoxStand),1);
	TestTrue(TEXT("E1 box target and drop policy"),S->SourceBox->LoadTarget == E.Car->LoadTarget && !S->SourceBox->bUsePlacementAssist && S->SourceBox->bDestroyAtFloor);
	TestTrue(TEXT("E1 box bottom on stand"),FMath::IsNearlyEqual(S->SourceBox->GetCollisionBoundsInPose(S->SourceBox->GetActorTransform(),S->GetActorTransform()).Min.Z,75.01,0.02));
	TestTrue(TEXT("E1 rear plate below floor"),FP3FlowTestAccess::Plates(E.Car)[3]->Bounds.GetBox().Max.Z <= T->FloorHeightCm+0.01);
	E.Steps({P3bStep(TEXT("03"),TEXT("Route.AroundCar")),P3bStep(TEXT("07"),TEXT("Route.ToDriverDoor"))});
	TestTrue(TEXT("E10 both route validations pass"),E.Runner->IsRunning() && E.Runner->ValidationProblems.IsEmpty());
	const FVector End = E.Car->AroundCarRoute->Spline->GetLocationAtSplinePoint(4,ESplineCoordinateSpace::World);
	const FVector Local = T->GetActorTransform().InverseTransformPosition(End);
	TestTrue(TEXT("E10 rear arrival 60-120 cm behind opening and within width"),Local.X <= -60 && Local.X >= -120 && FMath::Abs(Local.Y) <= T->InsideWidthCm/2);
	TestTrue(TEXT("E10 07 starts behind body and ends unchanged"),E.Car->DriverDoorRoute->Spline->GetLocationAtSplinePoint(0,ESplineCoordinateSpace::Local).X < -235 && E.Car->DriverDoorRoute->Spline->GetLocationAtSplinePoint(3,ESplineCoordinateSpace::Local).Equals(FVector(-65,-165,0)));
	for (AVBoRFloorRoute* Route : {E.Car->AroundCarRoute.Get(),E.Car->DriverDoorRoute.Get()})
	{
		double Minimum = TNumericLimits<double>::Max();
		for (float D = 0; D <= Route->Spline->GetSplineLength(); D += 1)
		{
			const FVector P = Route->Spline->GetLocationAtDistanceAlongSpline(D,ESplineCoordinateSpace::World);
			for (const FBox Obstacle : {S->Stand->Bounds.GetBox(), E.Car->RearKeyTable->TopMesh->Bounds.GetBox()})
			{
				const double DX = FMath::Max3(Obstacle.Min.X-P.X,0.0,P.X-Obstacle.Max.X);
				const double DY = FMath::Max3(Obstacle.Min.Y-P.Y,0.0,P.Y-Obstacle.Max.Y);
				Minimum = FMath::Min(Minimum,FMath::Sqrt(DX*DX+DY*DY));
			}
		}
		TestTrue(FString::Printf(TEXT("E10 %s obstacle clearance %.2f cm >= 10"),*Route->Tags[0].ToString(),Minimum),Minimum >= 10);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP4dOpenTest, "vBoR.P4d.E2E3PoweredOpen", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP4dOpenTest::RunTest(const FString&)
{
	FP3World E; auto* G = E.Car->Liftgate.Get();
	const auto Open = P3Step(TEXT("04"),EVBoRStepRule::PoweredOpen,TEXT("Car.Liftgate"),EVBoRConfirmBy::None);
	E.Steps({Open,P3Step(TEXT("05"))}); E.Runner->ProcessFlow(0);
	TestEqual(TEXT("E2 closed waits"),E.Runner->GetStepIndex(),0);
	G->RequestOpen(); E.Runner->ProcessFlow(0); TestEqual(TEXT("E2 wait state"),E.Runner->GetStepIndex(),0);
	G->Tick(1.5); E.Runner->ProcessFlow(0); TestEqual(TEXT("E2 opening waits"),E.Runner->GetStepIndex(),0);
	G->Tick(4); TestEqual(TEXT("E2 no flow processing yet"),E.Runner->GetStepIndex(),0);
	E.Runner->ProcessFlow(0); TestEqual(TEXT("E2 open completes next processing frame"),E.Runner->GetStepIndex(),1);
	TestTrue(TEXT("E2 participant rule attribution"),E.Runner->LastCompletion.Contains(TEXT("참가자 규칙")));
	E.Steps({P3Step(TEXT("03")),Open,P3Step(TEXT("05"))}); E.Command(EVBoRFlowCommand::Next);
	TestEqual(TEXT("E3 already open does not complete entered frame"),E.Runner->GetStepIndex(),1);
	E.Runner->ProcessFlow(0); TestEqual(TEXT("E3 completes following frame"),E.Runner->GetStepIndex(),2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP4dCloseTest, "vBoR.P4d.E4ClosedAndObstruction", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP4dCloseTest::RunTest(const FString&)
{
	FP3World E; auto* G = E.Car->Liftgate.Get(); P4dOpen(G);
	const auto Closed = P3Step(TEXT("06"),EVBoRStepRule::PoweredClosed,TEXT("Car.Liftgate"),EVBoRConfirmBy::None);
	E.Steps({Closed,P3Step(TEXT("07"))}); G->RequestClose(); G->Tick(1.5); E.Runner->ProcessFlow(0);
	TestEqual(TEXT("E4 closing waits"),E.Runner->GetStepIndex(),0);
	G->Tick(5); E.Runner->ProcessFlow(0); TestEqual(TEXT("E4 closed completes"),E.Runner->GetStepIndex(),1);
	P4dOpen(G); E.Steps({Closed,P3Step(TEXT("07"))});
	auto* Box = E.Car->BoxStand->SourceBox.Get(); Box->SetActorLocation(FVector(-245,0,100));
	G->RequestClose(); G->Tick(6); E.Runner->ProcessFlow(0);
	TestTrue(TEXT("E4 actual obstruction reopens"),G->GetObstructionCount() > 0 && G->GetState() != EVBoRPoweredState::Closed);
	G->Tick(5); E.Runner->ProcessFlow(0);
	TestTrue(TEXT("E4 reopened still waits"),G->GetState() == EVBoRPoweredState::Open && E.Runner->GetStepIndex() == 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP4dTargetTest, "vBoR.P4d.E5TargetValidation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP4dTargetTest::RunTest(const FString&)
{
	FP3World E;
	for (auto Rule : {EVBoRStepRule::PoweredOpen,EVBoRStepRule::PoweredClosed})
	{
		E.Steps({P3Step(TEXT("04"),Rule,TEXT("Car.DriverDoor"),EVBoRConfirmBy::None)});
		TestTrue(TEXT("E5 wrong type blocks"),!E.Runner->IsRunning() && E.Runner->ValidationProblems.Contains(TEXT("종류 불일치")));
		E.Steps({P3Step(TEXT("04"),Rule,TEXT("Car.MissingGate"),EVBoRConfirmBy::None)});
		TestTrue(TEXT("E5 missing tag blocks"),!E.Runner->IsRunning() && E.Runner->ValidationProblems.Contains(TEXT("0개 발견")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP4dStandTest, "vBoR.P4d.E6E7SupplyFloorResetCleanup", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP4dStandTest::RunTest(const FString&)
{
	FP3World E; auto* S = E.Car->BoxStand.Get(); auto* First = S->SourceBox.Get(); const FTransform Start = First->GetActorTransform();
	FP4dTestAccess::Grab(First); S->Tick(0); TestEqual(TEXT("E6 grabbing alone keeps one"),P4dBoxes(E.World,S),1);
	First->AddActorWorldOffset(FVector(0,0,5)); S->Tick(0); TestEqual(TEXT("E6 overlap keeps one"),P4dBoxes(E.World,S),1);
	First->SetActorLocation(Start.GetLocation()+FVector(0,0,31)); S->Tick(0); TestEqual(TEXT("E6 one cm boundary still occupied"),P4dBoxes(E.World,S),1);
	First->SetActorLocation(Start.GetLocation()+FVector(0,0,40)); FP4dTestAccess::Pause(First,true); S->Tick(0);
	TestEqual(TEXT("E6 paused clear source is not replenished"),P4dBoxes(E.World,S),1);
	FP4dTestAccess::Pause(First,false); S->Tick(0); TestEqual(TEXT("E6 clear source makes one more"),P4dBoxes(E.World,S),2);
	TestTrue(TEXT("E6 replacement at original pose"),S->SourceBox->GetActorTransform().Equals(Start));
	S->Tick(0); TestEqual(TEXT("E6 only one replacement"),P4dBoxes(E.World,S),2);
	First->SetActorLocation(FVector(-500,300,25)); FP4dTestAccess::Release(First); First->Tick(1);
	TestFalse(TEXT("E7 released box reaches floor and disappears"),IsValid(First));
	TestEqual(TEXT("E7 floor leaves one"),P4dBoxes(E.World,S),1);
	auto* Before = S->SourceBox.Get(); S->ResetStand(); TestFalse(TEXT("E7 reset destroys old source"),IsValid(Before));
	TestEqual(TEXT("E7 reset creates one"),P4dBoxes(E.World,S),1);
	auto* Last = S->SourceBox.Get(); S->Destroy(); TestFalse(TEXT("E7 EndPlay cleans source"),IsValid(Last));
	TestEqual(TEXT("E7 EndPlay cleans all"),P4dBoxes(E.World,S),0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP4dRestartTest, "vBoR.P4d.E8E9Restart", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP4dRestartTest::RunTest(const FString&)
{
	for (bool bPassenger : {false,true}) for (bool bClosing : {false,true})
	{
		FP3World E; auto* S = E.Car->BoxStand.Get(); auto* G = E.Car->Liftgate.Get();
		E.Steps({P3Step(TEXT("01")),P3Step(TEXT("04"),EVBoRStepRule::PoweredOpen,TEXT("Car.Liftgate"),EVBoRConfirmBy::None)});
		TArray<AVBoRCarryObject*> Old;
		for (int32 I = 0; I < 2; ++I)
		{
			auto* Box = S->SourceBox.Get(); Old.Add(Box); FP4dTestAccess::Grab(Box);
			Box->SetActorLocation(FVector(-180,I == 0 ? -22 : 22,90)); FP4dTestAccess::Release(Box); Box->Tick(0); S->Tick(0);
		}
		Old.Add(S->SourceBox); FP4dTestAccess::Grab(S->SourceBox);
		P4dOpen(G); if (bClosing) { G->RequestClose(); G->Tick(1.5); }
		E.Car->Door->Unlock(); E.DoorMotion().AngleDeg = 40;
		const FTransform KeyPose = E.Car->FrontKeyTable->Key->GetActorTransform(); FP4cTestAccess::Hold(E.Car->FrontKeyTable->Key);
		if (bPassenger) { E.Command(EVBoRFlowCommand::Passenger); E.FinishSwitch(); }
		E.Command(EVBoRFlowCommand::Restart);
		for (auto* Box : Old) { TestFalse(TEXT("E8/E9 all old boxes gone"),IsValid(Box)); }
		TestEqual(TEXT("E8/E9 one new box"),P4dBoxes(E.World,S),1);
		TestEqual(TEXT("E8/E9 closed gate"),G->GetState(),EVBoRPoweredState::Closed);
		TestTrue(TEXT("E8/E9 locked closed door"),E.DoorMotion().bLocked && E.DoorMotion().AngleDeg == 0);
		TestTrue(TEXT("E8/E9 key unchanged"),E.Car->FrontKeyTable->Key->GetActorTransform().Equals(KeyPose) && E.Car->FrontKeyTable->Key->GetHoldingHand() == 0);
		if (bPassenger) { TestFalse(TEXT("E9 waits for base"),E.Runner->IsRunning()); E.FinishSwitch(); E.Runner->ProcessFlow(0); }
		TestTrue(TEXT("E8/E9 running 01 on base"),E.Runner->IsRunning() && E.Runner->GetStepIndex() == 0 && E.Car->Alignment->IsBaseViewpoint());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP4dAreaTest, "vBoR.P4d.E11UnknownAreaRecovery", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP4dAreaTest::RunTest(const FString&)
{
	for (bool bInside : {true,false})
	{
		FP3World E; FP3bRecords Records; E.HeadXR->PlayArea = FVector2D::ZeroVector;
		E.Steps({P3bStep(TEXT("03"),TEXT("Route.AroundCar"))});
		E.Runner->ProcessFlow(0); E.Runner->ProcessFlow(0);
		TestTrue(TEXT("E11 unknown hidden"),E.Car->AroundCarRoute->IsHidden());
		TestEqual(TEXT("E11 unknown recorded once"),Records.Count(TEXT("play area unknown")),1);
		E.HeadXR->PlayArea = bInside ? FVector2D(10000,10000) : FVector2D(100,100); E.Runner->ProcessFlow(0);
		TestEqual(TEXT("E11 visibility after bounds arrive"),E.Car->AroundCarRoute->IsHidden(),!bInside);
		TestEqual(TEXT("E11 outside recorded once if necessary"),Records.Count(TEXT("outside the play area")),bInside ? 0 : 1);
		TestEqual(TEXT("E11 notice cleared or replaced"),E.Runner->GetOperatorText().Contains(TEXT("play area unknown")),false);
		E.HeadXR->PlayArea = FVector2D(10000,10000); E.Runner->ProcessFlow(0);
		TestEqual(TEXT("E11 known outside does not retry"),E.Car->AroundCarRoute->IsHidden(),!bInside);
		TestEqual(TEXT("E11 no repeated outside record"),Records.Count(TEXT("outside the play area")),bInside ? 0 : 1);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP4dFlowTest, "vBoR.P4d.E12E13AssetRearFlow", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP4dFlowTest::RunTest(const FString&)
{
	FP3World E; E.Runner->Scenario = LoadObject<UVBoRScenarioAsset>(nullptr,TEXT("/Game/P3/DA_P3_Scenario.DA_P3_Scenario"));
	if (!TestNotNull(TEXT("E12 asset loads"),E.Runner->Scenario.Get())) { return false; }
	TestTrue(TEXT("E13 asset validates"),E.Runner->InitializeFlow());
	E.Command(EVBoRFlowCommand::Next); E.Command(EVBoRFlowCommand::Next);
	TestTrue(TEXT("E13 03 route visible"),E.Runner->GetStepIndex() == 2 && !E.Car->AroundCarRoute->IsHidden());
	E.Command(EVBoRFlowCommand::Confirm); TestEqual(TEXT("E13 04"),E.Runner->GetStepIndex(),3);
	P4dOpen(E.Car->Liftgate); E.Runner->ProcessFlow(0); TestEqual(TEXT("E13 05"),E.Runner->GetStepIndex(),4);
	E.Runner->ProcessFlow(1); TestEqual(TEXT("E13 05 waits for operator"),E.Runner->GetStepIndex(),4);
	E.Command(EVBoRFlowCommand::Confirm); TestEqual(TEXT("E13 06"),E.Runner->GetStepIndex(),5);
	E.Car->Liftgate->RequestClose(); E.Car->Liftgate->Tick(6); E.Runner->ProcessFlow(0);
	TestTrue(TEXT("E13 07 route visible"),E.Runner->GetStepIndex() == 6 && !E.Car->DriverDoorRoute->IsHidden());
	return true;
}
struct FP4eTestAccess
{
	static void BeforeStart(AVBoRScenarioRunner* R) { R->bRunning = false; }
	static void Reload(AVBoRScenarioRunner* R) { R->bReloadPending = true; }
	static void Lock(AVBoRScenarioRunner* R) { R->ConfirmLockRemaining = 1; }
	static void Dispatch(AVBoRScenarioRunner* R, EVBoRFlowCommand Action) { R->Dispatch({Action, EVBoRCommandSource::Operator, R->GetToken()}); }
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP4eMotionTest, "vBoR.P4e.E1E2Motion", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP4eMotionTest::RunTest(const FString&)
{
	FP3World E; auto* S = E.Car->RearSeat.Get();
	TestTrue(TEXT("Standing accepts fold"), S->Fold()); S->Tick(1);
	TestEqual(TEXT("45 deg per second"), S->GetAngleDeg(), 45.0f);
	S->Tick(100); TestEqual(TEXT("Large tick clamps to 80 deg"), S->GetAngleDeg(), 80.0f);
	TestTrue(TEXT("Folded endpoint"), S->GetState() == EVBoRFoldingSeatState::Folded);
	TestTrue(TEXT("Folded accepts unfold"), S->Unfold()); S->Tick(1);
	TestEqual(TEXT("Unfold at same speed"), S->GetAngleDeg(), 35.0f);
	S->Tick(100); TestEqual(TEXT("Clamps to zero"), S->GetAngleDeg(), 0.0f);
	TestTrue(TEXT("Standing endpoint"), S->GetState() == EVBoRFoldingSeatState::Standing);
	S->FoldAngleDeg = 50; S->SpeedDegPerSecond = 20; S->Fold(); S->Tick(1);
	TestEqual(TEXT("Edited speed"), S->GetAngleDeg(), 20.0f); S->Tick(10);
	TestEqual(TEXT("Edited fold angle"), S->GetAngleDeg(), 50.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP4eIgnoreTest, "vBoR.P4e.E3IgnoreRepeatedCommands", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP4eIgnoreTest::RunTest(const FString&)
{
	FP3World E; auto* S = E.Car->RearSeat.Get();
	TestFalse(TEXT("Standing ignores unfold"), S->Unfold()); S->Fold(); S->Tick(0.5);
	TestFalse(TEXT("Folding ignores fold"), S->Fold()); TestFalse(TEXT("Folding ignores reversal"), S->Unfold());
	S->Tick(0.5); TestEqual(TEXT("Repeated commands preserve motion"), S->GetAngleDeg(), 45.0f);
	S->Tick(10); TestFalse(TEXT("Folded ignores fold"), S->Fold()); S->Unfold(); S->Tick(0.5);
	TestFalse(TEXT("Unfolding ignores fold"), S->Fold()); TestFalse(TEXT("Unfolding ignores unfold"), S->Unfold());
	S->Tick(0.5); TestEqual(TEXT("Unfold continues"), S->GetAngleDeg(), 35.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP4ePivotTest, "vBoR.P4e.E4PivotAndMeshRest", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP4ePivotTest::RunTest(const FString&)
{
	FP3World E;
	const FTransform ActorPose(FRotator(0,37,0), FVector(200,-80,12));
	auto* S = E.World->SpawnActorDeferred<AVBoRFoldingSeat>(AVBoRFoldingSeat::StaticClass(), ActorPose);
	S->HingeLocationCm = FVector(-128,0,48);
	S->MeshRestFromRoot = FTransform(FRotator(4,12,2), FVector(-128,0,78), FVector(0.08,1.7,0.6));
	S->FinishSpawning(ActorPose);
	TestTrue(TEXT("CAD root rest pose preserved"), S->SeatMesh->GetComponentTransform().Equals(S->MeshRestFromRoot * ActorPose, 0.001));
	const FVector AxisBefore = S->HingePivot->GetComponentTransform().TransformPosition(FVector(0,30,0));
	const FVector TopBefore = S->HingePivot->GetComponentTransform().TransformPosition(FVector(0,0,60));
	S->Fold(); S->Tick(1);
	const FVector AxisAfter = S->HingePivot->GetComponentTransform().TransformPosition(FVector(0,30,0));
	const FVector TopAfter = S->HingePivot->GetComponentTransform().TransformPosition(FVector(0,0,60));
	TestTrue(TEXT("Hinge axis stays fixed"), AxisAfter.Equals(AxisBefore, 0.001));
	TestTrue(TEXT("Top moves forward in actor +X"), FVector::DotProduct(TopAfter - TopBefore, S->GetActorForwardVector()) > 40);
	TestTrue(TEXT("Top lowers"), TopAfter.Z < TopBefore.Z);
	S->ResetSeat(); TestTrue(TEXT("Reset restores mesh world rest"), S->SeatMesh->GetComponentTransform().Equals(S->MeshRestFromRoot * ActorPose, 0.001));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP4eMockTest, "vBoR.P4e.E5RearSeatAssembly", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP4eMockTest::RunTest(const FString&)
{
	FP3World E; auto* S = E.Car->RearSeat.Get(); int32 Tagged = 0;
	for (TActorIterator<AActor> It(E.World); It; ++It) { if (It->ActorHasTag(TEXT("Car.RearSeat"))) { ++Tagged; } }
	TestEqual(TEXT("One tagged folding seat"), Tagged, 1);
	TestTrue(TEXT("Rear viewpoint unchanged"), E.Car->RearView->GetRelativeLocation().Equals(FVector(-100,-37,118)));
	TestTrue(TEXT("Cushion uses rear eye height"), E.Car->RearCushion->GetRelativeLocation().Equals(FVector(-100,0,48)));
	TestTrue(TEXT("Approximate plates have no collision"), S->SeatMesh->GetCollisionEnabled() == ECollisionEnabled::NoCollision && E.Car->RearCushion->GetCollisionEnabled() == ECollisionEnabled::NoCollision);
	const FBox Body(FVector(-235,-92.5,0), FVector(235,92.5,145));
	const FTransform CarInverse = E.Car->GetActorTransform().Inverse();
	const FBox Loading = E.Car->LoadTarget->LoadingRegion->Bounds.GetBox();
	for (bool bFolded : {false,true})
	{
		if (bFolded) { S->Fold(); S->Tick(10); }
		const FBox Local = S->SeatMesh->GetStaticMesh()->GetBoundingBox().TransformBy(S->SeatMesh->GetComponentTransform() * CarInverse);
		TestTrue(TEXT("Seat inside body at both endpoints"), Body.IsInsideOrOn(Local.Min) && Body.IsInsideOrOn(Local.Max));
		TestFalse(TEXT("Seat does not overlap loading region"), S->SeatMesh->Bounds.GetBox().Intersect(Loading));
	}
	E.Car->Destroy(); TestFalse(TEXT("Car cleanup destroys seat"), IsValid(S));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP4eCommandTest, "vBoR.P4e.E6E8CommandsAndStep16", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP4eCommandTest::RunTest(const FString&)
{
	FP3World E; E.Steps({P3Step(TEXT("16"), EVBoRStepRule::Confirm, TEXT(""), EVBoRConfirmBy::Operator)});
	auto* S = E.Car->RearSeat.Get(); auto* Untagged = E.World->SpawnActor<AVBoRFoldingSeat>();
	FP4eTestAccess::Lock(E.Runner);
	FP3FlowTestAccess::NewInputFrame(E.Runner); E.Runner->HandleOperatorKey(EKeys::F); E.Runner->ProcessFlow(0);
	TestTrue(TEXT("F folds tagged and untagged seats despite confirm lock"), S->GetState() == EVBoRFoldingSeatState::Folding && Untagged->GetState() == EVBoRFoldingSeatState::Folding);
	S->Tick(10); Untagged->Tick(10); E.Runner->ProcessFlow(10);
	TestTrue(TEXT("16 still waits for operator confirmation"), E.Runner->GetStepIndex() == 0 && E.Runner->IsRunning());
	FP3FlowTestAccess::NewInputFrame(E.Runner); E.Runner->PressOperatorButton(EVBoROperatorButton::UnfoldSeat); E.Runner->ProcessFlow(0);
	TestTrue(TEXT("Button unfolds both seats"), S->GetState() == EVBoRFoldingSeatState::Unfolding && Untagged->GetState() == EVBoRFoldingSeatState::Unfolding);
	S->Tick(10); Untagged->Tick(10); E.Command(EVBoRFlowCommand::EnterFreeMode);
	FP3FlowTestAccess::NewInputFrame(E.Runner); E.Runner->PressOperatorButton(EVBoROperatorButton::FoldSeat); E.Runner->ProcessFlow(0); S->Tick(10); Untagged->Tick(10);
	FP3FlowTestAccess::NewInputFrame(E.Runner); E.Runner->HandleOperatorKey(EKeys::U); E.Runner->ProcessFlow(0);
	TestTrue(TEXT("U unfolds in free mode"), E.Runner->IsFreeMode() && S->GetState() == EVBoRFoldingSeatState::Unfolding);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP4eDecisionTest, "vBoR.P4e.E7DecisionAndDiscards", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP4eDecisionTest::RunTest(const FString&)
{
	using B = EVBoROperatorButton;
	for (int32 State = 0; State < 9; ++State)
	{
		FP3World E; E.Steps({P3Step(TEXT("16"))}); auto* S = E.Car->RearSeat.Get();
		if (State == 0) { FP4eTestAccess::BeforeStart(E.Runner); }
		if (State == 1) { E.Command(EVBoRFlowCommand::Passenger); }
		if (State == 2) { E.Command(EVBoRFlowCommand::EnterFreeMode); FP4eTestAccess::Reload(E.Runner); }
		if (State == 3) { S->Destroy(); }
		if (State == 4) { S->Fold(); S->Tick(10); }
		if (State == 5) { S->Fold(); S->Tick(0.5); }
		if (State == 6) { FP4eTestAccess::Lock(E.Runner); }
		if (State == 7) { E.Command(EVBoRFlowCommand::EnterFreeMode); }
		if (State == 8) { E.Car->Alignment->RequestReapply(); }
		for (B Button : {B::FoldSeat, B::UnfoldSeat})
		{
			const auto D = E.Runner->EvaluateOperatorButton(Button);
			const bool bAccept = (State == 4 && Button == B::UnfoldSeat) || ((State == 6 || State == 7) && Button == B::FoldSeat);
			TestEqual(TEXT("Accepted in correct state"), D.bAccepted, bAccept);
			TestEqual(TEXT("Snapshot shares decision"), E.Runner->GetOperatorSnapshot().Buttons[static_cast<int32>(Button)].Decision.bAccepted, bAccept);
			TestTrue(TEXT("Button style shares decision"), E.Runner->GetOperatorStyle(Button) == (bAccept ? EVBoROperatorStyle::Secondary : EVBoROperatorStyle::Disabled));
			if (!bAccept)
			{
				const auto Expected = State == 1 || State == 2 || State == 8 ? EVBoRDiscard::Transition : State == 0 || State == 3 ? EVBoRDiscard::Rule : EVBoRDiscard::None;
				TestTrue(TEXT("Correct discard kind"), D.Discard == Expected); TestFalse(TEXT("English reason provided"), D.Reason.IsEmpty());
				const int32 Rules = E.Runner->RuleDiscards, Transitions = E.Runner->TransitionDiscards;
				const FString Record = E.Runner->LastRecord;
				FP4eTestAccess::Dispatch(E.Runner, Button == B::FoldSeat ? EVBoRFlowCommand::FoldSeat : EVBoRFlowCommand::UnfoldSeat);
				TestEqual(TEXT("Rule count"), E.Runner->RuleDiscards, Rules + (Expected == EVBoRDiscard::Rule ? 1 : 0));
				TestEqual(TEXT("Transition count"), E.Runner->TransitionDiscards, Transitions + (Expected == EVBoRDiscard::Transition ? 1 : 0));
				if (Expected == EVBoRDiscard::None) { TestTrue(TEXT("No-op has no record"), D.Record.IsEmpty() && E.Runner->LastRecord == Record); }
				if (State == 3) { TestEqual(TEXT("Missing seat reason"), D.Reason, FString(TEXT("No folding seat"))); }
			}
		}
	}
	FP3World E; E.Steps({P3Step(TEXT("16"))});
	TestTrue(TEXT("Participant cannot fold"), E.Runner->EvaluateCommand({EVBoRFlowCommand::FoldSeat, EVBoRCommandSource::Participant, E.Runner->GetToken()}).Discard == EVBoRDiscard::Permission);
	auto Old = E.Runner->GetToken(); --Old.Generation;
	TestTrue(TEXT("Stale fold rejected"), E.Runner->EvaluateCommand({EVBoRFlowCommand::FoldSeat, EVBoRCommandSource::Operator, Old}).Discard == EVBoRDiscard::Stale);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP4eResetTest, "vBoR.P4e.E9RestartResetsSeats", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP4eResetTest::RunTest(const FString&)
{
	for (bool bFinished : {false,true})
	{
		FP3World E; FP3bRecords Records; E.Steps({P3Step(TEXT("16"))}); auto* S = E.Car->RearSeat.Get();
		auto* Untagged = E.World->SpawnActor<AVBoRFoldingSeat>(); S->Fold(); Untagged->Fold(); S->Tick(bFinished ? 10 : 0.5); Untagged->Tick(0.5);
		E.Command(EVBoRFlowCommand::Restart);
		TestTrue(TEXT("R immediately stands all seats"), S->GetState() == EVBoRFoldingSeatState::Standing && Untagged->GetState() == EVBoRFoldingSeatState::Standing);
		TestEqual(TEXT("Reset angle zero"), S->GetAngleDeg(), 0.0f); S->Tick(10); TestEqual(TEXT("No pending motion"), S->GetAngleDeg(), 0.0f);
		TestEqual(TEXT("Reset record includes seat"), Records.Count(TEXT("시트 세움")), 1);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP4eMarkerTest, "vBoR.P4e.E10RecoveredRouteMarker", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP4eMarkerTest::RunTest(const FString&)
{
	FP3World E; auto* R = E.Car->AroundCarRoute.Get();
	E.HeadXR->PlayArea = FVector2D::ZeroVector;
	E.Steps({P3bStep(TEXT("03"), TEXT("Route.AroundCar"))});
	for (const auto& Part : FP3bTestAccess::Ring(E.Runner)) { TestFalse(TEXT("Fallback marker ring initially visible"), Part->bHiddenInGame); }
	E.HeadXR->PlayArea = FVector2D(10000,10000); E.Runner->ProcessFlow(0);
	const FVector End = R->Spline->GetLocationAtSplinePoint(R->Spline->GetNumberOfSplinePoints()-1, ESplineCoordinateSpace::World);
	TestFalse(TEXT("Recovered arrow visible"), FP3bTestAccess::Arrow(E.Runner)->bHiddenInGame);
	TestTrue(TEXT("Arrow above route end +160 cm"), FP3bTestAccess::Arrow(E.Runner)->GetComponentLocation().Equals(End + FVector(0,0,160), 0.001));
	for (const auto& Part : FP3bTestAccess::Ring(E.Runner)) { TestTrue(TEXT("Fallback marker ring hidden after recovery"), Part->bHiddenInGame); }
	return true;
}
#endif
