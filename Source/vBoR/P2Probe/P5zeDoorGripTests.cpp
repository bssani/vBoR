#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "P5zeTestFixture.h"
#include "Components/BoxComponent.h"
#include "Components/ChildActorComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "Interaction/IsdkGrabberComponent.h"
#include "Interaction/IsdkGrabbableComponent.h"
#include "IsdkRuntimeSettings.h"

struct FP5zeTestAccess
{
	static void Angle(AVBoRHingedPanel* Door, double Angle)
	{
		Door->Motion.AngleDeg = Angle; Door->Motion.bLatchArmed = Angle > Door->RearmAngleDeg;
		Door->ApplyState(0, Door->Motion.Metrics.Unlocks, Door->Motion.Metrics.Latches);
	}
	static int32 HoverCount(const AVBoRAxisPart* Part) { return Part->Hovering.Num(); }
	static double Value(const AVBoRAxisPart* Part) { return Part->Value; }
	static void LeverOpen(AVBoRAxisPart* Part) { Part->AcceptTransform(FTransform(FQuat(FVector::XAxisVector, FMath::DegreesToRadians(30.0)))); }
	static AVBoRHingedPanel* Linked(const AVBoRAxisPart* Part) { return Part->LinkedDoor; }
	static int32 Problems(const AVBoRHingedPanel* Door) { return Door->EvaluateSetup().Problems.Num(); }
};

