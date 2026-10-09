#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "P0ButtonBenchMeasurement.h"
#include "../Interaction/VBoRPokeButtonLayout.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP0ButtonDepthTest, "vBoR.P0ButtonBench.RawPenetration", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP0ButtonDepthTest::RunTest(const FString& Parameters)
{
	FP0ButtonPressMeasurement Press;
	Press.Hover(1.0);
	Press.Sample(true, FVector(0, 0, 2), FVector::ZeroVector, FVector::UpVector, 10.0);
	TestTrue(TEXT("Valid sample distinguished from unavailable depth"), Press.bHasDepth);
	TestEqual(TEXT("In front is zero mm"), Press.MaxDepthMm, 0.0);
	Press.Select(1.5);
	Press.Sample(true, FVector(0, 0, -0.4), FVector::ZeroVector, FVector::UpVector, 10.0);
	Press.Sample(false, FVector(0, 0, -100), FVector::ZeroVector, FVector::UpVector, 10.0);
	Press.Sample(true, FVector(0, 0, -0.2), FVector::ZeroVector, FVector::UpVector, 10.0);
	TestEqual(TEXT("Maximum persists, invalid tracking excluded"), Press.MaxDepthMm, 4.0);
	TestTrue(TEXT("Only selected release completes a press"), Press.Unselect());
	TestFalse(TEXT("Duplicate release is not a new press"), Press.Unselect());
	Press.Sample(true, FVector(0, 0, -5), FVector::ZeroVector, FVector::UpVector, 10.0);
	TestEqual(TEXT("After release sample cannot inflate finished press"), Press.MaxDepthMm, 4.0);
	Press.Hover(2.0);
	TestEqual(TEXT("New hover resets maximum"), Press.MaxDepthMm, 0.0);
	Press.Select(2.2);
	Press.Cancel();
	TestFalse(TEXT("Tracking cancel is not completion"), Press.Unselect());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP0ButtonTimeTest, "vBoR.P0ButtonBench.HoverToSelect", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP0ButtonTimeTest::RunTest(const FString& Parameters)
{
	FP0ButtonPressMeasurement Press;
	Press.Hover(3.0);
	Press.Hover(3.5);
	Press.Select(4.25);
	TestEqual(TEXT("Repeated hover does not overwrite initial time"), Press.HoverToSelectSeconds, 1.25);
	Press.Unselect();
	Press.Select(5.0);
	TestEqual(TEXT("Missing fresh hover is n/a"), Press.HoverToSelectSeconds, -1.0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP0ButtonHoverTest, "vBoR.P0ButtonBench.SimultaneousHover", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP0ButtonHoverTest::RunTest(const FString& Parameters)
{
	FP0ButtonHoverMeasurement State;
	for (const int32 Count : {0, 1, 2, 2, 3, 2, 1, 2, 2}) { State.Update(Count); }
	TestEqual(TEXT("Current count"), State.Current, 2);
	TestEqual(TEXT("Maximum"), State.Maximum, 3);
	TestEqual(TEXT("Counts entry into >=2, not frames or 2->3"), State.MultipleEpisodes, 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP0ButtonTargetTest, "vBoR.P0ButtonBench.TargetSwitches", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP0ButtonTargetTest::RunTest(const FString& Parameters)
{
	FP0ButtonTargetMeasurement State;
	for (const int32 Target : {2, 2, 3, 3, 2}) { State.Hover(Target); }
	TestEqual(TEXT("First target and repeat events do not count"), State.Switches, 2);
	State.Reset();
	State.Hover(7);
	TestEqual(TEXT("New press has no previous target"), State.Switches, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP0ButtonLayoutTest, "vBoR.P0ButtonBench.MeshLayout", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP0ButtonLayoutTest::RunTest(const FString& Parameters)
{
	const FBox Bounds(FVector(-1, -2, -0.5), FVector(1, 2, 0.5));
	const auto Layout = FVBoRPokeButtonLayout::Calculate(Bounds, FTransform(FVector(3, 4, 5)), 3.0, 1.5);
	TestTrue(TEXT("Front center from actor-space top"), Layout.FrontCenter.Equals(FVector(3, 4, 5.5)));
	TestTrue(TEXT("3 mm travel is 0.3 cm down"), Layout.PlaneCenter.Equals(FVector(3, 4, 5.2)));
	TestTrue(TEXT("Full plane size expands, mesh does not"), Layout.PlaneSize.Equals(FVector2D(3, 6)));
	const auto Rotated = FVBoRPokeButtonLayout::Calculate(Bounds, FTransform(FRotator(90, 0, 0)), 3.0, 1.0);
	TestTrue(TEXT("Rotated actor bounds define new top"), FMath::IsNearlyEqual(Rotated.FrontCenter.Z, 1.0));
	TestTrue(TEXT("90 degree rotation swaps width and thickness"), Rotated.PlaneSize.Equals(FVector2D(1, 4)));
	return true;
}

#include "../Interaction/VBoRPokeButton.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Interaction/IsdkClippedPlaneSurface.h"
#include "Interaction/IsdkPokeInteractable.h"
#include "Interaction/IsdkPokeButtonVisual.h"
#include "Interaction/Surfaces/IsdkPointablePlane.h"
#include "IsdkContentAssetPaths.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "GameFramework/WorldSettings.h"
#include "Kismet/GameplayStatics.h"
#include "Components/TextRenderComponent.h"
#include "P0ButtonBench.h"
#include "isdk_api/isdk_api.hpp"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP0ButtonComponentTest, "vBoR.P0ButtonBench.ButtonComponents", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP0ButtonComponentTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	AVBoRPokeButton* Button = World->SpawnActor<AVBoRPokeButton>();
	const FTransform MeshBefore = Button->ButtonMesh->GetComponentTransform();
	Button->OnConstruction(Button->GetActorTransform());
	TestTrue(TEXT("Repeated construction keeps mesh rest position"), Button->ButtonMesh->GetComponentTransform().Equals(MeshBefore));
	AddInfo(FString::Printf(TEXT("Plane rotation=%s, Poke forward=%s, native normal=%s"), *Button->Plane->GetComponentRotation().ToString(), *Button->Poke->GetForwardVector().ToString(), *Button->Plane->GetNormal().ToString()));
	TestTrue(TEXT("Plane normal points out of actor top"), Button->Plane->GetNormal().GetSafeNormal().Equals(FVector::UpVector, 1.e-6));
	TestTrue(TEXT("Visual starts 3 mm above full press plane"), FMath::IsNearlyEqual(
		Button->ButtonVisual->GetComponentLocation().Z - Button->Poke->GetComponentLocation().Z, 0.3));
	auto* Surface = Button->Surface->GetApiClippedPlaneSurface();
	TestNotNull(TEXT("Native clipped surface built"), Surface);
	Button->OnConstruction(Button->GetActorTransform());
	if (Surface)
	{
		isdk_Ray Ray{};
		Ray.direction = {0, 0, -1};
		isdk_SurfaceHit Hit{};
		Ray.origin = {0.74f, 0, 2};
		TestTrue(TEXT("Ray just inside 15 mm visible width hits"), Surface->raycast(&Ray, &Hit, 10) != 0);
		Ray.origin = {0.76f, 0, 2};
		TestFalse(TEXT("Ray just outside 15 mm visible width misses"), Surface->raycast(&Ray, &Hit, 10) != 0);
	}
	UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, IsdkContentAssetPaths::Materials::GrabbableDefault);
	TArray<FMaterialParameterInfo> Infos;
	TArray<FGuid> Ids;
	if (Material) { Material->GetAllVectorParameterInfo(Infos, Ids); }
	for (const auto& Info : Infos) { AddInfo(TEXT("Color material parameter: ") + Info.Name.ToString()); }
	TestTrue(TEXT("State color uses an actual vector parameter"), Infos.ContainsByPredicate([](const FMaterialParameterInfo& Info) { return Info.Name == TEXT("ColorParam"); }));
	AP0ButtonBench* Bench = World->SpawnActor<AP0ButtonBench>();
	World->InitializeActorsForPlay(FURL());
	World->GetWorldSettings()->NotifyBeginPlay();
	TArray<AActor*> SpawnedButtons;
	UGameplayStatics::GetAllActorsOfClass(World, AVBoRPokeButton::StaticClass(), SpawnedButtons);
	TestEqual(TEXT("Startup spawns 3 single, 12 cluster, 1 far plus test button"), SpawnedButtons.Num(), 17);
	TestTrue(TEXT("Startup panel displays measurements"), Bench->Panel->Text.ToString().Contains(TEXT("Cluster hover:")));
	TestTrue(TEXT("Visual is bound to poke before play"), Button->ButtonVisual->GetPokeInteractable() == Button->Poke.Get());
	UMaterialInstanceDynamic* Color = Cast<UMaterialInstanceDynamic>(Button->ButtonMesh->GetMaterial(0));
	TestNotNull(TEXT("Startup creates a color material"), Color);
	if (Color)
	{
		TestTrue(TEXT("Normal color is visible in material"), Color->K2_GetVectorParameterValue(TEXT("ColorParam")).Equals(Button->NormalColor));
		for (const auto State : {EIsdkInteractableState::Hover, EIsdkInteractableState::Select, EIsdkInteractableState::Normal})
		{
			FIsdkInteractableStateEvent Event;
			Event.Interactable = Button->Poke.Get();
			Event.Args.NewState = State;
			Button->Poke->GetInteractableStateChangedDelegate()->Broadcast(Event);
			const FLinearColor Expected = State == EIsdkInteractableState::Hover ? Button->HoverColor
				: State == EIsdkInteractableState::Select ? Button->SelectColor : Button->NormalColor;
			TestTrue(TEXT("State event updates actual material color"), Color->K2_GetVectorParameterValue(TEXT("ColorParam")).Equals(Expected));
		}
	}

	World->EndPlay(EEndPlayReason::Quit);
	World->DestroyWorld(false);
	return true;
}

#endif
