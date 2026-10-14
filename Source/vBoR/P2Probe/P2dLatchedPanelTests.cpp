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
		Panel->bUseLatch = true; Panel->bUseInsideGrabArea = false;
		Panel->HingePivot->SetRelativeTransform(FTransform(FQuat(FVector::XAxisVector, PI/2), FVector(-45,0,70)));
		Panel->MeshRestFromRoot = FTransform(FQuat::Identity, FVector(-27.5,0,70), FVector(0.35,0.24,0.03));
		Panel->MaxAngleDeg = 100; Panel->OnConstruction(Panel->GetActorTransform()); return Panel;
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
		// mock 의 Actor 가 경첩 자식이어도 쉬는 car-origin 좌표는 보존함 (P2d D5)
		To->AxisPivot->SetRelativeTransform(From->AxisPivot->GetRelativeTransform()); To->OnConstruction(To->GetActorTransform());
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP2dLockedPush, "vBoR.P2d.Panel.E1LockedTopAndBottomPush", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP2dLockedPush::RunTest(const FString&)
{
	FP2dWorld W; AP2cTrackedPanel* Panel = P2dLid(W); W.Play();
	Panel->Hands[0] = P2dFace(0,2); Panel->Tick(0.1f); Panel->Hands[0] = P2dFace(0,-2); Panel->Tick(0.1f);
	Panel->Hands[0] = {}; Panel->Tick(0.1f);
	Panel->Hands[0] = P2dFace(0,-2,30,FVector(0,1,0)); Panel->Tick(0.1f);
	Panel->Hands[0] = P2dFace(0,2,30,FVector(0,1,0)); Panel->Tick(0.1f);
	TestEqual(TEXT("Both faces stay closed"), Panel->GetMotion().AngleDeg, 0.0);
	TestTrue(TEXT("Still locked"), Panel->GetMotion().bLocked);
	TestEqual(TEXT("Both locked contacts counted"), Panel->GetMotion().Metrics.Ignored[0], 2); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP2dDisabledArea, "vBoR.P2d.Panel.E2DisabledGrabIgnoresInjectedSelect", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP2dDisabledArea::RunTest(const FString&)
{
	FP2dWorld W; AP2cTrackedPanel* Panel = P2dLid(W); UIsdkGrabberComponent* Grab = P2dGrabber(W); W.Play();
	Panel->Hands[0] = P2dFace(0,0); P2dSelect(Panel->InsideGrab, Grab);
	TestTrue(TEXT("Disabled inside stays locked"), Panel->GetMotion().bLocked);
	TestEqual(TEXT("Disabled inside never grabs"), Panel->GetMotion().GrabHand, INDEX_NONE);
	TestEqual(TEXT("No disabled unlock count"), Panel->GetMotion().Metrics.Unlocks, 0);
	TestEqual(TEXT("No inside collision"), Panel->InsideHandle->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
	TestFalse(TEXT("No inside overlap"), Panel->InsideHandle->GetGenerateOverlapEvents());
	TestFalse(TEXT("No inside visibility"), Panel->InsideHandle->IsVisible());
	TestFalse(TEXT("No inside SDK palm input"), Panel->InsideGrab->IsGrabInputMethodAllowed(EIsdkGrabInputMethod::Palm));
	Panel->bUseOutsideGrabArea = false; Panel->OnConstruction(Panel->GetActorTransform()); P2dSelect(Panel->OutsideGrab, Grab);
	TestEqual(TEXT("Outside disable also rejects select"), Panel->GetMotion().GrabHand, INDEX_NONE); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP2dTabTransfer, "vBoR.P2d.Panel.E3TabUnlockAndSameHandLift", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP2dTabTransfer::RunTest(const FString&)
{
	FP2dWorld W; AP3VehicleMock* Car = W.World->SpawnActor<AP3VehicleMock>(); AP2cTrackedPanel* Panel = P2dLid(W);
	AP2bTrackedPart* Tab = W.World->SpawnActor<AP2bTrackedPart>(); W.Play(); P2dCopyPart(Tab, Car->ConsoleTab); Panel->SetAxisHandle(0,Tab);
	UP2cEventObserver* Observer = P2dObserve(Panel); UP2dAudioObserver* Audio = FP2dTestAccess::ObserveAudio(Panel);
	const FVector Rest = Tab->PartMesh->GetComponentLocation(); P2dPull(Tab,25); Tab->Tick(0.01f);
	// 첫 Tick 은 손 없어서 넘기지 않음. 실제 탭은 먼저 위로 젖혀져야 함 (P2d E3)
	TestTrue(TEXT("Tab lifts upward"), Tab->PartMesh->GetComponentLocation().Z > Rest.Z);
	Panel->Hands[0] = P2dFace(0,0,35); Tab->Tick(0.01f);
	TestFalse(TEXT("Tab unlocks at threshold"), Panel->GetMotion().bLocked);
	TestEqual(TEXT("Same hand transfers"), Panel->GetMotion().GrabHand, 0);
	TestEqual(TEXT("One unlock"), Observer->Unlocks, 1); TestEqual(TEXT("Same unlock sound requested"), Audio->Plays, 1);
	Panel->Hands[0] = P2dFace(35,0,35); Panel->Tick(1);
	TestTrue(TEXT("Same hand lifts lid 35 deg"), FMath::IsNearlyEqual(Panel->GetMotion().AngleDeg,35.0,0.001)); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP2dRelatch, "vBoR.P2d.Panel.E4PushClosesAndRelatches", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP2dRelatch::RunTest(const FString&)
{
	FP2dWorld W; AP2cTrackedPanel* Panel = P2dLid(W); W.Play(); UP2cEventObserver* Observer = P2dObserve(Panel);
	Panel->Unlock(); Panel->Hands[0] = P2dFace(0,-2,30,FVector(0,1,0)); Panel->Tick(0.1f);
	Panel->Hands[0] = P2dFace(0,25,30,FVector(0,1,0)); Panel->Tick(1);
	TestTrue(TEXT("Opening arms latch"), Panel->GetMotion().bLatchArmed && Panel->GetMotion().AngleDeg > 5);
	const double Open = Panel->GetMotion().AngleDeg; Panel->Hands[0] = {}; Panel->Tick(0.1f);
	Panel->Hands[0] = P2dFace(Open,2); Panel->Tick(0.1f); Panel->Hands[0] = P2dFace(0,0); Panel->Tick(1);
	TestTrue(TEXT("Top push closes and locks"), Panel->GetMotion().bLocked && Panel->GetMotion().AngleDeg == 0);
	TestEqual(TEXT("One latch event"), Observer->Locks, 1);
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
	Panel->OutsideTransformer->GetCancelGrabEventDelegate()->AddDynamic(Observer, &UP2dButtonObserver::Cancel);
	Panel->InsideTransformer->GetCancelGrabEventDelegate()->AddDynamic(Observer, &UP2dButtonObserver::Cancel);
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
	AddExpectedMessagePlain(TEXT("Latched panel has no opening handle or grab area"), ELogVerbosity::Warning);
	FP2dWorld W; AP2cTrackedPanel* Panel = P2dLid(W); Panel->bUseOutsideGrabArea = false; W.Play();
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
	// 레버 밑 2 cm 점을 실제 SDK 로 젖힘 (P2d E9)
	Lever->MeshRestFromRoot = FTransform(FQuat::Identity,FVector(0,0,-2),FVector(0.18,0.04,0.04)); Lever->OnConstruction(Lever->GetActorTransform());
	P2dPull(Lever,25); Lever->Tick(0.01f); TestEqual(TEXT("Open door regrabs through inside lever"), Motion.GrabHandle, 1);
	Door->Hands[0] = P2dHandAt(FVector(85,-9,50)); Door->Tick(1);
	TestTrue(TEXT("Inside pull closes and locks"), Motion.bLocked && Motion.AngleDeg == 0);
	TestEqual(TEXT("Latch cancels application grab"), Motion.GrabHand, INDEX_NONE); TestEqual(TEXT("Lever hand released"), Lever->GetHoldingHand(), INDEX_NONE);
	Lever->Tick(1); TestTrue(TEXT("Released lever returns"), FMath::IsNearlyZero(Lever->GetOpenRatio()));
	TestEqual(TEXT("No second unlock for open-door lever"), Motion.Metrics.Unlocks, 1); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP2dInsideBox, "vBoR.P2d.Door.E10ConnectedInsideBoxIgnoresSelect", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP2dInsideBox::RunTest(const FString&)
{
	FP2dWorld W; AP3VehicleMock* Car = W.World->SpawnActor<AP3VehicleMock>();
	AP2bTrackedDoor* TrackedDoor = W.World->SpawnActor<AP2bTrackedDoor>(); UIsdkGrabberComponent* Grab = P2dGrabber(W); W.Play();
	TrackedDoor->SetAxisHandle(1,Car->InsideAxisHandle); TrackedDoor->Hands[0] = P2dHandAt(FVector(85,-9,50));
	P2dSelect(TrackedDoor->InsideGrab,Grab);
	TestTrue(TEXT("Connected box rejects even a tracked hand"), TrackedDoor->GetMotion().bLocked);
	TestEqual(TEXT("Connected box never starts grab"), TrackedDoor->GetMotion().GrabHand,INDEX_NONE);
	P2dSelect(Car->Door->InsideGrab,Grab);
	TestTrue(TEXT("Actual P3 inside box stays locked"), Car->Door->GetMotion().bLocked);
	TestEqual(TEXT("No injected box unlock"), Car->Door->GetMotion().Metrics.Unlocks, 0);
	TestEqual(TEXT("No box collision"), Car->Door->InsideHandle->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
	TestFalse(TEXT("No box display"), Car->Door->InsideHandle->IsVisible());
	TestFalse(TEXT("No box SDK grab"), Car->Door->InsideGrab->IsGrabInputMethodAllowed(EIsdkGrabInputMethod::Palm));
	TestTrue(TEXT("Inside lever follows door hinge"), Car->InsideAxisHandle->GetRootComponent()->GetAttachParent() == Car->Door->HingePivot);
	TestTrue(TEXT("Console tab follows lid hinge"), Car->ConsoleTab->GetRootComponent()->GetAttachParent() == Car->ConsoleLid->HingePivot); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP2dFreePanel, "vBoR.P2d.Panel.E11LatchDisabledPushAndGrab", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP2dFreePanel::RunTest(const FString&)
{
	FP2dWorld W; AP2cTrackedPanel* Panel = P2dLid(W); Panel->bUseLatch = false; Panel->bUseInsideGrabArea = true;
	Panel->OnConstruction(Panel->GetActorTransform()); UIsdkGrabberComponent* Grab = P2dGrabber(W); W.Play();
	Panel->Hands[0] = P2dFace(0,-2,30,FVector(0,1,0)); Panel->Tick(0.1f); Panel->Hands[0] = P2dFace(0,2,30,FVector(0,1,0)); Panel->Tick(1);
	TestTrue(TEXT("Latch-disabled panel accepts closed push"), Panel->GetMotion().AngleDeg > 0);
	P2dSelect(Panel->InsideGrab,Grab); TestEqual(TEXT("Enabled inside still accepts SDK select"), Panel->GetMotion().GrabHandle, 1);
	TestEqual(TEXT("Free panel has no unlock count"), Panel->GetMotion().Metrics.Unlocks, 0); return true;
}
#endif
