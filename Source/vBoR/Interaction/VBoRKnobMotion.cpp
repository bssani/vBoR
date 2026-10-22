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
	// Positive pivot +Z rotation goes front to right (P5f D2/R1-1, rad → deg)
	const double TwistDeg = FMath::RadiansToDegrees(Delta.GetTwistAngle(FVector::ZAxisVector));
	if (bPaused) { RecoveryDiscardDeg = FMath::Abs(TwistDeg); }
	else if (FMath::Abs(TwistDeg) <= JumpThresholdDeg + 1.e-6)
	{
		// Real hand angle for jump/recovery; multiplier for motion only (P5n D2, deg)
		AngleDeg += TwistDeg * TwistGain;
		if (StepDeg > 0)
		{
			// Directional handling at exact H=0 boundary (no round-trip notifications in same frame, P5f D4)
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
	// Rebase on jump/recovery too (no next-frame accumulation, P5f D3/D6)
	LastPalm = Palm; bPaused = false;
}

void FVBoRKnobMotion::ReleaseGrab()
{
	if (GrabHand == INDEX_NONE) { return; }
	if (StepDeg > 0) { AngleDeg = static_cast<double>(StepIndex)*StepDeg; }
	GrabHand = INDEX_NONE; bPaused = false;
}
