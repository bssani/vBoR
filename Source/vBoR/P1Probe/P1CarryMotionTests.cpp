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
	TestTrue(TEXT("Second hand joins existing ownership"), Session.Begin(1, Hand, Object, 1));
	TestFalse(TEXT("Second hand release leaves first carrying"), Session.Release(1, true, 2));
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
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP1CarryRestTiltTest, "vBoR.P1Carry.RestTiltAcrossRegrabs", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP1CarryRestTiltTest::RunTest(const FString& Parameters)
{
	FVBoRCarrySession Session;
	const FTransform Rest(FRotator(0, 0, 90), FVector::ZeroVector);
	FTransform Object = Rest;
	for (int32 Grab = 0; Grab < 5; ++Grab)
	{
		Session.Begin(0, FTransform::Identity, Object, Grab * 3);
		FTransform Desired;
		Session.Update(true, FTransform(FRotator(10, 0, 0)), Object, Grab * 3 + 1, 15, Desired);
		Object = Desired;
		Session.Release(0, true, Grab * 3 + 2);
	}
	const FQuat Delta = Object.GetRotation() * Rest.GetRotation().Inverse();
	TestTrue(TEXT("Repeated regrab keeps rest tilt below 15 deg"), FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(
		FVector::DotProduct(Delta.RotateVector(FVector::UpVector), FVector::UpVector), -1.0, 1.0))) <= 15.001);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP1CarryTwoHandTest, "vBoR.P1Carry.TwoHandMotionAndTransitions", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP1CarryTwoHandTest::RunTest(const FString& Parameters)
{
	FVBoRCarrySession Session;
	const FTransform Rest(FRotator(0, 0, 90), FVector(50, 0, 90), FVector(0.28, 0.28, 1.2));
	FTransform Hands[2] = {FTransform(FVector(0, 0, 90)), FTransform(FVector(100, 0, 90))};
	FTransform Object = Rest;
	FTransform Desired;
	Session.Begin(0, Hands[0], Object, 0);
	Session.Begin(1, Hands[1], Object, 1);
	Session.UpdateHands(true, Hands, Object, 1, 15, Desired);
	TestTrue(TEXT("Joining frame preserves pose"), Desired.Equals(Object));
	const FVector LeftLocal = Object.InverseTransformPosition(Hands[0].GetLocation());
	Hands[1].SetLocation(FVector(86.602540378, 50, 90));
	Session.UpdateHands(true, Hands, Object, 2, 15, Desired);
	TestTrue(TEXT("Left grip stays at stationary left palm"), Desired.TransformPosition(LeftLocal).Equals(Hands[0].GetLocation(), 0.001));
	TestTrue(TEXT("Pair horizontal turn gives 30 deg yaw"), FMath::IsNearlyEqual((Desired.GetRotation() * Rest.GetRotation().Inverse()).Rotator().Yaw, 30.0, 0.001));
	Object = Desired;
	Hands[0].SetRotation(FRotator(65, -123, 40).Quaternion());
	Hands[1].SetRotation(FRotator(-90, 80, 70).Quaternion());
	Session.UpdateHands(true, Hands, Object, 3, 15, Desired);
	TestTrue(TEXT("Wrist rotation has no effect in two hand mode"), Desired.Equals(Object));
	Hands[1].SetLocation(FVector(100, 100, 300));
	Session.UpdateHands(true, Hands, Object, 4, 15, Desired);
	const FQuat Delta = Desired.GetRotation() * Rest.GetRotation().Inverse();
	TestTrue(TEXT("Pair height difference stays in rest tilt cone"), FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(Delta.RotateVector(FVector::UpVector), FVector::UpVector), -1.0, 1.0))) <= 15.001);
	TestTrue(TEXT("Stretch keeps object scale"), Desired.GetScale3D().Equals(Rest.GetScale3D()));
	Hands[1].SetLocation(FVector(20, 20, 90));
	Session.UpdateHands(true, Hands, Object, 4.5, 15, Desired);
	TestTrue(TEXT("Reduced pair spacing also keeps object scale"), Desired.GetScale3D().Equals(Rest.GetScale3D()));
	Object = Desired;
	Hands[1].SetLocation(Hands[0].GetLocation() + FVector(0.001, 0, 0));
	Session.UpdateHands(true, Hands, Object, 5, 15, Desired);
	TestTrue(TEXT("Close pair transition does not turn object"), Desired.Equals(Object));
	Hands[1].SetLocation(Hands[0].GetLocation() + FVector(-0.001, 0, 0));
	Session.UpdateHands(true, Hands, Object, 6, 15, Desired);
	TestTrue(TEXT("Near zero separation cannot flip yaw"), Desired.GetRotation().Equals(Object.GetRotation()));
	Hands[0].AddToTranslation(FVector(10, 5, 0));
	Hands[1].AddToTranslation(FVector(10, 5, 0));
	Hands[0].SetRotation(FRotator(30, 120, 70).Quaternion());
	Session.UpdateHands(true, Hands, Object, 6.2, 15, Desired);
	TestTrue(TEXT("Close pair follows primary translation only"), Desired.GetLocation().Equals(Object.GetLocation() + FVector(10, 5, 0), 0.001));
	TestTrue(TEXT("Close pair fallback does not use wrist rotation"), Desired.GetRotation().Equals(Object.GetRotation()));
	Object = Desired;
	Hands[1].SetLocation(Hands[0].GetLocation() + FVector(-100, 0, 0));
	TestFalse(TEXT("Regaining reliable pair direction only rebases"), Session.UpdateHands(true, Hands, Object, 6.5, 15, Desired));
	TestTrue(TEXT("Regaining pair direction cannot flip actor"), Desired.Equals(Object));
	TestFalse(TEXT("Loss of either hand freezes entire pair"), Session.UpdateHands(false, Hands, Object, 7, 15, Desired));
	TestFalse(TEXT("Paused first hand release is ignored"), Session.Release(0, false, 8, Hands, &Object));
	TestFalse(TEXT("Paused second hand release is ignored"), Session.Release(1, true, 8, Hands, &Object));
	TestTrue(TEXT("Both hands stay owned while paused"), Session.HasHand(0) && Session.HasHand(1));
	Hands[0].AddToTranslation(FVector(200, 0, 50));
	Hands[1].AddToTranslation(FVector(250, 0, 100));
	TestFalse(TEXT("Recovery frame only rebases both"), Session.UpdateHands(true, Hands, Object, 9, 15, Desired));
	TestTrue(TEXT("Recovery leaves object exactly unchanged"), Desired.Equals(Object));
	Session.UpdateHands(true, Hands, Object, 10, 15, Desired);
	TestTrue(TEXT("Next unchanged frame also has no stale movement"), Desired.Equals(Object));
	TestFalse(TEXT("Releasing first leaves second hand carrying"), Session.Release(0, true, 11, Hands, &Object));
	TestEqual(TEXT("Remaining hand becomes primary"), Session.Hand, 1);
	Session.Update(true, Hands[1], Object, 11, 15, Desired);
	TestTrue(TEXT("Two to one transition preserves exact pose"), Desired.Equals(Object));
	Hands[1].AddToTranslation(FVector(0, 10, 0));
	Session.Update(true, Hands[1], Object, 12, 15, Desired);
	TestTrue(TEXT("Remaining hand resumes translation"), Desired.GetLocation().Equals(Object.GetLocation() + FVector(0, 10, 0), 0.001));
	TestTrue(TEXT("Last hand ends carry"), Session.Release(1, true, 13));
	TestEqual(TEXT("One two-hand entry"), Session.TwoHandEntries, 1);
	TestEqual(TEXT("Two hand ownership time includes tracking pause"), Session.TwoHandSeconds, 10.0);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP1CarryTwoHandAxialTest, "vBoR.P1Carry.TwoHandPreservesEntryAxialTilt", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP1CarryTwoHandAxialTest::RunTest(const FString& Parameters)
{
	for (double RestYaw : {0.0, 37.0, 170.0})
	for (double EntryAngle : {0.0, 10.0, 15.0})
	{
		const FTransform Rest(FRotator(0, RestYaw, 90));
		const FVector AxialAxis = FRotator(0, RestYaw, 0).RotateVector(FVector::YAxisVector);
		const FQuat EntryAxial(AxialAxis, FMath::DegreesToRadians(EntryAngle));
		const FTransform Entry(EntryAxial * Rest.GetRotation(), FVector::ZeroVector);
		FTransform Hands[2] = {FTransform(AxialAxis * -50), FTransform(AxialAxis * 50)};
		FVBoRCarrySession Session;
		Session.SetRestPose(Rest);
		Session.Begin(0, Hands[0], Entry, 0);
		Session.Begin(1, Hands[1], Entry, 1);
		Hands[1].AddToTranslation(FVector(0, 0, 300));
		FTransform Desired;
		Session.UpdateHands(true, Hands, Entry, 2, 15, Desired);
		FQuat Swing;
		FQuat Twist;
		(Desired.GetRotation() * Entry.GetRotation().Inverse()).ToSwingTwist(AxialAxis, Swing, Twist);
		TestTrue(TEXT("Height limiting adds no axial twist beyond two-hand entry"), Twist.Equals(FQuat::Identity, 0.00001));
		const FQuat Relative = Desired.GetRotation() * Rest.GetRotation().Inverse();
		TestTrue(TEXT("Preserving entry axial tilt still respects rest cone"), FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(
			FVector::DotProduct(Relative.RotateVector(FVector::UpVector), FVector::UpVector), -1.0, 1.0))) <= 15.001);
	}
	return true;
}
#endif
