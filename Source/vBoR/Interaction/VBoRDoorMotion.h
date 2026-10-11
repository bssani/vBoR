#pragma once

#include "CoreMinimal.h"

// HingePivot 의 닫힌 좌표계: +X 뒤 끝, +Y 바깥, +Z 경첩 축 (P2 D6, cm)
struct FVBoRDoorHand
{
	bool bTracked = false;
	FVector Points[5] = {}; // Palm, Index, Middle, Ring, Little
	FVector PalmNormal = FVector::ZeroVector;
};

enum class EVBoRDoorIgnore : uint8 { Locked, PalmDirection, Behind, NearAxis, Outside, TrackingLow, Count };

struct FVBoRDoorMetrics
{
	int32 Unlocks = 0, Latches = 0, LastUnlockHandle = INDEX_NONE;
	int32 PushStarts[2][2] = {}; // 손, 면 (0 바깥 / 1 안쪽)
	int32 Ignored[static_cast<int32>(EVBoRDoorIgnore::Count)] = {};
	int32 Attempts[2][2] = {}, Grabs[2][2] = {}; // 손, 손잡이
	int32 RateLimitedFrames = 0, Pauses = 0, IgnoredReleases = 0;
	double PushedDeg = 0, HeldSeconds = 0, PausedSeconds = 0, RecoveryDistanceCm = 0;
	FVBoRDoorMetrics Since(const FVBoRDoorMetrics& Before) const;
};

struct FVBoRDoorMotion
{
	double WidthCm = 110, HeightCm = 105, ThicknessCm = 10;
	double MaxAngleDeg = 70, MaxSpeedDegPerSecond = 180;
	double LatchAngleDeg = 2, RearmAngleDeg = 5;
	double PalmLimitDeg = 60, MinAxisDistanceCm = 15, FrontToleranceCm = 0.5;
	double AngleDeg = 0;
	bool bLocked = true, bLatchArmed = false, bPaused = false;
	int32 GrabHand = INDEX_NONE, GrabHandle = INDEX_NONE;
	int32 PushHand = INDEX_NONE, PushFace = INDEX_NONE;
	double PalmAngleDeg[2] = {-1, -1};
	FVBoRDoorMetrics Metrics;
	bool BeginGrab(int32 Hand, int32 Handle, const FVBoRDoorHand& Input);
	bool ReleaseGrab(bool bTracked);
	void Advance(const FVBoRDoorHand (&Hands)[2], double DeltaSeconds);
	void Reset(); // 누적은 유지함 (P2 E18)
	FVector ToDoor(const FVector& HingePoint) const;
private:
	FVBoRDoorHand Previous[2];
	bool bHavePrevious[2] = {false, false};
	double PreviousAngleDeg = 0;
	FVector LastGrabPoint = FVector::ZeroVector;
	double GrabReferenceBearingDeg = 0, GrabReferenceAngleDeg = 0;
	void RebaseGrab(const FVector& Point);
	bool FindPush(int32 Hand, const FVBoRDoorHand& Input, int32 Face, double& OutAngle);
	void MoveTo(double Desired, double DeltaSeconds, bool bPushing);
};
