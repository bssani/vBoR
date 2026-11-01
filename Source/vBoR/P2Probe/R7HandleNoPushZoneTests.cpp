#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "P2bTestFixture.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "Misc/OutputDevice.h"
#include "Misc/OutputDeviceRedirector.h"

struct FR7TestAccess
{
	static FVBoRDoorMotion& Motion(AVBoRHingedPanel* Panel) { return Panel->Motion; }
	static void Angle(AVBoRHingedPanel* Panel, double Value)
	{
		Panel->Motion.AngleDeg = Value;
		Panel->ApplyState(0, Panel->Motion.Metrics.Unlocks, Panel->Motion.Metrics.Latches);
	}
};

namespace
{
constexpr float R7Dt = .1f;
constexpr int32 R7Near = static_cast<int32>(EVBoRDoorIgnore::NearHandle);

FVBoRDoorHand R7Hand(double X, double Y, double Angle = 0)
{
	FVBoRDoorHand Hand; Hand.bTracked = true;
	const FQuat Rotation(FVector::UpVector, FMath::DegreesToRadians(Angle));
	for (FVector& Point : Hand.Points) { Point = Rotation.RotateVector(FVector(X, Y, 50)); }
	Hand.PalmNormal = Rotation.RotateVector(FVector::YAxisVector);
	return Hand;
}

struct FR7World
{
	UWorld* World;
	AP2bTrackedDoor* Door;
	AVBoRAxisPart* Handle = nullptr;
	explicit FR7World(EWorldType::Type Type = EWorldType::Game)
	{
		World = UWorld::CreateWorld(Type, false);
		GEngine->CreateNewWorldContext(Type).SetCurrentWorld(World);
		Door = World->SpawnActor<AP2bTrackedDoor>();
	}
	void AddHandle(int32 Slot = 1, bool Attached = false)
	{
		Handle = World->SpawnActor<AVBoRAxisPart>();
		Handle->DoorHandleSide = Slot == 0 ? EVBoRDoorHandleSide::Outside : Slot == 1 ? EVBoRDoorHandleSide::Inside : EVBoRDoorHandleSide::Grip;
		Handle->MeshRestFromRoot = FTransform(FQuat::Identity, FVector::ZeroVector, FVector(.06));
		Handle->OnConstruction(Handle->GetActorTransform());
		Handle->SetActorLocation(FVector(80, -8, 50));
		if (Attached) { Handle->AttachToComponent(Door->GetRootComponent(), FAttachmentTransformRules::KeepWorldTransform); }
		else if (Slot == 0) { Door->OutsideAxisHandle = Handle; }
		else if (Slot == 1) { Door->InsideAxisHandle = Handle; }
		else { Door->GripAxisHandle = Handle; }
	}
	void Play(bool Locked = false)
	{
		World->InitializeActorsForPlay(FURL()); World->GetWorldSettings()->NotifyBeginPlay();
		if (!Locked) { Door->Unlock(); }
	}
	void Step(const FVBoRDoorHand& Left) { Door->Hands[0] = Left; Door->Tick(R7Dt); }
	double NearX() const { return 80 + Handle->GrabBox->GetScaledBoxExtent().X + 5; }
	void Cross(double X, double Angle = 0) { Step(R7Hand(X, -6, Angle)); Step(R7Hand(X, -3, Angle)); }
	~FR7World()
	{
		World->EndPlay(EEndPlayReason::Quit); World->DestroyWorld(false); GEngine->DestroyWorldContext(World);
	}
};

struct FR7SetupLogs : FOutputDevice
{
	int32 Warnings = 0;
	FR7SetupLogs() { GLog->AddOutputDevice(this); }
	~FR7SetupLogs() { GLog->RemoveOutputDevice(this); }
	virtual void Serialize(const TCHAR* Message, ELogVerbosity::Type Verbosity, const FName&) override
	{
		if (Verbosity == ELogVerbosity::Warning && FString(Message).Contains(TEXT("P2e setup:"))) { ++Warnings; }
	}
};
}

#define R7_TEST(N, Name) IMPLEMENT_SIMPLE_AUTOMATION_TEST(FR7E##N, "vBoR.R7.E" #N Name, EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter) bool FR7E##N::RunTest(const FString&)

