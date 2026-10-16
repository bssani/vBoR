#pragma once

#include "CoreMinimal.h"

// + 는 윗면에서 시계 방향. 손 위치·기울임은 버림 (P5f D2·D3, deg)
struct FVBoRKnobMotion
{
	double AngleDeg = 0, RecoveryDiscardDeg = 0;
	double StepDeg = 15, StepHysteresisDeg = 3, JumpThresholdDeg = 30;
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
