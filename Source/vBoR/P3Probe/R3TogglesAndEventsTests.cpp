#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "R3TestObserver.h"
#include "../Interaction/VBoRVehiclePowerComponent.h"
#include "../Interaction/VBoRVehicleAlignmentComponent.h"
#include "../Interaction/VBoRScenarioRunner.h"
#include "../Interaction/VBoRPokeButton.h"
#include "../Interaction/VBoRHingedDoor.h"
#include "../Interaction/VBoRBoxStand.h"
#include "../Interaction/VBoRCarryObject.h"
#include "Components/SceneComponent.h"
#include "CoreGlobals.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/WorldSettings.h"
#include "Misc/OutputDevice.h"
#include "Misc/OutputDeviceRedirector.h"
#include "Sound/SoundWave.h"
#include <initializer_list>

namespace
{
struct FR3Log : FOutputDevice
{
	int32 MirrorWarnings = 0;
	FR3Log() { GLog->FlushThreadedLogs(); GLog->AddOutputDevice(this); }
	~FR3Log() { GLog->FlushThreadedLogs(); GLog->RemoveOutputDevice(this); }
	virtual void Serialize(const TCHAR* Text, ELogVerbosity::Type Verbosity, const FName&) override
	{ if (Verbosity == ELogVerbosity::Warning && FString(Text).StartsWith(TEXT("Side mirror:"))) { ++MirrorWarnings; } }
};

struct FR3World
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	AActor* Car;
	UVBoRVehiclePowerComponent* Power = nullptr;
	UVBoRVehicleAlignmentComponent* Alignment;
	UR3TestObserver* Observer;
	AVBoRFoldingMirror* Mirror;
	AVBoRFoldingSeat* Seat;
	uint64 OriginalFrame = GFrameCounter;
	FR3World(bool bFollow = true, bool bWithPower = true)
	{
		GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
		auto* PC = World->SpawnActor<APlayerController>(); PC->Possess(World->SpawnActor<APawn>());
		Car = World->SpawnActor<AActor>();
		Car->SetRootComponent(NewObject<USceneComponent>(Car)); Car->GetRootComponent()->RegisterComponent();
		Alignment = NewObject<UVBoRVehicleAlignmentComponent>(Car); Car->AddInstanceComponent(Alignment); Alignment->RegisterComponent();
		Observer = NewObject<UR3TestObserver>(Car);
		if (bWithPower)
		{
			Power = NewObject<UVBoRVehiclePowerComponent>(Car); Car->AddInstanceComponent(Power); Power->RegisterComponent();
			World->SpawnActor<AVBoRPokeButton>()->Tags.Add(TEXT("Car.StartButton"));
		}
		Mirror = NewMirror(bFollow);
		Seat = World->SpawnActorDeferred<AVBoRFoldingSeat>(AVBoRFoldingSeat::StaticClass(), FTransform::Identity);
		Observe(Seat); Seat->Fold(); Seat->FinishSpawning(FTransform::Identity);
	}
	void Observe(AVBoRFoldingMirror* Part)
	{ Part->OnStateChanged.AddDynamic(Observer, &UR3TestObserver::MirrorChanged); Part->OnReset.AddDynamic(Observer, &UR3TestObserver::MirrorReset); }
	void Observe(AVBoRFoldingSeat* Part)
	{ Part->OnStateChanged.AddDynamic(Observer, &UR3TestObserver::SeatChanged); Part->OnReset.AddDynamic(Observer, &UR3TestObserver::SeatReset); }
	void Observe(AVBoRPoweredPanel* Part)
	{ Part->OnReset.AddDynamic(Observer, &UR3TestObserver::GateReset); Part->OnStateChanged.AddDynamic(Observer, &UR3TestObserver::GateChanged); }
	void Observe(AVBoRHingedPanel* Part) { Part->OnReset.AddDynamic(Observer, &UR3TestObserver::DoorReset); }
	AVBoRFoldingMirror* NewMirror(bool bFollow)
	{
		auto* Part = World->SpawnActorDeferred<AVBoRFoldingMirror>(AVBoRFoldingMirror::StaticClass(), FTransform::Identity);
		Part->bFollowVehiclePower = bFollow;
		Part->UnfoldMotorCue.Sound = NewObject<USoundWave>(Part); Part->FoldMotorCue.Sound = NewObject<USoundWave>(Part);
		Observe(Part); Part->FinishSpawning(FTransform::Identity); return Part;
	}
	void Play()
	{ World->InitializeActorsForPlay(FURL()); World->GetWorldSettings()->NotifyBeginPlay(); Alignment->State = EVBoRAlignmentState::Applied; }
	AVBoRScenarioRunner* NewRunner(const TArray<FVBoRScenarioStep>& Steps)
	{
		auto* Runner = World->SpawnActor<AVBoRScenarioRunner>();
		Runner->Scenario = NewObject<UVBoRScenarioAsset>(Runner); Runner->Scenario->Steps = Steps; return Runner;
	}
	void ClearEvents()
	{
		Observer->MirrorStates.Reset(); Observer->SeatStates.Reset(); Observer->Resets.Reset();
		Observer->MirrorOrder.Reset(); Observer->SeatOrder.Reset(); Observer->GateOrder.Reset(); Observer->GateStates = 0;
	}
	~FR3World()
	{ World->EndPlay(EEndPlayReason::Quit); World->DestroyWorld(false); GEngine->DestroyWorldContext(World); GFrameCounter = OriginalFrame; }
};

