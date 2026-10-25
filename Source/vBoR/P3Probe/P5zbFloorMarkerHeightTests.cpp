#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "P3VehicleMock.h"
#include "../Interaction/VBoRScenarioRunner.h"
#include "../Interaction/VBoRVehicleAlignmentComponent.h"
#include "Components/ArrowComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/WorldSettings.h"

struct FP5zbTestAccess
{
	static const TArray<TObjectPtr<UStaticMeshComponent>>& Ring(AVBoRScenarioRunner* Runner) { return Runner->MarkerRing; }
	static UArrowComponent* Arrow(AVBoRScenarioRunner* Runner) { return Runner->MarkerArrow; }
};

namespace
{
	struct FP5zbWorld
	{
		UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
		AP3VehicleMock* Car;
		AVBoRScenarioRunner* Runner;
		FP5zbWorld()
		{
			GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
			APlayerController* Controller = World->SpawnActor<APlayerController>();
			APawn* Pawn = World->SpawnActor<APawn>();
			Pawn->SetRootComponent(NewObject<USceneComponent>(Pawn));
			Pawn->GetRootComponent()->RegisterComponent(); Controller->Possess(Pawn);
			Car = World->SpawnActor<AP3VehicleMock>();
			AActor* Target = World->SpawnActor<AActor>();
			Target->SetRootComponent(NewObject<USceneComponent>(Target));
			Target->GetRootComponent()->RegisterComponent();
			Target->SetActorLocation(FVector(123, -87, 64)); Target->Tags.Add(TEXT("P5zb.Marker"));
			Runner = World->SpawnActor<AVBoRScenarioRunner>();
			Runner->Scenario = NewObject<UVBoRScenarioAsset>(Runner);
			Runner->Scenario->Steps.Reset();
			FVBoRScenarioStep Step; Step.StepId = TEXT("03");
			Step.Instruction = FText::FromString(TEXT("Observe the vehicle.")); Step.MarkerTag = TEXT("P5zb.Marker");
			Runner->Scenario->Steps.Add(Step);
			World->InitializeActorsForPlay(FURL()); World->GetWorldSettings()->NotifyBeginPlay();
			FVBoRBuckSavedAlignment Saved; Saved.SavedAt = TEXT("test"); Saved.TrackingOrigin = 3;
			Saved.PlayAreaCm = FVector2D(300, 400); Saved.Capture.PositionCm = FVector(10, 20, 115); Saved.Capture.YawDeg = 30;
			Car->Alignment->InitializeAlignment(Pawn, &Saved, 3, Saved.PlayAreaCm);
		}
		~FP5zbWorld()
		{ World->EndPlay(EEndPlayReason::Quit); GEngine->DestroyWorldContext(World); World->DestroyWorld(false); }
	};

	bool P5zbCheckRing(FAutomationTestBase& Test, AVBoRScenarioRunner* Runner, double ExpectedZ)
	{
		const auto& Ring = FP5zbTestAccess::Ring(Runner);
		if (!Test.TestEqual(TEXT("Four ring pieces"), Ring.Num(), 4)) { return false; }
		const FVector ExpectedXY[] = {FVector(123, -117, 0), FVector(123, -57, 0), FVector(93, -87, 0), FVector(153, -87, 0)};
		for (int32 I = 0; I < 4; ++I)
		{
			if (!Test.TestNotNull(TEXT("Ring piece exists"), Ring[I].Get())) { return false; }
			Test.TestFalse(TEXT("Ring piece visible"), Ring[I]->bHiddenInGame);
			const FVector Position = Ring[I]->GetComponentLocation();
			Test.TestTrue(TEXT("Ring world height"), FMath::IsNearlyEqual(Position.Z, ExpectedZ, 0.01));
			Test.TestTrue(TEXT("Target XY offsets unchanged"), FVector(Position.X, Position.Y, 0).Equals(ExpectedXY[I], 0.01));
			Test.TestTrue(TEXT("Ring rotation unchanged"), Ring[I]->GetComponentQuat().Equals(FQuat::Identity, 0.0001));
			Test.TestTrue(TEXT("Ring size unchanged"), Ring[I]->GetComponentScale().Equals(I < 2 ? FVector(0.62, 0.02, 0.005) : FVector(0.02, 0.62, 0.005), 0.0001));
		}
		return true;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5zbZeroFloorTest, "vBoR.P5zb.E1ZeroFloor", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5zbZeroFloorTest::RunTest(const FString&)
{
	FP5zbWorld E;
	TestEqual(TEXT("Default floor height"), E.Car->Alignment->FloorWorldZCm, 0.0f);
	if (!TestTrue(TEXT("Flow started"), E.Runner->InitializeFlow())) { return false; }
	return P5zbCheckRing(*this, E.Runner, 0.5);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5zbRaisedFloorTest, "vBoR.P5zb.E2RaisedFloor", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5zbRaisedFloorTest::RunTest(const FString&)
{
	FP5zbWorld E; E.Car->Alignment->FloorWorldZCm = 37;
	if (!TestTrue(TEXT("Flow started"), E.Runner->InitializeFlow())) { return false; }
	return P5zbCheckRing(*this, E.Runner, 37.5);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5zbNegativeFloorTest, "vBoR.P5zb.E3NegativeFloor", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5zbNegativeFloorTest::RunTest(const FString&)
{
	FP5zbWorld E; E.Car->Alignment->FloorWorldZCm = -12.5f; E.Car->Alignment->bHeightFromFloor = false;
	if (!TestTrue(TEXT("Flow started"), E.Runner->InitializeFlow())) { return false; }
	return P5zbCheckRing(*this, E.Runner, -12.0);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5zbLiveFloorTest, "vBoR.P5zb.E4LiveFloorChange", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5zbLiveFloorTest::RunTest(const FString&)
{
	FP5zbWorld E;
	if (!TestTrue(TEXT("Flow started"), E.Runner->InitializeFlow())) { return false; }
	P5zbCheckRing(*this, E.Runner, 0.5);
	E.Car->Alignment->FloorWorldZCm = 20; E.Runner->Tick(0.016f);
	TestEqual(TEXT("Same step after one tick"), E.Runner->GetStepIndex(), 0);
	return P5zbCheckRing(*this, E.Runner, 20.5);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5zbArrowTest, "vBoR.P5zb.E5ArrowUnchanged", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5zbArrowTest::RunTest(const FString&)
{
	FP5zbWorld E; E.Car->Alignment->FloorWorldZCm = 37;
	if (!TestTrue(TEXT("Flow started"), E.Runner->InitializeFlow())) { return false; }
	UArrowComponent* Arrow = FP5zbTestAccess::Arrow(E.Runner);
	if (!TestNotNull(TEXT("Arrow exists"), Arrow)) { return false; }
	TestFalse(TEXT("Arrow visible"), Arrow->bHiddenInGame);
	TestTrue(TEXT("Arrow stays 160 cm above target"), Arrow->GetComponentLocation().Equals(FVector(123, -87, 224), 0.01));
	return true;
}
#endif
