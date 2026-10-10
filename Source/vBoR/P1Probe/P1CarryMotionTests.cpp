#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "../Interaction/VBoRCarryMotion.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP1CarryPauseTest, "vBoR.P1Carry.TrackingPause", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP1CarryPauseTest::RunTest(const FString& Parameters)
{
	FVBoRCarrySession Session;
	const FTransform Object(FVector(20, 0, 90));
	Session.Begin(0, FTransform(FVector(10, 0, 90)), Object, 0);
	FTransform Desired;
	TestFalse(TEXT("Low tracking freezes pose"), Session.Update(false, FTransform(FVector(500, 0, 90)), Object, 1, 15, Desired));
	TestTrue(TEXT("Frozen pose is unchanged"), Desired.Equals(Object));
	TestFalse(TEXT("Invalid release cannot drop or seat"), Session.Release(0, false, 2));
	TestEqual(TEXT("Ownership survives SDK release"), Session.Hand, 0);
	TestEqual(TEXT("Ignored release count"), Session.IgnoredReleases, 1);
	TestFalse(TEXT("Repeated invalid sample stays paused"), Session.Update(false, FTransform(FVector(800, 0, 90)), Object, 3, 15, Desired));
	TestEqual(TEXT("One pause episode"), Session.PauseCount, 1);
	TestFalse(TEXT("First recovered frame only rebases"), Session.Update(true, FTransform(FVector(110, 0, 90)), Object, 4, 15, Desired));
	TestTrue(TEXT("Recovery first frame has zero movement"), Desired.Equals(Object));
	TestEqual(TEXT("Recovery distance is discarded movement in cm"), Session.RecoveryDistanceCm, 100.0);
	TestEqual(TEXT("Paused duration"), Session.PausedSeconds, 3.0);
	Session.Update(true, FTransform(FVector(115, 0, 90)), Object, 5, 15, Desired);
	TestTrue(TEXT("Subsequent frame follows new relative pose"), Desired.GetLocation().Equals(FVector(25, 0, 90)));
	TestTrue(TEXT("Tracked intended release after recovery completes"), Session.Release(0, true, 6));
	TestEqual(TEXT("Grab to release includes paused time"), Session.HeldSeconds, 6.0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP1CarryLimitsTest, "vBoR.P1Carry.RotationAndOwnership", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP1CarryLimitsTest::RunTest(const FString& Parameters)
{
	const FTransform StartHand(FQuat::Identity, FVector::ZeroVector);
	const FTransform Hand(FRotator(60, 85, -70), FVector(20, 0, 0));
	const FTransform Object(FRotator(90, 0, 0), FVector(0, 0, 10), FVector(0.28, 0.28, 1.2));
	const FQuat Delta = FVBoRCarryMotion::LimitedHandDelta(StartHand, Hand, 15);
	TestTrue(TEXT("Combined tilt remains within 15 deg cone"), FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(
		FVector::DotProduct(Delta.RotateVector(FVector::UpVector), FVector::UpVector), -1.0, 1.0))) <= 15.001);
	TestTrue(TEXT("Pure world yaw follows 85 deg hand turn"), FVBoRCarryMotion::LimitedHandDelta(StartHand,
		FTransform(FRotator(0, 85, 0)), 15).Equals(FRotator(0, 85, 0).Quaternion(), 0.0001));
	TestTrue(TEXT("Object shape scale is unchanged"), FVBoRCarryMotion::Follow(StartHand, Hand, Object, 15).GetScale3D().Equals(Object.GetScale3D()));
	FVBoRCarrySession Session;
	TestTrue(TEXT("First hand takes ownership"), Session.Begin(0, StartHand, Object, 0));
	TestFalse(TEXT("Second hand cannot take ownership"), Session.Begin(1, Hand, Object, 1));
	TestFalse(TEXT("Second hand cannot release"), Session.Release(1, true, 2));
	TestEqual(TEXT("First owner stays unchanged"), Session.Hand, 0);
	FTransform Moved;
	Session.Update(true, FTransform(FRotator(15, 0, 15), FVector::ZeroVector), Object, 3, 15, Moved);
	for (int32 Recovery = 0; Recovery < 3; ++Recovery)
	{
		Session.Pause(4 + Recovery * 3);
		const FTransform Before = Moved;
		TestFalse(TEXT("Tilted recovery only rebases"), Session.Update(true, StartHand, Before, 5 + Recovery * 3, 15, Moved));
		TestTrue(TEXT("Tilted recovery frame does not change pose"), Moved.Equals(Before));
		Session.Update(true, FTransform(FRotator(15, 30, 15), FVector::ZeroVector), Before, 6 + Recovery * 3, 15, Moved);
		const FRotator FromFirstGrab = (Moved.GetRotation() * Object.GetRotation().Inverse()).Rotator();
		TestTrue(TEXT("Repeated rebase cannot accumulate pitch or roll beyond first-grab limits"),
			FMath::Abs(FromFirstGrab.Pitch) <= 15.001 && FMath::Abs(FromFirstGrab.Roll) <= 15.001);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP1CarryPitchBoundaryTest, "vBoR.P1Carry.PitchBoundary", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP1CarryPitchBoundaryTest::RunTest(const FString& Parameters)
{
	FQuat Previous = FVBoRCarryMotion::LimitedHandDelta(FTransform::Identity, FTransform(FRotator(80, 0, 0)), 15);
	FVBoRCarrySession Session;
	Session.Begin(0, FTransform::Identity, FTransform::Identity, 0);
	FTransform Desired;
	for (int32 Pitch = 81; Pitch <= 100; ++Pitch)
	{
		const FTransform Hand(FRotator(Pitch, 0, 0));
		const FQuat Limited = FVBoRCarryMotion::LimitedHandDelta(FTransform::Identity, Hand, 15);
		TestTrue(*FString::Printf(TEXT("Pitch %d does not flip object yaw"), Pitch), FMath::Abs(Limited.Rotator().Yaw) < 1.0);
		TestTrue(TEXT("Adjacent output rotates at most one degree"), FMath::RadiansToDegrees(Previous.AngularDistance(Limited)) < 1.01);
		TestTrue(TEXT("Tilt stays within 15 deg"), FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(
			FVector::DotProduct(Limited.RotateVector(FVector::UpVector), FVector::UpVector), -1.0, 1.0))) <= 15.001);
		Session.Update(true, Hand, FTransform::Identity, Pitch, 15, Desired);
		TestTrue(TEXT("First-grab re-limit also avoids yaw flip"), FMath::Abs(Desired.Rotator().Yaw) < 1.0);
		Previous = Limited;
	}
	Session.Update(true, FTransform(FRotator(179, 0, 0)), FTransform::Identity, 110, 15, Desired);
	TestTrue(TEXT("One inverted hand sample cannot turn yaw by 180 deg"), FMath::Abs(Desired.Rotator().Yaw) < 1.0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP1CarryReleaseTest, "vBoR.P1Carry.ReleaseDecisions", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP1CarryReleaseTest::RunTest(const FString& Parameters)
{
	const FTransform Object(FVector(0, 0, 90));
	const FTransform Goal(FRotator(0, 10, 0), FVector(5, 0, 90));
	TestTrue(TEXT("Near clear target allows assistance"), FVBoRCarryMotion::CanAssist(Object, Goal, true, 10, 20));
	TestFalse(TEXT("Target overlap rejects assistance"), FVBoRCarryMotion::CanAssist(Object, Goal, false, 10, 20));
	TestFalse(TEXT("Distance outside threshold rejects assistance"), FVBoRCarryMotion::CanAssist(Object, FTransform(FVector(11, 0, 90)), true, 10, 20));
	TestFalse(TEXT("Angle outside threshold rejects assistance"), FVBoRCarryMotion::CanAssist(Object, FTransform(FRotator(0, 21, 0)), true, 100, 20));
	TestTrue(TEXT("Drop has constant vertical movement only"), FVBoRCarryMotion::Drop(FVector(5, 6, 100), 200, 0.1).Equals(FVector(5, 6, 80)));
	TestTrue(TEXT("Synthetic sweep stops at first hit"), FVBoRCarryMotion::SweepStop(FVector(0, 0, 100), FVector(0, 0, 0), 0.3).Equals(FVector(0, 0, 70)));
	return true;
}
#endif
