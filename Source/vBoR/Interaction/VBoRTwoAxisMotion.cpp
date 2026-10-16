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

bool FVBoRTwoAxisMotion::BeginGrab(int32 Hand, const FQuat& Wrist)
{
	if (Hand < 0 || Hand > 1 || GrabHand != INDEX_NONE || Wrist.ContainsNaN() || !Wrist.IsNormalized()) { return false; }
	GrabHand = Hand; LastWrist = Wrist; bPaused = false; ClearPush(); return true;
}

void FVBoRTwoAxisMotion::ReleaseGrab() { GrabHand = INDEX_NONE; bPaused = false; ClearPush(); }

void FVBoRTwoAxisMotion::AdvanceGrab(bool bTracked, const FQuat& Wrist)
{
	if (GrabHand == INDEX_NONE) { return; }
	if (!bTracked || Wrist.ContainsNaN() || !Wrist.IsNormalized()) { bPaused = true; return; }
	const FQuat Delta = (Wrist * LastWrist.Inverse()).GetNormalized();
	const double Degrees = FMath::RadiansToDegrees(Delta.AngularDistance(FQuat::Identity));
	if (bPaused) { RecoveryDiscardDeg = Degrees; }
	if (!bPaused && Degrees <= 30+1.e-6) { ApplyNormal(Delta.RotateVector(Rotation().RotateVector(-FVector::XAxisVector))); }
	// 한계 밖 회전·튐·추적 복구도 기준만 옮김 (되돌릴 때 즉시 따라오게, P5e D2·D3·D6)
	LastWrist = Wrist; bPaused = false;
}

void FVBoRTwoAxisMotion::ClearPush()
{
	PushHand = PushPoint = INDEX_NONE; Previous[0] = Previous[1] = FVBoRDoorHand(); PreviousRotation = Rotation();
}

bool FVBoRTwoAxisMotion::FindPush(int32 Hand, const FVBoRDoorHand& Input, FVector& Point, double& Depth, int32& PointIndex) const
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
		const double D = P.X-FaceMinCm.X;
		if (D < (Continuing ? -FrontToleranceCm : 1.e-6) || D > FaceMaxCm.X-FaceMinCm.X+DepthMarginCm) { continue; }
		const FVector Prev = PreviousRotation.UnrotateVector(Previous[Hand].Points[I]);
		if (!Continuing && (!Previous[Hand].bTracked || Prev.ContainsNaN() || !Inside(Prev) || Prev.X > FaceMinCm.X+FrontToleranceCm)) { continue; }
		const FVector Center = (FaceMinCm+FaceMaxCm)/2;
		if (FMath::Square(P.Y-Center.Y)+FMath::Square(P.Z-Center.Z) < FMath::Square(CenterExclusionCm)) { continue; }
		if (D > Depth) { Depth = D; Point = Input.Points[I]; PointIndex = I; Found = true; }
	}
	return Found;
}

void FVBoRTwoAxisMotion::AdvancePush(const FVBoRDoorHand (&Hands)[2])
{
	if (GrabHand != INDEX_NONE) { ClearPush(); return; }
	FVector Point = FVector::ZeroVector; double Depth = 0;
	const FQuat Before = Rotation();
	bool Found = PushHand != INDEX_NONE && FindPush(PushHand,Hands[PushHand],Point,Depth,PushPoint);
	if (!Found)
	{
		PushHand = PushPoint = INDEX_NONE;
		for (int32 Hand=0; Hand<2; ++Hand)
		{
			if (FindPush(Hand,Hands[Hand],Point,Depth,PushPoint)) { PushHand = Hand; Found = true; break; }
		}
	}
	if (Found && Depth > 1.e-6)
	{
		const FVector Normal = Before.RotateVector(-FVector::XAxisVector);
		const double Radius = Point.Size(), PlaneDistance = -FaceMinCm.X;
		if (Radius > FMath::Abs(PlaneDistance)+1.e-6)
		{
			// 가장 가까운 법선 중 N·손점 = 앞면 거리를 푸는 방향임 (멈춘 손에서 계속 돌지 않게, P5e D4)
			const double Angle = FMath::Acos(FMath::Clamp(FVector::DotProduct(Normal,Point)/Radius,-1.0,1.0)) - FMath::Acos(PlaneDistance/Radius);
			const FVector Axis = FVector::CrossProduct(Normal,Point).GetSafeNormal();
			if (!Axis.IsNearlyZero()) { ApplyNormal(FQuat(Axis,Angle).RotateVector(Normal)); }
		}
	}
	Previous[0] = Hands[0]; Previous[1] = Hands[1]; PreviousRotation = Rotation();
}
