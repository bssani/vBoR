#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "../Interaction/VBoRCarryObject.h"
#include "../Interaction/VBoRLoadTarget.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
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
	Goal->OnConstruction(Goal->GetActorTransform());
	World->InitializeActorsForPlay(FURL());
	World->GetWorldSettings()->NotifyBeginPlay();
	TestTrue(TEXT("Palm input allowed"), Object->Grabbable->IsGrabInputMethodAllowed(EIsdkGrabInputMethod::Palm));
	TestFalse(TEXT("Pinch input disabled"), Object->Grabbable->IsGrabInputMethodAllowed(EIsdkGrabInputMethod::Pinch));
	TestTrue(TEXT("Only near hand grab detected"), Object->Grabbable->IsHandGrabAllowed() && !Object->Grabbable->IsDistanceGrabAllowed() && !Object->Grabbable->IsRayGrabAllowed());
	TestFalse(TEXT("Throwable disabled before play"), Object->GrabTransformer->IsThrowingEnabled());
	TestTrue(TEXT("Second hand rejected at SDK selection"), Object->GrabTransformer->GetMultiGrabBehavior() == EIsdkMultiGrabBehavior::SingleGrabFirstRetained);
	TArray<UIsdkThrowable*> Throwables;
	Object->GetComponents(Throwables);
	TestEqual(TEXT("No throwable component created at BeginPlay"), Throwables.Num(), 0);
	TestTrue(TEXT("Custom one-hand transformer bound"), Object->GrabTransformer->GetSingleGrabTransformerObject()->IsA<UVBoRCarryTransformer>());
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
	Object->MoveHeldPose(Desired);
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
#endif
