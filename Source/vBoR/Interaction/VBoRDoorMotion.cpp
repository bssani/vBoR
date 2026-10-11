#include "VBoRDoorMotion.h"

FVBoRDoorMetrics FVBoRDoorMetrics::Since(const FVBoRDoorMetrics& Before) const
{
	FVBoRDoorMetrics Result = *this;
	Result.Unlocks -= Before.Unlocks; Result.Latches -= Before.Latches;
	Result.RateLimitedFrames -= Before.RateLimitedFrames; Result.Pauses -= Before.Pauses;
	Result.IgnoredReleases -= Before.IgnoredReleases;
	Result.PushedDeg -= Before.PushedDeg; Result.HeldSeconds -= Before.HeldSeconds; Result.PausedSeconds -= Before.PausedSeconds;
	for (int32 H = 0; H < 2; ++H)
	{
		for (int32 F = 0; F < 2; ++F)
		{
			Result.PushStarts[H][F] -= Before.PushStarts[H][F];
			Result.Attempts[H][F] -= Before.Attempts[H][F]; Result.Grabs[H][F] -= Before.Grabs[H][F];
		}
	}
	for (int32 I = 0; I < static_cast<int32>(EVBoRDoorIgnore::Count); ++I) { Result.Ignored[I] -= Before.Ignored[I]; }
	return Result;
}

FVector FVBoRDoorMotion::ToDoor(const FVector& Point) const
{
	return FQuat(FVector::UpVector, FMath::DegreesToRadians(-AngleDeg)).RotateVector(Point);
}

void FVBoRDoorMotion::RebaseGrab(const FVector& Point)
{
	LastGrabPoint = Point;
	GrabReferenceBearingDeg = FMath::RadiansToDegrees(FMath::Atan2(Point.Y, Point.X));
	GrabReferenceAngleDeg = AngleDeg;
}

bool FVBoRDoorMotion::BeginGrab(int32 Hand, int32 Handle, const FVBoRDoorHand& Input)
{
	if (Hand < 0 || Hand > 1 || Handle < 0 || Handle > 1 || !Input.bTracked || Input.Points[0].ContainsNaN()) { return false; }
	if (GrabHand != INDEX_NONE || bPaused || Input.Points[0].SizeSquared2D() < SMALL_NUMBER) { return false; }
	GrabHand = Hand; GrabHandle = Handle;
	PushHand = PushFace = INDEX_NONE;
	RebaseGrab(Input.Points[0]);
	++Metrics.Grabs[Hand][Handle];
	if (bLocked) { bLocked = false; bLatchArmed = false; ++Metrics.Unlocks; Metrics.LastUnlockHandle = Handle; }
	return true;
}

bool FVBoRDoorMotion::ReleaseGrab(bool bTracked)
{
	if (GrabHand == INDEX_NONE) { return false; }
	if (!bTracked || bPaused)
	{
		if (!bPaused) { ++Metrics.Pauses; }
		bPaused = true; ++Metrics.IgnoredReleases; return false;
	}
	GrabHand = GrabHandle = INDEX_NONE;
	return true;
}

bool FVBoRDoorMotion::FindPush(int32 Hand, const FVBoRDoorHand& Input, int32 Face, double& OutAngle)
{
	const double Sign = Face == 0 ? 1.0 : -1.0;
	const FVector Inward(0, -Sign, 0);
	const FVector Palm = ToDoor(Input.PalmNormal).GetSafeNormal();
	const double PalmAngle = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(Palm, Inward), -1.0, 1.0)));
	PalmAngleDeg[Hand] = PalmAngle;
	EVBoRDoorIgnore Reason = EVBoRDoorIgnore::Outside;
	if (Palm.IsNearlyZero() || PalmAngle > PalmLimitDeg) { ++Metrics.Ignored[static_cast<int32>(EVBoRDoorIgnore::PalmDirection)]; return false; }
	double Depth = 0;
	int32 Deepest = INDEX_NONE;
	for (int32 I = 0; I < 5; ++I)
	{
		const FVector P = ToDoor(Input.Points[I]);
		if (P.ContainsNaN() || P.X < 0 || P.X > WidthCm || P.Z < 0 || P.Z > HeightCm) { continue; }
		const double D = Sign * P.Y - ThicknessCm * 0.5;
		if (D >= -1.e-6) { continue; }
		if (Input.Points[I].SizeSquared2D() < FMath::Square(MinAxisDistanceCm)) { Reason = EVBoRDoorIgnore::NearAxis; continue; }
		const bool bContinuing = PushHand == Hand && PushFace == Face;
		const FVector Prev = FQuat(FVector::UpVector, FMath::DegreesToRadians(-PreviousAngleDeg)).RotateVector(Previous[Hand].Points[I]);
		// 같은 밀기에는 crossing 을 다시 요구하지 않음 (속도 상한으로 면 뒤에 남는 손, P2 E8b)
		if (!bContinuing && (!bHavePrevious[Hand] || Sign * Prev.Y - ThicknessCm * 0.5 < -FrontToleranceCm))
		{
			Reason = EVBoRDoorIgnore::Behind; continue;
		}
		if (-D > Depth) { Depth = -D; Deepest = I; }
	}
	if (Deepest == INDEX_NONE)
	{
		++Metrics.Ignored[static_cast<int32>(Reason)];
		return false;
	}
	const FVector& Point = Input.Points[Deepest];
	const double Radius = Point.Size2D();
	// y = ±두께/2 가 되는 회전. x>=0 인 문 면 해만 사용함 (감도 없이 기하 추종, P2 D1)
	const double Target = FMath::RadiansToDegrees(FMath::Atan2(Point.Y, Point.X) -
		FMath::Asin(FMath::Clamp(Sign * ThicknessCm * 0.5 / Radius, -1.0, 1.0)));
	OutAngle = AngleDeg + FMath::FindDeltaAngleDegrees(AngleDeg, Target);
	OutAngle = Face == 0 ? FMath::Min(AngleDeg, OutAngle) : FMath::Max(AngleDeg, OutAngle);
	return true;
}

