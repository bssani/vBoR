#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "P5gTestFixture.h"
#include "P3VehicleMock.h"
#include "../Interaction/VBoRVehicleAlignmentComponent.h"
#include "../Interaction/VBoREyePointComponent.h"
#include "../Interaction/VBoRViewpointComponent.h"
#include "../Interaction/VBoRPokeButton.h"
#include "../Interaction/VBoRHingedDoor.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/ArrowComponent.h"
#include "Components/InputComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/WorldSettings.h"
#include "Interaction/IsdkPokeInteractable.h"
#include "Kismet/GameplayStatics.h"
#include "Widgets/Text/STextBlock.h"
#include "XRTrackingSystemBase.h"

struct FP5gTestAccess
{
	static AVBoRPokeButton* Button(AVBoRScenarioRunner* R, int32 I) { return I == 0 ? R->ConfirmButton.Get() : R->ActionButton.Get(); }
	static FString Label(AVBoRScenarioRunner* R, int32 I) { return R->ButtonLabelTexts[I]->GetText().ToString(); }
	static bool MarkersHidden(AVBoRScenarioRunner* R)
	{
		if (!R->MarkerArrow->bHiddenInGame) { return false; }
		for (const auto& Part : R->MarkerRing) { if (!Part->bHiddenInGame) { return false; } }
		return true;
	}
	static void Key(AVBoRScenarioRunner* R, FKey Key)
	{
		R->LiveFrame = MAX_uint64;
		for (auto& Binding : R->InputComponent->KeyBindings)
		{ if (Binding.Chord.Key == Key && Binding.KeyEvent == IE_Pressed) { Binding.KeyDelegate.Execute(Key); return; } }
	}
	static void NewFrame(AVBoRScenarioRunner* R) { R->LiveFrame = MAX_uint64; }
};

