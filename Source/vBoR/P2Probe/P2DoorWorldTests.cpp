#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "P2DoorBench.h"
#include "P2DoorBenchGameMode.h"
#include "../Interaction/VBoRHingedDoor.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "Interaction/IsdkGrabbableComponent.h"
#include "Interaction/IsdkGrabberComponent.h"
#include "Rig/IsdkGrabInteractionRigComponent.h"
#include "Rig/IsdkRayInteractionRigComponent.h"
#include "StructTypesNonGenerated.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP2DoorAssemblyTest, "vBoR.P2Door.World.AssemblyAndRig", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP2DoorAssemblyTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	AVBoRHingedDoor* Door = World->SpawnActor<AVBoRHingedDoor>();
	AP2PalmGrabPawn* Pawn = World->SpawnActor<AP2PalmGrabPawn>();
	World->InitializeActorsForPlay(FURL());
	World->GetWorldSettings()->NotifyBeginPlay();
	TestTrue(TEXT("Hinge is below independent root"), Door->HingePivot->GetAttachParent() == Door->GetRootComponent());
	TestTrue(TEXT("Door and both handles follow pivot"), Door->DoorMesh->GetAttachParent() == Door->HingePivot && Door->OutsideHandle->GetAttachParent() == Door->HingePivot && Door->InsideHandle->GetAttachParent() == Door->HingePivot);
	TestTrue(TEXT("Palm only outside and inside"), Door->OutsideGrab->IsGrabInputMethodAllowed(EIsdkGrabInputMethod::Palm) && Door->InsideGrab->IsGrabInputMethodAllowed(EIsdkGrabInputMethod::Palm) && !Door->OutsideGrab->IsGrabInputMethodAllowed(EIsdkGrabInputMethod::Pinch) && !Door->InsideGrab->IsGrabInputMethodAllowed(EIsdkGrabInputMethod::Pinch));
	TestTrue(TEXT("Grab targets pivot, preserves root"), Door->OutsideTransformer->GetTransformTarget() == Door->HingePivot && Door->InsideTransformer->GetTransformTarget() == Door->HingePivot);
	TestFalse(TEXT("Outside throwing disabled"), Door->OutsideTransformer->IsThrowingEnabled());
	TestFalse(TEXT("Inside throwing disabled"), Door->InsideTransformer->IsThrowingEnabled());
	TestNotNull(TEXT("Original material still present"), Door->DoorMesh->GetMaterial(0));
	TestNotNull(TEXT("State is overlay, not base replacement"), Door->DoorMesh->GetOverlayMaterial());
	TestTrue(TEXT("P0 default still poke only"), !GetDefault<AVBoRHandRigPawn>()->bAllowPalmGrab);
	TestTrue(TEXT("P2 game mode uses opted-in hand pawn"), GetDefault<AP2DoorBenchGameMode>()->DefaultPawnClass == AP2PalmGrabPawn::StaticClass());
	for (int32 H = 0; H < 2; ++H)
	{
		UIsdkHandRigComponent* Rig = Pawn->GetHandRig(H);
		TestTrue(TEXT("P2 palm grab rig active"), Rig->GetGrabInteraction()->IsActive());
		TestFalse(TEXT("Ray disabled"), Rig->GetRayInteraction()->IsActive());
		TestFalse(TEXT("Distance blocked"), Rig->GetGrabber()->IsGrabDetectionTypeAllowed(EIsdkGrabDetectorType::DistanceGrab));
		TestFalse(TEXT("Ray grab blocked"), Rig->GetGrabber()->IsGrabDetectionTypeAllowed(EIsdkGrabDetectorType::RayGrab));
		TestFalse(TEXT("Pinch blocked"), Rig->GetGrabber()->IsGrabInputMethodAllowed(EIsdkGrabInputMethod::Pinch));
	}
	World->EndPlay(EEndPlayReason::Quit); World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP2DoorBenchTest, "vBoR.P2Door.World.PlacementResetAndTotals", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP2DoorBenchTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	AP2DoorBench* Bench = World->SpawnActor<AP2DoorBench>();
	World->InitializeActorsForPlay(FURL());
	World->GetWorldSettings()->NotifyBeginPlay();
	const FTransform Head(FRotator(-30, 40, 15), FVector(300, 20, 165));
	Bench->PlaceFromHead(Head, 25);
	TestNotNull(TEXT("GameMode-owned bench spawns one door"), Bench->Door.Get());
	if (Bench->Door)
	{
		AVBoRHingedDoor* Door = Bench->Door;
		const FVector Forward = FRotator(0, 40, 0).Vector();
		const FVector Outside = Bench->GetActorTransform().TransformPosition(FVector(80, 5, 95));
		TestTrue(TEXT("Outside face 60 cm ahead"), FMath::Abs(FVector::DotProduct(Outside - Head.GetLocation(), Forward) - 60) < 0.01);
		TestTrue(TEXT("Hinge left of head, behind-edge offset 30 cm"), Bench->GetActorTransform().InverseTransformPosition(Head.GetLocation()).Equals(FVector(80, 65, 140), 0.01));
		TestTrue(TEXT("Door bottom floor+35 cm"), FMath::Abs(Door->HingePivot->GetComponentLocation().Z - 60) < 0.01);
		TestTrue(TEXT("Handles floor+95 and +85 cm"), FMath::Abs(Door->OutsideHandle->GetComponentLocation().Z - 120) < 0.01 && FMath::Abs(Door->InsideHandle->GetComponentLocation().Z - 110) < 0.01);
		const int32 Trial = Bench->GetTrialNumber();
		Bench->NextTrial();
		TestEqual(TEXT("E18 B increments trial"), Bench->GetTrialNumber(), Trial + 1);
		Bench->PlaceFromHead(FTransform(FRotator(0, -60, 0), FVector(300, 20, 190)), 25);
		TestEqual(TEXT("E18 R keeps trial"), Bench->GetTrialNumber(), Trial + 1);
		TestTrue(TEXT("R reuses same door"), Bench->Door == Door);
		TestTrue(TEXT("R resets locked zero"), Door->GetMotion().bLocked && Door->GetMotion().AngleDeg == 0);
		TestTrue(TEXT("Head height does not alter physical-floor bottom"), FMath::Abs(Door->HingePivot->GetComponentLocation().Z - 60) < 0.01);
		TestTrue(TEXT("Panel marks approximation and not-tested"), Bench->Panel->Text.ToString().Contains(TEXT("Approximate geometry")) && Bench->Panel->Text.ToString().Contains(TEXT("not-tested")));
	}
	for (UStaticMeshComponent* Frame : Bench->FrameParts) { TestTrue(TEXT("Frame has no collision"), Frame->GetCollisionEnabled() == ECollisionEnabled::NoCollision); }
	FVBoRDoorMetrics Before, After;
	Before.Unlocks = 3; Before.Attempts[1][0] = 5; Before.PushedDeg = 12;
	After = Before; After.Unlocks += 2; After.Attempts[1][0] += 1; After.PushedDeg += 25;
	const FVBoRDoorMetrics TrialMetrics = After.Since(Before);
	TestEqual(TEXT("Trial unlock difference"), TrialMetrics.Unlocks, 2);
	TestEqual(TEXT("Trial handle attempt difference"), TrialMetrics.Attempts[1][0], 1);
	TestEqual(TEXT("Trial push angle difference"), TrialMetrics.PushedDeg, 25.0);
	World->EndPlay(EEndPlayReason::Quit); World->DestroyWorld(false);
	return true;
}
#endif
