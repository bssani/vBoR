#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "P3VehicleMock.h"
#include "../Interaction/VBoRPanelAnchor.h"
#include "../Interaction/VBoRScenarioRunner.h"
#include "../Interaction/VBoRVehicleAlignmentComponent.h"
#include "../Interaction/VBoREyePointComponent.h"
#include "../Interaction/VBoRViewpointComponent.h"
#include "../Interaction/VBoRFloorRoute.h"
#include "../Interaction/VBoRPokeButton.h"
#include "../Spectator/VBoRSpectatorModel.h"
#include "Components/ArrowComponent.h"
#include "Components/SplineComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/WorldSettings.h"
#include "Interaction/IsdkPokeInteractable.h"
#include "Misc/OutputDevice.h"
#include "Misc/OutputDeviceRedirector.h"
#include "XRTrackingSystemBase.h"
#include "UObject/UnrealType.h"

struct FP5zTestAccess
{
	static AVBoRPokeButton* Button(AVBoRScenarioRunner* R, int32 I) { return I == 0 ? R->ConfirmButton.Get() : R->ActionButton.Get(); }
	static UWidgetComponent* Label(AVBoRScenarioRunner* R, int32 I) { return R->ButtonLabels[I]; }
	static UWidgetComponent* Operator(AVBoRScenarioRunner* R) { return R->OperatorPanel; }
	static double Lock(AVBoRScenarioRunner* R) { return R->ConfirmLockRemaining; }
	static bool Arrived(AVBoRScenarioRunner* R) { return R->bRouteArrived; }
	static FString ActionLabel(AVBoRScenarioRunner* R) { return R->ActionLabel(); }
	static void NewInputFrame(AVBoRScenarioRunner* R) { R->LiveFrame = MAX_uint64; }
	static TArray<FString> Options(UVBoRScenarioAsset* Asset, FName FVBoRScenarioStep::* Field, UWorld* World, const UClass* Type = nullptr)
	{ return Asset->TagOptions(Field, World, Type); }
	static void PawnMoved(AVBoRScenarioRunner* R) { R->HandlePawnMoved(); }
	static void Poke(AVBoRScenarioRunner* R, int32 I)
	{
		NewInputFrame(R);
		FIsdkInteractionPointerEvent Event; Event.Type = EIsdkPointerEventType::Select; Event.Interactable = Button(R,I)->Poke.Get();
		Button(R,I)->Poke->GetInteractionPointerEventDelegate().Broadcast(Event);
	}
};

