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

struct FP5zTestAccess
{
	static AVBoRPokeButton* Button(AVBoRScenarioRunner* R, int32 I) { return I == 0 ? R->ConfirmButton.Get() : R->ActionButton.Get(); }
	static UWidgetComponent* Label(AVBoRScenarioRunner* R, int32 I) { return R->ButtonLabels[I]; }
	static UWidgetComponent* Operator(AVBoRScenarioRunner* R) { return R->OperatorPanel; }
	static double Lock(AVBoRScenarioRunner* R) { return R->ConfirmLockRemaining; }
	static bool Arrived(AVBoRScenarioRunner* R) { return R->bRouteArrived; }
	static void PawnMoved(AVBoRScenarioRunner* R) { R->HandlePawnMoved(); }
	static void Poke(AVBoRScenarioRunner* R, int32 I)
	{
		R->LiveFrame = MAX_uint64;
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
		bool Start(std::initializer_list<FVBoRScenarioStep> Steps)
		{ Runner->Scenario->Steps.Reset(); for (const auto& S : Steps) { Runner->Scenario->Steps.Add(S); } return Runner->InitializeFlow(); }
		void Command(EVBoRFlowCommand Action)
		{ Runner->QueueCommand({Action,EVBoRCommandSource::Operator,Runner->GetToken()}); Runner->ProcessFlow(0); }
		void Ready() { Runner->ProcessFlow(0); Runner->ProcessFlow(1); }
		void Key(FKey Key) { Runner->HandleOperatorKey(Key); Runner->ProcessFlow(0); }
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
#endif
