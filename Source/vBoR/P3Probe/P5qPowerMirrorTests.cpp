#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "P5qTestFixture.h"
#include "../Interaction/VBoRVehiclePowerComponent.h"
#include "../Interaction/VBoRFoldingMirror.h"
#include "../Interaction/VBoRPokeButton.h"
#include "../Interaction/VBoRScenarioRunner.h"
#include "../Interaction/VBoRVehicleAlignmentComponent.h"
#include "../Interaction/VBoRHingedDoor.h"
#include "../Interaction/VBoRAxisPart.h"
#include "Interaction/IsdkPokeInteractable.h"
#include "Components/InputComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "CoreGlobals.h"
#include "Engine/Engine.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/WorldSettings.h"
#include "Misc/OutputDevice.h"
#include "Misc/OutputDeviceRedirector.h"
#include "UObject/UnrealType.h"

struct FP5qTestAccess
{
	static void Mode(AVBoRScenarioRunner* R, bool bStarted, bool bFree = false, bool bTransition = false)
	{ R->bRunning = bStarted; R->bFreeMode = bFree; R->bReloadPending = bTransition; }
	static void Key(AVBoRScenarioRunner* R)
	{ for (auto& B : R->InputComponent->KeyBindings) { if (B.Chord.Key == EKeys::E) { B.KeyDelegate.Execute(EKeys::E); return; } } }
};

namespace
{
struct FP5qLog : FOutputDevice
{
	TArray<FString> Warnings, Changes;
	FP5qLog() { GLog->FlushThreadedLogs(); GLog->AddOutputDevice(this); }
	~FP5qLog() { GLog->FlushThreadedLogs(); GLog->RemoveOutputDevice(this); }
	virtual void Serialize(const TCHAR* Text, ELogVerbosity::Type Verbosity, const FName&) override
	{
		const FString S(Text);
		if (Verbosity == ELogVerbosity::Warning && (S.StartsWith(TEXT("Vehicle power:")) || S.StartsWith(TEXT("Side mirror:")))) { Warnings.Add(S); }
		if (Verbosity == ELogVerbosity::Display && S.StartsWith(TEXT("Vehicle power:"))) { Changes.Add(S); }
	}
};

struct FP5qWorld
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	AActor* Car;
	UVBoRVehicleAlignmentComponent* Alignment;
	AVBoRScenarioRunner* Runner;
	TArray<UVBoRVehiclePowerComponent*> Powers;
	TArray<AVBoRPokeButton*> Buttons;
	AVBoRFoldingMirror* Mirrors[2];
	UP5qPowerObserver* Observer = nullptr;
	uint64 OriginalFrame = GFrameCounter;
	FP5qWorld(int32 PowerCount = 1, int32 ButtonCount = 1)
	{
		GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
		auto* PC = World->SpawnActor<APlayerController>(); auto* Pawn = World->SpawnActor<APawn>(); PC->Possess(Pawn);
		Car = World->SpawnActor<AActor>();
		Car->SetRootComponent(NewObject<USceneComponent>(Car)); Car->GetRootComponent()->RegisterComponent();
		Alignment = NewObject<UVBoRVehicleAlignmentComponent>(Car); Alignment->RegisterComponent();
		for (int32 I = 0; I < PowerCount; ++I)
		{
			auto* P = NewObject<UVBoRVehiclePowerComponent>(Car); Car->AddInstanceComponent(P); P->RegisterComponent(); Powers.Add(P);
		}
		for (int32 I = 0; I < ButtonCount; ++I)
		{
			auto* B = World->SpawnActor<AVBoRPokeButton>(); B->Tags.Add(TEXT("Car.StartButton")); Buttons.Add(B);
		}
		for (int32 I = 0; I < 2; ++I)
		{
			Mirrors[I] = World->SpawnActor<AVBoRFoldingMirror>();
			if (I == 1)
			{
				Mirrors[I]->HingeLocationCm.Y *= -1;
				FVector Location = Mirrors[I]->MeshRestFromRoot.GetLocation(); Location.Y *= -1;
				Mirrors[I]->MeshRestFromRoot.SetLocation(Location); Mirrors[I]->FoldAngleDeg *= -1;
				Mirrors[I]->OnConstruction(Mirrors[I]->GetActorTransform());
			}
		}
		if (PowerCount > 0)
		{
			Observer = NewObject<UP5qPowerObserver>(Car); Powers[0]->OnPowerChanged.AddDynamic(Observer, &UP5qPowerObserver::Changed);
		}
		Runner = World->SpawnActor<AVBoRScenarioRunner>();
		Runner->Scenario = NewObject<UVBoRScenarioAsset>(Runner);
		Runner->Scenario->Steps.Reset();
		FVBoRScenarioStep Step; Step.StepId = TEXT("01"); Step.Rule = EVBoRStepRule::Confirm; Step.ConfirmBy = EVBoRConfirmBy::Operator;
		Runner->Scenario->Steps.Add(Step);
	}
	void Play()
	{
		World->InitializeActorsForPlay(FURL()); World->GetWorldSettings()->NotifyBeginPlay();
		Alignment->State = EVBoRAlignmentState::Applied;
		Runner->InitializeFlow();
	}
	void Poke(int32 I = 0)
	{
		FIsdkInteractionPointerEvent E; E.Type = EIsdkPointerEventType::Select; E.Interactable = Buttons[I]->Poke.Get();
		Buttons[I]->Poke->GetInteractionPointerEventDelegate().Broadcast(E);
	}
	void Command(EVBoROperatorButton Button)
	{ Runner->PressOperatorButton(Button); Runner->ProcessFlow(0); }
	void TickMirrors(float Dt) { for (auto* Mirror : Mirrors) { Mirror->Tick(Dt); } }
	~FP5qWorld()
	{
		World->EndPlay(EEndPlayReason::Quit); World->DestroyWorld(false); GEngine->DestroyWorldContext(World); GFrameCounter = OriginalFrame;
	}
};

