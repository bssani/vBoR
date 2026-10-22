#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "P2dTestFixture.h"
#include "../P3Probe/P3VehicleMock.h"
#include "../Interaction/VBoRPokeButton.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "Interaction/IsdkGrabberComponent.h"
#include "Interaction/IsdkGrabbableComponent.h"
#include "Interaction/IsdkPokeInteractable.h"
#include "IsdkFunctionLibrary.h"
#include "IsdkRuntimeSettings.h"

namespace
{
	struct FP2dWorld
	{
		UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
		void Play() { World->InitializeActorsForPlay(FURL()); World->GetWorldSettings()->NotifyBeginPlay(); }
		~FP2dWorld() { World->EndPlay(EEndPlayReason::Quit); World->DestroyWorld(false); }
	};
	FVBoRDoorHand P2dHandAt(FVector Point, FVector Normal = FVector::ZeroVector)
	{
		FVBoRDoorHand Hand; Hand.bTracked = true; Hand.PalmNormal = Normal;
		for (FVector& P : Hand.Points) { P = Point; } return Hand;
	}
	FVBoRDoorHand P2dFace(double Angle, double Y, double X = 30, FVector Normal = FVector(0,-1,0))
	{
		const FQuat R(FVector::ZAxisVector, FMath::DegreesToRadians(Angle));
		return P2dHandAt(R.RotateVector(FVector(X,Y,0)), R.RotateVector(Normal));
	}
	AP2cTrackedPanel* P2dLid(FP2dWorld& W)
	{
		AP2cTrackedPanel* Panel = W.World->SpawnActor<AP2cTrackedPanel>();
		Panel->bUseLatch = true;
		Panel->HingePivot->SetRelativeTransform(FTransform(FQuat(FVector::XAxisVector, PI/2), FVector(-45,0,70)));
		Panel->MeshRestFromRoot = FTransform(FQuat::Identity, FVector(-27.5,0,70), FVector(0.35,0.24,0.03));
		Panel->MaxAngleDeg = 100; Panel->OnConstruction(Panel->GetActorTransform()); return Panel;
	}
	AP2bTrackedPart* P2dConsoleHandle(FP2dWorld& W, AP2cTrackedPanel* Panel, int32 Side = 0)
	{
		auto* Part = W.World->SpawnActorDeferred<AP2bTrackedPart>(AP2bTrackedPart::StaticClass(), Panel->GetActorTransform());
		Part->AxisPivot->SetRelativeLocation(FVector(-9,0,70));
		Part->MeshRestFromRoot = FTransform(FQuat(FVector::XAxisVector, PI/2), FVector(-9,0,70), FVector(0.02,0.03,0.14));
		Part->Movement = EVBoRAxisMovement::Translate; Part->MaxTravelCm = 0.75;
		Part->Axis = EAxis::X; Part->bReverse = true;
		Part->FinishSpawning(Panel->GetActorTransform()); Panel->SetAxisHandle(Side, Part);
		return Part;
	}
	void P2dPress(AVBoRAxisPart* Part, double Cm = 0.75)
	{
		const FVector Rest = Part->PartMesh->GetComponentLocation();
		FP2dTestAccess::Begin(Part, 0, UIsdkFunctionLibrary::MakeGrabPoseStruct(11, Rest, FQuat::Identity));
		FP2dTestAccess::Move(Part, UIsdkFunctionLibrary::MakeGrabPoseStruct(11, Rest - Part->GetActorForwardVector()*Cm, FQuat::Identity));
		Part->Tick(0.01f);
	}
	UP2cEventObserver* P2dObserve(AVBoRHingedPanel* Panel)
	{
		UP2cEventObserver* Observer = NewObject<UP2cEventObserver>(Panel);
		Panel->OnPanelUnlocked.AddDynamic(Observer, &UP2cEventObserver::Unlocked);
		Panel->OnPanelLocked.AddDynamic(Observer, &UP2cEventObserver::Locked); return Observer;
	}
	UIsdkGrabberComponent* P2dGrabber(FP2dWorld& W)
	{
		AActor* Actor = W.World->SpawnActor<AActor>();
		UIsdkGrabberComponent* Grab = NewObject<UIsdkGrabberComponent>(Actor); Actor->SetRootComponent(Grab);
		Grab->InteractorTags.AddTag(IsdkGameplayTags::TAG_Isdk_Type_Device_Left);
		Grab->SetWorldLocation(FVector(300));
		Grab->SetGrabDetectionTypeAllowed(EIsdkGrabDetectorType::DistanceGrab,false);
		Grab->SetGrabDetectionTypeAllowed(EIsdkGrabDetectorType::RayGrab,false);
		Grab->SetGrabInputMethodAllowed(EIsdkGrabInputMethod::Palm,true);
		Grab->SetGrabInputMethodAllowed(EIsdkGrabInputMethod::Pinch,false);
		Grab->RegisterComponent(); return Grab;
	}
	void P2dSelect(UIsdkGrabbableComponent* Grab, UIsdkGrabberComponent* Interactor)
	{
		FIsdkInteractionPointerEvent Event; Event.Type = EIsdkPointerEventType::Select; Event.Interactor = Interactor;
		Event.Interactable = Grab; Grab->GetInteractionPointerEventDelegate().Broadcast(Event);
	}
	void P2dPull(AVBoRAxisPart* Part, double Degrees, int32 Hand = 0)
	{
		const FTransform Rest = Part->AxisPivot->GetComponentTransform();
		const FVector Offset = Part->MeshRestFromRoot.GetLocation() - Part->AxisPivot->GetRelativeLocation();
		const FVector Local = Rest.InverseTransformVectorNoScale(Part->GetActorTransform().TransformVectorNoScale(Offset));
		FP2dTestAccess::Begin(Part, Hand, UIsdkFunctionLibrary::MakeGrabPoseStruct(11, Rest.TransformPosition(Local), FQuat::Identity));
		const FVector Axis = Part->Axis == EAxis::Y ? FVector::YAxisVector : FVector::XAxisVector;
		const FQuat R(Axis, FMath::DegreesToRadians(Degrees * (Part->bReverse ? -1 : 1)));
		FP2dTestAccess::Move(Part, UIsdkFunctionLibrary::MakeGrabPoseStruct(11, Rest.TransformPosition(R.RotateVector(Local)), FQuat::Identity));
	}
	void P2dCopyPart(AVBoRAxisPart* To, const AVBoRAxisPart* From)
	{
		To->Axis = From->Axis; To->bReverse = From->bReverse; To->MeshRestFromRoot = From->MeshRestFromRoot;
		// Preserve rest car-origin coordinates even when mock Actor is hinge child (P2d D5)
		To->AxisPivot->SetRelativeTransform(From->AxisPivot->GetRelativeTransform()); To->OnConstruction(To->GetActorTransform());
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP2dLockedPush, "vBoR.P2d.Panel.E1LockedTopAndBottomPush", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP2dLockedPush::RunTest(const FString&)
{
	FP2dWorld W; AP2cTrackedPanel* Panel = P2dLid(W); Panel->bAllowPalmOpening = false; W.Play();
	Panel->Hands[0] = P2dFace(0,2); Panel->Tick(0.1f); Panel->Hands[0] = P2dFace(0,-2); Panel->Tick(0.1f);
	Panel->Hands[0] = {}; Panel->Tick(0.1f);
	Panel->Hands[0] = P2dFace(0,-2,30,FVector(0,1,0)); Panel->Tick(0.1f);
	Panel->Hands[0] = P2dFace(0,2,30,FVector(0,1,0)); Panel->Tick(0.1f);
	TestEqual(TEXT("Both faces stay closed"), Panel->GetMotion().AngleDeg, 0.0);
	TestTrue(TEXT("Still locked"), Panel->GetMotion().bLocked);
	TestEqual(TEXT("Both locked contacts counted"), Panel->GetMotion().Metrics.Ignored[0], 2); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP2dFrontGrab, "vBoR.P2d.Panel.E3FrontGrabUnlockAndSameHandLift", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP2dFrontGrab::RunTest(const FString&)
{
	FP2dWorld W; AP2cTrackedPanel* Panel = P2dLid(W); auto* Handle = P2dConsoleHandle(W,Panel);
	Panel->bAllowPalmOpening = false; W.Play();
	UP2cEventObserver* Observer = P2dObserve(Panel); UP2dAudioObserver* Audio = FP2dTestAccess::ObserveAudio(Panel);
	Panel->Hands[0] = P2dFace(0,0,36); P2dPress(Handle);
	TestFalse(TEXT("Full handle press unlocks"), Panel->GetMotion().bLocked);
	TestEqual(TEXT("Same hand transfers"), Panel->GetMotion().GrabHand, 0);
	TestEqual(TEXT("Outside is unlock source"), Panel->GetMotion().Metrics.LastUnlockHandle, 0);
	TestEqual(TEXT("One unlock"), Observer->Unlocks, 1); TestEqual(TEXT("Same unlock sound requested"), Audio->Plays, 1);
	TestTrue(TEXT("Handle pressed 0.75 cm"), FMath::IsNearlyEqual(Handle->GetOpenRatio()*Handle->MaxTravelCm,0.75f));
	Panel->Hands[0] = P2dFace(35,0,36); Panel->Tick(1);
	TestTrue(TEXT("Same hand lifts lid 35 deg"), FMath::IsNearlyEqual(Panel->GetMotion().AngleDeg,35.0,0.001));
	TestTrue(TEXT("Press does not accumulate"), FMath::IsNearlyEqual(Handle->GetOpenRatio()*Handle->MaxTravelCm,0.75f)); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP2dRelatch, "vBoR.P2d.Panel.E4PushClosesAndRelatches", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP2dRelatch::RunTest(const FString&)
{
	FP2dWorld W; AP2cTrackedPanel* Panel = P2dLid(W); Panel->bAllowPalmOpening = false;
	auto* Handle = P2dConsoleHandle(W,Panel); W.Play(); UP2cEventObserver* Observer = P2dObserve(Panel);
	Panel->Hands[0] = P2dFace(0,0); P2dPress(Handle);
	Panel->Hands[0] = P2dFace(40,0); Panel->Tick(1);
	Panel->bPalmClosed[0] = false; Panel->Tick(0.1f);
	TestTrue(TEXT("Opening arms latch"), Panel->GetMotion().bLatchArmed && Panel->GetMotion().AngleDeg > 5);
	const double Open = Panel->GetMotion().AngleDeg; Panel->Hands[0] = {}; Panel->Tick(0.1f);
	Panel->Hands[0] = P2dFace(Open,-2,30,FVector(0,1,0)); Panel->Tick(0.1f);
	Panel->Hands[0] = P2dFace(Open,2,30,FVector(0,1,0)); Panel->Tick(1);
	TestEqual(TEXT("Open lid rejects bottom push"), Panel->GetMotion().AngleDeg, Open);
	TestEqual(TEXT("Opening disabled contact counted"), Panel->GetMotion().Metrics.Ignored[static_cast<int32>(EVBoRDoorIgnore::OpeningDisabled)],1);
	Panel->Hands[0] = {}; Panel->Tick(0.1f);
	Panel->Hands[0] = P2dFace(Open,2); Panel->Tick(0.1f); Panel->Hands[0] = P2dFace(0,0); Panel->Tick(1);
	TestTrue(TEXT("Top push closes and locks"), Panel->GetMotion().bLocked && Panel->GetMotion().AngleDeg == 0);
	TestEqual(TEXT("One latch event"), Observer->Locks, 1);
	Panel->Hands[0] = {}; Panel->Tick(0.1f); Panel->Hands[0] = P2dFace(0,2); Panel->Tick(0.1f);
	Panel->Hands[0] = P2dFace(0,-2); Panel->Tick(1);
	TestEqual(TEXT("Relatched top push blocked"), Panel->GetMotion().AngleDeg, 0.0);
	Panel->Hands[0] = {}; Panel->Tick(0.1f); Panel->Hands[0] = P2dFace(0,-2,30,FVector(0,1,0)); Panel->Tick(0.1f);
	Panel->Hands[0] = P2dFace(0,2,30,FVector(0,1,0)); Panel->Tick(1);
	TestEqual(TEXT("Relatched bottom push blocked"), Panel->GetMotion().AngleDeg, 0.0); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP2dButtonUnlock, "vBoR.P2d.Panel.E5ButtonEventUnlockWithoutSDKCancel", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP2dButtonUnlock::RunTest(const FString&)
{
	FP2dWorld W; AP2cTrackedPanel* Panel = P2dLid(W); AP2bTrackedPart* Part = W.World->SpawnActor<AP2bTrackedPart>();
	UIsdkGrabberComponent* Grab = P2dGrabber(W);
	AVBoRPokeButton* Button = W.World->SpawnActor<AVBoRPokeButton>(); W.Play(); Panel->SetAxisHandle(0,Part);
	Grab->Activate(true); Grab->SetWorldLocation(Part->GrabBox->GetComponentLocation());
	Grab->TickComponent(0.016f,LEVELTICK_All,nullptr); Grab->PalmGrab();
	TestTrue(TEXT("Real SDK selection active before button"), Grab->IsGrabbing() && Grab->GetGrabbedComponent() == Part->Grabbable);
	UP2cEventObserver* Events = P2dObserve(Panel); UP2dAudioObserver* Audio = FP2dTestAccess::ObserveAudio(Panel);
	UP2dButtonObserver* Observer = NewObject<UP2dButtonObserver>(Panel); Observer->Panel = Panel;
	Button->OnButtonPressed.AddDynamic(Observer, &UP2dButtonObserver::Pressed);
	Part->GrabComponent->GetCancelGrabEventDelegate()->AddDynamic(Observer, &UP2dButtonObserver::Cancel);
	FIsdkInteractionPointerEvent Event; Event.Type = EIsdkPointerEventType::Select; Event.Interactable = Button->Poke.Get();
	Button->Poke->GetInteractionPointerEventDelegate().Broadcast(Event);
	TestEqual(TEXT("Actual button press reaches Unlock"), Observer->Presses, 1);
	TestFalse(TEXT("Button unlocks"), Panel->GetMotion().bLocked); TestEqual(TEXT("One unlock event"), Events->Unlocks, 1);
	TestEqual(TEXT("One sound request"), Audio->Plays, 1); TestNotNull(TEXT("Uses existing unlock sound"), Audio->Sound.Get());
	TestEqual(TEXT("No SDK cancellation from event"), Observer->Cancels, 0);
	TestEqual(TEXT("Zero ForceCancel calls on connected part"), Part->GrabComponent->CancelCalls, 0);
	TestTrue(TEXT("SDK selection still owned after button"), Grab->IsGrabbing() && Grab->GetGrabbedComponent() == Part->Grabbable);
	TestEqual(TEXT("SDK transformer pose retained"), Part->GrabComponent->GetNumGrabbers(),1);
	Grab->PalmRelease();
	TestEqual(TEXT("Button never begins grab"), Panel->GetMotion().GrabHand, INDEX_NONE);
	Panel->Tick(10); TestFalse(TEXT("Waiting without opening does not relock"), Panel->GetMotion().bLocked);
	Panel->Hands[0] = P2dFace(0,-2,30,FVector(0,1,0)); Panel->Tick(0.1f);
	Panel->Hands[0] = P2dFace(0,2,30,FVector(0,1,0)); Panel->Tick(1);
	TestTrue(TEXT("Button-unlocked panel accepts bottom push"), Panel->GetMotion().AngleDeg > 0); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP2dNoOpUnlock, "vBoR.P2d.Panel.E6UnlockNoOp", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP2dNoOpUnlock::RunTest(const FString&)
{
	FP2dWorld W; AP2cTrackedPanel* Locked = P2dLid(W); AP2cTrackedPanel* Free = W.World->SpawnActor<AP2cTrackedPanel>(); W.Play();
	UP2cEventObserver* Events = P2dObserve(Locked); UP2dAudioObserver* Audio = FP2dTestAccess::ObserveAudio(Locked);
	TestFalse(TEXT("Latch-disabled Unlock is no-op"), Free->Unlock()); TestEqual(TEXT("No free-panel count"), Free->GetMotion().Metrics.Unlocks, 0);
	TestTrue(TEXT("First locked Unlock succeeds"), Locked->Unlock()); TestFalse(TEXT("Second Unlock no-op"), Locked->Unlock());
	TestEqual(TEXT("Count unchanged after duplicate"), Locked->GetMotion().Metrics.Unlocks, 1);
	TestEqual(TEXT("Event unchanged after duplicate"), Events->Unlocks, 1); TestEqual(TEXT("Sound unchanged after duplicate"), Audio->Plays, 1); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP2dNoHandleWarning, "vBoR.P2d.Panel.E7MissingOpeningWarning", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP2dNoHandleWarning::RunTest(const FString&)
{
	AddExpectedMessagePlain(TEXT("Latched panel has no opening handle"), ELogVerbosity::Warning);
	FP2dWorld W; AP2cTrackedPanel* Panel = P2dLid(W); W.Play();
	TestTrue(TEXT("Warning does not block button unlock"), Panel->Unlock()); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP2dInsideOpen, "vBoR.P2d.Door.E8InsideLeverUnlockAndPushOpen", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP2dInsideOpen::RunTest(const FString&)
{
	FP2dWorld W; AP3VehicleMock* Car = W.World->SpawnActor<AP3VehicleMock>(); AP2bTrackedDoor* Door = W.World->SpawnActor<AP2bTrackedDoor>();
	AP2bTrackedPart* Lever = W.World->SpawnActor<AP2bTrackedPart>(); W.Play(); P2dCopyPart(Lever,Car->InsideAxisHandle); Door->SetAxisHandle(1,Lever);
	const double RestY = Lever->PartMesh->GetComponentLocation().Y; Door->Hands[0] = P2dHandAt(FVector(85,-9,50)); P2dPull(Lever,25); Lever->Tick(0.01f);
	TestTrue(TEXT("Lever pulls toward interior +Y"), Lever->PartMesh->GetComponentLocation().Y > RestY);
	TestEqual(TEXT("Inside lever is unlock source"), Door->GetMotion().Metrics.LastUnlockHandle, 1);
	TestEqual(TEXT("Exactly one unlock"), Door->GetMotion().Metrics.Unlocks, 1); TestEqual(TEXT("Same hand"), Door->GetMotion().GrabHand, 0);
	Door->Hands[0] = P2dHandAt(FQuat(FVector::ZAxisVector,PI/6).RotateVector(FVector(85,-9,50))); Door->Tick(1);
	TestTrue(TEXT("Same hand pushes door open 30 deg"), FMath::IsNearlyEqual(Door->GetMotion().AngleDeg,30.0,0.001)); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP2dInsideClose, "vBoR.P2d.Door.E9InsideLeverCloseRelatchAndReturn", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP2dInsideClose::RunTest(const FString&)
{
	FP2dWorld W; AP2bTrackedDoor* Door = W.World->SpawnActor<AP2bTrackedDoor>(); AP2bTrackedPart* Lever = W.World->SpawnActor<AP2bTrackedPart>(); W.Play(); Door->SetAxisHandle(1,Lever);
	Door->Unlock(); FVBoRDoorMotion& Motion = FP2dTestAccess::Motion(Door); Motion.AngleDeg = 40; Motion.bLatchArmed = true;
	Door->Hands[0] = P2dHandAt(FQuat(FVector::ZAxisVector,FMath::DegreesToRadians(40.0)).RotateVector(FVector(85,-9,50)));
	// Tilt point 2 cm below lever through real SDK (P2d E9)
	Lever->MeshRestFromRoot = FTransform(FQuat::Identity,FVector(0,0,-2),FVector(0.18,0.04,0.04)); Lever->OnConstruction(Lever->GetActorTransform());
	P2dPull(Lever,25); Lever->Tick(0.01f); TestEqual(TEXT("Open door regrabs through inside lever"), Motion.GrabHandle, 1);
	Door->Hands[0] = P2dHandAt(FVector(85,-9,50)); Door->Tick(1);
	TestTrue(TEXT("Inside pull closes and locks"), Motion.bLocked && Motion.AngleDeg == 0);
	TestEqual(TEXT("Latch cancels application grab"), Motion.GrabHand, INDEX_NONE); TestEqual(TEXT("Lever hand released"), Lever->GetHoldingHand(), INDEX_NONE);
	Lever->Tick(1); TestTrue(TEXT("Released lever returns"), FMath::IsNearlyZero(Lever->GetOpenRatio()));
	TestEqual(TEXT("No second unlock for open-door lever"), Motion.Metrics.Unlocks, 1); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP2dFreePanel, "vBoR.P2d.Panel.E11LatchDisabledPushAndGrab", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP2dFreePanel::RunTest(const FString&)
{
	FP2dWorld W; AP2cTrackedPanel* Panel = P2dLid(W); Panel->bUseLatch = false;
	Panel->OnConstruction(Panel->GetActorTransform()); auto* Handle = P2dConsoleHandle(W,Panel,1); W.Play();
	Panel->Hands[0] = P2dFace(0,-2,30,FVector(0,1,0)); Panel->Tick(0.1f); Panel->Hands[0] = P2dFace(0,2,30,FVector(0,1,0)); Panel->Tick(1);
	TestTrue(TEXT("Latch-disabled panel accepts closed push"), Panel->GetMotion().AngleDeg > 0);
	P2dPress(Handle); TestEqual(TEXT("Inside handle accepts same-hand grab"), Panel->GetMotion().GrabHandle, 1);
	TestEqual(TEXT("Free panel has no unlock count"), Panel->GetMotion().Metrics.Unlocks, 0); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP2dPalmOpeningDefault, "vBoR.P2d.Panel.E13PalmOpeningDefaultKeepsBothFaces", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP2dPalmOpeningDefault::RunTest(const FString&)
{
	FP2dWorld W; AP2cTrackedPanel* Panel = P2dLid(W); Panel->bUseLatch = false; W.Play();
	TestTrue(TEXT("Panel allows palm opening by default"), Panel->bAllowPalmOpening && Panel->GetMotion().bAllowPalmOpening);
	Panel->Hands[0] = P2dFace(0,-2,30,FVector(0,1,0)); Panel->Tick(0.1f);
	Panel->Hands[0] = P2dFace(0,25,30,FVector(0,1,0)); Panel->Tick(1);
	const double Open = Panel->GetMotion().AngleDeg;
	TestTrue(TEXT("Default inside face opens panel"), Open > 5);
	Panel->Hands[0] = {}; Panel->Tick(0.1f);
	Panel->Hands[0] = P2dFace(Open,2); Panel->Tick(0.1f);
	Panel->Hands[0] = P2dFace(0,0); Panel->Tick(1);
	TestTrue(TEXT("Default outside face closes panel"), Panel->GetMotion().AngleDeg < Open);
	TestEqual(TEXT("Default has no disabled opening contacts"), Panel->GetMotion().Metrics.Ignored[static_cast<int32>(EVBoRDoorIgnore::OpeningDisabled)],0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP2dVisualReturn, "vBoR.P2d.Panel.E14VisualReturnsOnReleaseLatchAndReset", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP2dVisualReturn::RunTest(const FString&)
{
	FP2dWorld W; AP2cTrackedPanel* Panel = P2dLid(W); auto* Handle = P2dConsoleHandle(W,Panel);
	UIsdkGrabberComponent* Grab = P2dGrabber(W); W.Play();
	Panel->Hands[0] = P2dFace(0,0); P2dPress(Handle);
	Panel->Hands[0] = P2dFace(40,0); Panel->Tick(1);
	Panel->bPalmClosed[0] = false; Panel->Tick(0.1f); Handle->Tick(1);
	TestEqual(TEXT("Palm open ends grab"), Panel->GetMotion().GrabHand,INDEX_NONE);
	TestTrue(TEXT("Palm open returns handle"), FMath::IsNearlyZero(Handle->GetOpenRatio()));
	Panel->bPalmClosed[0] = true; P2dPress(Handle);
	Panel->Hands[0] = P2dFace(0,0); Panel->Tick(1); Handle->Tick(1);
	TestTrue(TEXT("Latch ends grab"), Panel->GetMotion().bLocked && Panel->GetMotion().GrabHand == INDEX_NONE);
	TestTrue(TEXT("Latch returns handle"), FMath::IsNearlyZero(Handle->GetOpenRatio()));
	FP2dTestAccess::PalmOpen(Panel,Handle,0); P2dPress(Handle); Panel->ResetDoor(); Handle->Tick(1);
	TestTrue(TEXT("Reset returns handle"), FMath::IsNearlyZero(Handle->GetOpenRatio()));
	TestEqual(TEXT("Reset ends grab"), Panel->GetMotion().GrabHand,INDEX_NONE);
	FP2dTestAccess::PalmOpen(Panel,Handle,0); P2dSelect(Handle->Grabbable,Grab); P2dPress(Handle,0.5);
	FIsdkInteractionPointerEvent Event; Event.Interactor = Grab; Event.Interactable = Handle->Grabbable;
	Event.Type = EIsdkPointerEventType::Unselect; Handle->Grabbable->GetInteractionPointerEventDelegate().Broadcast(Event); Handle->Tick(1);
	TestTrue(TEXT("SDK unselect returns handle"), FMath::IsNearlyZero(Handle->GetOpenRatio())); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP2dRegrab, "vBoR.P2d.Panel.E15OpenLidRegrabMovesBothWays", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP2dRegrab::RunTest(const FString&)
{
	FP2dWorld W; AP2cTrackedPanel* Panel = P2dLid(W); Panel->bAllowPalmOpening = false;
	auto* Handle = P2dConsoleHandle(W,Panel); W.Play();
	Panel->Hands[0] = P2dFace(0,0); P2dPress(Handle);
	Panel->Hands[0] = P2dFace(30,0); Panel->Tick(1);
	Panel->bPalmClosed[0] = false; Panel->Tick(0.1f); Panel->bPalmClosed[0] = true;
	Handle->Tick(1); FP2dTestAccess::PalmOpen(Panel,Handle,0); P2dPress(Handle);
	TestEqual(TEXT("Open lid accepts front regrab"), Panel->GetMotion().GrabHand,0);
	Panel->Hands[0] = P2dFace(60,0); Panel->Tick(1);
	TestTrue(TEXT("Regrab lifts without palm opening"), FMath::IsNearlyEqual(Panel->GetMotion().AngleDeg,60.0,0.001));
	Panel->Hands[0] = P2dFace(20,0); Panel->Tick(1);
	TestTrue(TEXT("Held hand also lowers lid"), FMath::IsNearlyEqual(Panel->GetMotion().AngleDeg,20.0,0.001));
	TestEqual(TEXT("Open regrab does not unlock twice"), Panel->GetMotion().Metrics.Unlocks,1);
	Panel->Hands[0] = P2dFace(0,0); Panel->Tick(1);
	TestTrue(TEXT("Closing relatches and cancels grab"), Panel->GetMotion().bLocked && Panel->GetMotion().GrabHand == INDEX_NONE);
	Handle->Tick(1);
	TestTrue(TEXT("Closing returns handle"), FMath::IsNearlyZero(Handle->GetOpenRatio()));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP2dActualConsole, "vBoR.P2d.P3.E16ActualConsoleConfiguration", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP2dActualConsole::RunTest(const FString&)
{
	FP2dWorld W; AP3VehicleMock* Car = W.World->SpawnActor<AP3VehicleMock>(); W.Play();
	AVBoRHingedPanel* Panel = Car->ConsoleLid; AVBoRAxisPart* Handle = Car->ConsoleHandle;
	if (!TestNotNull(TEXT("Actual lid"),Panel) || !TestNotNull(TEXT("Actual handle"),Handle)) { return false; }
	TestTrue(TEXT("Outside handle connected"),Panel->OutsideAxisHandle == Handle);
	TestNull(TEXT("No inside handle"),Panel->InsideAxisHandle.Get());
	TestTrue(TEXT("Actual lid enables latch"),Panel->bUseLatch && Panel->GetMotion().bLocked);
	TestFalse(TEXT("Actual lid disables palm opening"),Panel->bAllowPalmOpening || Panel->GetMotion().bAllowPalmOpening);
	TestEqual(TEXT("Translate handle"),Handle->Movement,EVBoRAxisMovement::Translate);
	TestEqual(TEXT("Travel cm"),Handle->MaxTravelCm,0.75f);
	TestEqual(TEXT("Return handle"),Handle->Release,EVBoRAxisRelease::Return);
	TestEqual(TEXT("Outside side"),Handle->DoorHandleSide,EVBoRDoorHandleSide::Outside);
	TestTrue(TEXT("Palm only"),Handle->bAllowPalmGrab && !Handle->bAllowPinchGrab && !Handle->bAllowPush);
	TestTrue(TEXT("Handle follows hinge"),Handle->GetRootComponent()->GetAttachParent() == Panel->HingePivot);
	const FVector Front = Panel->HingePivot->GetComponentTransform().InverseTransformPosition(Handle->AxisPivot->GetComponentLocation());
	TestTrue(TEXT("Front center at 36 cm"),Front.Equals(FVector(36,0,0),0.001));
	TestTrue(TEXT("Press direction is hinge -X"),Handle->Axis == EAxis::X && Handle->bReverse && Handle->AxisPivot->GetForwardVector().Equals(Panel->HingePivot->GetForwardVector(),0.001));
	TArray<AActor*> Attached; Car->GetAttachedActors(Attached,true,true);
	int32 AxisParts = 0; for (AActor* Actor : Attached) { if (Actor->IsA(AVBoRAxisPart::StaticClass()) && Actor != Car->GloveboxHandle && Actor != Car->GearLever && Actor != Car->TurnSignalLever) { ++AxisParts; } }
	TestEqual(TEXT("Door levers and console handle"),AxisParts,3); return true;
}
#endif
