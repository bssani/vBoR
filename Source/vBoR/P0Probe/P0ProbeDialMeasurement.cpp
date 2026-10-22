#include "P0ProbeDialMeasurement.h"

void FP0ProbeDialMeasurement::ResetAngles()
{
	DialAngleDeg = 0.0;
	WristTwistDeg = 0.0;
	ThumbLowCount = 0;
	IndexLowCount = 0;
	HeldCount = 0;
	// First sample after pinch rebases only (no previous-frame motion, P0-a §2)
	bHasReference = false;
	bUpdating = false;
}

TArray<FString> FP0ProbeDialMeasurement::Update(double Time, bool bPositionValid, bool bThumbHighNow, bool bIndexHighNow, bool bBonesReady,
	const FVector& ThumbPosition, const FVector& IndexPosition, const FQuat& WristRotation,
	const FVector& WorldAxis, double MmPerUnit)
{
	TArray<FString> Events;
	// ── 1) Raw distance/projection (comparable even at Low confidence, P0-a §2) ──
	const FVector Axis = WorldAxis.GetSafeNormal();
	bHasDistances = bPositionValid && bBonesReady && !ThumbPosition.ContainsNaN() && !IndexPosition.ContainsNaN();
	const FVector TipVector = bHasDistances ? IndexPosition - ThumbPosition : FVector::ZeroVector;
	const FVector Projection = TipVector - Axis * FVector::DotProduct(TipVector, Axis);
	// UE units -> mm. 10x at default WorldToMeters=100 (P0-a §2)
	TipDistanceMm = TipVector.Size() * MmPerUnit;
	ProjectedLengthMm = Projection.Size() * MmPerUnit;
	bUpdating = bHasDistances && bThumbHighNow && bIndexHighNow && !Axis.IsNearlyZero()
		&& !WristRotation.ContainsNaN() && WristRotation.SizeSquared() > UE_SMALL_NUMBER
		&& ProjectedLengthMm >= MinProjectedLengthMm;
	// Initial/pinch reset is no transition. Compare previous measurement state (P0-a §2, R2-1)
	if (bTransitionsInitialized)
	{
		if (bThumbHighNow != bThumbHigh)
		{
			ThumbLowCount += bThumbHighNow ? 0 : 1;
			Events.Add(FString::Printf(TEXT("Thumb confidence %s -> %s"), bThumbHigh ? TEXT("High") : TEXT("Low"), bThumbHighNow ? TEXT("High") : TEXT("Low")));
		}
		if (bIndexHighNow != bIndexHigh)
		{
			IndexLowCount += bIndexHighNow ? 0 : 1;
			Events.Add(FString::Printf(TEXT("Index confidence %s -> %s"), bIndexHigh ? TEXT("High") : TEXT("Low"), bIndexHighNow ? TEXT("High") : TEXT("Low")));
		}
		if (bUpdating != bLastUpdating)
		{
			HeldCount += bUpdating ? 0 : 1;
			Events.Add(FString::Printf(TEXT("dial %s -> %s"), bLastUpdating ? TEXT("updating") : TEXT("held"), bUpdating ? TEXT("updating") : TEXT("held")));
		}
	}
	bThumbHigh = bThumbHighNow;
	bIndexHigh = bIndexHighNow;
	bLastUpdating = bUpdating;
	bTransitionsInitialized = true;
	DiameterDeg = 0.0;
	if (!bUpdating)
	{
		// No invalid-interval accumulation. Rebase on recovery frame (P0-a §4-6)
		bHasReference = false;
		Samples.Reset();
		return Events;
	}
	const FVector Direction = Projection / Projection.Size();
	const FQuat Wrist = WristRotation.GetNormalized();
	if (!Axis.Equals(PreviousAxis, UE_SMALL_NUMBER))
	{
		// No comparison across different projection planes after editor axis change (P0-a §2)
		bHasReference = false;
		Samples.Reset();
	}

	// ── 2) Accumulate signed frame delta (unwrap ±180 deg, P0-a §2) ──
	if (bHasReference)
	{
		const double SinAngle = FVector::DotProduct(Axis, FVector::CrossProduct(PreviousDirection, Direction));
		const double CosAngle = FVector::DotProduct(PreviousDirection, Direction);
		DialAngleDeg += FMath::RadiansToDegrees(FMath::Atan2(SinAngle, CosAngle));
		// Split world rotation delta into axis twist and remaining swing (P0-a §3)
		const FQuat WristDelta = (Wrist * PreviousWrist.Inverse()).GetNormalized();
		FQuat Swing;
		FQuat Twist;
		WristDelta.ToSwingTwist(Axis, Swing, Twist);
		// q and -q equivalent. Use W >= 0 hemisphere for shortest signed angle (P0-a §4-6)
		// Use atan2 (avoid GetTwistAngle float 2π wrap error, P0-a §3)
		const double Sign = Twist.W < 0.0 ? -1.0 : 1.0;
		const double TwistAlongAxis = FVector::DotProduct(Axis, FVector(Twist.X, Twist.Y, Twist.Z));
		WristTwistDeg += FMath::RadiansToDegrees(2.0 * FMath::Atan2(Sign * TwistAlongAxis, Sign * Twist.W));
	}
	PreviousDirection = Direction;
	PreviousWrist = Wrist;
	PreviousAxis = Axis;
	bHasReference = true;

	// ── 3) 1 s projection-vector angular diameter (raw jitter independent of total reset, P0-a §2) ──
	Samples.RemoveAll([Time](const FP0ProbeDialSample& Sample) { return Time - Sample.Time > 1.0; });
	Samples.Add({ Time, Direction });
	for (int32 A = 0; A < Samples.Num(); ++A)
	{
		for (int32 B = A + 1; B < Samples.Num(); ++B)
		{
			const FVector& First = Samples[A].Direction;
			const FVector& Second = Samples[B].Direction;
			const double Angle = FMath::RadiansToDegrees(FMath::Atan2(FVector::CrossProduct(First, Second).Size(), FVector::DotProduct(First, Second)));
			DiameterDeg = FMath::Max(DiameterDeg, Angle);
		}
	}
	return Events;
}
