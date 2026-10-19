#pragma once

#include "CoreMinimal.h"

// + 는 피벗 +Z 양의 회전 (앞 → 오른쪽). 손 위치·기울임은 버림 (P5f D2·R1-1, deg)
struct FVBoRKnobMotion
{
	double AngleDeg = 0, RecoveryDiscardDeg = 0;
	double StepDeg = 15, StepHysteresisDeg = 3, JumpThresholdDeg = 30;
	double TwistGain = 1;
	int64 StepIndex = 0;
	int32 GrabHand = INDEX_NONE;
	bool bPaused = false;
	TArray<int32> PendingSteps;
	bool BeginGrab(int32 Hand, const FQuat& Palm);
	void AdvanceGrab(bool bTracked, const FQuat& Palm);
	void ReleaseGrab();
private:
	FQuat LastPalm = FQuat::Identity;
};