R7_TEST(1, "NearInsideHandle")
{
	FR7World W; W.AddHandle(); W.Play();
	TestEqual(TEXT("Default margin is 8 cm"), W.Door->NoPushNearHandleCm, 8.f);
	W.Cross(W.NearX());
	for (int32 I = 0; I < 3; ++I) { W.Step(R7Hand(W.NearX(), -3)); }
	TestEqual(TEXT("Near handle stays closed"), W.Door->GetMotion().AngleDeg, 0.0);
	TestEqual(TEXT("One near-handle episode"), W.Door->GetMotion().Metrics.Ignored[R7Near], 1);
	TestEqual(TEXT("No push starts"), W.Door->GetMotion().Metrics.PushStarts[0][1], 0);
	W.Step(R7Hand(50, -6)); W.Cross(W.NearX());
	TestEqual(TEXT("Re-entry counts again"), W.Door->GetMotion().Metrics.Ignored[R7Near], 2);
	return true;
}

R7_TEST(2, "FarFromHandle")
{
	FR7World W; W.AddHandle(); W.Play(); W.Cross(50);
	TestTrue(TEXT("Thirty cm from handle opens"), W.Door->GetMotion().AngleDeg > 0);
	TestEqual(TEXT("One push start"), W.Door->GetMotion().Metrics.PushStarts[0][1], 1);
	TestEqual(TEXT("No near-handle rejection"), W.Door->GetMotion().Metrics.Ignored[R7Near], 0);
	return true;
}

R7_TEST(3, "AnyOfFivePoints")
{
	for (int32 Point = 0; Point < 5; ++Point)
	{
		FR7World W; W.AddHandle(); W.Play(); W.Step(R7Hand(50, -6));
		auto Hand = R7Hand(50, -3); Hand.Points[Point] = FVector(W.NearX(), -3, 50); W.Step(Hand);
		TestEqual(TEXT("One point blocks entire hand"), W.Door->GetMotion().AngleDeg, 0.0);
		TestEqual(TEXT("Near handle counted"), W.Door->GetMotion().Metrics.Ignored[R7Near], 1);
	}
	return true;
}

R7_TEST(4, "EndPushLikeWithdrawal")
{
	for (bool Coast : {false, true})
	{
		FR7World W; W.AddHandle(); W.Door->bCoastAfterRelease = Coast; W.Play(); W.Cross(50);
		TestEqual(TEXT("Push active before entering zone"), W.Door->GetMotion().PushHand, 0);
		const double Before = W.Door->GetMotion().AngleDeg;
		FVBoRDoorMotion Withdrawn = W.Door->GetMotion();
		FVBoRDoorHand Hands[2] = {R7Hand(200, 200), {}}; Withdrawn.Advance(Hands, R7Dt);
		W.Step(R7Hand(W.NearX(), -3, Before));
		const auto& State = W.Door->GetMotion();
		TestEqual(TEXT("Push ends in entering frame"), State.PushHand, INDEX_NONE);
		TestEqual(TEXT("Same angle as withdrawal"), State.AngleDeg, Withdrawn.AngleDeg);
		TestEqual(TEXT("Same momentum as withdrawal"), State.CoastSpeed, Withdrawn.CoastSpeed);
		TestEqual(TEXT("Same push travel as withdrawal"), State.Metrics.PushedDeg, Withdrawn.Metrics.PushedDeg);
		TestEqual(TEXT("No extra push start"), State.Metrics.PushStarts[0][1], 1);
		TestEqual(TEXT("No interaction color driver"), W.Door->IsSpectatorInteracting(), false);
		if (Coast) { TestTrue(TEXT("Coast continues"), State.AngleDeg > Before); }
		else { TestEqual(TEXT("Without coast stops"), State.AngleDeg, Before); }
	}
	return true;
}