void FVBoRDoorMotion::MoveTo(double Desired, double DeltaSeconds, bool bPushing)
{
	Desired = FMath::Clamp(Desired, 0.0, MaxAngleDeg);
	const double Before = AngleDeg;
	const double Budget = FMath::Max(0.0, MaxSpeedDegPerSecond * DeltaSeconds);
	const double Gap = Desired - AngleDeg;
	if (FMath::Abs(Gap) > Budget + 1.e-6) { ++Metrics.RateLimitedFrames; }
	AngleDeg += FMath::Clamp(Gap, -Budget, Budget);
	if (AngleDeg > RearmAngleDeg) { bLatchArmed = true; }
	if (bLatchArmed && AngleDeg < Before && AngleDeg <= LatchAngleDeg)
	{
		AngleDeg = 0; bLocked = true; bLatchArmed = false;
		++Metrics.Latches;
		GrabHand = GrabHandle = PushHand = PushFace = INDEX_NONE;
	}
	if (bPushing) { Metrics.PushedDeg += FMath::Abs(AngleDeg - Before); }
}

void FVBoRDoorMotion::Advance(const FVBoRDoorHand (&Hands)[2], double DeltaSeconds)
{
	if (!FMath::IsFinite(DeltaSeconds) || DeltaSeconds <= 0) { return; }
	PalmAngleDeg[0] = PalmAngleDeg[1] = -1;
	// ── 1) 추적 손실·복구 ──
	const int32 Driver = GrabHand != INDEX_NONE ? GrabHand : PushHand;
	if (Driver != INDEX_NONE && !Hands[Driver].bTracked)
	{
		if (!bPaused) { ++Metrics.Pauses; bPaused = true; }
		Metrics.PausedSeconds += DeltaSeconds;
		for (int32 H = 0; H < 2; ++H) { bHavePrevious[H] = false; }
		return;
	}
	const bool bRecovering = bPaused;
	if (bRecovering && Driver != INDEX_NONE)
	{
		const FVector OldPoint = GrabHand != INDEX_NONE ? LastGrabPoint : Previous[Driver].Points[0];
		Metrics.RecoveryDistanceCm = FVector::Distance(OldPoint, Hands[Driver].Points[0]);
		if (GrabHand != INDEX_NONE) { RebaseGrab(Hands[Driver].Points[0]); }
		PushHand = PushFace = INDEX_NONE;
		bPaused = false;
	}
	// ── 2) 먼저 잡은 손 > 밀기 ──
	if (GrabHand != INDEX_NONE)
	{
		Metrics.HeldSeconds += DeltaSeconds;
		const FVector Point = Hands[GrabHand].Points[0];
		if (!bRecovering && Point.SizeSquared2D() > SMALL_NUMBER)
		{
			const double Bearing = FMath::RadiansToDegrees(FMath::Atan2(Point.Y, Point.X));
			MoveTo(GrabReferenceAngleDeg + FMath::FindDeltaAngleDegrees(GrabReferenceBearingDeg, Bearing), DeltaSeconds, false);
		}
		LastGrabPoint = Point;
	}
	else if (!bRecovering)
	{
		bool bFound = false;
		const int32 First = PushHand != INDEX_NONE ? PushHand : 0;
		for (int32 Pass = 0; Pass < 2; ++Pass)
		{
			const int32 H = Pass == 0 ? First : 1 - First;
			if (!Hands[H].bTracked) { ++Metrics.Ignored[static_cast<int32>(EVBoRDoorIgnore::TrackingLow)]; continue; }
			const FVector Palm = ToDoor(Hands[H].PalmNormal);
			const int32 Face = Palm.Y <= 0 ? 0 : 1;
			PalmAngleDeg[H] = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FMath::Abs(Palm.Y), 0.0, 1.0)));
			if (bLocked) { ++Metrics.Ignored[static_cast<int32>(EVBoRDoorIgnore::Locked)]; continue; }
			double Target = AngleDeg;
			if (!FindPush(H, Hands[H], Face, Target)) { continue; }
			if (PushHand != H || PushFace != Face) { ++Metrics.PushStarts[H][Face]; }
			PushHand = H; PushFace = Face;
			MoveTo(Target, DeltaSeconds, true);
			bFound = true; break;
		}
		if (!bFound) { PushHand = PushFace = INDEX_NONE; }
	}
	// 다음 frame 은 지금 움직인 면을 기준으로 crossing 판정함 (P2 조건 3)
	for (int32 H = 0; H < 2; ++H) { Previous[H] = Hands[H]; bHavePrevious[H] = Hands[H].bTracked; }
	PreviousAngleDeg = AngleDeg;
}

void FVBoRDoorMotion::Reset()
{
	AngleDeg = 0; bLocked = true; bLatchArmed = bPaused = false;
	GrabHand = GrabHandle = PushHand = PushFace = INDEX_NONE;
	bHavePrevious[0] = bHavePrevious[1] = false;
	PalmAngleDeg[0] = PalmAngleDeg[1] = -1;
}
