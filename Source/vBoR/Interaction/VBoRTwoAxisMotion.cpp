#include "VBoRTwoAxisMotion.h"

FQuat FVBoRTwoAxisMotion::Rotation() const
{
	return FQuat(FVector::ZAxisVector,FMath::DegreesToRadians(YawDeg)) * FQuat(FVector::YAxisVector,FMath::DegreesToRadians(-PitchDeg));
}

bool FVBoRTwoAxisMotion::BeginGrab(int32 Hand, const FVector& Point)
{
	if (Hand < 0 || Hand > 1 || GrabHand != INDEX_NONE || Point.ContainsNaN()) { return false; }
	GrabHand = Hand; LastGrabPoint = Point; bPaused = false; ClearPush(); return true;
}

void FVBoRTwoAxisMotion::ReleaseGrab() { GrabHand = INDEX_NONE; bPaused = false; ClearPush(); }

void FVBoRTwoAxisMotion::AdvanceGrab(bool bTracked, const FVector& Point)
{
	if (GrabHand == INDEX_NONE) { return; }
	if (!bTracked || Point.ContainsNaN()) { bPaused = true; return; }
	const double DistanceCm = FVector::Distance(Point,LastGrabPoint);
	if (bPaused) { RecoveryDiscardCm = DistanceCm; }
	if (!bPaused && DistanceCm <= MaxGrabJumpCm)
	{
		const double YawLeverCm = LastGrabPoint.Size2D();
		if (YawLeverCm > UE_SMALL_NUMBER && Point.Size2D() > UE_SMALL_NUMBER)
		{
			const double Delta = FMath::FindDeltaAngleDegrees(FMath::RadiansToDegrees(FMath::Atan2(LastGrabPoint.Y,LastGrabPoint.X)),FMath::RadiansToDegrees(FMath::Atan2(Point.Y,Point.X)));
			YawDeg = FMath::Clamp(YawDeg + Delta*YawLeverCm/FMath::Max(YawLeverCm,MinLeverCm),-YawLimitDeg,YawLimitDeg);
		}
		// Measure on vertical plane after Yaw. Lever arm reduces near-pivot jitter (P5l D1, cm/deg)
		const FQuat Yaw(FVector::ZAxisVector,FMath::DegreesToRadians(YawDeg));
		const FVector PreviousPoint = Yaw.UnrotateVector(LastGrabPoint), CurrentPoint = Yaw.UnrotateVector(Point);
		const double PitchLeverCm = FMath::Sqrt(FMath::Square(PreviousPoint.X)+FMath::Square(PreviousPoint.Z));
		if (PitchLeverCm > UE_SMALL_NUMBER && FMath::Square(CurrentPoint.X)+FMath::Square(CurrentPoint.Z) > UE_SMALL_NUMBER)
		{
			const double Delta = FMath::FindDeltaAngleDegrees(FMath::RadiansToDegrees(FMath::Atan2(PreviousPoint.Z,-PreviousPoint.X)),FMath::RadiansToDegrees(FMath::Atan2(CurrentPoint.Z,-CurrentPoint.X)));
			PitchDeg = FMath::Clamp(PitchDeg - Delta*PitchLeverCm/FMath::Max(PitchLeverCm,MinLeverCm),-PitchLimitDeg,PitchLimitDeg);
		}
	}
	// Rebase on beyond-limit motion/jump/recovery too (follow immediately on return, P5l D1/D2)
	LastGrabPoint = Point; bPaused = false;
}

void FVBoRTwoAxisMotion::ClearPush()
{
	PushHand = PushPoint = PushFace = INDEX_NONE; Previous[0] = Previous[1] = FVBoRDoorHand(); PreviousRotation = Rotation();
}

