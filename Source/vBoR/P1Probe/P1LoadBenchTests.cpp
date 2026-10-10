#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "P1TrunkMock.h"
#include "P1LoadBench.h"
#include "P1LoadBenchGameMode.h"
#include "../Interaction/VBoRCarryObject.h"
#include "../Interaction/VBoRLoadTarget.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP1MockGeometryTest, "vBoR.P1LoadBench.MockGeometry", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP1MockGeometryTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	AP1TrunkMock* Trunk = World->SpawnActor<AP1TrunkMock>();
	Trunk->OnConstruction(Trunk->GetActorTransform());
	TestTrue(TEXT("Opening has default width/depth/height 100/90/70 cm"),
		FMath::IsNearlyEqual(Trunk->InsideWidthCm, 100) && FMath::IsNearlyEqual(Trunk->InsideDepthCm, 90) && FMath::IsNearlyEqual(Trunk->InsideHeightCm, 70));
	TestTrue(TEXT("Floor top at 75 cm"), FMath::IsNearlyEqual(Trunk->Floor->GetRelativeLocation().Z + Trunk->Floor->GetRelativeScale3D().Z * 50, 75));
	TestTrue(TEXT("Ceiling lower face at 145 cm"), FMath::IsNearlyEqual(Trunk->Ceiling->GetRelativeLocation().Z - Trunk->Ceiling->GetRelativeScale3D().Z * 50, 145));
	TestTrue(TEXT("Lip top 5 cm above floor"), FMath::IsNearlyEqual(Trunk->BumperLip->GetRelativeLocation().Z + Trunk->BumperLip->GetRelativeScale3D().Z * 50, 80));
	TestTrue(TEXT("Left/right interior wall faces are at +/-50 cm"),
		FMath::IsNearlyEqual(Trunk->LeftWall->GetRelativeLocation().Y + Trunk->LeftWall->GetRelativeScale3D().Y * 50, -50) &&
		FMath::IsNearlyEqual(Trunk->RightWall->GetRelativeLocation().Y - Trunk->RightWall->GetRelativeScale3D().Y * 50, 50));
	for (UStaticMeshComponent* Panel : {Trunk->Floor.Get(), Trunk->LeftWall.Get(), Trunk->RightWall.Get(), Trunk->FrontWall.Get(), Trunk->Ceiling.Get(), Trunk->BumperLip.Get()})
	{
		TestTrue(TEXT("Solid panel blocks query collision"), Panel->GetCollisionEnabled() == ECollisionEnabled::QueryOnly && Panel->GetCollisionResponseToChannel(ECC_WorldStatic) == ECR_Block);
	}
	TestTrue(TEXT("Visible approximation warning"), Trunk->ApproximateLabel->Text.ToString().Contains(TEXT("Approximate geometry")));
	World->EndPlay(EEndPlayReason::Quit);
	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP1BenchWorldTest, "vBoR.P1LoadBench.PlacementAndReset", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP1BenchWorldTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	AP1LoadBench* Bench = World->SpawnActor<AP1LoadBench>();
	World->InitializeActorsForPlay(FURL());
	World->GetWorldSettings()->NotifyBeginPlay();
	Bench->PlaceFromHead(FTransform(FRotator(-30, 40, 15), FVector(300, 20, 165)));
	TestTrue(TEXT("Bench placed from head yaw only"), Bench->GetActorRotation().Equals(FRotator(0, 40, 0)));
	TestTrue(TEXT("Opening is 130 cm horizontally ahead of head"), Bench->GetActorLocation().Equals(FVector(300, 20, 0) + FRotator(0, 40, 0).RotateVector(FVector(130, 0, 0)), 0.01));
	TestNotNull(TEXT("Trunk spawned"), Bench->Trunk.Get());
	TestNotNull(TEXT("Target spawned"), Bench->Target.Get());
	TestNotNull(TEXT("Bag spawned"), Bench->Bag.Get());
	if (Bench->Trunk && Bench->Target && Bench->Bag)
	{
		TestTrue(TEXT("Trunk remains attached at opening"), Bench->Trunk->GetAttachParentActor() == Bench);
		TestTrue(TEXT("Bag/target remain attached to bench"), Bench->Bag->GetAttachParentActor() == Bench && Bench->Target->GetAttachParentActor() == Bench);
		TestTrue(TEXT("Bag lies on 75 cm stand 100 cm behind opening"),
			Bench->GetActorTransform().InverseTransformPosition(Bench->Bag->GetActorLocation()).Equals(FVector(-100, 0, 89), 0.01));
		TestTrue(TEXT("Goal is crosswise on 75 cm trunk floor"),
			Bench->GetActorTransform().InverseTransformPosition(Bench->Target->GetActorLocation()).Equals(FVector(45, 0, 89), 0.01));
		TestTrue(TEXT("P1 bag explicitly uses engine Cylinder rather than carry parent Cube"),
			Bench->Bag->Mesh->GetStaticMesh() && Bench->Bag->Mesh->GetStaticMesh()->GetPathName().Contains(TEXT("/Engine/BasicShapes/Cylinder")));
		if (const UStaticMesh* Cylinder = Bench->Bag->Mesh->GetStaticMesh())
		{
			const FVector BodyCm = Cylinder->GetBoundingBox().GetSize() * Bench->Bag->GetActorScale3D().GetAbs();
			TestTrue(TEXT("Actor-root mesh stays 28 x 28 x 120 cm after FinishSpawning"), BodyCm.Equals(FVector(28, 28, 120), 0.02));
		}
		TestTrue(*FString::Printf(TEXT("Bag long axis %s follows bench right %s at head yaw"),
			*Bench->Bag->GetActorUpVector().ToString(), *Bench->GetActorRightVector().ToString()),
			Bench->Bag->GetActorUpVector().Equals(Bench->GetActorRightVector(), 0.001));
		TestTrue(*FString::Printf(TEXT("Goal long axis %s follows bench right %s at head yaw"),
			*Bench->Target->GetActorUpVector().ToString(), *Bench->GetActorRightVector().ToString()),
			Bench->Target->GetActorUpVector().Equals(Bench->GetActorRightVector(), 0.001));
		TestTrue(TEXT("Target guide retains full 120 cm bag length"), Bench->Target->GoalSizeCm.Equals(FVector(28, 28, 120)));
		TestFalse(TEXT("120 cm crosswise bag does not fit 100 cm trunk width"), Bench->Target->IsStaticFit(Bench->Bag));
		TestTrue(TEXT("Panel states failed static fit instead of auto-resizing"), Bench->Panel->Text.ToString().Contains(TEXT("overlap / does not fit")));
		// 시작·초기 tick 은 통과로 세지 않고, 몸체 앞 경계가 +X 로 들어갈 때만 셈 (P1 §2 경로)
		Bench->Tick(0);
		TestEqual(TEXT("Initial bag pose has no crossing"), Bench->GetEntranceCrossings(), 0);
		Bench->Bag->SetActorLocation(Bench->GetActorTransform().TransformPosition(FVector(-20, 0, 89)), false, nullptr, ETeleportType::TeleportPhysics);
		Bench->Tick(0);
		TestEqual(TEXT("Bag entirely behind opening has no crossing"), Bench->GetEntranceCrossings(), 0);
		Bench->Bag->SetActorLocation(Bench->GetActorTransform().TransformPosition(FVector(-13, 0, 89)), false, nullptr, ETeleportType::TeleportPhysics);
		Bench->Tick(0);
		TestEqual(TEXT("Leading bag boundary crosses opening once"), Bench->GetEntranceCrossings(), 1);
		Bench->Tick(0);
		TestEqual(TEXT("Stationary bag at opening does not repeat crossing"), Bench->GetEntranceCrossings(), 1);
	}
	const FVector RelativeStand = Bench->Stand->GetRelativeLocation();
	const FVector BagScaleBefore = Bench->Bag ? Bench->Bag->GetActorScale3D() : FVector::ZeroVector;
	Bench->PlaceFromHead(FTransform(FRotator(25, -70, -15), FVector(-80, 210, 170)));
	TestTrue(TEXT("R reset uses new yaw and preserves stand relative placement"), Bench->GetActorRotation().Equals(FRotator(0, -70, 0)) && Bench->Stand->GetRelativeLocation().Equals(RelativeStand));
	if (Bench->Bag)
	{
		TestTrue(TEXT("Bag returns to new world-fixed starting pose"),
			Bench->GetActorTransform().InverseTransformPosition(Bench->Bag->GetActorLocation()).Equals(FVector(-100, 0, 89), 0.01));
		TestTrue(TEXT("R reset preserves full Cylinder scale"), Bench->Bag->GetActorScale3D().Equals(BagScaleBefore, 0.0001));
		TestTrue(*FString::Printf(TEXT("Reset bag long axis %s follows bench right %s"),
			*Bench->Bag->GetActorUpVector().ToString(), *Bench->GetActorRightVector().ToString()),
			Bench->Bag->GetActorUpVector().Equals(Bench->GetActorRightVector(), 0.001));
		TestTrue(*FString::Printf(TEXT("Reset goal long axis %s follows bench right %s"),
			*Bench->Target->GetActorUpVector().ToString(), *Bench->GetActorRightVector().ToString()),
			Bench->Target->GetActorUpVector().Equals(Bench->GetActorRightVector(), 0.001));
	}
	TestEqual(TEXT("Reset does not fabricate entrance crossings"), Bench->GetEntranceCrossings(), 0);
	TestEqual(TEXT("Reset does not fabricate blocked crossings"), Bench->GetBlockedAtCrossings(), 0);
	World->EndPlay(EEndPlayReason::Quit);
	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP1GameModeTest, "vBoR.P1LoadBench.GameModeDefaults", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP1GameModeTest::RunTest(const FString& Parameters)
{
	const AP1LoadBenchGameMode* Mode = GetDefault<AP1LoadBenchGameMode>();
	TestTrue(TEXT("GameMode uses palm grab pawn"), Mode->DefaultPawnClass == AP1PalmGrabPawn::StaticClass());
	TestTrue(TEXT("GameMode spawns P1 bench"), Mode->BenchClass == AP1LoadBench::StaticClass());
	TestTrue(TEXT("P1 pawn allows palm grab"), GetDefault<AP1PalmGrabPawn>()->bAllowPalmGrab);
	TestFalse(TEXT("Shared hand rig still defaults to no grab"), GetDefault<AVBoRHandRigPawn>()->bAllowPalmGrab);
	return true;
}
#endif
