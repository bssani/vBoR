#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "../Interaction/VBoRBoxStand.h"
#include "../Interaction/VBoRCarryObject.h"
#include "../Interaction/VBoRLoadTarget.h"
#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/WorldSettings.h"

struct FP5zfTestAccess
{
	static bool Grab(AVBoRCarryObject* Box, int32 Hand = 0)
	{ return Box->BeginCarry(Hand, nullptr, FTransform(Box->GetActorLocation() + FVector(0, Hand * 10, 0))); }
	static void Release(AVBoRCarryObject* Box, int32 Hand = INDEX_NONE) { Box->ReleaseCarry(Hand); }
	static void Track(AVBoRCarryObject* Box, bool bTracked)
	{
		FTransform Desired;
		const FTransform Poses[2] = {Box->Session.LastHands[0], Box->Session.LastHands[1]};
		Box->Session.UpdateHands(bTracked, Poses, Box->GetActorTransform(), Box->GetWorld()->GetTimeSeconds(), Box->TiltLimitDeg, Desired);
	}
};

namespace
{
struct FP5zfWorld
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	AVBoRBoxStand* Stand;
	FTransform Start;
	FP5zfWorld()
	{
		Stand = World->SpawnActorDeferred<AVBoRBoxStand>(AVBoRBoxStand::StaticClass(), FTransform(FRotator(0, 30, 0), FVector(100, 200, 10)));
		Stand->LoadTarget = World->SpawnActor<AVBoRLoadTarget>();
		Stand->LoadTarget->SetActorLocation(FVector(400, 200, 100));
		Stand->LoadTarget->LoadingRegion->SetBoxExtent(FVector(100));
		Stand->FinishSpawning(Stand->GetActorTransform());
		World->InitializeActorsForPlay(FURL());
		World->GetWorldSettings()->NotifyBeginPlay();
		Start = Stand->SourceBox->GetActorTransform();
	}
	~FP5zfWorld() { World->EndPlay(EEndPlayReason::Quit); World->DestroyWorld(false); }
	int32 Count() const
	{
		int32 Count = 0;
		for (TActorIterator<AVBoRCarryObject> It(World); It; ++It) { Count += It->GetOwner() == Stand; }
		return Count;
	}
	void Lift(AVBoRCarryObject* Box, double Cm = 50) { Box->SetActorLocation(Start.GetLocation() + FVector(0, 0, Cm)); }
	void TickStand() { Stand->Tick(0); }
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5zfHeld, "vBoR.P5zf.E1HeldOutsideSlot", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5zfHeld::RunTest(const FString&)
{
	FP5zfWorld W; auto* First = W.Stand->SourceBox.Get();
	TestTrue(TEXT("Grab accepted"), FP5zfTestAccess::Grab(First)); W.Lift(First);
	for (int32 I = 0; I < 3; ++I)
	{
		W.TickStand(); TestEqual(TEXT("Held box prevents refill every tick"), W.Count(), 1);
		TestTrue(TEXT("Original source retained"), W.Stand->SourceBox == First);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5zfReleased, "vBoR.P5zf.E2RefillAfterRelease", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5zfReleased::RunTest(const FString&)
{
	FP5zfWorld W; auto* First = W.Stand->SourceBox.Get();
	TestTrue(TEXT("Grab accepted"), FP5zfTestAccess::Grab(First)); W.Lift(First); W.TickStand();
	TestEqual(TEXT("Held source stays alone"), W.Count(), 1);
	FP5zfTestAccess::Release(First);
	TestEqual(TEXT("Release clears hand"), First->GetCarrySession().Hand, INDEX_NONE);
	TestEqual(TEXT("Release alone does not spawn"), W.Count(), 1);
	W.TickStand(); TestEqual(TEXT("Next stand tick supplies one"), W.Count(), 2);
	TestTrue(TEXT("Replacement differs from released source"), W.Stand->SourceBox != First);
	TestTrue(TEXT("Replacement uses original position and rotation"), W.Stand->SourceBox->GetActorTransform().Equals(W.Start));
	W.TickStand(); TestEqual(TEXT("Following tick has no duplicate"), W.Count(), 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5zfNearSlot, "vBoR.P5zf.E3NearSlotHeld", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5zfNearSlot::RunTest(const FString&)
{
	FP5zfWorld W; auto* First = W.Stand->SourceBox.Get();
	TestTrue(TEXT("Grab accepted"), FP5zfTestAccess::Grab(First));
	W.Lift(First, 32); // Native box height 30 cm + 2 cm (P5zf E3)
	for (int32 I = 0; I < 3; ++I) { W.TickStand(); TestEqual(TEXT("Near-slot held box prevents refill"), W.Count(), 1); }
	FP5zfTestAccess::Release(First); W.TickStand();
	TestEqual(TEXT("Released near-slot box permits refill"), W.Count(), 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5zfTracking, "vBoR.P5zf.E4TrackingPauseRecovery", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5zfTracking::RunTest(const FString&)
{
	FP5zfWorld W; auto* First = W.Stand->SourceBox.Get();
	TestTrue(TEXT("Grab accepted"), FP5zfTestAccess::Grab(First)); W.Lift(First);
	FP5zfTestAccess::Track(First, false);
	TestTrue(TEXT("Tracking loss pauses held session"), First->GetCarrySession().bPaused && First->GetCarrySession().Hand == 0);
	W.TickStand(); TestEqual(TEXT("Paused session prevents refill"), W.Count(), 1);
	FP5zfTestAccess::Track(First, true);
	TestFalse(TEXT("Tracking recovery clears pause"), First->GetCarrySession().bPaused);
	TestEqual(TEXT("Recovery keeps hand ownership"), First->GetCarrySession().Hand, 0);
	W.TickStand(); TestEqual(TEXT("Recovered held session prevents refill"), W.Count(), 1);
	FP5zfTestAccess::Release(First); W.TickStand();
	TestEqual(TEXT("Release after recovery permits refill"), W.Count(), 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5zfTwoHands, "vBoR.P5zf.E5TwoHandRelease", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5zfTwoHands::RunTest(const FString&)
{
	for (int32 ReleasingHand : {0, 1})
	{
		FP5zfWorld W; auto* First = W.Stand->SourceBox.Get();
		TestTrue(TEXT("First hand accepted"), FP5zfTestAccess::Grab(First, 0));
		TestTrue(TEXT("Second hand accepted"), FP5zfTestAccess::Grab(First, 1));
		TestTrue(TEXT("Two-hand session"), First->GetCarrySession().IsTwoHanded()); W.Lift(First);
		W.TickStand(); TestEqual(TEXT("Two hands prevent refill"), W.Count(), 1);
		FP5zfTestAccess::Release(First, ReleasingHand);
		TestEqual(TEXT("Other hand remains"), First->GetCarrySession().Hand, 1 - ReleasingHand);
		W.TickStand(); TestEqual(TEXT("Remaining hand prevents refill"), W.Count(), 1);
		FP5zfTestAccess::Release(First); W.TickStand();
		TestEqual(TEXT("Final release permits refill"), W.Count(), 2);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5zfReturned, "vBoR.P5zf.E6ReturnedToSlot", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5zfReturned::RunTest(const FString&)
{
	FP5zfWorld W; auto* First = W.Stand->SourceBox.Get();
	W.Lift(First); W.TickStand(); TestEqual(TEXT("Ungrabbed displacement never refills"), W.Count(), 1);
	First->SetActorTransform(W.Start);
	TestTrue(TEXT("Grab accepted"), FP5zfTestAccess::Grab(First)); W.Lift(First); W.TickStand();
	First->SetActorTransform(W.Start); FP5zfTestAccess::Release(First);
	TestEqual(TEXT("Returned box released"), First->GetCarrySession().Hand, INDEX_NONE);
	W.TickStand(); TestEqual(TEXT("Returned box occupies slot"), W.Count(), 1);
	TestTrue(TEXT("Returned box stays source"), W.Stand->SourceBox == First);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5zfRegrab, "vBoR.P5zf.E7RegrabLoadedBox", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5zfRegrab::RunTest(const FString&)
{
	FP5zfWorld W; auto* First = W.Stand->SourceBox.Get();
	TestTrue(TEXT("Grab accepted"), FP5zfTestAccess::Grab(First));
	First->SetActorTransform(W.Stand->LoadTarget->GetGoalPose()); FP5zfTestAccess::Release(First);
	TestTrue(TEXT("Released box inside loading region"), W.Stand->LoadTarget->IsObjectInside(First));
	W.TickStand(); TestEqual(TEXT("Replacement ready"), W.Count(), 2);
	auto* Replacement = W.Stand->SourceBox.Get();
	TestTrue(TEXT("Loaded box regrab accepted"), FP5zfTestAccess::Grab(First)); First->AddActorWorldOffset(FVector(0, 0, 50));
	W.TickStand(); TestEqual(TEXT("Regrab leaves count unchanged"), W.Count(), 2);
	TestTrue(TEXT("Replacement remains source at start"), W.Stand->SourceBox == Replacement && Replacement->GetActorTransform().Equals(W.Start));
	// An older held box also blocks refill when the current source clears (P5zf D1)
	TestTrue(TEXT("Other hand grabs replacement"), FP5zfTestAccess::Grab(Replacement, 1));
	W.Lift(Replacement); FP5zfTestAccess::Release(Replacement, 1); W.TickStand();
	TestEqual(TEXT("Older held box prevents third spawn"), W.Count(), 2);
	FP5zfTestAccess::Release(First); W.TickStand();
	TestEqual(TEXT("All boxes released permit third spawn"), W.Count(), 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5zfFloor, "vBoR.P5zf.E8FloorRemoval", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5zfFloor::RunTest(const FString&)
{
	FP5zfWorld W; auto* First = W.Stand->SourceBox.Get();
	TestTrue(TEXT("Grab accepted"), FP5zfTestAccess::Grab(First));
	First->SetActorLocation(FVector(500, 500, 35)); FP5zfTestAccess::Release(First); First->Tick(1);
	TestFalse(TEXT("Released box removed at stand floor"), IsValid(First));
	TestEqual(TEXT("No box before refill tick"), W.Count(), 0);
	W.TickStand(); TestEqual(TEXT("Next stand tick restores one box"), W.Count(), 1);
	TestTrue(TEXT("New source at start"), W.Stand->SourceBox->GetActorTransform().Equals(W.Start));
	W.TickStand(); TestEqual(TEXT("No duplicate after floor removal"), W.Count(), 1);
	return true;
}
#endif
