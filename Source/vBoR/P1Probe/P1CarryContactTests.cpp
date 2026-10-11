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
#include <limits>

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
	Object->MoveHeldPose(Desired, 1.0f / 60.0f);
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
	Box->MoveHeldPose(Desired, 1.0f / 60.0f);
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
	Bag->MoveHeldPose(Desired, 1.0f / 60.0f);
	const double Yaw = FVBoRCarryMotion::AngleDeg(Before, Bag->GetActorTransform());
	TestTrue(TEXT("Contact applies some requested yaw"), Yaw > 0.01);
	TestTrue(TEXT("Pivot pose is clear"), Bag->IsPoseClear(Bag->GetActorTransform()));
	TestTrue(TEXT("Contact corner stays on inside of wall"), Bag->GetActorTransform().TransformPosition(FVector(-50, 0, 50)).Y <= 65.01);
	TestTrue(TEXT("Opposite end follows requested direction"), Bag->GetActorTransform().TransformPosition(FVector(0, 0, -50)).X < Before.TransformPosition(FVector(0, 0, -50)).X);
	TestTrue(TEXT("Large rotation is reduced rather than applied whole"), Yaw < 9.99);
	TestTrue(TEXT("Rotation push stays within rate times dt"), FVector::Distance(Before.GetLocation(), Bag->GetActorLocation()) <= Bag->MaxRotationPushCmPerSecond / 60.0 + 1e-6);
	TestTrue(TEXT("Contact rotation preserves scale"), Bag->GetActorScale3D().Equals(Before.GetScale3D()));
	const FTransform First = Bag->GetActorTransform();
	Bag->MoveHeldPose(Desired, 1.0f / 60.0f);
	TestTrue(TEXT("Stable request continues toward requested rotation"),
		FVBoRCarryMotion::AngleDeg(Desired, Bag->GetActorTransform()) < FVBoRCarryMotion::AngleDeg(Desired, First) - 0.01);
	double PreviousError = FVBoRCarryMotion::AngleDeg(Desired, Bag->GetActorTransform());
	for (int32 Frame = 0; Frame < 90; ++Frame)
	{
		const FTransform Previous = Bag->GetActorTransform();
		Bag->MoveHeldPose(Desired, 1.0f / 60.0f);
		const double Error = FVBoRCarryMotion::AngleDeg(Desired, Bag->GetActorTransform());
		TestTrue(TEXT("Stable request angular error cannot increase"), Error <= PreviousError + 1e-6);
		TestTrue(TEXT("Stable request cannot overshoot yaw"), FVBoRCarryMotion::AngleDeg(Before, Bag->GetActorTransform()) <= 10.0 + 1e-6);
		TestTrue(TEXT("Stable request frame push stays within rate times dt"), FVector::Distance(Previous.GetLocation(), Bag->GetActorLocation()) <= Bag->MaxRotationPushCmPerSecond / 60.0 + 1e-6);
		TestTrue(TEXT("Stable request remains nonoverlapping"), Bag->IsPoseClear(Bag->GetActorTransform()));
		PreviousError = Error;
	}
	TestTrue(TEXT("Stable request reaches requested yaw"), PreviousError < 0.01);
	const FTransform Settled = Bag->GetActorTransform();
	Bag->MoveHeldPose(Desired, 1.0f / 60.0f);
	TestTrue(TEXT("Reached request stays settled"), Bag->GetActorTransform().Equals(Settled, 1e-6));
	FContactWorld Floor;
	Floor.Wall(FVector(0, 0, -5), FVector(300, 300, 5));
	AVBoRCarryObject* Resting = Floor.Bag(FVector(0, 0, 14));
	const FTransform Rest = Resting->GetActorTransform();
	Resting->Session.SetRestPose(Rest);
	Resting->Session.Begin(0, Rest, Rest, 0);
	Desired = Rest;
	Desired.AddToTranslation(FVector(0, 0, -0.3));
	Desired.SetRotation(FQuat(FVector::ForwardVector, FMath::DegreesToRadians(0.5)) * Rest.GetRotation());
	for (int32 Frame = 0; Frame < 30; ++Frame)
	{
		const FTransform Previous = Resting->GetActorTransform();
		Resting->MoveHeldPose(Desired, 1.0f / 60.0f);
		TestTrue(TEXT("Repeated real-sized regrab tremor leaves floor pose clear"), Resting->IsPoseClear(Resting->GetActorTransform()));
		TestFalse(TEXT("Repeated 0.3 cm downward and 0.5 degree tremor remains held"), Resting->IsBlocked());
		TestTrue(TEXT("Floor tremor frame push stays within rate times dt"), FVector::Distance(Previous.GetLocation(), Resting->GetActorLocation()) <= Resting->MaxRotationPushCmPerSecond / 60.0 + 1e-6);
	}
	TestTrue(TEXT("Floor contact allows partial tilt"), FVBoRCarryMotion::AngleDeg(Rest, Resting->GetActorTransform()) > 0.01);
	Resting->Session.bPaused = true;
	const FTransform Paused = Resting->GetActorTransform();
	Desired.AddToTranslation(FVector(100, 0, 100));
	Resting->MoveHeldPose(Desired, 1.0f / 60.0f);
	TestTrue(TEXT("Paused contact response cannot move"), Resting->GetActorTransform().Equals(Paused));
	Resting->ResetToPose(Rest);
	Resting->BeginCarry(0, nullptr, Rest);
	Desired = Rest;
	Desired.SetRotation(FQuat(FVector::ForwardVector, FMath::DegreesToRadians(60.0)) * Rest.GetRotation());
	Resting->MoveHeldPose(Desired, 1.0f / 60.0f);
	const FVector ContactUp = (Resting->GetActorQuat() * Rest.GetRotation().Inverse()).RotateVector(FVector::UpVector);
	TestTrue(TEXT("Contact result remains inside default 45 degree rest cone"), ContactUp.Z >= FMath::Cos(FMath::DegreesToRadians(45.0)) - 1e-6);
	TestTrue(TEXT("Over-limit contact request stays clear"), Resting->IsPoseClear(Resting->GetActorTransform()));
	Resting->ResetToPose(Rest);
	Desired = Rest;
	Desired.AddToTranslation(FVector(0, 0, 50));
	Desired.SetRotation(FQuat(FVector::ForwardVector, FMath::DegreesToRadians(30.0)) * Rest.GetRotation());
	TestTrue(TEXT("Lift and tilt requested pose is clear"), Resting->IsPoseClear(Desired));
	Resting->MoveHeldPose(Desired, 1.0f / 60.0f);
	TestTrue(TEXT("Lifting clears floor restriction in the same request"), Resting->GetActorTransform().Equals(Desired, 0.01));
	TestFalse(TEXT("Free lift and tilt remains held"), Resting->IsBlocked());
	const FTransform Lifted = Resting->GetActorTransform();
	Resting->MoveHeldPose(Desired, 1.0f / 60.0f);
	TestTrue(TEXT("Lift and tilt repeated request stays stationary"), Resting->GetActorTransform().Equals(Lifted, 1e-6));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP1CarryContactRateTest, "vBoR.P1Carry.ContactFrameRate", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP1CarryContactRateTest::RunTest(const FString& Parameters)
{
	FContactWorld Fixture;
	AActor* Wall = Fixture.Wall(FVector(5, 0, 70), FVector(5, 300, 70));
	AVBoRCarryObject* Bag = Fixture.Bag(FVector(-14, 0, 70));
	const FTransform Rest = Bag->GetActorTransform();
	FTransform Desired = Rest;
	Desired.SetRotation(FRotator(0, -30, 0).Quaternion() * Rest.GetRotation());
	auto Simulate = [&](int32 Fps)
	{
		Bag->ResetToPose(Rest);
		Bag->MaxRotationPushCmPerSecond = 30;
		double PreviousError = 30;
		int32 Frames = 0;
		double Elapsed = 0;
		while (Elapsed < 0.5 - 1e-12)
		{
			// 15 fps 는 7 frame + 반 frame 으로 정확히 0.5 s 를 맞춤 (P1 R6-1)
			const double StepSeconds = FMath::Min(1.0 / Fps, 0.5 - Elapsed);
			const float DeltaSeconds = static_cast<float>(StepSeconds);
			const FTransform Previous = Bag->GetActorTransform();
			Bag->MoveHeldPose(Desired, DeltaSeconds);
			const double Error = FVBoRCarryMotion::AngleDeg(Desired, Bag->GetActorTransform());
			TestTrue(TEXT("Rate-limited frame angular error cannot increase"), Error <= PreviousError + 1e-6);
			TestTrue(TEXT("Rate-limited frame cannot overshoot requested yaw"), FVBoRCarryMotion::AngleDeg(Rest, Bag->GetActorTransform()) <= 30.0 + 1e-6);
			TestTrue(TEXT("Each frame push obeys editable rate times dt"), FVector::Distance(Previous.GetLocation(), Bag->GetActorLocation()) <= 30.0 * DeltaSeconds + 1e-6);
			TestTrue(TEXT("Rate-limited frame remains clear"), Bag->IsPoseClear(Bag->GetActorTransform()));
			PreviousError = Error;
			Elapsed += StepSeconds;
			++Frames;
		}
		TestEqual(TEXT("Exact half-second frame count"), Frames, Fps == 15 ? 8 : 45);
		TestTrue(TEXT("Comparison still has unapplied requested rotation"), PreviousError > 1.0);
		TestTrue(TEXT("Comparison has substantial contact motion"), FVBoRCarryMotion::AngleDeg(Rest, Bag->GetActorTransform()) > 5.0);
		AddInfo(FString::Printf(TEXT("ContactFrameRate fps=%d duration=%.9f frames=%d position=%s rotation=%s errorDeg=%.6f"),
			Fps, Elapsed, Frames, *Bag->GetActorLocation().ToString(), *Bag->GetActorRotation().ToString(), PreviousError));
		return Bag->GetActorTransform();
	};
	const FTransform At15 = Simulate(15);
	const FTransform At90 = Simulate(90);
	const double PositionDifference = FVector::Distance(At15.GetLocation(), At90.GetLocation());
	const double AngleDifference = FVBoRCarryMotion::AngleDeg(At15, At90);
	TestTrue(TEXT("15 and 90 fps position difference within 0.2 cm"), PositionDifference <= 0.2);
	TestTrue(TEXT("15 and 90 fps angle difference within 0.5 degrees"), AngleDifference <= 0.5);
	AddInfo(FString::Printf(TEXT("ContactFrameRate differenceCm=%.9f differenceDeg=%.9f"), PositionDifference, AngleDifference));
	for (float DeltaSeconds : {0.0f, -1.0f, std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()})
	{
		Bag->ResetToPose(Rest);
		Bag->MoveHeldPose(Desired, DeltaSeconds);
		TestTrue(TEXT("Nonpositive or nonfinite dt cannot add contact push"), Bag->GetActorLocation().Equals(Rest.GetLocation(), 1e-6));
		TestTrue(TEXT("Invalid dt result is finite and clear"), !Bag->GetActorTransform().ContainsNaN() && Bag->IsPoseClear(Bag->GetActorTransform()));
	}
	for (float Rate : {0.0f, -30.0f, std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()})
	{
		Bag->ResetToPose(Rest);
		Bag->MaxRotationPushCmPerSecond = Rate;
		Bag->MoveHeldPose(Desired, 1.0f / 60.0f);
		TestTrue(TEXT("Nonpositive or nonfinite rate cannot add contact push"), Bag->GetActorLocation().Equals(Rest.GetLocation(), 1e-6));
	}
	// 양쪽 벽으로 회전 여지를 없앰 (도달하지 못한 같은 요청도 정지해야 함, P1 R6-1)
	Wall->Destroy();
	Fixture.Wall(FVector(19, 0, 70), FVector(5, 300, 70));
	Fixture.Wall(FVector(-19, 0, 70), FVector(5, 300, 70));
	Bag->ResetToPose(FTransform(Rest.GetRotation(), FVector(0, 0, 70), Rest.GetScale3D()));
	Bag->MaxRotationPushCmPerSecond = 30;
	Desired = Bag->GetActorTransform();
	Desired.SetRotation(FRotator(0, -30, 0).Quaternion() * Rest.GetRotation());
	for (int32 Frame = 0; Frame < 20; ++Frame) { Bag->MoveHeldPose(Desired, 1.0f / 60.0f); }
	TestTrue(TEXT("Two-wall request remains blocked and cannot reach rotation"), Bag->IsBlocked() && FVBoRCarryMotion::AngleDeg(Desired, Bag->GetActorTransform()) > 1.0);
	const FTransform Blocked = Bag->GetActorTransform();
	for (int32 Frame = 0; Frame < 20; ++Frame) { Bag->MoveHeldPose(Desired, 1.0f / 60.0f); }
	TestTrue(TEXT("Unreachable request settles without autonomous movement"), Bag->GetActorTransform().Equals(Blocked, 1e-6));
	TestTrue(TEXT("Blocked settled pose remains clear"), Bag->IsPoseClear(Bag->GetActorTransform()));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP1CarryContactFloorTiltTest, "vBoR.P1Carry.ContactFloorTilt", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP1CarryContactFloorTiltTest::RunTest(const FString& Parameters)
{
	FContactWorld Floor;
	Floor.Wall(FVector(0, 0, -5), FVector(300, 300, 5));
	AVBoRCarryObject* Bag = Floor.Bag(FVector(0, 0, 14));
	const FTransform Rest = Bag->GetActorTransform();
	Bag->Session.SetRestPose(Rest);
	Bag->Session.Begin(0, Rest, Rest, 0);
	FTransform Desired = Rest;
	Desired.SetRotation(FQuat(FVector::ForwardVector, FMath::DegreesToRadians(3.0)) * Rest.GetRotation());
	for (int32 Frame = 0; Frame < 60; ++Frame)
	{
		const FTransform Previous = Bag->GetActorTransform();
		Bag->MoveHeldPose(Desired, 1.0f / 60.0f);
		TestTrue(TEXT("Repeated 3 degree floor tilt remains nonoverlapping"), Bag->IsPoseClear(Bag->GetActorTransform()));
		TestTrue(TEXT("Floor tilt frame push obeys rate times dt"), FVector::Distance(Previous.GetLocation(), Bag->GetActorLocation()) <= Bag->MaxRotationPushCmPerSecond / 60.0 + 1e-6);
		TestTrue(TEXT("Low end stays above floor skin"), Bag->GetCollisionBoundsInPose(Bag->GetActorTransform(), FTransform::Identity).Min.Z >= -0.005 - 1e-6);
		TestTrue(TEXT("Floor tilt cannot exceed requested angle"), FVBoRCarryMotion::AngleDeg(Rest, Bag->GetActorTransform()) <= 3.0 + 1e-6);
	}
	TestTrue(TEXT("Floor tilt reaches requested 3 degree rotation"), FVBoRCarryMotion::AngleDeg(Desired, Bag->GetActorTransform()) < 0.01);
	const FTransform Settled = Bag->GetActorTransform();
	Bag->MoveHeldPose(Desired, 1.0f / 60.0f);
	TestTrue(TEXT("Floor tilt stays settled once reached"), Bag->GetActorTransform().Equals(Settled, 1e-6));
	AddInfo(FString::Printf(TEXT("Floor3deg centerRiseCm=%.6f requestedGapCm=%.6f blocked=%s lowPointZCm=%.6f angleDeg=%.6f; Quest 3 naturalness verdict not-tested (T-02/T-09)"),
		Bag->GetActorLocation().Z - Rest.GetLocation().Z, FVector::Distance(Desired.GetLocation(), Bag->GetActorLocation()), Bag->IsBlocked() ? TEXT("true") : TEXT("false"),
		Bag->GetCollisionBoundsInPose(Bag->GetActorTransform(), FTransform::Identity).Min.Z, FVBoRCarryMotion::AngleDeg(Rest, Bag->GetActorTransform())));
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
		Object->MoveHeldPose(Desired, 1.0f / 60.0f);
	}
	TestEqual(TEXT("Position hysteresis enters blockage once"), Object->BlockedCount, 1);
	TestTrue(TEXT("Hysteresis keeps blockage above release threshold"), Object->IsBlocked());
	Desired.SetLocation(Rest + FVector(0.5, 0, 0));
	Object->MoveHeldPose(Desired, 1.0f / 60.0f);
	TestFalse(TEXT("Below 1 cm releases blockage"), Object->IsBlocked());
	Desired.SetLocation(Rest + FVector(2, 0, 0));
	Object->MoveHeldPose(Desired, 1.0f / 60.0f);
	TestTrue(TEXT("Exactly 2 cm enters blockage"), Object->IsBlocked());
	TestEqual(TEXT("Fresh blockage after release counts again"), Object->BlockedCount, 2);
	Object->ResetToPose(FTransform(FQuat::Identity, FVector(-100, 0, 110), FVector(0.2)));
	for (double Angle : {60.0, 48.0})
	{
		Desired = Object->GetActorTransform();
		Desired.SetRotation(FQuat(FVector::ForwardVector, FMath::DegreesToRadians(Angle)));
		Object->MoveHeldPose(Desired, 1.0f / 60.0f);
		TestTrue(TEXT("Angular error enters and retains blockage"), Object->IsBlocked());
	}
	TestEqual(TEXT("Angular hysteresis counts entry once"), Object->BlockedCount, 1);
	Desired.SetRotation(FQuat(FVector::ForwardVector, FMath::DegreesToRadians(47.0)));
	Object->MoveHeldPose(Desired, 1.0f / 60.0f);
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
	Bench->Bag->MoveHeldPose(Desired, 1.0f / 60.0f);
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
		Bag->MoveHeldPose(Desired, 1.0f / 60.0f);
		TestTrue(*FString::Printf(TEXT("Two-hand %.0f degree request uses default limit"), RequestAngle),
			FMath::IsNearlyEqual(FVBoRCarryMotion::AngleDeg(Rest, Bag->GetActorTransform()), FMath::Min(RequestAngle, 45.0), 0.01));
	}
	Bag->ResetToPose(Rest);
	FQuat Previous = Rest.GetRotation();
	for (double Pitch = 85; Pitch <= 95; ++Pitch)
	{
		FTransform Desired = Rest;
		Desired.SetRotation(FRotator(Pitch, 0, 0).Quaternion() * Rest.GetRotation());
		Bag->MoveHeldPose(Desired, 1.0f / 60.0f);
		if (Pitch > 85) { TestTrue(TEXT("Default 45 degree cone keeps yaw continuous across 90 degrees"), Previous.AngularDistance(Bag->GetActorQuat()) < FMath::DegreesToRadians(1.01)); }
		Previous = Bag->GetActorQuat();
	}
	return true;
}
#endif
