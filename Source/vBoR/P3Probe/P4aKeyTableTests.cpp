#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "P4aTestFixture.h"
#include "P3VehicleMock.h"
#include "P3FlowGameMode.h"
#include "../Interaction/VBoRKeyTable.h"
#include "../Interaction/VBoRPokeButton.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/WorldSettings.h"
#include "Interaction/IsdkGrabberComponent.h"
#include "Interaction/IsdkPokeInteractable.h"
#include "Interaction/IsdkThrowable.h"
#include "Interaction/Surfaces/IsdkPointablePlane.h"
#include "Interaction/Grabbable/IsdkGrabFreeTransformer.h"
#include "IsdkRuntimeSettings.h"
#include "Misc/OutputDevice.h"
#include "Misc/OutputDeviceRedirector.h"
#include "Rig/IsdkRayInteractionRigComponent.h"

namespace
{
	struct FP4aWorld
	{
		UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
		void Play() { World->InitializeActorsForPlay(FURL()); World->GetWorldSettings()->NotifyBeginPlay(); }
		~FP4aWorld() { World->EndPlay(EEndPlayReason::Quit); World->DestroyWorld(false); }
	};
	AVBoRKeyTable* P4aTable(FP4aWorld& W, FVector Position = FVector::ZeroVector)
	{
		AVBoRKeyTable* Table = W.World->SpawnActor<AVBoRKeyTable>(Position, FRotator::ZeroRotator);
		Table->KeyClass = AP4aTrackedKey::StaticClass(); return Table;
	}
	UIsdkGrabberComponent* P4aGrabber(FP4aWorld& W, int32 Hand)
	{
		AActor* Actor = W.World->SpawnActor<AActor>();
		UIsdkGrabberComponent* Grab = NewObject<UIsdkGrabberComponent>(Actor); Actor->SetRootComponent(Grab);
		Grab->InteractorTags.AddTag(Hand == 0 ? IsdkGameplayTags::TAG_Isdk_Type_Device_Left : IsdkGameplayTags::TAG_Isdk_Type_Device_Right);
		Grab->SetWorldLocation(FVector(1000));
		Grab->SetGrabDetectionTypeAllowed(EIsdkGrabDetectorType::DistanceGrab, false);
		Grab->SetGrabDetectionTypeAllowed(EIsdkGrabDetectorType::RayGrab, false);
		Grab->SetGrabInputMethodAllowed(EIsdkGrabInputMethod::Palm, true);
		Grab->SetGrabInputMethodAllowed(EIsdkGrabInputMethod::Pinch, true);
		Grab->RegisterComponent(); return Grab;
	}
	bool P4aGrip(AVBoRKeyFob* Key, UIsdkGrabberComponent* Grab, bool bPinch = false)
	{
		Grab->Activate(true); Grab->SetWorldLocationAndRotation(Key->GetActorLocation(), Key->GetActorQuat());
		Grab->TickComponent(0.016f, LEVELTICK_All, nullptr);
		if (bPinch) { Grab->PinchGrab(); } else { Grab->PalmGrab(); }
		return Grab->IsGrabbing() && Grab->GetGrabbedComponent() == Key->Grabbable;
	}
	void P4aMove(UIsdkGrabberComponent* Grab, FVector Position, FQuat Rotation = FQuat::Identity)
	{
		Grab->SetWorldLocationAndRotation(Position, Rotation); Grab->TickComponent(0.016f, LEVELTICK_All, nullptr);
	}
	void P4aPoke(AVBoRPokeButton* Button, UIsdkGrabberComponent* Hand)
	{
		FIsdkInteractionPointerEvent Event; Event.Type = EIsdkPointerEventType::Select;
		Event.Interactor = Hand; Event.Interactable = Button->Poke.Get();
		Button->Poke->GetInteractionPointerEventDelegate().Broadcast(Event);
	}
	UP4aKeyObserver* P4aObserve(AVBoRKeyFob* Key, AP3VehicleMock* Car = nullptr)
	{
		UP4aKeyObserver* Observer = NewObject<UP4aKeyObserver>(Key);
		Key->OnUnlockPressed.AddDynamic(Observer, &UP4aKeyObserver::Unlock);
		Key->OnLockPressed.AddDynamic(Observer, &UP4aKeyObserver::Lock);
		Key->UnlockButton->OnButtonPressed.AddDynamic(Observer, &UP4aKeyObserver::Button);
		Key->LockButton->OnButtonPressed.AddDynamic(Observer, &UP4aKeyObserver::Button);
		if (Car)
		{
			Car->OnKeyUnlockPressed.AddDynamic(Observer, &UP4aKeyObserver::CarUnlock);
			Car->OnKeyLockPressed.AddDynamic(Observer, &UP4aKeyObserver::CarLock);
		}
		return Observer;
	}
	struct FP4aLogs : FOutputDevice
	{
		TArray<FString> Lines;
		FP4aLogs() { GLog->FlushThreadedLogs(); GLog->AddOutputDevice(this); }
		~FP4aLogs() { GLog->RemoveOutputDevice(this); }
		virtual void Serialize(const TCHAR* Text, ELogVerbosity::Type Verbosity, const FName& Category) override
		{ if (Category == TEXT("LogKeyFob")) { Lines.Add(Text); } }
		bool Contains(const TCHAR* Text) { GLog->FlushThreadedLogs(); for (const FString& Line : Lines) { if (Line.Contains(Text)) { return true; } } return false; }
	};
	void P4aThrowSamples(FP4aWorld& W, AP4aTrackedKey* Key, UIsdkGrabberComponent* Grab, int32 LowFrame = INDEX_NONE, int32 Frames = 12)
	{
		const FVector Start = Grab->GetComponentLocation();
		for (int32 I = 0; I < Frames; ++I)
		{
			Key->bHigh[Key->GetHoldingHand()] = I != LowFrame;
			P4aMove(Grab, Start + FVector(I * 2, I, I * 0.5), FQuat(FVector::UpVector, I * 0.04));
			// frame 시간만 합성하고 SDK 표본·Actor 추적 창을 같은 순서로 갱신함 (P4a E6·E7)
			W.World->TimeSeconds += 0.016;
			Key->Tick(0.016f);
			static_cast<UActorComponent*>(Key->FindComponentByClass<UIsdkThrowable>())->TickComponent(0.016f, LEVELTICK_All, nullptr);
		}
		Key->bHigh[Key->GetHoldingHand()] = true;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP4aPalm, "vBoR.P4a.Key.E1PalmFreePose", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP4aPalm::RunTest(const FString&)
{
	FP4aWorld W; AVBoRKeyTable* Table = P4aTable(W); UIsdkGrabberComponent* Grab = P4aGrabber(W,0); W.Play();
	AVBoRKeyFob* Key = Table->Key; const FVector Scale = Key->GetActorScale3D(); const FVector Start = Key->GetActorLocation();
	if (!TestTrue(TEXT("Real SDK palm selects table key"), P4aGrip(Key,Grab))) { return false; }
	TestEqual(TEXT("Left hand recorded"), Key->GetHoldingHand(),0); TestFalse(TEXT("SDK disables physics while held"), Key->KeyMesh->IsSimulatingPhysics());
	const FVector Delta(30,20,15); const FQuat Rotation(FVector::YAxisVector,PI/3);
	P4aMove(Grab,Start+Delta,Rotation);
	TestTrue(TEXT("All translation axes follow"), Key->GetActorLocation().Equals(Start+Delta,0.01));
	TestTrue(TEXT("Free rotation follows"), Key->GetActorQuat().Equals(Rotation,0.001));
	TestTrue(TEXT("Scale unchanged"), Key->GetActorScale3D().Equals(Scale));
	TestTrue(TEXT("Buttons follow held root"), Key->UnlockButton->GetActorLocation().Equals(Key->GetActorLocation()+Rotation.RotateVector(Key->UnlockButtonCm),0.01));
	TestTrue(TEXT("Button plane follows rotation"), Key->UnlockButton->Plane->GetNormal().GetSafeNormal().Equals(Rotation.RotateVector(FVector::UpVector),0.001));
	Grab->PalmRelease(); TestTrue(TEXT("SDK restores physics"), Key->KeyMesh->IsSimulatingPhysics()); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP4aPinch, "vBoR.P4a.Key.E1PinchAndP3Rig", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP4aPinch::RunTest(const FString&)
{
	FP4aWorld W; AVBoRKeyTable* Table = P4aTable(W); UIsdkGrabberComponent* Grab = P4aGrabber(W,1);
	AP3PalmGrabPawn* Pawn = W.World->SpawnActor<AP3PalmGrabPawn>(); W.Play();
	AVBoRKeyFob* Key = Table->Key; const FVector Start = Key->GetActorLocation(), Scale = Key->GetActorScale3D();
	if (!TestTrue(TEXT("Real SDK pinch selects table key"),P4aGrip(Key,Grab,true))) { return false; }
	const FQuat Rotation(FVector::XAxisVector,PI/4); P4aMove(Grab,Start+FVector(10,-15,20),Rotation);
	TestTrue(TEXT("Pinch follows position"),Key->GetActorLocation().Equals(Start+FVector(10,-15,20),0.01));
	TestTrue(TEXT("Pinch follows rotation"),Key->GetActorQuat().Equals(Rotation,0.001));
	TestTrue(TEXT("Pinch scale unchanged"),Key->GetActorScale3D().Equals(Scale));
	for (int32 H=0; H<2; ++H)
	{
		UIsdkGrabberComponent* RigGrab = Pawn->GetHandRig(H)->GetGrabber();
		TestTrue(TEXT("Actual P3 rig enables pinch for key"),RigGrab->IsGrabInputMethodAllowed(EIsdkGrabInputMethod::Pinch));
		TestFalse(TEXT("Distance remains blocked"),RigGrab->IsGrabDetectionTypeAllowed(EIsdkGrabDetectorType::DistanceGrab));
		TestFalse(TEXT("Ray remains blocked"),RigGrab->IsGrabDetectionTypeAllowed(EIsdkGrabDetectorType::RayGrab));
		TestFalse(TEXT("Ray rig inactive"),Pawn->GetHandRig(H)->GetRayInteraction()->IsActive());
	}
	Grab->PinchRelease(); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP4aButtons, "vBoR.P4a.Key.E2E5OtherHandAndRepeatedEvents", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP4aButtons::RunTest(const FString&)
{
	FP4aWorld W; AP3VehicleMock* Car = W.World->SpawnActor<AP3VehicleMock>(); UIsdkGrabberComponent* Left = P4aGrabber(W,0); UIsdkGrabberComponent* Right = P4aGrabber(W,1); W.Play();
	AVBoRKeyFob* Key = Car->FrontKeyTable->Key; UP4aKeyObserver* O = P4aObserve(Key,Car);
	if (!TestTrue(TEXT("Grab key"),P4aGrip(Key,Left))) { return false; }
	Key->Tick(0.016f);
	P4aPoke(Key->UnlockButton,Right); P4aPoke(Key->LockButton,Right);
	TestEqual(TEXT("One unlock on key"),O->Unlocks,1); TestEqual(TEXT("One lock on key"),O->Locks,1);
	TestEqual(TEXT("One unlock on car"),O->CarUnlocks,1); TestEqual(TEXT("One lock on car"),O->CarLocks,1);
	TestTrue(TEXT("Both events carry pressed key"),O->CarKeys.Num()==2 && O->CarKeys[0]==Key && O->CarKeys[1]==Key);
	P4aPoke(Key->UnlockButton,Right); P4aPoke(Key->LockButton,Right);
	TestEqual(TEXT("Repeated unlock remains event"),O->Unlocks,2); TestEqual(TEXT("Repeated lock remains event"),O->Locks,2);
	TestEqual(TEXT("Repeated car unlock"),O->CarUnlocks,2); TestEqual(TEXT("Repeated car lock"),O->CarLocks,2);
	TestEqual(TEXT("Four accepted button events"),O->ButtonPresses,4); Left->PalmRelease(); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP4aHoldingPoke, "vBoR.P4a.Key.E3HoldingHandIgnored", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP4aHoldingPoke::RunTest(const FString&)
{
	FP4aWorld W; AVBoRKeyTable* Table = P4aTable(W); UIsdkGrabberComponent* Left = P4aGrabber(W,0); W.Play(); UP4aKeyObserver* O = P4aObserve(Table->Key);
	if (!TestTrue(TEXT("Grab key"),P4aGrip(Table->Key,Left))) { return false; }
	Table->Key->Tick(0.016f);
	P4aPoke(Table->Key->UnlockButton,Left); P4aPoke(Table->Key->LockButton,Left);
	TestEqual(TEXT("Holding hand cannot unlock"),O->Unlocks,0); TestEqual(TEXT("Holding hand cannot lock"),O->Locks,0);
	TestEqual(TEXT("No ButtonPressed for scenario either"),O->ButtonPresses,0); Left->PalmRelease(); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP4aTablePoke, "vBoR.P4a.Key.E4UnheldKeyIgnored", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP4aTablePoke::RunTest(const FString&)
{
	FP4aWorld W; AVBoRKeyTable* Table = P4aTable(W); UIsdkGrabberComponent* Right = P4aGrabber(W,1); W.Play(); UP4aKeyObserver* O = P4aObserve(Table->Key);
	AVBoRKeyFob* Key = Table->Key;
	TestFalse(TEXT("Unheld unlock poke inactive at BeginPlay"),Key->UnlockButton->Poke->IsActive());
	TestFalse(TEXT("Unheld lock poke inactive at BeginPlay"),Key->LockButton->Poke->IsActive());
	P4aPoke(Table->Key->UnlockButton,Right); P4aPoke(Table->Key->LockButton,Right);
	TestEqual(TEXT("Unheld unlock ignored"),O->Unlocks,0); TestEqual(TEXT("Unheld lock ignored"),O->Locks,0);
	TestEqual(TEXT("No unheld ButtonPressed"),O->ButtonPresses,0);
	if (!TestTrue(TEXT("Grab key before deferred activation"),P4aGrip(Key,Right))) { return false; }
	TestFalse(TEXT("Select callback leaves unlock poke inactive"),Key->UnlockButton->Poke->IsActive());
	TestFalse(TEXT("Select callback leaves lock poke inactive"),Key->LockButton->Poke->IsActive());
	Key->Tick(0.016f);
	TestTrue(TEXT("Held unlock poke active after Tick"),Key->UnlockButton->Poke->IsActive());
	TestTrue(TEXT("Held lock poke active after Tick"),Key->LockButton->Poke->IsActive());
	Right->PalmRelease();
	TestTrue(TEXT("Release callback defers unlock deactivation"),Key->UnlockButton->Poke->IsActive());
	TestTrue(TEXT("Release callback defers lock deactivation"),Key->LockButton->Poke->IsActive());
	P4aPoke(Key->UnlockButton,Right); P4aPoke(Key->LockButton,Right);
	TestEqual(TEXT("Filter rejects events before deferred deactivation"),O->ButtonPresses,0);
	Key->Tick(0.016f);
	TestFalse(TEXT("Released unlock poke inactive after Tick"),Key->UnlockButton->Poke->IsActive());
	TestFalse(TEXT("Released lock poke inactive after Tick"),Key->LockButton->Poke->IsActive()); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP4aThrowHigh, "vBoR.P4a.Key.E6HighThrowVelocity", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP4aThrowHigh::RunTest(const FString&)
{
	FP4aWorld W; AVBoRKeyTable* Table = P4aTable(W); UIsdkGrabberComponent* Grab = P4aGrabber(W,0); W.Play(); AP4aTrackedKey* Key = CastChecked<AP4aTrackedKey>(Table->Key);
	if (!TestTrue(TEXT("Grab key"),P4aGrip(Key,Grab))) { return false; }
	P4aThrowSamples(W,Key,Grab); const FVector SDKVelocity = Key->FindComponentByClass<UIsdkThrowable>()->GetVelocity();
	TestTrue(TEXT("Actual SDK has throw samples"),SDKVelocity.X > 0 && SDKVelocity.Y > 0 && SDKVelocity.Z > 0);
	Grab->PalmRelease();
	TestTrue(TEXT("Physics restored"),Key->KeyMesh->IsSimulatingPhysics()); TestTrue(TEXT("Gravity restored"),Key->KeyMesh->IsGravityEnabled());
	TestTrue(TEXT("High release keeps SDK velocity"),Key->KeyMesh->GetPhysicsLinearVelocity().Equals(SDKVelocity,0.01));
	TestEqual(TEXT("Released hand cleared"),Key->GetHoldingHand(),INDEX_NONE); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP4aThrowLow, "vBoR.P4a.Key.E7LowInSampleWindow", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP4aThrowLow::RunTest(const FString&)
{
	FP4aWorld W; AVBoRKeyTable* Table = P4aTable(W); UIsdkGrabberComponent* Grab = P4aGrabber(W,0); W.Play(); AP4aTrackedKey* Key = CastChecked<AP4aTrackedKey>(Table->Key);
	if (!TestTrue(TEXT("Grab key"),P4aGrip(Key,Grab))) { return false; }
	P4aThrowSamples(W,Key,Grab,8); FP4aLogs Logs;
	TestFalse(TEXT("SDK would throw despite Low frame"),Key->FindComponentByClass<UIsdkThrowable>()->GetVelocity().IsNearlyZero());
	Grab->PalmRelease();
	TestTrue(TEXT("Physics restored after Low"),Key->KeyMesh->IsSimulatingPhysics());
	TestTrue(TEXT("Low-window linear velocity zero"),Key->KeyMesh->GetPhysicsLinearVelocity().IsNearlyZero());
	TestTrue(TEXT("Low-window angular velocity zero"),Key->KeyMesh->GetPhysicsAngularVelocityInDegrees().IsNearlyZero());
	TestTrue(TEXT("Reason logged"),Logs.Contains(TEXT("non-High frame in throw sample window"))); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP4aThrowWindow, "vBoR.P4a.Key.E7SampleSizeAndExpiredLow", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP4aThrowWindow::RunTest(const FString&)
{
	FP4aWorld W; AVBoRKeyTable* Table = P4aTable(W); UIsdkGrabberComponent* Grab = P4aGrabber(W,0); W.Play(); AP4aTrackedKey* Key = CastChecked<AP4aTrackedKey>(Table->Key);
	UIsdkThrowable* Throwable = Key->FindComponentByClass<UIsdkThrowable>(); FIsdkThrowableSettings Settings; Throwable->GetThrowableSettings(Settings);
	Settings.SampleSize = 3; Throwable->SetThrowableSettings(Settings);
	if (!TestTrue(TEXT("Grab key"),P4aGrip(Key,Grab))) { return false; }
	P4aThrowSamples(W,Key,Grab,1,7); Grab->PalmRelease();
	TestFalse(TEXT("Low older than configured window expires"),Key->KeyMesh->GetPhysicsLinearVelocity().IsNearlyZero());
	if (!TestTrue(TEXT("Regrab same key"),P4aGrip(Key,Grab))) { return false; }
	P4aThrowSamples(W,Key,Grab,5,7); Grab->PalmRelease();
	TestTrue(TEXT("Low inside configured window zeros velocity"),Key->KeyMesh->GetPhysicsLinearVelocity().IsNearlyZero()); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP4aLostRelease, "vBoR.P4a.Key.E7bLostSelectionDrops", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP4aLostRelease::RunTest(const FString&)
{
	FP4aWorld W; AVBoRKeyTable* Table = P4aTable(W); UIsdkGrabberComponent* Left = P4aGrabber(W,0); UIsdkGrabberComponent* Right = P4aGrabber(W,1); W.Play();
	AP4aTrackedKey* Key = CastChecked<AP4aTrackedKey>(Table->Key); UP4aKeyObserver* O = P4aObserve(Key);
	if (!TestTrue(TEXT("Grab key"),P4aGrip(Key,Left))) { return false; }
	P4aThrowSamples(W,Key,Left); Key->bHigh[0] = false; FP4aLogs Logs;
	TestTrue(TEXT("Held unlock poke active before tracking loss"),Key->UnlockButton->Poke->IsActive());
	TestTrue(TEXT("Held lock poke active before tracking loss"),Key->LockButton->Poke->IsActive());
	// 추적 손실로 끝난 선택도 같은 Unselect 경로. 마지막 Low frame 은 Tick 전에도 검사함 (P4a E7b)
	Left->PalmRelease();
	TestTrue(TEXT("Tracking-loss release simulates physics"),Key->KeyMesh->IsSimulatingPhysics());
	TestTrue(TEXT("Tracking-loss release has zero velocity"),Key->KeyMesh->GetPhysicsLinearVelocity().IsNearlyZero());
	TestTrue(TEXT("Tracking-loss angular velocity zero"),Key->KeyMesh->GetPhysicsAngularVelocityInDegrees().IsNearlyZero());
	TestEqual(TEXT("No held hand after SDK termination"),Key->GetHoldingHand(),INDEX_NONE);
	TestEqual(TEXT("SDK selection empty"),Key->GrabComponent->GetNumGrabbers(),0);
	Key->Tick(0.016f);
	TestFalse(TEXT("Lost selection disables unlock poke on Tick"),Key->UnlockButton->Poke->IsActive());
	TestFalse(TEXT("Lost selection disables lock poke on Tick"),Key->LockButton->Poke->IsActive());
	P4aPoke(Key->UnlockButton,Right); TestEqual(TEXT("No button after lost selection"),O->Unlocks,0);
	TestTrue(TEXT("Loss reason logged"),Logs.Contains(TEXT("tracking lost or Low"))); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP4aReturn, "vBoR.P4a.Key.E8SameActorOwnTableAndEvent", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP4aReturn::RunTest(const FString&)
{
	FP4aWorld W; AVBoRKeyTable* Front = P4aTable(W,FVector(330,-90,25)); AVBoRKeyTable* Rear = P4aTable(W,FVector(-330,-90,25));
	UIsdkGrabberComponent* Left = P4aGrabber(W,0); UIsdkGrabberComponent* Right = P4aGrabber(W,1); W.Play();
	AVBoRKeyFob* Keys[] = {Front->Key.Get(),Rear->Key.Get()}; AVBoRKeyTable* Tables[] = {Front,Rear};
	for (int32 I=0; I<2; ++I)
	{
		AVBoRKeyFob* Key = Keys[I]; UP4aKeyObserver* O = P4aObserve(Key); const FTransform Home = Tables[I]->GetKeyHomeTransform(Key);
		AVBoRPokeButton* OriginalButton = Key->UnlockButton;
		Key->SetActorLocation(FVector(0,0,Tables[I]->GetActorLocation().Z+Key->FloorMarginCm),false,nullptr,ETeleportType::TeleportPhysics);
		Key->KeyMesh->SetPhysicsLinearVelocity(FVector(100,200,-300)); Key->KeyMesh->SetPhysicsAngularVelocityInDegrees(FVector(20,30,40)); Key->Tick(0.016f);
		TestTrue(TEXT("Same key Actor retained"),Tables[I]->Key==Keys[I]); TestTrue(TEXT("Returns to own home"),Key->GetActorTransform().Equals(Home,0.001));
		TestTrue(TEXT("Same button Actor retained"),Key->UnlockButton==OriginalButton);
		TestTrue(TEXT("Return clears linear velocity"),Key->KeyMesh->GetPhysicsLinearVelocity().IsNearlyZero());
		TestTrue(TEXT("Return clears angular velocity"),Key->KeyMesh->GetPhysicsAngularVelocityInDegrees().IsNearlyZero());
		TestFalse(TEXT("Returned unlock poke inactive"),Key->UnlockButton->Poke->IsActive());
		TestFalse(TEXT("Returned lock poke inactive"),Key->LockButton->Poke->IsActive());
		// 끝으로 세운 키는 원점이 3 cm 높아도 아랫면이 바닥에 닿음 (P4a R1-3)
		Key->SetActorRotation(FRotator(90,0,0),ETeleportType::TeleportPhysics);
		Key->SetActorLocation(FVector(0,0,Tables[I]->GetActorLocation().Z+3),false,nullptr,ETeleportType::TeleportPhysics);
		TestTrue(TEXT("Upright pivot above floor margin"),Key->GetActorLocation().Z > Tables[I]->GetActorLocation().Z+Key->FloorMarginCm);
		TestTrue(TEXT("Upright mesh bottom touches floor"),FMath::IsNearlyEqual(Key->KeyMesh->Bounds.GetBox().Min.Z,Tables[I]->GetActorLocation().Z,0.001));
		Key->Tick(0.016f);
		TestTrue(TEXT("Upright floor contact returns same key home"),Tables[I]->Key==Key && Key->GetActorTransform().Equals(Home,0.001));
		TestFalse(TEXT("Upright return keeps unlock poke inactive"),Key->UnlockButton->Poke->IsActive());
		TestFalse(TEXT("Upright return keeps lock poke inactive"),Key->LockButton->Poke->IsActive());
		if (!TestTrue(TEXT("Real SDK regrab returned key"),P4aGrip(Key,Left))) { return false; }
		Key->Tick(0.016f);
		P4aPoke(Key->UnlockButton,Right); TestEqual(TEXT("Previously bound event survives"),O->Unlocks,1); Left->PalmRelease();
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP4aHeldFloor, "vBoR.P4a.Key.E9HeldBelowFloorStays", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP4aHeldFloor::RunTest(const FString&)
{
	FP4aWorld W; AVBoRKeyTable* Table = P4aTable(W); UIsdkGrabberComponent* Grab = P4aGrabber(W,0); W.Play();
	AVBoRKeyFob* Key = Table->Key; if (!TestTrue(TEXT("Grab key"),P4aGrip(Key,Grab))) { return false; }
	const FVector Low(30,0,-10); P4aMove(Grab,Low); Key->Tick(0.016f);
	TestTrue(TEXT("Held below floor does not return"),Key->GetActorLocation().Equals(Low,0.01));
	TestEqual(TEXT("Still holding"),Key->GetHoldingHand(),0); Grab->PalmRelease(); Key->Tick(0.016f);
	TestTrue(TEXT("After release below floor returns"),Key->GetActorTransform().Equals(Table->GetKeyHomeTransform(Key),0.01)); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP4aHighRest, "vBoR.P4a.Key.E10AboveFloorStaysAndRegrabs", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP4aHighRest::RunTest(const FString&)
{
	FP4aWorld W; AVBoRKeyTable* Table = P4aTable(W); UIsdkGrabberComponent* Grab = P4aGrabber(W,0); W.Play();
	AVBoRKeyFob* Key = Table->Key; const FTransform Rest(FQuat(FVector::UpVector,0.7),FVector(100,120,50),Key->GetActorScale3D());
	Key->SetActorTransform(Rest,false,nullptr,ETeleportType::TeleportPhysics); Key->Tick(0.016f);
	TestTrue(TEXT("Above-floor rest unchanged"),Key->GetActorTransform().Equals(Rest));
	TestTrue(TEXT("Can grab from above-floor rest"),P4aGrip(Key,Grab)); Grab->PalmRelease(); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP4aTransfer, "vBoR.P4a.Key.E11SDKTransferAndButtonHand", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP4aTransfer::RunTest(const FString&)
{
	FP4aWorld W; AVBoRKeyTable* Table = P4aTable(W); UIsdkGrabberComponent* Left = P4aGrabber(W,0); UIsdkGrabberComponent* Right = P4aGrabber(W,1); W.Play();
	AVBoRKeyFob* Key = Table->Key; UP4aKeyObserver* O = P4aObserve(Key); const FVector Scale = Key->GetActorScale3D();
	if (!TestTrue(TEXT("First hand grabs"),P4aGrip(Key,Left))) { return false; }
	Key->Tick(0.016f);
	if (!TestTrue(TEXT("Second hand takes real SDK grab"),P4aGrip(Key,Right))) { return false; }
	Key->Tick(0.016f);
	TestFalse(TEXT("Old SDK grabber released"),Left->IsGrabbing()); TestEqual(TEXT("New holding hand"),Key->GetHoldingHand(),1);
	TestEqual(TEXT("Only one selected pose"),Key->GrabComponent->GetNumGrabbers(),1); TestTrue(TEXT("No transfer scaling"),Key->GetActorScale3D().Equals(Scale));
	P4aPoke(Key->UnlockButton,Right); TestEqual(TEXT("New holding hand ignored"),O->Unlocks,0);
	P4aPoke(Key->UnlockButton,Left); TestEqual(TEXT("Old hand now presses"),O->Unlocks,1);
	const FVector Position = Key->GetActorLocation()+FVector(-20,10,12); const FQuat Rotation(FVector::XAxisVector,PI/5);
	P4aMove(Right,Position,Rotation); TestTrue(TEXT("New hand drives pose"),Key->GetActorLocation().Equals(Position,0.01) && Key->GetActorQuat().Equals(Rotation,0.001));
	Right->PalmRelease(); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP4aPlacement, "vBoR.P4a.P3.E12StartTablesAndUniqueTags", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP4aPlacement::RunTest(const FString&)
{
	FP4aWorld W; AP3VehicleMock* Car = W.World->SpawnActor<AP3VehicleMock>(FVector(400,70,20),FRotator(0,30,0)); W.Play();
	UStaticMeshComponent* Marker = CastChecked<UStaticMeshComponent>(Car->GetDefaultSubobjectByName(TEXT("StartMarker")));
	UWidgetComponent* Label = CastChecked<UWidgetComponent>(Car->GetDefaultSubobjectByName(TEXT("ApproximationLabel")));
	TestTrue(TEXT("Start lies ahead of nose by existing distance"),Marker->GetRelativeLocation().Equals(FVector(Car->BodySizeCm.X/2+Car->StartOutsideCm,0,0.5)));
	TestTrue(TEXT("Start faces car -X"),Marker->GetForwardVector().Equals(-Car->GetActorForwardVector(),0.001));
	TestTrue(TEXT("Label on front side"),Label->GetRelativeLocation().X > Car->BodySizeCm.X/2);
	TestTrue(TEXT("Label faces front start"),Label->GetForwardVector().Equals(Car->GetActorForwardVector(),0.001));
	TestTrue(TEXT("Front table configured placement"),Car->FrontKeyTable->GetActorLocation().Equals(Car->GetActorTransform().TransformPosition(Car->FrontKeyTableCm),0.01));
	TestTrue(TEXT("Rear table configured placement"),Car->RearKeyTable->GetActorLocation().Equals(Car->GetActorTransform().TransformPosition(Car->RearKeyTableCm),0.01));
	TestTrue(TEXT("Front table off walking line"),FMath::Abs(Car->FrontKeyTableCm.Y) > Car->FrontKeyTable->TableSizeCm.Y/2);
	const FName Tags[] = {TEXT("Car.FrontKey.Unlock"),TEXT("Car.FrontKey.Lock"),TEXT("Car.RearKey.Unlock"),TEXT("Car.RearKey.Lock")};
	for (FName Tag : Tags)
	{
		int32 Matches = 0; for (TActorIterator<AVBoRPokeButton> It(W.World); It; ++It) { if (It->ActorHasTag(Tag)) { ++Matches; } }
		TestEqual(TEXT("Each key button tag matches once"),Matches,1);
	}
	TestTrue(TEXT("Two independent homes"),Car->FrontKeyTable->Key->HomeTable==Car->FrontKeyTable && Car->RearKeyTable->Key->HomeTable==Car->RearKeyTable);
	AVBoRKeyTable* Front = Car->FrontKeyTable; AVBoRKeyFob* Key = Front->Key; AVBoRPokeButton* Button = Key->UnlockButton;
	Car->Destroy(); TestTrue(TEXT("Car destroys table"),Front->IsActorBeingDestroyed()); TestTrue(TEXT("Table destroys key"),Key->IsActorBeingDestroyed());
	TestTrue(TEXT("Key destroys button"),Button->IsActorBeingDestroyed()); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP4aBothKeys, "vBoR.P4a.P3.E13FrontAndRearRelay", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP4aBothKeys::RunTest(const FString&)
{
	FP4aWorld W; AP3VehicleMock* Car = W.World->SpawnActor<AP3VehicleMock>(); UIsdkGrabberComponent* Left = P4aGrabber(W,0); UIsdkGrabberComponent* Right = P4aGrabber(W,1); W.Play();
	UP4aKeyObserver* O = P4aObserve(Car->FrontKeyTable->Key,Car);
	for (AVBoRKeyFob* Key : {Car->FrontKeyTable->Key.Get(),Car->RearKeyTable->Key.Get()})
	{
		if (!TestTrue(TEXT("Grab front/rear key"),P4aGrip(Key,Left))) { return false; }
		Key->Tick(0.016f);
		P4aPoke(Key->UnlockButton,Right); P4aPoke(Key->LockButton,Right); Left->PalmRelease();
	}
	TestEqual(TEXT("Car receives each unlock"),O->CarUnlocks,2); TestEqual(TEXT("Car receives each lock"),O->CarLocks,2);
	TestTrue(TEXT("Car receives distinct keys"),O->CarKeys.Num()==4 && O->CarKeys[0]==Car->FrontKeyTable->Key && O->CarKeys[2]==Car->RearKeyTable->Key);
	return true;
}
#endif
