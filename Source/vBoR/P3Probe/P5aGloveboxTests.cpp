#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "P3VehicleMock.h"
#include "../P2Probe/P2dTestFixture.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "Interaction/IsdkGrabberComponent.h"
#include "Interaction/IsdkGrabbableComponent.h"
#include "IsdkFunctionLibrary.h"
#include "IsdkRuntimeSettings.h"

namespace
{
	struct FP5aWorld
	{
		UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
		AP3VehicleMock* Car = nullptr;
		AP2cTrackedPanel* Panel = nullptr;
		AP2bTrackedPart* Handle = nullptr;
		void Play(FTransform CarWorld = FTransform::Identity)
		{
			Car = World->SpawnActor<AP3VehicleMock>(AP3VehicleMock::StaticClass(), CarWorld);
			World->InitializeActorsForPlay(FURL()); World->GetWorldSettings()->NotifyBeginPlay();
		}
		void Tracked()
		{
			Play();
			// 근사 차의 실제 설정을 복사하고 추적 입력만 바꿈 (P5a E1–E8)
			const AVBoRHingedPanel* Source = Car->Glovebox;
			Panel = World->SpawnActorDeferred<AP2cTrackedPanel>(AP2cTrackedPanel::StaticClass(), Car->GetActorTransform());
			Panel->HingePivot->SetRelativeTransform(Source->HingePivot->GetRelativeTransform());
			Panel->MeshRestFromRoot = Source->MeshRestFromRoot;
			Panel->bUseLatch = Source->bUseLatch; Panel->bAllowPalmOpening = Source->bAllowPalmOpening;
			Panel->bUseInsideGrabArea = Source->bUseInsideGrabArea;
			Panel->MaxAngleDeg = Source->MaxAngleDeg; Panel->MinAxisDistanceCm = Source->MinAxisDistanceCm;
			const AVBoRAxisPart* Lever = Car->GloveboxHandle;
			Handle = World->SpawnActorDeferred<AP2bTrackedPart>(AP2bTrackedPart::StaticClass(), Car->GetActorTransform());
			Handle->AxisPivot->SetRelativeTransform(Lever->AxisPivot->GetRelativeTransform());
			Handle->MeshRestFromRoot = Lever->MeshRestFromRoot;
			Handle->Axis = Lever->Axis; Handle->bReverse = Lever->bReverse;
			Handle->MaxAngleDeg = Lever->MaxAngleDeg; Handle->Release = Lever->Release;
			Handle->FinishSpawning(Car->GetActorTransform());
			Panel->OutsideAxisHandle = Handle; Panel->FinishSpawning(Car->GetActorTransform());
			Handle->AttachToComponent(Panel->HingePivot, FAttachmentTransformRules::KeepWorldTransform);
		}
		~FP5aWorld() { World->EndPlay(EEndPlayReason::Quit); World->DestroyWorld(false); }
	};
	FVBoRDoorHand P5aFace(double Angle, double Y = 0, double X = 20, FVector Normal = FVector(0,-1,0))
	{
		const FQuat R(FVector::ZAxisVector, FMath::DegreesToRadians(Angle));
		FVBoRDoorHand Hand; Hand.bTracked = true; Hand.PalmNormal = R.RotateVector(Normal);
		for (FVector& Point : Hand.Points) { Point = R.RotateVector(FVector(X,Y,0)); }
		return Hand;
	}
	void P5aPull(AVBoRAxisPart* Handle, double Degrees)
	{
		const FTransform Pivot = Handle->AxisPivot->GetComponentTransform();
		const FVector Local = Pivot.InverseTransformPosition(Handle->PartMesh->GetComponentLocation());
		FP2dTestAccess::Begin(Handle, 0, UIsdkFunctionLibrary::MakeGrabPoseStruct(11, Pivot.TransformPosition(Local), FQuat::Identity));
		const FQuat R(FVector::YAxisVector, FMath::DegreesToRadians(Degrees));
		FP2dTestAccess::Move(Handle, UIsdkFunctionLibrary::MakeGrabPoseStruct(11, Pivot.TransformPosition(R.RotateVector(Local)), FQuat::Identity));
	}
	UP2cEventObserver* P5aObserve(AVBoRHingedPanel* Panel)
	{
		UP2cEventObserver* Events = NewObject<UP2cEventObserver>(Panel);
		Panel->OnPanelUnlocked.AddDynamic(Events, &UP2cEventObserver::Unlocked);
		Panel->OnPanelLocked.AddDynamic(Events, &UP2cEventObserver::Locked); return Events;
	}
	void P5aOpen(FP5aWorld& W, double Angle)
	{
		W.Panel->Hands[0] = P5aFace(0); P5aPull(W.Handle, W.Handle->MaxAngleDeg); W.Handle->Tick(0.01f);
		W.Panel->Hands[0] = P5aFace(Angle); W.Panel->Tick(1);
	}
	void P5aRelease(FP5aWorld& W)
	{
		W.Panel->bPalmClosed[0] = false; W.Panel->Tick(0.01f); W.Handle->Tick(1);
		W.Panel->Hands[0] = {}; W.Panel->Tick(0.01f);
	}
	UIsdkGrabberComponent* P5aGrabber(FP5aWorld& W)
	{
		AActor* Actor = W.World->SpawnActor<AActor>();
		UIsdkGrabberComponent* Grab = NewObject<UIsdkGrabberComponent>(Actor); Actor->SetRootComponent(Grab);
		Grab->InteractorTags.AddTag(IsdkGameplayTags::TAG_Isdk_Type_Device_Left);
		Grab->SetGrabDetectionTypeAllowed(EIsdkGrabDetectorType::DistanceGrab, false);
		Grab->SetGrabDetectionTypeAllowed(EIsdkGrabDetectorType::RayGrab, false);
		Grab->RegisterComponent(); return Grab;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5aLockedPalm, "vBoR.P5a.E1LockedBothFaces", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5aLockedPalm::RunTest(const FString&)
{
	FP5aWorld W; W.Tracked();
	for (int32 Face = 0; Face < 2; ++Face)
	{
		const double Sign = Face == 0 ? 1 : -1;
		W.Panel->Hands[0] = P5aFace(0,Sign*2,20,FVector(0,-Sign,0)); W.Panel->Tick(0.1f);
		W.Panel->Hands[0] = P5aFace(0,-Sign*2,20,FVector(0,-Sign,0)); W.Panel->Tick(1);
		W.Panel->Hands[0] = {}; W.Panel->Tick(0.1f);
	}
	TestTrue(TEXT("Both faces remain latched at zero"), W.Panel->GetMotion().bLocked && W.Panel->GetMotion().AngleDeg == 0);
	TestEqual(TEXT("Two locked contact episodes"), W.Panel->GetMotion().Metrics.Ignored[static_cast<int32>(EVBoRDoorIgnore::Locked)],2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5aEarlyRelease, "vBoR.P5a.E2BelowThresholdReturnsOnlyHandle", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5aEarlyRelease::RunTest(const FString&)
{
	FP5aWorld W; W.Tracked(); W.Panel->Hands[0] = P5aFace(0);
	P5aPull(W.Handle,10); W.Handle->Tick(0.01f);
	TestTrue(TEXT("Handle moves below threshold"), W.Handle->GetOpenRatio() > 0 && W.Handle->GetOpenRatio() < W.Handle->OpenThreshold);
	TestTrue(TEXT("Door remains locked"), W.Panel->GetMotion().bLocked);
	W.Handle->EndDoorHold(false); W.Handle->Tick(1); W.Panel->Tick(1);
	TestTrue(TEXT("Handle returns to rest"), FMath::IsNearlyZero(W.Handle->GetOpenRatio()));
	TestEqual(TEXT("Door stays at zero"), W.Panel->GetMotion().AngleDeg,0.0);
	TestEqual(TEXT("No unlock"), W.Panel->GetMotion().Metrics.Unlocks,0); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5aSameHand, "vBoR.P5a.E3SameHandOpensAndReleaseHoldsDoor", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5aSameHand::RunTest(const FString&)
{
	FP5aWorld W; W.Tracked(); UP2cEventObserver* Events = P5aObserve(W.Panel);
	const FVector HandleRest = W.Handle->PartMesh->GetComponentLocation();
	W.Panel->Hands[0] = P5aFace(0); P5aPull(W.Handle,20); W.Handle->Tick(0.01f);
	TestTrue(TEXT("Handle bottom tilts toward seat and upward"), W.Handle->PartMesh->GetComponentLocation().X < HandleRest.X && W.Handle->PartMesh->GetComponentLocation().Z > HandleRest.Z);
	TestEqual(TEXT("Threshold transfers same hand"), W.Panel->GetMotion().GrabHand,0);
	TestEqual(TEXT("Outside handle unlock source"), W.Panel->GetMotion().Metrics.LastUnlockHandle,0);
	TestEqual(TEXT("One unlock event"), Events->Unlocks,1);
	const FVector ClosedTop = W.Panel->HingePivot->GetComponentTransform().TransformPosition(FVector(28,0,0));
	W.Panel->Hands[0] = P5aFace(40); W.Panel->Tick(1);
	const FVector OpenTop = W.Panel->HingePivot->GetComponentTransform().TransformPosition(FVector(28,0,0));
	TestTrue(TEXT("Door upper edge moves toward seat and down"), OpenTop.X < ClosedTop.X && OpenTop.Z < ClosedTop.Z);
	TestTrue(TEXT("Door follows hand to forty degrees"), FMath::IsNearlyEqual(W.Panel->GetMotion().AngleDeg,40.0,0.001));
	P5aRelease(W); W.Panel->Tick(3); W.Handle->Tick(3);
	TestEqual(TEXT("Palm open releases door"), W.Panel->GetMotion().GrabHand,INDEX_NONE);
	TestTrue(TEXT("Door holds angle for seconds"), FMath::IsNearlyEqual(W.Panel->GetMotion().AngleDeg,40.0,0.001));
	TestTrue(TEXT("Only handle returns"), FMath::IsNearlyZero(W.Handle->GetOpenRatio())); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5aPalmClose, "vBoR.P5a.E4BottomPushClosesAndRelatches", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5aPalmClose::RunTest(const FString&)
{
	FP5aWorld W; W.Tracked(); UP2cEventObserver* Events = P5aObserve(W.Panel); P5aOpen(W,60); P5aRelease(W);
	W.Panel->Hands[0] = P5aFace(60,2); W.Panel->Tick(0.1f);
	W.Panel->Hands[0] = P5aFace(30,0); W.Panel->Tick(1);
	TestTrue(TEXT("Bottom push first moves in closing direction"), W.Panel->GetMotion().AngleDeg < 60 && W.Panel->GetMotion().AngleDeg > 0);
	W.Panel->Hands[0] = P5aFace(0,0); W.Panel->Tick(1);
	TestTrue(TEXT("Bottom push closes and relatches"), W.Panel->GetMotion().bLocked && W.Panel->GetMotion().AngleDeg == 0);
	TestEqual(TEXT("One lock event"), Events->Locks,1);
	for (int32 Face = 0; Face < 2; ++Face)
	{
		W.Panel->Hands[0] = {}; W.Panel->Tick(0.1f);
		const double Sign = Face == 0 ? 1 : -1;
		W.Panel->Hands[0] = P5aFace(0,Sign*2,20,FVector(0,-Sign,0)); W.Panel->Tick(0.1f);
		W.Panel->Hands[0] = P5aFace(0,-Sign*2,20,FVector(0,-Sign,0)); W.Panel->Tick(1);
		TestEqual(TEXT("Relatched face cannot move door"), W.Panel->GetMotion().AngleDeg,0.0);
	}
	TestEqual(TEXT("No duplicate lock event"), Events->Locks,1); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5aRejectTop, "vBoR.P5a.E5TopPushCannotOpen", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5aRejectTop::RunTest(const FString&)
{
	FP5aWorld W; W.Tracked(); P5aOpen(W,60); P5aRelease(W);
	const double Before = W.Panel->GetMotion().AngleDeg;
	W.Panel->Hands[0] = P5aFace(Before,-2,20,FVector(0,1,0)); W.Panel->Tick(0.1f);
	W.Panel->Hands[0] = P5aFace(Before,2,20,FVector(0,1,0)); W.Panel->Tick(1);
	TestEqual(TEXT("Top push leaves angle unchanged"), W.Panel->GetMotion().AngleDeg,Before);
	TestEqual(TEXT("OpeningDisabled counted"), W.Panel->GetMotion().Metrics.Ignored[static_cast<int32>(EVBoRDoorIgnore::OpeningDisabled)],1); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5aPalmBand, "vBoR.P5a.E6NearAxisAndUsablePalmBand", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5aPalmBand::RunTest(const FString&)
{
	FP5aWorld W; W.Tracked(); P5aOpen(W,60); P5aRelease(W);
	W.Panel->Hands[0] = P5aFace(60,2,4); W.Panel->Tick(0.1f);
	W.Panel->Hands[0] = P5aFace(60,0,4); W.Panel->Tick(1);
	TestTrue(TEXT("Near-axis push ignored"), FMath::IsNearlyEqual(W.Panel->GetMotion().AngleDeg,60.0,0.001));
	TestEqual(TEXT("NearAxis counted"), W.Panel->GetMotion().Metrics.Ignored[static_cast<int32>(EVBoRDoorIgnore::NearAxis)],1);
	const FVBoRDoorMotion& Motion = W.Panel->GetMotion();
	TestTrue(TEXT("Palm band covers at least half door height"), Motion.FaceMaxCm.X - W.Panel->MinAxisDistanceCm >= (Motion.FaceMaxCm.X - Motion.FaceMinCm.X)/2);
	W.Panel->Hands[0] = {}; W.Panel->Tick(0.1f);
	W.Panel->Hands[0] = P5aFace(60,2,12); W.Panel->Tick(0.1f);
	W.Panel->Hands[0] = P5aFace(40,0,12); W.Panel->Tick(1);
	TestTrue(TEXT("Mid-height outside minimum accepts palm"), W.Panel->GetMotion().AngleDeg < 60);
	TestEqual(TEXT("Outside face push recorded"), Motion.Metrics.PushStarts[0][0],1); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5aRegrabClose, "vBoR.P5a.E7RegrabClosesCancelsAndReturns", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5aRegrabClose::RunTest(const FString&)
{
	FP5aWorld W; W.Tracked(); UP2cEventObserver* Events = P5aObserve(W.Panel); P5aOpen(W,50); P5aRelease(W);
	W.Panel->bPalmClosed[0] = true; W.Panel->Hands[0] = P5aFace(50);
	P5aPull(W.Handle,25); W.Handle->Tick(0.01f);
	TestEqual(TEXT("Open door regrabs same hand"), W.Panel->GetMotion().GrabHand,0);
	W.Panel->Hands[0] = P5aFace(20); W.Panel->Tick(1);
	TestTrue(TEXT("Lifting held door reduces angle"), FMath::IsNearlyEqual(W.Panel->GetMotion().AngleDeg,20.0,0.001));
	W.Panel->Hands[0] = P5aFace(0); W.Panel->Tick(1); W.Handle->Tick(1);
	TestTrue(TEXT("Latch locks and cancels door grab"), W.Panel->GetMotion().bLocked && W.Panel->GetMotion().GrabHand == INDEX_NONE);
	TestEqual(TEXT("Latch releases lever hand"), W.Handle->GetHoldingHand(),INDEX_NONE);
	TestTrue(TEXT("Lever returns"), FMath::IsNearlyZero(W.Handle->GetOpenRatio()));
	TestEqual(TEXT("One lock event"), Events->Locks,1); TestEqual(TEXT("Open regrab never unlocks twice"), Events->Unlocks,1); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5aMaximum, "vBoR.P5a.E8MaximumAngleClamp", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5aMaximum::RunTest(const FString&)
{
	FP5aWorld W; W.Tracked(); P5aOpen(W,120);
	TestEqual(TEXT("Door clamps at configured maximum"), W.Panel->GetMotion().AngleDeg,static_cast<double>(W.Car->GloveboxMaxAngleDeg)); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5aActualCar, "vBoR.P5a.E9ActualCarConfigurationAndCleanup", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5aActualCar::RunTest(const FString&)
{
	FP5aWorld W; W.Play(FTransform(FRotator(0,37,0),FVector(123,-256,42)));
	AVBoRHingedPanel* Panel = W.Car->Glovebox; AVBoRAxisPart* Handle = W.Car->GloveboxHandle;
	if (!TestNotNull(TEXT("Actual panel"), Panel) || !TestNotNull(TEXT("Actual handle"), Handle)) { return false; }
	TestTrue(TEXT("Latched with palm opening disabled"), Panel->bUseLatch && Panel->GetMotion().bLocked && !Panel->bAllowPalmOpening);
	TestFalse(TEXT("Inside grab option disabled"), Panel->bUseInsideGrabArea);
	TestTrue(TEXT("Actual outside handle connected"), Panel->OutsideAxisHandle == Handle);
	TestTrue(TEXT("Handle parent is panel hinge"), Handle->GetRootComponent()->GetAttachParent() == Panel->HingePivot);
	TestTrue(TEXT("Panel actor at car origin"), Panel->GetActorTransform().Equals(W.Car->GetActorTransform(),0.001));
	TestTrue(TEXT("Handle actor at car origin"), Handle->GetActorTransform().Equals(W.Car->GetActorTransform(),0.001));
	TestTrue(TEXT("Panel mesh keeps rest world position"), Panel->DoorMesh->GetComponentTransform().Equals(Panel->MeshRestFromRoot * W.Car->GetActorTransform(),0.001));
	TestTrue(TEXT("Handle mesh keeps rest world position"), Handle->PartMesh->GetComponentTransform().Equals(Handle->MeshRestFromRoot * W.Car->GetActorTransform(),0.001));
	const FTransform Frame = Panel->HingePivot->GetRelativeTransform();
	TestTrue(TEXT("Local X is car +Z"), Frame.TransformVectorNoScale(FVector::XAxisVector).Equals(FVector::ZAxisVector,0.001));
	TestTrue(TEXT("Local Y is car -X"), Frame.TransformVectorNoScale(FVector::YAxisVector).Equals(-FVector::XAxisVector,0.001));
	TestTrue(TEXT("Local Z is car -Y"), Frame.TransformVectorNoScale(FVector::ZAxisVector).Equals(-FVector::YAxisVector,0.001));
	TestEqual(TEXT("Handle rotates"), Handle->Movement,EVBoRAxisMovement::Rotate);
	TestEqual(TEXT("Handle alone returns"), Handle->Release,EVBoRAxisRelease::Return);
	TestFalse(TEXT("Door has no physics gravity"), Panel->DoorMesh->IsSimulatingPhysics());
	TestTrue(TEXT("Passenger side below dashboard"), W.Car->GloveboxHingeCm.Y > 0 && W.Car->GloveboxHingeCm.Z + W.Car->GloveboxSizeCm.Z < W.Car->DashboardCenterCm.Z - W.Car->DashboardSizeCm.Z/2);
	for (int32 I = 0; I < 2; ++I)
	{
		UBoxComponent* Box = I == 0 ? Panel->OutsideHandle.Get() : Panel->InsideHandle.Get();
		UIsdkGrabbableComponent* Grab = I == 0 ? Panel->OutsideGrab.Get() : Panel->InsideGrab.Get();
		TestEqual(TEXT("Disabled box collision"), Box->GetCollisionEnabled(),ECollisionEnabled::NoCollision);
		TestFalse(TEXT("Disabled box overlap"), Box->GetGenerateOverlapEvents());
		TestFalse(TEXT("Disabled visual"), (I == 0 ? Panel->OutsideHandleMesh : Panel->InsideHandleMesh)->IsVisible());
		TestFalse(TEXT("Disabled palm input"), Grab->IsGrabInputMethodAllowed(EIsdkGrabInputMethod::Palm));
	}
	FIsdkInteractionPointerEvent Event; Event.Type = EIsdkPointerEventType::Select; Event.Interactor = P5aGrabber(W); Event.Interactable = Panel->InsideGrab;
	Panel->InsideGrab->GetInteractionPointerEventDelegate().Broadcast(Event);
	TestTrue(TEXT("Injected inside select remains locked"), Panel->GetMotion().bLocked && Panel->GetMotion().GrabHand == INDEX_NONE && Panel->GetMotion().Metrics.Unlocks == 0);
	W.Car->Destroy(); TestTrue(TEXT("Car EndPlay destroys panel and handle"), Panel->IsActorBeingDestroyed() && Handle->IsActorBeingDestroyed()); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5aActualTransfer, "vBoR.P5a.E10ActualHandleTransformerTransfer", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5aActualTransfer::RunTest(const FString&)
{
	FP5aWorld W; W.Play(); AVBoRHingedPanel* Panel = W.Car->Glovebox; AVBoRAxisPart* Handle = W.Car->GloveboxHandle;
	UP2cEventObserver* Events = P5aObserve(Panel); P5aPull(Handle,20);
	// 실제 손잡이 SDK 변환 뒤 추적된 손만 합성함 (P2b E17 방식, P5a E10)
	TestTrue(TEXT("Actual threshold reached"), Handle->GetOpenRatio() >= Handle->OpenThreshold);
	TestTrue(TEXT("Actual lever transfers"), FP2dTestAccess::Transfer(Panel,Handle,P5aFace(0)));
	TestEqual(TEXT("One actual unlock event"), Events->Unlocks,1);
	TestEqual(TEXT("Outside unlock source"), Panel->GetMotion().Metrics.LastUnlockHandle,0);
	TestEqual(TEXT("Same hand owns actual panel"), Panel->GetMotion().GrabHand,0);
	const FVector Closed = Panel->DoorMesh->GetComponentLocation();
	FVBoRDoorHand Hands[2] = {P5aFace(45),{}}; FP2dTestAccess::Advance(Panel,Hands,1);
	TestTrue(TEXT("Actual panel follows hand"), FMath::IsNearlyEqual(Panel->GetMotion().AngleDeg,45.0,0.001));
	TestTrue(TEXT("Actual panel moves toward seat and down"), Panel->DoorMesh->GetComponentLocation().X < Closed.X && Panel->DoorMesh->GetComponentLocation().Z < Closed.Z);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5aWarnings, "vBoR.P5a.E11MissingOpeningWarnings", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5aWarnings::RunTest(const FString&)
{
	AddExpectedMessagePlain(TEXT("버튼 Unlock 뒤에도 열 수 없음 (손바닥 열기 꺼짐)."), ELogVerbosity::Warning);
	AddExpectedMessagePlain(TEXT("버튼으로 Unlock을 부르면 무시."), ELogVerbosity::Warning);
	FP5aWorld W;
	for (bool bPalm : {false,true})
	{
		AVBoRHingedPanel* Panel = W.World->SpawnActorDeferred<AVBoRHingedPanel>(AVBoRHingedPanel::StaticClass(),FTransform::Identity);
		Panel->bUseLatch = true; Panel->bUseOutsideGrabArea = Panel->bUseInsideGrabArea = false;
		Panel->bAllowPalmOpening = bPalm; Panel->FinishSpawning(FTransform::Identity);
	}
	W.World->InitializeActorsForPlay(FURL()); W.World->GetWorldSettings()->NotifyBeginPlay(); return true;
}
#endif
