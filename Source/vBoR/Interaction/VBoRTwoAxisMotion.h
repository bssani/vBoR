#pragma once

#include "CoreMinimal.h"
#include "VBoRDoorMotion.h"

// 피벗 로컬 -X 가 앞면, +Z 좌우·-Y 위아래 회전임 (P5e D1·D8, cm·deg)
struct FVBoRTwoAxisMotion
{
	FVector FaceMinCm = FVector(-4,-12.5,-3.5), FaceMaxCm = FVector(-2,12.5,3.5);
	double YawLimitDeg = 25, PitchLimitDeg = 15;
	double CenterExclusionCm = 2, FrontToleranceCm = 0.5, DepthMarginCm = 1, EdgeMarginCm = 0.5;
	double MinLeverCm = 10, MaxGrabJumpCm = 5;
	double PushMinLeverCm = 4;
	double YawDeg = 0, PitchDeg = 0, RecoveryDiscardCm = 0;
	int32 GrabHand = INDEX_NONE, PushHand = INDEX_NONE, PushPoint = INDEX_NONE, PushFace = INDEX_NONE;
	bool bPaused = false;
	FQuat Rotation() const;
	bool BeginGrab(int32 Hand, const FVector& Point);
	void ReleaseGrab();
	void AdvanceGrab(bool bTracked, const FVector& Point);
	void AdvancePush(const FVBoRDoorHand (&Hands)[2]);
	void ClearPush();
private:
	FVector LastGrabPoint = FVector::ZeroVector;
	FQuat PreviousRotation = FQuat::Identity;
	FVBoRDoorHand Previous[2];
	bool FindPush(int32 Hand, const FVBoRDoorHand& Input, FVector& Point, double& Depth, int32& PointIndex, int32& FaceIndex) const;
};
