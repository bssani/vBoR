#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "P5iTestFixture.h"
#include "P5gTestFixture.h"
#include "P3VehicleMock.h"
#include "../Interaction/VBoRScenarioRunner.h"
#include "../Interaction/VBoREyePointComponent.h"
#include "../Interaction/VBoRViewpointComponent.h"
#include "../Interaction/VBoRPokeButton.h"
#include "Components/InputComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "HAL/IConsoleManager.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/WorldSettings.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Misc/OutputDevice.h"
#include "Misc/OutputDeviceRedirector.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/SlateRenderer.h"
#include "Fonts/FontMeasure.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/SWindow.h"
#include "XRTrackingSystemBase.h"

struct FP5iTestAccess
{
	static bool StartPending(AVBoRScenarioRunner* R) { return R->bStartHeadPending; }
	static void NewFrame(AVBoRScenarioRunner* R) { R->LiveFrame = MAX_uint64; }
	static const TArray<FVBoRFlowCommand>& Queue(AVBoRScenarioRunner* R) { return R->PendingCommands; }
	static void Clear(AVBoRScenarioRunner* R) { R->PendingCommands.Reset(); }
	static void Key(AVBoRScenarioRunner* R, FKey Key)
	{ for (auto& Binding : R->InputComponent->KeyBindings) { if (Binding.Chord.Key == Key) { Binding.KeyDelegate.Execute(Key); return; } } }
	static void Dispatch(AVBoRScenarioRunner* R, const FVBoRFlowCommand& C) { R->Dispatch(C); }
	static void Lock(AVBoRScenarioRunner* R) { R->ConfirmLockRemaining = 1; }
	static void Invalid(AVBoRScenarioRunner* R) { R->ValidationProblems = TEXT("invalid tags"); }
	static void SetWindow(AVBoRScenarioRunner* R, const TSharedRef<SWindow>& Window) { R->OperatorWindow = Window; }
	static void WindowClosed(AVBoRScenarioRunner* R, const TSharedRef<SWindow>& Window) { R->HandleOperatorWindowClosed(Window); }
	static void CloseWindow(AVBoRScenarioRunner* R) { R->CloseOperatorWindow(); }
	static void ResetScans(AVBoRScenarioRunner* R) { R->OperatorViewpointScans = 0; }
	static int32 Scans(AVBoRScenarioRunner* R) { return R->OperatorViewpointScans; }
	static bool SeatButtonsShown(AVBoRScenarioRunner* R) { return !R->ConfirmButton->IsHidden() && !R->ActionButton->IsHidden(); }
	static const FVBoROperatorSnapshot& Snapshot(const SVBoROperatorPanel& Panel) { return Panel.Snapshot; }
	static bool ButtonEnabled(const SVBoROperatorPanel& Panel, EVBoROperatorButton Button)
	{ for (const auto& Entry : Panel.Buttons) { if (Entry.Key == Button) { return Entry.Value->IsEnabled(); } } return false; }
};

namespace
{
	using B = EVBoROperatorButton;
	class FP5iHead : public FXRTrackingSystemBase
	{
	public:
		FP5iHead() : FXRTrackingSystemBase(nullptr) {}
		FVector2D Bounds = FVector2D(300,400);
		FTransform LocalHead;
		virtual FName GetSystemName() const override { return TEXT("P5iSyntheticHead"); }
		virtual int32 GetXRSystemFlags() const override { return 0; }
		virtual bool EnumerateTrackedDevices(TArray<int32>& Out, EXRTrackedDeviceType) override { Out.Add(0); return true; }
		virtual bool IsTracking(int32 Device) override { return Device == 0; }
		virtual bool GetCurrentPose(int32 Device, FQuat& Rotation, FVector& Position) override
		{ Rotation = LocalHead.GetRotation(); Position = LocalHead.GetLocation(); return IsTracking(Device); }
		virtual float GetWorldToMetersScale() const override { return 100; }
		virtual void ResetOrientationAndPosition(float) override {}
		virtual bool IsHeadTrackingAllowed() const override { return true; }
		virtual EHMDTrackingOrigin::Type GetTrackingOrigin() const override { return EHMDTrackingOrigin::Stage; }
		virtual FVector2D GetPlayAreaBounds(EHMDTrackingOrigin::Type) const override { return Bounds; }
	};
	struct FP5iWorld
	{
		UWorld* World = UWorld::CreateWorld(EWorldType::Game,false);
		APawn* Pawn;
		AP3VehicleMock* Car;
		UP5iStoredAlignment* Alignment;
		AP5gReloadObserver* Runner;
		FVBoRBuckSavedAlignment Saved;
		TSharedPtr<IXRTrackingSystem,ESPMode::ThreadSafe> OriginalXR = GEngine->XRSystem;
		TSharedPtr<FP5iHead,ESPMode::ThreadSafe> Head = MakeShared<FP5iHead,ESPMode::ThreadSafe>();
		IConsoleVariable* EyeAlignment = IConsoleManager::Get().FindConsoleVariable(TEXT("vBoR.EyeAlignment"));
		int32 OriginalEyeAlignment = EyeAlignment->GetInt();
		FString Path;
		FP5iWorld(bool bSave = true)
		{
			// Enable only tests using file version 1 (P5u §3 revision 2, Q1)
			EyeAlignment->Set(1, ECVF_SetByCode);
			GEngine->XRSystem = Head;
			GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
			auto* PC = World->SpawnActor<APlayerController>(); Pawn = World->SpawnActor<APawn>();
			Pawn->SetRootComponent(NewObject<USceneComponent>(Pawn)); Pawn->GetRootComponent()->RegisterComponent(); PC->Possess(Pawn);
			Car = World->SpawnActor<AP3VehicleMock>();
			Car->Alignment->DestroyComponent();
			Alignment = NewObject<UP5iStoredAlignment>(Car); Car->Alignment = Alignment;
			Alignment->BaseEye = Car->DriverEye;
			Path = FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("Automation"),FGuid::NewGuid().ToString()+TEXT("-P5i.json"));
			Alignment->TestPath = Path;
			Saved.SavedAt = TEXT("2026-10-16T00:00:00Z"); Saved.TrackingOrigin = static_cast<int32>(EHMDTrackingOrigin::Stage);
			Saved.PlayAreaCm = Head->Bounds; Saved.Capture.PositionCm = FVector(10,20,115); Saved.Capture.YawDeg = 30; Saved.Capture.UsedFrames = 20;
			if (bSave) { FString Reason; check(FVBoRBuckAlignmentStore::Save(Path,Saved,Reason)); }
			Alignment->RegisterComponent();
			Runner = World->SpawnActor<AP5gReloadObserver>(); Runner->Scenario = NewObject<UVBoRScenarioAsset>(Runner);
			Runner->Scenario->Steps.Reset();
			for (const TCHAR* Id : {TEXT("01"),TEXT("02"),TEXT("03")})
			{ FVBoRScenarioStep Step; Step.StepId = Id; Step.ConfirmBy = EVBoRConfirmBy::Either; Runner->Scenario->Steps.Add(Step); }
			World->InitializeActorsForPlay(FURL()); World->GetWorldSettings()->NotifyBeginPlay();
			Head->LocalHead = Car->DriverEye->GetComponentTransform().GetRelativeTransform(Pawn->GetActorTransform());
			Runner->InitializeFlow(); Ready();
		}
		~FP5iWorld()
		{
			World->EndPlay(EEndPlayReason::Quit); GEngine->DestroyWorldContext(World); World->DestroyWorld(false); GEngine->XRSystem = OriginalXR; IFileManager::Get().Delete(*Path);
			// Restore after world cleanup (no D6 disable detection during cleanup, P5u Q1)
			EyeAlignment->Set(OriginalEyeAlignment, ECVF_SetByCode);
		}
		void Ready() { Runner->ProcessFlow(0); Runner->ProcessFlow(1); }
		void Button(B Id) { FP5iTestAccess::NewFrame(Runner); Runner->PressOperatorButton(Id); Runner->ProcessFlow(0); }
		void Free() { Button(B::Free); Ready(); }
		void FinishSwitch() { Alignment->AdvanceSwitch(Alignment->FadeSeconds); Alignment->AdvanceSwitch(Alignment->FadeSeconds); Runner->ProcessFlow(0); }
		void NeedsCheck() { Alignment->State = EVBoRAlignmentState::NeedsCheck; Alignment->Reason = TEXT("suspected guardian change"); }
	};
	void Enabled(FAutomationTestBase& Test, FP5iWorld& W, std::initializer_list<B> Expected)
	{
		for (const auto& S : VBoROperator::Shortcuts())
		{
			bool bExpected = false; for (B Id : Expected) { bExpected |= Id == S.Button; }
			Test.TestEqual(S.Label,W.Runner->EvaluateOperatorButton(S.Button).bAccepted,bExpected);
		}
	}
	constexpr auto Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5iE1,"vBoR.P5i.E1NoAutomationWindow",Flags)
