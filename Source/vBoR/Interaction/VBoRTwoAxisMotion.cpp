#include "VBoRTwoAxisMotion.h"

FQuat FVBoRTwoAxisMotion::Rotation() const
{
	return FQuat(FVector::ZAxisVector,FMath::DegreesToRadians(YawDeg)) * FQuat(FVector::YAxisVector,FMath::DegreesToRadians(-PitchDeg));
}

void FVBoRTwoAxisMotion::ApplyNormal(const FVector& Normal)
{
	YawDeg = FMath::Clamp(FMath::RadiansToDegrees(FMath::Atan2(-Normal.Y,-Normal.X)),-YawLimitDeg,YawLimitDeg);
	PitchDeg = FMath::Clamp(FMath::RadiansToDegrees(FMath::Asin(FMath::Clamp(-Normal.Z,-1.0,1.0))),-PitchLimitDeg,PitchLimitDeg);
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
		// 좌우로 돈 뒤 세로면에서 잼. 피벗 가까운 손의 떨림은 지렛대로 줄임 (P5l D1, cm·deg)
		const FQuat Yaw(FVector::ZAxisVector,FMath::DegreesToRadians(YawDeg));
		const FVector PreviousPoint = Yaw.UnrotateVector(LastGrabPoint), CurrentPoint = Yaw.UnrotateVector(Point);
		const double PitchLeverCm = FMath::Sqrt(FMath::Square(PreviousPoint.X)+FMath::Square(PreviousPoint.Z));
		if (PitchLeverCm > UE_SMALL_NUMBER && FMath::Square(CurrentPoint.X)+FMath::Square(CurrentPoint.Z) > UE_SMALL_NUMBER)
		{
			const double Delta = FMath::FindDeltaAngleDegrees(FMath::RadiansToDegrees(FMath::Atan2(PreviousPoint.Z,-PreviousPoint.X)),FMath::RadiansToDegrees(FMath::Atan2(CurrentPoint.Z,-CurrentPoint.X)));
			PitchDeg = FMath::Clamp(PitchDeg - Delta*PitchLeverCm/FMath::Max(PitchLeverCm,MinLeverCm),-PitchLimitDeg,PitchLimitDeg);
		}
	}
	// 한계 밖 이동·튐·추적 복구도 기준만 옮김 (되돌릴 때 즉시 따라오게, P5l D1·D2)
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
			// 뒷면 위의 복구 점은 새 밀기가 아님. 뒤쪽에서 넘어온 점만 받음 (P5l D3·E12, P5e E13)
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
	if (Found && Depth > 1.e-6)
	{
		const FVector Normal = Before.RotateVector(-FVector::XAxisVector);
		const double Radius = Point.Size(), PlaneDistance = PushFace==0 ? -FaceMinCm.X : -FaceMaxCm.X;
		if (Radius > FMath::Abs(PlaneDistance)+1.e-6)
		{
			// 처음 닿은 면의 N·손점 = 면 거리를 풂 (한 밀기에서 점·면 유지, P5l D3)
			const double Angle = FMath::Acos(FMath::Clamp(FVector::DotProduct(Normal,Point)/Radius,-1.0,1.0)) - FMath::Acos(PlaneDistance/Radius);
			const FVector Axis = FVector::CrossProduct(Normal,Point).GetSafeNormal();
			if (!Axis.IsNearlyZero()) { ApplyNormal(FQuat(Axis,Angle).RotateVector(Normal)); }
		}
	}
	Previous[0] = Hands[0]; Previous[1] = Hands[1]; PreviousRotation = Rotation();
}
