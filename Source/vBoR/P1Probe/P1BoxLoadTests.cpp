#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "P1LoadBench.h"
#include "P1TrunkMock.h"
#include "../Interaction/VBoRLoadTarget.h"
#include "../Interaction/VBoRHandRigPawn.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/WorldSettings.h"

namespace
{
	struct FBoxLoadWorld
	{
		UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
		AP1LoadBench* Bench = nullptr;
		FBoxLoadWorld()
		{
			APlayerController* Controller = World->SpawnActor<APlayerController>();
			AVBoRHandRigPawn* Pawn = World->SpawnActor<AVBoRHandRigPawn>();
			Pawn->SetActorLocation(FVector(0, 0, 92));
			Controller->Possess(Pawn);
			Bench = World->SpawnActor<AP1LoadBench>();
			World->InitializeActorsForPlay(FURL());
			World->GetWorldSettings()->NotifyBeginPlay();
			Bench->PlaceFromHead(FTransform(FRotator(0, 37, 0), FVector(200, -100, 257)));
		}
		~FBoxLoadWorld() { World->EndPlay(EEndPlayReason::Quit); World->DestroyWorld(false); }
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP1BoxSizeTest, "vBoR.P1Box.SizeMeshCollisionAndFit", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP1BoxSizeTest::RunTest(const FString& Parameters)
{
	for (const FVector Size : {FVector(20, 30, 40), FVector(60, 70, 80)})
	{
		FBoxLoadWorld Fixture;
		AP1LoadBench* Bench = Fixture.Bench;
		Bench->BoxSizeCm = Size;
		Bench->PlaceFromHead(FTransform(FVector(0, 0, 257)));
		AVBoRCarryObject* Box = Bench->Bag;
		TestTrue(TEXT("E1 mesh dimensions use edited cm size"),
			(Box->GetMeshLocalBounds().GetSize() * Box->GetActorScale3D()).Equals(Size, 0.01));
		TestTrue(TEXT("E1 collision dimensions use same cm size"),
			Box->GetCollisionBoundsInPose(Box->GetActorTransform(), Bench->GetActorTransform()).GetSize().Equals(Size, 0.02));
		TestTrue(TEXT("E1 target guide dimensions follow bench size"), Bench->Target->GoalSizeCm.Equals(Size));
		TestTrue(TEXT("E1 start bottom rests on stand for each size"), FMath::IsNearlyEqual(
			Box->GetCollisionBoundsInPose(Box->GetActorTransform(), Bench->GetActorTransform()).Min.Z, 75.01, 0.02));
		FTransform Goal = Bench->Target->GetGoalPose();
		Goal.SetScale3D(Box->GetActorScale3D());
		TestTrue(TEXT("R1-1 target bottom follows trunk floor for each size"), FMath::IsNearlyEqual(
			Box->GetCollisionBoundsInPose(Goal, Bench->Trunk->GetActorTransform()).Min.Z, Bench->Trunk->FloorHeightCm, 0.02));
		TestEqual(TEXT("R1-1 static fit accepts 40 cm height and rejects 80 cm height"), Bench->Target->IsStaticFit(Box), Size.Z <= 70);
		Box->SetActorLocation(Bench->GetActorTransform().TransformPosition(FVector(45, 0, 115)));
		TestEqual(TEXT("E1 loading containment uses resized collision"), Bench->Target->IsObjectInside(Box), Size.Z <= 70);
		if (Size.Z == 40)
		{
			Goal.AddToTranslation(FVector(0, 0, 5));
			Box->ResetToPose(Goal);
			Box->bUsePlacementAssist = true;
			Box->BeginCarry(0, nullptr, Box->GetActorTransform());
			Box->ReleaseCarry();
			TestTrue(TEXT("R1-1 E12 40 cm box starts opted-in assistance near target"), Box->bAssisting);
			Box->AssistStartedAt = Fixture.World->GetTimeSeconds() - Box->AssistDurationSeconds;
			Box->Tick(0.1);
			TestTrue(TEXT("R1-1 E12 40 cm box completes assisted seating"), Box->GetMetrics().Outcome == EVBoRCarryOutcome::Assisted);
		}
		Box->BoxSizeCm = FVector(10, 12, 14);
		Box->OnConstruction(Box->GetActorTransform());
		TestTrue(TEXT("E1 carry property updates mesh and collision without bench recreation"),
			Box->GetCollisionBoundsInPose(Box->GetActorTransform(), Bench->GetActorTransform()).GetSize().Equals(Box->BoxSizeCm, 0.02));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP1BoxPlacementTest, "vBoR.P1Box.DropStackAndHeldCollision", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP1BoxPlacementTest::RunTest(const FString& Parameters)
{
	FBoxLoadWorld Fixture;
	AP1LoadBench* Bench = Fixture.Bench;
	const FTransform Frame = Bench->GetActorTransform();
	AVBoRCarryObject* Lower = Bench->Bag;
	TestFalse(TEXT("E3 placement assistance defaults off"), Lower->bUsePlacementAssist);
	Lower->BeginCarry(0, nullptr, Lower->GetActorTransform());
	Lower->SetActorLocation(Frame.TransformPosition(FVector(45, 0, 125)));
	Bench->Tick(0);
	const FTransform Release = Lower->GetActorTransform();
	Lower->ReleaseCarry();
	TestFalse(TEXT("E3 release starts no assistance"), Lower->bAssisting);
	Lower->Tick(1);
	TestTrue(TEXT("E3 drop stops on trunk floor"), FMath::IsNearlyEqual(
		Frame.InverseTransformPosition(Lower->GetActorLocation()).Z, 90.0, 0.03));
	TestTrue(TEXT("E3 drop preserves XY and rotation"), Lower->GetActorLocation().Equals(
		Release.GetLocation() + FVector(0, 0, Lower->GetActorLocation().Z - Release.GetLocation().Z), 0.01) &&
		Lower->GetActorQuat().Equals(Release.GetRotation()));
	TestEqual(TEXT("E3 one resting box inside"), Bench->GetRestingBoxCount(), 1);
	AVBoRCarryObject* Upper = Bench->Bag;
	Upper->BeginCarry(1, nullptr, Upper->GetActorTransform());
	Upper->SetActorLocation(Frame.TransformPosition(FVector(45, 0, 140)));
	Bench->Tick(0);
	const FTransform LowerRest = Lower->GetActorTransform();
	Upper->ReleaseCarry();
	Upper->Tick(1);
	TestTrue(TEXT("E4 second box stops on lower box"), FMath::IsNearlyEqual(
		Frame.InverseTransformPosition(Upper->GetActorLocation()).Z, 120.0, 0.04));
	TestTrue(TEXT("E4 lower box stays still"), Lower->GetActorTransform().Equals(LowerRest));
	TestEqual(TEXT("E4 both resting boxes counted"), Bench->GetRestingBoxCount(), 2);
	TestEqual(TEXT("All boxes contribute drop metrics"), Bench->GetTotals().DroppedInside, 2);
	TestEqual(TEXT("All boxes contribute grab metrics"), Bench->GetTotals().Grabbed, 2);
	Upper->BeginCarry(0, nullptr, Upper->GetActorTransform());
	FTransform Desired = Upper->GetActorTransform();
	Desired.SetLocation(Lower->GetActorLocation());
	Upper->MoveHeldPose(Desired, 1.0f / 60.0f);
	TestTrue(TEXT("E5 held box is blocked by resting box"), Upper->IsBlocked());
	TestTrue(TEXT("E5 lower box cannot be pushed"), Lower->GetActorTransform().Equals(LowerRest));
	TestTrue(TEXT("E5 held pose stays clear"), Upper->IsPoseClear(Upper->GetActorTransform()));
	TestFalse(TEXT("E4 no physics simulation"), Lower->Mesh->IsSimulatingPhysics());
	TestEqual(TEXT("Held box excluded from resting count"), Bench->GetRestingBoxCount(), 1);
	Bench->Tick(0);
	TestTrue(TEXT("Panel shows aggregate box count"), Bench->Panel->Text.ToString().Contains(TEXT("resting inside trunk=1")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP1BoxSupplyTest, "vBoR.P1Box.SupplyFloorAndReset", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP1BoxSupplyTest::RunTest(const FString& Parameters)
{
	FBoxLoadWorld Fixture;
	AP1LoadBench* Bench = Fixture.Bench;
	AVBoRCarryObject* First = Bench->Bag;
	const FTransform Start = First->GetActorTransform();
	First->BeginCarry(0, nullptr, Start);
	Bench->Tick(0);
	TestEqual(TEXT("E6 grabbing alone makes no box"), Bench->Boxes.Num(), 1);
	First->AddActorWorldOffset(FVector(0, 0, 5));
	Bench->Tick(0);
	TestEqual(TEXT("E6 still inside source slot makes no box"), Bench->Boxes.Num(), 1);
	First->SetActorLocation(Start.GetLocation() + FVector(0, 0, 40));
	Bench->Tick(0);
	TestEqual(TEXT("E7 clear source slot replenishes once"), Bench->Boxes.Num(), 2);
	AVBoRCarryObject* Second = Bench->Bag;
	TestTrue(TEXT("E7 new box occupies original stand slot"), Second->GetActorLocation().Equals(Start.GetLocation(), 0.01));
	TestTrue(TEXT("E7 new box does not overlap held box"), Second->IsPoseClear(Second->GetActorTransform()));
	First->SetActorLocation(Start.GetLocation());
	Bench->Tick(0);
	First->SetActorLocation(Start.GetLocation() + FVector(0, 0, 40));
	Bench->Tick(0);
	TestEqual(TEXT("E7 old source cannot replenish again"), Bench->Boxes.Num(), 2);
	// 기존 박스를 출고 자리에 되돌리면 다음 출고도 기다림 (P1b §3)
	Second->BeginCarry(1, nullptr, Second->GetActorTransform());
	Second->SetActorLocation(Start.GetLocation() + FVector(0, 0, 80));
	First->SetActorLocation(Start.GetLocation());
	Bench->Tick(0);
	TestEqual(TEXT("Occupied source slot postpones replenishment"), Bench->Boxes.Num(), 2);
	First->SetActorLocation(Start.GetLocation() + FVector(0, 150, 0));
	Bench->Tick(0);
	TestEqual(TEXT("Cleared slot replenishes pending source"), Bench->Boxes.Num(), 3);
	AVBoRCarryObject* StandBox = Bench->Bag;
	First->SetActorLocation(Bench->GetActorLocation() + FVector(-250, 0, -50));
	First->Tick(0);
	TestTrue(TEXT("E9 held box below floor survives"), IsValid(First));
	TestEqual(TEXT("E9 hand below floor keeps all boxes"), Bench->Boxes.Num(), 3);
	// 합성 손은 실제 XR 입력이 없으므로 보류됨. 유효 복구 후 의도한 놓기만 허용함 (E9)
	FTransform Hands[2] = {First->GetActorTransform(), FTransform::Identity};
	FTransform Desired;
	First->Session.UpdateHands(true, Hands, First->GetActorTransform(), 1, First->TiltLimitDeg, Desired);
	First->SetActorLocation(Bench->GetActorLocation() + FVector(-250, 0, 100));
	First->ReleaseCarry();
	First->Tick(0.1);
	TestTrue(TEXT("E8 released box above floor keeps falling"), IsValid(First));
	First->Tick(1);
	TestFalse(TEXT("E8 released box at physical floor is removed"), IsValid(First));
	TestEqual(TEXT("E8 removes only fallen box"), Bench->Boxes.Num(), 2);
	TestTrue(TEXT("E8 stand box remains unchanged"), IsValid(StandBox) && StandBox == Bench->Bag);
	TestEqual(TEXT("Destroyed box grab metrics retained"), Bench->GetTotals().Grabbed, 2);
	TestEqual(TEXT("E8 floor disappearance counts outside drop once"), Bench->GetTotals().DroppedOutside, 1);
	const FTransform Before = Bench->GetActorTransform();
	Bench->ResetBagForNextTrial();
	TestEqual(TEXT("E11 reset leaves one box"), Bench->Boxes.Num(), 1);
	TestFalse(TEXT("E11 removes held box"), IsValid(Second));
	TestFalse(TEXT("E11 removes prior stand box"), IsValid(StandBox));
	TestEqual(TEXT("E11 new stand box has no holding hand"), Bench->Bag->Session.Hand, INDEX_NONE);
	TestTrue(TEXT("E11 B preserves bench placement"), Bench->GetActorTransform().Equals(Before));
	TestEqual(TEXT("E11 B archives each metric once"), Bench->GetTotals().Grabbed, 2);
	Bench->ResetBagForNextTrial();
	TestEqual(TEXT("Repeated B cannot duplicate metrics"), Bench->GetTotals().Grabbed, 2);
	Bench->PlaceFromHead(FTransform(FRotator(0, -50, 0), FVector(0, 0, 257)));
	TestEqual(TEXT("R starts with one new box"), Bench->Boxes.Num(), 1);
	TestEqual(TEXT("R clears accumulated metrics"), Bench->GetTotals().Grabbed, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP1BoxHandsTest, "vBoR.P1Box.HandsTrackingAndAssist", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP1BoxHandsTest::RunTest(const FString& Parameters)
{
	FBoxLoadWorld Fixture;
	AP1LoadBench* Bench = Fixture.Bench;
	AVBoRCarryObject* Box = Bench->Bag;
	Box->SetActorLocation(Box->GetActorLocation() + FVector(0, 0, 80));
	const FTransform Rest = Box->GetActorTransform();
	Box->Session.SetRestPose(Rest);
	FTransform Hands[2] = {FTransform(Rest.GetLocation() + FVector(0, -20, 0)), FTransform(Rest.GetLocation() + FVector(0, 20, 0))};
	Box->BeginCarry(0, nullptr, Hands[0]);
	Hands[0].AddToTranslation(FVector(10, 0, 0));
	FTransform Desired;
	Box->Session.UpdateHands(true, Hands, Rest, 1, Box->TiltLimitDeg, Desired);
	Box->MoveHeldPose(Desired, 1.0f / 60.0f);
	TestTrue(TEXT("E2 one hand moves box by palm displacement"), Box->GetActorLocation().Equals(Rest.GetLocation() + FVector(10, 0, 0), 0.01));
	Box->BeginCarry(1, nullptr, Hands[1]);
	const FTransform PairStart = Box->GetActorTransform();
	const FVector PairLine = Hands[1].GetLocation() - Hands[0].GetLocation();
	Hands[1].SetLocation(Hands[0].GetLocation() + FRotator(0, 30, 0).RotateVector(PairLine));
	Box->Session.UpdateHands(true, Hands, PairStart, 2, Box->TiltLimitDeg, Desired);
	Box->MoveHeldPose(Desired, 1.0f / 60.0f);
	TestTrue(TEXT("E2 opposite-side hands turn by pair yaw"), FMath::IsNearlyEqual(
		(Box->GetActorQuat() * PairStart.GetRotation().Inverse()).Rotator().Yaw, 30.0, 0.01));
	TestTrue(TEXT("E2 both hands preserve box size"), Box->GetActorScale3D().Equals(Rest.GetScale3D()));
	Hands[1].AddToTranslation(FVector(0, 0, 300));
	Box->Session.UpdateHands(true, Hands, Box->GetActorTransform(), 3, Box->TiltLimitDeg, Desired);
	Box->MoveHeldPose(Desired, 1.0f / 60.0f);
	const FQuat Tilt = Box->GetActorQuat() * Rest.GetRotation().Inverse();
	TestTrue(TEXT("E2 two-hand tilt remains constrained"), FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(
		FVector::DotProduct(Tilt.RotateVector(FVector::UpVector), FVector::UpVector), -1.0, 1.0))) <= 45.01);
	const FTransform Frozen = Box->GetActorTransform();
	Box->Session.UpdateHands(false, Hands, Frozen, 4, Box->TiltLimitDeg, Desired);
	Box->ReleaseCarry(0);
	Box->ReleaseCarry(1);
	Bench->Tick(0);
	TestEqual(TEXT("E10 loss cannot refill source"), Bench->Boxes.Num(), 1);
	TestTrue(TEXT("E10 loss retains both hands"), Box->Session.IsTwoHanded());
	TestTrue(TEXT("E10 loss cannot drop or destroy"), IsValid(Box) && !Box->bDropping && Box->GetActorTransform().Equals(Frozen));
	Hands[0].AddToTranslation(FVector(1000, 0, 0));
	Hands[1].AddToTranslation(FVector(1000, 0, 0));
	TestFalse(TEXT("E10 recovered frame only rebases"), Box->Session.UpdateHands(true, Hands, Frozen, 5, Box->TiltLimitDeg, Desired));
	TestTrue(TEXT("E10 accumulated loss displacement discarded"), Desired.Equals(Frozen));
	Bench->Tick(0);
	TestEqual(TEXT("E10 valid recovered source can replenish once"), Bench->Boxes.Num(), 2);
	Box->ResetToPose(FTransform(Bench->Target->GetActorQuat(), Bench->Target->GetActorLocation() + FVector(0, 0, 5), Box->GetActorScale3D()));
	Box->bUsePlacementAssist = true;
	Box->BeginCarry(0, nullptr, Box->GetActorTransform());
	Box->ReleaseCarry();
	TestTrue(TEXT("E12 explicit opt-in starts existing assistance"), Box->bAssisting);
	Box->AssistStartedAt = Fixture.World->GetTimeSeconds() - 0.3;
	Box->Tick(0.1);
	TestTrue(TEXT("E12 existing assisted outcome preserved"), Box->GetMetrics().Outcome == EVBoRCarryOutcome::Assisted);
	return true;
}
#endif
