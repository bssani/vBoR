#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "P2cTestFixture.h"
#include "P2bTestFixture.h"
#include "../P3Probe/P3VehicleMock.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "IsdkFunctionLibrary.h"

struct FP2cTestAccess
{
	static FVBoRDoorMotion& Motion(AVBoRHingedPanel* Panel) { return Panel->Motion; }
	static FTransform Frame(AVBoRHingedPanel* Panel) { return Panel->HingeFrame(); }
	static void Apply(AVBoRHingedPanel* Panel, double Before = 0, int32 Unlocks = 0, int32 Latches = 0) { Panel->ApplyState(Before, Unlocks, Latches); }
	static bool Grab(AVBoRHingedPanel* Panel, int32 Hand, int32 Handle, const FVBoRDoorHand& Input)
	{
		const int32 Unlocks = Panel->Motion.Metrics.Unlocks;
		const bool Result = Panel->Motion.BeginGrab(Hand, Handle, Input);
		Panel->ApplyState(Panel->Motion.AngleDeg, Unlocks, Panel->Motion.Metrics.Latches);
		return Result;
	}
	static void Begin(AVBoRAxisPart* Part, const FIsdkGrabPose& Pose)
	{
		Part->HoldingHand = 0; Part->Transformer->BeginTransform({Pose}, FIsdkTargetTransform(Part->AxisPivot));
	}
	static void Move(AVBoRAxisPart* Part, const FIsdkGrabPose& Pose)
	{
		Part->AxisPivot->SetRelativeTransform(Part->Transformer->UpdateTracked({Pose}, FIsdkTargetTransform(Part->AxisPivot), true));
	}
	static void Value(AVBoRAxisPart* Part, double Value)
	{
		Part->Value = Value; Part->AxisPivot->SetRelativeTransform(Part->ValueTransform() * Part->RestPivot);
	}
	static void Release(AVBoRAxisPart* Part) { Part->ReleasePart(); }
};

