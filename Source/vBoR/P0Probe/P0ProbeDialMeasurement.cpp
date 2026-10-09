#include "P0ProbeDialMeasurement.h"

void FP0ProbeDialMeasurement::ResetAngles()
{
	DialAngleDeg = 0.0;
	WristTwistDeg = 0.0;
	ThumbLowCount = 0;
	IndexLowCount = 0;
	HeldCount = 0;
	// pinch 직후 첫 측정은 새 기준만 잡음 (이전 frame 이동 미적용, P0-a §2)
	bHasReference = false;
	bUpdating = false;
}

TArray<FString> FP0ProbeDialMeasurement::Update(double Time, bool bPositionValid, bool bThumbHighNow, bool bIndexHighNow, bool bBonesReady,
	const FVector& ThumbPosition, const FVector& IndexPosition, const FQuat& WristRotation,
	const FVector& WorldAxis, double MmPerUnit)
{
	TArray<FString> Events;
	// ── 1) 날것의 거리·투영 측정 (신뢰도 Low 중에도 수치는 비교 가능, P0-a §2) ──
	const FVector Axis = WorldAxis.GetSafeNormal();
	bHasDistances = bPositionValid && bBonesReady && !ThumbPosition.ContainsNaN() && !IndexPosition.ContainsNaN();
	const FVector TipVector = bHasDistances ? IndexPosition - ThumbPosition : FVector::ZeroVector;
	const FVector Projection = TipVector - Axis * FVector::DotProduct(TipVector, Axis);
	// UE units -> mm. 기본 WorldToMeters=100 에서 10 배 (P0-a §2)
	TipDistanceMm = TipVector.Size() * MmPerUnit;
	ProjectedLengthMm = Projection.Size() * MmPerUnit;
	bUpdating = bHasDistances && bThumbHighNow && bIndexHighNow && !Axis.IsNearlyZero()
		&& !WristRotation.ContainsNaN() && WristRotation.SizeSquared() > UE_SMALL_NUMBER
		&& ProjectedLengthMm >= MinProjectedLengthMm;
	// 초기값·pinch reset 은 전환 아님. 직전 측정 상태로 비교 (P0-a §2, R2-1)
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
		// 유효하지 않은 구간 이동은 누적하지 않음. 복구 frame 에 rebase (P0-a §4-6)
		bHasReference = false;
		Samples.Reset();
		return Events;
	}
	const FVector Direction = Projection / Projection.Size();
	const FQuat Wrist = WristRotation.GetNormalized();
	if (!Axis.Equals(PreviousAxis, UE_SMALL_NUMBER))
	{
		// 에디터에서 축을 바꾸면 서로 다른 투영면을 비교하지 않음 (P0-a §2)
		bHasReference = false;
		Samples.Reset();
	}

	// ── 2) 부호 있는 frame 변화량 누적 (±180 deg 경계 unwrap, P0-a §2) ──
	if (bHasReference)
	{
		const double SinAngle = FVector::DotProduct(Axis, FVector::CrossProduct(PreviousDirection, Direction));
		const double CosAngle = FVector::DotProduct(PreviousDirection, Direction);
		DialAngleDeg += FMath::RadiansToDegrees(FMath::Atan2(SinAngle, CosAngle));
		// 월드 회전 차이를 축 둘레 twist 와 나머지 swing 으로 분리 (P0-a §3)
		const FQuat WristDelta = (Wrist * PreviousWrist.Inverse()).GetNormalized();
		FQuat Swing;
		FQuat Twist;
		WristDelta.ToSwingTwist(Axis, Swing, Twist);
		// q 와 -q 는 같은 회전. W >= 0 반구로 맞춰 최단 부호 각도 사용 (P0-a §4-6)
		// GetTwistAngle 의 float 2π 감기 오차를 피하려고 atan2 로 계산함 (P0-a §3)
		const double Sign = Twist.W < 0.0 ? -1.0 : 1.0;
		const double TwistAlongAxis = FVector::DotProduct(Axis, FVector(Twist.X, Twist.Y, Twist.Z));
		WristTwistDeg += FMath::RadiansToDegrees(2.0 * FMath::Atan2(Sign * TwistAlongAxis, Sign * Twist.W));
	}
	PreviousDirection = Direction;
	PreviousWrist = Wrist;
	PreviousAxis = Axis;
	bHasReference = true;

	// ── 3) 투영 벡터의 1 s 각도 지름 (누적값 reset 과 무관한 날것의 흔들림, P0-a §2) ──
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
