#include "VBoRKnobMotion.h"

bool FVBoRKnobMotion::BeginGrab(int32 Hand, const FQuat& Palm)
{
	if (Hand < 0 || Hand > 1 || GrabHand != INDEX_NONE || Palm.ContainsNaN() || !Palm.IsNormalized()) { return false; }
	GrabHand = Hand; LastPalm = Palm; bPaused = false; return true;
}

void FVBoRKnobMotion::AdvanceGrab(bool bTracked, const FQuat& Palm)
{
	if (GrabHand == INDEX_NONE) { return; }
	if (!bTracked || Palm.ContainsNaN() || !Palm.IsNormalized()) { bPaused = true; return; }
	const FQuat Delta = (Palm * LastPalm.Inverse()).GetNormalized();
	// 피벗 +Z 음의 회전이 윗면 시계 방향임 (P5f D2·D3, rad → deg)
	const double TwistDeg = -FMath::RadiansToDegrees(Delta.GetTwistAngle(FVector::ZAxisVector));
	if (bPaused) { RecoveryDiscardDeg = FMath::Abs(TwistDeg); }
	else if (FMath::Abs(TwistDeg) <= JumpThresholdDeg + 1.e-6)
	{
		AngleDeg += TwistDeg;
		if (StepDeg > 0)
		{
			// H=0 의 정확한 경계에서도 같은 frame 에 왕복 알림이 나지 않게 방향별 처리함 (P5f D4)
			if (TwistDeg > 0)
			{
				while (AngleDeg >= (static_cast<double>(StepIndex)+0.5)*StepDeg+StepHysteresisDeg) { ++StepIndex; PendingSteps.Add(1); }
			}
			else if (TwistDeg < 0)
			{
				while (AngleDeg <= (static_cast<double>(StepIndex)-0.5)*StepDeg-StepHysteresisDeg) { --StepIndex; PendingSteps.Add(-1); }
			}
		}
	}
	// 튐·복구도 기준만 옮김 (다음 frame 에 누적되지 않게, P5f D3·D6)
	LastPalm = Palm; bPaused = false;
}

void FVBoRKnobMotion::ReleaseGrab()
{
	if (GrabHand == INDEX_NONE) { return; }
	if (StepDeg > 0) { AngleDeg = static_cast<double>(StepIndex)*StepDeg; }
	GrabHand = INDEX_NONE; bPaused = false;
}
