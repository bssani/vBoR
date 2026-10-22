#pragma once

#include "CoreMinimal.h"

// Closed HingePivot coordinates: +X trailing edge, +Y outside, +Z hinge axis (P2 D6, cm)
struct FVBoRDoorHand
{
	bool bTracked = false;
	FVector Points[5] = {}; // Palm, Index, Middle, Ring, Little
	FVector PalmNormal = FVector::ZeroVector;
};

enum class EVBoRDoorIgnore : uint8 { Locked, PalmDirection, Behind, NearAxis, Outside, TrackingLow, OpeningDisabled, Count };

struct FVBoRDoorMetrics
{
	int32 Unlocks = 0, Latches = 0, LastUnlockHandle = INDEX_NONE;
	int32 PushStarts[2][2] = {}; // Hand, face (0 outside / 1 inside)
	int32 Ignored[static_cast<int32>(EVBoRDoorIgnore::Count)] = {};
	int32 Attempts[2][2] = {}, Grabs[2][2] = {}; // Hand, handle
	int32 RateLimitedFrames = 0, Pauses = 0, IgnoredReleases = 0, PushTrackingLosses = 0;
	double PushedDeg = 0, HeldSeconds = 0, PausedSeconds = 0, RecoveryDistanceCm = 0;
	FVBoRDoorMetrics Since(const FVBoRDoorMetrics& Before) const;
};

struct FVBoRDoorMotion
{
	double ThicknessCm = 10;
	FVector FaceMinCm = FVector(0,-5,0), FaceMaxCm = FVector(110,5,105);
	double MaxAngleDeg = 70, MaxSpeedDegPerSecond = 180;
	double LatchAngleDeg = 2, RearmAngleDeg = 5;
	double PalmLimitDeg = 60, MinAxisDistanceCm = 15, FrontToleranceCm = 0.5;
	double AngleDeg = 0;
	bool bUseLatch = true; // Lock enabled for existing door, disabled for shared panel (P2c D2)
	bool bAllowPalmOpening = true;
	bool bLocked = true, bLatchArmed = false, bPaused = false;
	int32 GrabHand = INDEX_NONE, GrabHandle = INDEX_NONE;
	int32 PushHand = INDEX_NONE, PushFace = INDEX_NONE;
	double PalmAngleDeg[2] = {-1, -1};
	FVBoRDoorMetrics Metrics;
	// Door-space point/face → face Y (cm). Empty query keeps existing plane (P2g D5)
	TFunction<bool(const FVector&, int32, double&)> QueryFace;
	bool BeginGrab(int32 Hand, int32 Handle, const FVBoRDoorHand& Input);
	bool Unlock(int32 Handle = INDEX_NONE);
	bool ReleaseGrab(bool bTracked);
	void Advance(const FVBoRDoorHand (&Hands)[2], double DeltaSeconds);
	void Reset(); // Keep totals (P2 E18)
	FVector ToDoor(const FVector& HingePoint) const;
private:
	FVBoRDoorHand Previous[2];
	bool bHavePrevious[2] = {false, false};
	bool bContactNear[2] = {false, false};
	int32 ContactFace[2] = {INDEX_NONE, INDEX_NONE};
	EVBoRDoorIgnore LastIgnored[2] = {EVBoRDoorIgnore::Count, EVBoRDoorIgnore::Count};
	double PreviousAngleDeg = 0;
	FVector LastGrabPoint = FVector::ZeroVector;
	double GrabReferenceBearingDeg = 0, GrabReferenceAngleDeg = 0;
	void RebaseGrab(const FVector& Point);
	bool FindPush(int32 Hand, const FVBoRDoorHand& Input, int32 Face, double& OutAngle, EVBoRDoorIgnore& OutReason);
	void RecordIgnore(int32 Hand, EVBoRDoorIgnore Reason);
	void MoveTo(double Desired, double DeltaSeconds, bool bPushing);
};