namespace
{
	struct FP2cWorld
	{
		UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
		void Play() { World->InitializeActorsForPlay(FURL()); World->GetWorldSettings()->NotifyBeginPlay(); }
		~FP2cWorld() { World->EndPlay(EEndPlayReason::Quit); World->DestroyWorld(false); }
	};
	void ConfigureLid(AVBoRHingedPanel* Panel)
	{
		// +Y 를 윗면으로 둠. +Z 는 차 -Y 축 (열림 +deg 가 위로, P2c D3)
		Panel->HingePivot->SetRelativeTransform(FTransform(FQuat(FVector::XAxisVector, PI / 2), FVector(-45, 0, 70)));
		Panel->MeshRestFromRoot = FTransform(FQuat::Identity, FVector(-27.5, 0, 70), FVector(0.35, 0.24, 0.03));
		Panel->MaxAngleDeg = 100;
		Panel->OutsideHandle->SetRelativeLocation(FVector(35, 5.5, 0));
		Panel->InsideHandle->SetRelativeLocation(FVector(35, -5.5, 0));
		Panel->OnConstruction(Panel->GetActorTransform());
	}
	FVBoRDoorHand HandAt(FVector Point, FVector Normal = FVector::ZeroVector)
	{
		FVBoRDoorHand Hand; Hand.bTracked = true; Hand.PalmNormal = Normal;
		for (FVector& P : Hand.Points) { P = Point; }
		return Hand;
	}
	FVBoRDoorHand OnFace(double Angle, double Y, double X = 30, FVector Normal = FVector(0, -1, 0))
	{
		const FQuat Rotation(FVector::ZAxisVector, FMath::DegreesToRadians(Angle));
		return HandAt(Rotation.RotateVector(FVector(X, Y, 0)), Rotation.RotateVector(Normal));
	}
	UP2cEventObserver* Observe(AVBoRHingedPanel* Panel)
	{
		UP2cEventObserver* Observer = NewObject<UP2cEventObserver>(Panel);
		Panel->OnPanelUnlocked.AddDynamic(Observer, &UP2cEventObserver::Unlocked);
		Panel->OnPanelLocked.AddDynamic(Observer, &UP2cEventObserver::Locked);
		return Observer;
	}
	FIsdkGrabPose Pose(FVector Point) { return UIsdkFunctionLibrary::MakeGrabPoseStruct(11, Point, FQuat::Identity); }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP2cUnlockedStart, "vBoR.P2c.Panel.E1UnlockedStart", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP2cUnlockedStart::RunTest(const FString& Parameters)
{
	FP2cWorld W; AP2cTrackedPanel* Panel = W.World->SpawnActor<AP2cTrackedPanel>(); ConfigureLid(Panel);
	UP2cEventObserver* Observer = Observe(Panel); W.Play();
	TestFalse(TEXT("Panel starts unlocked"), Panel->GetMotion().bLocked);
	Panel->Hands[0] = OnFace(0, -2, 30, FVector(0, 1, 0)); Panel->Tick(0.1f);
	Panel->Hands[0] = OnFace(0, 2, 30, FVector(0, 1, 0)); Panel->Tick(0.1f);
	TestTrue(TEXT("Closed panel accepts push"), Panel->GetMotion().AngleDeg > 0);
	Panel->Hands[0] = HandAt(FVector(35, 0, 0));
	TestTrue(TEXT("Panel accepts grab"), FP2cTestAccess::Grab(Panel, 0, 0, Panel->Hands[0]));
	TestEqual(TEXT("No unlock event"), Observer->Unlocks, 0); TestEqual(TEXT("No lock event"), Observer->Locks, 0);
	Panel->ResetDoor(); TestFalse(TEXT("Reset keeps latch disabled"), Panel->GetMotion().bLocked);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP2cNoRelatch, "vBoR.P2c.Panel.E2CloseAndReopen", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP2cNoRelatch::RunTest(const FString& Parameters)
{
	FP2cWorld W; AP2cTrackedPanel* Panel = W.World->SpawnActor<AP2cTrackedPanel>(); ConfigureLid(Panel); W.Play();
	UP2cEventObserver* Observer = Observe(Panel);
	FP2cTestAccess::Motion(Panel).AngleDeg = 30;
	Panel->Hands[0] = OnFace(30, 0, 35); FP2cTestAccess::Grab(Panel, 0, 0, Panel->Hands[0]);
	Panel->Hands[0] = OnFace(0, 0, 35); Panel->Tick(1);
	TestEqual(TEXT("Closes to zero"), Panel->GetMotion().AngleDeg, 0.0);
	TestFalse(TEXT("No relatch"), Panel->GetMotion().bLocked);
	Panel->Hands[0] = OnFace(30, 0, 35); Panel->Tick(1);
	TestTrue(TEXT("Same grab immediately reopens"), FMath::IsNearlyEqual(Panel->GetMotion().AngleDeg, 30.0));
	TestEqual(TEXT("No latch count"), Panel->GetMotion().Metrics.Latches, 0);
	TestEqual(TEXT("No lock event"), Observer->Locks, 0); TestEqual(TEXT("No unlock event"), Observer->Unlocks, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP2cTopPush, "vBoR.P2c.Panel.E3TopPushCloses", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP2cTopPush::RunTest(const FString& Parameters)
{
	FP2cWorld W; AP2cTrackedPanel* Panel = W.World->SpawnActor<AP2cTrackedPanel>(); ConfigureLid(Panel); W.Play();
	FP2cTestAccess::Motion(Panel).AngleDeg = 60; FP2cTestAccess::Apply(Panel);
	const FTransform Frame = FP2cTestAccess::Frame(Panel);
	const FVBoRDoorHand Above = OnFace(60, 2), Below = OnFace(60, -2);
	TestTrue(TEXT("World hand moves downward through top face"), Frame.TransformPosition(Below.Points[0]).Z < Frame.TransformPosition(Above.Points[0]).Z);
	Panel->Hands[0] = Above; Panel->Tick(0.1f); Panel->Hands[0] = Below; Panel->Tick(1);
	const double Expected = 60 + FMath::RadiansToDegrees(FMath::Atan2(-2.0, 30.0) - FMath::Asin(1.5 / FMath::Sqrt(904.0)));
	TestTrue(TEXT("Top push follows geometric face solution"), FMath::IsNearlyEqual(Panel->GetMotion().AngleDeg, Expected, 0.001));
	TestEqual(TEXT("Top push counted"), Panel->GetMotion().Metrics.PushStarts[0][0], 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP2cBottomPush, "vBoR.P2c.Panel.E4BottomPushOpens", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP2cBottomPush::RunTest(const FString& Parameters)
{
	FP2cWorld W; AP2cTrackedPanel* Panel = W.World->SpawnActor<AP2cTrackedPanel>(); ConfigureLid(Panel); W.Play();
	Panel->Hands[0] = OnFace(0, -2, 30, FVector(0, 1, 0)); Panel->Tick(0.1f);
	Panel->Hands[0] = OnFace(0, 2, 30, FVector(0, 1, 0)); Panel->Tick(1);
	TestTrue(TEXT("Bottom push opens"), Panel->GetMotion().AngleDeg > 0);
	TestEqual(TEXT("Bottom push counted"), Panel->GetMotion().Metrics.PushStarts[0][1], 1);
	const FVector MeshRest(-27.5, 0, 70);
	TestTrue(TEXT("Opening raises mesh in car coordinates"), Panel->DoorMesh->GetComponentLocation().Z > MeshRest.Z);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP2cGrabLimit, "vBoR.P2c.Panel.E5GrabLimitAndStay", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP2cGrabLimit::RunTest(const FString& Parameters)
{
	FP2cWorld W; AP2cTrackedPanel* Panel = W.World->SpawnActor<AP2cTrackedPanel>(); ConfigureLid(Panel); W.Play();
	Panel->Hands[0] = OnFace(0, 0, 35); TestTrue(TEXT("Edge grab starts"), FP2cTestAccess::Grab(Panel, 0, 1, Panel->Hands[0]));
	Panel->Hands[0] = OnFace(120, 0, 35); Panel->Tick(1);
	TestTrue(TEXT("Grab clamps at 100 deg"), FMath::IsNearlyEqual(Panel->GetMotion().AngleDeg, 100.0));
	Panel->bPalmClosed[0] = false; Panel->Tick(0.1f); Panel->Hands[0] = {}; Panel->Tick(1);
	TestEqual(TEXT("Released grab cleared"), Panel->GetMotion().GrabHand, INDEX_NONE);
	TestTrue(TEXT("Released panel stays"), FMath::IsNearlyEqual(Panel->GetMotion().AngleDeg, 100.0));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP2cRejectPush, "vBoR.P2c.Panel.E6GlanceAndNearAxis", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP2cRejectPush::RunTest(const FString& Parameters)
{
	FP2cWorld W; AP2cTrackedPanel* Panel = W.World->SpawnActor<AP2cTrackedPanel>(); ConfigureLid(Panel); W.Play();
	FP2cTestAccess::Motion(Panel).AngleDeg = 60; FP2cTestAccess::Apply(Panel);
	Panel->Hands[0] = OnFace(60, 2, 30, FVector(1, 0, 0)); Panel->Tick(0.1f);
	Panel->Hands[0] = OnFace(60, -2, 30, FVector(1, 0, 0)); Panel->Tick(0.1f);
	TestEqual(TEXT("Glance keeps angle"), Panel->GetMotion().AngleDeg, 60.0);
	Panel->Hands[0] = {}; Panel->Tick(0.1f);
	Panel->Hands[0] = OnFace(60, 2, 10); Panel->Tick(0.1f); Panel->Hands[0] = OnFace(60, -2, 10); Panel->Tick(0.1f);
	TestEqual(TEXT("Near-axis push keeps angle"), Panel->GetMotion().AngleDeg, 60.0);
	TestEqual(TEXT("Glance counted"), Panel->GetMotion().Metrics.Ignored[static_cast<int32>(EVBoRDoorIgnore::PalmDirection)], 1);
	TestEqual(TEXT("Near-axis counted"), Panel->GetMotion().Metrics.Ignored[static_cast<int32>(EVBoRDoorIgnore::NearAxis)], 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP2cTracking, "vBoR.P2c.Panel.E7TrackingPauseAndRebase", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP2cTracking::RunTest(const FString& Parameters)
{
	FP2cWorld W; AP2cTrackedPanel* Panel = W.World->SpawnActor<AP2cTrackedPanel>(); ConfigureLid(Panel); W.Play();
	Panel->Hands[0] = OnFace(0, 0, 35); FP2cTestAccess::Grab(Panel, 0, 0, Panel->Hands[0]);
	Panel->Hands[0] = OnFace(30, 0, 35); Panel->Tick(1);
	const FTransform Before = Panel->DoorMesh->GetComponentTransform();
	Panel->Hands[0] = OnFace(160, 0, 35); Panel->Hands[0].bTracked = false; Panel->Tick(1);
	TestTrue(TEXT("One-second loss keeps mesh"), Panel->DoorMesh->GetComponentTransform().Equals(Before));
	TestTrue(TEXT("Loss is pause"), Panel->GetMotion().bPaused && Panel->GetMotion().GrabHand == 0);
	Panel->Hands[0].bTracked = true; Panel->Tick(0.1f);
	TestTrue(TEXT("Recovery frame has no jump"), Panel->DoorMesh->GetComponentTransform().Equals(Before));
	Panel->Hands[0] = OnFace(165, 0, 35); Panel->Tick(0.1f);
	TestTrue(TEXT("Only new five-degree motion applied"), FMath::IsNearlyEqual(Panel->GetMotion().AngleDeg, 35.0, 0.001));
	TestEqual(TEXT("Pause counted once"), Panel->GetMotion().Metrics.Pauses, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP2cPivot, "vBoR.P2c.Panel.E8CarOriginPivotBounds", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP2cPivot::RunTest(const FString& Parameters)
{
	FP2cWorld W; AP2cTrackedPanel* Panel = W.World->SpawnActor<AP2cTrackedPanel>();
	Panel->SetActorTransform(FTransform(FRotator(0, 35, 0), FVector(200, -100, 25))); ConfigureLid(Panel);
	TestTrue(TEXT("Mesh retains root rest transform"), Panel->DoorMesh->GetComponentTransform().Equals(Panel->MeshRestFromRoot * Panel->GetActorTransform(), 0.001));
	const FBox Bounds = Panel->DoorMesh->GetStaticMesh()->GetBoundingBox().TransformBy(Panel->DoorMesh->GetRelativeTransform());
	TestTrue(TEXT("Faces match bounds"), Panel->GetMotion().FaceMinCm.Equals(Bounds.Min) && Panel->GetMotion().FaceMaxCm.Equals(Bounds.Max));
	W.Play(); const FTransform Frame = FP2cTestAccess::Frame(Panel);
	TestTrue(TEXT("Top face is car up"), Frame.TransformVectorNoScale(FVector::YAxisVector).Equals(FVector::UpVector, 0.001));
	const FVector AxisPoint = Panel->HingePivot->GetComponentTransform().TransformPosition(FVector(0, 0, 10));
	const FVector Center = Panel->DoorMesh->GetComponentLocation();
	FP2cTestAccess::Motion(Panel).AngleDeg = 30; FP2cTestAccess::Apply(Panel);
	TestTrue(TEXT("Point on hinge axis fixed at 30 deg"), Panel->HingePivot->GetComponentTransform().TransformPosition(FVector(0, 0, 10)).Equals(AxisPoint, 0.001));
	const FVector Expected = Frame.TransformPosition(FQuat(FVector::ZAxisVector, PI / 6).RotateVector(Frame.InverseTransformPosition(Center)));
	TestTrue(TEXT("Mesh follows horizontal hinge arc"), Panel->DoorMesh->GetComponentLocation().Equals(Expected, 0.001));
	TestTrue(TEXT("Closed frame stays fixed after opening"), FP2cTestAccess::Frame(Panel).Equals(Frame, 0.001));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP2cOpenDoorHandle, "vBoR.P2c.Door.E9OpenHandleMovesBeforeTransfer", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP2cOpenDoorHandle::RunTest(const FString& Parameters)
{
	FP2cWorld W; AP2bTrackedDoor* Door = W.World->SpawnActor<AP2bTrackedDoor>(); AP2bTrackedPart* Part = W.World->SpawnActor<AP2bTrackedPart>(); W.Play(); Door->SetAxisHandle(0, Part);
	FVBoRDoorMotion& Motion = FP2cTestAccess::Motion(Door); Motion.bLocked = false; Motion.AngleDeg = 30; FP2cTestAccess::Apply(Door);
	Door->Hands[0] = HandAt(FVector(100, 9, 60));
	// 문을 따라 돈 손잡이의 쉬는 축 좌표로 입력함 (로컬 15/25 deg 유지, P2e §3·Q1)
	const FTransform HandleFrame = Part->AxisPivot->GetComponentTransform();
	FP2cTestAccess::Begin(Part, Pose(HandleFrame.TransformPosition(FVector(0, 10, 0)))); FP2cTestAccess::Move(Part, Pose(HandleFrame.TransformPosition(FQuat(FVector::XAxisVector, FMath::DegreesToRadians(15.0)).RotateVector(FVector(0, 10, 0)))));
	Part->Tick(0.01f);
	TestTrue(TEXT("Open-door handle follows hand below threshold"), FMath::IsNearlyEqual(Part->GetOpenRatio(), 0.5f, 0.001f));
	TestEqual(TEXT("No transfer below threshold"), Motion.GrabHand, INDEX_NONE);
	FP2cTestAccess::Move(Part, Pose(HandleFrame.TransformPosition(FQuat(FVector::XAxisVector, FMath::DegreesToRadians(25.0)).RotateVector(FVector(0, 10, 0))))); Part->Tick(0.01f);
	TestEqual(TEXT("Threshold transfers same hand"), Motion.GrabHand, 0);
	TestEqual(TEXT("No second unlock"), Motion.Metrics.Unlocks, 0);
	Door->Hands[0] = HandAt(FQuat(FVector::ZAxisVector, PI / 12).RotateVector(FVector(100, 9, 60))); Door->Tick(1);
	TestTrue(TEXT("Same hand pulls door"), FMath::IsNearlyEqual(Motion.AngleDeg, 45.0, 0.001));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP2cLockedDoorHandle, "vBoR.P2c.Door.E10LockedHandleThreshold", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP2cLockedDoorHandle::RunTest(const FString& Parameters)
{
	FP2cWorld W; AP2bTrackedDoor* Door = W.World->SpawnActor<AP2bTrackedDoor>(); AP2bTrackedPart* Part = W.World->SpawnActor<AP2bTrackedPart>(); W.Play(); Door->SetAxisHandle(0, Part);
	UP2cEventObserver* Observer = Observe(Door); Door->OnUnlocked.AddDynamic(Observer, &UP2cEventObserver::DoorUnlocked);
	Door->Hands[0] = HandAt(FVector(100, 9, 60)); FP2cTestAccess::Begin(Part, Pose(FVector(0, 10, 0)));
	FP2cTestAccess::Value(Part, 20); Part->Tick(0.01f); TestTrue(TEXT("Below threshold stays locked"), Door->GetMotion().bLocked);
	FP2cTestAccess::Value(Part, 25); Part->Tick(0.01f);
	TestFalse(TEXT("Threshold unlocks"), Door->GetMotion().bLocked); TestEqual(TEXT("Same hand retained"), Door->GetMotion().GrabHand, 0);
	TestEqual(TEXT("Door-typed event once"), Observer->DoorUnlocks, 1); TestTrue(TEXT("Event carries original door"), Observer->LastDoor == Door);
	TestEqual(TEXT("Panel event once"), Observer->Unlocks, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP2cEarlyRelease, "vBoR.P2c.Door.E11OpenHandleEarlyRelease", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP2cEarlyRelease::RunTest(const FString& Parameters)
{
	FP2cWorld W; AP2bTrackedDoor* Door = W.World->SpawnActor<AP2bTrackedDoor>(); AP2bTrackedPart* Part = W.World->SpawnActor<AP2bTrackedPart>(); W.Play(); Door->SetAxisHandle(0, Part);
	FVBoRDoorMotion& Motion = FP2cTestAccess::Motion(Door); Motion.bLocked = false; Motion.AngleDeg = 30; FP2cTestAccess::Apply(Door);
	Door->Hands[0] = HandAt(FVector(100, 9, 60)); FP2cTestAccess::Begin(Part, Pose(FVector(0, 10, 0))); FP2cTestAccess::Value(Part, 15); Part->Tick(0.01f);
	TestFalse(TEXT("Door API also rejects below threshold when unlocked"), Door->BeginGrabFromAxisPart(Part));
	FP2cTestAccess::Release(Part); Part->Tick(1); Door->Tick(1);
	TestTrue(TEXT("Handle returns"), FMath::IsNearlyZero(Part->GetOpenRatio())); TestEqual(TEXT("Door stays at 30 deg"), Motion.AngleDeg, 30.0);
	TestEqual(TEXT("No door grab"), Motion.Metrics.Grabs[0][0], 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP2cDoorCompatibility, "vBoR.P2c.Door.E12CompatibilityAndOptionalLatch", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP2cDoorCompatibility::RunTest(const FString& Parameters)
{
	FP2cWorld W; AVBoRHingedDoor* Door = W.World->SpawnActor<AVBoRHingedDoor>(); AP2cTrackedPanel* Panel = W.World->SpawnActor<AP2cTrackedPanel>(); Panel->bUseLatch = true; ConfigureLid(Panel); W.Play();
	TestTrue(TEXT("Door is hinged panel subclass"), Door->IsA(AVBoRHingedPanel::StaticClass())); TestTrue(TEXT("Door retains lock default"), Door->GetMotion().bLocked);
	TestTrue(TEXT("Outside handle unchanged"), Door->OutsideHandle->GetRelativeLocation().Equals(FVector(100, 9, 60)));
	TestTrue(TEXT("Inside handle unchanged"), Door->InsideHandle->GetRelativeLocation().Equals(FVector(85, -9, 50)));
	UP2cEventObserver* Observer = Observe(Panel); Panel->Hands[0] = OnFace(0, 0, 35); FP2cTestAccess::Grab(Panel, 0, 0, Panel->Hands[0]);
	Panel->Hands[0] = OnFace(30, 0, 35); Panel->Tick(1); Panel->Hands[0] = OnFace(0, 0, 35); Panel->Tick(1);
	TestTrue(TEXT("Optional latch works on panel"), Panel->GetMotion().bLocked); TestEqual(TEXT("Panel unlock event"), Observer->Unlocks, 1); TestEqual(TEXT("Panel latch event"), Observer->Locks, 1);
	Door->ResetDoor(); TestTrue(TEXT("Door reset API keeps locked zero"), Door->GetMotion().bLocked && Door->GetMotion().AngleDeg == 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP2cP3Lid, "vBoR.P2c.P3.E13ConsolePushAndGrab", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP2cP3Lid::RunTest(const FString& Parameters)
{
	FP2cWorld W; AP3VehicleMock* Car = W.World->SpawnActor<AP3VehicleMock>(); W.Play();
	AVBoRHingedPanel* Lid = Car->ConsoleLid;
	if (!TestNotNull(TEXT("P3 creates hinged panel lid"), Lid)) { return false; }
	TestTrue(TEXT("Lid actor remains car origin"), Lid->GetActorTransform().Equals(Car->GetActorTransform(), 0.001));
	TestTrue(TEXT("P2d lid starts latched"), Lid->GetMotion().bLocked);
	TestNull(TEXT("P2d console uses front grab instead of axis handle"), Lid->OutsideAxisHandle.Get());
	TestTrue(TEXT("Lid keeps original mesh size"), Lid->DoorMesh->GetComponentScale().Equals(FVector(0.35, 0.24, 0.03)));
	TestTrue(TEXT("Local Y is top face"), FP2cTestAccess::Frame(Lid).TransformVectorNoScale(FVector::YAxisVector).Equals(FVector::UpVector, 0.001));
	FVBoRDoorHand Hands[2] = {OnFace(0, -2, 30, FVector(0, 1, 0)), {}};
	FVBoRDoorMotion& Motion = FP2cTestAccess::Motion(Lid); Motion.Advance(Hands, 0.1); Hands[0] = OnFace(0, 2, 30, FVector(0, 1, 0)); Motion.Advance(Hands, 1); FP2cTestAccess::Apply(Lid);
	TestEqual(TEXT("P2d closed console rejects bottom push"), Motion.AngleDeg, 0.0);
	TestTrue(TEXT("P2d explicit Unlock succeeds"), Lid->Unlock());
	Hands[0] = OnFace(0, -2, 30, FVector(0, 1, 0)); Motion.Advance(Hands, 0.1);
	Hands[0] = OnFace(0, 2, 30, FVector(0, 1, 0)); Motion.Advance(Hands, 1); FP2cTestAccess::Apply(Lid, 0, Motion.Metrics.Unlocks);
	TestEqual(TEXT("Actual P3 unlocked lid rejects bottom push"), Motion.AngleDeg, 0.0);
	TestTrue(TEXT("Actual P3 lid accepts edge grab"), FP2cTestAccess::Grab(Lid, 0, 0, OnFace(Motion.AngleDeg, 0, 35)));
	Hands[0] = OnFace(120, 0, 35); Motion.Advance(Hands, 1); FP2cTestAccess::Apply(Lid);
	TestTrue(TEXT("Actual P3 lid clamp 100 deg"), FMath::IsNearlyEqual(Motion.AngleDeg, 100.0));
	return true;
}
#endif
