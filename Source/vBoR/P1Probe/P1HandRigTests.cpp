#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "../Interaction/VBoRHandRigPawn.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "Interaction/IsdkGrabberComponent.h"
#include "Rig/IsdkGrabInteractionRigComponent.h"
#include "Rig/IsdkRayInteractionRigComponent.h"
#include "StructTypesNonGenerated.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP1HandRigDefaultTest, "vBoR.P1Probe.HandRig.DefaultPokeOnly", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP1HandRigDefaultTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	AVBoRHandRigPawn* Pawn = World->SpawnActor<AVBoRHandRigPawn>();
	World->InitializeActorsForPlay(FURL());
	World->GetWorldSettings()->NotifyBeginPlay();
	TestTrue(TEXT("Default pawn reached BeginPlay"), Pawn->HasActorBegunPlay());
	TestFalse(TEXT("Palm grabbing is opt-in"), Pawn->bAllowPalmGrab);
	for (int32 Hand = 0; Hand < 2; ++Hand)
	{
		bool bClosed = false;
		TestTrue(TEXT("Default rig palm recognizer available after BeginPlay"),
			Hand == 0 ? Pawn->LeftRig->ReadPalmGrab(bClosed) : Pawn->RightRig->ReadPalmGrab(bClosed));
		UIsdkHandRigComponent* Rig = Pawn->GetHandRig(Hand);
		TestFalse(TEXT("Default grab rig remains inactive"), Rig->GetGrabInteraction()->IsActive());
		TestFalse(TEXT("Default ray rig remains inactive"), Rig->GetRayInteraction()->IsActive());
		UIsdkGrabberComponent* Grabber = Rig->GetGrabber();
		TestFalse(TEXT("Default distance grab remains blocked"), Grabber->IsGrabDetectionTypeAllowed(EIsdkGrabDetectorType::DistanceGrab));
		TestFalse(TEXT("Default ray grab remains blocked"), Grabber->IsGrabDetectionTypeAllowed(EIsdkGrabDetectorType::RayGrab));
	}
	World->EndPlay(EEndPlayReason::Quit);
	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP1HandRigPalmTest, "vBoR.P1Probe.HandRig.PalmOptIn", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP1HandRigPalmTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	AVBoRHandRigPawn* Pawn = World->SpawnActor<AVBoRHandRigPawn>();
	Pawn->bAllowPalmGrab = true;
	World->InitializeActorsForPlay(FURL());
	World->GetWorldSettings()->NotifyBeginPlay();
	TestTrue(TEXT("Opted-in pawn reached BeginPlay"), Pawn->HasActorBegunPlay());
	for (int32 Hand = 0; Hand < 2; ++Hand)
	{
		bool bClosed = false;
		TestTrue(TEXT("Opted-in rig palm recognizer available after BeginPlay"),
			Hand == 0 ? Pawn->LeftRig->ReadPalmGrab(bClosed) : Pawn->RightRig->ReadPalmGrab(bClosed));
		UIsdkHandRigComponent* Rig = Pawn->GetHandRig(Hand);
		TestTrue(TEXT("Opted-in grab rig active at BeginPlay"), Rig->GetGrabInteraction()->IsActive());
		TestFalse(TEXT("Ray rig remains inactive"), Rig->GetRayInteraction()->IsActive());
		UIsdkGrabberComponent* Grabber = Rig->GetGrabber();
		TestTrue(TEXT("Nearby hand grab allowed"), Grabber->IsGrabDetectionTypeAllowed(EIsdkGrabDetectorType::HandGrab));
		TestFalse(TEXT("Distance grab blocked"), Grabber->IsGrabDetectionTypeAllowed(EIsdkGrabDetectorType::DistanceGrab));
		TestFalse(TEXT("Ray grab blocked"), Grabber->IsGrabDetectionTypeAllowed(EIsdkGrabDetectorType::RayGrab));
		TestTrue(TEXT("Palm input allowed"), Grabber->IsGrabInputMethodAllowed(EIsdkGrabInputMethod::Palm));
		TestFalse(TEXT("Pinch input blocked"), Grabber->IsGrabInputMethodAllowed(EIsdkGrabInputMethod::Pinch));
	}
	const FString RigStatus = Pawn->DescribeRig();
	TestTrue(TEXT("Status shows palm opt-in"), RigStatus.Contains(TEXT("allow palm grab=1")));
	TestTrue(TEXT("Status shows palm on and pinch off"), RigStatus.Contains(TEXT("palm allowed=1, pinch allowed=0")));
	World->EndPlay(EEndPlayReason::Quit);
	World->DestroyWorld(false);
	return true;
}
#endif