FVBoRScenarioStep R3Step(const TCHAR* Id, const TCHAR* Tag = TEXT(""))
{ FVBoRScenarioStep Step; Step.StepId = Id; Step.Rule = EVBoRStepRule::Confirm; Step.ConfirmBy = EVBoRConfirmBy::Operator; Step.TargetTag = Tag; return Step; }

void R3MirrorStates(FAutomationTestBase& Test, FR3World& W, std::initializer_list<EVBoRFoldingMirrorState> Expected)
{
	const auto& Actual = W.Observer->MirrorStates.FindOrAdd(W.Mirror);
	Test.TestEqual(TEXT("Mirror transition count"), Actual.Num(), static_cast<int32>(Expected.size()));
	int32 I = 0; for (auto State : Expected) { if (Actual.IsValidIndex(I)) { Test.TestEqual(TEXT("Mirror transition order"), Actual[I], State); } ++I; }
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FR3PowerEvents, "vBoR.R3.E1DefaultPowerEvents", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FR3PowerEvents::RunTest(const FString&)
{
	FR3World W; W.Play();
	TestTrue(TEXT("Follow power defaults on"), GetDefault<AVBoRFoldingMirror>()->bFollowVehiclePower);
	W.Power->SetPowerOn(true); W.Mirror->Tick(10); W.Power->SetPowerOn(false); W.Mirror->Tick(10);
	TestEqual(TEXT("Power cycle returns folded"), W.Mirror->GetState(), EVBoRFoldingMirrorState::Folded);
	TestEqual(TEXT("Folded angle preserved"), W.Mirror->GetAngleDeg(), -65.f);
	R3MirrorStates(*this, W, {EVBoRFoldingMirrorState::Unfolding, EVBoRFoldingMirrorState::Unfolded, EVBoRFoldingMirrorState::Folding, EVBoRFoldingMirrorState::Folded});
	TestEqual(TEXT("Unfold motor once"), W.Mirror->GetSoundPlaybackForTest(0).Requests, 1);
	TestEqual(TEXT("Fold motor once"), W.Mirror->GetSoundPlaybackForTest(1).Requests, 1); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FR3PowerOff, "vBoR.R3.E2FollowDisabled", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FR3PowerOff::RunTest(const FString&)
{
	{
		FR3World W(false); W.Play(); W.Power->SetPowerOn(true); W.Mirror->Tick(10); W.Power->SetPowerOn(false); W.Mirror->Tick(10);
		TestEqual(TEXT("Disabled follow stays folded"), W.Mirror->GetState(), EVBoRFoldingMirrorState::Folded);
		R3MirrorStates(*this, W, {});
		TestEqual(TEXT("No unfold motor"), W.Mirror->GetSoundPlaybackForTest(0).Requests, 0);
		TestEqual(TEXT("No fold motor"), W.Mirror->GetSoundPlaybackForTest(1).Requests, 0);
	}
	FR3World W(false, false); FR3Log Log; W.Play(); GLog->FlushThreadedLogs();
	TestEqual(TEXT("No power needed, no mirror warning"), Log.MirrorWarnings, 0); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FR3ManualUnfold, "vBoR.R3.E3ManualCommands", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FR3ManualUnfold::RunTest(const FString&)
{
	FR3World W(false); W.Play();
	TestFalse(TEXT("Fold at folded endpoint ignored"), W.Mirror->Fold());
	TestTrue(TEXT("Manual unfold accepted"), W.Mirror->Unfold());
	TestFalse(TEXT("Repeated unfold in motion ignored"), W.Mirror->Unfold()); W.Mirror->Tick(10);
	TestEqual(TEXT("Manual unfold reaches zero deg"), W.Mirror->GetAngleDeg(), 0.f);
	TestFalse(TEXT("Unfold at endpoint ignored"), W.Mirror->Unfold()); W.Mirror->Tick(10);
	R3MirrorStates(*this, W, {EVBoRFoldingMirrorState::Unfolding, EVBoRFoldingMirrorState::Unfolded});
	TestEqual(TEXT("Ignored requests do not restart motor"), W.Mirror->GetSoundPlaybackForTest(0).Requests, 1); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FR3Reverse, "vBoR.R3.E4ReverseInMotion", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FR3Reverse::RunTest(const FString&)
{
	FR3World W(false); W.Play(); W.Mirror->Unfold(); W.Mirror->Tick(.5f);
	const float Before = W.Mirror->GetAngleDeg();
	TestTrue(TEXT("Reverse accepted"), W.Mirror->Fold()); TestEqual(TEXT("Reverse preserves angle"), W.Mirror->GetAngleDeg(), Before);
	W.Mirror->Tick(.1f); TestTrue(TEXT("Reverse moves toward folded"), W.Mirror->GetAngleDeg() < Before); W.Mirror->Tick(10);
	R3MirrorStates(*this, W, {EVBoRFoldingMirrorState::Unfolding, EVBoRFoldingMirrorState::Folding, EVBoRFoldingMirrorState::Folded});
	TestTrue(TEXT("Manual unfold can reverse folding"), W.Mirror->Unfold()); W.Mirror->Tick(.5f); W.Mirror->Fold(); W.Mirror->Tick(.1f);
	TestTrue(TEXT("Opposite reverse accepted"), W.Mirror->Unfold()); W.Mirror->Tick(10);
	TestEqual(TEXT("Second reversal reaches unfolded"), W.Mirror->GetState(), EVBoRFoldingMirrorState::Unfolded); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FR3PowerEndpoint, "vBoR.R3.E5PowerAtEndpoint", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FR3PowerEndpoint::RunTest(const FString&)
{
	FR3World W; W.Play(); W.Mirror->Unfold(); W.Mirror->Tick(10); W.ClearEvents();
	const int32 Requests = W.Mirror->GetSoundPlaybackForTest(0).Requests;
	W.Power->SetPowerOn(true); W.Mirror->Tick(.1f);
	TestEqual(TEXT("Power on keeps unfolded endpoint"), W.Mirror->GetState(), EVBoRFoldingMirrorState::Unfolded);
	TestEqual(TEXT("Power on keeps endpoint angle"), W.Mirror->GetAngleDeg(), 0.f);
	R3MirrorStates(*this, W, {}); TestEqual(TEXT("Power on does not restart motor"), W.Mirror->GetSoundPlaybackForTest(0).Requests, Requests);
	W.Power->SetPowerOn(false); W.Mirror->Tick(10);
	R3MirrorStates(*this, W, {EVBoRFoldingMirrorState::Folding, EVBoRFoldingMirrorState::Folded}); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FR3SeatEvents, "vBoR.R3.E6SeatTransitions", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FR3SeatEvents::RunTest(const FString&)
{
	FR3World W; W.Play(); TestTrue(TEXT("Fold accepted"), W.Seat->Fold()); TestFalse(TEXT("Repeated fold ignored"), W.Seat->Fold()); W.Seat->Tick(10);
	TestFalse(TEXT("Fold at endpoint ignored"), W.Seat->Fold()); TestTrue(TEXT("Unfold accepted"), W.Seat->Unfold()); TestFalse(TEXT("Repeated unfold ignored"), W.Seat->Unfold()); W.Seat->Tick(10);
	const TArray<EVBoRFoldingSeatState> Expected = {EVBoRFoldingSeatState::Folding, EVBoRFoldingSeatState::Folded, EVBoRFoldingSeatState::Unfolding, EVBoRFoldingSeatState::Standing};
	TestTrue(TEXT("Seat events once per transition"), W.Observer->SeatStates.FindOrAdd(W.Seat) == Expected);
	TestEqual(TEXT("Seat upright angle"), W.Seat->GetAngleDeg(), 0.f); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FR3Resets, "vBoR.R3.E7RuntimeResets", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FR3Resets::RunTest(const FString&)
{
	FR3World W; auto* Gate = W.World->SpawnActor<AVBoRPoweredPanel>(); auto* Door = W.World->SpawnActor<AVBoRHingedPanel>();
	W.Observe(Gate); W.Observe(Door); W.Play(); W.Mirror->Unfold(); W.Mirror->Tick(10); W.Seat->Fold(); W.Seat->Tick(10); W.ClearEvents();
	W.Mirror->ResetMirror(); W.Seat->ResetSeat(); Gate->ResetPanel(); Door->ResetDoor();
	for (AActor* Part : TArray<AActor*>{W.Mirror, W.Seat, Gate, Door}) { TestEqual(TEXT("One reset event per part"), W.Observer->Resets.FindRef(Part), 1); }
	R3MirrorStates(*this, W, {EVBoRFoldingMirrorState::Folded});
	TestTrue(TEXT("Seat reset publishes standing"), W.Observer->SeatStates.FindOrAdd(W.Seat) == TArray<EVBoRFoldingSeatState>{EVBoRFoldingSeatState::Standing});
	TestTrue(TEXT("Seat state precedes reset"), W.Observer->SeatOrder == TArray<FString>{TEXT("Standing"), TEXT("Reset")});
	TestTrue(TEXT("Gate state precedes reset"), W.Observer->GateOrder == TArray<FString>{TEXT("State"), TEXT("Reset")});
	W.Mirror->ResetMirror(); W.Seat->ResetSeat(); Gate->ResetPanel(); Door->ResetDoor();
	R3MirrorStates(*this, W, {EVBoRFoldingMirrorState::Folded}); TestEqual(TEXT("Unchanged seat has no extra state event"), W.Observer->SeatStates.FindOrAdd(W.Seat).Num(), 1);
	TestEqual(TEXT("Legacy gate state published even unchanged"), W.Observer->GateStates, 2);
	for (AActor* Part : TArray<AActor*>{W.Mirror, W.Seat, Gate, Door}) { TestEqual(TEXT("Each reset call publishes once"), W.Observer->Resets.FindRef(Part), 2); }
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FR3Startup, "vBoR.R3.E8StartupSuppression", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FR3Startup::RunTest(const FString&)
{
	FR3World W;
	auto* Gate = W.World->SpawnActorDeferred<AVBoRPoweredPanel>(AVBoRPoweredPanel::StaticClass(), FTransform::Identity); W.Observe(Gate); Gate->FinishSpawning(FTransform::Identity);
	auto* Door = W.World->SpawnActorDeferred<AVBoRHingedPanel>(AVBoRHingedPanel::StaticClass(), FTransform::Identity); W.Observe(Door); Door->FinishSpawning(FTransform::Identity);
	TestEqual(TEXT("Construction reset events suppressed"), W.Observer->Resets.Num(), 0);
	TestEqual(TEXT("Construction mirror events suppressed"), W.Observer->MirrorStates.Num(), 0);
	TestEqual(TEXT("Construction seat events suppressed"), W.Observer->SeatStates.Num(), 0);
	W.Play();
	TestEqual(TEXT("BeginPlay reset events suppressed"), W.Observer->Resets.Num(), 0);
	TestEqual(TEXT("Power off startup mirror events suppressed"), W.Observer->MirrorStates.Num(), 0);
	TestEqual(TEXT("Seat startup events suppressed"), W.Observer->SeatStates.Num(), 0);
	W.Power->SetPowerOn(true); W.ClearEvents();
	// Spawn into playing world with power already on (R3 E8)
	W.Mirror = W.NewMirror(true);
	TestEqual(TEXT("Powered startup starts unfolding"), W.Mirror->GetState(), EVBoRFoldingMirrorState::Unfolding);
	R3MirrorStates(*this, W, {}); TestEqual(TEXT("Powered startup has no reset event"), W.Observer->Resets.Num(), 0);
	W.Mirror->Tick(10); R3MirrorStates(*this, W, {EVBoRFoldingMirrorState::Unfolded}); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FR3Restart, "vBoR.R3.E9RunnerResetTargets", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FR3Restart::RunTest(const FString&)
{
	FR3World W;
	auto* Gate = W.World->SpawnActor<AVBoRPoweredPanel>(); Gate->Tags.Add(TEXT("Car.Liftgate")); W.Observe(Gate);
	auto* TargetGate = W.World->SpawnActor<AVBoRPoweredPanel>(); TargetGate->Tags.Add(TEXT("Test.Gate")); W.Observe(TargetGate);
	auto* Door = W.World->SpawnActor<AVBoRHingedDoor>(); Door->Tags.Add(TEXT("Car.DriverDoor")); W.Observe(Door);
	auto* TargetDoor = W.World->SpawnActor<AVBoRHingedDoor>(); TargetDoor->Tags.Add(TEXT("Test.Door")); W.Observe(TargetDoor);
	auto* Console = W.World->SpawnActor<AVBoRHingedPanel>(); Console->Tags.Add(TEXT("Test.Console")); W.Observe(Console);
	auto* OtherSeat = W.World->SpawnActor<AVBoRFoldingSeat>(); W.Observe(OtherSeat);
	auto* OtherMirror = W.NewMirror(false);
	auto* Stand = W.World->SpawnActor<AVBoRBoxStand>(); Stand->SetActorLocation(FVector(2000,0,0));
	auto* Runner = W.NewRunner({R3Step(TEXT("01"), TEXT("Car.Liftgate")), R3Step(TEXT("02"), TEXT("Car.DriverDoor")), R3Step(TEXT("03"), TEXT("Test.Gate")), R3Step(TEXT("04"), TEXT("Test.Door")), R3Step(TEXT("05"), TEXT("Test.Console"))});
	W.Play(); TestTrue(TEXT("Restart fixture flow valid"), Runner->InitializeFlow());
	Door->Unlock(); TargetDoor->Unlock(); W.Seat->Fold(); OtherSeat->Fold(); W.Seat->Tick(10); OtherSeat->Tick(10);
	Gate->RequestOpen(); Gate->Tick(10); TargetGate->RequestOpen(); TargetGate->Tick(10);
	W.Power->SetPowerOn(true); W.Mirror->Tick(10); OtherMirror->Unfold(); OtherMirror->Tick(10); W.ClearEvents();
	Runner->PressOperatorButton(EVBoROperatorButton::Restart); Runner->ProcessFlow(0);
	const TArray<AActor*> ResetParts = {Gate, TargetGate, Door, TargetDoor, W.Seat, OtherSeat, W.Mirror, OtherMirror};
	for (AActor* Part : ResetParts) { TestEqual(TEXT("Runner resets each selected part once"), W.Observer->Resets.FindRef(Part), 1); }
	TestEqual(TEXT("Runner does not reset console target"), W.Observer->Resets.FindRef(Console), 0);
	TestTrue(TEXT("Driver door closed and locked"), Door->GetMotion().bLocked && Door->GetMotion().AngleDeg == 0);
	TestTrue(TEXT("Target door closed and locked"), TargetDoor->GetMotion().bLocked && TargetDoor->GetMotion().AngleDeg == 0);
	TestEqual(TEXT("Liftgate reset"), Gate->GetState(), EVBoRPoweredState::Closed); TestEqual(TEXT("Target gate reset"), TargetGate->GetState(), EVBoRPoweredState::Closed);
	TestEqual(TEXT("Seat reset upright"), W.Seat->GetState(), EVBoRFoldingSeatState::Standing); TestEqual(TEXT("Other seat reset upright"), OtherSeat->GetState(), EVBoRFoldingSeatState::Standing);
	int32 Boxes = 0; for (TActorIterator<AVBoRCarryObject> It(W.World); It; ++It) { ++Boxes; }
	TestEqual(TEXT("Stand contains one box after restart"), Boxes, 1); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FR3RestartOrder, "vBoR.R3.E10RestartEventOrder", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FR3RestartOrder::RunTest(const FString&)
{
	for (bool bFollow : {true, false})
	{
		FR3World W(bFollow); auto* Runner = W.NewRunner({R3Step(TEXT("01"))}); W.Play(); TestTrue(TEXT("Flow valid"), Runner->InitializeFlow());
		if (bFollow) { W.Power->SetPowerOn(true); } else { W.Mirror->Unfold(); }
		W.Mirror->Tick(10); W.ClearEvents(); Runner->PressOperatorButton(EVBoROperatorButton::Restart); Runner->ProcessFlow(0);
		const TArray<FString> Expected = bFollow ? TArray<FString>{TEXT("Folding"), TEXT("Folded"), TEXT("Reset")} : TArray<FString>{TEXT("Folded"), TEXT("Reset")};
		TestTrue(TEXT("Reset event last, preserves power-off ordering"), W.Observer->MirrorOrder == Expected);
		TestEqual(TEXT("Reset once"), W.Observer->Resets.FindRef(W.Mirror), 1);
		TestEqual(TEXT("Immediately folded"), W.Mirror->GetState(), EVBoRFoldingMirrorState::Folded);
		TestEqual(TEXT("Reset stops unfold motor"), W.Mirror->GetSoundPlaybackForTest(0).Requests, 0);
		TestEqual(TEXT("Reset clears fold motor request"), W.Mirror->GetSoundPlaybackForTest(1).Requests, 0);
		TestFalse(TEXT("No unfold pending"), W.Mirror->GetSoundPlaybackForTest(0).HasPending());
		TestFalse(TEXT("No fold pending"), W.Mirror->GetSoundPlaybackForTest(1).HasPending());
		W.Mirror->Tick(10); TestTrue(TEXT("No later state events"), W.Observer->MirrorOrder == Expected);
	}
	return true;
}
#endif
