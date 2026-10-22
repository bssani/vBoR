#include "VBoRDoorMotion.h"

namespace
{
	constexpr double DoorEdgeMarginCm = 5; // Count edge brushes only (placeholder, P2 R1-1)
}

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
	for (int32 H = 0; H < 2; ++H) { bContactNear[H] = false; ContactFace[H] = PushSide[H] = INDEX_NONE; LastIgnored[H] = EVBoRDoorIgnore::Count; }
	RebaseGrab(Input.Points[0]);
	++Metrics.Grabs[Hand][Handle];
	Unlock(Handle);
	return true;
}

bool FVBoRDoorMotion::Unlock(int32 Handle)
{
	if (!bUseLatch || !bLocked) { return false; }
	// Button shares unlock count/rearm (stay unlocked until opened then closed, P2d D3)
	bLocked = false; bLatchArmed = false;
	++Metrics.Unlocks; Metrics.LastUnlockHandle = Handle;
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
	// Count same hand/reason once per contact episode (P2 R1-1)
	if (Reason != EVBoRDoorIgnore::Count && LastIgnored[Hand] != Reason) { ++Metrics.Ignored[static_cast<int32>(Reason)]; }
	LastIgnored[Hand] = Reason;
}

bool FVBoRDoorMotion::HasLeftPushSide(const FVBoRDoorHand& Input, int32 Face) const
{
	const double Sign = Face == 0 ? 1.0 : -1.0;
	const double BoundsY = Face == 0 ? FaceMaxCm.Y : FaceMinCm.Y;
	for (const FVector& Point : Input.Points)
	{
		const FVector P = ToDoor(Point);
		if (P.X < FaceMinCm.X - DoorEdgeMarginCm || P.X > FaceMaxCm.X + DoorEdgeMarginCm ||
			P.Z < FaceMinCm.Z - DoorEdgeMarginCm || P.Z > FaceMaxCm.Z + DoorEdgeMarginCm) { continue; }
		double PlaneY = BoundsY;
		if (QueryFace && !QueryFace(P, Face, PlaneY)) { PlaneY = BoundsY; }
		// All five points must clear remembered face (P5w D3)
		if (!(Sign * (P.Y - PlaneY) >= 0)) { return false; }
	}
	return true;
}

