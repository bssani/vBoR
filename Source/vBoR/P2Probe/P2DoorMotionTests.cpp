#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "../Interaction/VBoRDoorMotion.h"

namespace
{
	FVBoRDoorHand DoorHand(FVector Point, FVector Normal = FVector(0, -1, 0))
	{
		FVBoRDoorHand Hand;
		Hand.bTracked = true;
		for (FVector& P : Hand.Points) { P = Point; }
		Hand.PalmNormal = Normal;
		return Hand;
	}
	FVector DoorPoint(double Angle, FVector Point)
	{
		return FQuat(FVector::UpVector, FMath::DegreesToRadians(Angle)).RotateVector(Point);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP2DoorAsymmetricFacesTest, "vBoR.P2Door.AsymmetricFacePlanes", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP2DoorAsymmetricFacesTest::RunTest(const FString& Parameters)
{
	FVBoRDoorMotion Door;
	Door.FaceMinCm.Y = -20; Door.FaceMaxCm.Y = 5; Door.ThicknessCm = 25;
	Door.bLocked = false; Door.AngleDeg = 30;
	FVBoRDoorHand Hands[2] = {DoorHand(DoorPoint(30, FVector(80,8,50)), DoorPoint(30, FVector(0,-1,0))), {}};
	Door.Advance(Hands, 0.1);
	Hands[0] = DoorHand(DoorPoint(30, FVector(80,0,50)), DoorPoint(30, FVector(0,-1,0)));
	Door.Advance(Hands, 0.1);
	TestTrue(TEXT("Outside +8 approach closes via +5 plane"), Door.AngleDeg < 30);
	TestEqual(TEXT("Outside face selected"), Door.PushFace, 0);
	TestTrue(TEXT("Point follows asymmetric outside plane"), FMath::IsNearlyEqual(Door.ToDoor(Hands[0].Points[0]).Y, 5.0, 0.001));
	Door.Reset(); Door.bLocked = false;
	Hands[0] = DoorHand(FVector(80,-23,50), FVector(0,1,0)); Door.Advance(Hands, 0.1);
	Hands[0] = DoorHand(FVector(80,-12,50), FVector(0,1,0)); Door.Advance(Hands, 0.1);
	TestTrue(TEXT("Inside -23 approach opens via -20 plane"), Door.AngleDeg > 0);
	TestEqual(TEXT("Inside face selected"), Door.PushFace, 1);
	TestTrue(TEXT("Point follows asymmetric inside plane"), FMath::IsNearlyEqual(Door.ToDoor(Hands[0].Points[0]).Y, -20.0, 0.001));
	for (const FVector& Normal : {FVector(0,1,0), FVector(0,-1,0)})
	{
		Door.Reset(); Door.bLocked = false;
		Hands[0] = DoorHand(FVector(80,-8,50), Normal);
		Door.Advance(Hands, 0.1); Door.Advance(Hands, 0.1);
		TestEqual(TEXT("Hand appearing between planes never pushes"), Door.AngleDeg, 0.0);
		TestEqual(TEXT("No push owner between planes"), Door.PushHand, INDEX_NONE);
	}
	Door.Reset(); Door.bLocked = false;
	const int32 WrongPalm = Door.Metrics.Ignored[static_cast<int32>(EVBoRDoorIgnore::PalmDirection)];
	Hands[0] = DoorHand(FVector(80,8,50), FVector(0,1,0)); Door.Advance(Hands, 0.1);
	Hands[0] = DoorHand(FVector(80,4,50), FVector(0,1,0)); Door.Advance(Hands, 0.1);
	TestEqual(TEXT("Exterior approach chooses outside even with misleading palm normal"), Door.Metrics.Ignored[static_cast<int32>(EVBoRDoorIgnore::PalmDirection)], WrongPalm + 1);
	TestEqual(TEXT("Wrong-facing palm cannot move door"), Door.AngleDeg, 0.0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP2DoorGrabTest, "vBoR.P2Door.GrabLatchStop", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP2DoorGrabTest::RunTest(const FString& Parameters)
{
	FVBoRDoorMotion Door;
	FVBoRDoorHand Hands[2] = {DoorHand(FVector(100, 10, 60)), {}};
	Door.Advance(Hands, 0.1);
	TestEqual(TEXT("E1 locked ignores outside push"), Door.AngleDeg, 0.0);
	TestEqual(TEXT("Hand outside face has no locked contact"), Door.Metrics.Ignored[0], 0);
	TestTrue(TEXT("E2 outside handle unlocks"), Door.BeginGrab(0, 0, Hands[0]));
	TestFalse(TEXT("Unlocked"), Door.bLocked);
	TestEqual(TEXT("Unlock once"), Door.Metrics.Unlocks, 1);
	TestEqual(TEXT("No snap on grab"), Door.AngleDeg, 0.0);
	for (int32 I = 0; I < 5; ++I) { Door.Advance(Hands, 0.1); }
	TestFalse(TEXT("E13 not relatched at zero"), Door.bLocked);
	Hands[0].Points[0] += FVector(0, 20, 0);
	Door.Advance(Hands, 0.1);
	const double Expected = FMath::RadiansToDegrees(FMath::Atan2(30.0, 100.0) - FMath::Atan2(10.0, 100.0));
	TestTrue(TEXT("E3 projected handle relation follows 20 cm pull"), FMath::Abs(Door.AngleDeg - Expected) < 0.5);
	const double Stopped = Door.AngleDeg;
	Door.Advance(Hands, 0.1);
	TestEqual(TEXT("E4 stopped hand"), Door.AngleDeg, Stopped);
	TestTrue(TEXT("Real release"), Door.ReleaseGrab(true));
	Door.Advance(Hands, 0.1);
	TestEqual(TEXT("E4 release has no inertia"), Door.AngleDeg, Stopped);
	TestTrue(TEXT("Grab inside handle"), Door.BeginGrab(1, 1, DoorHand(FVector(80, -10, 50))));
	Hands[1] = DoorHand(DoorPoint(-Stopped, FVector(80, -10, 50)));
	Door.Advance(Hands, 0.1);
	TestTrue(TEXT("E12 latch armed above five, closes to locked zero"), Door.bLocked && Door.AngleDeg == 0);
	TestEqual(TEXT("Latch once"), Door.Metrics.Latches, 1);
	TestEqual(TEXT("Latch terminates grab"), Door.GrabHand, INDEX_NONE);
	Door.Advance(Hands, 0.1);
	TestEqual(TEXT("Latch sound/event count remains one"), Door.Metrics.Latches, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP2DoorPushTest, "vBoR.P2Door.PushGeometryAndWithdrawal", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP2DoorPushTest::RunTest(const FString& Parameters)
{
	FVBoRDoorMotion Door;
	Door.bLocked = false;
	FVBoRDoorHand Hands[2] = {DoorHand(FVector(80, -8, 50), FVector(0, 1, 0)), {}};
	Door.Advance(Hands, 0.1);
	Hands[0].Points[0] = FVector(80, 15, 50);
	Door.Advance(Hands, 0.1);
	TestTrue(TEXT("E5 inner face opens"), Door.AngleDeg > 0);
	TestTrue(TEXT("Deepest of five points placed on inner face"), FMath::Abs(Door.ToDoor(Hands[0].Points[0]).Y + 5) < 0.01);
	TestEqual(TEXT("One inside push start"), Door.Metrics.PushStarts[0][1], 1);
	const double Stopped = Door.AngleDeg;
	Hands[0] = DoorHand(DoorPoint(Stopped, FVector(80, -20, 50)), DoorPoint(Stopped, FVector(0, 1, 0)));
	Door.Advance(Hands, 0.1);
	Door.Advance(Hands, 0.1);
	TestEqual(TEXT("E6 withdrawal never pulls"), Door.AngleDeg, Stopped);
	TestEqual(TEXT("Push ends"), Door.PushHand, INDEX_NONE);
	Hands[0] = DoorHand(DoorPoint(Stopped, FVector(80, 8, 50)), DoorPoint(Stopped, FVector(0, -1, 0)));
	Door.Advance(Hands, 0.1);
	Hands[0] = DoorHand(DoorPoint(Stopped, FVector(80, -5, 50)), DoorPoint(Stopped, FVector(0, -1, 0)));
	Door.Advance(Hands, 0.1);
	TestTrue(TEXT("Outside pushes only closing direction"), Door.AngleDeg < Stopped);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP2DoorPushGatesTest, "vBoR.P2Door.PushGates", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP2DoorPushGatesTest::RunTest(const FString& Parameters)
{
	FVBoRDoorMotion Door;
	Door.bLocked = false;
	FVBoRDoorHand Hands[2] = {DoorHand(FVector(80, 0, 50), FVector(0, 1, 0)), {}};
	Door.Advance(Hands, 0.1);
	Door.Advance(Hands, 0.1);
	TestEqual(TEXT("E8 appearing behind does not start"), Door.AngleDeg, 0.0);
	TestEqual(TEXT("Behind contact is one episode"), Door.Metrics.Ignored[2], 1);
	Hands[0] = DoorHand(FVector(80, -10, 50), FVector(1, 0, 0));
	Door.Advance(Hands, 0.1);
	Hands[0].Points[0] = FVector(80, 0, 50);
	Door.Advance(Hands, 0.1);
	TestEqual(TEXT("E7 hand edge"), Door.AngleDeg, 0.0);
	TestEqual(TEXT("Direction contact is one episode"), Door.Metrics.Ignored[1], 1);
	Hands[0] = DoorHand(FVector(5, -8, 50), FVector(0, 1, 0));
	Door.Advance(Hands, 0.1);
	Hands[0] = DoorHand(FVector(5, 0, 50), FVector(0, 1, 0));
	Door.Advance(Hands, 0.1);
	TestEqual(TEXT("E9 near hinge"), Door.AngleDeg, 0.0);
	TestEqual(TEXT("Near axis contact is one episode"), Door.Metrics.Ignored[3], 1);
	Hands[0] = DoorHand(FVector(113, -8, 50), FVector(0, 1, 0));
	Door.Advance(Hands, 0.1);
	Hands[0] = DoorHand(FVector(113, 0, 50), FVector(0, 1, 0));
	Door.Advance(Hands, 0.1);
	TestEqual(TEXT("Edge band counted once"), Door.Metrics.Ignored[4], 1);
	Hands[0] = DoorHand(FVector(80, 0, 110), FVector(0, 1, 0));
	Door.Advance(Hands, 0.1);
	TestEqual(TEXT("Outside height"), Door.AngleDeg, 0.0);
	Hands[0].bTracked = false;
	Door.Advance(Hands, 0.1);
	TestEqual(TEXT("Tracking loss after edge contact counted once"), Door.Metrics.Ignored[5], 1);
	Hands[0] = DoorHand(FVector(80, -8, 50), FVector(0, 1, 0));
	Door.Advance(Hands, 0.1);
	Hands[0] = DoorHand(FVector(80, 0, 50), FVector(0, 1, 0));
	Door.Advance(Hands, 0.1);
	TestTrue(TEXT("E8 can start after withdrawal"), Door.AngleDeg > 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP2DoorRateTest, "vBoR.P2Door.RateAndMaximum", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP2DoorRateTest::RunTest(const FString& Parameters)
{
	double Results[2] = {};
	for (int32 Mode = 0; Mode < 2; ++Mode)
	{
		const double Dt = 1.0 / (Mode == 0 ? 15 : 90);
		FVBoRDoorMotion Door;
		Door.bLocked = false;
		FVBoRDoorHand Hands[2] = {DoorHand(FVector(80, -8, 50), FVector(0, 1, 0)), {}};
		Door.Advance(Hands, Dt);
		Hands[0] = DoorHand(FVector(80, 32, 50), FVector(0, 1, 0));
		for (double T = 0; T < 0.5 - SMALL_NUMBER; T += Dt)
		{
			const double Step = FMath::Min(Dt, 0.5 - T);
			const double Before = Door.AngleDeg;
			Door.Advance(Hands, Step);
			TestTrue(TEXT("E10 per second cap"), Door.AngleDeg - Before <= 180 * Step + 0.0001);
		}
		Results[Mode] = Door.AngleDeg;
		TestTrue(TEXT("E8b pending push continues without crossing again"), FMath::Abs(Door.ToDoor(Hands[0].Points[0]).Y + 5) < 0.01);
		const double Stop = Door.AngleDeg;
		Door.Advance(Hands, Dt);
		TestEqual(TEXT("Reached stationary hand, no overshoot"), Door.AngleDeg, Stop);
		TestTrue(TEXT("Rate limit measured"), Door.Metrics.RateLimitedFrames > 0);
		Door.Reset();
		Hands[0] = DoorHand(FVector(80, 8, 50));
		Door.BeginGrab(0, 0, Hands[0]);
		Hands[0] = DoorHand(DoorPoint(110, FVector(80, 8, 50)));
		for (int32 I = 0; I < 90; ++I) { Door.Advance(Hands, Dt); }
		TestEqual(TEXT("E11 maximum"), Door.AngleDeg, 70.0);
	}
	TestTrue(TEXT("15/90 fps equivalent at 0.5 s"), FMath::Abs(Results[0] - Results[1]) < 0.5);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP2DoorPauseTest, "vBoR.P2Door.TrackingAndOwnership", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP2DoorPauseTest::RunTest(const FString& Parameters)
{
	FVBoRDoorMotion Door;
	FVBoRDoorHand Hands[2] = {DoorHand(FVector(80, 8, 50)), DoorHand(FVector(60, -8, 50), FVector(0, 1, 0))};
	Door.BeginGrab(0, 0, Hands[0]);
	TestFalse(TEXT("E17 second hand cannot take ownership"), Door.BeginGrab(1, 1, Hands[1]));
	Hands[1] = DoorHand(FVector(60, 20, 50), FVector(0, 1, 0));
	Door.Advance(Hands, 0.1);
	TestEqual(TEXT("E16 grabbed hand wins over push"), Door.AngleDeg, 0.0);
	Hands[0].bTracked = false;
	Door.Advance(Hands, 0.1);
	TestTrue(TEXT("E14 pause"), Door.bPaused);
	TestFalse(TEXT("E14 paused grab rejects other hand"), Door.BeginGrab(1, 1, Hands[1]));
	TestFalse(TEXT("E14 SDK release ignored"), Door.ReleaseGrab(false));
	TestEqual(TEXT("Ignored release counted"), Door.Metrics.IgnoredReleases, 1);
	Hands[0] = DoorHand(FVector(80, 48, 50));
	Door.Advance(Hands, 0.1);
	TestEqual(TEXT("Recovery rebase no jump"), Door.AngleDeg, 0.0);
	TestTrue(TEXT("Recovery distance excludes lost movement"), FMath::Abs(Door.Metrics.RecoveryDistanceCm - 40) < 0.001);
	Door.Advance(Hands, 0.1);
	TestEqual(TEXT("Rebased stationary hand"), Door.AngleDeg, 0.0);
	Hands[0].Points[0] += FVector(0, 10, 0);
	Door.Advance(Hands, 0.1);
	TestTrue(TEXT("New movement accepted"), Door.AngleDeg > 0);
	const int32 Grabs = Door.Metrics.Grabs[0][0];
	Door.Reset();
	TestTrue(TEXT("E18 reset closed locked, no sessions"), Door.bLocked && Door.AngleDeg == 0 && Door.GrabHand == INDEX_NONE && Door.PushHand == INDEX_NONE);
	TestEqual(TEXT("Reset keeps cumulative metrics"), Door.Metrics.Grabs[0][0], Grabs);
	Door.bLocked = false;
	Hands[0] = DoorHand(FVector(80, -8, 50), FVector(0, 1, 0)); Hands[1] = {};
	Door.Advance(Hands, 0.1);
	Hands[0] = DoorHand(FVector(80, 0, 50), FVector(0, 1, 0));
	Door.Advance(Hands, 0.1);
	const double Stop = Door.AngleDeg;
	Hands[0].bTracked = false;
	Door.Advance(Hands, 0.1);
	TestFalse(TEXT("E15 push loss is not a grab pause"), Door.bPaused);
	TestEqual(TEXT("E15 push loss measured separately"), Door.Metrics.PushTrackingLosses, 1);
	Hands[0] = DoorHand(FVector(80, 40, 50), FVector(0, 1, 0));
	Door.Advance(Hands, 0.1);
	Door.Advance(Hands, 0.1);
	TestEqual(TEXT("E15 recovery behind does not resume push"), Door.AngleDeg, Stop);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP2DoorIgnoreEpisodesTest, "vBoR.P2Door.IgnoreContactEpisodes", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP2DoorIgnoreEpisodesTest::RunTest(const FString& Parameters)
{
	FVBoRDoorMotion Door;
	FVBoRDoorHand Hands[2] = {DoorHand(FVector(80, 35, 50)), DoorHand(FVector(80, -35, 50), FVector(0, 1, 0))};
	for (int32 I = 0; I < 60; ++I) { Door.Advance(Hands, 1.0 / 60); }
	Hands[0] = DoorHand(FVector(160, 0, 50), FVector(0, 0, -1)); Hands[1] = {};
	for (int32 I = 0; I < 60; ++I) { Door.Advance(Hands, 1.0 / 60); }
	for (int32 Count : Door.Metrics.Ignored) { TestEqual(TEXT("Distant and untracked hands produce no ignored contacts"), Count, 0); }
	Hands[0] = DoorHand(FVector(80, 8, 50)); Door.Advance(Hands, 0.1);
	Hands[0] = DoorHand(FVector(80, 0, 50));
	for (int32 I = 0; I < 60; ++I) { Door.Advance(Hands, 1.0 / 60); }
	TestEqual(TEXT("Locked push for one second is one episode"), Door.Metrics.Ignored[0], 1);
	Door.Reset(); Door.bLocked = false;
	for (int32 Episode = 1; Episode <= 2; ++Episode)
	{
		Hands[0] = DoorHand(FVector(80, -8, 50), FVector(0, -1, 0)); Door.Advance(Hands, 0.1);
		Hands[0] = DoorHand(FVector(80, 0, 50), FVector(0, -1, 0));
		for (int32 I = 0; I < 60; ++I) { Door.Advance(Hands, 1.0 / 60); }
		TestEqual(TEXT("Back of hand counted once until withdrawal"), Door.Metrics.Ignored[1], Episode);
	}
	Hands[0].bTracked = false;
	for (int32 I = 0; I < 60; ++I) { Door.Advance(Hands, 1.0 / 60); }
	TestEqual(TEXT("Reason change to Low is one episode"), Door.Metrics.Ignored[5], 1);
	Hands[0] = DoorHand(FVector(113, -8, 50), FVector(0, 1, 0)); Door.Advance(Hands, 0.1);
	Hands[0] = DoorHand(FVector(113, 0, 50), FVector(0, 1, 0));
	for (int32 I = 0; I < 60; ++I) { Door.Advance(Hands, 1.0 / 60); }
	TestEqual(TEXT("Three cm beyond edge is one outside contact"), Door.Metrics.Ignored[4], 1);
	Hands[0] = DoorHand(FVector(140, -8, 50), FVector(0, 1, 0)); Door.Advance(Hands, 0.1);
	Hands[0] = DoorHand(FVector(140, 0, 50), FVector(0, 1, 0)); Door.Advance(Hands, 0.1);
	TestEqual(TEXT("Thirty cm beyond edge adds no contact"), Door.Metrics.Ignored[4], 1);
	TestEqual(TEXT("Rejected contacts never move door"), Door.AngleDeg, 0.0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP2DoorPushLossTest, "vBoR.P2Door.PushLossAllowsOtherHand", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP2DoorPushLossTest::RunTest(const FString& Parameters)
{
	for (int32 Mode = 0; Mode < 2; ++Mode)
	{
		FVBoRDoorMotion Door; Door.bLocked = false;
		FVBoRDoorHand Hands[2] = {{}, DoorHand(FVector(80, -8, 50), FVector(0, 1, 0))};
		Door.Advance(Hands, 0.1);
		Hands[1] = DoorHand(FVector(80, 10, 50), FVector(0, 1, 0));
		Door.Advance(Hands, 0.1);
		double Before = Door.AngleDeg;
		Hands[0] = DoorHand(DoorPoint(Before, FVector(70, -8, 50)), DoorPoint(Before, FVector(0, 1, 0)));
		// Push still active before loss (distinct from ending on face arrival, P2 E15b)
		Hands[1] = DoorHand(FVector(80, 40, 50), FVector(0, 1, 0));
		Door.Advance(Hands, 0.01);
		Before = Door.AngleDeg;
		TestEqual(TEXT("Right hand is still pushing before loss"), Door.PushHand, 1);
		Hands[1].bTracked = false;
		Door.Advance(Hands, 0.1);
		TestFalse(TEXT("E15b losing right push never pauses whole door"), Door.bPaused);
		TestEqual(TEXT("Push loss once"), Door.Metrics.PushTrackingLosses, 1);
		TestEqual(TEXT("Push loss is not counted as grab pause"), Door.Metrics.Pauses, 0);
		TestEqual(TEXT("Push loss holds angle"), Door.AngleDeg, Before);
		if (Mode == 0)
		{
			Hands[0] = DoorHand(DoorPoint(Before, FVector(70, 10, 50)), DoorPoint(Before, FVector(0, 1, 0)));
			Door.Advance(Hands, 0.1);
			TestTrue(TEXT("E15b next frame left push moves door"), Door.AngleDeg > Before);
		}
		else { TestTrue(TEXT("E15b next frame left grab accepted"), Door.BeginGrab(0, 1, Hands[0])); }
		FVBoRDoorMetrics Baseline; Baseline.PushTrackingLosses = 0;
		TestEqual(TEXT("Trial includes push tracking loss"), Door.Metrics.Since(Baseline).PushTrackingLosses, 1);
	}
	return true;
}
#endif
