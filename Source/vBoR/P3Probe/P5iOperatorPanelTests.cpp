#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "P5iTestFixture.h"
#include "P5gTestFixture.h"
#include "P3VehicleMock.h"
#include "../Interaction/VBoRScenarioRunner.h"
#include "../Interaction/VBoREyePointComponent.h"
#include "../Interaction/VBoRViewpointComponent.h"
#include "Components/InputComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/WorldSettings.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/SlateRenderer.h"
#include "Fonts/FontMeasure.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "XRTrackingSystemBase.h"

struct FP5iTestAccess
{
	static void NewFrame(AVBoRScenarioRunner* R) { R->LiveFrame = MAX_uint64; }
	static const TArray<FVBoRFlowCommand>& Queue(AVBoRScenarioRunner* R) { return R->PendingCommands; }
	static void Clear(AVBoRScenarioRunner* R) { R->PendingCommands.Reset(); }
	static void Key(AVBoRScenarioRunner* R, FKey Key)
	{ for (auto& Binding : R->InputComponent->KeyBindings) { if (Binding.Chord.Key == Key) { Binding.KeyDelegate.Execute(Key); return; } } }
	static void Dispatch(AVBoRScenarioRunner* R, const FVBoRFlowCommand& C) { R->Dispatch(C); }
	static void Lock(AVBoRScenarioRunner* R) { R->ConfirmLockRemaining = 1; }
	static void Invalid(AVBoRScenarioRunner* R) { R->ValidationProblems = TEXT("invalid tags"); }
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
		FString Path;
		FP5iWorld(bool bSave = true)
		{
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
		{ World->EndPlay(EEndPlayReason::Quit); GEngine->DestroyWorldContext(World); World->DestroyWorld(false); GEngine->XRSystem = OriginalXR; IFileManager::Get().Delete(*Path); }
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
bool FP5iE6::RunTest(const FString&) { FP5iWorld W; W.Free(); Enabled(*this,W,{B::Driver,B::Passenger,B::Rear,B::Restart,B::Reload,B::Reapply,B::Info}); return true; }

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
	// 메모리의 이전 값과 다르게 파일을 갱신해 실제 재읽기도 확인함 (P5i D8, cm)
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
	TestEqual(TEXT("Transition discards"),W.Runner->TransitionDiscards,5); TestEqual(TEXT("Same step"),W.Runner->GetStepIndex(),0); W.FinishSwitch(); return true;
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
	// 실제 위젯의 상태 줄이 긴 이유를 한 줄 말줄임으로 처리하는지 확인함 (P5i E23)
	W.Alignment->State = EVBoRAlignmentState::NeedsCheck; W.Alignment->Reason = FString::ChrN(400,TEXT('W'));
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
	W.Free(); TestTrue(TEXT("R restart shown"),W.Runner->GetOperatorText().Contains(TEXT("R 처음부터"))); TestFalse(TEXT("F5 absent"),W.Runner->GetOperatorText().Contains(TEXT("F5"))); return true;
}
#endif