R7_TEST(5, "GrabReleaseAndReenter")
{
	FR7World W; W.AddHandle(2); W.Door->bUseLatch = false; W.Play();
	auto& Motion = FR7TestAccess::Motion(W.Door);
	TestTrue(TEXT("Grip grab allowed inside zone"), Motion.BeginGrab(0, 2, R7Hand(80, -8), false));
	W.Step(R7Hand(80, -8, 20));
	W.Step(R7Hand(80, -8, 20));
	TestTrue(TEXT("Grab opens normally"), FMath::IsNearlyEqual(Motion.AngleDeg, 20.0, .001));
	TestTrue(TEXT("Tracked release allowed"), Motion.ReleaseGrab(true));
	const double Released = Motion.AngleDeg;
	W.Step(R7Hand(80, -8, Released)); W.Step(R7Hand(80, -3, Released));
	TestEqual(TEXT("Released hand cannot push"), Motion.AngleDeg, Released);
	TestEqual(TEXT("Released hand near handle counted"), Motion.Metrics.Ignored[R7Near], 1);
	W.Step(R7Hand(50, -6, Released)); W.Step(R7Hand(50, -3, Released));
	TestTrue(TEXT("Withdrawal then front crossing opens"), Motion.AngleDeg > Released);
	TestEqual(TEXT("Only far push starts"), Motion.Metrics.PushStarts[0][1], 1);
	return true;
}

R7_TEST(6, "LockedFirst")
{
	FR7World W; W.AddHandle(); W.Play(true); W.Cross(W.NearX());
	TestEqual(TEXT("Locked still closed"), W.Door->GetMotion().AngleDeg, 0.0);
	TestEqual(TEXT("Locked count first"), W.Door->GetMotion().Metrics.Ignored[static_cast<int32>(EVBoRDoorIgnore::Locked)], 1);
	TestEqual(TEXT("No near-handle count when locked"), W.Door->GetMotion().Metrics.Ignored[R7Near], 0);
	TestEqual(TEXT("Old reason numbers retained"), static_cast<int32>(EVBoRDoorIgnore::OpeningDisabled), 6);
	return true;
}

R7_TEST(7, "OtherHandPushes")
{
	FR7World W; W.AddHandle(); W.Play();
	W.Door->Hands[1] = R7Hand(50, -6); W.Step(R7Hand(W.NearX(), -6));
	W.Door->Hands[1] = R7Hand(50, -3); W.Step(R7Hand(W.NearX(), -3));
	TestTrue(TEXT("Far right hand opens"), W.Door->GetMotion().AngleDeg > 0);
	TestEqual(TEXT("Right owns push"), W.Door->GetMotion().PushHand, 1);
	TestEqual(TEXT("Right start counted"), W.Door->GetMotion().Metrics.PushStarts[1][1], 1);
	TestEqual(TEXT("Left rejected independently"), W.Door->GetMotion().Metrics.Ignored[R7Near], 1);
	return true;
}

R7_TEST(8, "MovingGripWorldBox")
{
	for (bool MeshGrab : {false, true})
	{
		FR7World W; W.AddHandle(2, true); W.Handle->bGrabWithMesh = MeshGrab;
		// Preserve plugin pivot warning policy while keeping handle bounds at the same world point
		W.Handle->MeshRestFromRoot.SetLocation(FVector(0, 0, 2)); W.Handle->OnConstruction(W.Handle->GetActorTransform());
		W.Door->SetActorTransform(FTransform(FRotator(12, 30, 8), FVector(200, 100, 70), FVector(1.2)));
		W.Play(); FR7TestAccess::Angle(W.Door, 50);
		TestTrue(TEXT("Grip follows hinge"), W.Handle->GetRootComponent()->IsAttachedTo(W.Door->HingePivot));
		if (MeshGrab) { TestEqual(TEXT("Mesh grab disables GrabBox collision"), W.Handle->GrabBox->GetCollisionEnabled(), ECollisionEnabled::NoCollision); }
		W.Cross(80, 50);
		TestEqual(TEXT("Rotated world zone blocks push"), W.Door->GetMotion().AngleDeg, 50.0);
		TestEqual(TEXT("Rotated grip counted"), W.Door->GetMotion().Metrics.Ignored[R7Near], 1);
		W.Cross(50, 50);
		TestTrue(TEXT("Away from rotated grip still pushes"), W.Door->GetMotion().AngleDeg > 50);
	}
	return true;
}

R7_TEST(9, "ZeroDisables")
{
	FR7World W; W.AddHandle(); W.Door->NoPushNearHandleCm = 0; W.Play(); W.Cross(W.NearX());
	TestTrue(TEXT("Zero restores original push"), W.Door->GetMotion().AngleDeg > 0);
	TestEqual(TEXT("No near-handle count"), W.Door->GetMotion().Metrics.Ignored[R7Near], 0);
	return true;
}

