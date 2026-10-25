#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "../Interaction/VBoRDoorMotion.h"
#include "../Interaction/VBoRPoweredPanel.h"
#include "P2bTestFixture.h"
#include "Engine/World.h"
#include "UObject/UnrealType.h"

namespace
{
	constexpr double P5zaDt = 1.0 / 90;
	FVector P5zaPoint(double Angle, FVector Point)
	{
		return FQuat(FVector::UpVector, FMath::DegreesToRadians(Angle)).RotateVector(Point);
	}
	FVBoRDoorHand P5zaHand(double Angle, FVector Point, FVector Normal = FVector(0, 1, 0))
	{
		FVBoRDoorHand Hand;
		Hand.bTracked = true;
		for (FVector& P : Hand.Points) { P = P5zaPoint(Angle, Point); }
		Hand.PalmNormal = P5zaPoint(Angle, Normal);
		return Hand;
	}
	FVBoRDoorMotion P5zaDoor(double Angle = 20)
	{
		FVBoRDoorMotion Door;
		Door.bUseLatch = Door.bLocked = false;
		Door.bCoast = true;
		Door.AngleDeg = Angle;
		return Door;
	}
	void P5zaWait(FVBoRDoorMotion& Door, FVBoRDoorHand (&Hands)[2], double Seconds, double Dt = P5zaDt)
	{
		for (double Time = 0; Time < Seconds - 1.e-8;)
		{
			const double Step = FMath::Min(Dt, Seconds - Time);
			Door.Advance(Hands, Step);
			Time += Step;
		}
	}
	void P5zaPush(FVBoRDoorMotion& Door, FVBoRDoorHand (&Hands)[2], double Speed = 60,
		double Seconds = 0.5, double Dt = P5zaDt)
	{
		const double Start = Door.AngleDeg;
		const double Sign = Speed > 0 ? 1.0 : -1.0;
		const FVector Normal(0, Sign, 0);
		Hands[0] = P5zaHand(Start, FVector(80, -Sign * 8, 50), Normal);
		Door.Advance(Hands, Dt);
		for (double Time = 0; Time < Seconds - 1.e-8;)
		{
			const double Step = FMath::Min(Dt, Seconds - Time);
			Time += Step;
			// Fixed angle trajectory, face thickness included (0.05 deg lead, P5za section 4)
			Hands[0] = P5zaHand(Start + Speed * Time + Sign * 0.05, FVector(80, -Sign * 5, 50), Normal);
			Door.Advance(Hands, Step);
		}
	}
	void P5zaGrab(FVBoRDoorMotion& Door, FVBoRDoorHand (&Hands)[2])
	{
		const double Start = Door.AngleDeg;
		Hands[0] = P5zaHand(Start, FVector(80, 0, 50));
		Door.BeginGrab(0, 0, Hands[0]);
		for (int32 I = 1; I <= 45; ++I)
		{
			Hands[0] = P5zaHand(Start + 60 * I * P5zaDt, FVector(80, 0, 50));
			Door.Advance(Hands, P5zaDt);
		}
	}
	void P5zaNear(FAutomationTestBase& Test, const TCHAR* Label, double Actual, double Expected, double Tolerance = 1.5)
	{
		Test.TestTrue(FString::Printf(TEXT("%s: %.4f expected %.4f +/- %.4f"), Label, Actual, Expected, Tolerance),
			FMath::Abs(Actual - Expected) <= Tolerance);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5zaOff, "vBoR.P5za.E1.Disabled", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5zaOff::RunTest(const FString& Parameters)
{
	FVBoRDoorMotion Door, Baseline;
	Door.bCoast = false; Door.CoastDecelerationDegPerSecond2 = 360;
	Door.bUseLatch = Baseline.bUseLatch = false;
	Door.bLocked = Baseline.bLocked = false;
	Door.AngleDeg = Baseline.AngleDeg = 20;
	FVBoRDoorHand Hands[2] = {};
	Hands[0] = P5zaHand(20, FVector(80, -8, 50));
	for (int32 I = 0; I < 271; ++I)
	{
		if (I > 0 && I <= 45) { Hands[0] = P5zaHand(20 + 60 * I * P5zaDt + 0.05, FVector(80, -5, 50)); }
		Door.Advance(Hands, P5zaDt); Baseline.Advance(Hands, P5zaDt);
		TestEqual(TEXT("Off angle"), Door.AngleDeg, Baseline.AngleDeg);
		TestEqual(TEXT("Off hand"), Door.PushHand, Baseline.PushHand);
		TestEqual(TEXT("Off starts"), Door.Metrics.PushStarts[0][1], Baseline.Metrics.PushStarts[0][1]);
		TestEqual(TEXT("Off distance"), Door.Metrics.PushedDeg, Baseline.Metrics.PushedDeg);
		if (I == 45)
		{
			const double Stopped = Door.AngleDeg;
			auto Released = Door;
			FVBoRDoorHand None[2] = {};
			P5zaWait(Released, None, 1);
			TestEqual(TEXT("Off stops immediately"), Released.AngleDeg, Stopped);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5zaPushRelease, "vBoR.P5za.E2.PushRelease", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5zaPushRelease::RunTest(const FString& Parameters)
{
	auto Door = P5zaDoor(); FVBoRDoorHand Hands[2] = {};
	P5zaPush(Door, Hands);
	TestEqual(TEXT("Inside face drives before stop"), Door.PushFace, 1);
	const double Before = Door.AngleDeg;
	P5zaWait(Door, Hands, 0.5);
	TestTrue(TEXT("Keeps opening"), Door.AngleDeg > Before);
	TestEqual(TEXT("Palm cannot hold"), Door.PushHand, INDEX_NONE);
	P5zaNear(*this, TEXT("Stopping distance deg"), Door.AngleDeg - Before, 10);
	const double Stopped = Door.AngleDeg;
	P5zaWait(Door, Hands, 2);
	TestEqual(TEXT("Stays stopped"), Door.AngleDeg, Stopped);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5zaSpeed, "vBoR.P5za.E3.SpeedDistance", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5zaSpeed::RunTest(const FString& Parameters)
{
	double PreviousDistance = 0;
	for (double Speed : {30.0, 60.0, 90.0})
	{
		// Start at 0 for 90 deg/s (avoid 70 deg end stop masking free stopping distance)
		auto Door = P5zaDoor(Speed == 90 ? 0 : 20); FVBoRDoorHand Hands[2] = {};
		P5zaPush(Door, Hands, Speed);
		const double Before = Door.AngleDeg;
		P5zaWait(Door, Hands, 2);
		const double Distance = Door.AngleDeg - Before;
		P5zaNear(*this, TEXT("Speed squared distance deg"), Distance, Speed * Speed / 360);
		TestTrue(TEXT("Faster travels farther"), Distance > PreviousDistance);
		PreviousDistance = Distance;
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5zaDeceleration, "vBoR.P5za.E4.Deceleration", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5zaDeceleration::RunTest(const FString& Parameters)
{
	auto Door = P5zaDoor(); Door.CoastDecelerationDegPerSecond2 = 360;
	FVBoRDoorHand Hands[2] = {}; P5zaPush(Door, Hands);
	const double Before = Door.AngleDeg; P5zaWait(Door, Hands, 1);
	P5zaNear(*this, TEXT("Double deceleration halves distance deg"), Door.AngleDeg - Before, 5);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5zaLimit, "vBoR.P5za.E5.OpenLimit", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5zaLimit::RunTest(const FString& Parameters)
{
	auto Door = P5zaDoor(55); FVBoRDoorHand Hands[2] = {};
	P5zaPush(Door, Hands, 90, 0.1);
	const double Before = Door.AngleDeg;
	TestTrue(TEXT("Release before limit"), Before < 70);
	for (int32 I = 0; I < 180; ++I)
	{
		const double Previous = Door.AngleDeg; Door.Advance(Hands, P5zaDt);
		TestTrue(TEXT("No overshoot or bounce"), Door.AngleDeg >= Previous && Door.AngleDeg <= 70);
	}
	TestEqual(TEXT("Open end stop"), Door.AngleDeg, 70.0);
	TestEqual(TEXT("Open end speed"), Door.CoastSpeed, 0.0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5zaLatch, "vBoR.P5za.E6.CoastLatch", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5zaLatch::RunTest(const FString& Parameters)
{
	auto Door = P5zaDoor(30); Door.bUseLatch = true;
	FVBoRDoorHand Hands[2] = {}; P5zaPush(Door, Hands, -90, 0.2);
	TestTrue(TEXT("Armed and still open when hand stops"), Door.bLatchArmed && Door.AngleDeg > 2 && !Door.bLocked);
	P5zaWait(Door, Hands, 2);
	TestEqual(TEXT("Closed"), Door.AngleDeg, 0.0);
	TestTrue(TEXT("Locked by coast"), Door.bLocked);
	TestEqual(TEXT("One latch"), Door.Metrics.Latches, 1);
	TestEqual(TEXT("Locked speed"), Door.CoastSpeed, 0.0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5zaNoLatch, "vBoR.P5za.E7.NoLatchEnd", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5zaNoLatch::RunTest(const FString& Parameters)
{
	auto Door = P5zaDoor(30); FVBoRDoorHand Hands[2] = {}; P5zaPush(Door, Hands, -90, 0.2);
	for (int32 I = 0; I < 180; ++I)
	{
		const double Previous = Door.AngleDeg; Door.Advance(Hands, P5zaDt);
		TestTrue(TEXT("No negative angle or bounce"), Door.AngleDeg >= 0 && Door.AngleDeg <= Previous);
	}
	TestEqual(TEXT("Closed without latch"), Door.AngleDeg, 0.0);
	TestEqual(TEXT("No latch count"), Door.Metrics.Latches, 0);
	TestEqual(TEXT("Closed speed"), Door.CoastSpeed, 0.0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5zaPushLoss, "vBoR.P5za.E8.PushTrackingLoss", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5zaPushLoss::RunTest(const FString& Parameters)
{
	auto Door = P5zaDoor(); FVBoRDoorHand Hands[2] = {}; P5zaPush(Door, Hands);
	const double Before = Door.AngleDeg; Hands[0].bTracked = false;
	P5zaWait(Door, Hands, 0.5); TestEqual(TEXT("Loss stops immediately"), Door.AngleDeg, Before);
	Hands[0].bTracked = true; P5zaWait(Door, Hands, 1);
	TestEqual(TEXT("Tracking alone cannot restart"), Door.AngleDeg, Before);
	TestEqual(TEXT("Push loss counted once"), Door.Metrics.PushTrackingLosses, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5zaFreeLoss, "vBoR.P5za.E9.FreeCoastTrackingLoss", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5zaFreeLoss::RunTest(const FString& Parameters)
{
	auto Door = P5zaDoor(); FVBoRDoorHand Hands[2] = {}; P5zaPush(Door, Hands);
	const double Before = Door.AngleDeg;
	Hands[0] = P5zaHand(Door.AngleDeg, FVector(80, -20, 50)); Door.Advance(Hands, P5zaDt);
	TestEqual(TEXT("Hand has separated"), Door.PushHand, INDEX_NONE);
	TestTrue(TEXT("Still coasting"), Door.CoastSpeed > 0);
	auto Control = Door; FVBoRDoorHand ControlHands[2] = {Hands[0], Hands[1]};
	Hands[0].bTracked = false;
	P5zaWait(Door, Hands, 1); P5zaWait(Control, ControlHands, 1);
	TestEqual(TEXT("Free coast ignores tracking loss"), Door.AngleDeg, Control.AngleDeg);
	P5zaNear(*this, TEXT("Free stopping distance deg"), Door.AngleDeg - Before, 10);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5zaGrabRelease, "vBoR.P5za.E10.GrabRelease", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5zaGrabRelease::RunTest(const FString& Parameters)
{
	auto Door = P5zaDoor(); FVBoRDoorHand Hands[2] = {}; P5zaGrab(Door, Hands);
	P5zaNear(*this, TEXT("Grab follows exactly deg"), Door.AngleDeg, 50, 1.e-6);
	TestTrue(TEXT("Tracked release"), Door.ReleaseGrab(true));
	const double Before = Door.AngleDeg; P5zaWait(Door, Hands, 1);
	P5zaNear(*this, TEXT("Grab stopping distance deg"), Door.AngleDeg - Before, 10);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5zaGrabLoss, "vBoR.P5za.E11.GrabLossRecovery", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5zaGrabLoss::RunTest(const FString& Parameters)
{
	auto Door = P5zaDoor(); FVBoRDoorHand Hands[2] = {}; P5zaGrab(Door, Hands);
	const double Before = Door.AngleDeg; Hands[0].bTracked = false;
	Door.Advance(Hands, P5zaDt);
	TestFalse(TEXT("Untracked release ignored"), Door.ReleaseGrab(false));
	P5zaWait(Door, Hands, 0.5); TestEqual(TEXT("Pause holds angle"), Door.AngleDeg, Before);
	for (FVector& P : Hands[0].Points) { P += FVector(0, 15, 0); }
	Hands[0].bTracked = true; Door.Advance(Hands, P5zaDt);
	TestEqual(TEXT("Recovery rebases angle"), Door.AngleDeg, Before);
	P5zaNear(*this, TEXT("Recovery distance cm"), Door.Metrics.RecoveryDistanceCm, 15, 1.e-6);
	P5zaWait(Door, Hands, 0.5); Door.ReleaseGrab(true); P5zaWait(Door, Hands, 1);
	TestEqual(TEXT("Recovery adds no speed"), Door.AngleDeg, Before);
	// Ignored release can precede the tracking-loss Advance (P5za D6)
	Door = P5zaDoor(); Hands[0] = FVBoRDoorHand(); P5zaGrab(Door, Hands);
	const double OtherBefore = Door.AngleDeg;
	Door.ReleaseGrab(false); Door.Advance(Hands, P5zaDt); Door.ReleaseGrab(true); P5zaWait(Door, Hands, 1);
	TestEqual(TEXT("Ignored release clears speed immediately"), Door.AngleDeg, OtherBefore);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5zaFrameRate, "vBoR.P5za.E12.FrameRate", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5zaFrameRate::RunTest(const FString& Parameters)
{
	double OpenMin = 1000, OpenMax = -1000, CloseMin = 1000, CloseMax = -1000;
	for (double Dt : {1.0 / 15, 1.0 / 45, P5zaDt})
	{
		auto Door = P5zaDoor(); FVBoRDoorHand Hands[2] = {}; P5zaPush(Door, Hands, 60, 0.5, Dt);
		P5zaWait(Door, Hands, 2, Dt);
		OpenMin = FMath::Min(OpenMin, Door.AngleDeg); OpenMax = FMath::Max(OpenMax, Door.AngleDeg);
		Door = P5zaDoor(30); Door.bUseLatch = true; Hands[0] = FVBoRDoorHand();
		P5zaPush(Door, Hands, -90, 0.2, Dt); P5zaWait(Door, Hands, 2, Dt);
		TestTrue(TEXT("Each frame rate latches"), Door.bLocked && Door.Metrics.Latches == 1);
		CloseMin = FMath::Min(CloseMin, Door.AngleDeg); CloseMax = FMath::Max(CloseMax, Door.AngleDeg);
	}
	TestTrue(TEXT("Opening spread <= 1.5 deg"), OpenMax - OpenMin <= 1.5);
	TestTrue(TEXT("Closing spread <= 1.5 deg"), CloseMax - CloseMin <= 1.5);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5zaJitter, "vBoR.P5za.E13.Jitter", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5zaJitter::RunTest(const FString& Parameters)
{
	auto Door = P5zaDoor(30); FVBoRDoorHand Hands[2] = {};
	Hands[0] = P5zaHand(30, FVector(80, -8, 50)); Door.Advance(Hands, P5zaDt);
	for (int32 I = 0; I < 90; ++I)
	{
		Hands[0] = P5zaHand(30, FVector(80, -5 + (I % 2 == 0 ? 0.3 : -0.3), 50));
		Door.Advance(Hands, P5zaDt);
	}
	TestTrue(TEXT("Jitter included accepted push"), Door.Metrics.PushStarts[0][1] > 0);
	const double Before = Door.AngleDeg;
	Hands[0] = P5zaHand(Door.AngleDeg, FVector(80, -25, 50)); P5zaWait(Door, Hands, 1);
	TestTrue(TEXT("Jitter stopping distance <= 1 deg"), Door.AngleDeg - Before <= 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5zaOpposite, "vBoR.P5za.E14.OppositePalm", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5zaOpposite::RunTest(const FString& Parameters)
{
	auto Door = P5zaDoor(); FVBoRDoorHand Hands[2] = {}; P5zaPush(Door, Hands);
	Hands[0] = P5zaHand(Door.AngleDeg, FVector(80, -20, 50)); Door.Advance(Hands, P5zaDt);
	Hands[1] = P5zaHand(Door.AngleDeg, FVector(80, 8, 50), FVector(0, -1, 0)); Door.Advance(Hands, P5zaDt);
	const double Before = Door.AngleDeg;
	// 1 cm penetration stays within 2 deg/frame cap (isolate momentum block, P5za section 4)
	Hands[1] = P5zaHand(Door.AngleDeg, FVector(80, 4, 50), FVector(0, -1, 0)); Door.Advance(Hands, P5zaDt);
	TestEqual(TEXT("Opposite palm accepted"), Door.PushFace, 0);
	TestTrue(TEXT("Opposite palm stops opening"), Door.AngleDeg <= Before);
	TestTrue(TEXT("Blocker target reached within one frame"), Before - Door.AngleDeg < Door.MaxSpeedDegPerSecond * P5zaDt);
	TestEqual(TEXT("Opposite speed cleared"), Door.CoastSpeed, 0.0);
	const double Stopped = Door.AngleDeg; P5zaWait(Door, Hands, 0.5);
	TestEqual(TEXT("Stationary blocker holds"), Door.AngleDeg, Stopped);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5zaRegrab, "vBoR.P5za.E15.Regrab", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5zaRegrab::RunTest(const FString& Parameters)
{
	auto Door = P5zaDoor(); FVBoRDoorHand Hands[2] = {}; P5zaPush(Door, Hands);
	Hands[0] = P5zaHand(Door.AngleDeg, FVector(80, -20, 50)); Door.Advance(Hands, P5zaDt);
	TestTrue(TEXT("Coasting before grab"), Door.CoastSpeed > 0);
	Hands[0] = P5zaHand(Door.AngleDeg, FVector(80, 0, 50));
	TestTrue(TEXT("Grab accepted"), Door.BeginGrab(0, 0, Hands[0]));
	const double Before = Door.AngleDeg; P5zaWait(Door, Hands, 0.5);
	TestEqual(TEXT("Grab stops free coast"), Door.AngleDeg, Before);
	TestEqual(TEXT("No residual speed"), Door.CoastSpeed, 0.0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5zaReset, "vBoR.P5za.E16.Reset", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5zaReset::RunTest(const FString& Parameters)
{
	auto Door = P5zaDoor(); FVBoRDoorHand Hands[2] = {}; P5zaPush(Door, Hands);
	Hands[0] = P5zaHand(Door.AngleDeg, FVector(80, -20, 50)); Door.Advance(Hands, P5zaDt);
	Door.Reset(); Door.bLocked = false; Door.AngleDeg = 30; Hands[0] = FVBoRDoorHand();
	P5zaWait(Door, Hands, 1); TestEqual(TEXT("Reset clears coast"), Door.AngleDeg, 30.0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5zaDistance, "vBoR.P5za.E17.PushMetrics", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5zaDistance::RunTest(const FString& Parameters)
{
	auto Door = P5zaDoor(); FVBoRDoorHand Hands[2] = {}; P5zaPush(Door, Hands);
	const double Before = Door.AngleDeg, Pushed = Door.Metrics.PushedDeg;
	P5zaNear(*this, TEXT("Driven distance deg"), Pushed, Before - 20, 1.e-6);
	P5zaWait(Door, Hands, 1);
	TestTrue(TEXT("Coast actually moved"), Door.AngleDeg > Before);
	TestEqual(TEXT("Coast excluded from pushed distance"), Door.Metrics.PushedDeg, Pushed);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5zaProperties, "vBoR.P5za.E18.DoorPropertiesOnly", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5zaProperties::RunTest(const FString& Parameters)
{
	const auto* Door = GetDefault<AVBoRHingedDoor>();
	TestFalse(TEXT("Coast default off"), Door->bCoastAfterRelease);
	TestEqual(TEXT("Approximate deceleration deg/s^2"), Door->CoastDecelerationDegPerSecond2, 180.0f);
	TestNotNull(TEXT("Door enable property"), FindFProperty<FBoolProperty>(Door->GetClass(), TEXT("bCoastAfterRelease")));
	TestNotNull(TEXT("Door deceleration property"), FindFProperty<FFloatProperty>(Door->GetClass(), TEXT("CoastDecelerationDegPerSecond2")));
	for (const UClass* Class : {AVBoRHingedPanel::StaticClass(), AVBoRPoweredPanel::StaticClass()})
	{
		TestNull(TEXT("Other panels have no enable property"), FindFProperty<FProperty>(Class, TEXT("bCoastAfterRelease")));
		TestNull(TEXT("Other panels have no deceleration property"), FindFProperty<FProperty>(Class, TEXT("CoastDecelerationDegPerSecond2")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5zaSettings, "vBoR.P5za.E19.LiveSettings", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5zaSettings::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	AP2bTrackedDoor* Door = World->SpawnActor<AP2bTrackedDoor>();
	if (TestNotNull(TEXT("Tracked door"), Door))
	{
		Door->bCoastAfterRelease = true; Door->CoastDecelerationDegPerSecond2 = 360;
		Door->Tick(P5zaDt);
		TestTrue(TEXT("Coast synced"), Door->GetMotion().bCoast);
		TestEqual(TEXT("Deceleration synced"), Door->GetMotion().CoastDecelerationDegPerSecond2, 360.0);
		Door->bCoastAfterRelease = false; Door->Tick(P5zaDt);
		TestFalse(TEXT("Coast disabled next Tick"), Door->GetMotion().bCoast);
	}
	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5zaHandleRelease, "vBoR.P5za.E20.HandleRelease", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5zaHandleRelease::RunTest(const FString& Parameters)
{
	for (double Direction : {1.0, -1.0})
	{
		for (double OffsetCm : {7.0, 9.0, 12.0})
		{
			const double Start = Direction > 0 ? 20 : 50;
			auto Door = P5zaDoor(Start);
			FVBoRDoorHand Hands[2] = {};
			const FVector Point(80, Direction * OffsetCm, 50), Normal(0, -Direction, 0);
			Hands[0] = P5zaHand(Start, Point, Normal);
			TestTrue(TEXT("Handle grab accepted"), Door.BeginGrab(0, Direction > 0 ? 0 : 1, Hands[0]));
			for (int32 Frame = 1; Frame <= 45; ++Frame)
			{
				Hands[0] = P5zaHand(Start + Direction * 60 * Frame * P5zaDt, Point, Normal);
				Door.Advance(Hands, P5zaDt);
			}
			TestTrue(TEXT("Handle release accepted"), Door.ReleaseGrab(true));
			const double Before = Door.AngleDeg;
			P5zaWait(Door, Hands, 1);
			TestTrue(FString::Printf(TEXT("Handle Y=%.0f cm direction=%.0f: coast %.4f deg"),
				OffsetCm, Direction, Direction * (Door.AngleDeg - Before)),
				FMath::Abs(Direction * (Door.AngleDeg - Before) - 10) <= 1.5);
			const int32 OppositeFace = Direction > 0 ? 0 : 1;
			TestEqual(TEXT("Released hand never starts opposite push"), Door.Metrics.PushStarts[0][OppositeFace], 0);
			TestEqual(TEXT("Coast decelerated to rest"), Door.CoastSpeed, 0.0);
			if (Direction > 0 && OffsetCm == 9)
			{
				const double Stopped = Door.AngleDeg;
				Hands[0] = P5zaHand(Stopped, FVector(80, -8, 50)); Door.Advance(Hands, P5zaDt);
				Hands[0] = P5zaHand(Stopped, FVector(80, -4, 50)); Door.Advance(Hands, P5zaDt);
				TestEqual(TEXT("Same hand can push after coast ends"), Door.Metrics.PushStarts[0][1], 1);
				TestTrue(TEXT("Same hand opens after coast ends"), Door.AngleDeg > Stopped);
			}
		}
	}
	// No momentum means no suppression (P5za R1-1)
	auto Door = P5zaDoor(); FVBoRDoorHand Hands[2] = {};
	Hands[0] = P5zaHand(20, FVector(80, -8, 50));
	TestTrue(TEXT("Stationary grab accepted"), Door.BeginGrab(0, 1, Hands[0]));
	Door.Advance(Hands, P5zaDt);
	TestTrue(TEXT("Stationary release accepted"), Door.ReleaseGrab(true));
	Hands[0] = P5zaHand(20, FVector(80, -4, 50)); Door.Advance(Hands, P5zaDt);
	TestEqual(TEXT("Stationary release allows immediate push"), Door.Metrics.PushStarts[0][1], 1);
	TestTrue(TEXT("Stationary release push moves door"), Door.AngleDeg > 20);
	return true;
}
#endif
