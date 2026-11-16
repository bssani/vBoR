#pragma once

#include "CoreMinimal.h"

// + = positive pivot +Z rotation (front → right). Ignore hand position/tilt (P5f D2/R1-1, deg)
struct FVBoRKnobMotion
{
	double AngleDeg = 0, RecoveryDiscardDeg = 0;
	double StepDeg = 15, StepHysteresisDeg = 3, JumpThresholdDeg = 30;
	double TwistGain = 1;
	int64 StepIndex = 0;
	int32 GrabHand = INDEX_NONE;
	bool bPaused = false;
	TArray<int32> PendingSteps;
	int32 PendingSoundSteps = 0;
	bool BeginGrab(int32 Hand, const FQuat& Palm);
	void AdvanceGrab(bool bTracked, const FQuat& Palm, double SoundStepDeg = 0);
	void ReleaseGrab();
private:
	FQuat LastPalm = FQuat::Identity;
	double SoundRemainderDeg = 0;
};