namespace
{
	class FP5zHead : public FXRTrackingSystemBase
	{
	public:
		FP5zHead() : FXRTrackingSystemBase(nullptr) {}
		FTransform LocalHead;
		bool bTracked = true;
		virtual FName GetSystemName() const override { return TEXT("P5zSyntheticHead"); }
		virtual int32 GetXRSystemFlags() const override { return 0; }
		virtual bool EnumerateTrackedDevices(TArray<int32>& Out, EXRTrackedDeviceType) override { Out.Add(0); return true; }
		virtual bool IsTracking(int32 Device) override { return bTracked && Device == 0; }
		virtual bool GetCurrentPose(int32 Device, FQuat& Rotation, FVector& Position) override
		{ Rotation = LocalHead.GetRotation(); Position = LocalHead.GetLocation(); return IsTracking(Device); }
		virtual float GetWorldToMetersScale() const override { return 100; }
		virtual void ResetOrientationAndPosition(float) override {}
		virtual bool IsHeadTrackingAllowed() const override { return true; }
		virtual FVector2D GetPlayAreaBounds(EHMDTrackingOrigin::Type) const override { return FVector2D(10000,10000); }
	};
	struct FP5zWorld
	{
		UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
		APawn* Pawn;
		AP3VehicleMock* Car;
		AVBoRScenarioRunner* Runner;
		TSharedPtr<IXRTrackingSystem, ESPMode::ThreadSafe> OriginalXR;
		TSharedPtr<FP5zHead, ESPMode::ThreadSafe> Head;
		FP5zWorld()
		{
			GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
			auto* PC = World->SpawnActor<APlayerController>(); Pawn = World->SpawnActor<APawn>();
			Pawn->SetRootComponent(NewObject<USceneComponent>(Pawn)); Pawn->GetRootComponent()->RegisterComponent(); PC->Possess(Pawn);
			Car = World->SpawnActor<AP3VehicleMock>(); Runner = World->SpawnActor<AVBoRScenarioRunner>();
			Runner->Scenario = NewObject<UVBoRScenarioAsset>(Runner);
			World->InitializeActorsForPlay(FURL()); World->GetWorldSettings()->NotifyBeginPlay();
			FVBoRBuckSavedAlignment Saved; Saved.SavedAt = TEXT("test"); Saved.TrackingOrigin = 3; Saved.PlayAreaCm = FVector2D(300,400);
			Saved.Capture.PositionCm = FVector(10,20,115); Saved.Capture.YawDeg = 30;
			Car->Alignment->InitializeAlignment(Pawn,&Saved,3,Saved.PlayAreaCm);
			OriginalXR = GEngine->XRSystem; Head = MakeShared<FP5zHead, ESPMode::ThreadSafe>(); GEngine->XRSystem = Head;
			WorldHead(Car->DriverEye->GetComponentTransform());
		}
		~FP5zWorld() { World->EndPlay(EEndPlayReason::Quit); GEngine->DestroyWorldContext(World); World->DestroyWorld(false); GEngine->XRSystem = OriginalXR; }
		void WorldHead(const FTransform& Pose) { Head->LocalHead = Pose.GetRelativeTransform(Pawn->GetActorTransform()); }
		AVBoRPanelAnchor* Anchor(FName Tag)
		{ for (TActorIterator<AVBoRPanelAnchor> It(World); It; ++It) { if (It->ActorHasTag(Tag)) { return *It; } } return nullptr; }
		FVBoRScenarioStep Step(const TCHAR* Id, FName Tag = TEXT("Guide.Outside"))
		{ FVBoRScenarioStep S; S.StepId = Id; S.Instruction = FText::FromString(TEXT("Observe the vehicle.")); S.PanelTag = Tag; return S; }
		FVBoRScenarioStep SeatStep(const TCHAR* Id, bool bPassenger)
		{
			auto S = Step(Id, bPassenger ? TEXT("Guide.Passenger") : TEXT("Guide.Rear"));
			S.Rule = EVBoRStepRule::Viewpoint;
			S.TargetTag = bPassenger ? TEXT("Car.PassengerView") : TEXT("Car.RearView");
			return S;
		}
		bool Start(std::initializer_list<FVBoRScenarioStep> Steps)
		{ Runner->Scenario->Steps.Reset(); for (const auto& S : Steps) { Runner->Scenario->Steps.Add(S); } return Runner->InitializeFlow(); }
		void Command(EVBoRFlowCommand Action)
		{ Runner->QueueCommand({Action,EVBoRCommandSource::Operator,Runner->GetToken()}); Runner->ProcessFlow(0); }
		void Ready() { Runner->ProcessFlow(0); Runner->ProcessFlow(1); }
		void Key(FKey Key) { Runner->HandleOperatorKey(Key); Runner->ProcessFlow(0); }
		void Action() { FP5zTestAccess::Poke(Runner,1); Runner->ProcessFlow(0); }
		bool AtAnchor(FName Tag) { return Runner->Panel->GetComponentLocation().Equals(Anchor(Tag)->GetActorLocation(), 0.01); }
		void FinishSwitch() { Car->Alignment->AdvanceSwitch(Car->Alignment->FadeSeconds); Car->Alignment->AdvanceSwitch(Car->Alignment->FadeSeconds); Runner->ProcessFlow(0); }
	};
	struct FP5zRecords : FOutputDevice
	{
		int32 Repositions = 0;
		FP5zRecords() { GLog->FlushThreadedLogs(); GLog->AddOutputDevice(this); }
		~FP5zRecords() { GLog->FlushThreadedLogs(); GLog->RemoveOutputDevice(this); }
		virtual void Serialize(const TCHAR* Message, ELogVerbosity::Type, const FName& Category) override
		{ if (Category == TEXT("LogP3Flow") && FString(Message).Contains(TEXT("Panel repositioned — confirmation lock"))) { ++Repositions; } }
		int32 Count() { GLog->FlushThreadedLogs(); return Repositions; }
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5zStartTest, "vBoR.P5z.E1FixedStartWithoutTracking", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5zStartTest::RunTest(const FString&)
{
	FP5zWorld E; E.Head->bTracked = false;
	TestTrue(TEXT("Start without head tracking"),E.Start({E.Step(TEXT("01"))}));
	auto* Anchor = E.Anchor(TEXT("Guide.Outside"));
	if (!TestNotNull(TEXT("Mock outside anchor"),Anchor)) { return false; }
	TestTrue(TEXT("Exact anchor position"),E.Runner->Panel->GetComponentLocation().Equals(Anchor->GetActorLocation(),0.01));
	TestTrue(TEXT("Exact anchor rotation"),E.Runner->Panel->GetComponentQuat().Equals(Anchor->GetActorQuat(),FMath::DegreesToRadians(0.01)));
	TestTrue(TEXT("Front faces anchor +X"),E.Runner->Panel->GetForwardVector().Equals(Anchor->GetActorForwardVector(),0.0001));
	TestFalse(TEXT("Instruction visible before tracking"),E.Runner->Panel->bHiddenInGame);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5zStableTest, "vBoR.P5z.E2MovementRouteAndViewpoint", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5zStableTest::RunTest(const FString&)
{
	FP5zWorld E; auto S = E.Step(TEXT("03")); S.RouteTag = TEXT("Route.ToDriverDoor"); E.Start({S}); E.Ready();
	const FTransform Panel = E.Runner->Panel->GetComponentTransform();
	const FTransform Button = FP5zTestAccess::Button(E.Runner,0)->GetActorTransform();
	E.Pawn->AddActorWorldOffset(FVector(100,20,0)); FP5zTestAccess::PawnMoved(E.Runner);
	E.WorldHead(FTransform(FRotator(0,75,0),FVector(100,100,160))); E.Runner->ProcessFlow(0);
	TestTrue(TEXT("Pawn/head move keeps panel"),E.Runner->Panel->GetComponentTransform().Equals(Panel,0.0001));
	const FVector End = E.Car->DriverDoorRoute->Spline->GetLocationAtSplinePoint(E.Car->DriverDoorRoute->Spline->GetNumberOfSplinePoints()-1,ESplineCoordinateSpace::World);
	E.WorldHead(FTransform(End+FVector(0,0,160))); E.Runner->ProcessFlow(0.6);
	TestTrue(TEXT("Real route arrival reached"),FP5zTestAccess::Arrived(E.Runner));
	TestEqual(TEXT("Arrival does not lock"),FP5zTestAccess::Lock(E.Runner),0.0);
	E.Key(EKeys::One); E.FinishSwitch();
	TestTrue(TEXT("Real viewpoint switched"),E.Car->Alignment->IsAtViewpoint(E.Car->PassengerView));
	E.Head->bTracked = false; E.Runner->ProcessFlow(0); E.Head->bTracked = true; E.Runner->ProcessFlow(0);
	TestTrue(TEXT("All events keep panel"),E.Runner->Panel->GetComponentTransform().Equals(Panel,0.0001));
	TestTrue(TEXT("All events keep button"),FP5zTestAccess::Button(E.Runner,0)->GetActorTransform().Equals(Button,0.0001));
	TestEqual(TEXT("No new placement lock"),FP5zTestAccess::Lock(E.Runner),0.0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5zStepsTest, "vBoR.P5z.E3ChangedSameAndEmptyTags", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5zStepsTest::RunTest(const FString&)
{
	FP5zWorld E; FP5zRecords Records;
	E.Start({E.Step(TEXT("01")),E.Step(TEXT("02"),TEXT("Guide.Trunk")),E.Step(TEXT("03"),TEXT("Guide.Trunk")),E.Step(TEXT("04"),NAME_None)});
	TestEqual(TEXT("First placement recorded once"),Records.Count(),1); E.Ready();
	E.Command(EVBoRFlowCommand::Next);
	TestTrue(TEXT("Different anchor moves"),E.Runner->Panel->GetComponentLocation().Equals(E.Anchor(TEXT("Guide.Trunk"))->GetActorLocation(),0.01));
	TestTrue(TEXT("Different anchor locks"),FP5zTestAccess::Lock(E.Runner)>0); TestEqual(TEXT("One new placement record"),Records.Count(),2);
	const FTransform Pose = E.Runner->Panel->GetComponentTransform(); E.Command(EVBoRFlowCommand::Next);
	TestTrue(TEXT("Same anchor keeps step-start lock"),FMath::IsNearlyEqual(FP5zTestAccess::Lock(E.Runner),static_cast<double>(E.Runner->ConfirmLockSeconds))); TestEqual(TEXT("Same anchor adds no placement record"),Records.Count(),2);
	E.Command(EVBoRFlowCommand::Next);
	TestTrue(TEXT("Empty tag retains position/rotation"),E.Runner->Panel->GetComponentTransform().Equals(Pose,0.0001));
	TestTrue(TEXT("Empty tag keeps step-start lock"),FMath::IsNearlyEqual(FP5zTestAccess::Lock(E.Runner),static_cast<double>(E.Runner->ConfirmLockSeconds))); TestEqual(TEXT("No redundant placement record"),Records.Count(),2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5zButtonsTest, "vBoR.P5z.E4TiltedButtons", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5zButtonsTest::RunTest(const FString&)
{
	FP5zWorld E; auto* Anchor = E.Anchor(TEXT("Guide.Outside")); Anchor->SetActorRotation(FRotator(20,45,0));
	E.Runner->bPanelFacesParticipant = false;
	E.Start({E.Step(TEXT("01"))});
	const FTransform Pose(Anchor->GetActorQuat(),Anchor->GetActorLocation());
	for (int32 I = 0; I < 2; ++I)
	{
		auto* Button = FP5zTestAccess::Button(E.Runner,I);
		// Half-height 16 cm + gap 8 cm, left/right 12 cm (D4)
		TestTrue(TEXT("Button offset in anchor frame"),Pose.InverseTransformPosition(Button->GetActorLocation()).Equals(FVector(0,I == 0 ? -12 : 12,-24),0.01));
		TestTrue(TEXT("Press +Z faces participant"),Button->GetActorUpVector().Equals(Anchor->GetActorForwardVector(),0.0001));
		TestTrue(TEXT("Label follows tilted frame"),Pose.InverseTransformPosition(FP5zTestAccess::Label(E.Runner,I)->GetComponentLocation()).Equals(FVector(0,I == 0 ? -12 : 12,-17),0.01));
	}
	E.Runner->PanelButtonGapCm = 12; Anchor->AddActorWorldOffset(FVector(10,0,0)); E.Command(EVBoRFlowCommand::Previous);
	TestTrue(TEXT("Editable gap applied"),FP5zTestAccess::Button(E.Runner,0)->GetActorLocation().Equals(Anchor->GetActorLocation()+Anchor->GetActorQuat().RotateVector(FVector(0,-12,-28)),0.01));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5zValidationTest, "vBoR.P5z.E5PanelTagValidation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5zValidationTest::RunTest(const FString&)
{
	FP5zWorld E;
	TestFalse(TEXT("Missing tag blocks"),E.Start({E.Step(TEXT("01"),TEXT("Missing"))}));
	TestTrue(TEXT("Problem identifies ID and tag"),E.Runner->ValidationProblems.Contains(TEXT("01: panel tag Missing")));
	auto* Duplicate = E.World->SpawnActor<AVBoRPanelAnchor>(); Duplicate->Tags.Add(TEXT("Guide.Outside"));
	TestFalse(TEXT("Duplicate tag blocks"),E.Start({E.Step(TEXT("01"))}));
	TestTrue(TEXT("Duplicate count reported"),E.Runner->ValidationProblems.Contains(TEXT("found 2"))); Duplicate->Destroy();
	auto* Plain = E.World->SpawnActor<AActor>(); Plain->Tags.Add(TEXT("NoScene"));
	TestFalse(TEXT("Non-scene blocks"),E.Start({E.Step(TEXT("01"),TEXT("NoScene"))}));
	TestTrue(TEXT("Non-scene problem identifies tag"),E.Runner->ValidationProblems.Contains(TEXT("01: panel tag NoScene")));
	TestFalse(TEXT("Fixed mode requires first tag"),E.Start({E.Step(TEXT("01"),NAME_None),E.Step(TEXT("02"))}));
	TestTrue(TEXT("First tag message"),E.Runner->ValidationProblems.Contains(TEXT("first step needs a panel tag in fixed mode")));
	E.Car->DriverEye->ComponentTags.Add(TEXT("Guide.Component"));
	TestTrue(TEXT("Scene component tag also works"),E.Start({E.Step(TEXT("01"),TEXT("Guide.Component"))}));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5zTextTest, "vBoR.P5z.E6InstructionOnlyAndHiddenStates", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5zTextTest::RunTest(const FString&)
{
	FP5zWorld E; E.Car->Alignment->State = EVBoRAlignmentState::NeedsCheck;
	E.Start({E.Step(TEXT("01"))});
	TestTrue(TEXT("Preparing has no text and hidden plate"),E.Runner->GetPanelText().IsEmpty() && E.Runner->Panel->bHiddenInGame);
	E.Car->Alignment->State = EVBoRAlignmentState::Applied; E.Start({E.Step(TEXT("01"))});
	TestEqual(TEXT("Only exact instruction"),E.Runner->GetPanelText(),FString(TEXT("Observe the vehicle.")));
	TestTrue(TEXT("Fixed operator plate hidden"),FP5zTestAccess::Operator(E.Runner)->bHiddenInGame);
	E.Ready(); E.Command(EVBoRFlowCommand::Confirm);
	TestTrue(TEXT("Completed guidance hidden"),E.Runner->IsFinished() && E.Runner->GetPanelText().IsEmpty() && E.Runner->Panel->bHiddenInGame);
	auto S = E.Step(TEXT("01"),NAME_None); S.Instruction = FText::GetEmpty(); E.Start({S});
	TestTrue(TEXT("Empty instruction hidden in head mode"),E.Runner->Panel->bHiddenInGame);
	TestTrue(TEXT("Head mode operator plate hidden"),FP5zTestAccess::Operator(E.Runner)->bHiddenInGame);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5zFreeTest, "vBoR.P5z.E7FreeModeButtonsAndOperatorKeys", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5zFreeTest::RunTest(const FString&)
{
	FP5zWorld E; E.Start({E.Step(TEXT("01"))}); E.Command(EVBoRFlowCommand::EnterFreeMode); E.Ready();
	TestTrue(TEXT("Free panel hidden"),E.Runner->Panel->bHiddenInGame);
	for (int32 I = 0; I < 2; ++I)
	{
		TestTrue(TEXT("Free button and label hidden"),FP5zTestAccess::Button(E.Runner,I)->IsHidden() && FP5zTestAccess::Label(E.Runner,I)->bHiddenInGame);
		TestFalse(TEXT("Free poke disabled"),FP5zTestAccess::Button(E.Runner,I)->Poke->IsActive());
		FP5zTestAccess::Poke(E.Runner,I); E.Runner->ProcessFlow(0);
	}
	TestFalse(TEXT("Injected button events never switch"),E.Car->Alignment->IsSwitching());
	TestTrue(TEXT("Base retained"),E.Car->Alignment->IsBaseViewpoint());
	E.Key(EKeys::One); E.FinishSwitch(); TestTrue(TEXT("Key 1 passenger"),E.Car->Alignment->IsAtViewpoint(E.Car->PassengerView));
	E.Key(EKeys::Two); E.FinishSwitch(); TestTrue(TEXT("Key 2 rear"),E.Car->Alignment->IsAtViewpoint(E.Car->RearView));
	E.Key(EKeys::Home); E.FinishSwitch(); TestTrue(TEXT("Home base"),E.Car->Alignment->IsBaseViewpoint());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5zLegacyTest, "vBoR.P5z.E8HeadPlacementRetained", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5zLegacyTest::RunTest(const FString&)
{
	FP5zWorld E; E.Start({E.Step(TEXT("01"),NAME_None)});
	TestTrue(TEXT("Untagged scenario stays ahead of head"),E.Runner->Panel->GetComponentLocation().Equals(E.Car->DriverEye->GetComponentLocation()+FVector(70,0,0),0.01));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5zAnchorTest, "vBoR.P5z.E9AnchorPreview", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5zAnchorTest::RunTest(const FString&)
{
	FP5zWorld E; int32 Count = 0;
	for (TActorIterator<AVBoRPanelAnchor> It(E.World); It; ++It)
	{
		++Count; TArray<UPrimitiveComponent*> Pieces; It->GetComponents(Pieces);
		TestEqual(TEXT("Panel, two button markers and arrow"),Pieces.Num(),4);
		for (auto* Piece : Pieces)
		{ TestTrue(TEXT("Preview hidden in game"),Piece->bHiddenInGame); TestEqual(TEXT("Preview has no collision"),Piece->GetCollisionEnabled(),ECollisionEnabled::NoCollision); }
	}
	TestEqual(TEXT("Mock provides five tagged anchors"),Count,5);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FR12Confirm, "vBoR.R12.E1OperatorOnlyConfirmation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FR12Confirm::RunTest(const FString&)
{
	FP5zWorld E;
	TArray<FVBoRScenarioStep> Rules = E.Runner->Scenario->Steps;
	for (auto Rule : {EVBoRStepRule::KeyHeld, EVBoRStepRule::KeyUnlockAndLock, EVBoRStepRule::PoweredOpen, EVBoRStepRule::PoweredClosed})
	{
		auto S = E.Step(TEXT("Extra"));
		S.Rule = Rule;
		S.TargetTag = Rule == EVBoRStepRule::KeyHeld || Rule == EVBoRStepRule::KeyUnlockAndLock ? TEXT("Car.FrontKey") : TEXT("Car.Liftgate");
		Rules.Add(S);
	}
	for (auto ConfirmBy : {EVBoRConfirmBy::None, EVBoRConfirmBy::Participant, EVBoRConfirmBy::Operator, EVBoRConfirmBy::Either})
	{
		for (auto S : Rules)
		{
			S.ConfirmBy = ConfirmBy;
			if (!TestTrue(TEXT("Valid rule target"), E.Start({S}))) { return false; }
			E.Ready();
			const auto* Button = FP5zTestAccess::Button(E.Runner, 0);
			TestTrue(TEXT("Confirm actor hidden, poke disabled"), Button->IsHidden() && !Button->Poke->IsActive());
		}
		auto S = E.Step(TEXT("Observe"));
		S.ConfirmBy = ConfirmBy;
		E.Start({S});
		E.Command(EVBoRFlowCommand::Confirm);
		TestFalse(TEXT("Operator confirm locked at step start"), E.Runner->IsFinished());
		E.Ready();
		FP5zTestAccess::Poke(E.Runner, 0);
		E.Runner->ProcessFlow(0);
		TestFalse(TEXT("Confirm poke cannot finish the step"), E.Runner->IsFinished());
		E.Command(EVBoRFlowCommand::Confirm);
		TestTrue(TEXT("Operator confirm finishes the step"), E.Runner->IsFinished());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FR12Consecutive, "vBoR.R12.E2ConsecutiveSeats", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FR12Consecutive::RunTest(const FString&)
{
	FP5zWorld E;
	E.Start({E.Step(TEXT("12"), TEXT("Guide.Driver")), E.SeatStep(TEXT("13"), true), E.SeatStep(TEXT("14"), false), E.Step(TEXT("15"), TEXT("Guide.Trunk"))});
	E.Ready(); E.Key(EKeys::SpaceBar);
	auto* Action = FP5zTestAccess::Button(E.Runner, 1);
	TestTrue(TEXT("Passenger step starts at driver panel"), E.AtAnchor(TEXT("Guide.Driver")));
	TestEqual(TEXT("Driver button"), FP5zTestAccess::ActionLabel(E.Runner), FString(TEXT("Move to passenger seat")));
	E.Action();
	TestTrue(TEXT("Button hidden during fade"), Action->IsHidden() && !Action->Poke->IsActive());
	E.FinishSwitch();
	TestEqual(TEXT("Arrival alone keeps passenger step"), E.Runner->GetStepIndex(), 1);
	TestTrue(TEXT("Passenger panel follows arrival"), E.AtAnchor(TEXT("Guide.Passenger")));
	TestTrue(TEXT("Only action shown at passenger seat"), !Action->IsHidden() && FP5zTestAccess::Button(E.Runner, 0)->IsHidden());
	TestEqual(TEXT("Passenger button"), FP5zTestAccess::ActionLabel(E.Runner), FString(TEXT("Move to rear seat")));
	E.Action(); E.FinishSwitch();
	TestEqual(TEXT("Seat advance completes passenger step"), E.Runner->GetStepIndex(), 2);
	TestTrue(TEXT("Rear seat and panel reached"), E.Car->Alignment->IsAtViewpoint(E.Car->RearView) && E.AtAnchor(TEXT("Guide.Rear")));
	TestTrue(TEXT("Completion attributed to participant"), E.Runner->LastCompletion.Contains(TEXT("Participant seat advance")));
	TestEqual(TEXT("Rear button"), FP5zTestAccess::ActionLabel(E.Runner), FString(TEXT("Return to driver seat")));
	E.Action(); E.FinishSwitch();
	TestEqual(TEXT("Return completes rear step"), E.Runner->GetStepIndex(), 3);
	TestTrue(TEXT("Driver seat and next panel reached"), E.Car->Alignment->IsBaseViewpoint() && E.AtAnchor(TEXT("Guide.Trunk")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FR12Rejected, "vBoR.R12.E3SeatAdvanceAtomic", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FR12Rejected::RunTest(const FString&)
{
	FP5zWorld E;
	E.Start({E.SeatStep(TEXT("13"), true), E.SeatStep(TEXT("14"), false)});
	TestTrue(TEXT("First seat step uses its own panel before arrival"), E.AtAnchor(TEXT("Guide.Passenger")));
	E.Action(); E.FinishSwitch();
	E.Car->Alignment->State = EVBoRAlignmentState::NeedsCheck;
	E.Action();
	TestEqual(TEXT("Rejected move leaves step active"), E.Runner->GetStepIndex(), 0);
	TestTrue(TEXT("Rejected move leaves seat unchanged"), E.Car->Alignment->IsAtViewpoint(E.Car->PassengerView));
	TestTrue(TEXT("Alignment rejection in operator status"), E.Runner->GetOperatorStatus().LastCommand.Contains(TEXT("Saved alignment required")));
	E.Car->Alignment->State = EVBoRAlignmentState::Applied;
	E.Action();
	E.Car->Alignment->State = EVBoRAlignmentState::NeedsCheck;
	E.FinishSwitch();
	TestEqual(TEXT("Interrupted fade also leaves step active"), E.Runner->GetStepIndex(), 0);
	E.Car->Alignment->State = EVBoRAlignmentState::Applied;
	const int32 Requests = E.Car->Alignment->SwitchCompletions;
	FP5zTestAccess::Poke(E.Runner, 1); FP5zTestAccess::Poke(E.Runner, 1);
	E.Runner->ProcessFlow(0);
	E.Action(); E.FinishSwitch();
	TestEqual(TEXT("Double press makes one seat transition"), E.Car->Alignment->SwitchCompletions, Requests + 1);
	TestEqual(TEXT("Double press completes one step"), E.Runner->GetStepIndex(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FR12Operator, "vBoR.R12.E4OperatorSeatNavigation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FR12Operator::RunTest(const FString&)
{
	FP5zWorld E;
	E.Start({E.Step(TEXT("12"), TEXT("Guide.Driver")), E.SeatStep(TEXT("13"), true), E.SeatStep(TEXT("14"), false)});
	E.Ready(); E.Command(EVBoRFlowCommand::Confirm);
	E.Action(); E.FinishSwitch(); E.Ready(); E.Command(EVBoRFlowCommand::Confirm);
	TestEqual(TEXT("Operator completed passenger step"), E.Runner->GetStepIndex(), 2);
	TestTrue(TEXT("Operator confirm leaves participant in passenger seat"), E.Car->Alignment->IsAtViewpoint(E.Car->PassengerView) && E.AtAnchor(TEXT("Guide.Passenger")));
	TestEqual(TEXT("Next step offers rear move"), FP5zTestAccess::ActionLabel(E.Runner), FString(TEXT("Move to rear seat")));
	E.Command(EVBoRFlowCommand::Previous);
	TestEqual(TEXT("Previous returns to passenger step"), E.Runner->GetStepIndex(), 1);
	TestEqual(TEXT("Already seated, action targets next seat"), FP5zTestAccess::ActionLabel(E.Runner), FString(TEXT("Move to rear seat")));
	FP5zTestAccess::NewInputFrame(E.Runner); E.Key(EKeys::Home); E.FinishSwitch();
	TestEqual(TEXT("Back at driver, action targets current step"), FP5zTestAccess::ActionLabel(E.Runner), FString(TEXT("Move to passenger seat")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FR12Last, "vBoR.R12.E5LastAndSingleSeat", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FR12Last::RunTest(const FString&)
{
	for (bool bLast : {false, true})
	{
		FP5zWorld E;
		const auto Seat = E.SeatStep(TEXT("13"), true);
		if (bLast) { E.Start({Seat}); }
		else { E.Start({Seat, E.Step(TEXT("14"), TEXT("Guide.Driver"))}); }
		E.Action(); E.FinishSwitch();
		TestEqual(TEXT("Single seat action returns to driver"), FP5zTestAccess::ActionLabel(E.Runner), FString(TEXT("Return to driver seat")));
		E.Action(); E.FinishSwitch();
		TestTrue(TEXT("Driver seat reached"), E.Car->Alignment->IsBaseViewpoint());
		if (bLast) { TestTrue(TEXT("Last seat completes guidance"), E.Runner->IsFinished()); }
		else { TestTrue(TEXT("Confirm step starts at driver panel"), E.Runner->GetStepIndex() == 1 && E.AtAnchor(TEXT("Guide.Driver"))); }
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FR12Facing, "vBoR.R12.E6PanelFacing", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FR12Facing::RunTest(const FString&)
{
	FP5zWorld E; FP5zRecords Records;
	auto* Anchor = E.Anchor(TEXT("Guide.Outside"));
	Anchor->SetActorRotation(FRotator(20, 45, 15));
	const FVector Point = Anchor->GetActorLocation();
	E.WorldHead(FTransform(Point + FVector(0, 100, 20)));
	auto S = E.Step(TEXT("Observe"));
	E.Start({S, S, S, S});
	TestTrue(TEXT("Placement preserves anchor location"), E.AtAnchor(TEXT("Guide.Outside")));
	TestTrue(TEXT("Placement changes only anchor yaw"), E.Runner->Panel->GetComponentRotation().Equals(FRotator(20, 90, 15), 0.01));
	E.WorldHead(FTransform(Point + FVector(100, 0, 30)));
	E.Ready(); E.Ready();
	TestEqual(TEXT("Stationary placement records only once"), Records.Count(), 1);
	TestEqual(TEXT("Lock expires despite head motion"), FP5zTestAccess::Lock(E.Runner), 0.0);
	TestTrue(TEXT("Panel does not follow head"), E.Runner->Panel->GetComponentRotation().Equals(FRotator(20, 90, 15), 0.01));
	E.Command(EVBoRFlowCommand::Next);
	TestTrue(TEXT("Next placement recomputes yaw"), E.Runner->Panel->GetComponentRotation().Equals(FRotator(20, 0, 15), 0.01));
	E.Head->bTracked = false;
	E.Command(EVBoRFlowCommand::Next);
	TestTrue(TEXT("No head tracking uses anchor rotation"), E.Runner->Panel->GetComponentRotation().Equals(Anchor->GetActorRotation(), 0.01));
	E.Head->bTracked = true; E.WorldHead(FTransform(Point + FVector(0, 9, 30)));
	E.Command(EVBoRFlowCommand::Next);
	TestTrue(TEXT("Head within 10 cm horizontally uses anchor yaw"), E.Runner->Panel->GetComponentRotation().Equals(Anchor->GetActorRotation(), 0.01));
	E.Runner->bPanelFacesParticipant = false; E.WorldHead(FTransform(Point + FVector(100, 0, 30)));
	E.Command(EVBoRFlowCommand::Previous);
	TestTrue(TEXT("Disabled facing uses anchor rotation"), E.Runner->Panel->GetComponentRotation().Equals(Anchor->GetActorRotation(), 0.01));
	return true;
}

#if WITH_METADATA
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FR12Editing, "vBoR.R12.E7ScenarioEditing", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FR12Editing::RunTest(const FString&)
{
	FP5zWorld E;
	auto* Asset = E.Runner->Scenario.Get();
	const auto* Steps = FindFProperty<FProperty>(UVBoRScenarioAsset::StaticClass(), TEXT("Steps"));
	TestEqual(TEXT("Step array title template"), Steps->GetMetaData(TEXT("TitleProperty")), FString(TEXT("{StepId} · {Instruction}")));
	const auto* StepStruct = FVBoRScenarioStep::StaticStruct();
	for (const TCHAR* Field : {TEXT("TargetTag"), TEXT("DoorOpenDeg"), TEXT("SeatedSeconds")})
	{
		const auto* Property = FindFProperty<FProperty>(StepStruct, Field);
		TestTrue(Field, Property->HasMetaData(TEXT("EditCondition")) && Property->HasMetaData(TEXT("EditConditionHides")));
	}
	for (const TCHAR* Field : {TEXT("PanelTag"), TEXT("RouteTag"), TEXT("MarkerTag"), TEXT("TargetTag")})
	{
		const auto* Property = FindFProperty<FProperty>(StepStruct, Field);
		TestTrue(Field, Property->HasMetaData(TEXT("DisplayName")) && Asset->FindFunction(*Property->GetMetaData(TEXT("GetOptions"))) != nullptr);
	}
	const auto* ConfirmBy = FindFProperty<FProperty>(StepStruct, TEXT("ConfirmBy"));
	TestEqual(TEXT("Confirmation display name"), ConfirmBy->GetMetaData(TEXT("DisplayName")), FString(TEXT("Operator confirm")));
	Asset->Steps[0].PanelTag = TEXT("Legacy.Panel");
	const auto Panels = FP5zTestAccess::Options(Asset, &FVBoRScenarioStep::PanelTag, E.World, AVBoRPanelAnchor::StaticClass());
	TestTrue(TEXT("With a level, list its panel anchors only"), Panels[0] == TEXT("None") && Panels.Contains(TEXT("Guide.Driver")) && !Panels.Contains(TEXT("Route.ToDriverDoor")) && !Panels.Contains(TEXT("Legacy.Panel")));
	const auto Routes = FP5zTestAccess::Options(Asset, &FVBoRScenarioStep::RouteTag, E.World, AVBoRFloorRoute::StaticClass());
	TestTrue(TEXT("Route option comes from route actor"), Routes.Contains(TEXT("Route.ToDriverDoor")) && !Routes.Contains(TEXT("Guide.Driver")));
	const auto Targets = FP5zTestAccess::Options(Asset, &FVBoRScenarioStep::TargetTag, E.World);
	TestTrue(TEXT("Car actor and component tags only"), Targets.Contains(TEXT("Car.DriverDoor")) && Targets.Contains(TEXT("Car.PassengerView")) && !Targets.Contains(TEXT("Guide.Driver")));
	const auto Saved = FP5zTestAccess::Options(Asset, &FVBoRScenarioStep::PanelTag, nullptr, AVBoRPanelAnchor::StaticClass());
	TestTrue(TEXT("Without an editor level, offer None and saved tag"), Saved == TArray<FString>{TEXT("None"), TEXT("Legacy.Panel")});
	TestEqual(TEXT("Stored tag unchanged"), Asset->Steps[0].PanelTag, FName(TEXT("Legacy.Panel")));
	return true;
}
#endif

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FR12Spectator, "vBoR.R12.E8SpectatorFallback", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FR12Spectator::RunTest(const FString&)
{
	FVBoRScenarioStep S; S.StepId = TEXT("08");
	const FText Unknown = FText::FromString(TEXT("Unknown"));
	S.SpectatorText = FText::FromString(TEXT("TV override"));
	S.Instruction = FText::FromString(TEXT("Participant instruction"));
	TestEqual(TEXT("TV text wins"), VBoRSpectator::MissionText(S, Unknown).ToString(), FString(TEXT("TV override")));
	S.SpectatorText = FText::GetEmpty(); S.StepId = TEXT("Renumbered");
	TestEqual(TEXT("Use instruction even for renumbered step"), VBoRSpectator::MissionText(S, Unknown).ToString(), FString(TEXT("Participant instruction")));
	S.Instruction = FText::GetEmpty(); S.StepId = TEXT("08");
	TestEqual(TEXT("Original ID supplies fallback"), VBoRSpectator::MissionText(S, Unknown).ToString(), FString(TEXT("Open the door.")));
	S.StepId = TEXT("Renumbered");
	TestTrue(TEXT("Unknown without any other text"), VBoRSpectator::MissionText(S, Unknown).EqualTo(Unknown));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FR12Legacy, "vBoR.R12.E9LegacyConfirmValues", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FR12Legacy::RunTest(const FString&)
{
	FP5zWorld E;
	auto A = E.Step(TEXT("LegacyA")); A.ConfirmBy = static_cast<EVBoRConfirmBy>(1);
	auto B = E.Step(TEXT("LegacyB")); B.ConfirmBy = static_cast<EVBoRConfirmBy>(3);
	E.Start({A, B});
	E.Ready(); E.Command(EVBoRFlowCommand::Confirm);
	TestEqual(TEXT("Legacy Participant completed by operator"), E.Runner->GetStepIndex(), 1);
	E.Ready(); E.Command(EVBoRFlowCommand::Confirm);
	TestTrue(TEXT("Legacy Either completed by operator"), E.Runner->IsFinished());
	TestEqual(TEXT("Participant still stored"), E.Runner->Scenario->Steps[0].ConfirmBy, EVBoRConfirmBy::Participant);
	TestEqual(TEXT("Either still stored"), E.Runner->Scenario->Steps[1].ConfirmBy, EVBoRConfirmBy::Either);
	return true;
}
#endif
