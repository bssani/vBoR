#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "P1TrunkMock.h"
#include "P1LoadBench.h"
#include "P1LoadBenchGameMode.h"
#include "../Interaction/VBoRCarryObject.h"
#include "../Interaction/VBoRLoadTarget.h"
#include "../Interaction/VBoRPoweredPanel.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "GameFramework/PlayerController.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP1MockGeometryTest, "vBoR.P1LoadBench.MockGeometry", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP1MockGeometryTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	AP1TrunkMock* Trunk = World->SpawnActor<AP1TrunkMock>();
	Trunk->OnConstruction(Trunk->GetActorTransform());
	TestTrue(TEXT("Opening has default width/depth/height 130/90/70 cm"),
		FMath::IsNearlyEqual(Trunk->InsideWidthCm, 130) && FMath::IsNearlyEqual(Trunk->InsideDepthCm, 90) && FMath::IsNearlyEqual(Trunk->InsideHeightCm, 70));
	TestTrue(TEXT("Floor top at 75 cm"), FMath::IsNearlyEqual(Trunk->Floor->GetRelativeLocation().Z + Trunk->Floor->GetRelativeScale3D().Z * 50, 75));
	TestTrue(TEXT("Ceiling lower face at 145 cm"), FMath::IsNearlyEqual(Trunk->Ceiling->GetRelativeLocation().Z - Trunk->Ceiling->GetRelativeScale3D().Z * 50, 145));
	TestTrue(TEXT("Lip top 5 cm above floor"), FMath::IsNearlyEqual(Trunk->BumperLip->GetRelativeLocation().Z + Trunk->BumperLip->GetRelativeScale3D().Z * 50, 80));
	TestTrue(TEXT("Left/right interior wall faces are at +/-65 cm"),
		FMath::IsNearlyEqual(Trunk->LeftWall->GetRelativeLocation().Y + Trunk->LeftWall->GetRelativeScale3D().Y * 50, -65) &&
		FMath::IsNearlyEqual(Trunk->RightWall->GetRelativeLocation().Y - Trunk->RightWall->GetRelativeScale3D().Y * 50, 65));
	Trunk->InsideWidthCm = 100;
	Trunk->OnConstruction(Trunk->GetActorTransform());
	TestTrue(TEXT("Editable 100 cm width moves interior wall faces back to +/-50 cm"),
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
	APlayerController* Controller = World->SpawnActor<APlayerController>();
	AVBoRHandRigPawn* Pawn = World->SpawnActor<AVBoRHandRigPawn>();
	Pawn->SetActorLocation(FVector(10, -15, 25));
	Controller->Possess(Pawn);
	AP1LoadBench* Bench = World->SpawnActor<AP1LoadBench>();
	World->InitializeActorsForPlay(FURL());
	World->GetWorldSettings()->NotifyBeginPlay();
	Bench->PlaceFromHead(FTransform(FRotator(-30, 40, 15), FVector(300, 20, 165)));
	TestTrue(TEXT("Bench placed from head yaw only"), Bench->GetActorRotation().Equals(FRotator(0, 40, 0)));
	TestTrue(TEXT("Opening is 130 cm horizontally ahead of head at Pawn origin Z"), Bench->GetActorLocation().Equals(FVector(300, 20, 25) + FRotator(0, 40, 0).RotateVector(FVector(130, 0, 0)), 0.01));
	for (const float HeadZ : {120.0f, 165.0f, 190.0f})
	{
		Bench->PlaceFromHead(FTransform(FRotator(-30, 40, 15), FVector(300, 20, HeadZ)));
		TestTrue(*FString::Printf(TEXT("Head %.0f cm keeps trunk floor at Pawn origin Z + 75 cm"), HeadZ),
			Bench->Trunk && FMath::IsNearlyEqual(Bench->Trunk->Floor->GetComponentLocation().Z +
				Bench->Trunk->Floor->GetComponentScale().Z * 50.0f, 100.0f, 0.01f));
	}
	TestNotNull(TEXT("Trunk spawned"), Bench->Trunk.Get());
	TestNotNull(TEXT("Target spawned"), Bench->Target.Get());
	TestNotNull(TEXT("Bag spawned"), Bench->Bag.Get());
	if (Bench->Trunk && Bench->Target && Bench->Bag)
	{
		TestTrue(TEXT("Trunk remains attached at opening"), Bench->Trunk->GetAttachParentActor() == Bench);
		TestTrue(TEXT("Bag/target remain attached to bench"), Bench->Bag->GetAttachParentActor() == Bench && Bench->Target->GetAttachParentActor() == Bench);
		TestTrue(TEXT("Bag lies on 75 cm stand 100 cm behind opening"),
			Bench->GetActorTransform().InverseTransformPosition(Bench->Bag->GetActorLocation()).Equals(FVector(-100, 0, 90.01), 0.01));
		TestTrue(TEXT("Goal is upright on 75 cm trunk floor"),
			Bench->GetActorTransform().InverseTransformPosition(Bench->Target->GetActorLocation()).Equals(FVector(45, 0, 90), 0.01));
		TestTrue(TEXT("Loading region spans trunk interior depth/width/height"),
			Bench->Target->LoadingRegion->GetUnscaledBoxExtent().Equals(FVector(45, 65, 35), 0.01));
		TestTrue(TEXT("Loading region center follows trunk, not bag goal height"),
			Bench->Trunk->GetActorTransform().InverseTransformPosition(Bench->Target->LoadingRegion->GetComponentLocation()).Equals(FVector(45, 0, 110), 0.01));
		TestTrue(TEXT("Loading region axes follow trunk despite crosswise goal rotation"),
			Bench->Target->LoadingRegion->GetComponentQuat().Equals(Bench->Trunk->GetActorQuat(), 0.001));
		TestTrue(TEXT("Guide stays bag size while loading region is larger"),
			Bench->Target->Guide->GetRelativeScale3D().Equals(FVector(0.45, 0.35, 0.30), 0.001));
		TestTrue(TEXT("Loading region cannot block the bag"),
			Bench->Target->LoadingRegion->GetCollisionEnabled() == ECollisionEnabled::NoCollision);
		TestTrue(TEXT("P1 box uses engine Cube"),
			Bench->Bag->Mesh->GetStaticMesh() && Bench->Bag->Mesh->GetStaticMesh()->GetPathName().Contains(TEXT("/Engine/BasicShapes/Cube")));
		if (const UStaticMesh* Cylinder = Bench->Bag->Mesh->GetStaticMesh())
		{
			const FVector BodyCm = Cylinder->GetBoundingBox().GetSize() * Bench->Bag->GetActorScale3D().GetAbs();
			TestTrue(TEXT("Actor-root mesh stays 45 x 35 x 30 cm after FinishSpawning"), BodyCm.Equals(FVector(45, 35, 30), 0.02));
		}
		TestTrue(*FString::Printf(TEXT("Bag long axis %s follows bench right %s at head yaw"),
			*Bench->Bag->GetActorRightVector().ToString(), *Bench->GetActorRightVector().ToString()),
			Bench->Bag->GetActorRightVector().Equals(Bench->GetActorRightVector(), 0.001));
		TestTrue(*FString::Printf(TEXT("Goal long axis %s follows bench right %s at head yaw"),
			*Bench->Target->GetActorRightVector().ToString(), *Bench->GetActorRightVector().ToString()),
			Bench->Target->GetActorRightVector().Equals(Bench->GetActorRightVector(), 0.001));
		TestTrue(TEXT("Target guide uses box dimensions"), Bench->Target->GoalSizeCm.Equals(FVector(45, 35, 30)));
		TestTrue(TEXT("35 cm box clears default 130 cm trunk width"), Bench->Target->IsStaticFit(Bench->Bag));
		TestTrue(TEXT("Panel states clear static goal pose fit"), Bench->Panel->Text.ToString().Contains(TEXT("Static target pose: clear")));
		TestTrue(TEXT("Panel labels current containment as loading region"), Bench->Panel->Text.ToString().Contains(TEXT("loading region: not inside / overlap")));
		const FVector OriginalBagScale = Bench->Bag->GetActorScale3D();
		Bench->Trunk->InsideWidthCm = 30;
		Bench->Trunk->OnConstruction(Bench->Trunk->GetActorTransform());
		TestFalse(TEXT("35 cm box fails static fit when editable trunk width is 30 cm"), Bench->Target->IsStaticFit(Bench->Bag));
		Bench->Tick(0);
		TestTrue(TEXT("Narrow trunk panel reports failed fit"), Bench->Panel->Text.ToString().Contains(TEXT("overlap / does not fit")));
		TestTrue(TEXT("Failed fit leaves bag geometry unchanged"), Bench->Bag->GetActorScale3D().Equals(OriginalBagScale, 0.0001));
		Bench->Trunk->InsideWidthCm = 130;
		Bench->Trunk->OnConstruction(Bench->Trunk->GetActorTransform());
		// No initial/first-Tick passage; count only body front crossing into +X (P1 §2 path)
		Bench->Tick(0);
		TestEqual(TEXT("Initial bag pose has no crossing"), Bench->GetEntranceCrossings(), 0);
		Bench->Bag->SetActorLocation(Bench->GetActorTransform().TransformPosition(FVector(-30, 0, 90)), false, nullptr, ETeleportType::TeleportPhysics);
		Bench->Tick(0);
		TestEqual(TEXT("Bag entirely behind opening has no crossing"), Bench->GetEntranceCrossings(), 0);
		Bench->Bag->SetActorLocation(Bench->GetActorTransform().TransformPosition(FVector(-20, 0, 90)), false, nullptr, ETeleportType::TeleportPhysics);
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
			Bench->GetActorTransform().InverseTransformPosition(Bench->Bag->GetActorLocation()).Equals(FVector(-100, 0, 90.01), 0.01));
		TestTrue(TEXT("R reset preserves box scale"), Bench->Bag->GetActorScale3D().Equals(BagScaleBefore, 0.0001));
		TestTrue(*FString::Printf(TEXT("Reset bag long axis %s follows bench right %s"),
			*Bench->Bag->GetActorRightVector().ToString(), *Bench->GetActorRightVector().ToString()),
			Bench->Bag->GetActorRightVector().Equals(Bench->GetActorRightVector(), 0.001));
		TestTrue(*FString::Printf(TEXT("Reset goal long axis %s follows bench right %s"),
			*Bench->Target->GetActorRightVector().ToString(), *Bench->GetActorRightVector().ToString()),
			Bench->Target->GetActorRightVector().Equals(Bench->GetActorRightVector(), 0.001));
	}
	TestEqual(TEXT("Reset does not fabricate entrance crossings"), Bench->GetEntranceCrossings(), 0);
	TestEqual(TEXT("Reset does not fabricate blocked crossings"), Bench->GetBlockedAtCrossings(), 0);
	Bench->Trunk->InsideDepthCm = 110;
	Bench->Trunk->InsideWidthCm = 140;
	Bench->Trunk->InsideHeightCm = 80;
	Bench->Trunk->FloorHeightCm = 85;
	Bench->Trunk->OnConstruction(Bench->Trunk->GetActorTransform());
	Bench->PlaceFromHead(FTransform(FRotator(0, 25, 0), FVector(0, 0, 165)));
	TestTrue(TEXT("R applies edited trunk dimensions to loading region"),
		Bench->Target->LoadingRegion->GetUnscaledBoxExtent().Equals(FVector(55, 70, 40), 0.01));
	TestTrue(TEXT("R applies edited floor height to loading region center"),
		Bench->Trunk->GetActorTransform().InverseTransformPosition(Bench->Target->LoadingRegion->GetComponentLocation()).Equals(FVector(55, 0, 125), 0.01));
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP1BagRepeatTest, "vBoR.P1LoadBench.BagRepeat", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP1BagRepeatTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	APlayerController* Controller = World->SpawnActor<APlayerController>();
	AVBoRHandRigPawn* Pawn = World->SpawnActor<AVBoRHandRigPawn>();
	Controller->Possess(Pawn);
	AP1LoadBench* Bench = World->SpawnActor<AP1LoadBench>();
	World->InitializeActorsForPlay(FURL());
	World->GetWorldSettings()->NotifyBeginPlay();
	Bench->PlaceFromHead(FTransform(FRotator(0, 25, 0), FVector(200, 20, 165)));
	if (Bench->Bag && Bench->Trunk)
	{
		const FTransform BenchBefore = Bench->GetActorTransform();
		const FTransform TrunkBefore = Bench->Trunk->GetActorTransform();
		Bench->Bag->Session.Begin(0, Bench->Bag->GetActorTransform(), Bench->Bag->GetActorTransform(), 0);
		Bench->Bag->Successes[0] = 2;
		Bench->Bag->BlockedCount = 3;
		Bench->Bag->BlockedSeconds = 1.5;
		Bench->Bag->Session.PauseCount = 2;
		Bench->Bag->Session.PausedSeconds = 4;
		Bench->Bag->Session.HeldSeconds = 5;
		Bench->Bag->SetActorLocation(Bench->GetActorTransform().TransformPosition(FVector(-30, 0, 89)), false, nullptr, ETeleportType::TeleportPhysics);
		UFunction* Repeat = Bench->FindFunction(TEXT("ResetBagForNextTrial"));
		TestNotNull(TEXT("Bag-only repeat action is callable"), Repeat);
		if (Repeat)
		{
			Bench->ProcessEvent(Repeat, nullptr);
			TestTrue(TEXT("Bag repeat keeps bench transform"), Bench->GetActorTransform().Equals(BenchBefore));
			TestTrue(TEXT("Bag repeat keeps trunk transform"), Bench->Trunk->GetActorTransform().Equals(TrunkBefore));
			TestTrue(TEXT("Bag repeat restores stand start pose"),
				Bench->GetActorTransform().InverseTransformPosition(Bench->Bag->GetActorLocation()).Equals(FVector(-100, 0, 90.01), 0.01));
			TestTrue(TEXT("Bag repeat displays trial two"), Bench->Panel->Text.ToString().Contains(TEXT("Trial: 2")));
			TestEqual(TEXT("B ends synthetic owning hand session"), Bench->Bag->GetCarrySession().Hand, INDEX_NONE);
			TestEqual(TEXT("B preserves grabbed total"), Bench->GetTotals().Grabbed, 2);
			TestEqual(TEXT("B preserves blocked count"), Bench->GetTotals().BlockedCount, 3);
			TestEqual(TEXT("B preserves blocked duration"), Bench->GetTotals().BlockedSeconds, 1.5);
			TestEqual(TEXT("B preserves pause count"), Bench->GetTotals().PauseCount, 2);
			TestEqual(TEXT("B preserves pause duration"), Bench->GetTotals().PausedSeconds, 4.0);
			TestEqual(TEXT("B preserves held duration"), Bench->GetTotals().HeldSeconds, 5.0);
			Bench->ResetBagForNextTrial();
			TestEqual(TEXT("Empty next trial does not duplicate prior totals"), Bench->GetTotals().Grabbed, 2);
			TestEqual(TEXT("B increments number each time"), Bench->GetTrialNumber(), 3);
			TestEqual(TEXT("Bag repeat does not fabricate entrance crossings"), Bench->GetEntranceCrossings(), 0);
			Bench->PlaceFromHead(FTransform(FRotator(0, -15, 0), FVector(-20, 10, 190)));
			TestTrue(TEXT("R clears trial number"), Bench->Panel->Text.ToString().Contains(TEXT("Trial: 1")));
			TestEqual(TEXT("R clears grabbed total"), Bench->GetTotals().Grabbed, 0);
			TestEqual(TEXT("R clears blocked total"), Bench->GetTotals().BlockedCount, 0);
			TestEqual(TEXT("R clears pause duration"), Bench->GetTotals().PausedSeconds, 0.0);
			const FTransform TrunkPose = Bench->Trunk->GetActorTransform();
			Bench->Bag->Mesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder")));
			Bench->Bag->SetActorScale3D(FVector(0.28, 0.28, 1.2));
			const FQuat AxialRotation = TrunkPose.GetRotation() * FRotator(0, 0, 90).Quaternion() *
				FQuat(FVector::UpVector, FMath::DegreesToRadians(15.0));
			Bench->Bag->bBlocked = true;
			Bench->Bag->SetActorTransform(FTransform(AxialRotation, TrunkPose.TransformPosition(FVector(-20, 0, 105)), Bench->Bag->GetActorScale3D()),
				false, nullptr, ETeleportType::TeleportPhysics);
			Bench->Tick(0);
			Bench->Bag->SetActorLocation(TrunkPose.TransformPosition(FVector(-17, 0, 105)), false, nullptr, ETeleportType::TeleportPhysics);
			Bench->Tick(0);
			TestEqual(TEXT("Inflated corner cannot classify blocked hull outside plane proximity"), Bench->GetBlockedAtCrossings(), 0);
			Bench->Bag->SetActorLocation(TrunkPose.TransformPosition(FVector(-10, 0, 105)), false, nullptr, ETeleportType::TeleportPhysics);
			Bench->Tick(0);
			TestEqual(TEXT("Blocked actual hull at plane is counted once"), Bench->GetBlockedAtCrossings(), 1);
			Bench->Tick(0);
			TestEqual(TEXT("Stationary blocked hull cannot duplicate classification"), Bench->GetBlockedAtCrossings(), 1);
		}
	}
	World->EndPlay(EEndPlayReason::Quit);
	World->DestroyWorld(false);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP1BenchHullCrossingTest, "vBoR.P1LoadBench.CylinderHullEntrance", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP1BenchHullCrossingTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	APlayerController* Controller = World->SpawnActor<APlayerController>();
	AVBoRHandRigPawn* Pawn = World->SpawnActor<AVBoRHandRigPawn>();
	Controller->Possess(Pawn);
	AP1LoadBench* Bench = World->SpawnActor<AP1LoadBench>();
	World->InitializeActorsForPlay(FURL());
	World->GetWorldSettings()->NotifyBeginPlay();
	// Bumper/cylinder hull comparison only. Keep opening clear after R/B (P4b D8)
	Bench->Gate->bStartOpen = true;
	for (double Yaw : {0.0, 37.0})
	{
		Bench->PlaceFromHead(FTransform(FRotator(0, Yaw, 0), FVector(200, -30, 165)));
		if (!TestNotNull(TEXT("Hull crossing bag exists"), Bench->Bag.Get()) ||
			!TestNotNull(TEXT("Hull crossing trunk exists"), Bench->Trunk.Get())) { break; }
		const FTransform TrunkPose = Bench->Trunk->GetActorTransform();
		Bench->Bag->Mesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder")));
		Bench->Bag->SetActorScale3D(FVector(0.28, 0.28, 1.2));
		const FQuat Rotation = TrunkPose.GetRotation() * FRotator(0, 0, 90).Quaternion() *
			FQuat(FVector::UpVector, FMath::DegreesToRadians(15.0));
		auto PlaceBag = [&](double X)
		{
			Bench->Bag->SetActorTransform(FTransform(Rotation, TrunkPose.TransformPosition(FVector(X, 0, 105)), Bench->Bag->GetActorScale3D()),
				false, nullptr, ETeleportType::TeleportPhysics);
			Bench->Tick(0);
		};
		PlaceBag(-20);
		TestEqual(TEXT("Axially rotated hull starts behind opening"), Bench->GetEntranceCrossings(), 0);
		PlaceBag(-16);
		TestTrue(TEXT("Raised axial cylinder is clear of bumper/body"), Bench->Bag->IsPoseClear(Bench->Bag->GetActorTransform()));
		TestEqual(TEXT("Render-box corner cannot fabricate cylinder entrance"), Bench->GetEntranceCrossings(), 0);
		PlaceBag(-10);
		TestEqual(TEXT("Actual leading cylinder hull enters exactly once"), Bench->GetEntranceCrossings(), 1);
		Bench->Tick(0);
		TestEqual(TEXT("Stationary entered hull does not repeat crossing"), Bench->GetEntranceCrossings(), 1);
		Bench->ResetBagForNextTrial();
		Bench->Bag->Mesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder")));
		Bench->Bag->SetActorScale3D(FVector(0.28, 0.28, 1.2));
		TestEqual(TEXT("B clears hull crossing count"), Bench->GetEntranceCrossings(), 0);
		PlaceBag(-10);
		TestEqual(TEXT("B clears prior armed crossing state"), Bench->GetEntranceCrossings(), 0);
	}
	Bench->PlaceFromHead(FTransform(FVector(200, -30, 165)));
	if (Bench->Bag && Bench->Trunk)
	{
		const FTransform TrunkPose = Bench->Trunk->GetActorTransform();
		Bench->Bag->Mesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder")));
		Bench->Bag->SetActorTransform(FTransform(TrunkPose.GetRotation() * FRotator(0, 0, 90).Quaternion(), TrunkPose.TransformPosition(FVector(-20, 0, 105)), FVector(0.28, 0.28, 1.2)));
		UStaticMesh* Cylinder = Bench->Bag->Mesh->GetStaticMesh();
		Bench->Bag->SetActorLocation(TrunkPose.TransformPosition(FVector(-20, 0, 105)), false, nullptr, ETeleportType::TeleportPhysics);
		Bench->Tick(0);
		Bench->Bag->Mesh->SetStaticMesh(nullptr);
		Bench->Tick(0);
		TestEqual(TEXT("Missing simple geometry fabricates no crossing"), Bench->GetEntranceCrossings(), 0);
		Bench->Bag->Mesh->SetStaticMesh(Cylinder);
		Bench->Bag->SetActorLocation(TrunkPose.TransformPosition(FVector(-10, 0, 105)), false, nullptr, ETeleportType::TeleportPhysics);
		Bench->Tick(0);
		TestEqual(TEXT("Geometry gap cannot bridge an old behind-opening sample"), Bench->GetEntranceCrossings(), 0);
		Bench->Bag->SetActorLocation(TrunkPose.TransformPosition(FVector(-20, 0, 105)), false, nullptr, ETeleportType::TeleportPhysics);
		Bench->Tick(0);
		Bench->Bag->SetActorLocation(TrunkPose.TransformPosition(FVector(-10, 0, 105)), false, nullptr, ETeleportType::TeleportPhysics);
		Bench->Tick(0);
		TestEqual(TEXT("Fresh valid behind sample rearms crossing after geometry recovery"), Bench->GetEntranceCrossings(), 1);
	}
	World->EndPlay(EEndPlayReason::Quit);
	World->DestroyWorld(false);
	return true;
}
#endif