bool FVBoRTwoAxisMotion::FindPush(int32 Hand, const FVBoRDoorHand& Input, FVector& Point, double& Depth, int32& PointIndex, int32& FaceIndex) const
{
	if (!Input.bTracked) { return false; }
	const FQuat Inverse = Rotation().Inverse();
	const bool Continuing = PushHand == Hand;
	bool Found = false; Depth = -FrontToleranceCm;
	auto Inside = [this](const FVector& P)
	{
		return P.Y >= FaceMinCm.Y-EdgeMarginCm && P.Y <= FaceMaxCm.Y+EdgeMarginCm &&
			P.Z >= FaceMinCm.Z-EdgeMarginCm && P.Z <= FaceMaxCm.Z+EdgeMarginCm;
	};
	for (int32 I=0; I<5; ++I)
	{
		if (Continuing && I != PushPoint) { continue; }
		const FVector P = Inverse.RotateVector(Input.Points[I]);
		if (P.ContainsNaN() || !Inside(P)) { continue; }
		const FVector Prev = PreviousRotation.UnrotateVector(Previous[Hand].Points[I]);
		const FVector Center = (FaceMinCm+FaceMaxCm)/2;
		if (FMath::Square(P.Y-Center.Y)+FMath::Square(P.Z-Center.Z) < FMath::Square(CenterExclusionCm)) { continue; }
		for (int32 Face=0; Face<2; ++Face)
		{
			if (Continuing && Face != PushFace) { continue; }
			const double D = Face==0 ? P.X-FaceMinCm.X : FaceMaxCm.X-P.X;
			if (D < (Continuing ? -FrontToleranceCm : 1.e-6) || D > FaceMaxCm.X-FaceMinCm.X+DepthMarginCm) { continue; }
			const double PreviousDepth = Face==0 ? Prev.X-FaceMinCm.X : FaceMaxCm.X-Prev.X;
			// Recovered point on back face is no new push. Accept crossing from behind only (P5l D3/E12, P5e E13)
			const bool WasOutside = Face==0 ? PreviousDepth <= FrontToleranceCm : PreviousDepth < -1.e-6;
			if (!Continuing && (!Previous[Hand].bTracked || Prev.ContainsNaN() || !Inside(Prev) || !WasOutside)) { continue; }
			if (D > Depth) { Depth = D; Point = Input.Points[I]; PointIndex = I; FaceIndex = Face; Found = true; }
		}
	}
	return Found;
}

void FVBoRTwoAxisMotion::AdvancePush(const FVBoRDoorHand (&Hands)[2])
{
	if (GrabHand != INDEX_NONE) { ClearPush(); return; }
	FVector Point = FVector::ZeroVector; double Depth = 0;
	const FQuat Before = Rotation();
	bool Found = PushHand != INDEX_NONE && FindPush(PushHand,Hands[PushHand],Point,Depth,PushPoint,PushFace);
	if (!Found)
	{
		PushHand = PushPoint = PushFace = INDEX_NONE;
		for (int32 Hand=0; Hand<2; ++Hand)
		{
			if (FindPush(Hand,Hands[Hand],Point,Depth,PushPoint,PushFace)) { PushHand = Hand; Found = true; break; }
		}
	}
	// Small depth near axis needs large Yaw. Keep ownership, block rotation only (P5n D3, cm)
	if (Found && Depth > 1.e-6 && FMath::Abs(Before.UnrotateVector(Point).Y) >= PushMinLeverCm)
	{
		const FVector Normal = Before.RotateVector(-FVector::XAxisVector);
		const double HorizontalNormal = Normal.Size2D();
		const FVector FlatNormal = FVector(Normal.X,Normal.Y,0).GetSafeNormal();
		const FVector FlatPoint(Point.X,Point.Y,0);
		const double Radius = FlatPoint.Size();
		// Remove Z term from fixed-Pitch plane equation (Yaw-only push on grab-tilted mirror too, P5n D3)
		const double Plane = PushFace==0 ? -FaceMinCm.X : -FaceMaxCm.X;
		const double PlaneDistance = HorizontalNormal > UE_SMALL_NUMBER ? (Plane-Normal.Z*Point.Z)/HorizontalNormal : 0;
		if (HorizontalNormal > UE_SMALL_NUMBER && Radius > FMath::Abs(PlaneDistance)+1.e-6)
		{
			const double Angle = FMath::Acos(FMath::Clamp(FVector::DotProduct(FlatNormal,FlatPoint)/Radius,-1.0,1.0)) - FMath::Acos(PlaneDistance/Radius);
			const double CrossZ = FVector::CrossProduct(FlatNormal,FlatPoint).Z;
			if (!FMath::IsNearlyZero(CrossZ)) { YawDeg = FMath::Clamp(YawDeg+FMath::Sign(CrossZ)*FMath::RadiansToDegrees(Angle),-YawLimitDeg,YawLimitDeg); }
		}
	}
	Previous[0] = Hands[0]; Previous[1] = Hands[1]; PreviousRotation = Rotation();
}
