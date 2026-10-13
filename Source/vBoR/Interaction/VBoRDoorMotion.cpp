#include "VBoRDoorMotion.h"

FVBoRDoorMetrics FVBoRDoorMetrics::Since(const FVBoRDoorMetrics& Before) const
{
	FVBoRDoorMetrics Result = *this;
	Result.Unlocks -= Before.Unlocks; Result.Latches -= Before.Latches;
	Result.RateLimitedFrames -= Before.RateLimitedFrames; Result.Pauses -= Before.Pauses;
	Result.IgnoredReleases -= Before.IgnoredReleases;
	Result.PushTrackingLosses -= Before.PushTrackingLosses;
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
	for (int32 H = 0; H < 2; ++H) { bContactNear[H] = false; ContactFace[H] = INDEX_NONE; LastIgnored[H] = EVBoRDoorIgnore::Count; }
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

void FVBoRDoorMotion::RecordIgnore(int32 Hand, EVBoRDoorIgnore Reason)
{
	// 같은 손·같은 이유는 1회만 집계함 (접촉 에피소드, P2 R1-1)
	if (Reason != EVBoRDoorIgnore::Count && LastIgnored[Hand] != Reason) { ++Metrics.Ignored[static_cast<int32>(Reason)]; }
	LastIgnored[Hand] = Reason;
}

bool FVBoRDoorMotion::FindPush(int32 Hand, const FVBoRDoorHand& Input, int32 Face, double& OutAngle, EVBoRDoorIgnore& OutReason)
{
	const double Sign = Face == 0 ? 1.0 : -1.0;
	const double PlaneY = Face == 0 ? FaceMaxCm.Y : FaceMinCm.Y;
	constexpr double EdgeMarginCm = 5; // 가장자리 스침만 집계함 (자리값, P2 R1-1)
	const bool bRejectedCrossing = bContactNear[Hand] && ContactFace[Hand] == Face &&
		(LastIgnored[Hand] == EVBoRDoorIgnore::PalmDirection || LastIgnored[Hand] == EVBoRDoorIgnore::NearAxis || LastIgnored[Hand] == EVBoRDoorIgnore::Outside);
	bool bCrossing = false, bAwayFromAxis = false, bInside = false;
	bContactNear[Hand] = false;
	OutReason = EVBoRDoorIgnore::Count;
	double Depth = 0;
	int32 Deepest = INDEX_NONE;
	for (int32 I = 0; I < 5; ++I)
	{
		const FVector P = ToDoor(Input.Points[I]);
		if (P.ContainsNaN() || P.X < FaceMinCm.X - EdgeMarginCm || P.X > FaceMaxCm.X + EdgeMarginCm || P.Z < FaceMinCm.Z - EdgeMarginCm || P.Z > FaceMaxCm.Z + EdgeMarginCm) { continue; }
		const double D = Sign * (P.Y - PlaneY);
		if (D >= -1.e-6) { continue; }
		const bool bContinuing = PushHand == Hand && PushFace == Face;
		const FVector Prev = FQuat(FVector::UpVector, FMath::DegreesToRadians(-PreviousAngleDeg)).RotateVector(Previous[Hand].Points[I]);
		const bool bFromFront = bHavePrevious[Hand] && Sign * (Prev.Y - PlaneY) >= -FrontToleranceCm;
		// 면 뒤의 먼 손은 제외함 (실제 crossing·연속 밀기만 예외, P2 R1-1)
		if (D < -ThicknessCm && !bFromFront && !bContinuing) { continue; }
		bContactNear[Hand] = true;
		// 같은 밀기에는 crossing 을 다시 요구하지 않음 (속도 상한으로 면 뒤에 남는 손, P2 E8b)
		if (!bContinuing && !bFromFront && !bRejectedCrossing) { continue; }
		bCrossing = true;
		if (Input.Points[I].SizeSquared2D() < FMath::Square(MinAxisDistanceCm)) { continue; }
		bAwayFromAxis = true;
		if (P.X < FaceMinCm.X || P.X > FaceMaxCm.X || P.Z < FaceMinCm.Z || P.Z > FaceMaxCm.Z) { continue; }
		bInside = true;
		if ((bContinuing || bFromFront) && -D > Depth) { Depth = -D; Deepest = I; }
	}
	if (!bContactNear[Hand]) { ContactFace[Hand] = INDEX_NONE; return false; }
	ContactFace[Hand] = Face;
	const FVector Palm = ToDoor(Input.PalmNormal).GetSafeNormal();
	const double PalmAngle = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(Palm, FVector(0, -Sign, 0)), -1.0, 1.0)));
	PalmAngleDeg[Hand] = PalmAngle;
	if (bLocked) { OutReason = EVBoRDoorIgnore::Locked; }
	else if (!bCrossing) { OutReason = EVBoRDoorIgnore::Behind; }
	else if (!bAwayFromAxis) { OutReason = EVBoRDoorIgnore::NearAxis; }
	else if (Palm.IsNearlyZero() || PalmAngle > PalmLimitDeg) { OutReason = EVBoRDoorIgnore::PalmDirection; }
	else if (!bInside) { OutReason = EVBoRDoorIgnore::Outside; }
	else if (Deepest == INDEX_NONE) { OutReason = EVBoRDoorIgnore::Behind; }
	if (OutReason != EVBoRDoorIgnore::Count) { return false; }
	const FVector& Point = Input.Points[Deepest];
	const double Radius = Point.Size2D();
	// bounds 면의 Y 가 되는 회전. x>=0 인 문 면 해만 사용함 (P2 D1·P2b R1-2, cm)
	const double Target = FMath::RadiansToDegrees(FMath::Atan2(Point.Y, Point.X) -
		FMath::Asin(FMath::Clamp(PlaneY / Radius, -1.0, 1.0)));
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
	const int32 Driver = GrabHand;
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
		Metrics.RecoveryDistanceCm = FVector::Distance(LastGrabPoint, Hands[Driver].Points[0]);
		RebaseGrab(Hands[Driver].Points[0]);
		PushHand = PushFace = INDEX_NONE;
		bPaused = false;
	}
	if (PushHand != INDEX_NONE && !Hands[PushHand].bTracked)
	{
		// 밀기는 소유가 없으므로 그 손만 끝냄 (다른 손은 바로 사용, P2 E15b)
		++Metrics.PushTrackingLosses;
		bHavePrevious[PushHand] = false;
		PushHand = PushFace = INDEX_NONE;
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
			if (!Hands[H].bTracked)
			{
				if (bContactNear[H]) { RecordIgnore(H, bLocked ? EVBoRDoorIgnore::Locked : EVBoRDoorIgnore::TrackingLow); }
				else { RecordIgnore(H, EVBoRDoorIgnore::Count); }
				continue;
			}
			if (bFound) { bContactNear[H] = false; ContactFace[H] = INDEX_NONE; RecordIgnore(H, EVBoRDoorIgnore::Count); continue; }
			int32 Face = ContactFace[H];
			if (PushHand == H) { Face = PushFace; }
			else if (bHavePrevious[H])
			{
				const FVector Prev = FQuat(FVector::UpVector, FMath::DegreesToRadians(-PreviousAngleDeg)).RotateVector(Previous[H].Points[0]);
				if (Prev.Y >= FaceMaxCm.Y) { Face = 0; }
				else if (Prev.Y <= FaceMinCm.Y) { Face = 1; }
			}
			if (Face == INDEX_NONE) { Face = ToDoor(Hands[H].PalmNormal).Y <= 0 ? 0 : 1; }
			double Target = AngleDeg;
			EVBoRDoorIgnore Reason;
			const bool bPush = FindPush(H, Hands[H], Face, Target, Reason);
			RecordIgnore(H, Reason);
			if (!bPush) { continue; }
			if (PushHand != H || PushFace != Face) { ++Metrics.PushStarts[H][Face]; }
			PushHand = H; PushFace = Face;
			MoveTo(Target, DeltaSeconds, true);
			bFound = true;
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
	for (int32 H = 0; H < 2; ++H) { bContactNear[H] = false; ContactFace[H] = INDEX_NONE; LastIgnored[H] = EVBoRDoorIgnore::Count; }
	PalmAngleDeg[0] = PalmAngleDeg[1] = -1;
}
