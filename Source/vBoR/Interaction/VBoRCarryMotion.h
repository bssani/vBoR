#pragma once

#include "CoreMinimal.h"

struct FVBoRCarryMotion
{
	static FQuat LimitedHandDelta(const FTransform& StartHand, const FTransform& Hand, double TiltLimitDeg)
	{
		const FRotator Delta = (Hand.GetRotation() * StartHand.GetRotation().Inverse()).Rotator();
		return FRotator(FMath::Clamp(Delta.Pitch, -TiltLimitDeg, TiltLimitDeg), Delta.Yaw,
			FMath::Clamp(Delta.Roll, -TiltLimitDeg, TiltLimitDeg)).Quaternion();
	}
	static FTransform Follow(const FTransform& StartHand, const FTransform& Hand, const FTransform& StartObject, double TiltLimitDeg)
	{
		const FQuat Delta = LimitedHandDelta(StartHand, Hand, TiltLimitDeg);
		return FTransform(Delta * StartObject.GetRotation(),
			Hand.GetLocation() + Delta.RotateVector(StartObject.GetLocation() - StartHand.GetLocation()), StartObject.GetScale3D());
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

// ISDK 선택 해제와 별도로 소유 손을 유지함 (복구 뒤 SDK Move 가 없어도 재개, P1 §5)
struct FVBoRCarrySession
{
	int32 Hand = INDEX_NONE;
	bool bPaused = false;
	FTransform StartHand;
	FTransform StartObject;
	FQuat GrabObjectRotation = FQuat::Identity;
	FVector LastValidHand = FVector::ZeroVector;
	double GrabAt = 0;
	double HeldSeconds = 0;
	double PauseAt = 0;
	double PausedSeconds = 0;
	double RecoveryDistanceCm = 0;
	int32 PauseCount = 0;
	int32 IgnoredReleases = 0;
	bool Begin(int32 NewHand, const FTransform& HandPose, const FTransform& ObjectPose, double Time)
	{
		if (Hand != INDEX_NONE || NewHand < 0 || NewHand > 1) { return false; }
		Hand = NewHand;
		StartHand = HandPose;
		StartObject = ObjectPose;
		GrabObjectRotation = ObjectPose.GetRotation();
		LastValidHand = HandPose.GetLocation();
		GrabAt = Time;
		HeldSeconds = 0;
		return true;
	}
	void Pause(double Time)
	{
		if (Hand == INDEX_NONE || bPaused) { return; }
		bPaused = true;
		PauseAt = Time;
		++PauseCount;
	}
	bool Update(bool bTracking, const FTransform& HandPose, const FTransform& ObjectPose, double Time, double TiltLimitDeg, FTransform& Desired)
	{
		Desired = ObjectPose;
		if (Hand == INDEX_NONE) { return false; }
		HeldSeconds = FMath::Max(0.0, Time - GrabAt);
		if (!bTracking) { Pause(Time); return false; }
		if (bPaused)
		{
			// 첫 High frame 은 상대 자세만 다시 잡음 (끊긴 동안 이동을 적용하지 않음, P1 §4)
			RecoveryDistanceCm = FVector::Distance(LastValidHand, HandPose.GetLocation());
			PausedSeconds += FMath::Max(0.0, Time - PauseAt);
			StartHand = HandPose;
			StartObject = ObjectPose;
			LastValidHand = HandPose.GetLocation();
			bPaused = false;
			return false;
		}
		LastValidHand = HandPose.GetLocation();
		Desired = FVBoRCarryMotion::Follow(StartHand, HandPose, StartObject, TiltLimitDeg);
		// rebase 를 반복해도 최초 잡기 기준의 ±15 deg 를 넘지 않음 (P1 §2 회전)
		const FRotator FromGrab = (Desired.GetRotation() * GrabObjectRotation.Inverse()).Rotator();
		const FQuat Rotation = FRotator(FMath::Clamp(FromGrab.Pitch, -TiltLimitDeg, TiltLimitDeg), FromGrab.Yaw,
			FMath::Clamp(FromGrab.Roll, -TiltLimitDeg, TiltLimitDeg)).Quaternion() * GrabObjectRotation;
		const FQuat RebasedDelta = Rotation * StartObject.GetRotation().Inverse();
		Desired.SetRotation(Rotation);
		Desired.SetLocation(HandPose.GetLocation() + RebasedDelta.RotateVector(StartObject.GetLocation() - StartHand.GetLocation()));
		return true;
	}
	bool Release(int32 ReleasingHand, bool bTracking, double Time)
	{
		if (Hand == INDEX_NONE || ReleasingHand != Hand) { return false; }
		if (!bTracking || bPaused) { Pause(Time); ++IgnoredReleases; return false; }
		HeldSeconds = FMath::Max(0.0, Time - GrabAt);
		Hand = INDEX_NONE;
		return true;
	}
};