bool FP5iE1::RunTest(const FString&)
{
	FP5iWorld W; W.Runner->ShowOperatorWindow(); TestFalse(TEXT("Headless automation cannot create a window"),W.Runner->HasOperatorWindow());
	{ TGuardValue<bool> Testing(GIsAutomationTesting,true); W.Runner->ShowOperatorWindow(); TestFalse(TEXT("Rendering automation also cannot create a window"),W.Runner->HasOperatorWindow()); }
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5iE2,"vBoR.P5i.E2ButtonsEqualGameKeys",Flags)
bool FP5iE2::RunTest(const FString&)
{
	FP5iWorld W;
	for (const auto& S : VBoROperator::Shortcuts())
	{
		FP5iTestAccess::Clear(W.Runner); FP5iTestAccess::NewFrame(W.Runner); W.Runner->PressOperatorButton(S.Button);
		if (S.Button == B::Info) { TestEqual(TEXT("Info is not a flow command"),FP5iTestAccess::Queue(W.Runner).Num(),0); FP5iTestAccess::Key(W.Runner,S.Key); continue; }
		const auto ButtonCommand = FP5iTestAccess::Queue(W.Runner)[0]; FP5iTestAccess::Clear(W.Runner);
		FP5iTestAccess::Key(W.Runner,S.Key);
		const auto& KeyQueue = FP5iTestAccess::Queue(W.Runner);
		if (!TestEqual(S.Label,KeyQueue.Num(),1)) { return false; }
		TestTrue(TEXT("Same action and operator source"),KeyQueue[0].Action == ButtonCommand.Action && KeyQueue[0].Source == EVBoRCommandSource::Operator);
		TestTrue(TEXT("Same live token and lock"),KeyQueue[0].Token.StepIndex == ButtonCommand.Token.StepIndex && KeyQueue[0].Token.Generation == ButtonCommand.Token.Generation && KeyQueue[0].bConfirmLocked == ButtonCommand.bConfirmLocked);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5iE3,"vBoR.P5i.E3BeforeStart",Flags)
bool FP5iE3::RunTest(const FString&)
{ FP5iWorld W(false); Enabled(*this,W,{B::Ignore,B::Reapply,B::Info}); W.NeedsCheck(); Enabled(*this,W,{B::Ignore,B::Reapply,B::Info}); return true; }

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5iE4,"vBoR.P5i.E4ConfirmRulePermissionAndLock",Flags)
bool FP5iE4::RunTest(const FString&)
{
	FP5iWorld W;
	TestTrue(TEXT("Ready confirmation enabled"),W.Runner->EvaluateOperatorButton(B::Confirm).bAccepted);
	FP5iTestAccess::Lock(W.Runner); TestFalse(TEXT("Locked confirmation disabled"),W.Runner->EvaluateOperatorButton(B::Confirm).bAccepted); W.Ready();
	W.Runner->Scenario->Steps[0].ConfirmBy = EVBoRConfirmBy::Participant;
	TestFalse(TEXT("Participant step disabled for operator"),W.Runner->EvaluateOperatorButton(B::Confirm).bAccepted);
	W.Runner->Scenario->Steps[0].ConfirmBy = EVBoRConfirmBy::Operator;
	W.Runner->Scenario->Steps[0].bRequireBaseViewpoint = true;
	W.Button(B::Passenger); W.FinishSwitch(); W.Ready();
	TestFalse(TEXT("Unsatisfied base rule disabled"),W.Runner->EvaluateOperatorButton(B::Confirm).bAccepted);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5iE5,"vBoR.P5i.E5DuringSeatFade",Flags)
