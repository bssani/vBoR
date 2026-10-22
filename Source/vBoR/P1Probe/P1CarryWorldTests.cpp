#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "../Interaction/VBoRCarryObject.h"
#include "../Interaction/VBoRLoadTarget.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "PhysicsEngine/BodySetup.h"
#include "GameFramework/WorldSettings.h"
#include "Interaction/IsdkGrabbableComponent.h"
#include "Interaction/IsdkThrowable.h"
#include "P1CarryTestObserver.h"
#include "P1LoadBench.h"
#include "P1TrunkMock.h"
#include "../Interaction/VBoRHandRigPawn.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace
{
	AActor* AddBlocker(UWorld* World, const FVector& Center, const FVector& Extent)
	{
		AActor* Actor = World->SpawnActor<AActor>();
		UBoxComponent* Box = NewObject<UBoxComponent>(Actor);
		Actor->SetRootComponent(Box);
		Box->SetBoxExtent(Extent);
		Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		Box->SetCollisionResponseToAllChannels(ECR_Block);
		Box->RegisterComponent();
		Actor->SetActorLocation(Center);
		return Actor;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP1CarryCollisionTest, "vBoR.P1Carry.WorldCollision", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP1CarryCollisionTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	AddBlocker(World, FVector(0, 0, 100), FVector(5, 100, 100));
	AVBoRCarryObject* Object = World->SpawnActor<AVBoRCarryObject>();
	Object->SetActorTransform(FTransform(FQuat::Identity, FVector(-100, 0, 110), FVector(0.2)));
	const FVector OriginalScale = Object->GetActorScale3D();
	TestTrue(TEXT("Starting object is clear"), Object->IsPoseClear(Object->GetActorTransform()));
	FTransform Desired = Object->GetActorTransform();
	Desired.SetLocation(FVector(100, 0, 110));
	Desired.SetScale3D(FVector(0.01));
	TestFalse(TEXT("Swept movement is blocked by wall"), Object->TryMoveToPose(Desired));
	TestTrue(TEXT("Object stops before blocking wall"), Object->GetActorLocation().X <= -14.99);
	TestTrue(TEXT("Visible object does not overlap at swept stop"), Object->IsPoseClear(Object->GetActorTransform()));
	TestTrue(TEXT("Movement cannot silently resize object"), Object->GetActorScale3D().Equals(OriginalScale));
	Desired.SetLocation(FVector(-70, 0, 110));
	TestTrue(TEXT("Withdrawing hand can move away from blocker"), Object->TryMoveToPose(Desired));
	Object->SetActorTransform(FTransform(FQuat::Identity, FVector(-30, 0, 110), FVector(0.2, 0.8, 0.2)));
	Desired = Object->GetActorTransform();
	Desired.SetRotation(FRotator(0, 90, 0).Quaternion());
	TestFalse(TEXT("Overlapping rotation rejected"), Object->TryMoveToPose(Desired));
	TestTrue(TEXT("Previous rotation retained"), Object->GetActorRotation().IsNearlyZero());
	World->EndPlay(EEndPlayReason::Quit);
	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP1CarryDropTest, "vBoR.P1Carry.DropToSurface", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP1CarryDropTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	AddBlocker(World, FVector(0, 0, 50), FVector(100, 100, 5));
	AVBoRCarryObject* Object = World->SpawnActor<AVBoRCarryObject>();
	Object->SetActorTransform(FTransform(FRotator(0, 30, 0), FVector(5, 6, 110), FVector(0.2)));
	FTransform Dropped = Object->GetActorTransform();
	Dropped.SetLocation(FVBoRCarryMotion::Drop(Dropped.GetLocation(), 200, 1));
	TestFalse(TEXT("Fall stops at first surface"), Object->TryMoveToPose(Dropped));
	TestTrue(TEXT("Object rests with bottom on first top surface"), FMath::IsNearlyEqual(Object->GetActorLocation().Z, 65.0, 0.01));
	TestTrue(TEXT("Drop does not change horizontal position"), FVector2D(Object->GetActorLocation()).Equals(FVector2D(5, 6)));
	TestTrue(TEXT("Drop does not rotate object"), Object->GetActorRotation().Equals(FRotator(0, 30, 0)));
	TestTrue(TEXT("Drop never enables physics or throw velocity"), !Object->Mesh->IsSimulatingPhysics() && Object->Mesh->GetPhysicsLinearVelocity().IsNearlyZero());
	Object->SetActorLocation(FVector(5, 6, 65));
	FTransform Lifted = Object->GetActorTransform();
	Lifted.SetLocation(FVector(5, 6, 100));
	TestTrue(TEXT("Bag initially touching its stand can be lifted"), Object->TryMoveToPose(Lifted));
	Object->SetActorLocation(FVector(5, 6, 65));
	Lifted.SetLocation(FVector(20, 6, 65));
	TestTrue(TEXT("Initial surface contact does not block horizontal withdrawal"), Object->TryMoveToPose(Lifted));
	World->EndPlay(EEndPlayReason::Quit);
	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP1CarryPolicyTest, "vBoR.P1Carry.ComponentsAndTarget", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP1CarryPolicyTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	AVBoRCarryObject* Object = World->SpawnActor<AVBoRCarryObject>();
	Object->SetActorTransform(FTransform(FQuat::Identity, FVector(0, 0, 120), FVector(0.2)));
	UMaterialInterface* OriginalMaterial = Object->Mesh->GetMaterial(0);
	AVBoRLoadTarget* Goal = World->SpawnActor<AVBoRLoadTarget>();
	Goal->SetActorLocation(FVector(0, 0, 120));
	Goal->GoalSizeCm = FVector(20);
	Goal->LoadingRegion->SetBoxExtent(FVector(10));
	Goal->OnConstruction(Goal->GetActorTransform());
	World->InitializeActorsForPlay(FURL());
	World->GetWorldSettings()->NotifyBeginPlay();
	TestTrue(TEXT("Palm input allowed"), Object->Grabbable->IsGrabInputMethodAllowed(EIsdkGrabInputMethod::Palm));
	TestFalse(TEXT("Pinch input disabled"), Object->Grabbable->IsGrabInputMethodAllowed(EIsdkGrabInputMethod::Pinch));
	TestTrue(TEXT("Only near hand grab detected"), Object->Grabbable->IsHandGrabAllowed() && !Object->Grabbable->IsDistanceGrabAllowed() && !Object->Grabbable->IsRayGrabAllowed());
	TestFalse(TEXT("Throwable disabled before play"), Object->GrabTransformer->IsThrowingEnabled());
	TestTrue(TEXT("Second hand allowed at SDK selection"), Object->GrabTransformer->GetMultiGrabBehavior() == EIsdkMultiGrabBehavior::MultiGrab);
	TArray<UIsdkThrowable*> Throwables;
	Object->GetComponents(Throwables);
	TestEqual(TEXT("No throwable component created at BeginPlay"), Throwables.Num(), 0);
	TestTrue(TEXT("Custom one-hand transformer bound"), Object->GrabTransformer->GetSingleGrabTransformerObject()->IsA<UVBoRCarryTransformer>());
	TestTrue(TEXT("Two-hand SDK uses same constrained transformer"), Object->GrabTransformer->GetMultiGrabTransformerObject() == Object->GrabTransformer->GetSingleGrabTransformerObject());
	TestTrue(TEXT("Mesh itself is grab collider"), Object->Grabbable->GetGrabCollider() == Object->Mesh.Get());
	TestTrue(TEXT("Mesh original material preserved"), Object->Mesh->GetMaterial(0) == OriginalMaterial);
	TestNull(TEXT("Idle has no state overlay"), Object->Mesh->GetOverlayMaterial());
	TestTrue(TEXT("Target accepts unchanged shape in clear pose"), Goal->IsStaticFit(Object));
	TestTrue(TEXT("Inside target at same pose"), Goal->IsObjectInside(Object));
	AddBlocker(World, FVector(0, 0, 120), FVector(5));
	TestFalse(TEXT("Target overlap prevents static fit and assistance"), Goal->IsStaticFit(Object));
	TestFalse(TEXT("Overlap prevents inside-clear result"), Goal->IsObjectInside(Object));
	Object->ResetToPose(FTransform(FVector(100, 0, 120)));
	TestTrue(TEXT("Reset preserves geometry scale"), Object->GetActorScale3D().Equals(FVector(0.2)));
	World->EndPlay(EEndPlayReason::Quit);
	World->DestroyWorld(false);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP1CarryPlacementTest, "vBoR.P1Carry.PlacementOutcome", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP1CarryPlacementTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	AddBlocker(World, FVector(0, 0, 50), FVector(300, 300, 5));
	AVBoRCarryObject* Object = World->SpawnActor<AVBoRCarryObject>();
	Object->SetActorTransform(FTransform(FQuat::Identity, FVector(100, 0, 120), FVector(0.2)));
	AVBoRLoadTarget* Target = World->SpawnActor<AVBoRLoadTarget>();
	Target->SetActorLocation(FVector(0, 0, 65));
	Target->GoalSizeCm = FVector(20);
	Target->LoadingRegion->SetBoxExtent(FVector(10));
	Target->OnConstruction(Target->GetActorTransform());
	Object->LoadTarget = Target;
	World->InitializeActorsForPlay(FURL());
	World->GetWorldSettings()->NotifyBeginPlay();
	UP1CarryTestObserver* Observer = NewObject<UP1CarryTestObserver>(World);
	Object->OnSeated.AddDynamic(Observer, &UP1CarryTestObserver::OnSeated);
	Object->Session.Begin(0, Object->GetActorTransform(), Object->GetActorTransform(), 0);
	Object->ReleaseCarry();
	Object->Tick(1);
	TestEqual(TEXT("Outside target drop does not emit seated event"), Observer->SeatedCount, 0);
	TestEqual(TEXT("Outside target has released state"), Object->GetStateText(), FString(TEXT("Released (outside target)")));
	TestFalse(TEXT("Original outside drop remains outside loading region"), Target->IsObjectInside(Object));
	if (UMaterialInstanceDynamic* Overlay = Cast<UMaterialInstanceDynamic>(Object->Mesh->GetOverlayMaterial()))
	{
		FLinearColor Color;
		TestTrue(TEXT("Overlay color readable"), Overlay->GetVectorParameterValue(FMaterialParameterInfo(TEXT("ColorParam")), Color));
		TestFalse(TEXT("Outside drop never uses seated green"), Color.Equals(Object->SeatedColor));
	}
	TestTrue(TEXT("Outside drop result identifies outside target"), Object->GetMeasurementText().Contains(TEXT("vertical drop -> outside target")));
	Object->ResetToPose(FTransform(FVector(0, 0, 120)));
	Object->Session.Begin(0, Object->GetActorTransform(), Object->GetActorTransform(), 0);
	Object->ReleaseCarry();
	TestTrue(TEXT("Above target and beyond assistance range starts dropping"), Object->GetStateText().Contains(TEXT("Dropping")));
	Object->Tick(1);
	TestEqual(TEXT("Inside target drop emits seated event once"), Observer->SeatedCount, 1);
	TestEqual(TEXT("Inside target drop has seated state"), Object->GetStateText(), FString(TEXT("Seated")));
	TestTrue(TEXT("Inside drop result identifies target seating"), Object->GetMeasurementText().Contains(TEXT("vertical drop -> inside target")));
	Object->Tick(1);
	TestEqual(TEXT("Resting inside target does not repeat event"), Observer->SeatedCount, 1);
	World->EndPlay(EEndPlayReason::Quit);
	World->DestroyWorld(false);

	UWorld* BenchWorld = UWorld::CreateWorld(EWorldType::Game, false);
	APlayerController* Controller = BenchWorld->SpawnActor<APlayerController>();
	AVBoRHandRigPawn* Pawn = BenchWorld->SpawnActor<AVBoRHandRigPawn>();
	Controller->Possess(Pawn);
	AP1LoadBench* Bench = BenchWorld->SpawnActor<AP1LoadBench>();
	BenchWorld->InitializeActorsForPlay(FURL());
	BenchWorld->GetWorldSettings()->NotifyBeginPlay();
	Bench->PlaceFromHead(FTransform(FRotator(0, 37, 0), FVector(200, -100, 165)));
	AVBoRCarryObject* Bag = Bench->Bag;
	if (!TestNotNull(TEXT("Loading region fixture has bag"), Bag))
	{
		BenchWorld->DestroyWorld(false);
		return false;
	}
	UP1CarryTestObserver* BagObserver = NewObject<UP1CarryTestObserver>(BenchWorld);
	Bag->OnSeated.AddDynamic(BagObserver, &UP1CarryTestObserver::OnSeated);
	const FTransform TrunkFrame = Bench->Trunk->GetActorTransform();
	FTransform DropPose = Bag->GetActorTransform();
	// Only 5 cm width clearance; offset 20 cm in depth (outside assistance, inside trunk, P1 R4-1)
	DropPose.SetLocation(TrunkFrame.TransformPosition(FVector(65, 0, 120)));
	Bag->ResetToPose(DropPose);
	Bag->Session.Begin(0, Bag->GetActorTransform(), Bag->GetActorTransform(), 0);
	Bag->ReleaseCarry();
	TestTrue(TEXT("Offset in trunk starts drop, not assistance"), Bag->GetStateText().Contains(TEXT("Dropping")));
	Bag->Tick(1);
	TestTrue(TEXT("20 cm from goal still inside trunk loading region"), Bench->Target->IsObjectInside(Bag));
	TestEqual(TEXT("Offset drop emits seated event once"), BagObserver->SeatedCount, 1);
	TestEqual(TEXT("Offset drop is seated"), Bag->GetStateText(), FString(TEXT("Seated")));
	TestTrue(TEXT("Offset drop records drop inside"), Bag->GetMetrics().Outcome == EVBoRCarryOutcome::DroppedInside);
	UMaterialInstanceDynamic* Overlay = Cast<UMaterialInstanceDynamic>(Bag->Mesh->GetOverlayMaterial());
	if (TestNotNull(TEXT("Seated offset drop has overlay"), Overlay))
	{
		FLinearColor Color;
		TestTrue(TEXT("Seated color readable"), Overlay->GetVectorParameterValue(FMaterialParameterInfo(TEXT("ColorParam")), Color));
		TestTrue(TEXT("Offset drop uses seated green"), Color.Equals(Bag->SeatedColor));
	}
	Bag->Tick(1);
	TestEqual(TEXT("Offset rest does not repeat seated event"), BagObserver->SeatedCount, 1);
	DropPose.SetLocation(TrunkFrame.TransformPosition(FVector(0, 0, 120)));
	Bag->ResetToPose(DropPose);
	Bag->Session.Begin(0, Bag->GetActorTransform(), Bag->GetActorTransform(), 0);
	Bag->ReleaseCarry();
	Bag->Tick(1);
	TestFalse(TEXT("Bag straddling entrance and bumper is outside loading region"), Bench->Target->IsObjectInside(Bag));
	TestEqual(TEXT("Entrance rest does not emit seated event"), BagObserver->SeatedCount, 1);
	TestTrue(TEXT("Entrance rest records outside drop"), Bag->GetMetrics().Outcome == EVBoRCarryOutcome::DroppedOutside);
	TestEqual(TEXT("Entrance rest has released state"), Bag->GetStateText(), FString(TEXT("Released (outside target)")));
	Overlay = Cast<UMaterialInstanceDynamic>(Bag->Mesh->GetOverlayMaterial());
	if (TestNotNull(TEXT("Entrance rest has overlay"), Overlay))
	{
		FLinearColor Color;
		TestTrue(TEXT("Released color readable"), Overlay->GetVectorParameterValue(FMaterialParameterInfo(TEXT("ColorParam")), Color));
		TestTrue(TEXT("Entrance rest uses released gray"), Color.Equals(Bag->ReleasedColor));
	}
	BenchWorld->EndPlay(EEndPlayReason::Quit);
	BenchWorld->DestroyWorld(false);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP1CarryMissingCollisionTest, "vBoR.P1Carry.MissingSimpleCollision", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP1CarryMissingCollisionTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	AVBoRCarryObject* Object = World->SpawnActor<AVBoRCarryObject>();
	// Test-only duplicate (preserve shared asset collision, P1 R4-2)
	UStaticMesh* MeshWithoutCollision = DuplicateObject<UStaticMesh>(Object->Mesh->GetStaticMesh(), World);
	MeshWithoutCollision->SetBodySetup(NewObject<UBodySetup>(MeshWithoutCollision));
	Object->Mesh->SetStaticMesh(MeshWithoutCollision);
	AddExpectedMessagePlain(TEXT("No simple collision on carry mesh"), ELogVerbosity::Warning);
	World->InitializeActorsForPlay(FURL());
	World->GetWorldSettings()->NotifyBeginPlay();
	TestEqual(TEXT("Missing collision has actionable state at startup"), Object->GetStateText(), FString(TEXT("No simple collision")));
	FTransform Desired = Object->GetActorTransform();
	Desired.AddToTranslation(FVector(0, 0, 50));
	const FTransform Before = Object->GetActorTransform();
	TestFalse(TEXT("Missing collision cannot move"), Object->TryMoveToPose(Desired));
	TestTrue(TEXT("Missing collision leaves pose unchanged"), Object->GetActorTransform().Equals(Before));
	Object->Tick(1);
	TestEqual(TEXT("Missing collision warning persists after tick"), Object->GetStateText(), FString(TEXT("No simple collision")));
	AP1LoadBench* Bench = World->SpawnActor<AP1LoadBench>();
	Bench->Bag = Object;
	Bench->Tick(0);
	TestTrue(TEXT("Bench panel shows missing collision cause"), Bench->Panel->Text.ToString().Contains(TEXT("Stand box: No simple collision")));
	World->EndPlay(EEndPlayReason::Quit);
	World->DestroyWorld(false);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP1CarryBlockedGapTest, "vBoR.P1Carry.RequestedActualGap", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP1CarryBlockedGapTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	AddBlocker(World, FVector(0, 0, 100), FVector(5, 100, 100));
	AVBoRCarryObject* Object = World->SpawnActor<AVBoRCarryObject>();
	Object->SetActorTransform(FTransform(FQuat::Identity, FVector(-25, 0, 110), FVector(0.2)));
	Object->Session.Begin(0, FTransform(FVector(-75, 0, 110)), Object->GetActorTransform(), 0);
	FTransform Desired;
	Object->Session.Update(true, FTransform(FVector(-55, 0, 110)), Object->GetActorTransform(), 1, 15, Desired);
	Object->MoveHeldPose(Desired, 1.0f / 60.0f);
	TestTrue(TEXT("Object is stopped at wall"), Object->IsBlocked());
	TestTrue(TEXT("Blocked requested/actual gap excludes 50 cm grab offset"), FMath::IsNearlyEqual(Object->MaxRequestedGapCm, 10.0, 0.01));
	World->EndPlay(EEndPlayReason::Quit);
	World->DestroyWorld(false);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP1CarryCumulativeTest, "vBoR.P1Carry.CumulativeRegrab", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP1CarryCumulativeTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	APlayerController* Controller = World->SpawnActor<APlayerController>();
	AVBoRHandRigPawn* Pawn = World->SpawnActor<AVBoRHandRigPawn>();
	Controller->Possess(Pawn);
	AP1LoadBench* Bench = World->SpawnActor<AP1LoadBench>();
	World->InitializeActorsForPlay(FURL());
	World->GetWorldSettings()->NotifyBeginPlay();
	Bench->PlaceFromHead(FTransform(FVector(0, 0, 165)));
	AVBoRCarryObject* Bag = Bench->Bag;
	if (!TestNotNull(TEXT("Bag exists for cumulative lifecycle"), Bag)) { World->DestroyWorld(false); return false; }
	Bag->BeginCarry(0, nullptr, Bag->GetActorTransform());
	Bag->Session.GrabAt = World->GetTimeSeconds() - 2;
	Bag->ReleaseCarry();
	Bag->Tick(1);
	TestEqual(TEXT("First outside release increments displayed cumulative result"), Bench->GetTotals().DroppedOutside, 1);
	Bag->SetActorLocation(Bench->Target->GetActorLocation() + FVector(0, 0, 30));
	Bag->BeginCarry(0, nullptr, Bag->GetActorTransform());
	TestEqual(TEXT("Regrab cannot remove previously completed result"), Bench->GetTotals().DroppedOutside, 1);
	TestEqual(TEXT("Regrab retains first completed hold duration"), Bench->GetTotals().HeldSeconds, 2.0);
	Bag->Session.GrabAt = World->GetTimeSeconds() - 3;
	Bag->ReleaseCarry();
	Bag->Tick(1);
	TestEqual(TEXT("Second inside release increments drop seating"), Bench->GetTotals().DroppedInside, 1);
	TestEqual(TEXT("Completed holds sum across regrabs"), Bench->GetTotals().HeldSeconds, 5.0);
	Bag->bUsePlacementAssist = true;
	Bag->SetActorLocation(Bench->Target->GetActorLocation() + FVector(0, 0, 5));
	Bag->BeginCarry(0, nullptr, Bag->GetActorTransform());
	Bag->Session.GrabAt = World->GetTimeSeconds() - 1;
	Bag->ReleaseCarry();
	TestTrue(TEXT("Clear nearby target starts assistance"), Bag->bAssisting);
	Bag->AssistStartedAt = World->GetTimeSeconds() - 0.3;
	Bag->Tick(0.1);
	TestEqual(TEXT("Actual assistance completion increments assisted count"), Bench->GetTotals().Assisted, 1);
	Bench->ResetBagForNextTrial();
	TestEqual(TEXT("B preserves first result and final result once each"), Bench->GetTotals().DroppedOutside, 1);
	TestEqual(TEXT("B preserves inside seating once"), Bench->GetTotals().DroppedInside, 1);
	TestEqual(TEXT("B preserves all three summed hold durations"), Bench->GetTotals().HeldSeconds, 6.0);
	TestEqual(TEXT("B preserves assisted result"), Bench->GetTotals().Assisted, 1);
	Bench->ResetBagForNextTrial();
	TestEqual(TEXT("Empty B cannot duplicate completed results"), Bench->GetTotals().DroppedOutside + Bench->GetTotals().DroppedInside, 2);
	Bench->PlaceFromHead(FTransform(FVector(0, 0, 120)));
	TestEqual(TEXT("R clears all completed results"), Bench->GetTotals().DroppedOutside + Bench->GetTotals().DroppedInside, 0);
	TestEqual(TEXT("R clears assistance and held duration"), Bench->GetTotals().Assisted, 0);
	TestEqual(TEXT("R clears accumulated hold duration"), Bench->GetTotals().HeldSeconds, 0.0);
	World->EndPlay(EEndPlayReason::Quit);
	World->DestroyWorld(false);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP1CarryCylinderShapeTest, "vBoR.P1Carry.CylinderShape", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP1CarryCylinderShapeTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	AddBlocker(World, FVector(0, 0, -5), FVector(300, 300, 5));
	AVBoRCarryObject* Object = World->SpawnActor<AVBoRCarryObject>();
	Object->Mesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder")));
	const FQuat Rest = FRotator(0, 0, 90).Quaternion();
	AVBoRLoadTarget* Target = World->SpawnActor<AVBoRLoadTarget>();
	Target->SetActorTransform(FTransform(Rest, FVector(0, 0, 15)));
	Target->GoalSizeCm = FVector(28, 28, 120);
	Target->LoadingRegion->SetBoxExtent(Target->GoalSizeCm * 0.5);
	for (double Axial : {0.0, 15.0, 45.0, 90.0})
	{
		Object->SetActorTransform(FTransform(Rest * FQuat(FVector::UpVector, FMath::DegreesToRadians(Axial)), FVector(0, 0, 15), FVector(0.28, 0.28, 1.2)));
		TestTrue(TEXT("Cylinder above floor stays clear after axial rotation"), Object->IsPoseClear(Object->GetActorTransform()));
		TestTrue(TEXT("Axial rotation cannot enlarge cylinder containment"), Target->IsObjectInside(Object));
		FTransform Shifted = Object->GetActorTransform();
		Shifted.AddToTranslation(FVector(20, 0, 0));
		TestTrue(TEXT("Axially rotated cylinder moves horizontally 1 cm above floor"), Object->TryMoveToPose(Shifted));
	}
	World->EndPlay(EEndPlayReason::Quit);
	World->DestroyWorld(false);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP1CarryTwoHandLifecycleTest, "vBoR.P1Carry.TwoHandActorLifecycle", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP1CarryTwoHandLifecycleTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	APlayerController* Controller = World->SpawnActor<APlayerController>();
	AVBoRHandRigPawn* Pawn = World->SpawnActor<AVBoRHandRigPawn>();
	Controller->Possess(Pawn);
	AP1LoadBench* Bench = World->SpawnActor<AP1LoadBench>();
	World->InitializeActorsForPlay(FURL());
	World->GetWorldSettings()->NotifyBeginPlay();
	Bench->PlaceFromHead(FTransform(FVector(0, 0, 165)));
	AVBoRCarryObject* Bag = Bench->Bag;
	if (!TestNotNull(TEXT("Two-hand lifecycle bag exists"), Bag)) { World->DestroyWorld(false); return false; }
	UP1CarryTestObserver* Observer = NewObject<UP1CarryTestObserver>(World);
	Bag->OnGrabbed.AddDynamic(Observer, &UP1CarryTestObserver::OnGrabbed);
	Bag->OnCarryReleased.AddDynamic(Observer, &UP1CarryTestObserver::OnReleased);
	const FTransform Before = Bag->GetActorTransform();
	FTransform Hands[2] = {FTransform(Before.GetLocation() + FVector(0, -30, 0)), FTransform(Before.GetLocation() + FVector(0, 30, 0))};
	TestTrue(TEXT("Actor accepts first palm"), Bag->BeginCarry(0, nullptr, Hands[0]));
	TestTrue(TEXT("Actor accepts second palm"), Bag->BeginCarry(1, nullptr, Hands[1]));
	TestTrue(TEXT("Second accepted palm does not move actor"), Bag->GetActorTransform().Equals(Before));
	TestFalse(TEXT("Duplicate palm is not another success"), Bag->BeginCarry(1, nullptr, Hands[1]));
	TestEqual(TEXT("Two distinct palms produce two grab events"), Observer->GrabbedCount, 2);
	TestEqual(TEXT("Actor mode reports two hands"), Bag->GetStateText(), FString(TEXT("Held (two hands)")));
	TestEqual(TEXT("Accepted second palm records one entry"), Bag->GetMetrics().TwoHandEntries, 1);
	TestTrue(TEXT("Measurement snapshot includes current two-hand mode"), Bag->GetMeasurementText().Contains(TEXT("mode=two hands")));
	FTransform Desired;
	Bag->Session.UpdateHands(false, Hands, Before, World->GetTimeSeconds(), 15, Desired);
	Hands[0].AddToTranslation(FVector(200, 0, 0));
	Hands[1].AddToTranslation(FVector(200, 0, 0));
	Bag->ReleaseCarry(0);
	Bag->ReleaseCarry(1);
	TestTrue(TEXT("Paused actor retains both owning palms"), Bag->Session.HasHand(0) && Bag->Session.HasHand(1));
	TestEqual(TEXT("Paused actor never emits release event"), Observer->ReleasedCount, 0);
	TestTrue(TEXT("Paused actor stays in place"), Bag->GetActorTransform().Equals(Before));
	TestEqual(TEXT("Paused actor displays pause"), Bag->GetStateText(), FString(TEXT("Paused (tracking)")));
	TestFalse(TEXT("Actor session recovery rebases without movement"), Bag->Session.UpdateHands(true, Hands, Before, World->GetTimeSeconds(), 15, Desired));
	TestTrue(TEXT("Recovery request and actor are unchanged"), Desired.Equals(Before) && Bag->GetActorTransform().Equals(Before));
	Bag->Session.LastTime = World->GetTimeSeconds() - 2;
	Bag->ReleaseCarry(0);
	TestTrue(TEXT("First accepted release keeps second palm owning"), Bag->Session.HasHand(1) && !Bag->Session.HasHand(0));
	TestTrue(TEXT("Two-to-one release does not move actor"), Bag->GetActorTransform().Equals(Before));
	TestEqual(TEXT("One palm release never starts drop or emits event"), Observer->ReleasedCount, 0);
	TestFalse(TEXT("One palm remains held rather than dropping"), Bag->bDropping);
	Bag->ReleaseCarry(1);
	TestEqual(TEXT("Last palm emits release once"), Observer->ReleasedCount, 1);
	Bag->ReleaseCarry(1);
	TestEqual(TEXT("Duplicate last release cannot duplicate event"), Observer->ReleasedCount, 1);
	TestEqual(TEXT("First two-hand ownership totals two seconds"), Bench->GetTotals().TwoHandSeconds, 2.0);
	Bag->BeginCarry(1, nullptr, Hands[1]);
	Bag->BeginCarry(0, nullptr, Hands[0]);
	Bag->Session.LastTime = World->GetTimeSeconds() - 3;
	Bag->ReleaseCarry(0);
	Bag->ReleaseCarry(1);
	TestEqual(TEXT("Regrab keeps both two-hand entries"), Bench->GetTotals().TwoHandEntries, 2);
	TestEqual(TEXT("Regrab sums both two-hand durations"), Bench->GetTotals().TwoHandSeconds, 5.0);
	Bench->ResetBagForNextTrial();
	TestEqual(TEXT("B preserves two-hand entries"), Bench->GetTotals().TwoHandEntries, 2);
	TestEqual(TEXT("B preserves two-hand duration once"), Bench->GetTotals().TwoHandSeconds, 5.0);
	Bench->ResetBagForNextTrial();
	TestEqual(TEXT("Empty B cannot duplicate two-hand entries"), Bench->GetTotals().TwoHandEntries, 2);
	TestEqual(TEXT("Empty B cannot duplicate two-hand duration"), Bench->GetTotals().TwoHandSeconds, 5.0);
	Bench->PlaceFromHead(FTransform(FVector(0, 0, 165)));
	TestEqual(TEXT("R clears two-hand entries"), Bench->GetTotals().TwoHandEntries, 0);
	TestEqual(TEXT("R clears two-hand duration"), Bench->GetTotals().TwoHandSeconds, 0.0);
	World->EndPlay(EEndPlayReason::Quit);
	World->DestroyWorld(false);
	return true;
}
#endif
