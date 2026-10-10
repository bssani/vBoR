#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "P0ButtonBenchMeasurement.h"
#include "../Interaction/VBoRPokeButtonLayout.h"
#include "../Interaction/VBoRHandRigPawn.h"
#include "Interaction/IsdkGrabberComponent.h"
#include "Rig/IsdkGrabInteractionRigComponent.h"

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
	Press.Sample(true, FVector(0, 0, -0.2), FVector::ZeroVector, FVector::UpVector, 10.0);
	TestEqual(TEXT("Maximum persists within a press"), Press.MaxDepthMm, 4.0);
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP0ButtonActiveTest, "vBoR.P0ButtonBench.SimultaneousActive", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP0ButtonActiveTest::RunTest(const FString& Parameters)
{
	using S = EIsdkInteractableState;
	TestEqual(TEXT("Select and Hover both count"), FP0ButtonActiveMeasurement::CountActive(TArray<S>{S::Select, S::Hover}), 2);
	TestEqual(TEXT("Select alone counts"), FP0ButtonActiveMeasurement::CountActive(TArray<S>{S::Select}), 1);
	TestEqual(TEXT("Normal and Disabled do not count"), FP0ButtonActiveMeasurement::CountActive(TArray<S>{S::Normal, S::Disabled}), 0);
	FP0ButtonActiveMeasurement State;
	for (const int32 Count : {0, 1, 2, 2, 3, 2, 1, 2, 2}) { State.Update(Count); }
	TestEqual(TEXT("Current count"), State.Current, 2);
	TestEqual(TEXT("Maximum"), State.Maximum, 3);
	TestEqual(TEXT("Counts entry into >=2, not frames or 2->3"), State.MultipleEpisodes, 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP0ButtonSelectDepthTest, "vBoR.P0ButtonBench.SelectDepth", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP0ButtonSelectDepthTest::RunTest(const FString& Parameters)
{
	FP0ButtonPressMeasurement Press;
	Press.Hover(1.0);
	Press.Select(1.5);
	Press.Sample(true, FVector(0, 0, 0.2), FVector::ZeroVector, FVector::UpVector, 10.0, true);
	TestTrue(TEXT("Select depth available"), Press.bHasDepthAtSelect);
	TestEqual(TEXT("Select keeps negative depth in front of surface"), Press.DepthAtSelectMm, -2.0);
	Press.Sample(true, FVector(0, 0, -0.5), FVector::ZeroVector, FVector::UpVector, 10.0);
	TestEqual(TEXT("Max depth updated separately"), Press.MaxDepthMm, 5.0);
	TestEqual(TEXT("Select snapshot does not follow max"), Press.DepthAtSelectMm, -2.0);
	Press.Unselect();
	Press.Hover(2.0);
	TestFalse(TEXT("Fresh hover resets select snapshot availability"), Press.bHasDepthAtSelect);
	Press.Select(2.1);
	Press.Sample(false, FVector::ZeroVector, FVector::ZeroVector, FVector::UpVector, 10.0, true);
	TestFalse(TEXT("Unavailable select sample is n/a"), Press.bHasDepthAtSelect);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP0ButtonAttemptTest, "vBoR.P0ButtonBench.AttemptOutcomes", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP0ButtonAttemptTest::RunTest(const FString& Parameters)
{
	FP0ButtonPressMeasurement Press;
	Press.Hover(1.0);
	TestTrue(TEXT("Invalid active sample reports tracking cancellation"), Press.Sample(false, FVector::ZeroVector, FVector::ZeroVector, FVector::UpVector, 10.0) == EP0ButtonSampleResult::TrackingCanceled);
	Press.Sample(false, FVector::ZeroVector, FVector::ZeroVector, FVector::UpVector, 10.0);
	TestEqual(TEXT("Repeated invalid sample counts once"), Press.TrackingCanceled, 1);
	Press.Hover(2.0);
	TestTrue(TEXT("Hover ending without select is missed"), Press.Unhover());
	Press.Unhover();
	TestEqual(TEXT("Missed episode counts once"), Press.Missed, 1);
	Press.Hover(3.0);
	Press.Select(3.1);
	Press.Unselect();
	Press.Unhover();
	TestEqual(TEXT("Selected release is not missed"), Press.Missed, 1);
	Press.Hover(4.0);
	Press.Select(4.1);
	FP0ButtonBenchPlacement::CancelMeasurements(MakeArrayView(&Press, 1));
	TestFalse(TEXT("Re-placement cancel cannot complete"), Press.Unselect());
	TestEqual(TEXT("Manual cancel preserves tracking count"), Press.TrackingCanceled, 1);
	TestEqual(TEXT("Manual cancel is not missed"), Press.Missed, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP0ButtonPlacementTest, "vBoR.P0ButtonBench.HeadPlacement", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP0ButtonPlacementTest::RunTest(const FString& Parameters)
{
	const FVector HeadPosition(120, -40, 165);
	const auto Placement = FP0ButtonBenchPlacement::Calculate(FTransform(FRotator(-40, 30, 15), HeadPosition), 45, 25);
	TestTrue(TEXT("Only head yaw carried into bench"), Placement.Rotator().Equals(FRotator(0, 30, 0)));
	TestTrue(TEXT("Horizontal forward 45 cm and below 25 cm"), Placement.GetLocation().Equals(HeadPosition + FRotator(0, 30, 0).RotateVector(FVector(45, 0, 0)) - FVector(0, 0, 25)));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP0ButtonAttemptGroupsTest, "vBoR.P0ButtonBench.AttemptGroups", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP0ButtonAttemptGroupsTest::RunTest(const FString& Parameters)
{
	TArray<FP0ButtonPressMeasurement> Presses;
	Presses.SetNum(4);
	Presses[0].Hover(1.0);
	Presses[0].Unhover();
	auto Totals = FP0ButtonAttemptTotals::Calculate(Presses, 2, 3);
	TestEqual(TEXT("Single miss goes only to single count"), Totals.MissedSingle, 1);
	TestEqual(TEXT("Single miss does not count as cluster pass"), Totals.ClusterPass, 0);
	Presses[2].Hover(2.0);
	Presses[2].Unhover();
	Totals = FP0ButtonAttemptTotals::Calculate(Presses, 2, 3);
	TestEqual(TEXT("Cluster pass does not change single count"), Totals.MissedSingle, 1);
	TestEqual(TEXT("Cluster hover without select goes only to cluster count"), Totals.ClusterPass, 1);
	Presses[3].Hover(3.0);
	Presses[3].Unhover();
	Presses[1].Hover(4.0);
	Presses[1].Sample(false, FVector::ZeroVector, FVector::ZeroVector, FVector::UpVector, 10.0);
	Totals = FP0ButtonAttemptTotals::Calculate(Presses, 2, 3);
	TestEqual(TEXT("Far hover and tracking cancel do not count as single misses"), Totals.MissedSingle, 1);
	TestEqual(TEXT("Far hover and tracking cancel do not count as cluster passes"), Totals.ClusterPass, 1);
	TestEqual(TEXT("Tracking cancellation remains separate"), Totals.TrackingCanceled, 1);
	TArray<FP0ButtonPressMeasurement> OtherHand;
	OtherHand.SetNum(4);
	Totals = FP0ButtonAttemptTotals::Calculate(OtherHand, 2, 3);
	TestEqual(TEXT("Other hand single count stays separate"), Totals.MissedSingle, 0);
	TestEqual(TEXT("Other hand cluster count stays separate"), Totals.ClusterPass, 0);
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
#include "Engine/StaticMesh.h"
#include "Interaction/IsdkClippedPlaneSurface.h"
#include "Interaction/IsdkPokeInteractable.h"
#include "Interaction/IsdkPokeButtonVisual.h"
#include "Interaction/Surfaces/IsdkPointablePlane.h"
#include "IsdkContentAssetPaths.h"
#include "Materials/MaterialInterface.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "GameFramework/WorldSettings.h"
#include "Kismet/GameplayStatics.h"
#include "Components/TextRenderComponent.h"
#include "P0ButtonBench.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP0HandRigGrabTest, "vBoR.P0ButtonBench.GrabDetectionRestrictions", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP0HandRigGrabTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	AVBoRHandRigPawn* Pawn = World->SpawnActor<AVBoRHandRigPawn>();
	World->InitializeActorsForPlay(FURL());
	World->GetWorldSettings()->NotifyBeginPlay();
	TestTrue(TEXT("Pawn reached BeginPlay"), Pawn->HasActorBegunPlay());
	for (int32 Hand = 0; Hand < 2; ++Hand)
	{
		UIsdkHandRigComponent* Rig = Pawn->GetHandRig(Hand);
		UIsdkGrabberComponent* Grabber = Rig->GetGrabber();
		TestNotNull(TEXT("Hand rig has a grabber"), Grabber);
		if (!Grabber) { continue; }
		TestFalse(TEXT("Distance grab disallowed after BeginPlay"), Grabber->IsGrabDetectionTypeAllowed(EIsdkGrabDetectorType::DistanceGrab));
		TestFalse(TEXT("Ray grab disallowed after BeginPlay"), Grabber->IsGrabDetectionTypeAllowed(EIsdkGrabDetectorType::RayGrab));
		TestFalse(TEXT("Poke-only bench starts with grab rig inactive"), Rig->GetGrabInteraction()->IsActive());
		Rig->GetGrabInteraction()->Activate(true);
		TestTrue(TEXT("Grab rig reactivated"), Rig->GetGrabInteraction()->IsActive());
		TestFalse(TEXT("Reactivation cannot allow distance grab"), Grabber->IsGrabDetectionTypeAllowed(EIsdkGrabDetectorType::DistanceGrab));
		TestFalse(TEXT("Reactivation cannot allow ray grab"), Grabber->IsGrabDetectionTypeAllowed(EIsdkGrabDetectorType::RayGrab));
		TestTrue(TEXT("Nearby hand grab remains allowed"), Grabber->IsGrabDetectionTypeAllowed(EIsdkGrabDetectorType::HandGrab));
		Rig->GetGrabInteraction()->Deactivate();
	}
	const FString RigStatus = Pawn->DescribeRig();
	TestFalse(TEXT("Status does not misrepresent detector objects as active distance grabs"), RigStatus.Contains(TEXT("Distance grab:")));
	TestTrue(TEXT("Status reports blocked grab detection for left hand"), RigStatus.Contains(TEXT("Left ray active=0, enabled=0 | grabber active=0, distance allowed=0, ray grab allowed=0")));
	TestTrue(TEXT("Status reports blocked grab detection for right hand"), RigStatus.Contains(TEXT("Right ray active=0, enabled=0 | grabber active=0, distance allowed=0, ray grab allowed=0")));
	World->EndPlay(EEndPlayReason::Quit);
	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP0ButtonComponentTest, "vBoR.P0ButtonBench.ButtonComponents", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP0ButtonComponentTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	AVBoRPokeButton* Button = World->SpawnActor<AVBoRPokeButton>();
	UMaterialInterface* OriginalMaterial = Button->ButtonMesh->GetMaterial(0);
	Button->HoverOpacity = 0.23f;
	Button->SelectOpacity = 0.71f;
	const FTransform MeshBefore = Button->ButtonMesh->GetComponentTransform();
	Button->OnConstruction(Button->GetActorTransform());
	TestTrue(TEXT("Repeated construction keeps mesh rest position"), Button->ButtonMesh->GetComponentTransform().Equals(MeshBefore));
	AddInfo(FString::Printf(TEXT("Plane rotation=%s, Poke forward=%s, native normal=%s"), *Button->Plane->GetComponentRotation().ToString(), *Button->Poke->GetForwardVector().ToString(), *Button->Plane->GetNormal().ToString()));
	TestTrue(TEXT("Plane normal points out of actor top"), Button->Plane->GetNormal().GetSafeNormal().Equals(FVector::UpVector, 1.e-6));
	TestTrue(TEXT("Visual starts 3 mm above full press plane"), FMath::IsNearlyEqual(
		Button->ButtonVisual->GetComponentLocation().Z - Button->Poke->GetComponentLocation().Z, 0.3));
	Button->OnConstruction(Button->GetActorTransform());
	for (const FRotator MeshRotation : {FRotator::ZeroRotator, FRotator(90, 0, 0)})
	{
		Button->ButtonMesh->SetRelativeRotation(MeshRotation);
		Button->OnConstruction(Button->GetActorTransform());
		const FTransform MeshToActor = Button->ButtonMesh->GetComponentTransform().GetRelativeTransform(Button->GetActorTransform());
		const auto Layout = FVBoRPokeButtonLayout::Calculate(Button->ButtonMesh->GetStaticMesh()->GetBoundingBox(), MeshToActor, Button->PressDepthMm, Button->HitAreaScale);
		TestTrue(TEXT("Public plane half-extents match visible Actor Y/X dimensions"),
			Button->Plane->GetSize().Equals(FVector2D(Layout.PlaneSize.Y, Layout.PlaneSize.X) / 2.0));
		TestTrue(TEXT("Plane Right parallels Actor Y"), FMath::IsNearlyEqual(FMath::Abs(FVector::DotProduct(Button->Plane->GetRightVector(), Button->GetActorRightVector())), 1.0));
		TestTrue(TEXT("Plane Up parallels Actor X"), FMath::IsNearlyEqual(FMath::Abs(FVector::DotProduct(Button->Plane->GetUpVector(), Button->GetActorForwardVector())), 1.0));
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
	Bench->PlaceFromHead(FTransform(FVector(100, 20, 165)));
	TArray<AActor*> SpawnedButtons;
	UGameplayStatics::GetAllActorsOfClass(World, AVBoRPokeButton::StaticClass(), SpawnedButtons);
	TestEqual(TEXT("Startup spawns 3 single, 12 cluster, 1 far plus test button"), SpawnedButtons.Num(), 17);
	TestTrue(TEXT("Startup panel displays both-hands active measurements"), Bench->Panel->Text.ToString().Contains(TEXT("Cluster active (both hands):")));
	TestTrue(TEXT("Panel separates tracking cancellation, single misses and cluster passes"), Bench->Panel->Text.ToString().Contains(TEXT("Canceled (tracking): L=0 R=0 | Missed single: L=0 R=0 | cluster pass: L=0 R=0")));
	TArray<FTransform> RelativeBefore;
	for (AActor* Spawned : SpawnedButtons)
	{
		if (Spawned == Button) { continue; }
		TestTrue(TEXT("Bench owns attachment of every trial button"), Spawned->GetAttachParentActor() == Bench);
		RelativeBefore.Add(Spawned->GetActorTransform().GetRelativeTransform(Bench->GetActorTransform()));
	}
	Bench->PlaceFromHead(FTransform(FRotator(-40, 30, 15), FVector(300, -60, 170)));
	int32 RelativeIndex = 0;
	for (AActor* Spawned : SpawnedButtons)
	{
		if (Spawned == Button) { continue; }
		TestTrue(TEXT("Re-placement keeps button relative pose"), Spawned->GetActorTransform().GetRelativeTransform(Bench->GetActorTransform()).Equals(RelativeBefore[RelativeIndex++], 1.e-5));
	}
	TestTrue(TEXT("Visual is bound to poke before play"), Button->ButtonVisual->GetPokeInteractable() == Button->Poke.Get());
	TestTrue(TEXT("Startup preserves original material slot"), Button->ButtonMesh->GetMaterial(0) == OriginalMaterial);
	TestNull(TEXT("Normal has no overlay"), Button->ButtonMesh->GetOverlayMaterial());
	for (const auto State : {EIsdkInteractableState::Hover, EIsdkInteractableState::Select, EIsdkInteractableState::Normal})
	{
		FIsdkInteractableStateEvent Event;
		Event.Interactable = Button->Poke.Get();
		Event.Args.NewState = State;
		Button->Poke->GetInteractableStateChangedDelegate()->Broadcast(Event);
		UMaterialInstanceDynamic* Overlay = Cast<UMaterialInstanceDynamic>(Button->ButtonMesh->GetOverlayMaterial());
		if (State == EIsdkInteractableState::Normal) { TestNull(TEXT("Normal removes overlay"), Overlay); }
		else
		{
			TestNotNull(TEXT("Active state has overlay"), Overlay);
			if (Overlay)
			{
				FLinearColor Expected = State == EIsdkInteractableState::Hover ? Button->HoverColor : Button->SelectColor;
				Expected.A = State == EIsdkInteractableState::Hover ? Button->HoverOpacity : Button->SelectOpacity;
				TestTrue(TEXT("Overlay color and opacity match state"), Overlay->K2_GetVectorParameterValue(TEXT("ColorParam")).Equals(Expected));
				TestTrue(TEXT("Overlay is translucent"), Overlay->GetBlendMode() == BLEND_Translucent);
			}
		}
	}

	World->EndPlay(EEndPlayReason::Quit);
	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP0OverlayMaterialTest, "vBoR.P0ButtonBench.OverlayMaterial", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP0OverlayMaterialTest::RunTest(const FString& Parameters)
{
	UMaterialInterface* Interface = LoadObject<UMaterialInterface>(nullptr, IsdkContentAssetPaths::Materials::GrabbableDefault);
	TestNotNull(TEXT("Existing overlay material available"), Interface);
	if (!Interface) { return false; }
	TestTrue(TEXT("Overlay supports translucency"), Interface->GetBlendMode() == BLEND_Translucent);
#if WITH_EDITOR
	const FExpressionInput& Opacity = Interface->GetMaterial()->GetEditorOnlyData()->Opacity;
	const auto* Vector = Cast<UMaterialExpressionVectorParameter>(Opacity.Expression);
	TestTrue(TEXT("Opacity is driven by ColorParam alpha"), Vector && Vector->ParameterName == TEXT("ColorParam") &&
		Opacity.MaskA && !Opacity.MaskR && !Opacity.MaskG && !Opacity.MaskB);
#endif
	return true;
}

#endif
