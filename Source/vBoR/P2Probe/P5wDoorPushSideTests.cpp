#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "../Interaction/VBoRDoorMotion.h"

namespace
{
	FVBoRDoorHand P5wHand(FVector Point, FVector Normal = FVector(0, -1, 0))
	{
		FVBoRDoorHand Hand;
		Hand.bTracked = true;
		for (FVector& P : Hand.Points) { P = Point; }
		Hand.PalmNormal = Normal;
		return Hand;
	}
	FVector P5wPoint(double Angle, FVector Point)
	{
		return FQuat(FVector::UpVector, FMath::DegreesToRadians(Angle)).RotateVector(Point);
	}
	void P5wInput(FVBoRDoorMotion& Door, FVBoRDoorHand (&Hands)[2], double ReferenceAngle,
		FVector Point, FVector Normal = FVector(0, -1, 0), double Dt = 0.1, int32 Hand = 0)
	{
		Hands[Hand] = P5wHand(P5wPoint(ReferenceAngle, Point), P5wPoint(ReferenceAngle, Normal));
		Door.Advance(Hands, Dt);
	}
	void P5wOpenThrough(FAutomationTestBase& Test, FVBoRDoorMotion& Door,
		FVBoRDoorHand (&Hands)[2], double Dt = 0.1, int32 Frames = 40)
	{
		Door.bLocked = false;
		P5wInput(Door, Hands, 0, FVector(80, -8, 50), FVector(0, 1, 0), Dt);
		P5wInput(Door, Hands, 0, FVector(80, 15, 50), FVector(0, 1, 0), Dt);
		for (int32 I = 0; I < Frames; ++I)
		{
			P5wInput(Door, Hands, FMath::Min(Door.AngleDeg + 15, 85.0), FVector(80, 0, 50), FVector(0, 1, 0), Dt);
		}
		Test.TestEqual(TEXT("E1 opened to limit"), Door.AngleDeg, 70.0);
		Test.TestEqual(TEXT("E1 inside push continues at limit"), Door.PushFace, 1);
		Hands[0].PalmNormal = P5wPoint(85, FVector(0, -1, 0));
		Door.Advance(Hands, Dt);
		Test.TestEqual(TEXT("E1 turned palm ends push"), Door.PushHand, INDEX_NONE);
	}
	void P5wWithdraw(FAutomationTestBase& Test, FVBoRDoorMotion& Door,
		FVBoRDoorHand (&Hands)[2], double Dt = 0.1)
	{
		const double Before = Door.AngleDeg;
		P5wInput(Door, Hands, 70, FVector(80, 0, 50), FVector(0, -1, 0), Dt);
		Test.TestEqual(TEXT("E1 withdrawal keeps angle"), Door.AngleDeg, Before);
		Test.TestEqual(TEXT("E1 withdrawal has no push"), Door.PushHand, INDEX_NONE);
		Test.TestEqual(TEXT("E1 no outside push started"), Door.Metrics.PushStarts[0][0], 0);
	}
	void P5wCloseFromOutside(FVBoRDoorMotion& Door, FVBoRDoorHand (&Hands)[2], int32 Hand = 0)
	{
		const double ReferenceAngle = Door.AngleDeg;
		P5wInput(Door, Hands, ReferenceAngle, FVector(80, 8, 50), FVector(0, -1, 0), 0.1, Hand);
		P5wInput(Door, Hands, ReferenceAngle, FVector(80, -2, 50), FVector(0, -1, 0), 0.1, Hand);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5wWithdraw, "vBoR.P5w.E1.Withdrawal", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5wWithdraw::RunTest(const FString& Parameters)
{
	FVBoRDoorMotion Door;
	FVBoRDoorHand Hands[2] = {};
	P5wOpenThrough(*this, Door, Hands);
	P5wWithdraw(*this, Door, Hands);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5wFront, "vBoR.P5w.E2.FrontClearance", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5wFront::RunTest(const FString& Parameters)
{
	FVBoRDoorMotion Door;
	FVBoRDoorHand Hands[2] = {};
	P5wOpenThrough(*this, Door, Hands);
	P5wWithdraw(*this, Door, Hands);
	Hands[0] = P5wHand(P5wPoint(70, FVector(80, -20, 50)), P5wPoint(70, FVector(0, 1, 0)));
	Hands[0].Points[4] = P5wPoint(70, FVector(80, 20, 50));
	Door.Advance(Hands, 0.1);
	P5wCloseFromOutside(Door, Hands);
	TestEqual(TEXT("E2 one point behind retains side"), Door.AngleDeg, 70.0);
	P5wInput(Door, Hands, 70, FVector(80, -20, 50), FVector(0, 1, 0));
	P5wCloseFromOutside(Door, Hands);
	TestTrue(TEXT("E2 return in front permits closing"), Door.AngleDeg < 70);
	TestEqual(TEXT("E2 outside face selected"), Door.PushFace, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5wEdge, "vBoR.P5w.E3.EdgeClearance", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5wEdge::RunTest(const FString& Parameters)
{
	FVBoRDoorMotion Door;
	FVBoRDoorHand Hands[2] = {};
	P5wOpenThrough(*this, Door, Hands);
	P5wWithdraw(*this, Door, Hands);
	Hands[0] = P5wHand(P5wPoint(70, FVector(120, 20, 50)), P5wPoint(70, FVector(0, -1, 0)));
	Hands[0].Points[4] = P5wPoint(70, FVector(80, 20, 50));
	Door.Advance(Hands, 0.1);
	P5wCloseFromOutside(Door, Hands);
	TestEqual(TEXT("E3 one point within bounds retains side"), Door.AngleDeg, 70.0);
	P5wInput(Door, Hands, 70, FVector(120, 20, 50));
	P5wCloseFromOutside(Door, Hands);
	TestTrue(TEXT("E3 leave edge permits closing"), Door.AngleDeg < 70);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5wLatch, "vBoR.P5w.E4.LatchedWithdrawal", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5wLatch::RunTest(const FString& Parameters)
{
	FVBoRDoorMotion Door;
	Door.bLocked = false; Door.AngleDeg = 30;
	FVBoRDoorHand Hands[2] = {};
	P5wInput(Door, Hands, 30, FVector(80, 8, 50));
	P5wInput(Door, Hands, 30, FVector(80, -2, 50));
	for (int32 I = 0; I < 10 && !Door.bLocked; ++I)
	{
		P5wInput(Door, Hands, Door.AngleDeg, FVector(80, -20, 50));
	}
	TestTrue(TEXT("E4 closing latches"), Door.bLocked);
	TestEqual(TEXT("E4 one latch"), Door.Metrics.Latches, 1);
	const int32 Unlocks = Door.Metrics.Unlocks;
	P5wInput(Door, Hands, 0, FVector(80, -20, 50));
	P5wInput(Door, Hands, 0, FVector(80, 0, 50), FVector(0, 1, 0));
	P5wInput(Door, Hands, 0, FVector(80, 8, 50), FVector(0, 1, 0));
	TestEqual(TEXT("E4 withdrawal keeps closed angle"), Door.AngleDeg, 0.0);
	TestTrue(TEXT("E4 withdrawal stays locked"), Door.bLocked);
	TestEqual(TEXT("E4 no extra latch"), Door.Metrics.Latches, 1);
	TestEqual(TEXT("E4 no unlock"), Door.Metrics.Unlocks, Unlocks);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5wTracking, "vBoR.P5w.E5.TrackingLoss", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5wTracking::RunTest(const FString& Parameters)
{
	FVBoRDoorMotion Door;
	FVBoRDoorHand Hands[2] = {};
	P5wOpenThrough(*this, Door, Hands);
	Hands[0].bTracked = false;
	// Stale points must not clear the side during tracking loss (P5w D3)
	for (FVector& P : Hands[0].Points) { P = P5wPoint(70, FVector(120, 20, 50)); }
	for (int32 I = 0; I < 3; ++I) { Door.Advance(Hands, 0.1); }
	P5wInput(Door, Hands, 70, FVector(80, 15, 50));
	P5wWithdraw(*this, Door, Hands);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5wOtherHand, "vBoR.P5w.E6.OtherHand", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5wOtherHand::RunTest(const FString& Parameters)
{
	FVBoRDoorMotion Door;
	FVBoRDoorHand Hands[2] = {};
	P5wOpenThrough(*this, Door, Hands);
	P5wWithdraw(*this, Door, Hands);
	P5wCloseFromOutside(Door, Hands, 1);
	TestTrue(TEXT("E6 other hand closes"), Door.AngleDeg < 70);
	TestEqual(TEXT("E6 other hand starts outside push once"), Door.Metrics.PushStarts[1][0], 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5wResetGrab, "vBoR.P5w.E7.ResetAndGrab", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5wResetGrab::RunTest(const FString& Parameters)
{
	for (bool bGrab : {false, true})
	{
		FVBoRDoorMotion Door;
		FVBoRDoorHand Hands[2] = {};
		P5wOpenThrough(*this, Door, Hands);
		P5wWithdraw(*this, Door, Hands);
		const int32 Starts = Door.Metrics.PushStarts[0][0];
		if (bGrab)
		{
			TestTrue(TEXT("E7 grab accepted"), Door.BeginGrab(0, 0, Hands[0]));
			TestTrue(TEXT("E7 grab released"), Door.ReleaseGrab(true));
			P5wInput(Door, Hands, Door.AngleDeg, FVector(80, 15, 50));
			P5wInput(Door, Hands, Door.AngleDeg, FVector(80, -2, 50));
		}
		else
		{
			Door.Reset(); Door.bLocked = false; Door.AngleDeg = 30;
			P5wCloseFromOutside(Door, Hands);
			TestTrue(TEXT("E7 reset permits closing below 30 deg"), Door.AngleDeg < 30);
		}
		TestEqual(TEXT("E7 outside push starts after clear"), Door.Metrics.PushStarts[0][0], Starts + 1);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5wRate, "vBoR.P5w.E8.FrameRates", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5wRate::RunTest(const FString& Parameters)
{
	for (int32 Rate : {15, 90})
	{
		FVBoRDoorMotion Door;
		FVBoRDoorHand Hands[2] = {};
		const double Dt = 1.0 / Rate;
		P5wOpenThrough(*this, Door, Hands, Dt, Rate);
		P5wWithdraw(*this, Door, Hands, Dt);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5wQuery, "vBoR.P5w.E9.QueriedSurface", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5wQuery::RunTest(const FString& Parameters)
{
	for (bool bMiss : {false, true})
	{
		FVBoRDoorMotion Door;
		bool bQueryHit = true;
		Door.QueryFace = [&bQueryHit](const FVector& P, int32 Face, double& Y)
		{
			Y = Face == 0 ? 2.0 : -2.0;
			return bQueryHit && P.X >= 0 && P.X <= 110 && P.Z >= 0 && P.Z <= 105;
		};
		FVBoRDoorHand Hands[2] = {};
		P5wOpenThrough(*this, Door, Hands);
		P5wWithdraw(*this, Door, Hands);
		bQueryHit = !bMiss;
		// -3 cm is in front of queried -2, behind fallback -5 (P5w D3)
		P5wInput(Door, Hands, 70, FVector(80, -3, 50), FVector(0, 1, 0));
		bQueryHit = true;
		P5wCloseFromOutside(Door, Hands);
		if (bMiss)
		{
			TestEqual(TEXT("E9 query miss uses box plane, keeps side"), Door.AngleDeg, 70.0);
			TestEqual(TEXT("E9 query miss cannot start outside push"), Door.Metrics.PushStarts[0][0], 0);
		}
		else
		{
			TestTrue(TEXT("E9 queried front clears side"), Door.AngleDeg < 70);
			TestEqual(TEXT("E9 outside push follows queried clearance"), Door.Metrics.PushStarts[0][0], 1);
		}
	}
	return true;
}
#endif