bool FP5iE5::RunTest(const FString&) { FP5iWorld W; W.Button(B::Passenger); Enabled(*this,W,{B::Info}); return true; }

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5iE6,"vBoR.P5i.E6FreeMode",Flags)
bool FP5iE6::RunTest(const FString&) { FP5iWorld W; W.Free(); Enabled(*this,W,{B::Driver,B::Passenger,B::Rear,B::Restart,B::Reload,B::Reapply,B::Info,B::FoldSeat}); return true; }

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5iE7,"vBoR.P5i.E7DecisionAndDispatch",Flags)
bool FP5iE7::RunTest(const FString&)
{
	for (int32 State = 0; State < 7; ++State)
	{
		for (const auto& S : VBoROperator::Shortcuts())
		{
			FP5iWorld W(State != 0);
			if (State == 1) { W.Free(); }
			if (State == 2) { W.Button(B::Passenger); }
			if (State == 3) { W.Free(); W.Button(B::Reload); }
			if (State == 4) { W.NeedsCheck(); }
			if (State == 5) { FP5iTestAccess::Lock(W.Runner); }
			if (State == 6) { FP5iTestAccess::Invalid(W.Runner); }
			const auto Decision = W.Runner->EvaluateOperatorButton(S.Button);
			W.Button(S.Button);
			TestEqual(*FString::Printf(TEXT("%s state %d"),S.Label,State),W.Runner->GetOperatorStatus().LastCommand.Contains(TEXT(" — accepted")),Decision.bAccepted);
			if (!Decision.bAccepted) { TestTrue(TEXT("Discard reason displayed"),W.Runner->GetOperatorStatus().LastCommand.Contains(Decision.Reason)); }
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5iE8,"vBoR.P5i.E8EnglishStatus",Flags)
bool FP5iE8::RunTest(const FString&)
{
	FP5iWorld W(false); auto S = W.Runner->GetOperatorStatus();
	TestEqual(TEXT("Ready"),S.Title,FString(TEXT("Getting ready"))); TestEqual(TEXT("Waiting"),S.Waiting,FString(TEXT("Alignment check")));
	TestEqual(TEXT("Missing saved value"),S.Alignment,FString(TEXT("No saved alignment")));
	W.Button(B::Ignore); W.Ready(); S = W.Runner->GetOperatorStatus();
	TestTrue(TEXT("English mission and progress"),S.Title.StartsWith(TEXT("Mission 1 of 3")) && !S.Mission.IsEmpty() && S.Progress == TEXT("0 of 3 complete"));
	TestTrue(TEXT("Ignored flag"),S.Alignment.Contains(TEXT("started without alignment")));
	W.Button(B::Passenger); TestTrue(TEXT("Rejected command reason"),W.Runner->GetOperatorStatus().LastCommand.Contains(TEXT("discarded (Saved alignment required)")));
	W.Free(); S = W.Runner->GetOperatorStatus();
	TestEqual(TEXT("Free wait"),S.Waiting,FString(TEXT("Nothing — participant explores"))); TestTrue(TEXT("End origin"),S.Mission.Contains(TEXT("End")));
	TestTrue(TEXT("All counters"),S.Counts.Contains(TEXT("Re-apply")) && S.Discards.Contains(TEXT("transition")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5iE9,"vBoR.P5i.E9ButtonAndKeySameFrame",Flags)
bool FP5iE9::RunTest(const FString&)
{
	FP5iWorld W; FP5iTestAccess::NewFrame(W.Runner); W.Runner->PressOperatorButton(B::Confirm); FP5iTestAccess::Key(W.Runner,EKeys::SpaceBar);
	W.Runner->ProcessFlow(0); TestEqual(TEXT("One step"),W.Runner->GetStepIndex(),1); TestEqual(TEXT("Second input stale"),W.Runner->StaleDiscards,1); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5iE10,"vBoR.P5i.E10FocusedWindowKey",Flags)
bool FP5iE10::RunTest(const FString&)
{
	FP5iWorld W; const auto Widget = SNew(SVBoROperatorPanel).Runner(W.Runner);
	const FKeyEvent Key(EKeys::SpaceBar,FModifierKeysState(),0,false,0,0);
	TestTrue(TEXT("Preview consumes Space before focused button"),Widget->OnPreviewKeyDown(FGeometry(),Key).IsEventHandled());
	TestEqual(TEXT("Exactly one command"),FP5iTestAccess::Queue(W.Runner).Num(),1);
	const FKeyEvent Repeat(EKeys::SpaceBar,FModifierKeysState(),0,true,0,0); Widget->OnPreviewKeyDown(FGeometry(),Repeat);
	TestEqual(TEXT("Repeat consumed without queuing"),FP5iTestAccess::Queue(W.Runner).Num(),1);
	W.Runner->ProcessFlow(0); TestEqual(TEXT("One advance"),W.Runner->GetStepIndex(),1); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5iE11,"vBoR.P5i.E11PlacementAcrossTravel",Flags)
bool FP5iE11::RunTest(const FString&)
{
	const auto Original = FVBoROperatorPlacement::Session();
	FVBoROperatorPlacement::Session().Remember(FVector2D(1300,40),FVector2D(720,900));
	{ FP5iWorld W; W.Free(); W.Button(B::Reload); W.Runner->ProcessFlow(1); TestEqual(TEXT("Travel request"),W.Runner->ReloadRequests,1); }
	{ FP5iWorld W; TestTrue(TEXT("Placement persists"),FVBoROperatorPlacement::Session().bRemembered); TestTrue(TEXT("Position and client size preserved"),FVBoROperatorPlacement::Session().Position == FVector2D(1300,40) && FVBoROperatorPlacement::Session().ClientSize == FVector2D(720,900)); }
	FVBoROperatorPlacement::Session() = Original; return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5iE13,"vBoR.P5i.E13IgnoredAlignmentSeats",Flags)
bool FP5iE13::RunTest(const FString&)
{
	FP5iWorld W(false); W.Button(B::Ignore); W.Ready();
	for (B Id : {B::Driver,B::Passenger,B::Rear}) { TestFalse(TEXT("Ignored alignment seat disabled"),W.Runner->EvaluateOperatorButton(Id).bAccepted); W.Button(Id); }
	TestEqual(TEXT("Runner transition discards"),W.Runner->TransitionDiscards,3); TestEqual(TEXT("Component discards retained"),W.Alignment->SwitchDiscards,3); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5iE14,"vBoR.P5i.E14ReloadDisablesEverything",Flags)
bool FP5iE14::RunTest(const FString&) { FP5iWorld W; W.Free(); W.Button(B::Reload); Enabled(*this,W,{}); return true; }

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5iE15,"vBoR.P5i.E15ReapplyRecoversWithoutChangingStep",Flags)
bool FP5iE15::RunTest(const FString&)
{
	FP5iWorld W; W.Button(B::Next); W.NeedsCheck(); const auto Token = W.Runner->GetToken(); int32 Moves = 0;
	W.Alignment->OnPawnMoved.AddLambda([&]() { ++Moves; }); W.Pawn->AddActorWorldOffset(FVector(50,0,0));
	W.Button(B::Reapply); TestTrue(TEXT("Fade started"),W.Alignment->IsSwitching());
	W.Alignment->AdvanceSwitch(W.Alignment->FadeSeconds * 0.5); TestEqual(TEXT("No move before full black"),Moves,0);
	W.Alignment->AdvanceSwitch(W.Alignment->FadeSeconds * 0.5); TestEqual(TEXT("Moved once at black"),Moves,1);
	W.Alignment->AdvanceSwitch(W.Alignment->FadeSeconds); W.Runner->ProcessFlow(0);
	TestTrue(TEXT("Applied and fade finished"),W.Alignment->State == EVBoRAlignmentState::Applied && !W.Alignment->IsSwitching());
	TestTrue(TEXT("Same step and generation"),Token.StepIndex == W.Runner->GetToken().StepIndex && Token.Generation == W.Runner->GetToken().Generation);
	TestEqual(TEXT("One reapply"),W.Runner->ReapplyCount,1); TestTrue(TEXT("Applied result"),W.Runner->GetOperatorStatus().LastCommand.EndsWith(TEXT("accepted (Applied)")));
	W.Alignment->OnPawnMoved.Clear(); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5iE16,"vBoR.P5i.E16ReapplyStartsWaitingFlow",Flags)
bool FP5iE16::RunTest(const FString&)
{
	FP5iWorld W(false); W.NeedsCheck(); FString Reason; TestTrue(TEXT("Setup saved"),FVBoRBuckAlignmentStore::Save(W.Path,W.Saved,Reason));
	W.Button(B::Reapply); W.FinishSwitch(); TestTrue(TEXT("Flow started"),W.Runner->IsRunning()); TestEqual(TEXT("First step"),W.Runner->GetStepIndex(),0); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP4cReapplyStart,"vBoR.P4c.E8ReapplyStartAttempt",Flags)
bool FP4cReapplyStart::RunTest(const FString&)
{
	struct FStartRecords : FOutputDevice
	{
		int32 Starts = 0, Fallbacks = 0;
		FStartRecords() { GLog->FlushThreadedLogs(); GLog->AddOutputDevice(this); Starts = 0; Fallbacks = 0; }
		~FStartRecords() { GLog->FlushThreadedLogs(); GLog->RemoveOutputDevice(this); }
		virtual void Serialize(const TCHAR* Text, ELogVerbosity::Type, const FName& Category) override
		{
			if (Category != TEXT("LogP3Flow")) { return; }
			const FString Line(Text);
			Starts += Line.Contains(TEXT("Start position |")) ? 1 : 0;
			Fallbacks += Line.Contains(TEXT("start marker not visited")) ? 1 : 0;
		}
	};
	for (bool bVisitStart : {true,false})
	{
		FP5iWorld W(false); W.NeedsCheck();
		FString Reason; TestTrue(TEXT("E8 A setup saved"),FVBoRBuckAlignmentStore::Save(W.Path,W.Saved,Reason));
		TestFalse(TEXT("E8 A flow waits for alignment"),W.Runner->IsRunning());
		TestFalse(TEXT("E8 A no attempt before start"),FP5iTestAccess::StartPending(W.Runner));
		FStartRecords Records;
		W.Button(B::Reapply); W.FinishSwitch();
		TestTrue(TEXT("E8 A starts flow"),W.Runner->IsRunning());
		TestEqual(TEXT("E8 A starts 01"),W.Runner->GetStepIndex(),0);
		TestTrue(TEXT("E8 A arms start record"),FP5iTestAccess::StartPending(W.Runner));
		if (bVisitStart)
		{
			// Synthetic head over start marker passes recording gates (P4c R1-1, cm)
			const FTransform Start(FRotator::ZeroRotator,W.Car->GetActorTransform().TransformPosition(FVector(485,0,160.5)));
			W.Head->LocalHead = Start.GetRelativeTransform(W.Pawn->GetActorTransform());
			W.Runner->ProcessFlow(0); W.Runner->ProcessFlow(0);
			TestFalse(TEXT("E8 A records start once"),FP5iTestAccess::StartPending(W.Runner));
		}
		W.Button(B::Next); W.Button(B::Next); GLog->FlushThreadedLogs();
		TestEqual(TEXT("E8 A start log count"),Records.Starts,bVisitStart ? 1 : 0);
		TestEqual(TEXT("E8 A fallback log count"),Records.Fallbacks,bVisitStart ? 0 : 1);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5iE17,"vBoR.P5i.E17ReapplyFailureUnfades",Flags)
bool FP5iE17::RunTest(const FString&)
{
	{ FP5iWorld W; W.Head->Bounds.X += 10; W.NeedsCheck(); W.Button(B::Reapply); W.FinishSwitch();
		TestTrue(TEXT("Mismatch remains needs check"),W.Alignment->State == EVBoRAlignmentState::NeedsCheck); TestFalse(TEXT("Fade finished"),W.Alignment->IsSwitching());
		TestTrue(TEXT("Reason result"),W.Runner->GetOperatorStatus().LastCommand.Contains(TEXT("still needs check: suspected guardian change"))); }
	{ FP5iWorld W(false); W.Button(B::Reapply); W.FinishSwitch();
		TestTrue(TEXT("Still no save"),W.Alignment->State == EVBoRAlignmentState::NoSavedValue); TestFalse(TEXT("Missing save unfades"),W.Alignment->IsSwitching());
		TestTrue(TEXT("Missing result"),W.Runner->GetOperatorStatus().LastCommand.Contains(TEXT("accepted (no saved alignment)"))); }
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5iE18,"vBoR.P5i.E18ReapplyRefusedAwayOrDuringFade",Flags)
bool FP5iE18::RunTest(const FString&)
{
	for (B Seat : {B::Passenger,B::Rear})
	{ FP5iWorld W; W.Button(Seat); TestFalse(TEXT("Seat fade"),W.Runner->EvaluateOperatorButton(B::Reapply).bAccepted); W.FinishSwitch();
		TestEqual(TEXT("Return reason"),W.Runner->EvaluateOperatorButton(B::Reapply).Reason,FString(TEXT("Return to the driver seat first")));
		W.Button(B::Reapply); TestEqual(TEXT("No accepted reapply"),W.Runner->ReapplyCount,0); TestEqual(TEXT("Transition discard"),W.Runner->TransitionDiscards,1); }
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5iE19,"vBoR.P5i.E19ReapplyUsesSetupNotHead",Flags)
bool FP5iE19::RunTest(const FString&)
{
	FP5iWorld W; W.Head->LocalHead.AddToTranslation(FVector(30,0,0));
	// Save different value from memory (verify actual reread too, P5i D8, cm)
	W.Saved.Capture.PositionCm.X += 12; FString Reason; TestTrue(TEXT("Changed setup saved"),FVBoRBuckAlignmentStore::Save(W.Path,W.Saved,Reason));
	W.Button(B::Reapply); W.FinishSwitch();
	TestTrue(TEXT("Pawn comes only from saved setup and eye"),W.Pawn->GetActorTransform().Equals(FVBoRBuckAlignment::CalculatePawn(W.Saved.Capture,W.Car->DriverEye->GetComponentTransform()),0.001));
	W.Free(); const auto Token = W.Runner->GetToken(); W.Button(B::Reapply); W.FinishSwitch();
	TestTrue(TEXT("Free mode and generation unchanged"),W.Runner->IsFreeMode() && W.Runner->GetToken().Generation == Token.Generation); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5iE20,"vBoR.P5i.E20OtherCommandsDuringReapply",Flags)
bool FP5iE20::RunTest(const FString&)
{
	FP5iWorld W; W.Button(B::Reapply);
	for (B Id : {B::Passenger,B::Confirm,B::Next,B::Ignore,B::Reapply}) { W.Button(Id); }
	TestEqual(TEXT("Transition discards"),W.Runner->TransitionDiscards,4); TestEqual(TEXT("Same step"),W.Runner->GetStepIndex(),0); W.FinishSwitch(); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5iE21,"vBoR.P5i.E21ButtonStyles",Flags)
bool FP5iE21::RunTest(const FString&)
{
	FP5iWorld W;
	TestTrue(TEXT("Confirm primary"),W.Runner->GetOperatorStyle(B::Confirm) == EVBoROperatorStyle::Primary);
	TestTrue(TEXT("Current seat"),W.Runner->GetOperatorStyle(B::Driver) == EVBoROperatorStyle::Current);
	TestTrue(TEXT("Reapply secondary"),W.Runner->GetOperatorStyle(B::Reapply) == EVBoROperatorStyle::Secondary);
	TestTrue(TEXT("Reload disabled"),W.Runner->GetOperatorStyle(B::Reload) == EVBoROperatorStyle::Disabled);
	W.NeedsCheck(); int32 Primaries = 0;
	for (const auto& S : VBoROperator::Shortcuts()) { Primaries += W.Runner->GetOperatorStyle(S.Button) == EVBoROperatorStyle::Primary ? 1 : 0; }
	TestEqual(TEXT("Only one primary"),Primaries,1); TestTrue(TEXT("Needs check reapply primary"),W.Runner->GetOperatorStyle(B::Reapply) == EVBoROperatorStyle::Primary);
	W.Free(); W.Alignment->State = EVBoRAlignmentState::Applied;
	TestTrue(TEXT("Reload danger"),W.Runner->GetOperatorStyle(B::Reload) == EVBoROperatorStyle::Danger); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5iE22,"vBoR.P5i.E22SrgbPalette",Flags)
bool FP5iE22::RunTest(const FString&)
{
	for (const TCHAR* Hex : {TEXT("0b0f17"),TEXT("131a26"),TEXT("1c2534"),TEXT("2e3a4e"),TEXT("66748a"),TEXT("eef2f7"),TEXT("9aa6b8"),TEXT("5b8cff"),TEXT("0d2a66"),TEXT("ffffff"),TEXT("3cc285"),TEXT("f2b544"),TEXT("ff7a72")})
	{ TestTrue(Hex,VBoROperator::Color(Hex).Equals(FLinearColor::FromSRGBColor(FColor::FromHex(Hex)),0.00001f)); }
	const auto Primary = VBoROperator::ButtonStyle(EVBoROperatorStyle::Primary);
	TestTrue(TEXT("Actual primary brush uses palette"),Primary.Normal.TintColor.GetSpecifiedColor().Equals(VBoROperator::Color(TEXT("5b8cff")))); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5iE23,"vBoR.P5i.E23SlateTextFits",Flags)
bool FP5iE23::RunTest(const FString&)
{
	FP5iWorld W; const auto Widget = SNew(SVBoROperatorPanel).Runner(W.Runner); Widget->SlatePrepass(1);
	const auto Measure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
	for (const auto& S : VBoROperator::Shortcuts())
	{
		const float Width = S.Button == B::Confirm ? 640 : S.Button == B::Driver || S.Button == B::Passenger || S.Button == B::Rear || S.Button == B::Reapply || S.Button == B::Info ? 288 : 316;
		const float BadgeWidth = S.Button == B::Driver ? Measure->Measure(TEXT("Current"),FCoreStyle::GetDefaultFontStyle("Regular",9)).X : S.Button == B::Info ? Measure->Measure(TEXT("Off"),FCoreStyle::GetDefaultFontStyle("Regular",9)).X : 0;
		const float LabelWidth = Measure->Measure(S.Label,FCoreStyle::GetDefaultFontStyle("Regular",10.5f)).X;
		TestTrue(S.Label,LabelWidth + BadgeWidth + 8 + 28 + (S.Button == B::Confirm ? 44 : 28) <= Width);
	}
	// Check real widget ellipsizes long reason to one line (P5i E23)
	W.Alignment->State = EVBoRAlignmentState::NeedsCheck; W.Alignment->Reason = FString::ChrN(400,TEXT('W'));
	Widget->Tick(FGeometry(),0,0);
	Widget->SlatePrepass(1); int32 EllipsisCount = 0;
	TFunction<void(TSharedRef<SWidget>)> Visit = [&](TSharedRef<SWidget> Node)
	{
		if (Node->GetTypeAsString() == TEXT("STextBlock"))
		{ const auto Text = StaticCastSharedRef<STextBlock>(Node); if (Text->GetText().ToString().Contains(TEXT("Needs check —"))) { TestTrue(TEXT("Long reason retains one line height"),Text->GetDesiredSize().Y <= Measure->GetMaxCharacterHeight(Text->GetFont()) + 2); ++EllipsisCount; } }
		FChildren* Children = Node->GetChildren(); for (int32 I=0; I<Children->Num(); ++I) { Visit(Children->GetChildAt(I)); }
	};
	Visit(Widget); TestEqual(TEXT("Actual alignment text found"),EllipsisCount,1); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5iE24,"vBoR.P5i.E24AlphabetShortcuts",Flags)
bool FP5iE24::RunTest(const FString&)
{
	FP5iWorld W;
	for (FKey Key : {EKeys::R,EKeys::I,EKeys::A})
	{ bool bFound = false; for (const auto& Binding : W.Runner->InputComponent->KeyBindings) { bFound |= Binding.Chord.Key == Key; } TestTrue(TEXT("Alphabet key bound"),bFound); }
	for (FKey Key : {EKeys::F1,EKeys::F5})
	{ for (const auto& Binding : W.Runner->InputComponent->KeyBindings) { TestFalse(TEXT("Old key absent"),Binding.Chord.Key == Key); } }
	W.Free(); TestTrue(TEXT("R restart shown"),W.Runner->GetOperatorText().Contains(TEXT("R Restart"))); TestFalse(TEXT("F5 absent"),W.Runner->GetOperatorText().Contains(TEXT("F5"))); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5iR1Close,"vBoR.P5i.R1ExternalWindowClose",Flags)
bool FP5iR1Close::RunTest(const FString&)
{
	TGuardValue<FVBoROperatorPlacement> Placement(FVBoROperatorPlacement::Session(),FVBoROperatorPlacement{});
	FP5iWorld W;
	const auto Window = SNew(SWindow).ScreenPosition(FVector2D(1300,40)).ClientSize(FVector2D(720,900));
	const FVector2D Position = Window->GetPositionInScreen(), Size = Window->GetClientSizeInScreen();
	FP5iTestAccess::SetWindow(W.Runner,Window);
	FP5iTestAccess::WindowClosed(W.Runner,Window);
	TestFalse(TEXT("External close clears owned window"),W.Runner->HasOperatorWindow());
	TestTrue(TEXT("External close remembers position and client size"),FVBoROperatorPlacement::Session().bRemembered && FVBoROperatorPlacement::Session().Position == Position && FVBoROperatorPlacement::Session().ClientSize == Size);
	FP5iTestAccess::CloseWindow(W.Runner);
	TestTrue(TEXT("EndPlay close after external close preserves placement"),FVBoROperatorPlacement::Session().Position == Position && FVBoROperatorPlacement::Session().ClientSize == Size);
	const auto Replacement = SNew(SWindow).ScreenPosition(FVector2D(1600,80)).ClientSize(FVector2D(680,850));
	FP5iTestAccess::SetWindow(W.Runner,Replacement);
	FP5iTestAccess::WindowClosed(W.Runner,Window);
	TestTrue(TEXT("Late old close leaves replacement alive"),W.Runner->HasOperatorWindow());
	TestTrue(TEXT("Late old close preserves remembered placement"),FVBoROperatorPlacement::Session().Position == Position && FVBoROperatorPlacement::Session().ClientSize == Size);
	FP5iTestAccess::WindowClosed(W.Runner,Replacement);
	TestFalse(TEXT("Replacement can close independently"),W.Runner->HasOperatorWindow());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5iR1Ignore,"vBoR.P5i.R1IgnoreDuringTransitions",Flags)
bool FP5iR1Ignore::RunTest(const FString&)
{
	{
		FP5iWorld W; W.Button(B::Passenger);
		const int32 Transitions = W.Runner->TransitionDiscards, Rules = W.Runner->RuleDiscards;
		const auto Token = W.Runner->GetToken(); const FString Record = W.Runner->LastRecord;
		W.Button(B::Ignore);
		TestEqual(TEXT("Running O does not count transition discard"),W.Runner->TransitionDiscards,Transitions);
		TestEqual(TEXT("Running O does not count rule discard"),W.Runner->RuleDiscards,Rules);
		TestEqual(TEXT("Running O preserves record"),W.Runner->LastRecord,Record);
		TestTrue(TEXT("Running O preserves step and generation"),W.Runner->GetToken().StepIndex == Token.StepIndex && W.Runner->GetToken().Generation == Token.Generation);
	}
	{
		FP5iWorld W; W.Free(); W.Button(B::Passenger);
		const int32 Transitions = W.Runner->TransitionDiscards, Rules = W.Runner->RuleDiscards;
		W.Button(B::Ignore);
		TestEqual(TEXT("Free O is a rule discard"),W.Runner->RuleDiscards,Rules+1);
		TestEqual(TEXT("Free O does not count transition discard"),W.Runner->TransitionDiscards,Transitions);
	}
	{
		FP5iWorld W(false); FString Reason; TestTrue(TEXT("Setup saved for reapply"),FVBoRBuckAlignmentStore::Save(W.Path,W.Saved,Reason));
		W.Button(B::Reapply);
		TestFalse(TEXT("Waiting O disabled during reapply"),W.Runner->EvaluateOperatorButton(B::Ignore).bAccepted);
		W.Button(B::Ignore);
		TestEqual(TEXT("Waiting O counts transition discard"),W.Runner->TransitionDiscards,1);
		TestFalse(TEXT("Waiting O does not start flow"),W.Runner->IsRunning());
		W.FinishSwitch(); TestTrue(TEXT("Applied alignment starts flow"),W.Runner->IsRunning());
		TestEqual(TEXT("Reapply starts first step"),W.Runner->GetStepIndex(),0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5iR1Snapshot,"vBoR.P5i.R1OneSnapshotPerFrame",Flags)
bool FP5iR1Snapshot::RunTest(const FString&)
{
	for (int32 State = 0; State < 8; ++State)
	{
		FP5iWorld W(State != 0);
		const auto Widget = SNew(SVBoROperatorPanel).Runner(W.Runner);
		if (State == 1) { W.Free(); }
		if (State == 2) { W.Button(B::Passenger); }
		if (State == 3) { W.Free(); W.Button(B::Reload); }
		if (State == 4) { W.NeedsCheck(); }
		if (State == 5) { FP5iTestAccess::Lock(W.Runner); }
		if (State == 6) { FP5iTestAccess::Invalid(W.Runner); }
		if (State == 7) { W.Button(B::Passenger); W.FinishSwitch(); W.Ready(); }
		FP5iTestAccess::ResetScans(W.Runner);
		Widget->Tick(FGeometry(),0,0); Widget->SlatePrepass(1);
		const auto& Snapshot = FP5iTestAccess::Snapshot(*Widget);
		for (const auto& Shortcut : VBoROperator::Shortcuts())
		{ TestEqual(TEXT("Actual button reads snapshot enabled state"),FP5iTestAccess::ButtonEnabled(*Widget,Shortcut.Button),Snapshot.Buttons[static_cast<int32>(Shortcut.Button)].Decision.bAccepted); }
		TestEqual(TEXT("Tick and prepass use seats found at start (no world scan, P5o)"),FP5iTestAccess::Scans(W.Runner),0);
		for (const auto& Shortcut : VBoROperator::Shortcuts())
		{
			const auto& Button = Snapshot.Buttons[static_cast<int32>(Shortcut.Button)];
			const auto Live = W.Runner->EvaluateOperatorButton(Shortcut.Button);
			TestEqual(Shortcut.Label,Button.Decision.bAccepted,Live.bAccepted);
			TestTrue(TEXT("Snapshot discard category matches live decision"),Button.Decision.Discard == Live.Discard);
			TestEqual(TEXT("Snapshot reason matches live decision"),Button.Decision.Reason,Live.Reason);
			TestEqual(TEXT("Snapshot record matches live decision"),Button.Decision.Record,Live.Record);
			TestTrue(TEXT("Snapshot style matches live style"),Button.Style == W.Runner->GetOperatorStyle(Shortcut.Button));
			TestEqual(TEXT("Snapshot badge matches live badge"),Button.Badge,W.Runner->OperatorButtonBadge(Shortcut.Button));
		}
		const auto Status = W.Runner->GetOperatorStatus();
		TestTrue(TEXT("Snapshot refreshes status after state change"),Snapshot.Status.Title == Status.Title && Snapshot.Status.Alignment == Status.Alignment && Snapshot.Status.Seat == Status.Seat && Snapshot.Status.LastCommand == Status.LastCommand);
	}
	return true;
}
namespace
{
	struct FP3cEnabled
	{
		IConsoleVariable* Variable = IConsoleManager::Get().FindConsoleVariable(TEXT("vBoR.HomeShift"));
		FP3cEnabled() { check(Variable); Variable->Set(1, ECVF_SetByConsole); }
		~FP3cEnabled() { Variable->Set(0, ECVF_SetByConsole); }
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP3cE1,"vBoR.P3c.E1DisabledKeys",Flags)
bool FP3cE1::RunTest(const FString&)
{
	FP3cEnabled Setting; Setting.Variable->Set(0, ECVF_SetByConsole);
	TestEqual(TEXT("Default is off"),Setting.Variable->GetDefaultValue(),FString(TEXT("0")));
	FP5iWorld W; const FTransform Before = W.Pawn->GetActorTransform(); const FString Record = W.Runner->LastRecord;
	int32 Moves = 0; const auto Handle = W.Alignment->OnPawnMoved.AddLambda([&]() { ++Moves; });
	for (const FKey Key : {EKeys::NumPadEight,EKeys::NumPadTwo,EKeys::NumPadFour,EKeys::NumPadSix,EKeys::NumPadFive})
	{
		TestFalse(TEXT("Disabled focused key is unhandled"),W.Runner->HandleOperatorKey(Key));
		FP5iTestAccess::Key(W.Runner,Key);
	}
	TestTrue(TEXT("Pawn unchanged"),W.Pawn->GetActorTransform().Equals(Before,0.001));
	TestTrue(TEXT("Offset stays zero"),W.Alignment->GetHomeShiftCm().IsZero());
	TestEqual(TEXT("No record"),W.Runner->LastRecord,Record); TestEqual(TEXT("No move notification"),Moves,0);
	W.Alignment->OnPawnMoved.Remove(Handle); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP3cE2,"vBoR.P3c.E2CarDirectionAndFade",Flags)
bool FP3cE2::RunTest(const FString&)
{
	FP3cEnabled Setting; FP5iWorld W;
	W.Car->SetActorRotation(FRotator(0,30,0)); W.Alignment->InitializeAlignment(W.Pawn,&W.Saved,W.Saved.TrackingOrigin,W.Saved.PlayAreaCm);
	const FTransform Before = W.Pawn->GetActorTransform();
	int32 Moves = 0; const auto Handle = W.Alignment->OnPawnMoved.AddLambda([&]() { ++Moves; });
	FP5iTestAccess::Key(W.Runner,EKeys::NumPadEight);
	TestTrue(TEXT("Car forward 100 cm, independent geometry"),W.Pawn->GetActorLocation().Equals(Before.GetLocation()+FVector(86.6025403784,50,0),0.001));
	TestTrue(TEXT("Rotation unchanged"),W.Pawn->GetActorQuat().Equals(Before.GetRotation(),0.00001));
	TestEqual(TEXT("One move notification"),Moves,1);
	const auto* Camera = Cast<APlayerController>(W.Pawn->GetController())->PlayerCameraManager.Get();
	TestTrue(TEXT("Fade from black to clear"),Camera->FadeAlpha == FVector2D(1,0));
	TestEqual(TEXT("Fade duration"),Camera->FadeTime,W.Alignment->FadeSeconds);
	TestFalse(TEXT("Home shift does not start a seat transition"),W.Alignment->IsSwitching());
	TestTrue(TEXT("Movement recorded"),W.Runner->LastRecord.Contains(TEXT("Home shift +X 100 cm")));
	W.Alignment->OnPawnMoved.Remove(Handle); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP3cE3,"vBoR.P3c.E3AccumulateAndReset",Flags)
bool FP3cE3::RunTest(const FString&)
{
	FP3cEnabled Setting; FP5iWorld W; W.Car->SetActorRotation(FRotator(0,30,0));
	const FTransform Before = W.Pawn->GetActorTransform();
	for (const FKey Key : {EKeys::NumPadEight,EKeys::NumPadEight,EKeys::NumPadSix}) { FP5iTestAccess::Key(W.Runner,Key); }
	TestTrue(TEXT("Car offset (200,100) cm"),W.Alignment->GetHomeShiftCm() == FVector2D(200,100));
	TestTrue(TEXT("Forward plus right in world space"),W.Pawn->GetActorLocation().Equals(Before.GetLocation()+FVector(123.2050807569,186.6025403784,0),0.001));
	FP5iTestAccess::Key(W.Runner,EKeys::NumPadFive);
	TestTrue(TEXT("Original pose within 0.001 cm"),W.Pawn->GetActorTransform().Equals(Before,0.001));
	TestTrue(TEXT("Offset zero"),W.Alignment->GetHomeShiftCm().IsZero());
	const FString Record = W.Runner->LastRecord; FP5iTestAccess::Key(W.Runner,EKeys::NumPadFive);
	TestEqual(TEXT("Already home is a no-op"),W.Runner->LastRecord,Record);
	FP5iTestAccess::Key(W.Runner,EKeys::NumPadTwo);
	TestTrue(TEXT("Back key"),W.Alignment->GetHomeShiftCm() == FVector2D(-100,0));
	FP5iTestAccess::Key(W.Runner,EKeys::NumPadFive); W.Alignment->HomeShiftStepCm = 75;
	FP5iTestAccess::Key(W.Runner,EKeys::NumPadSix);
	TestTrue(TEXT("Editable step in cm"),W.Alignment->GetHomeShiftCm() == FVector2D(0,75)); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP3cE4,"vBoR.P3c.E4FocusedWindowRepeat",Flags)
bool FP3cE4::RunTest(const FString&)
{
	FP3cEnabled Setting; FP5iWorld W; const auto Widget = SNew(SVBoROperatorPanel).Runner(W.Runner);
	const FVector Before = W.Pawn->GetActorLocation();
	const FKeyEvent Key(EKeys::NumPadFour,FModifierKeysState(),0,false,0,0);
	TestTrue(TEXT("Focused keypad key handled"),Widget->OnPreviewKeyDown(FGeometry(),Key).IsEventHandled());
	const FString Record = W.Runner->LastRecord;
	const FKeyEvent Repeat(EKeys::NumPadFour,FModifierKeysState(),0,true,0,0);
	TestTrue(TEXT("Repeat handled"),Widget->OnPreviewKeyDown(FGeometry(),Repeat).IsEventHandled());
	TestTrue(TEXT("Moves once to driver side"),W.Pawn->GetActorLocation().Equals(Before+FVector(0,-100,0),0.001));
	TestEqual(TEXT("Repeat adds no record"),W.Runner->LastRecord,Record);
	TestFalse(TEXT("Unrelated key unhandled"),W.Runner->HandleOperatorKey(EKeys::NumPadNine)); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP3cE5,"vBoR.P3c.E5TransitionRejection",Flags)
bool FP3cE5::RunTest(const FString&)
{
	FP3cEnabled Setting;
	for (B Button : {B::Passenger,B::Reapply,B::Reload})
	{
		FP5iWorld W; if (Button == B::Reload) { W.Free(); }
		FP5iTestAccess::Key(W.Runner,EKeys::NumPadEight);
		W.Button(Button); const FTransform Before = W.Pawn->GetActorTransform(); const FVector2D Offset = W.Alignment->GetHomeShiftCm();
		for (const FKey Key : {EKeys::NumPadEight,EKeys::NumPadFive})
		{
			TestTrue(TEXT("Focused blocked key handled"),W.Runner->HandleOperatorKey(Key));
			TestTrue(TEXT("Rejection recorded"),W.Runner->LastRecord.StartsWith(TEXT("Home shift discarded")));
			FP5iTestAccess::Key(W.Runner,Key);
			TestTrue(TEXT("Blocked game key recorded"),W.Runner->LastRecord.StartsWith(TEXT("Home shift discarded")));
		}
		TestTrue(TEXT("No motion during blackout"),W.Pawn->GetActorTransform().Equals(Before,0.001));
		TestTrue(TEXT("Offset retained during blackout"),W.Alignment->GetHomeShiftCm() == Offset);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP3cE6,"vBoR.P3c.E6SeatAbsolutePose",Flags)
bool FP3cE6::RunTest(const FString&)
{
	FP3cEnabled Setting; FP5iWorld W;
	for (B Button : {B::Passenger,B::Rear,B::Driver})
	{
		W.Button(Button); W.FinishSwitch(); const FTransform Expected = W.Pawn->GetActorTransform();
		W.Button(B::Driver); W.FinishSwitch(); FP5iTestAccess::Key(W.Runner,EKeys::NumPadEight);
		W.Button(Button); W.FinishSwitch();
		TestTrue(TEXT("Seat uses saved absolute pose"),W.Pawn->GetActorTransform().Equals(Expected,0.001));
		TestTrue(TEXT("Seat placement clears offset"),W.Alignment->GetHomeShiftCm().IsZero());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP3cE7,"vBoR.P3c.E7ReapplyClearsOffset",Flags)
bool FP3cE7::RunTest(const FString&)
{
	FP3cEnabled Setting; FP5iWorld W; const FTransform Expected = W.Pawn->GetActorTransform();
	FP5iTestAccess::Key(W.Runner,EKeys::NumPadSix); W.Button(B::Reapply); W.FinishSwitch();
	TestTrue(TEXT("Reapply restores saved pose"),W.Pawn->GetActorTransform().Equals(Expected,0.001));
	TestTrue(TEXT("Reapply clears offset"),W.Alignment->GetHomeShiftCm().IsZero());
	FP5iTestAccess::Key(W.Runner,EKeys::NumPadEight);
	W.Alignment->InitializeAlignment(W.Pawn,&W.Saved,W.Saved.TrackingOrigin,W.Saved.PlayAreaCm);
	TestTrue(TEXT("Direct alignment also clears offset"),W.Alignment->GetHomeShiftCm().IsZero()); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP3cE8,"vBoR.P3c.E8RestartAtBase",Flags)
bool FP3cE8::RunTest(const FString&)
{
	FP3cEnabled Setting; FP5iWorld W; const FTransform Expected = W.Pawn->GetActorTransform(); W.Button(B::Next);
	FP5iTestAccess::Key(W.Runner,EKeys::NumPadEight); W.Button(B::Restart);
	TestTrue(TEXT("R at base restores original pose"),W.Pawn->GetActorTransform().Equals(Expected,0.001));
	TestTrue(TEXT("R clears offset"),W.Alignment->GetHomeShiftCm().IsZero());
	TestEqual(TEXT("Back at mission 01"),W.Runner->GetStepIndex(),0); TestTrue(TEXT("Flow running"),W.Runner->IsRunning());
	TestEqual(TEXT("Retry counted once"),W.Runner->RetryCount,1); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP3cE9,"vBoR.P3c.E9WithoutAppliedAlignment",Flags)
bool FP3cE9::RunTest(const FString&)
{
	FP3cEnabled Setting;
	for (bool bSaved : {false,true})
	{
		FP5iWorld W(bSaved); if (bSaved) { W.NeedsCheck(); }
		const auto State = W.Alignment->State; const FVector Before = W.Pawn->GetActorLocation();
		FP5iTestAccess::Key(W.Runner,EKeys::NumPadEight);
		TestTrue(TEXT("Shift allowed without Applied"),W.Pawn->GetActorLocation().Equals(Before+FVector(100,0,0),0.001));
		TestTrue(TEXT("Alignment state unchanged"),W.Alignment->State == State);
		W.Button(B::Ignore); W.Button(B::Passenger);
		TestFalse(TEXT("Seat still rejected"),W.Alignment->IsSwitching());
		TestTrue(TEXT("Rejection requires saved alignment"),W.Runner->GetOperatorStatus().LastCommand.Contains(TEXT("Saved alignment required")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP3cE10,"vBoR.P3c.E10OperatorStatus",Flags)
bool FP3cE10::RunTest(const FString&)
{
	FP3cEnabled Setting; FP5iWorld W;
	for (const FKey Key : {EKeys::NumPadEight,EKeys::NumPadEight,EKeys::NumPadFour}) { FP5iTestAccess::Key(W.Runner,Key); }
	TestTrue(TEXT("Status shows signed metres"),W.Runner->GetOperatorStatus().Seat.Contains(TEXT("Home shift ON · +2.0 m front · -1.0 m side")));
	Setting.Variable->Set(0, ECVF_SetByConsole);
	TestEqual(TEXT("Disabled status is unchanged"),W.Runner->GetOperatorStatus().Seat,FString(TEXT("Driver")));
	TestTrue(TEXT("Turning off does not move Pawn"),W.Alignment->GetHomeShiftCm() == FVector2D(200,-100)); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5oSeatViews,"vBoR.P5o.E1E2SeatViewsFoundAtStart",Flags)
bool FP5oSeatViews::RunTest(const FString&)
{
	FP5iWorld W;
	// E1: Find passenger/rear once at startup. No per-frame lookup in operator/free-mode panel
	TestEqual(TEXT("Start check scans each seat once"),FP5iTestAccess::Scans(W.Runner),2);
	W.Free(); FP5iTestAccess::ResetScans(W.Runner);
	for (int32 I = 0; I < 5; ++I) { W.Ready(); W.Runner->GetOperatorSnapshot(); }
	TestEqual(TEXT("Free-mode frames and snapshots do not scan"),FP5iTestAccess::Scans(W.Runner),0);
	TestTrue(TEXT("Passenger available"),W.Runner->EvaluateOperatorButton(B::Passenger).bAccepted);
	TestTrue(TEXT("Rear available"),W.Runner->EvaluateOperatorButton(B::Rear).bAccepted);
	TestTrue(TEXT("Free seat buttons shown"),FP5iTestAccess::SeatButtonsShown(W.Runner));
	// E2: Runtime duplicate tag ignored until next startup validation. Deleted viewpoint immediately missing
	auto* Extra = NewObject<UVBoRViewpointComponent>(W.Car); W.Car->AddInstanceComponent(Extra);
	Extra->ComponentTags.Add(TEXT("Car.RearView")); Extra->RegisterComponent();
	TestTrue(TEXT("Rear kept until the next start check"),W.Runner->EvaluateOperatorButton(B::Rear).bAccepted);
	W.Car->PassengerView->DestroyComponent(); W.Ready();
	TestEqual(TEXT("Destroyed passenger view"),W.Runner->EvaluateOperatorButton(B::Passenger).Reason,FString(TEXT("Seat not found")));
	TestFalse(TEXT("Free seat buttons need both seats"),FP5iTestAccess::SeatButtonsShown(W.Runner));
	W.Runner->InitializeFlow(); W.Ready();
	TestEqual(TEXT("Next start check scans again"),FP5iTestAccess::Scans(W.Runner),2);
	TestEqual(TEXT("Duplicate rear seen at the next start check"),W.Runner->EvaluateOperatorButton(B::Rear).Reason,FString(TEXT("Seat not found")));
	return true;
}
#endif