R7_TEST(10, "MissingHandleOrMesh")
{
	for (bool EmptyMesh : {false, true})
	{
		FR7World W; W.Door->bUseLatch = false;
		if (EmptyMesh)
		{
			W.AddHandle(); W.Handle->PartMesh->SetStaticMesh(nullptr);
			AddExpectedMessagePlain(TEXT("No part mesh; grabbing disabled"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 1);
			W.Handle->OnConstruction(W.Handle->GetActorTransform());
			TestEqual(TEXT("Missing mesh disables box"), W.Handle->GrabBox->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
		}
		W.Play(); W.Cross(80);
		TestTrue(TEXT("No usable handle leaves push unchanged"), W.Door->GetMotion().AngleDeg > 0);
		TestEqual(TEXT("No near-handle rejection"), W.Door->GetMotion().Metrics.Ignored[R7Near], 0);
	}
	return true;
}

R7_TEST(11, "EditorZonesAndNoWarnings")
{
#if WITH_EDITOR
	for (int32 Slot = 0; Slot < 3; ++Slot) for (bool Attached : {false, true})
	{
		FR7World W(EWorldType::EditorPreview); W.AddHandle(Slot, Attached);
		W.Door->MotionGuide = EVBoRMotionGuide::Off;
		W.Handle->GrabBox->SetRelativeRotation(FRotator(7, 19, 13));
		W.Handle->SetActorScale3D(FVector(-1.5, .7, 2));
		const FTransform Frame(W.Handle->GrabBox->GetComponentQuat(), W.Handle->GrabBox->GetComponentLocation());
		const FVector Extent = W.Handle->GrabBox->GetScaledBoxExtent().GetAbs() + FVector(8);
		TArray<FVector> Corners;
		for (int32 I = 0; I < 8; ++I) { Corners.Add(Frame.TransformPosition(FVector(I & 1 ? Extent.X : -Extent.X, I & 2 ? Extent.Y : -Extent.Y, I & 4 ? Extent.Z : -Extent.Z))); }
		W.Door->bUsePushFaceBox = true; W.Door->PushFaceBox->SetRelativeRotation(FRotator(0, 15, 0));
		FR7SetupLogs Logs;
		for (int32 I = 0; I < 3; ++I)
		{
			W.Door->MotionDisplay->Refresh();
			const auto Lines = W.Door->MotionDisplay->BuildShapes().FilterByPredicate([](const auto& L) { return L.Kind == EVBoRGuideShape::NearHandle; });
			TestEqual(TEXT("Twelve zone edges before Play"), Lines.Num(), 12);
			for (const auto& L : Lines) for (const FVector& P : {L.Start, L.End})
			{
				TestTrue(TEXT("Zone endpoint within 0.1 cm"), Corners.ContainsByPredicate([&](const FVector& C) { return C.Equals(P, .1); }));
			}
			for (const FVector& C : Corners)
			{
				TestTrue(TEXT("All zone corners represented"), Lines.ContainsByPredicate([&](const auto& L) { return L.Start.Equals(C, .1) || L.End.Equals(C, .1); }));
			}
		}
		W.Door->JudgmentGuide = EVBoRMotionGuide::Off;
		TestEqual(TEXT("Off hides all judgment shapes"), W.Door->MotionDisplay->BuildShapes().Num(), 0);
		GLog->FlushThreadedLogs(); TestEqual(TEXT("Display adds no setup warnings"), Logs.Warnings, 0);
	}
#endif
	return true;
}

R7_TEST(12, "MetricsText")
{
	FR7World W; W.AddHandle(); W.Play(); W.Cross(W.NearX());
	const FVBoRDoorMetrics Baseline = W.Door->GetMotion().Metrics;
	const FString Text = W.Door->GetMeasurementText();
	TestTrue(TEXT("Ignored contacts includes near handle count"), Text.Contains(TEXT("Ignored contacts:")) && Text.Contains(TEXT("near handle=1")));
	TestTrue(TEXT("Baseline subtracts new reason"), W.Door->GetMeasurementText(&Baseline).Contains(TEXT("near handle=0")));
	return true;
}

#undef R7_TEST
#endif