bool FVBoRDoorMotion::FindPush(int32 Hand, const FVBoRDoorHand& Input, int32 Face, double& OutAngle, EVBoRDoorIgnore& OutReason)
{
	const double Sign = Face == 0 ? 1.0 : -1.0;
	const double BoundsY = Face == 0 ? FaceMaxCm.Y : FaceMinCm.Y;
	double DeepestPlaneY = BoundsY;
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
		if (P.ContainsNaN() || P.X < FaceMinCm.X - DoorEdgeMarginCm || P.X > FaceMaxCm.X + DoorEdgeMarginCm || P.Z < FaceMinCm.Z - DoorEdgeMarginCm || P.Z > FaceMaxCm.Z + DoorEdgeMarginCm) { continue; }
		double PlaneY = BoundsY;
		const bool bOnFace = QueryFace ? QueryFace(P, Face, PlaneY) :
			P.X >= FaceMinCm.X && P.X <= FaceMaxCm.X && P.Z >= FaceMinCm.Z && P.Z <= FaceMaxCm.Z;
		const double D = Sign * (P.Y - PlaneY);
		if (D >= -1.e-6) { continue; }
		const bool bContinuing = PushHand == Hand && PushFace == Face;
		const FVector Prev = FQuat(FVector::UpVector, FMath::DegreesToRadians(-PreviousAngleDeg)).RotateVector(Previous[Hand].Points[I]);
		double PreviousPlaneY = BoundsY;
		if (QueryFace && bHavePrevious[Hand]) { QueryFace(Prev, Face, PreviousPlaneY); }
		const bool bFromFront = bHavePrevious[Hand] && Sign * (Prev.Y - PreviousPlaneY) >= -FrontToleranceCm;
		// Skip far hands behind face (except real crossing or ongoing push, P2 R1-1)
		if (D < -ThicknessCm && !bFromFront && !bContinuing) { continue; }
		bContactNear[Hand] = true;
		// Same push needs no new crossing (speed cap leaves hand behind face, P2 E8b)
		if (!bContinuing && !bFromFront && !bRejectedCrossing) { continue; }
		bCrossing = true;
		if (Input.Points[I].SizeSquared2D() < FMath::Square(MinAxisDistanceCm)) { continue; }
		bAwayFromAxis = true;
		if (!bOnFace) { continue; }
		bInside = true;
		if ((bContinuing || bFromFront) && -D > Depth) { Depth = -D; Deepest = I; DeepestPlaneY = PlaneY; }
	}
	if (!bContactNear[Hand]) { ContactFace[Hand] = INDEX_NONE; return false; }
	ContactFace[Hand] = Face;
	const FVector Palm = ToDoor(Input.PalmNormal).GetSafeNormal();
	const double PalmAngle = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(Palm, FVector(0, -Sign, 0)), -1.0, 1.0)));
	PalmAngleDeg[Hand] = PalmAngle;
	if (bUseLatch && bLocked) { OutReason = EVBoRDoorIgnore::Locked; }
	// Keep locked count first. May allow closing face only after unlock (P2d R1 D7)
	else if (Face == 1 && !bAllowPalmOpening) { OutReason = EVBoRDoorIgnore::OpeningDisabled; }
	else if (!bCrossing) { OutReason = EVBoRDoorIgnore::Behind; }
	else if (!bAwayFromAxis) { OutReason = EVBoRDoorIgnore::NearAxis; }
	else if (Palm.IsNearlyZero() || PalmAngle > PalmLimitDeg) { OutReason = EVBoRDoorIgnore::PalmDirection; }
	else if (!bInside) { OutReason = EVBoRDoorIgnore::Outside; }
	else if (Deepest == INDEX_NONE) { OutReason = EVBoRDoorIgnore::Behind; }
	if (OutReason != EVBoRDoorIgnore::Count) { return false; }
	const FVector& Point = Input.Points[Deepest];
	const double Radius = Point.Size2D();
	// One step toward current point's face Y (query curve again next frame, P2g D4, cm)
	const double Target = FMath::RadiansToDegrees(FMath::Atan2(Point.Y, Point.X) -
		FMath::Asin(FMath::Clamp(DeepestPlaneY / Radius, -1.0, 1.0)));
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
	if (bUseLatch && AngleDeg > RearmAngleDeg) { bLatchArmed = true; }
	if (bUseLatch && bLatchArmed && AngleDeg < Before && AngleDeg <= LatchAngleDeg)
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
	// ── 1) Tracking loss/recovery ──
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
		// Push has no ownership. End this hand only (other hand can act immediately, P2 E15b)
		++Metrics.PushTrackingLosses;
		bHavePrevious[PushHand] = false;
		PushHand = PushFace = INDEX_NONE;
	}
	// ── 2) Existing grab > push ──
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
			if (PushHand != H && PushSide[H] != INDEX_NONE && HasLeftPushSide(Hands[H], PushSide[H])) { PushSide[H] = INDEX_NONE; }
			if (bFound) { bContactNear[H] = false; ContactFace[H] = INDEX_NONE; RecordIgnore(H, EVBoRDoorIgnore::Count); continue; }
			int32 Face = ContactFace[H];
			if (PushHand == H) { Face = PushFace; }
			else if (PushSide[H] != INDEX_NONE) { Face = PushSide[H]; }
			else if (bHavePrevious[H])
			{
				const FVector Prev = FQuat(FVector::UpVector, FMath::DegreesToRadians(-PreviousAngleDeg)).RotateVector(Previous[H].Points[0]);
				double OutsideY = FaceMaxCm.Y, InsideY = FaceMinCm.Y;
				if (QueryFace) { QueryFace(Prev, 0, OutsideY); QueryFace(Prev, 1, InsideY); }
				if (Prev.Y >= OutsideY) { Face = 0; }
				else if (Prev.Y <= InsideY) { Face = 1; }
			}
			if (Face == INDEX_NONE) { Face = ToDoor(Hands[H].PalmNormal).Y <= 0 ? 0 : 1; }
			double Target = AngleDeg;
			EVBoRDoorIgnore Reason;
			const bool bPush = FindPush(H, Hands[H], Face, Target, Reason);
			RecordIgnore(H, Reason);
			if (!bPush) { continue; }
			if (PushHand != H || PushFace != Face) { ++Metrics.PushStarts[H][Face]; }
			PushHand = H; PushFace = Face; PushSide[H] = Face;
			MoveTo(Target, DeltaSeconds, true);
			bFound = true;
		}
		if (!bFound) { PushHand = PushFace = INDEX_NONE; }
	}
	// Next frame checks crossing against newly moved face (P2 condition 3)
	for (int32 H = 0; H < 2; ++H) { Previous[H] = Hands[H]; bHavePrevious[H] = Hands[H].bTracked; }
	PreviousAngleDeg = AngleDeg;
}

void FVBoRDoorMotion::Reset()
{
	AngleDeg = 0; bLocked = bUseLatch; bLatchArmed = bPaused = false;
	GrabHand = GrabHandle = PushHand = PushFace = INDEX_NONE;
	bHavePrevious[0] = bHavePrevious[1] = false;
	for (int32 H = 0; H < 2; ++H) { bContactNear[H] = false; ContactFace[H] = PushSide[H] = INDEX_NONE; LastIgnored[H] = EVBoRDoorIgnore::Count; }
	PalmAngleDeg[0] = PalmAngleDeg[1] = -1;
}
