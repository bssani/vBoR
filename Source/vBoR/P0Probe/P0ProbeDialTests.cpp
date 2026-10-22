#include "P0ProbeDialMeasurement.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"

namespace
{
	// Exclude 3 cm axial component from projection (P0-a §2)
	FVector DialVector(double Degrees, double Radius = 2.0)
	{
		const double Radians = FMath::DegreesToRadians(Degrees);
		return FVector(Radius * FMath::Cos(Radians), Radius * FMath::Sin(Radians), 3.0);
	}

	void DialFrame(FP0ProbeDialMeasurement& State, double Time, double Degrees, double WristDegrees = 0.0,
		bool bValid = true, bool bThumbHigh = true, bool bIndexHigh = true, bool bBonesReady = true, double Radius = 2.0)
	{
		State.Update(Time, bValid, bThumbHigh, bIndexHigh, bBonesReady, FVector::ZeroVector, DialVector(Degrees, Radius),
			FQuat(FVector::UpVector, FMath::DegreesToRadians(WristDegrees)), FVector::UpVector, 10.0);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP0ProbeDialHoldTest, "vBoR.P0Probe.Dial.Hold", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP0ProbeDialHoldTest::RunTest(const FString& Parameters)
{
	// Break each condition separately (catch missing gates, P0-a §4-6)
	for (int32 Gate = 0; Gate < 5; ++Gate)
	{
		FP0ProbeDialMeasurement State;
		DialFrame(State, 0.0, 0.0);
		DialFrame(State, 0.1, 30.0, 10.0);
		TestTrue(TEXT("Valid frame accumulates dial"), FMath::IsNearlyEqual(State.DialAngleDeg, 30.0, 1.e-6));
		DialFrame(State, 0.2, 150.0, 90.0, Gate != 0, Gate != 1, Gate != 2, Gate != 3, Gate == 4 ? 0.0 : 2.0);
		TestFalse(TEXT("Broken condition holds"), State.bUpdating);
		TestTrue(TEXT("Held dial angle"), FMath::IsNearlyEqual(State.DialAngleDeg, 30.0, 1.e-6));
		TestTrue(TEXT("Held wrist twist"), FMath::IsNearlyEqual(State.WristTwistDeg, 10.0, 1.e-6));
		TestEqual(TEXT("Broken condition clears window"), State.Samples.Num(), 0);
		TestEqual(TEXT("Broken condition clears diameter"), State.DiameterDeg, 0.0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP0ProbeDialRecoveryTest, "vBoR.P0Probe.Dial.Recovery", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP0ProbeDialRecoveryTest::RunTest(const FString& Parameters)
{
	FP0ProbeDialMeasurement State;
	DialFrame(State, 0.0, 0.0);
	DialFrame(State, 0.1, 20.0, 15.0);
	DialFrame(State, 0.2, 120.0, 90.0, false);
	DialFrame(State, 0.3, -120.0, -90.0);
	TestTrue(TEXT("Recovery dial delta zero"), FMath::IsNearlyEqual(State.DialAngleDeg, 20.0, 1.e-6));
	TestTrue(TEXT("Recovery wrist delta zero"), FMath::IsNearlyEqual(State.WristTwistDeg, 15.0, 1.e-6));
	TestEqual(TEXT("Recovery starts fresh window"), State.Samples.Num(), 1);
	DialFrame(State, 0.4, -110.0, -80.0);
	TestTrue(TEXT("Movement after recovery accumulates"), FMath::IsNearlyEqual(State.DialAngleDeg, 30.0, 1.e-6));
	TestTrue(TEXT("Wrist movement after recovery accumulates"), FMath::IsNearlyEqual(State.WristTwistDeg, 25.0, 1.e-6));
	TestTrue(TEXT("Angular window diameter"), FMath::IsNearlyEqual(State.DiameterDeg, 10.0, 1.e-6));
	DialFrame(State, 1.5, -110.0, -80.0);
	TestEqual(TEXT("One second expiry"), State.Samples.Num(), 1);
	TestEqual(TEXT("Expired directions excluded"), State.DiameterDeg, 0.0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP0ProbeDialUnwrapTest, "vBoR.P0Probe.Dial.Unwrap", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP0ProbeDialUnwrapTest::RunTest(const FString& Parameters)
{
	FP0ProbeDialMeasurement State;
	DialFrame(State, 0.0, 170.0, 170.0);
	DialFrame(State, 0.1, -170.0, -170.0);
	TestTrue(TEXT("Forward crossing is +20 deg"), FMath::IsNearlyEqual(State.DialAngleDeg, 20.0, 1.e-6));
	TestTrue(TEXT("Wrist crossing is +20 deg"), FMath::IsNearlyEqual(State.WristTwistDeg, 20.0, 1.e-6));
	TestTrue(TEXT("Window uses shortest angle"), FMath::IsNearlyEqual(State.DiameterDeg, 20.0, 1.e-6));
	DialFrame(State, 0.2, 170.0, 170.0);
	TestTrue(TEXT("Reverse crossing is -20 deg"), FMath::IsNearlyZero(State.DialAngleDeg, 1.e-6));
	TestTrue(TEXT("Reverse wrist crossing is -20 deg"), FMath::IsNearlyZero(State.WristTwistDeg, 1.e-6));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP0ProbeDialWristTest, "vBoR.P0Probe.Dial.WristSeparation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP0ProbeDialWristTest::RunTest(const FString& Parameters)
{
	FP0ProbeDialMeasurement State;
	DialFrame(State, 0.0, 0.0);
	DialFrame(State, 0.1, 45.0);
	TestTrue(TEXT("Finger roll changes dial"), FMath::IsNearlyEqual(State.DialAngleDeg, 45.0, 1.e-6));
	TestEqual(TEXT("Finger roll leaves wrist unchanged"), State.WristTwistDeg, 0.0);
	DialFrame(State, 0.2, 75.0, 30.0);
	TestTrue(TEXT("Whole hand changes dial"), FMath::IsNearlyEqual(State.DialAngleDeg, 75.0, 1.e-6));
	TestTrue(TEXT("Whole hand changes wrist"), FMath::IsNearlyEqual(State.WristTwistDeg, 30.0, 1.e-6));
	TestTrue(TEXT("Projected length in mm"), FMath::IsNearlyEqual(State.ProjectedLengthMm, 20.0, 1.e-6));
	TestTrue(TEXT("Tip distance includes axial component"), FMath::IsNearlyEqual(State.TipDistanceMm, 36.05551275463989, 1.e-6));

	// Dial axis not limited to world Z. Check nonnormalized X with swing (P0-a §2)
	FP0ProbeDialMeasurement Side;
	const FVector Axis(4.0, 0.0, 0.0);
	const FQuat Swing(FVector::RightVector, FMath::DegreesToRadians(40.0));
	Side.Update(0.0, true, true, true, true, FVector::ZeroVector, FVector(3.0, 2.0, 0.0), Swing, Axis, 10.0);
	const FQuat Turn(FVector::ForwardVector, FMath::DegreesToRadians(30.0));
	Side.Update(0.1, true, true, true, true, FVector::ZeroVector, Turn.RotateVector(FVector(3.0, 2.0, 0.0)), Turn * Swing, Axis, 10.0);
	TestTrue(TEXT("World X dial angle"), FMath::IsNearlyEqual(Side.DialAngleDeg, 30.0, 1.e-6));
	TestTrue(TEXT("World X wrist twist with swing"), FMath::IsNearlyEqual(Side.WristTwistDeg, 30.0, 1.e-6));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP0ProbeDialPinchTest, "vBoR.P0Probe.Dial.PinchReset", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP0ProbeDialPinchTest::RunTest(const FString& Parameters)
{
	FP0ProbeDialMeasurement State;
	DialFrame(State, 0.0, 0.0);
	DialFrame(State, 0.1, 60.0, 30.0);
	TestTrue(TEXT("Dial nonzero before press"), FMath::IsNearlyEqual(State.DialAngleDeg, 60.0, 1.e-6));
	TestTrue(TEXT("Wrist nonzero before press"), FMath::IsNearlyEqual(State.WristTwistDeg, 30.0, 1.e-6));
	State.ResetAngles();
	TestEqual(TEXT("Pinch resets dial immediately"), State.DialAngleDeg, 0.0);
	TestEqual(TEXT("Pinch resets wrist immediately"), State.WristTwistDeg, 0.0);
	DialFrame(State, 0.2, 100.0, 70.0);
	TestEqual(TEXT("First frame after press rebases dial"), State.DialAngleDeg, 0.0);
	TestEqual(TEXT("First frame after press rebases wrist"), State.WristTwistDeg, 0.0);
	DialFrame(State, 0.3, 110.0, 80.0);
	TestTrue(TEXT("Dial accumulates from new reference"), FMath::IsNearlyEqual(State.DialAngleDeg, 10.0, 1.e-6));
	TestTrue(TEXT("Wrist accumulates from new reference"), FMath::IsNearlyEqual(State.WristTwistDeg, 10.0, 1.e-6));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP0ProbeDialSwingTest, "vBoR.P0Probe.Dial.FrameSwing", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP0ProbeDialSwingTest::RunTest(const FString& Parameters)
{
	// Check swing counted as twist/local used instead of world axis (R2-2, P0-a §4-6)
	const FVector Axis = FVector(1.0, 2.0, 3.0).GetSafeNormal();
	const FVector SwingAxis = FVector(2.0, -1.0, 0.0).GetSafeNormal();
	const FVector Tip = SwingAxis * 2.0;
	const FQuat Initial(FVector::RightVector, FMath::DegreesToRadians(25.0));
	const FQuat Swing(SwingAxis, FMath::DegreesToRadians(40.0));
	FP0ProbeDialMeasurement Pure;
	Pure.Update(0.0, true, true, true, true, FVector::ZeroVector, Tip, Initial, Axis, 10.0);
	Pure.Update(0.1, true, true, true, true, FVector::ZeroVector, Tip, Swing * Initial, Axis, 10.0);
	TestTrue(TEXT("Frame swing has zero twist"), FMath::IsNearlyZero(Pure.WristTwistDeg, 1.e-6));
	FP0ProbeDialMeasurement Mixed;
	Mixed.Update(0.0, true, true, true, true, FVector::ZeroVector, Tip, Initial, Axis, 10.0);
	const FQuat Twist(Axis, FMath::DegreesToRadians(30.0));
	const FQuat After = Swing * Twist * Initial;
	Mixed.Update(0.1, true, true, true, true, FVector::ZeroVector, Tip, After, Axis, 10.0);
	TestTrue(TEXT("Mixed frame counts only +30 deg twist"), FMath::IsNearlyEqual(Mixed.WristTwistDeg, 30.0, 1.e-6));
	const FQuat ReverseTwist(Axis, FMath::DegreesToRadians(-20.0));
	Mixed.Update(0.2, true, true, true, true, FVector::ZeroVector, Tip, Swing * ReverseTwist * After, Axis, 10.0);
	TestTrue(TEXT("Mixed frames accumulate signed twist"), FMath::IsNearlyEqual(Mixed.WristTwistDeg, 10.0, 1.e-6));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP0ProbeDialProjectionTest, "vBoR.P0Probe.Dial.ProjectionBoundary", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP0ProbeDialProjectionTest::RunTest(const FString& Parameters)
{
	// Check missing lower bound/unit confusion in UE cm -> mm comparison (R2-3, P0-a §4-6)
	FP0ProbeDialMeasurement State;
	DialFrame(State, 0.0, 0.0);
	DialFrame(State, 0.1, 30.0);
	DialFrame(State, 0.2, 90.0, 0.0, true, true, true, true, 0.005);
	TestTrue(TEXT("Small projection is 0.05 mm"), FMath::IsNearlyEqual(State.ProjectedLengthMm, 0.05, 1.e-6));
	TestFalse(TEXT("0.05 mm holds"), State.bUpdating);
	TestTrue(TEXT("Below boundary preserves angle"), FMath::IsNearlyEqual(State.DialAngleDeg, 30.0, 1.e-6));
	TestEqual(TEXT("Below boundary clears window"), State.Samples.Num(), 0);
	DialFrame(State, 0.3, 120.0, 0.0, true, true, true, true, 0.015);
	TestTrue(TEXT("Large projection is 0.15 mm"), FMath::IsNearlyEqual(State.ProjectedLengthMm, 0.15, 1.e-6));
	TestTrue(TEXT("0.15 mm updates"), State.bUpdating);
	TestTrue(TEXT("Above boundary rebases"), FMath::IsNearlyEqual(State.DialAngleDeg, 30.0, 1.e-6));
	DialFrame(State, 0.4, 130.0, 0.0, true, true, true, true, 0.015);
	TestTrue(TEXT("Above boundary accumulates"), FMath::IsNearlyEqual(State.DialAngleDeg, 40.0, 1.e-6));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP0ProbeDialTransitionTest, "vBoR.P0Probe.Dial.Transitions", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP0ProbeDialTransitionTest::RunTest(const FString& Parameters)
{
	FP0ProbeDialMeasurement State;
	auto Frame = [&State](double Time, bool bThumb, bool bIndex, bool bValid = true)
	{
		return State.Update(Time, bValid, bThumb, bIndex, true, FVector::ZeroVector, DialVector(0.0), FQuat::Identity, FVector::UpVector, 10.0);
	};
	TestEqual(TEXT("Initial sample is not a transition"), Frame(0.0, true, true).Num(), 0);
	TestEqual(TEXT("Thumb loss and held entry logged"), Frame(0.1, false, true).Num(), 2);
	TestEqual(TEXT("Thumb Low count"), State.ThumbLowCount, 1);
	TestEqual(TEXT("First held entry"), State.HeldCount, 1);
	TestEqual(TEXT("Repeated held sample is silent"), Frame(0.2, false, true).Num(), 0);
	TestEqual(TEXT("Index loss while held logged"), Frame(0.3, false, false).Num(), 1);
	TestEqual(TEXT("Index Low count"), State.IndexLowCount, 1);
	TestEqual(TEXT("Held count is not frame count"), State.HeldCount, 1);
	TestEqual(TEXT("Both recoveries and updating logged"), Frame(0.4, true, true).Num(), 3);
	TestEqual(TEXT("Recovery does not count Thumb loss"), State.ThumbLowCount, 1);
	TestEqual(TEXT("Recovery does not count Index loss"), State.IndexLowCount, 1);
	TestEqual(TEXT("Recovery does not count held entry"), State.HeldCount, 1);
	State.ResetAngles();
	TestEqual(TEXT("Pinch resets Thumb count"), State.ThumbLowCount, 0);
	TestEqual(TEXT("Pinch resets Index count"), State.IndexLowCount, 0);
	TestEqual(TEXT("Pinch resets held count"), State.HeldCount, 0);
	TestEqual(TEXT("Pinch does not invent a state transition"), Frame(0.5, true, true).Num(), 0);
	TestEqual(TEXT("Position loss logs held entry"), Frame(0.6, true, true, false).Num(), 1);
	TestEqual(TEXT("Position loss counted as held"), State.HeldCount, 1);
	State.ResetAngles();
	TestEqual(TEXT("Pinch while held does not repeat entry"), Frame(0.7, true, true, false).Num(), 0);
	TestEqual(TEXT("Still held after reset stays zero"), State.HeldCount, 0);
	Frame(0.8, true, true);
	Frame(0.9, false, false);
	TestEqual(TEXT("New Thumb loss counted after reset"), State.ThumbLowCount, 1);
	TestEqual(TEXT("New Index loss counted after reset"), State.IndexLowCount, 1);
	TestEqual(TEXT("New held entry counted after reset"), State.HeldCount, 1);
	FP0ProbeDialMeasurement InitialLow;
	InitialLow.Update(0.0, true, false, false, true, FVector::ZeroVector, DialVector(0.0), FQuat::Identity, FVector::UpVector, 10.0);
	TestEqual(TEXT("Initial Low is not Thumb loss"), InitialLow.ThumbLowCount, 0);
	TestEqual(TEXT("Initial Low is not Index loss"), InitialLow.IndexLowCount, 0);
	TestEqual(TEXT("Initial held is not entry"), InitialLow.HeldCount, 0);
	return true;
}
#endif
