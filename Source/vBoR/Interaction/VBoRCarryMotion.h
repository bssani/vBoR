#pragma once

#include "CoreMinimal.h"

struct FVBoRCarryMotion
{
	static FQuat LimitWorldTilt(const FQuat& Rotation, double TiltLimitDeg)
	{
		FQuat Swing;
		FQuat Twist;
		Rotation.GetNormalized().ToSwingTwist(FVector::UpVector, Swing, Twist);
		// q 와 -q 의 같은 회전을 최단 swing 으로 맞춤 (P1 R1-2, ±90 deg Euler 특이점 제외)
		if (Swing.W < 0) { Swing = Swing * -1.0; }
		const double Angle = 2.0 * FMath::Acos(FMath::Clamp(Swing.W, 0.0, 1.0));
		const double Limit = FMath::DegreesToRadians(FMath::Max(0.0, TiltLimitDeg));
		if (Angle > Limit) { Swing = FQuat::Slerp(FQuat::Identity, Swing, Limit / Angle); }
		return (Swing * Twist).GetNormalized();
	}
	static FQuat LimitedHandDelta(const FTransform& StartHand, const FTransform& Hand, double TiltLimitDeg)
	{
		return LimitWorldTilt(Hand.GetRotation() * StartHand.GetRotation().Inverse(), TiltLimitDeg);
	}
	static FTransform Follow(const FTransform& StartHand, const FTransform& Hand, const FTransform& StartObject, double TiltLimitDeg)
	{
		const FQuat Delta = LimitedHandDelta(StartHand, Hand, TiltLimitDeg);
		return FTransform(Delta * StartObject.GetRotation(),
			Hand.GetLocation() + Delta.RotateVector(StartObject.GetLocation() - StartHand.GetLocation()), StartObject.GetScale3D());
	}
	static FQuat LimitedPairDelta(const FQuat& Yaw, const FVector& TiltAxis, double Elevation,
		const FQuat& StartObject, const FQuat& Rest, double TiltLimitDeg)
	{
		const double CosLimit = FMath::Cos(FMath::DegreesToRadians(FMath::Max(0.0, TiltLimitDeg)));
		auto Fits = [&](double Fraction)
		{
			const FQuat Relative = FQuat(TiltAxis, Elevation * Fraction) * Yaw * StartObject * Rest.Inverse();
			return FVector::DotProduct(Relative.RotateVector(FVector::UpVector), FVector::UpVector) >= CosLimit;
		};
		double Fraction = 1.0;
		if (!Fits(Fraction))
		{
			// 높이 차로 더하는 기울기만 줄임 (두 손 진입 때 자기 축 회전을 유지, P1 D6·R3-2)
			double Allowed = 0.0;
			double Rejected = 1.0;
			for (int32 Step = 0; Step < 24; ++Step)
			{
				const double Candidate = (Allowed + Rejected) * 0.5;
				if (Fits(Candidate)) { Allowed = Candidate; }
				else { Rejected = Candidate; }
			}
			Fraction = Allowed;
		}
		return (FQuat(TiltAxis, Elevation * Fraction) * Yaw).GetNormalized();
	}
	static double AngleDeg(const FTransform& A, const FTransform& B)
	{
		return FMath::RadiansToDegrees(A.GetRotation().AngularDistance(B.GetRotation()));
	}
	static bool CanAssist(const FTransform& Object, const FTransform& Goal, bool bClear, double DistanceCm, double AngleLimitDeg)
	{
		return bClear && FVector::Distance(Object.GetLocation(), Goal.GetLocation()) <= DistanceCm && AngleDeg(Object, Goal) <= AngleLimitDeg;
	}
	static FVector Drop(const FVector& Position, double SpeedCmPerSecond, double DeltaSeconds)
	{
		return Position - FVector(0, 0, FMath::Max(0.0, SpeedCmPerSecond) * FMath::Max(0.0, DeltaSeconds));
	}
	static FVector SweepStop(const FVector& From, const FVector& To, double HitTime)
	{
		return FMath::Lerp(From, To, FMath::Clamp(HitTime, 0.0, 1.0));
	}
};

