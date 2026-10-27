#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "P5zcTestButton.h"
#include "P4aTestFixture.h"
#include "../P1Probe/P4bTestObserver.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "Interaction/IsdkGrabberComponent.h"
#include "Interaction/IsdkPokeInteractable.h"
#include "IsdkRuntimeSettings.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace
{
	struct FP5zcWorld
	{
		UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
		void Play() { World->InitializeActorsForPlay(FURL()); World->GetWorldSettings()->NotifyBeginPlay(); }
		~FP5zcWorld() { World->EndPlay(EEndPlayReason::Quit); World->DestroyWorld(false); }
	};
	void Poke(AVBoRPokeButton* Button, UIsdkGrabberComponent* Hand = nullptr)
	{
		FIsdkInteractionPointerEvent Event; Event.Type = EIsdkPointerEventType::Select;
		Event.Interactor = Hand; Event.Interactable = Button->Poke.Get();
		Button->Poke->GetInteractionPointerEventDelegate().Broadcast(Event);
	}
	UIsdkGrabberComponent* MakeHand(FP5zcWorld& W, bool bLeft)
	{
		AActor* Actor = W.World->SpawnActor<AActor>();
		UIsdkGrabberComponent* Hand = NewObject<UIsdkGrabberComponent>(Actor); Actor->SetRootComponent(Hand);
		Hand->InteractorTags.AddTag(bLeft ? IsdkGameplayTags::TAG_Isdk_Type_Device_Left : IsdkGameplayTags::TAG_Isdk_Type_Device_Right);
		Hand->SetWorldLocation(FVector(1000));
		Hand->SetGrabDetectionTypeAllowed(EIsdkGrabDetectorType::DistanceGrab, false);
		Hand->SetGrabDetectionTypeAllowed(EIsdkGrabDetectorType::RayGrab, false);
		Hand->SetGrabInputMethodAllowed(EIsdkGrabInputMethod::Palm, true);
		Hand->RegisterComponent(); return Hand;
	}
	void ChooseKeyButtons(AVBoRKeyFob* Key)
	{
		Key->UnlockButtonClass = AP5zcTestButton::StaticClass();
		Key->LockButtonClass = AP5zcTestButton::StaticClass();
	}
	void CheckCustom(FAutomationTestBase& Test, AVBoRPokeButton* Button)
	{
		const AP5zcTestButton* Defaults = GetDefault<AP5zcTestButton>();
		Test.TestTrue(TEXT("Selected class retained"), Button->GetClass() == AP5zcTestButton::StaticClass());
		Test.TestTrue(TEXT("Mesh scale retained"), Button->ButtonMesh->GetComponentScale().Equals(FVector(0.07, 0.05, 0.02), 0.0001));
		Test.TestEqual(TEXT("Mesh asset retained"), Button->ButtonMesh->GetStaticMesh(), Defaults->ButtonMesh->GetStaticMesh());
		Test.TestEqual(TEXT("Press depth retained, mm"), Button->PressDepthMm, 4.0f);
		Test.TestEqual(TEXT("Hit area retained"), Button->HitAreaScale, 1.5f);
		Test.TestTrue(TEXT("Material retained"), Button->ButtonMesh->GetMaterial(0) == Defaults->ButtonMesh->GetMaterial(0));
		Test.TestFalse(TEXT("No parent color material"), Button->ButtonMesh->GetMaterial(0)->IsA<UMaterialInstanceDynamic>());
	}
	void CheckGatePose(FAutomationTestBase& Test, AVBoRPoweredPanel* Gate)
	{
		for (bool bOpen : {true, false})
		{
			AVBoRPokeButton* Button = bOpen ? Gate->OpenButton.Get() : Gate->CloseButton.Get();
			const FTransform Relative = bOpen ? Gate->OpenButtonFromHinge : Gate->CloseButtonFromHinge;
			Test.TestTrue(TEXT("Button attached to hinge"), Button->GetRootComponent()->GetAttachParent() == Gate->HingePivot);
			Test.TestTrue(TEXT("Hinge-relative pose retained"), Button->GetActorTransform().GetRelativeTransform(Gate->HingePivot->GetComponentTransform()).Equals(Relative, 0.001));
		}
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5zcE1, "vBoR.P5zc.E1OpenClassOnly", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5zcE1::RunTest(const FString&)
{
	FP5zcWorld W;
	AVBoRPoweredPanel* Gate = W.World->SpawnActor<AVBoRPoweredPanel>(FVector(30, 50, 20), FRotator(0, 35, 0));
	Gate->OpenButtonClass = AP5zcTestButton::StaticClass(); W.Play();
	CheckCustom(*this, Gate->OpenButton);
	TestTrue(TEXT("Empty close class uses base button"), Gate->CloseButton->GetClass() == AVBoRPokeButton::StaticClass());
	TestTrue(TEXT("Default close size, cm to cube scale"), Gate->CloseButton->ButtonMesh->GetComponentScale().Equals(Gate->ButtonSizeCm / 100.0));
	CheckGatePose(*this, Gate); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5zcE2, "vBoR.P5zc.E2GateMotionAndSounds", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5zcE2::RunTest(const FString&)
{
	FP5zcWorld W; AVBoRPoweredPanel* Gate = W.World->SpawnActor<AVBoRPoweredPanel>();
	Gate->OpenButtonClass = Gate->CloseButtonClass = AP5zcTestButton::StaticClass();
	Gate->ButtonPressedCue.StartDelaySeconds = 0.23f; Gate->ButtonReleasedCue = Gate->ButtonPressedCue;
	W.Play();
	UP4bTestObserver* Observer = NewObject<UP4bTestObserver>(Gate);
	Gate->OnOpened.AddDynamic(Observer, &UP4bTestObserver::Open); Gate->OnClosed.AddDynamic(Observer, &UP4bTestObserver::Close);
	for (AVBoRPokeButton* Button : {Gate->OpenButton.Get(), Gate->CloseButton.Get()})
	{
		CheckCustom(*this, Button);
		TestTrue(TEXT("Pressed cue copied"), Button->PressedCue.Sound == Gate->ButtonPressedCue.Sound && Button->PressedCue.StartDelaySeconds == Gate->ButtonPressedCue.StartDelaySeconds);
		TestTrue(TEXT("Released cue copied"), Button->ReleasedCue.Sound == Gate->ButtonReleasedCue.Sound && Button->ReleasedCue.StartDelaySeconds == Gate->ButtonReleasedCue.StartDelaySeconds);
	}
	Poke(Gate->OpenButton); TestTrue(TEXT("Open poke accepted"), Gate->GetState() == EVBoRPoweredState::OpeningWait);
	Gate->Tick(5); TestTrue(TEXT("Gate opens"), Gate->GetState() == EVBoRPoweredState::Open);
	TestEqual(TEXT("Opened event once"), Observer->Opened, 1); CheckGatePose(*this, Gate);
	Poke(Gate->CloseButton); TestTrue(TEXT("Close poke accepted"), Gate->GetState() == EVBoRPoweredState::ClosingWait);
	Gate->Tick(5); TestTrue(TEXT("Gate closes"), Gate->GetState() == EVBoRPoweredState::Closed);
	TestEqual(TEXT("Closed event once"), Observer->Closed, 1); CheckGatePose(*this, Gate); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5zcE3, "vBoR.P5zc.E3KeyClassAndPose", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5zcE3::RunTest(const FString&)
{
	FP5zcWorld W; AVBoRKeyFob* Key = W.World->SpawnActor<AVBoRKeyFob>(FVector(20, 40, 100), FRotator(15, 40, 20));
	ChooseKeyButtons(Key); Key->ButtonPressedCue.StartDelaySeconds = 0.31f; Key->ButtonReleasedCue = Key->ButtonPressedCue; W.Play();
	for (bool bUnlock : {true, false})
	{
		AVBoRPokeButton* Button = bUnlock ? Key->UnlockButton.Get() : Key->LockButton.Get();
		CheckCustom(*this, Button);
		TestTrue(TEXT("Key button attached"), Button->GetRootComponent()->GetAttachParent() == Key->KeyMesh);
		TestTrue(TEXT("Key offset rotated without mesh scale"), Button->GetActorLocation().Equals(Key->GetActorLocation() + Key->GetActorQuat().RotateVector(bUnlock ? Key->UnlockButtonCm : Key->LockButtonCm), 0.001));
		TestTrue(TEXT("Key button rotation retained"), Button->GetActorQuat().Equals(Key->GetActorQuat(), 0.001));
		TestTrue(TEXT("Key pressed cue copied"), Button->PressedCue.Sound == Key->ButtonPressedCue.Sound && Button->PressedCue.StartDelaySeconds == Key->ButtonPressedCue.StartDelaySeconds);
		TestTrue(TEXT("Key released cue copied"), Button->ReleasedCue.Sound == Key->ButtonReleasedCue.Sound && Button->ReleasedCue.StartDelaySeconds == Key->ButtonReleasedCue.StartDelaySeconds);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5zcE4, "vBoR.P5zc.E4HeldKeyOnly", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5zcE4::RunTest(const FString&)
{
	FP5zcWorld W; AP4aTrackedKey* Key = W.World->SpawnActor<AP4aTrackedKey>(); ChooseKeyButtons(Key);
	UIsdkGrabberComponent* Left = MakeHand(W, true); UIsdkGrabberComponent* Right = MakeHand(W, false); W.Play();
	UP4aKeyObserver* Observer = NewObject<UP4aKeyObserver>(Key);
	Key->OnUnlockPressed.AddDynamic(Observer, &UP4aKeyObserver::Unlock); Key->OnLockPressed.AddDynamic(Observer, &UP4aKeyObserver::Lock);
	for (AVBoRPokeButton* Button : {Key->UnlockButton.Get(), Key->LockButton.Get()})
	{
		TestFalse(TEXT("Unheld poke inactive"), Button->Poke->IsActive()); Poke(Button, Right);
	}
	TestEqual(TEXT("Unheld presses ignored"), Observer->Unlocks + Observer->Locks, 0);
	Left->Activate(true); Left->SetWorldLocationAndRotation(Key->GetActorLocation(), Key->GetActorQuat());
	Left->TickComponent(0.016f, LEVELTICK_All, nullptr); Left->PalmGrab();
	if (!TestTrue(TEXT("SDK grabs selected-button key"), Left->IsGrabbing() && Left->GetGrabbedComponent() == Key->Grabbable)) { return false; }
	Key->Tick(0.016f);
	for (AVBoRPokeButton* Button : {Key->UnlockButton.Get(), Key->LockButton.Get()})
	{
		TestTrue(TEXT("Held poke active"), Button->Poke->IsActive()); Poke(Button, Left);
	}
	TestEqual(TEXT("Holding hand ignored"), Observer->Unlocks + Observer->Locks, 0);
	Poke(Key->UnlockButton, Right); Poke(Key->LockButton, Right);
	TestEqual(TEXT("Other hand unlocks once"), Observer->Unlocks, 1); TestEqual(TEXT("Other hand locks once"), Observer->Locks, 1);
	Left->PalmRelease(); Key->Tick(0.016f);
	for (AVBoRPokeButton* Button : {Key->UnlockButton.Get(), Key->LockButton.Get()})
	{
		TestFalse(TEXT("Released poke inactive"), Button->Poke->IsActive()); Poke(Button, Right);
	}
	TestEqual(TEXT("Released unlock ignored"), Observer->Unlocks, 1); TestEqual(TEXT("Released lock ignored"), Observer->Locks, 1); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5zcE5, "vBoR.P5zc.E5UnnamedChildren", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5zcE5::RunTest(const FString&)
{
	FP5zcWorld W; AVBoRPoweredPanel* Gate = W.World->SpawnActor<AVBoRPoweredPanel>();
	Gate->OpenButtonClass = AP5zcTestButton::StaticClass();
	AVBoRKeyFob* Key = W.World->SpawnActor<AVBoRKeyFob>(); ChooseKeyButtons(Key); W.Play();
	TestFalse(TEXT("Selected class has spectator name"), GetDefault<AP5zcTestButton>()->SpectatorName.IsEmpty());
	for (AVBoRPokeButton* Button : {Gate->OpenButton.Get(), Gate->CloseButton.Get(), Key->UnlockButton.Get(), Key->LockButton.Get()})
	{
		TestTrue(TEXT("Spawned child name cleared"), Button->SpectatorName.IsEmpty());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5zcE6, "vBoR.P5zc.E6AbstractFallback", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5zcE6::RunTest(const FString&)
{
	FP5zcWorld W; AVBoRPoweredPanel* Gate = W.World->SpawnActor<AVBoRPoweredPanel>();
	AVBoRKeyFob* Key = W.World->SpawnActor<AVBoRKeyFob>();
	Gate->OpenButtonClass = Gate->CloseButtonClass = AP5zcAbstractButton::StaticClass();
	Key->UnlockButtonClass = Key->LockButtonClass = AP5zcAbstractButton::StaticClass();
	const TCHAR* Slots[] = {TEXT("OpenButtonClass"), TEXT("CloseButtonClass"), TEXT("UnlockButtonClass"), TEXT("LockButtonClass")};
	for (int32 I = 0; I < 4; ++I)
	{
		AddExpectedMessagePlain(FString::Printf(TEXT("%s %s: unusable button class"), I < 2 ? *Gate->GetName() : *Key->GetName(), Slots[I]), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 1);
	}
	// Expected-message checks count each Warning before framework downgrades it (P5zc E6)
	W.Play();
	for (AVBoRPokeButton* Button : {Gate->OpenButton.Get(), Gate->CloseButton.Get(), Key->UnlockButton.Get(), Key->LockButton.Get()})
	{
		TestTrue(TEXT("Abstract class falls back"), Button->GetClass() == AVBoRPokeButton::StaticClass());
	}
	TestEqual(TEXT("Fallback key depth, mm"), Key->UnlockButton->PressDepthMm, 1.0f);
	TestTrue(TEXT("Fallback key color applied"), Key->UnlockButton->ButtonMesh->GetMaterial(0)->IsA<UMaterialInstanceDynamic>()); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5zcE7, "vBoR.P5zc.E7EmptyDefaults", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5zcE7::RunTest(const FString&)
{
	FP5zcWorld W; AVBoRPoweredPanel* Gate = W.World->SpawnActor<AVBoRPoweredPanel>();
	AVBoRKeyFob* Key = W.World->SpawnActor<AVBoRKeyFob>();
	TestTrue(TEXT("All class slots empty by default"), !Gate->OpenButtonClass && !Gate->CloseButtonClass && !Key->UnlockButtonClass && !Key->LockButtonClass); W.Play();
	for (AVBoRPokeButton* Button : {Gate->OpenButton.Get(), Gate->CloseButton.Get(), Key->UnlockButton.Get(), Key->LockButton.Get()})
	{
		const bool bKey = Button->GetOwner() == Key;
		TestTrue(TEXT("Base class retained"), Button->GetClass() == AVBoRPokeButton::StaticClass());
		TestTrue(TEXT("Default size retained"), Button->ButtonMesh->GetComponentScale().Equals((bKey ? Key->ButtonSizeCm : Gate->ButtonSizeCm) / 100.0, 0.0001));
		TestEqual(TEXT("Default depth retained, mm"), Button->PressDepthMm, bKey ? 1.0f : GetDefault<AVBoRPokeButton>()->PressDepthMm);
		if (bKey)
		{
			UMaterialInstanceDynamic* Material = Cast<UMaterialInstanceDynamic>(Button->ButtonMesh->GetMaterial(0));
			if (TestNotNull(TEXT("Default key color material"), Material))
			{
				const FLinearColor Expected = Button == Key->UnlockButton ? FLinearColor(0.1, 0.7, 0.2, 1) : FLinearColor(0.8, 0.15, 0.1, 1);
				TestTrue(TEXT("Default key color retained"), Material->K2_GetVectorParameterValue(TEXT("ColorParam")).Equals(Expected));
			}
		}
	}
	CheckGatePose(*this, Gate); return true;
}
#endif
