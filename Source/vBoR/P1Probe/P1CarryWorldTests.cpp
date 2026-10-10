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
#endif
