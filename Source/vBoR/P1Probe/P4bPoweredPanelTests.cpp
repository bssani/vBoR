#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "P4bTestObserver.h"
#include "P1LoadBench.h"
#include "P1TrunkMock.h"
#include "../Interaction/VBoRCarryObject.h"
#include "../Interaction/VBoRHandRigPawn.h"
#include "../Interaction/VBoRPokeButton.h"
#include "Components/AudioComponent.h"
#include "Components/InputComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/WorldSettings.h"
#include "Interaction/IsdkPokeInteractable.h"

namespace
{
	struct FPoweredWorld
	{
		UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
		AVBoRPoweredPanel* Gate = nullptr;
		UP4bTestObserver* Observer = nullptr;
		FPoweredWorld()
		{
			World->SpawnActor<APlayerController>();
			Gate = World->SpawnActor<AVBoRPoweredPanel>();
			World->InitializeActorsForPlay(FURL());
			World->GetWorldSettings()->NotifyBeginPlay();
			Observer = NewObject<UP4bTestObserver>(World);
			Gate->OnStateChanged.AddDynamic(Observer, &UP4bTestObserver::State);
			Gate->OnOpened.AddDynamic(Observer, &UP4bTestObserver::Open);
			Gate->OnClosed.AddDynamic(Observer, &UP4bTestObserver::Close);
			Gate->OnObstructed.AddDynamic(Observer, &UP4bTestObserver::Obstruction);
		}
		~FPoweredWorld() { World->EndPlay(EEndPlayReason::Quit); World->DestroyWorld(false); }
		void Key(FKey Key)
		{
			for (FInputKeyBinding& Binding : Gate->InputComponent->KeyBindings)
			{
				if (Binding.Chord.Key == Key && Binding.KeyEvent == IE_Pressed) { Binding.KeyDelegate.Execute(Key); }
			}
		}
		void Poke(AVBoRPokeButton* Button)
		{
			FIsdkInteractionPointerEvent Event;
			Event.Type = EIsdkPointerEventType::Select;
			Event.Interactable = Button->Poke.Get();
			Button->Poke->GetInteractionPointerEventDelegate().Broadcast(Event);
		}
		AVBoRCarryObject* Box(FVector Position, FVector Size = FVector(20))
		{
			const FTransform Pose(Gate->GetActorQuat(), Gate->GetActorTransform().TransformPosition(Position));
			AVBoRCarryObject* Object = World->SpawnActorDeferred<AVBoRCarryObject>(AVBoRCarryObject::StaticClass(), Pose, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
			Object->BoxSizeCm = Size;
			Object->FinishSpawning(Pose);
			return Object;
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP4bOpenTest, "vBoR.P4b.OpenWaitWarningAndArrival", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP4bOpenTest::RunTest(const FString& Parameters)
{
	FPoweredWorld F;
	F.Poke(F.Gate->OpenButton);
	TestTrue(TEXT("E1 accepts open"), F.Gate->GetState() == EVBoRPoweredState::OpeningWait);
	F.Gate->Tick(0.75);
	TestEqual(TEXT("E1 stationary during wait"), F.Gate->GetAngleDeg(), 0.0f);
	TestTrue(TEXT("E1 warning requested during wait"), F.Gate->GetWarningRequests() >= 2 && F.Gate->WarningSound != nullptr);
	F.Gate->Tick(0.25);
	TestTrue(TEXT("E1 starts at delay boundary"), F.Gate->GetState() == EVBoRPoweredState::Opening);
	F.Gate->Tick(4);
	TestEqual(TEXT("E1 reaches configured angle"), F.Gate->GetAngleDeg(), F.Gate->OpenAngleDeg);
	TestTrue(TEXT("E1 open state"), F.Gate->GetState() == EVBoRPoweredState::Open);
	TestEqual(TEXT("E1 opened event exactly once"), F.Observer->Opened, 1);
	TestEqual(TEXT("E1 wait/move/open events"), F.Observer->States.Num(), 3);
	const int32 Warnings = F.Gate->GetWarningRequests();
	F.Gate->Tick(10);
	TestEqual(TEXT("E1 idle has no warning requests"), F.Gate->GetWarningRequests(), Warnings);
	TestFalse(TEXT("E1 arrival stops audio"), F.Gate->WarningAudio->IsPlaying());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP4bCloseTest, "vBoR.P4b.CloseWaitWarningAndArrival", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP4bCloseTest::RunTest(const FString& Parameters)
{
	FPoweredWorld F;
	F.Gate->bStartOpen = true; F.Gate->ResetPanel(); F.Observer->States.Reset();
	F.Poke(F.Gate->CloseButton);
	F.Gate->Tick(0.5);
	TestEqual(TEXT("E2 holds open angle while waiting"), F.Gate->GetAngleDeg(), F.Gate->OpenAngleDeg);
	F.Gate->Tick(4.5);
	TestTrue(TEXT("E2 closed"), F.Gate->GetState() == EVBoRPoweredState::Closed);
	TestEqual(TEXT("E2 no overshoot"), F.Gate->GetAngleDeg(), 0.0f);
	TestEqual(TEXT("E2 closed event once"), F.Observer->Closed, 1);
	TestEqual(TEXT("E2 wait/move/closed events"), F.Observer->States.Num(), 3);
	const int32 Warnings = F.Gate->GetWarningRequests();
	TestTrue(TEXT("E2 warning throughout wait and move"), Warnings >= 10);
	F.Gate->Tick(1);
	TestEqual(TEXT("E2 warning stops"), F.Gate->GetWarningRequests(), Warnings);
	TestFalse(TEXT("E2 audio stopped"), F.Gate->WarningAudio->IsPlaying());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP4bIgnoreTest, "vBoR.P4b.BusyInputsDoNotRestartOrReverse", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP4bIgnoreTest::RunTest(const FString& Parameters)
{
	FPoweredWorld F;
	for (bool bOpening : {true, false})
	{
		if (bOpening) { F.Key(F.Gate->OpenKey); } else { F.Key(F.Gate->CloseKey); }
		F.Gate->Tick(0.4);
		const double Wait = F.Gate->GetWaitRemaining();
		const EVBoRPoweredState State = F.Gate->GetState();
		F.Poke(F.Gate->OpenButton); F.Poke(F.Gate->CloseButton);
		F.Key(F.Gate->OpenKey); F.Key(F.Gate->CloseKey);
		TestTrue(TEXT("E3 busy state unchanged"), F.Gate->GetState() == State);
		TestEqual(TEXT("E3 timer not restarted"), F.Gate->GetWaitRemaining(), Wait);
		F.Gate->Tick(0.6); F.Gate->Tick(1);
		const float Angle = F.Gate->GetAngleDeg();
		F.Poke(F.Gate->OpenButton); F.Poke(F.Gate->CloseButton);
		F.Key(F.Gate->OpenKey); F.Key(F.Gate->CloseKey);
		TestEqual(TEXT("E3 moving input cannot change angle"), F.Gate->GetAngleDeg(), Angle);
		F.Gate->Tick(3);
		TestTrue(TEXT("E3 original motion arrives"), F.Gate->GetState() == (bOpening ? EVBoRPoweredState::Open : EVBoRPoweredState::Closed));
	}
	TestEqual(TEXT("E3 opens once"), F.Observer->Opened, 1);
	TestEqual(TEXT("E3 closes once"), F.Observer->Closed, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP4bSameDirectionTest, "vBoR.P4b.IdleSameDirectionIgnored", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP4bSameDirectionTest::RunTest(const FString& Parameters)
{
	FPoweredWorld F;
	F.Key(F.Gate->CloseKey); F.Poke(F.Gate->CloseButton); F.Gate->Tick(3);
	TestTrue(TEXT("E4 closed stays closed"), F.Gate->GetState() == EVBoRPoweredState::Closed);
	F.Gate->bStartOpen = true; F.Gate->ResetPanel();
	F.Key(F.Gate->OpenKey); F.Poke(F.Gate->OpenButton); F.Gate->Tick(3);
	TestTrue(TEXT("E4 open stays open"), F.Gate->GetState() == EVBoRPoweredState::Open);
	TestEqual(TEXT("E4 no warnings for ignored input"), F.Gate->GetWarningRequests(), 0);
	TestEqual(TEXT("E4 no arrival"), F.Observer->Opened + F.Observer->Closed, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP4bDuplicateTest, "vBoR.P4b.SameFrameButtonKeyboard", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP4bDuplicateTest::RunTest(const FString& Parameters)
{
	FPoweredWorld F;
	F.Poke(F.Gate->OpenButton); F.Key(F.Gate->OpenKey);
	F.Gate->Tick(5);
	TestEqual(TEXT("E5 one open arrival"), F.Observer->Opened, 1);
	TestEqual(TEXT("E5 one waiting transition"), F.Observer->States.FilterByPredicate([](EVBoRPoweredState State) { return State == EVBoRPoweredState::OpeningWait; }).Num(), 1);
	TestEqual(TEXT("E5 exactly one delay and motion"), F.Gate->GetAngleDeg(), 85.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP4bObstructionTest, "vBoR.P4b.MultipleBoxesReverseWithoutPenetration", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP4bObstructionTest::RunTest(const FString& Parameters)
{
	for (bool bLargeTick : {false, true})
	{
		FPoweredWorld F;
		F.Gate->SetActorTransform(FTransform(FRotator(0, 37, 0), FVector(200, -100, 90)));
		F.Gate->bStartOpen = true; F.Gate->ResetPanel();
		AVBoRCarryObject* Inside = F.Box(FVector(45, 0, 100));
		AVBoRCarryObject* Obstacle = F.Box(FVector(-25, 15, 110));
		const FTransform Before = Obstacle->GetActorTransform(), OtherBefore = Inside->GetActorTransform();
		F.Gate->RequestClose();
		F.Gate->Tick(1);
		for (int32 I = 0; I < (bLargeTick ? 1 : 240) && F.Observer->Obstructed == 0; ++I) { F.Gate->Tick(bLargeTick ? 4 : 1.0f / 60); }
		TestEqual(TEXT("E6 any box triggers one obstruction"), F.Observer->Obstructed, 1);
		TestTrue(TEXT("E6 event identifies obstructing box"), F.Observer->HitBox.Get() == Obstacle);
		TestTrue(TEXT("E6 switches directly to opening"), F.Gate->GetState() == EVBoRPoweredState::Opening);
		TestTrue(TEXT("E6 stops before closed"), F.Gate->GetAngleDeg() > 0 && F.Gate->GetAngleDeg() < 85);
		TestTrue(TEXT("E6 accepted panel pose does not penetrate box"), Obstacle->IsPoseClear(Obstacle->GetActorTransform()));
		TestTrue(TEXT("E6 neither box pushed"), Obstacle->GetActorTransform().Equals(Before) && Inside->GetActorTransform().Equals(OtherBefore));
		const int32 Warnings = F.Gate->GetWarningRequests();
		const float Angle = F.Gate->GetAngleDeg();
		F.Gate->Tick(0.25);
		TestTrue(TEXT("E6 reopening has no delay"), F.Gate->GetAngleDeg() > Angle);
		F.Gate->Tick(4);
		TestEqual(TEXT("E6 reopens fully"), F.Observer->Opened, 1);
		TestTrue(TEXT("E6 warnings continue"), F.Gate->GetWarningRequests() > Warnings);
		TestEqual(TEXT("E6 cannot emit closed arrival"), F.Observer->Closed, 0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP4bInsideTest, "vBoR.P4b.InsideBoxesAllowClosing", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP4bInsideTest::RunTest(const FString& Parameters)
{
	FPoweredWorld F;
	F.Gate->bStartOpen = true; F.Gate->ResetPanel();
	AVBoRCarryObject* Box = F.Box(FVector(45, 0, 100));
	const FTransform Before = Box->GetActorTransform();
	F.Gate->RequestClose(); F.Gate->Tick(8);
	TestTrue(TEXT("E7 closed despite inner box"), F.Gate->GetState() == EVBoRPoweredState::Closed);
	TestEqual(TEXT("E7 no obstruction"), F.Observer->Obstructed, 0);
	TestTrue(TEXT("E7 box stays still"), Box->GetActorTransform().Equals(Before));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP4bDelayTest, "vBoR.P4b.ZeroAndTwoSecondDelay", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP4bDelayTest::RunTest(const FString& Parameters)
{
	for (float Delay : {0.0f, 2.0f})
	{
		FPoweredWorld F; F.Gate->DelaySeconds = Delay; F.Gate->RequestOpen();
		F.Gate->Tick(Delay);
		TestEqual(TEXT("E8 exact delay leaves angle zero"), F.Gate->GetAngleDeg(), 0.0f);
		TestTrue(TEXT("E8 movement starts at boundary"), F.Gate->GetState() == EVBoRPoweredState::Opening);
		F.Gate->Tick(0.5);
		TestTrue(TEXT("E8 half second travel"), FMath::IsNearlyEqual(F.Gate->GetAngleDeg(), 10.625f));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP4bRateTest, "vBoR.P4b.LargeTickMatchesSmallTicks", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP4bRateTest::RunTest(const FString& Parameters)
{
	for (float Step : {0.125f, 1.25f, 5.0f})
	{
		FPoweredWorld F;
		F.Gate->RequestOpen();
		for (int32 I = 0; I < FMath::RoundToInt(5 / Step); ++I) { F.Gate->Tick(Step); }
		TestTrue(TEXT("E9 same total time reaches open"), F.Gate->GetState() == EVBoRPoweredState::Open);
		TestEqual(TEXT("E9 exact angle no overshoot"), F.Gate->GetAngleDeg(), 85.0f);
		F.Gate->RequestClose(); F.Gate->Tick(50);
		TestEqual(TEXT("E9 closing cannot overshoot"), F.Gate->GetAngleDeg(), 0.0f);
		TestEqual(TEXT("E9 one closed event"), F.Observer->Closed, 1);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP4bPivotTest, "vBoR.P4b.VehicleOriginHingeAndButtons", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP4bPivotTest::RunTest(const FString& Parameters)
{
	FPoweredWorld F;
	F.Gate->SetActorTransform(FTransform(FRotator(0, 32, 0), FVector(200, -80, 40)));
	F.Gate->HingeLocationCm = FVector(80, 0, 160);
	F.Gate->MeshRestFromRoot = FTransform(FRotator::ZeroRotator, FVector(77.5, 0, 120), FVector(0.05, 1.1, 0.8));
	F.Gate->OnConstruction(F.Gate->GetActorTransform());
	const FTransform Rest = F.Gate->PanelMesh->GetComponentTransform();
	TestTrue(TEXT("E10 preserves rest transform relative to vehicle origin"), Rest.Equals(F.Gate->MeshRestFromRoot * F.Gate->GetActorTransform(), 0.01));
	const FVector Pivot = F.Gate->HingePivot->GetComponentLocation();
	const double Radius = FVector::Distance(Pivot, Rest.GetLocation());
	const FTransform ButtonRest = F.Gate->OpenButton->GetActorTransform().GetRelativeTransform(F.Gate->HingePivot->GetComponentTransform());
	TestTrue(TEXT("E10 button front faces -X"), F.Gate->OpenButtonFromHinge.GetRotation().RotateVector(FVector::UpVector).Equals(-FVector::ForwardVector, 0.001));
	F.Gate->RequestOpen(); F.Gate->Tick(5);
	TestTrue(TEXT("E10 fixed hinge"), F.Gate->HingePivot->GetComponentLocation().Equals(Pivot, 0.01));
	TestTrue(TEXT("E10 rotation about displaced hinge"), FMath::IsNearlyEqual(FVector::Distance(Pivot, F.Gate->PanelMesh->GetComponentLocation()), Radius, 0.01));
	TestFalse(TEXT("E10 mesh moves"), F.Gate->PanelMesh->GetComponentLocation().Equals(Rest.GetLocation(), 0.01));
	TestTrue(TEXT("E10 buttons follow hinge with same relative pose"), F.Gate->OpenButton->GetActorTransform().GetRelativeTransform(F.Gate->HingePivot->GetComponentTransform()).Equals(ButtonRest, 0.01));
	TestTrue(TEXT("E10 both buttons are attached"), F.Gate->CloseButton->GetRootComponent()->GetAttachParent() == F.Gate->HingePivot);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP4bCallbackTest, "vBoR.P4b.PointerDefersMotionAudioAndEvents", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP4bCallbackTest::RunTest(const FString& Parameters)
{
	FPoweredWorld F; F.Gate->DelaySeconds = 0;
	const FTransform Mesh = F.Gate->PanelMesh->GetComponentTransform();
	const bool Active = F.Gate->OpenButton->Poke->IsActive();
	F.Poke(F.Gate->OpenButton);
	TestTrue(TEXT("E11 callback leaves pose unchanged"), F.Gate->PanelMesh->GetComponentTransform().Equals(Mesh));
	TestEqual(TEXT("E11 no audio in callback"), F.Gate->GetWarningRequests(), 0);
	TestEqual(TEXT("E11 no state broadcast in callback"), F.Observer->States.Num(), 0);
	TestEqual(TEXT("E11 SDK poke activation unchanged"), F.Gate->OpenButton->Poke->IsActive(), Active);
	F.Gate->Tick(0.1);
	TestTrue(TEXT("E11 following Tick moves"), F.Gate->GetAngleDeg() > 0);
	TestTrue(TEXT("E11 following Tick requests sound"), F.Gate->GetWarningRequests() > 0);
	TestTrue(TEXT("E11 following Tick broadcasts"), F.Observer->States.Num() > 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP4bGateBlockingTest, "vBoR.P4b.BenchHeldBoxBlockingAndReset", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP4bGateBlockingTest::RunTest(const FString& Parameters)
{
	FPoweredWorld F;
	APlayerController* Controller = F.World->GetFirstPlayerController();
	AVBoRHandRigPawn* Pawn = F.World->SpawnActor<AVBoRHandRigPawn>();
	Controller->Possess(Pawn);
	AP1LoadBench* Bench = F.World->SpawnActor<AP1LoadBench>();
	Bench->PlaceFromHead(FTransform(FRotator(0, 37, 0), FVector(500, 200, 165)));
	AVBoRCarryObject* Box = Bench->Bag;
	AVBoRPoweredPanel* Gate = Bench->Gate;
	if (!TestNotNull(TEXT("E12 gate spawned on P1 bench"), Gate)) { return false; }
	const FTransform Frame = Bench->GetActorTransform();
	FTransform Start = Box->GetActorTransform();
	Start.SetLocation(Frame.TransformPosition(FVector(-40, 0, 110)));
	Box->ResetToPose(Start);
	Box->BeginCarry(0, nullptr, Box->GetActorTransform());
	FTransform Desired = Start; Desired.SetLocation(Frame.TransformPosition(FVector(40, 0, 110)));
	Box->MoveHeldPose(Desired, 1.0f / 60);
	TestTrue(TEXT("E12 closed gate blocks held box"), Box->IsBlocked());
	TestTrue(TEXT("E12 blocked box stays outside"), Frame.InverseTransformPosition(Box->GetActorLocation()).X < -20);
	Gate->RequestOpen(); Gate->Tick(5);
	Box->MoveHeldPose(Desired, 1.0f / 60);
	TestTrue(TEXT("E12 open gate allows entry"), Box->GetActorLocation().Equals(Desired.GetLocation(), 0.02));
	// 손에 든 박스도 닫힘 경로에 있으면 끼임으로 처리함 (P4b D5)
	Box->SetActorLocation(Frame.TransformPosition(FVector(-25, 0, 110)));
	const FTransform Held = Box->GetActorTransform();
	Gate->RequestClose(); Gate->Tick(5);
	TestEqual(TEXT("E12 held box triggers reversal"), Gate->GetObstructionCount(), 1);
	TestTrue(TEXT("E12 held box remains unchanged"), Box->GetActorTransform().Equals(Held));
	Bench->Tick(0);
	TestTrue(TEXT("E12 panel contains gate state and count"), Bench->Panel->Text.ToString().Contains(TEXT("Approximate gate:")) && Bench->Panel->Text.ToString().Contains(TEXT("obstructions=1")));
	Bench->ResetBagForNextTrial();
	TestTrue(TEXT("B restores default closed gate"), Gate->GetState() == EVBoRPoweredState::Closed);
	TestEqual(TEXT("B clears obstruction count"), Gate->GetObstructionCount(), 0);
	Gate->bStartOpen = true;
	Bench->PlaceFromHead(FTransform(FRotator(0, -48, 0), FVector(50, 100, 165)));
	TestTrue(TEXT("R restores configured start state"), Gate->GetState() == EVBoRPoweredState::Open);
	TestTrue(TEXT("R preserves vehicle origin attachment"), Gate->GetActorTransform().Equals(Bench->Trunk->GetActorTransform(), 0.01));
	AVBoRPokeButton* Open = Gate->OpenButton;
	Bench->Destroy();
	TestFalse(TEXT("Bench destruction cleans up gate"), IsValid(Gate));
	TestFalse(TEXT("Gate destruction cleans up buttons"), IsValid(Open));
	return true;
}
#endif