// SDK selection 과 별도로 두 손을 소유함 (손실 취소가 놓기로 바뀌지 않게, P1 D6)
struct FVBoRCarrySession
{
	int32 Hand = INDEX_NONE;
	int32 SecondHand = INDEX_NONE;
	bool bPaused = false;
	FTransform StartHand;
	FTransform StartObject;
	FTransform StartHands[2];
	FTransform LastHands[2];
	FQuat RestRotation = FQuat::Identity;
	bool bHasRestRotation = false;
	bool bPairDirectionValid = false;
	FVector LastValidHand = FVector::ZeroVector;
	double GrabAt = 0;
	double HeldSeconds = 0;
	double PauseAt = 0;
	double PausedSeconds = 0;
	double RecoveryDistanceCm = 0;
	int32 PauseCount = 0;
	int32 IgnoredReleases = 0;
	int32 TwoHandEntries = 0;
	double TwoHandSeconds = 0;
	double LastTime = 0;
	// 5 cm 미만 또는 수직에 가까운 두 점은 방향 대신 첫 손의 이동만 따름 (P1 D6 자리값)
	static constexpr double MinPairDistanceCm = 5.0;
	bool HasHand(int32 Id) const { return Id != INDEX_NONE && (Hand == Id || SecondHand == Id); }
	bool IsTwoHanded() const { return SecondHand != INDEX_NONE; }
	static bool HasDirection(const FTransform* Poses)
	{
		return (Poses[1].GetLocation() - Poses[0].GetLocation()).Size2D() >= MinPairDistanceCm;
	}
	void SetRestPose(const FTransform& Pose) { RestRotation = Pose.GetRotation(); bHasRestRotation = true; }
	void AccountTime(double Time)
	{
		if (IsTwoHanded()) { TwoHandSeconds += FMath::Max(0.0, Time - LastTime); }
		LastTime = Time;
		if (Hand != INDEX_NONE) { HeldSeconds = FMath::Max(0.0, Time - GrabAt); }
	}
	void Rebase(const FTransform* Poses, const FTransform& Object)
	{
		for (int32 Id = 0; Id < 2; ++Id) { StartHands[Id] = LastHands[Id] = Poses[Id]; }
		StartObject = Object;
		StartHand = Poses[Hand];
		LastValidHand = Poses[Hand].GetLocation();
		bPairDirectionValid = IsTwoHanded() && HasDirection(Poses);
	}
	bool Begin(int32 NewHand, const FTransform& HandPose, const FTransform& ObjectPose, double Time)
	{
		if (NewHand < 0 || NewHand > 1 || HasHand(NewHand) || bPaused || HandPose.ContainsNaN()) { return false; }
		AccountTime(Time);
		if (!bHasRestRotation) { SetRestPose(ObjectPose); }
		LastHands[NewHand] = HandPose;
		if (Hand == INDEX_NONE) { Hand = NewHand; GrabAt = Time; HeldSeconds = 0; }
		else { SecondHand = NewHand; ++TwoHandEntries; }
		Rebase(LastHands, ObjectPose);
		return true;
	}
	void Pause(double Time)
	{
		if (Hand == INDEX_NONE || bPaused) { return; }
		bPaused = true;
		PauseAt = Time;
		++PauseCount;
	}
	bool UpdateHands(bool bTracking, const FTransform* Poses, const FTransform& ObjectPose, double Time, double TiltLimitDeg, FTransform& Desired)
	{
		Desired = ObjectPose;
		if (Hand == INDEX_NONE) { return false; }
		AccountTime(Time);
		if (!bTracking) { Pause(Time); return false; }
		if (bPaused)
		{
			RecoveryDistanceCm = FVector::Distance(LastHands[Hand].GetLocation(), Poses[Hand].GetLocation());
			if (IsTwoHanded()) { RecoveryDistanceCm = FMath::Max(RecoveryDistanceCm, FVector::Distance(LastHands[SecondHand].GetLocation(), Poses[SecondHand].GetLocation())); }
			PausedSeconds += FMath::Max(0.0, Time - PauseAt);
			Rebase(Poses, ObjectPose);
			bPaused = false;
			return false;
		}
		if (IsTwoHanded() && bPairDirectionValid != HasDirection(Poses))
		{
			// 방향을 잃거나 되찾는 frame 은 재기준만 잡음 (근접 교차 때 Yaw 반전 방지, P1 D6)
			Rebase(Poses, ObjectPose);
			return false;
		}
		for (int32 Id = 0; Id < 2; ++Id) { LastHands[Id] = Poses[Id]; }
		LastValidHand = Poses[Hand].GetLocation();
		FQuat Delta = FQuat::Identity;
		FVector StartAnchor = StartHand.GetLocation();
		FVector Anchor = Poses[Hand].GetLocation();
		const bool bPairMotion = IsTwoHanded() && bPairDirectionValid;
		if (bPairMotion)
		{
			const FVector From = StartHands[1].GetLocation() - StartHands[0].GetLocation();
			const FVector To = Poses[1].GetLocation() - Poses[0].GetLocation();
			const double Yaw = FMath::Atan2(To.Y, To.X) - FMath::Atan2(From.Y, From.X);
			const double Elevation = FMath::Atan2(To.Z, To.Size2D()) - FMath::Atan2(From.Z, From.Size2D());
			const FVector TiltAxis = FVector::CrossProduct(FVector(To.X, To.Y, 0).GetSafeNormal(), FVector::UpVector);
			Delta = FVBoRCarryMotion::LimitedPairDelta(FQuat(FVector::UpVector, Yaw), TiltAxis, Elevation,
				StartObject.GetRotation(), RestRotation, TiltLimitDeg);
			StartAnchor = (StartHands[0].GetLocation() + StartHands[1].GetLocation()) * 0.5;
			Anchor = (Poses[0].GetLocation() + Poses[1].GetLocation()) * 0.5;
		}
		else if (!IsTwoHanded()) { Delta = FVBoRCarryMotion::LimitedHandDelta(StartHand, Poses[Hand], TiltLimitDeg); }
		// 처음 놓인 자세로 제한함 (재잡기·추적 복구·손 수 변경으로 기울기 누적 방지, P1 R3-2)
		const FQuat Rotation = bPairMotion ? (Delta * StartObject.GetRotation()).GetNormalized() :
			(FVBoRCarryMotion::LimitWorldTilt(Delta * StartObject.GetRotation() * RestRotation.Inverse(), TiltLimitDeg) * RestRotation).GetNormalized();
		const FQuat Applied = Rotation * StartObject.GetRotation().Inverse();
		Desired = FTransform(Rotation, Anchor + Applied.RotateVector(StartObject.GetLocation() - StartAnchor), StartObject.GetScale3D());
		return true;
	}
	bool Update(bool bTracking, const FTransform& HandPose, const FTransform& ObjectPose, double Time, double TiltLimitDeg, FTransform& Desired)
	{
		FTransform Poses[2] = {LastHands[0], LastHands[1]};
		if (Hand != INDEX_NONE) { Poses[Hand] = HandPose; }
		return UpdateHands(bTracking, Poses, ObjectPose, Time, TiltLimitDeg, Desired);
	}
	bool Release(int32 ReleasingHand, bool bTracking, double Time, const FTransform* Poses = nullptr, const FTransform* Object = nullptr)
	{
		if (!HasHand(ReleasingHand)) { return false; }
		AccountTime(Time);
		if (!bTracking || bPaused) { Pause(Time); ++IgnoredReleases; return false; }
		if (IsTwoHanded())
		{
			if (ReleasingHand == Hand) { Hand = SecondHand; }
			SecondHand = INDEX_NONE;
			Rebase(Poses ? Poses : LastHands, Object ? *Object : StartObject);
			return false;
		}
		Hand = INDEX_NONE;
		return true;
	}
};