namespace
{
	class FP5gHead : public FXRTrackingSystemBase
	{
	public:
		FP5gHead() : FXRTrackingSystemBase(nullptr) {}
		FTransform LocalHead;
		bool bTracked = true;
		virtual FName GetSystemName() const override { return TEXT("P5gSyntheticHead"); }
		virtual int32 GetXRSystemFlags() const override { return 0; }
		virtual bool EnumerateTrackedDevices(TArray<int32>& Out, EXRTrackedDeviceType) override { Out.Add(0); return true; }
		virtual bool IsTracking(int32 Device) override { return bTracked && Device == 0; }
		virtual bool GetCurrentPose(int32 Device, FQuat& Rotation, FVector& Position) override
		{ Rotation = LocalHead.GetRotation(); Position = LocalHead.GetLocation(); return IsTracking(Device); }
		virtual float GetWorldToMetersScale() const override { return 100; }
		virtual void ResetOrientationAndPosition(float) override {}
		virtual bool IsHeadTrackingAllowed() const override { return true; }
	};
	struct FP5gWorld
	{
		UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
		APawn* Pawn;
		APlayerController* Controller;
		AP3VehicleMock* Car;
		AP5gReloadObserver* Runner;
		TSharedPtr<IXRTrackingSystem, ESPMode::ThreadSafe> OriginalXR;
		TSharedPtr<FP5gHead, ESPMode::ThreadSafe> Head;
		FP5gWorld(bool bAligned = true)
		{
			GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
			Controller = World->SpawnActor<APlayerController>(); Pawn = World->SpawnActor<APawn>();
			Pawn->SetRootComponent(NewObject<USceneComponent>(Pawn)); Pawn->GetRootComponent()->RegisterComponent(); Controller->Possess(Pawn);
			Car = World->SpawnActor<AP3VehicleMock>(); Runner = World->SpawnActor<AP5gReloadObserver>();
			Runner->Scenario = NewObject<UVBoRScenarioAsset>(Runner);
			Runner->Scenario->Steps.Reset();
			FVBoRScenarioStep Step; Step.StepId = TEXT("05"); Step.Instruction = FText::FromString(TEXT("Test"));
			Car->DriverEye->ComponentTags.Add(TEXT("Car.DriverEye"));
			Step.MarkerTag = TEXT("Car.DriverEye"); Runner->Scenario->Steps.Add(Step);
			World->InitializeActorsForPlay(FURL()); World->GetWorldSettings()->NotifyBeginPlay();
			FVBoRBuckSavedAlignment Saved; Saved.SavedAt = TEXT("2026-10-16T00:00:00Z"); Saved.TrackingOrigin = 3;
			Saved.PlayAreaCm = FVector2D(300,400); Saved.Capture.PositionCm = FVector(10,20,115); Saved.Capture.YawDeg = 30;
			Car->Alignment->InitializeAlignment(Pawn, bAligned ? &Saved : nullptr, 3, Saved.PlayAreaCm);
			OriginalXR = GEngine->XRSystem; Head = MakeShared<FP5gHead, ESPMode::ThreadSafe>(); GEngine->XRSystem = Head;
			Head->LocalHead = Car->DriverEye->GetComponentTransform().GetRelativeTransform(Pawn->GetActorTransform());
			Runner->InitializeFlow(); Ready();
		}
		~FP5gWorld() { World->EndPlay(EEndPlayReason::Quit); GEngine->DestroyWorldContext(World); World->DestroyWorld(false); GEngine->XRSystem = OriginalXR; }
		void Ready() { Runner->ProcessFlow(0); Runner->ProcessFlow(1); }
		void Command(EVBoRFlowCommand Action, EVBoRCommandSource Source = EVBoRCommandSource::Operator)
		{ Runner->QueueCommand({Action, Source, Runner->GetToken()}); Runner->ProcessFlow(0); }
		void Key(FKey Key) { FP5gTestAccess::Key(Runner, Key); Runner->ProcessFlow(0); }
		void Free() { Key(EKeys::End); Ready(); }
		void FinishSwitch() { Car->Alignment->AdvanceSwitch(Car->Alignment->FadeSeconds); Car->Alignment->AdvanceSwitch(Car->Alignment->FadeSeconds); Runner->ProcessFlow(0); }
		void Poke(int32 I)
		{
			FP5gTestAccess::NewFrame(Runner);
			auto* Button = FP5gTestAccess::Button(Runner,I);
			FIsdkInteractionPointerEvent Event; Event.Type = EIsdkPointerEventType::Select; Event.Interactable = Button->Poke.Get();
			Button->Poke->GetInteractionPointerEventDelegate().Broadcast(Event);
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5gCompletionTest, "vBoR.P5g.E1E2CompletedFreeMode", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5gCompletionTest::RunTest(const FString&)
{
	FP5gWorld E;
	E.Runner->FreeModeInstruction = FText::FromString(TEXT("Custom free mode"));
	E.Command(EVBoRFlowCommand::Confirm);
	TestTrue(TEXT("Last step completes into free mode"),E.Runner->IsFinished() && E.Runner->IsFreeMode());
	TestFalse(TEXT("No running evaluation"),E.Runner->IsRunning());
	TestEqual(TEXT("No current step"),E.Runner->GetStepIndex(),INDEX_NONE);
	TestTrue(TEXT("Completion plus editable instruction"),E.Runner->GetPanelText().Contains(TEXT("안내 완료")) && E.Runner->GetPanelText().Contains(TEXT("Custom free mode")));
	TestTrue(TEXT("Markers hidden"),FP5gTestAccess::MarkersHidden(E.Runner));
	TestTrue(TEXT("No step instruction"),E.Runner->GetInstructionText().IsEmpty());
	TestFalse(TEXT("Passenger button visible"),FP5gTestAccess::Button(E.Runner,0)->IsHidden());
	TestEqual(TEXT("Passenger label"),FP5gTestAccess::Label(E.Runner,0),FString(TEXT("동승석")));
	TestEqual(TEXT("Rear label"),FP5gTestAccess::Label(E.Runner,1),FString(TEXT("2열")));
	const auto Token = E.Runner->GetToken(); const int32 Before = E.Runner->RuleDiscards;
	for (FKey Key : {EKeys::SpaceBar,EKeys::Left,EKeys::Right,EKeys::End}) { E.Key(Key); }
	E.Command(EVBoRFlowCommand::Confirm,EVBoRCommandSource::Participant);
	TestEqual(TEXT("Step commands discarded"),E.Runner->RuleDiscards,Before+5);
	TestEqual(TEXT("Generation retained"),E.Runner->GetToken().Generation,Token.Generation);
	TestEqual(TEXT("End shortcut not counted on completion"),E.Runner->EndCount,0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5gSeatsTest, "vBoR.P5g.E3E4E5E6ParticipantSeats", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5gSeatsTest::RunTest(const FString&)
{
	FP5gWorld E; E.Free();
	E.Poke(0); E.Runner->QueueCommand({EVBoRFlowCommand::Rear,EVBoRCommandSource::Operator,E.Runner->GetToken()}); E.Runner->ProcessFlow(0);
	TestTrue(TEXT("One accepted switch for simultaneous inputs"),E.Car->Alignment->IsSwitching() && E.Car->Alignment->SwitchRequests == 2 && E.Car->Alignment->SwitchDiscards == 1);
	TestTrue(TEXT("Both buttons hidden and inactive during fade"),FP5gTestAccess::Button(E.Runner,0)->IsHidden() && FP5gTestAccess::Button(E.Runner,1)->IsHidden() && !FP5gTestAccess::Button(E.Runner,0)->Poke->IsActive());
	const int32 Before = E.Runner->TransitionDiscards;
	E.Command(EVBoRFlowCommand::Passenger,EVBoRCommandSource::Participant); E.Key(EKeys::One);
	TestEqual(TEXT("Additional switches discarded"),E.Runner->TransitionDiscards,Before+2);
	E.FinishSwitch();
	TestTrue(TEXT("First passenger target retained"),E.Car->Alignment->IsAtViewpoint(E.Car->PassengerView));
	TestEqual(TEXT("Passenger offers driver"),FP5gTestAccess::Label(E.Runner,0),FString(TEXT("운전석")));
	TestEqual(TEXT("Passenger offers rear"),FP5gTestAccess::Label(E.Runner,1),FString(TEXT("2열")));
	const int32 Locked = E.Runner->ConfirmLockDiscards;
	E.Poke(1); E.Runner->ProcessFlow(1);
	TestEqual(TEXT("Input captured during placement lock stays discarded despite long dt"),E.Runner->ConfirmLockDiscards,Locked+1);
	TestFalse(TEXT("Locked input never starts transition"),E.Car->Alignment->IsSwitching());
	E.Poke(1); E.Runner->ProcessFlow(0); E.FinishSwitch(); E.Ready();
	TestTrue(TEXT("Participant rear destination"),E.Car->Alignment->IsAtViewpoint(E.Car->RearView));
	TestTrue(TEXT("Participant request distinguished"),E.Runner->LastViewRequest.Contains(TEXT("참가자")));
	TestEqual(TEXT("Rear offers driver"),FP5gTestAccess::Label(E.Runner,0),FString(TEXT("운전석")));
	TestEqual(TEXT("Rear offers passenger"),FP5gTestAccess::Label(E.Runner,1),FString(TEXT("동승석")));
	E.Poke(0); E.Runner->ProcessFlow(0); E.FinishSwitch();
	TestTrue(TEXT("Participant returns to base"),E.Car->Alignment->IsBaseViewpoint());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5gOperatorTest, "vBoR.P5g.E7E8E10E13OperatorAndRestart", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5gOperatorTest::RunTest(const FString&)
{
	FP5gWorld E; E.Key(EKeys::End);
	TestTrue(TEXT("End enters free mode without completion"),E.Runner->IsFreeMode() && !E.Runner->IsFinished());
	TestFalse(TEXT("End panel never claims completed"),E.Runner->GetPanelText().Contains(TEXT("안내 완료")));
	TestTrue(TEXT("End record contains origin and summary"),E.Runner->LastRecord.Contains(TEXT("05")) && E.Runner->LastRecord.Contains(TEXT("요약")));
	TestEqual(TEXT("End counted separately"),E.Runner->EndCount,1); TestEqual(TEXT("Skip unchanged"),E.Runner->SkipCount,0);
	const FString Text = E.Runner->GetOperatorText();
	for (const TCHAR* Word : {TEXT("자유모드"),TEXT("1"),TEXT("2"),TEXT("Home"),TEXT("F5"),TEXT("F6")}) { TestTrue(TEXT("Operator keys advertised"),Text.Contains(Word)); }
	E.Key(EKeys::One); E.FinishSwitch(); TestTrue(TEXT("Operator passenger"),E.Car->Alignment->IsAtViewpoint(E.Car->PassengerView));
	E.Key(EKeys::Two); E.FinishSwitch(); TestTrue(TEXT("Operator rear"),E.Car->Alignment->IsAtViewpoint(E.Car->RearView));
	E.Key(EKeys::Home); E.FinishSwitch(); TestTrue(TEXT("Operator base"),E.Car->Alignment->IsBaseViewpoint());
	TestTrue(TEXT("Operator source retained"),E.Runner->LastViewRequest.Contains(TEXT("진행자")));
	E.Key(EKeys::Two); E.FinishSwitch();
	auto& Door = const_cast<FVBoRDoorMotion&>(E.Car->Door->GetMotion()); Door.AngleDeg = 55; Door.bLocked = false;
	E.Key(EKeys::F5); E.FinishSwitch();
	TestTrue(TEXT("F5 restores evaluation and base"),E.Runner->IsRunning() && !E.Runner->IsFreeMode() && !E.Runner->IsFinished() && E.Car->Alignment->IsBaseViewpoint());
	TestEqual(TEXT("First step restored"),E.Runner->GetStepIndex(),0); TestEqual(TEXT("Retry counted"),E.Runner->RetryCount,1);
	TestTrue(TEXT("Door reset unchanged"),Door.bLocked && Door.AngleDeg == 0);
	TestTrue(TEXT("Seat action hidden during evaluation"),FP5gTestAccess::Button(E.Runner,1)->IsHidden());
	TestEqual(TEXT("Evaluation label restored"),FP5gTestAccess::Label(E.Runner,0),FString(TEXT("확인")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5gGuardsTest, "vBoR.P5g.E9E11E12GuardsAndTracking", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5gGuardsTest::RunTest(const FString&)
{
	FP5gWorld E(false); E.Key(EKeys::End);
	TestFalse(TEXT("End before alignment discarded"),E.Runner->IsFreeMode()); TestEqual(TEXT("Prestart refusal counted"),E.Runner->RuleDiscards,1);
	E.Key(EKeys::O); E.Command(EVBoRFlowCommand::Passenger,EVBoRCommandSource::Participant); E.Command(EVBoRFlowCommand::Rear,EVBoRCommandSource::Participant);
	TestEqual(TEXT("Evaluation participant seat permissions unchanged"),E.Runner->PermissionDiscards,2);
	E.Car->Alignment->State = EVBoRAlignmentState::Applied;
	// 유효 저장값을 다시 공급해 실제 전환을 시작함 (P5g E9)
	FVBoRBuckSavedAlignment Saved; Saved.SavedAt = TEXT("test"); Saved.TrackingOrigin = 3; Saved.PlayAreaCm = FVector2D(300,400);
	E.Car->Alignment->InitializeAlignment(E.Pawn,&Saved,3,Saved.PlayAreaCm);
	E.Key(EKeys::One); E.Key(EKeys::End);
	TestFalse(TEXT("End during switch discarded"),E.Runner->IsFreeMode()); TestEqual(TEXT("Transition refusal counted"),E.Runner->TransitionDiscards,1);
	E.FinishSwitch(); E.Head->bTracked = false; E.Key(EKeys::End);
	TestTrue(TEXT("End can enter with untracked head"),E.Runner->IsFreeMode());
	TestTrue(TEXT("Panel and buttons await tracked head"),E.Runner->Panel->bHiddenInGame && FP5gTestAccess::Button(E.Runner,0)->IsHidden() && FP5gTestAccess::Button(E.Runner,1)->IsHidden());
	E.Key(EKeys::End); TestEqual(TEXT("End accepted only once"),E.Runner->EndCount,1);
	E.Head->bTracked = true; E.Runner->ProcessFlow(0);
	TestFalse(TEXT("Recovery places panel"),E.Runner->Panel->bHiddenInGame); TestFalse(TEXT("Recovery shows seat buttons"),FP5gTestAccess::Button(E.Runner,0)->IsHidden());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5gReloadTest, "vBoR.P5g.E15E16E17ReloadAfterFade", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5gReloadTest::RunTest(const FString&)
{
	FP5gWorld E(false); E.Key(EKeys::F6);
	TestEqual(TEXT("Prestart reload rejected"),E.Runner->RuleDiscards,1); TestEqual(TEXT("No prestart travel"),E.Runner->ReloadRequests,0);
	TestFalse(TEXT("No prestart fade"),E.Controller->PlayerCameraManager->bEnableFading);
	E.Key(EKeys::O); E.Key(EKeys::F6); TestEqual(TEXT("Evaluation reload rejected"),E.Runner->RuleDiscards,2);
	TestFalse(TEXT("No evaluation fade"),E.Controller->PlayerCameraManager->bEnableFading);
	E.Free(); const FName Level(*UGameplayStatics::GetCurrentLevelName(E.Runner));
	FP5gTestAccess::Key(E.Runner,EKeys::F6); E.Runner->ProcessFlow(2);
	TestTrue(TEXT("Reload starts fade"),E.Controller->PlayerCameraManager->bEnableFading);
	TestTrue(TEXT("Reload fades to black using alignment duration"),E.Controller->PlayerCameraManager->FadeColor == FLinearColor::Black &&
		E.Controller->PlayerCameraManager->FadeAlpha == FVector2D(0,1) && E.Controller->PlayerCameraManager->FadeTime == E.Car->Alignment->FadeSeconds);
	TestEqual(TEXT("Request frame dt never counts toward new fade"),E.Runner->ReloadRequests,0);
	TestTrue(TEXT("Reload recorded"),E.Runner->LastRecord.Contains(TEXT("레벨")));
	TestTrue(TEXT("Buttons hidden during reload"),FP5gTestAccess::Button(E.Runner,0)->IsHidden() && FP5gTestAccess::Button(E.Runner,1)->IsHidden());
	const int32 Discards = E.Runner->TransitionDiscards;
	E.Command(EVBoRFlowCommand::Passenger,EVBoRCommandSource::Participant);
	for (FKey Key : {EKeys::F6,EKeys::One,EKeys::Two,EKeys::Home,EKeys::F5,EKeys::F1}) { E.Key(Key); }
	TestEqual(TEXT("All reload-fade inputs discarded"),E.Runner->TransitionDiscards,Discards+7);
	E.Runner->ProcessFlow(0.29); TestEqual(TEXT("No travel before 0.3s"),E.Runner->ReloadRequests,0);
	E.Runner->ProcessFlow(0.02); TestEqual(TEXT("Travel once after black"),E.Runner->ReloadRequests,1);
	TestEqual(TEXT("Same current level requested"),E.Runner->ReloadLevel,Level);
	E.Runner->ProcessFlow(10); E.Key(EKeys::F6); E.Runner->ProcessFlow(10);
	TestEqual(TEXT("No repeated travel while pending"),E.Runner->ReloadRequests,1);
	return true;
}
#endif
