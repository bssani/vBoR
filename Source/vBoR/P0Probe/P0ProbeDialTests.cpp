#include "P0ProbeDialMeasurement.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"

namespace
{
	// 축 방향 성분 3 cm 는 투영에서 빠져야 함 (P0-a §2)
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
	// 각 조건을 하나씩 깨뜨림 (누락된 gate 를 잡기 위함, P0-a §4-6)
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

	// 다이얼 축은 월드 Z 에 한정되지 않음. 비정규화 X 축과 swing 을 함께 확인 (P0-a §2)
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
#endif
