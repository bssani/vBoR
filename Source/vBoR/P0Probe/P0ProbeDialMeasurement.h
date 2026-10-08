#pragma once

#include "CoreMinimal.h"

struct FP0ProbeDialSample
{
	double Time;
	FVector Direction;
};

struct FP0ProbeDialMeasurement
{
	// 0 근처의 투영은 각도가 정의되지 않음. 수치 안정용이며 합격 기준 아님 (P0-a §3)
	static constexpr double MinProjectedLengthMm = 0.1;
	bool bUpdating = false;
	bool bHasReference = false;
	bool bHasDistances = false;
	double TipDistanceMm = 0.0;
	double ProjectedLengthMm = 0.0;
	double DialAngleDeg = 0.0;
	double WristTwistDeg = 0.0;
	double DiameterDeg = 0.0;
	FVector PreviousDirection = FVector::ZeroVector;
	FVector PreviousAxis = FVector::ZeroVector;
	FQuat PreviousWrist = FQuat::Identity;
	TArray<FP0ProbeDialSample> Samples;

	void ResetAngles();
	void Update(double Time, bool bPositionValid, bool bThumbHigh, bool bIndexHigh, bool bBonesReady,
		const FVector& ThumbPosition, const FVector& IndexPosition, const FQuat& WristRotation,
		const FVector& WorldAxis, double MmPerUnit);
};