namespace
{
	constexpr float GripDt = 1.f / 90;
	FVBoRDoorHand GripHand(double Angle)
	{
		FVBoRDoorHand Input; Input.bTracked = true;
		for (FVector& P : Input.Points) { P = FQuat(FVector::UpVector, FMath::DegreesToRadians(Angle)).RotateVector(FVector(80,-8,50)); }
		return Input;
	}
	struct FGripWorld
	{
		UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
		AP2bTrackedDoor* Door = World->SpawnActor<AP2bTrackedDoor>();
		AP5zeTrackedPart* Lever = Part(EVBoRDoorHandleSide::Inside, FVector(80,8,50));
		AP5zeTrackedPart* Grip = Part(EVBoRDoorHandleSide::Grip, FVector(80,-8,50));
		UIsdkGrabberComponent* Grabbers[2] = {};
		AP5zeTrackedPart* Part(EVBoRDoorHandleSide Side, FVector Position)
		{
			auto* P = World->SpawnActor<AP5zeTrackedPart>(); P->DoorHandleSide = Side;
			P->MeshRestFromRoot = FTransform(FQuat::Identity, FVector::ZeroVector, FVector(0.06));
			P->OnConstruction(P->GetActorTransform()); P->SetActorLocation(Position);
			P->bAllowPinchGrab = true;
			P->AttachToComponent(Door->GetRootComponent(), FAttachmentTransformRules::KeepWorldTransform);
			return P;
		}
		void Play(double Angle = 40)
		{
			for (int32 H = 0; H < 2; ++H)
			{
				auto* Actor = World->SpawnActor<AActor>(); auto* G = NewObject<UIsdkGrabberComponent>(Actor);
				Actor->SetRootComponent(G); Grabbers[H] = G;
				G->InteractorTags.AddTag(H == 0 ? IsdkGameplayTags::TAG_Isdk_Type_Device_Left : IsdkGameplayTags::TAG_Isdk_Type_Device_Right);
				G->SetWorldLocation(FVector(1000));
				G->SetGrabDetectionTypeAllowed(EIsdkGrabDetectorType::DistanceGrab, false);
				G->SetGrabDetectionTypeAllowed(EIsdkGrabDetectorType::RayGrab, false);
				G->SetGrabInputMethodAllowed(EIsdkGrabInputMethod::Palm, true);
				G->SetGrabInputMethodAllowed(EIsdkGrabInputMethod::Pinch, false); G->RegisterComponent();
			}
			World->InitializeActorsForPlay(FURL()); World->GetWorldSettings()->NotifyBeginPlay();
			for (auto* G : Grabbers) { G->Activate(true); }
			if (Angle > 0) { Door->Unlock(); FP5zeTestAccess::Angle(Door, Angle); }
			Door->Hands[0] = Door->Hands[1] = GripHand(Angle);
		}
		bool Grab(AP5zeTrackedPart* P, int32 H = 0, bool Pinch = false)
		{
			auto* G = Grabbers[H];
			G->SetGrabInputMethodAllowed(EIsdkGrabInputMethod::Palm, !Pinch);
			G->SetGrabInputMethodAllowed(EIsdkGrabInputMethod::Pinch, Pinch);
			G->SetWorldLocation(P->GrabBox->GetComponentLocation()); G->TickComponent(GripDt, LEVELTICK_All, nullptr);
			if (Pinch) { G->PinchGrab(); } else { G->PalmGrab(); }
			return P->GetHoldingHand() == H;
		}
		void Step() { Lever->Tick(GripDt); Grip->Tick(GripDt); Door->Tick(GripDt); }
		void Move(double Bearing, int32 H = 0, int32 Frames = 20)
		{
			Door->Hands[H] = GripHand(Bearing);
			for (int32 I = 0; I < Frames; ++I) { Step(); }
		}
		~FGripWorld() { World->EndPlay(EEndPlayReason::Quit); World->DestroyWorld(false); }
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5zeClose, "vBoR.P5ze.E1Close", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5zeClose::RunTest(const FString& Parameters)
{
	FGripWorld W; W.Play(); const int32 Unlocks = W.Door->GetMotion().Metrics.Unlocks;
	TestTrue(TEXT("SDK palm selects grip"), W.Grab(W.Grip)); W.Step();
	TestEqual(TEXT("Immediate door grab without lever travel"), W.Door->GetMotion().GrabHand, 0);
	W.Move(20); TestTrue(TEXT("Pull follows hand to 20 deg"), FMath::IsNearlyEqual(W.Door->GetMotion().AngleDeg, 20.0, 0.001));
	TestEqual(TEXT("Grip never unlocks"), W.Door->GetMotion().Metrics.Unlocks, Unlocks); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5zeOpen, "vBoR.P5ze.E2Open", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5zeOpen::RunTest(const FString& Parameters)
{
	FGripWorld W; W.Play(); TestTrue(TEXT("SDK pinch selects grip"), W.Grab(W.Grip, 0, true)); W.Step();
	W.Move(60); TestTrue(TEXT("Push follows hand to 60 deg"), FMath::IsNearlyEqual(W.Door->GetMotion().AngleDeg, 60.0, 0.001));
	W.Move(100, 0, 30); TestEqual(TEXT("Stops at 70 deg"), W.Door->GetMotion().AngleDeg, 70.0); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5zeLocked, "vBoR.P5ze.E3Locked", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5zeLocked::RunTest(const FString& Parameters)
{
	FGripWorld W; W.Play(0);
	for (bool Pinch : {false, true})
	{
		TestFalse(TEXT("Locked grip rejects SDK selection"), W.Grab(W.Grip, 0, Pinch));
		TestEqual(TEXT("Locked grip has no hover"), FP5zeTestAccess::HoverCount(W.Grip), 0);
		TestFalse(TEXT("SDK input disabled"), W.Grip->Grabbable->IsGrabInputMethodAllowed(Pinch ? EIsdkGrabInputMethod::Pinch : EIsdkGrabInputMethod::Palm));
		W.Move(30); W.Move(-30);
	}
	const auto& M = W.Door->GetMotion();
	TestTrue(TEXT("Still locked"), M.bLocked); TestEqual(TEXT("Angle stays zero"), M.AngleDeg, 0.0);
	TestEqual(TEXT("No unlock"), M.Metrics.Unlocks, 0); TestEqual(TEXT("No door owner"), M.GrabHand, INDEX_NONE);
	TestEqual(TEXT("No grip count"), M.Metrics.GripGrabs[0], 0);
	FVBoRDoorMotion Motion;
	TestFalse(TEXT("Motion also rejects locked grip"), Motion.BeginGrab(0, 2, GripHand(0), false)); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5zeTransfer, "vBoR.P5ze.E4Transfer", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5zeTransfer::RunTest(const FString& Parameters)
{
	FGripWorld W; W.Play(0); TestFalse(TEXT("Grip blocked before lever"), W.Grab(W.Grip, 1));
	TestTrue(TEXT("SDK selects inside lever"), W.Grab(W.Lever)); FP5zeTestAccess::LeverOpen(W.Lever); W.Step();
	TestEqual(TEXT("Lever unlocks and owns door"), W.Door->GetMotion().GrabHand, 0);
	W.Move(10); TestTrue(TEXT("Lever opens to 10 deg"), FMath::IsNearlyEqual(W.Door->GetMotion().AngleDeg, 10.0, 0.001));
	W.Door->Hands[1] = GripHand(10); TestTrue(TEXT("Other hand can select unlocked grip"), W.Grab(W.Grip, 1)); W.Step();
	W.Door->Hands[1].Points[0] += FVector(0,5,0); W.Step();
	TestEqual(TEXT("Lever retains ownership"), W.Door->GetMotion().GrabHand, 0);
	const double Before = W.Door->GetMotion().AngleDeg; W.Door->bPalmClosed[0] = false; W.Step(); W.Step();
	TestEqual(TEXT("Grip takes over next frame"), W.Door->GetMotion().GrabHand, 1);
	TestEqual(TEXT("Waiting hand motion discarded"), W.Door->GetMotion().AngleDeg, Before); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5zeRelease, "vBoR.P5ze.E5Release", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5zeRelease::RunTest(const FString& Parameters)
{
	for (bool Coast : {false, true})
	{
		FGripWorld W; W.Door->bCoastAfterRelease = Coast; W.Play(); TestTrue(TEXT("Select grip"), W.Grab(W.Grip)); W.Step();
		for (int32 I = 1; I <= 15; ++I) { W.Move(40 + I * 0.5, 0, 1); }
		W.Door->bPalmClosed[0] = false; W.Step(); const double Released = W.Door->GetMotion().AngleDeg;
		TestEqual(TEXT("Opening palm releases owner"), W.Door->GetMotion().GrabHand, INDEX_NONE);
		TestEqual(TEXT("Grip releases with door"), W.Grip->GetHoldingHand(), INDEX_NONE);
		W.Step();
		if (Coast) { TestTrue(TEXT("Grip release coasts"), W.Door->GetMotion().AngleDeg > Released); }
		else { TestEqual(TEXT("Default release stops"), W.Door->GetMotion().AngleDeg, Released); }
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5zeTracking, "vBoR.P5ze.E6Tracking", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5zeTracking::RunTest(const FString& Parameters)
{
	FGripWorld W; W.Play(); TestTrue(TEXT("Select grip"), W.Grab(W.Grip)); W.Step(); W.Move(30);
	const double Before = W.Door->GetMotion().AngleDeg; W.Door->Hands[0].bTracked = W.Grip->bTracked = false;
	for (int32 I = 0; I < 3; ++I) { W.Step(); }
	TestEqual(TEXT("Tracking loss retains angle"), W.Door->GetMotion().AngleDeg, Before);
	TestEqual(TEXT("Tracking loss retains grip"), W.Grip->GetHoldingHand(), 0);
	W.Grip->bTracked = true; W.Move(60, 0, 1);
	TestEqual(TEXT("Recovery discards accumulated motion"), W.Door->GetMotion().AngleDeg, Before);
	W.Move(65); TestTrue(TEXT("Movement resumes from recovered point"), FMath::IsNearlyEqual(W.Door->GetMotion().AngleDeg, Before + 5, 0.001)); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5zeLatch, "vBoR.P5ze.E7Latch", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5zeLatch::RunTest(const FString& Parameters)
{
	FGripWorld W; W.Play(6); TestTrue(TEXT("Select grip"), W.Grab(W.Grip)); W.Step();
	for (int32 I = 0; I < 3 && !W.Door->GetMotion().bLocked; ++I) { W.Move(1, 0, 1); }
	TestTrue(TEXT("Latched"), W.Door->GetMotion().bLocked); TestEqual(TEXT("One latch"), W.Door->GetMotion().Metrics.Latches, 1);
	TestEqual(TEXT("Door ownership ends in latch frame"), W.Door->GetMotion().GrabHand, INDEX_NONE);
	TestEqual(TEXT("Grip ownership ends in latch frame"), W.Grip->GetHoldingHand(), INDEX_NONE);
	TestFalse(TEXT("Locked grip cannot be reselected"), W.Grab(W.Grip));
	// Finish deferred latch cancellation before the next SDK Select (P2b R1-3)
	W.Grip->bClosed[0] = W.Door->bPalmClosed[0] = false; W.Step(); W.Grabbers[0]->PalmRelease();
	TestTrue(TEXT("Lever selectable after latch"), W.Grab(W.Lever, 1)); FP5zeTestAccess::LeverOpen(W.Lever); W.Step();
	TestFalse(TEXT("Lever unlocks again"), W.Door->GetMotion().bLocked);
	W.Grip->bClosed[0] = W.Door->bPalmClosed[0] = true;
	TestTrue(TEXT("Grip selectable after lever unlock"), W.Grab(W.Grip)); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5zeFixed, "vBoR.P5ze.E8Fixed", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5zeFixed::RunTest(const FString& Parameters)
{
	FGripWorld W; W.Grip->Release = EVBoRAxisRelease::Detent; W.Grip->bAllowPush = true; W.Play();
	auto* Observer = NewObject<UP5zePartObserver>(W.World);
	W.Grip->OnPartOpened.AddDynamic(Observer, &UP5zePartObserver::Opened); W.Grip->OnPartClosed.AddDynamic(Observer, &UP5zePartObserver::Closed);
	W.Grip->OnDetentChanged.AddDynamic(Observer, &UP5zePartObserver::Detent); W.Grip->OnDetentNudge.AddDynamic(Observer, &UP5zePartObserver::Nudge);
	const FTransform Rest = W.Grip->GetActorTransform().GetRelativeTransform(W.Door->HingePivot->GetComponentTransform());
	TestTrue(TEXT("Select grip"), W.Grab(W.Grip)); W.Step(); W.Move(20); W.Move(60);
	TestEqual(TEXT("Grip axis stays zero"), FP5zeTestAccess::Value(W.Grip), 0.0);
	TestTrue(TEXT("Grip retains hinge-relative pose"), W.Grip->GetActorTransform().GetRelativeTransform(W.Door->HingePivot->GetComponentTransform()).Equals(Rest));
	TestEqual(TEXT("No opened event"), Observer->Opens, 0); TestEqual(TEXT("No closed event"), Observer->Closes, 0);
	TestEqual(TEXT("No detents"), Observer->Detents, 0); TestEqual(TEXT("No nudges"), Observer->Nudges, 0);
	for (int32 Slot = 0; Slot < 4; ++Slot) { TestEqual(TEXT("No part sound requests"), W.Grip->GetSoundPlaybackForTest(Slot).Requests, 0); }
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5zeMetrics, "vBoR.P5ze.E9Metrics", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5zeMetrics::RunTest(const FString& Parameters)
{
	FGripWorld W; W.Play(); const FVBoRDoorMetrics Before = W.Door->GetMotion().Metrics;
	const FString Text = W.Door->GetMeasurementText(); TestTrue(TEXT("Select grip"), W.Grab(W.Grip, 1)); W.Grip->Tick(GripDt);
	const auto Delta = W.Door->GetMotion().Metrics.Since(Before);
	TestEqual(TEXT("Right grip count"), Delta.GripGrabs[1], 1); TestEqual(TEXT("Left grip count"), Delta.GripGrabs[0], 0);
	for (int32 H = 0; H < 2; ++H) { for (int32 S = 0; S < 2; ++S) { TestEqual(TEXT("Lever counts untouched"), Delta.Grabs[H][S], 0); TestEqual(TEXT("Lever attempts untouched"), Delta.Attempts[H][S], 0); } }
	TestEqual(TEXT("Measurement text unchanged by grip count"), W.Door->GetMeasurementText(), Text); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5zeSetup, "vBoR.P5ze.E10Setup", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5zeSetup::RunTest(const FString& Parameters)
{
	{
		FGripWorld W; TestEqual(TEXT("Inside plus grip has no conflict"), FP5zeTestAccess::Problems(W.Door), 0); W.Play();
		TestTrue(TEXT("Single grip auto connects"), W.Door->GripAxisHandle == W.Grip);
		TestTrue(TEXT("Inside slot remains lever"), W.Door->InsideAxisHandle == W.Lever);
		TestTrue(TEXT("Grip attached to hinge"), W.Grip->GetRootComponent()->IsAttachedTo(W.Door->HingePivot));
	}
	{
		FGripWorld W; W.Part(EVBoRDoorHandleSide::Grip, FVector(40,-8,50));
		TestEqual(TEXT("Duplicate grip has one setup issue"), FP5zeTestAccess::Problems(W.Door), 1);
		AddExpectedMessage(TEXT("grip has multiple attached candidates"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 1);
		W.Play(); TestNull(TEXT("Ambiguous grip not connected"), W.Door->GripAxisHandle.Get());
	}
	{
		FGripWorld W; W.Part(EVBoRDoorHandleSide::Grip, FVector(40,-8,50)); W.Door->GripAxisHandle = W.Grip;
		AddExpectedMessage(TEXT("grip slot"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 1);
		W.Play(); TestTrue(TEXT("Explicit grip wins"), W.Door->GripAxisHandle == W.Grip);
		auto* Other = W.World->SpawnActor<AVBoRHingedPanel>();
		AddExpectedMessage(TEXT("shared handle"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 1);
		Other->SetAxisHandle(2, W.Grip); TestNull(TEXT("Other panel cannot steal grip"), Other->GripAxisHandle.Get());
		TestTrue(TEXT("First connection retained"), FP5zeTestAccess::Linked(W.Grip) == W.Door);
	}
	{
		FGripWorld W; auto* Child = NewObject<UChildActorComponent>(W.Door); Child->SetupAttachment(W.Door->GetRootComponent());
		Child->SetChildActorClass(AVBoRAxisPart::StaticClass()); Child->RegisterComponent();
		auto* P = CastChecked<AVBoRAxisPart>(Child->GetChildActor()); P->DoorHandleSide = EVBoRDoorHandleSide::Grip;
		W.Grip->DoorHandleSide = EVBoRDoorHandleSide::None;
		TestEqual(TEXT("Child outside hinge rejected"), FP5zeTestAccess::Problems(W.Door), 1);
		AddExpectedMessage(TEXT("move its Child Actor component under HingePivot"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 1);
		W.Play(); TestNull(TEXT("Bad child stays unlinked"), W.Door->GripAxisHandle.Get());
	}
	return true;
}
#endif
