#pragma once

#include "CoreMinimal.h"

struct FP0ProbeSample
{
	double Time;
	FVector Position;
};

struct FP0ProbeMeasurement
{
	bool bInitialized = false;
	bool bPositionValid = false;
	bool bHighConfidence = false;
	bool bHasLastValidPosition = false;
	FVector LastValidPosition = FVector::ZeroVector;
	double ConfidenceLostAt = -1.0;
	double ConfidenceRecoveredAt = -1.0;
	double ConfidenceLossDuration = 0.0;
	double PositionLostAt = -1.0;
	double PositionRecoveredAt = -1.0;
	double PositionLossDuration = 0.0;
	double RecoveryDistanceMm = 0.0;
	bool bHasRecoveryDistance = false;
	bool bHasLastHighPosition = false;
	FVector LastHighPosition = FVector::ZeroVector;
	double ConfidenceJumpMm = 0.0;
	bool bHasConfidenceJump = false;
	bool bHasLowStartPosition = false;
	FVector LowStartPosition = FVector::ZeroVector;
	double LowMaxMoveMm = 0.0;
	double DiameterMm = 0.0;
	TArray<FP0ProbeSample> Samples;

	TArray<FString> Update(double Time, bool bValid, bool bHigh, const FVector& IndexPosition, bool bBonesAvailable, double MmPerUnit);
};