void P5qTransform(FAutomationTestBase& T, const FTransform& Actual, const FTransform& Expected)
{
	T.TestTrue(TEXT("Mesh position within .01 cm"), Actual.GetLocation().Equals(Expected.GetLocation(), .01));
	T.TestTrue(TEXT("Mesh rotation within .01 deg"), FMath::RadiansToDegrees(Actual.GetRotation().AngularDistance(Expected.GetRotation())) < .01);
	T.TestTrue(TEXT("Mesh scale preserved"), Actual.GetScale3D().Equals(Expected.GetScale3D(), 1.e-4));
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5qStart, "vBoR.P5q.E1InitialState", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5qStart::RunTest(const FString&)
{
	FP5qWorld W;
	for (auto* M : W.Mirrors)
	{
		M->HingeRotation = FRotator(15,25,-12); M->SetActorTransform(FTransform(FRotator(0,35,0), FVector(100,200,30)));
		M->OnConstruction(M->GetActorTransform());
		P5qTransform(*this, M->MirrorMesh->GetComponentTransform(), M->MeshRestFromRoot * M->GetActorTransform());
		TestEqual(TEXT("Editor unfolded"), M->GetState(), EVBoRFoldingMirrorState::Unfolded);
	}
	W.Play(); TestFalse(TEXT("Power initially off"), W.Powers[0]->IsPowerOn());
	for (auto* M : W.Mirrors)
	{
		TestEqual(TEXT("Play folded"), M->GetState(), EVBoRFoldingMirrorState::Folded);
		TestEqual(TEXT("Signed folded angle"), M->GetAngleDeg(), M->FoldAngleDeg);
		TestEqual(TEXT("No hand collision"), M->MirrorMesh->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
	}
	TestEqual(TEXT("No initial change event"), W.Observer->Values.Num(), 0); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5qOn, "vBoR.P5q.E2ButtonUnfolds", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5qOn::RunTest(const FString&)
{
	FP5qWorld W;
	for (auto* M : W.Mirrors) { M->HingeRotation = FRotator(-10,20,15); M->OnConstruction(M->GetActorTransform()); }
	W.Play(); FP5qLog Log; W.Poke();
	TestTrue(TEXT("Button powers on"), W.Powers[0]->IsPowerOn());
	TestEqual(TEXT("One true event"), W.Observer->Values.Num(), 1); TestTrue(TEXT("Event true"), W.Observer->Values[0]);
	for (int32 I = 0; I < 30; ++I)
	{
		float Before[2] = {W.Mirrors[0]->GetAngleDeg(), W.Mirrors[1]->GetAngleDeg()}; W.TickMirrors(.1f);
		for (int32 J = 0; J < 2; ++J) { TestTrue(TEXT("Speed cap deg/s"), FMath::Abs(W.Mirrors[J]->GetAngleDeg() - Before[J]) <= W.Mirrors[J]->SpeedDegPerSecond * .1f + .001f); }
	}
	for (auto* M : W.Mirrors)
	{
		TestEqual(TEXT("Unfolded"), M->GetState(), EVBoRFoldingMirrorState::Unfolded);
		P5qTransform(*this, M->MirrorMesh->GetComponentTransform(), M->MeshRestFromRoot * M->GetActorTransform());
	}
	GLog->FlushThreadedLogs();
	TestTrue(TEXT("Participant provenance"), Log.Changes.Num() == 1 && Log.Changes[0].Contains(TEXT("on | Participant"))); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5qOff, "vBoR.P5q.E3ButtonFolds", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5qOff::RunTest(const FString&)
{
	FP5qWorld W; W.Play(); W.Poke(); W.TickMirrors(10); ++GFrameCounter; W.Poke();
	TestFalse(TEXT("Second button powers off"), W.Powers[0]->IsPowerOn());
	TestTrue(TEXT("True then false exactly once"), W.Observer->Values.Num() == 2 && W.Observer->Values[0] && !W.Observer->Values[1]);
	W.TickMirrors(.5f);
	for (auto* M : W.Mirrors) { TestTrue(TEXT("Fold speed capped"), FMath::Abs(M->GetAngleDeg()) <= M->SpeedDegPerSecond * .5f + .001f); }
	W.TickMirrors(10);
	for (auto* M : W.Mirrors) { TestEqual(TEXT("Folded angle"), M->GetAngleDeg(), M->FoldAngleDeg); TestEqual(TEXT("Folded state"), M->GetState(), EVBoRFoldingMirrorState::Folded); }
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5qReverse, "vBoR.P5q.E4ReverseInMotion", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5qReverse::RunTest(const FString&)
{
	FP5qWorld W; W.Play(); W.Poke(); W.TickMirrors(.5f); ++GFrameCounter;
	float Before[2] = {W.Mirrors[0]->GetAngleDeg(), W.Mirrors[1]->GetAngleDeg()}; W.Poke();
	for (int32 I = 0; I < 2; ++I) { TestEqual(TEXT("Reverse does not snap"), W.Mirrors[I]->GetAngleDeg(), Before[I]); }
	W.TickMirrors(.1f);
	for (int32 I = 0; I < 2; ++I)
	{
		TestTrue(TEXT("Reversal heads toward fold"), FMath::Abs(W.Mirrors[I]->GetAngleDeg()) > FMath::Abs(Before[I]));
		TestTrue(TEXT("Reversal speed cap"), FMath::Abs(W.Mirrors[I]->GetAngleDeg()-Before[I]) <= 3.001f);
	}
	++GFrameCounter; W.Poke(); W.TickMirrors(.1f);
	for (int32 I = 0; I < 2; ++I) { TestTrue(TEXT("Reverse again from current angle"), FMath::IsNearlyEqual(W.Mirrors[I]->GetAngleDeg(), Before[I], .001f)); }
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5qOperator, "vBoR.P5q.E5OperatorAndKey", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5qOperator::RunTest(const FString&)
{
	FP5qWorld W; W.Play(); FP5qLog Log;
	W.Command(EVBoROperatorButton::Power); TestTrue(TEXT("Panel command powers on"), W.Powers[0]->IsPowerOn()); W.TickMirrors(10);
	for (auto* M : W.Mirrors) { TestEqual(TEXT("Operator unfolds"), M->GetState(), EVBoRFoldingMirrorState::Unfolded); }
	++GFrameCounter; FP5qTestAccess::Key(W.Runner); W.Runner->ProcessFlow(0);
	TestFalse(TEXT("Bound E key powers off"), W.Powers[0]->IsPowerOn());
	++GFrameCounter; TestTrue(TEXT("Window E consumed"), W.Runner->HandleOperatorKey(EKeys::E)); W.Runner->ProcessFlow(0);
	TestTrue(TEXT("Window E powers on"), W.Powers[0]->IsPowerOn());
	TestEqual(TEXT("Power shortcut is E"), VBoROperator::Shortcut(EVBoROperatorButton::Power).Key, EKeys::E);
	GLog->FlushThreadedLogs();
	TestEqual(TEXT("Three operator changes"), Log.Changes.Num(), 3);
	TestTrue(TEXT("Operator provenance"), Log.Changes.Num() == 3 && Log.Changes[0].Contains(TEXT("Operator")) && Log.Changes[1].Contains(TEXT("Operator")) && Log.Changes[2].Contains(TEXT("Operator")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5qSameFrame, "vBoR.P5q.E6SameFrameRequests", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5qSameFrame::RunTest(const FString&)
{
	FP5qWorld W(1,2); W.Play(); W.Poke(); W.Command(EVBoROperatorButton::Power); W.Poke(1);
	TestTrue(TEXT("Button plus operator plus second button only turns on"), W.Powers[0]->IsPowerOn()); TestEqual(TEXT("One event in frame"), W.Observer->Values.Num(), 1);
	++GFrameCounter; W.Command(EVBoROperatorButton::Power); W.Poke();
	TestFalse(TEXT("Next frame operator first turns off once"), W.Powers[0]->IsPowerOn()); TestEqual(TEXT("Second event next frame"), W.Observer->Values.Num(), 2);
	++GFrameCounter; W.Poke(1); W.Poke(); TestTrue(TEXT("Two buttons coalesce"), W.Powers[0]->IsPowerOn()); TestEqual(TEXT("Third event"), W.Observer->Values.Num(), 3); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5qRestart, "vBoR.P5q.E7Restart", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5qRestart::RunTest(const FString&)
{
	FP5qWorld W; W.Play(); W.Poke(); W.TickMirrors(10); W.Command(EVBoROperatorButton::Restart);
	TestFalse(TEXT("R powers off"), W.Powers[0]->IsPowerOn());
	for (auto* M : W.Mirrors) { TestEqual(TEXT("R folds immediately"), M->GetAngleDeg(), M->FoldAngleDeg); TestEqual(TEXT("R stops motion"), M->GetState(), EVBoRFoldingMirrorState::Folded); }
	W.TickMirrors(10); for (auto* M : W.Mirrors) { TestEqual(TEXT("No later motion"), M->GetAngleDeg(), M->FoldAngleDeg); }
	TestTrue(TEXT("R sends off once"), W.Observer->Values.Num() == 2 && !W.Observer->Values[1]); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5qMissing, "vBoR.P5q.E8MissingOrDuplicatePower", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5qMissing::RunTest(const FString&)
{
	for (int32 Count : {0,2})
	{
		FP5qWorld W(Count); FP5qLog Log; W.Play();
		TestNull(TEXT("Unique power unavailable"), UVBoRVehiclePowerComponent::FindUnique(W.World));
		auto D = W.Runner->EvaluateOperatorButton(EVBoROperatorButton::Power);
		TestFalse(TEXT("Power disabled"), D.bAccepted); TestEqual(TEXT("Reason"), D.Reason, FString(TEXT("No vehicle power")));
		TestEqual(TEXT("One setup warning plus one per mirror"), Log.Warnings.Num(), 3);
		for (int32 I = 0; I < 4; ++I)
		{
			++GFrameCounter; W.Poke(); W.Command(EVBoROperatorButton::Power); W.TickMirrors(1);
			for (auto* P : W.Powers) { P->TogglePower(); P->SetPowerOn(true); TestFalse(TEXT("Invalid power cannot change"), P->IsPowerOn()); }
		}
		TestEqual(TEXT("No repeated warnings"), Log.Warnings.Num(), 3);
		for (auto* M : W.Mirrors) { TestEqual(TEXT("Invalid power stays folded"), M->GetAngleDeg(), M->FoldAngleDeg); }
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5qMissingButton, "vBoR.P5q.E9NoStartButton", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5qMissingButton::RunTest(const FString&)
{
	FP5qWorld W(1,0); FP5qLog Log; W.Play();
	TestEqual(TEXT("One absent-button warning"), Log.Warnings.Num(), 1);
	W.Command(EVBoROperatorButton::Power); TestTrue(TEXT("Operator remains available"), W.Powers[0]->IsPowerOn());
	W.TickMirrors(10); TestEqual(TEXT("No repeated absent-button warnings"), Log.Warnings.Num(), 1); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5qDoor, "vBoR.P5q.E10DoorAttachment", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5qDoor::RunTest(const FString&)
{
	FP5qWorld W; auto* Door = W.World->SpawnActor<AVBoRHingedDoor>();
	auto* Handle = W.World->SpawnActor<AVBoRAxisPart>(); Handle->DoorHandleSide = EVBoRDoorHandleSide::Outside;
	Handle->AttachToComponent(Door->GetRootComponent(), FAttachmentTransformRules::KeepWorldTransform);
	W.Mirrors[0]->AttachToComponent(Door->GetRootComponent(), FAttachmentTransformRules::KeepWorldTransform);
	W.Play();
	TestTrue(TEXT("Mirror attaches to hinge"), W.Mirrors[0]->GetRootComponent()->GetAttachParent() == Door->HingePivot);
	TestTrue(TEXT("Existing handle still found"), Door->OutsideAxisHandle == Handle);
	TestTrue(TEXT("Handle still attaches to hinge"), Handle->GetRootComponent()->GetAttachParent() == Door->HingePivot);
	const FTransform MirrorLocal = W.Mirrors[0]->GetActorTransform().GetRelativeTransform(Door->HingePivot->GetComponentTransform());
	Door->HingePivot->SetRelativeRotation(FRotator(0,40,0));
	P5qTransform(*this, W.Mirrors[0]->GetActorTransform(), MirrorLocal * Door->HingePivot->GetComponentTransform());
	W.Poke(); W.TickMirrors(10);
	P5qTransform(*this, W.Mirrors[0]->MirrorMesh->GetComponentTransform(), W.Mirrors[0]->MeshRestFromRoot * W.Mirrors[0]->GetActorTransform());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5qEvents, "vBoR.P5q.E11AssignableEvent", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5qEvents::RunTest(const FString&)
{
	FP5qWorld W; W.Play(); auto* P = W.Powers[0];
	P->SetPowerOn(false); P->SetPowerOn(true); P->SetPowerOn(true); P->SetPowerOn(false); P->SetPowerOn(false);
	TestTrue(TEXT("Listener sees changes only"), W.Observer->Values.Num() == 2 && W.Observer->Values[0] && !W.Observer->Values[1]);
	const FMulticastDelegateProperty* Property = FindFProperty<FMulticastDelegateProperty>(P->GetClass(), GET_MEMBER_NAME_CHECKED(UVBoRVehiclePowerComponent, OnPowerChanged));
	TestTrue(TEXT("Event assignable in Blueprint"), Property && Property->HasAnyPropertyFlags(CPF_BlueprintAssignable)); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5qRules, "vBoR.P5q.E12OperatorEnableRules", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5qRules::RunTest(const FString&)
{
	FP5qWorld W; W.Play();
	FP5qTestAccess::Mode(W.Runner,false); TestFalse(TEXT("Before start disabled"), W.Runner->EvaluateOperatorButton(EVBoROperatorButton::Power).bAccepted);
	W.Command(EVBoROperatorButton::Power); TestFalse(TEXT("Before start command ignored"), W.Powers[0]->IsPowerOn());
	FP5qTestAccess::Mode(W.Runner,true); TestTrue(TEXT("Guided enabled like seat folding"), W.Runner->EvaluateOperatorButton(EVBoROperatorButton::Power).bAccepted);
	FP5qTestAccess::Mode(W.Runner,false,true); TestTrue(TEXT("Free mode enabled"), W.Runner->GetOperatorSnapshot().Buttons[static_cast<int32>(EVBoROperatorButton::Power)].Decision.bAccepted);
	FP5qTestAccess::Mode(W.Runner,false,true,true); TestFalse(TEXT("Transition disabled"), W.Runner->EvaluateOperatorButton(EVBoROperatorButton::Power).bAccepted);
	W.Command(EVBoROperatorButton::Power); TestFalse(TEXT("Transition command ignored"), W.Powers[0]->IsPowerOn());
	FP5qTestAccess::Mode(W.Runner,false,true); W.Command(EVBoROperatorButton::Power); TestTrue(TEXT("Free command accepted"), W.Powers[0]->IsPowerOn()); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5qAssets, "vBoR.P5q.E13MainAssets", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5qAssets::RunTest(const FString&)
{
	UPackage* Package = LoadPackage(nullptr, TEXT("/Game/Main/L_Main"), LOAD_None);
	UWorld* W = Package ? UWorld::FindWorldInPackage(Package) : nullptr; if (!TestNotNull(TEXT("Main package"), W)) { return false; }
	int32 CoreCount = 0, PowerCount = 0, StartCount = 0, LeftCount = 0, RightCount = 0;
	for (ULevel* Level : W->GetLevels()) { for (AActor* A : Level->Actors)
	{
		if (!A) { continue; }
		TArray<UVBoRVehiclePowerComponent*> Powers; A->GetComponents(Powers); PowerCount += Powers.Num();
		if (A->GetClass()->GetName() == TEXT("BP_Car_Core_C")) { ++CoreCount; TestEqual(TEXT("Core owns one power component"), Powers.Num(), 1); }
		if (A->ActorHasTag(TEXT("Car.StartButton")))
		{
			++StartCount; const auto* B = Cast<AVBoRPokeButton>(A); if (TestNotNull(TEXT("Start button parent"), B))
			{ TestEqual(TEXT("Start name"), B->SpectatorName.ToString(), FString(TEXT("Start Button"))); TestEqual(TEXT("Default hit area"), B->HitAreaScale, GetDefault<AVBoRPokeButton>()->HitAreaScale); }
		}
		const FString Name = A->GetClass()->GetName();
		if (Name == TEXT("BP_SideMirror_L_C") || Name == TEXT("BP_SideMirror_R_C"))
		{
			const bool bLeft = Name == TEXT("BP_SideMirror_L_C"); if (bLeft) { ++LeftCount; } else { ++RightCount; }
			TestTrue(TEXT("Mirror native parent"), A->GetClass()->GetSuperClass() == AVBoRFoldingMirror::StaticClass());
			P5qTransform(*this, A->GetRootComponent()->GetRelativeTransform(), FTransform::Identity);
			TestEqual(TEXT("Mirror name"), CastChecked<AVBoRFoldingMirror>(A)->SpectatorName.ToString(), FString(TEXT("Side Mirror")));
			if (bLeft)
			{
				AActor* Parent = A->GetAttachParentActor(); if (TestNotNull(TEXT("Left Outliner parent"), Parent)) { TestEqual(TEXT("Left parent is FL door"), Parent->GetClass()->GetName(), FString(TEXT("BP_Door_FL_C"))); }
			}
		}
	} }
	TestEqual(TEXT("One car core"), CoreCount, 1); TestEqual(TEXT("One level power component"), PowerCount, 1);
	TestEqual(TEXT("One start button"), StartCount, 1); TestEqual(TEXT("One left mirror"), LeftCount, 1); TestEqual(TEXT("One right mirror"), RightCount, 1); return true;
}
#endif
