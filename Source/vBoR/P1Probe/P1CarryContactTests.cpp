#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "../Interaction/VBoRCarryObject.h"
#include "../Interaction/VBoRLoadTarget.h"
#include "../Interaction/VBoRHandRigPawn.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/WorldSettings.h"
#include "Misc/OutputDevice.h"
#include "Misc/OutputDeviceRedirector.h"
#include "P1CarryTestObserver.h"
#include "P1LoadBench.h"
#include "P1TrunkMock.h"

namespace
{
	struct FContactWorld
	{
		UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
		~FContactWorld() { World->EndPlay(EEndPlayReason::Quit); World->DestroyWorld(false); }
		void BeginPlay() { World->InitializeActorsForPlay(FURL()); World->GetWorldSettings()->NotifyBeginPlay(); }
		AActor* Wall(const FVector& Center, const FVector& Half)
		{
			AActor* Actor = World->SpawnActor<AActor>();
			UBoxComponent* Box = NewObject<UBoxComponent>(Actor);
			Actor->SetRootComponent(Box);
			Box->SetBoxExtent(Half);
			Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
			Box->SetCollisionResponseToAllChannels(ECR_Block);
			Box->RegisterComponent();
			Actor->SetActorLocation(Center);
			return Actor;
		}
		AVBoRCarryObject* Bag(const FVector& Center)
		{
			AVBoRCarryObject* Object = World->SpawnActor<AVBoRCarryObject>();
			Object->Mesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder")));
			Object->SetActorTransform(FTransform(FRotator(0, 0, 90), Center, FVector(0.28, 0.28, 1.2)));
			return Object;
		}
	};
	struct FAssistanceWarnings : FOutputDevice
	{
		int32 Count = 0;
		FAssistanceWarnings() { GLog->AddOutputDevice(this); }
		~FAssistanceWarnings() { GLog->RemoveOutputDevice(this); }
		int32 GetCount() { GLog->FlushThreadedLogs(); return Count; }
		virtual void Serialize(const TCHAR* Message, ELogVerbosity::Type Verbosity, const FName& Category) override
		{
			if (Category == TEXT("LogVBoRCarry") && FString(Message).Contains(TEXT("Assisted seating finished outside loading region"))) { ++Count; }
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP1CarryDefaultAssistanceTest, "vBoR.P1Carry.DefaultGoalAssistance", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP1CarryDefaultAssistanceTest::RunTest(const FString& Parameters)
{
	FContactWorld Fixture;
	FAssistanceWarnings Warnings;
	AVBoRLoadTarget* Target = Fixture.World->SpawnActor<AVBoRLoadTarget>();
	Target->SetActorLocation(FVector(0, 0, 200));
	AVBoRCarryObject* Bag = Fixture.Bag(FVector(0, 0, 205));
	Bag->SetActorRotation(FQuat::Identity);
	Bag->LoadTarget = Target;
	Fixture.BeginPlay();
	UP1CarryTestObserver* Observer = NewObject<UP1CarryTestObserver>(Fixture.World);
	Bag->OnSeated.AddDynamic(Observer, &UP1CarryTestObserver::OnSeated);
	Bag->BeginCarry(0, nullptr, Bag->GetActorTransform());
	Bag->ReleaseCarry();
	TestTrue(TEXT("Default goal starts assistance for 120 cm cylinder"), Bag->bAssisting);
	Bag->AssistStartedAt = Fixture.World->GetTimeSeconds() - 0.3;
	Bag->Tick(0.1);
	TestEqual(TEXT("Default goal finishes seated"), Bag->GetStateText(), FString(TEXT("Seated")));
	TestTrue(TEXT("Default goal records assistance"), Bag->GetMetrics().Outcome == EVBoRCarryOutcome::Assisted);
	TestEqual(TEXT("Default goal emits seated once"), Observer->SeatedCount, 1);
	TestEqual(TEXT("Default goal has no configuration warning"), Warnings.GetCount(), 0);
	Target->LoadingRegion->SetBoxExtent(FVector(10));
	FTransform NearGoal = Bag->GetActorTransform();
	NearGoal.AddToTranslation(FVector(0, 0, 5));
	Bag->ResetToPose(NearGoal);
	Bag->BeginCarry(0, nullptr, Bag->GetActorTransform());
	Bag->ReleaseCarry();
	AddExpectedMessagePlain(TEXT("Assisted seating finished outside loading region"), ELogVerbosity::Warning);
	Bag->AssistStartedAt = Fixture.World->GetTimeSeconds() - 0.3;
	Bag->Tick(0.1);
	TestEqual(TEXT("Small region keeps outside state"), Bag->GetStateText(), FString(TEXT("Released (outside target)")));
	TestEqual(TEXT("Small region emits warning once"), Warnings.GetCount(), 1);
	TestEqual(TEXT("Small region cannot emit seated"), Observer->SeatedCount, 1);
	Bag->Tick(0.1);
	TestEqual(TEXT("Rest cannot repeat configuration warning"), Warnings.GetCount(), 1);
	TestTrue(TEXT("Explicit small region is not expanded"), Target->LoadingRegion->GetUnscaledBoxExtent().Equals(FVector(10)));
	APlayerController* Controller = Fixture.World->SpawnActor<APlayerController>();
	Controller->Possess(Fixture.World->SpawnActor<AVBoRHandRigPawn>());
	AP1LoadBench* Bench = Fixture.World->SpawnActor<AP1LoadBench>();
	Bench->PlaceFromHead(FTransform(FVector(500, 0, 165)));
	Bench->ResetBagForNextTrial();
	Bench->PlaceFromHead(FTransform(FVector(500, 0, 165)));
	TestEqual(TEXT("Bench placement R and B add no configuration warnings"), Warnings.GetCount(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP1CarryContactSlideTest, "vBoR.P1Carry.ContactSlide", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP1CarryContactSlideTest::RunTest(const FString& Parameters)
{
	FContactWorld Fixture;
	Fixture.Wall(FVector(0, 0, 100), FVector(5, 100, 100));
	AVBoRCarryObject* Object = Fixture.World->SpawnActor<AVBoRCarryObject>();
	Object->SetActorTransform(FTransform(FQuat::Identity, FVector(-15, 0, 110), FVector(0.2)));
	Object->Session.SetRestPose(Object->GetActorTransform());
	Object->Session.Begin(0, Object->GetActorTransform(), Object->GetActorTransform(), 0);
	FTransform Desired = Object->GetActorTransform();
	Desired.AddToTranslation(FVector(1, 1, 0));
	Object->MoveHeldPose(Desired);
	TestTrue(TEXT("45 degree push keeps requested tangent movement"), FMath::IsNearlyEqual(Object->GetActorLocation().Y, 1.0, 0.1));
	TestTrue(TEXT("45 degree push cannot cross wall"), FMath::IsNearlyEqual(Object->GetActorLocation().X, -15.0, 0.01));
	TestTrue(TEXT("Slide pose is clear"), Object->IsPoseClear(Object->GetActorTransform()));
	TestFalse(TEXT("Small normal error is contact not blockage"), Object->IsBlocked());
	TestEqual(TEXT("Sliding object stays held"), Object->GetStateText(), FString(TEXT("Held (one hand)")));
	FContactWorld Corner;
	Corner.Wall(FVector(0, 0, 50), FVector(5, 100, 50));
	Corner.Wall(FVector(0, 0, -5), FVector(100, 100, 5));
	AVBoRCarryObject* Box = Corner.World->SpawnActor<AVBoRCarryObject>();
	Box->SetActorTransform(FTransform(FQuat::Identity, FVector(-15, 0, 10), FVector(0.2)));
	Box->Session.SetRestPose(Box->GetActorTransform());
	Desired = Box->GetActorTransform();
	Desired.AddToTranslation(FVector(5, 20, -5));
	Box->MoveHeldPose(Desired);
	TestTrue(TEXT("Two walls keep intersection-line movement"), FMath::IsNearlyEqual(Box->GetActorLocation().Y, 20.0, 0.1));
	TestTrue(TEXT("Two walls reject both inward components"), FMath::IsNearlyEqual(Box->GetActorLocation().X, -15.0, 0.01) && FMath::IsNearlyEqual(Box->GetActorLocation().Z, 10.0, 0.01));
	TestTrue(TEXT("Corner pose clears both walls"), Box->IsPoseClear(Box->GetActorTransform()));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP1CarryContactRotationTest, "vBoR.P1Carry.ContactRotation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP1CarryContactRotationTest::RunTest(const FString& Parameters)
{
	FContactWorld Fixture;
	Fixture.Wall(FVector(50, 70, 70), FVector(50, 5, 70));
	AVBoRCarryObject* Bag = Fixture.Bag(FVector(14, 5, 40));
	const FTransform Before = Bag->GetActorTransform();
	Bag->Session.SetRestPose(Before);
	FTransform Desired = Before;
	Desired.SetRotation(FRotator(0, -10, 0).Quaternion() * Before.GetRotation());
	Bag->MoveHeldPose(Desired);
	const double Yaw = FVBoRCarryMotion::AngleDeg(Before, Bag->GetActorTransform());
	TestTrue(TEXT("Contact applies some requested yaw"), Yaw > 0.01);
	TestTrue(TEXT("Pivot pose is clear"), Bag->IsPoseClear(Bag->GetActorTransform()));
	TestTrue(TEXT("Contact corner stays on inside of wall"), Bag->GetActorTransform().TransformPosition(FVector(-50, 0, 50)).Y <= 65.01);
	TestTrue(TEXT("Opposite end follows requested direction"), Bag->GetActorTransform().TransformPosition(FVector(0, 0, -50)).X < Before.TransformPosition(FVector(0, 0, -50)).X);
	TestTrue(TEXT("Large rotation is reduced rather than applied whole"), Yaw < 9.99);
	TestTrue(TEXT("Rotation push stays within per-frame cap"), FVector::Distance(Before.GetLocation(), Bag->GetActorLocation()) <= Bag->MaxRotationPushCm + 1e-6);
	TestTrue(TEXT("Contact rotation preserves scale"), Bag->GetActorScale3D().Equals(Before.GetScale3D()));
	const FTransform First = Bag->GetActorTransform();
	Bag->MoveHeldPose(Desired);
	TestTrue(TEXT("Same requested pose cannot creep at contact"), Bag->GetActorTransform().Equals(First, 1e-6));
	FContactWorld Floor;
	Floor.Wall(FVector(0, 0, -5), FVector(300, 300, 5));
	AVBoRCarryObject* Resting = Floor.Bag(FVector(0, 0, 14));
	const FTransform Rest = Resting->GetActorTransform();
	Resting->Session.SetRestPose(Rest);
	Resting->Session.Begin(0, Rest, Rest, 0);
	Desired = Rest;
	Desired.AddToTranslation(FVector(0, 0, -1));
	Desired.SetRotation(FQuat(FVector::ForwardVector, FMath::DegreesToRadians(3.0)) * Rest.GetRotation());
	Resting->MoveHeldPose(Desired);
	TestTrue(TEXT("Regrab jitter leaves floor pose clear"), Resting->IsPoseClear(Resting->GetActorTransform()));
	TestFalse(TEXT("Small downward and tilt request remains held"), Resting->IsBlocked());
	TestTrue(TEXT("Floor contact allows partial tilt"), FVBoRCarryMotion::AngleDeg(Rest, Resting->GetActorTransform()) > 0.01);
	TestTrue(TEXT("Floor rotation push stays within cap"), FVector::Distance(Rest.GetLocation(), Resting->GetActorLocation()) <= Resting->MaxRotationPushCm + 1e-6);
	Resting->Session.bPaused = true;
	const FTransform Paused = Resting->GetActorTransform();
	Desired.AddToTranslation(FVector(100, 0, 100));
	Resting->MoveHeldPose(Desired);
	TestTrue(TEXT("Paused contact response cannot move"), Resting->GetActorTransform().Equals(Paused));
	Resting->ResetToPose(Rest);
	Resting->BeginCarry(0, nullptr, Rest);
	Desired = Rest;
	Desired.SetRotation(FQuat(FVector::ForwardVector, FMath::DegreesToRadians(60.0)) * Rest.GetRotation());
	Resting->MoveHeldPose(Desired);
	const FVector ContactUp = (Resting->GetActorQuat() * Rest.GetRotation().Inverse()).RotateVector(FVector::UpVector);
	TestTrue(TEXT("Contact result remains inside default 45 degree rest cone"), ContactUp.Z >= FMath::Cos(FMath::DegreesToRadians(45.0)) - 1e-6);
	TestTrue(TEXT("Over-limit contact request stays clear"), Resting->IsPoseClear(Resting->GetActorTransform()));
	Resting->ResetToPose(Rest);
	Desired = Rest;
	Desired.AddToTranslation(FVector(0, 0, 50));
	Desired.SetRotation(FQuat(FVector::ForwardVector, FMath::DegreesToRadians(30.0)) * Rest.GetRotation());
	TestTrue(TEXT("Lift and tilt requested pose is clear"), Resting->IsPoseClear(Desired));
	Resting->MoveHeldPose(Desired);
	TestTrue(TEXT("Lifting clears floor restriction in the same request"), Resting->GetActorTransform().Equals(Desired, 0.01));
	TestFalse(TEXT("Free lift and tilt remains held"), Resting->IsBlocked());
	const FTransform Lifted = Resting->GetActorTransform();
	Resting->MoveHeldPose(Desired);
	TestTrue(TEXT("Lift and tilt repeated request stays stationary"), Resting->GetActorTransform().Equals(Lifted, 1e-6));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP1CarryContactStateTest, "vBoR.P1Carry.ContactBlockageState", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP1CarryContactStateTest::RunTest(const FString& Parameters)
{
	FContactWorld Fixture;
	Fixture.Wall(FVector(0, 0, 100), FVector(5, 100, 100));
	AVBoRCarryObject* Object = Fixture.World->SpawnActor<AVBoRCarryObject>();
	Object->SetActorTransform(FTransform(FQuat::Identity, FVector(-15, 0, 110), FVector(0.2)));
	Object->Session.SetRestPose(Object->GetActorTransform());
	const FVector Rest = Object->GetActorLocation();
	FTransform Desired = Object->GetActorTransform();
	for (double Gap : {1.5, 2.5, 1.5, 2.5, 1.5})
	{
		Desired.SetLocation(Rest + FVector(Gap, 0, 0));
		Object->MoveHeldPose(Desired);
	}
	TestEqual(TEXT("Position hysteresis enters blockage once"), Object->BlockedCount, 1);
	TestTrue(TEXT("Hysteresis keeps blockage above release threshold"), Object->IsBlocked());
	Desired.SetLocation(Rest + FVector(0.5, 0, 0));
	Object->MoveHeldPose(Desired);
	TestFalse(TEXT("Below 1 cm releases blockage"), Object->IsBlocked());
	Desired.SetLocation(Rest + FVector(2, 0, 0));
	Object->MoveHeldPose(Desired);
	TestTrue(TEXT("Exactly 2 cm enters blockage"), Object->IsBlocked());
	TestEqual(TEXT("Fresh blockage after release counts again"), Object->BlockedCount, 2);
	Object->ResetToPose(FTransform(FQuat::Identity, FVector(-100, 0, 110), FVector(0.2)));
	for (double Angle : {60.0, 48.0})
	{
		Desired = Object->GetActorTransform();
		Desired.SetRotation(FQuat(FVector::ForwardVector, FMath::DegreesToRadians(Angle)));
		Object->MoveHeldPose(Desired);
		TestTrue(TEXT("Angular error enters and retains blockage"), Object->IsBlocked());
	}
	TestEqual(TEXT("Angular hysteresis counts entry once"), Object->BlockedCount, 1);
	Desired.SetRotation(FQuat(FVector::ForwardVector, FMath::DegreesToRadians(47.0)));
	Object->MoveHeldPose(Desired);
	TestFalse(TEXT("Below 2.5 degrees releases angular blockage"), Object->IsBlocked());
	FContactWorld Entrance;
	APlayerController* Controller = Entrance.World->SpawnActor<APlayerController>();
	Controller->Possess(Entrance.World->SpawnActor<AVBoRHandRigPawn>());
	AP1LoadBench* Bench = Entrance.World->SpawnActor<AP1LoadBench>();
	Entrance.BeginPlay();
	Bench->PlaceFromHead(FTransform(FVector(0, 0, 165)));
	const FTransform Frame = Bench->Trunk->GetActorTransform();
	Entrance.Wall(Frame.TransformPosition(FVector(25, 0, 100)), FVector(5, 300, 100));
	Bench->Bag->SetActorLocation(Frame.TransformPosition(FVector(-20, 0, 100)));
	Bench->Tick(0);
	Desired = Bench->Bag->GetActorTransform();
	Desired.SetLocation(Frame.TransformPosition(FVector(60, 0, 100)));
	Bench->Bag->MoveHeldPose(Desired);
	Bench->Tick(0);
	TestTrue(TEXT("Entrance crossing uses requested/actual blockage"), Bench->Bag->IsBlocked());
	TestEqual(TEXT("Blocked entrance crossing recorded once"), Bench->GetBlockedAtCrossings(), 1);
	TestEqual(TEXT("Entrance crossing recorded once"), Bench->GetEntranceCrossings(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP1CarryContactDropTest, "vBoR.P1Carry.ContactDropOnSlope", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP1CarryContactDropTest::RunTest(const FString& Parameters)
{
	FContactWorld Fixture;
	Fixture.Wall(FVector(0, 0, 0), FVector(100, 100, 5))->SetActorRotation(FRotator(20, 0, 0));
	AVBoRCarryObject* Object = Fixture.World->SpawnActor<AVBoRCarryObject>();
	Object->SetActorTransform(FTransform(FRotator(0, 15, 0), FVector(5, 6, 100), FVector(0.2)));
	const FTransform Before = Object->GetActorTransform();
	Fixture.BeginPlay();
	Object->BeginCarry(0, nullptr, Before);
	Object->ReleaseCarry();
	Object->Tick(1);
	TestEqual(TEXT("Slope stops drop outside target"), Object->GetStateText(), FString(TEXT("Released (outside target)")));
	TestTrue(TEXT("Slope drop stays clear"), Object->IsPoseClear(Object->GetActorTransform()));
	TestTrue(TEXT("Slope cannot cause sideways slide"), FVector2D(Object->GetActorLocation()).Equals(FVector2D(Before.GetLocation()), 1e-6));
	TestTrue(TEXT("Slope cannot rotate released object"), Object->GetActorQuat().Equals(Before.GetRotation()));
	const FTransform Stopped = Object->GetActorTransform();
	Object->Tick(1);
	TestTrue(TEXT("Released object remains stopped"), Object->GetActorTransform().Equals(Stopped));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP1CarryDefaultTiltTest, "vBoR.P1Carry.DefaultTilt45", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP1CarryDefaultTiltTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("Default carry tilt limit is 45 degrees"), GetDefault<AVBoRCarryObject>()->TiltLimitDeg, 45.0f);
	FContactWorld Fixture;
	AVBoRCarryObject* Bag = Fixture.Bag(FVector(0, 0, 200));
	const FTransform Rest = Bag->GetActorTransform();
	for (double RequestAngle : {30.0, 60.0})
	{
		Bag->ResetToPose(Rest);
		FTransform Hands[2] = {FTransform(Rest.GetLocation() + FVector(0, -30, 0)), FTransform(Rest.GetLocation() + FVector(0, 30, 0))};
		Bag->Session.Begin(0, Hands[0], Rest, 0);
		Bag->Session.Begin(1, Hands[1], Rest, 0);
		Hands[1].SetLocation(Hands[0].GetLocation() + FVector(0, 60 * FMath::Cos(FMath::DegreesToRadians(RequestAngle)), 60 * FMath::Sin(FMath::DegreesToRadians(RequestAngle))));
		FTransform Desired;
		Bag->Session.UpdateHands(true, Hands, Rest, 1, Bag->TiltLimitDeg, Desired);
		Bag->MoveHeldPose(Desired);
		TestTrue(*FString::Printf(TEXT("Two-hand %.0f degree request uses default limit"), RequestAngle),
			FMath::IsNearlyEqual(FVBoRCarryMotion::AngleDeg(Rest, Bag->GetActorTransform()), FMath::Min(RequestAngle, 45.0), 0.01));
	}
	Bag->ResetToPose(Rest);
	FQuat Previous = Rest.GetRotation();
	for (double Pitch = 85; Pitch <= 95; ++Pitch)
	{
		FTransform Desired = Rest;
		Desired.SetRotation(FRotator(Pitch, 0, 0).Quaternion() * Rest.GetRotation());
		Bag->MoveHeldPose(Desired);
		if (Pitch > 85) { TestTrue(TEXT("Default 45 degree cone keeps yaw continuous across 90 degrees"), Previous.AngularDistance(Bag->GetActorQuat()) < FMath::DegreesToRadians(1.01)); }
		Previous = Bag->GetActorQuat();
	}
	return true;
}
#endif
