#pragma once

#include "CoreMinimal.h"

struct VBOR_API FVBoRBuckHeadFrame
{
	bool bTracked = false;
	FVector PositionCm = FVector::ZeroVector;
	FQuat Orientation = FQuat::Identity;
	int32 TrackingOrigin = 0;
	FVector2D PlayAreaCm = FVector2D::ZeroVector;
};

struct VBOR_API FVBoRBuckCaptureResult
{
	FVector PositionCm = FVector::ZeroVector;
	double YawDeg = 0;
	double PositionSpreadMm = 0;
	double YawSpreadDeg = 0;
	int32 UsedFrames = 0;
	int32 DiscardedFrames = 0;
};

struct VBOR_API FVBoRBuckSavedAlignment
{
	static constexpr int32 FormatVersion = 1;
	FString SavedAt;
	int32 TrackingOrigin = 0;
	FVector2D PlayAreaCm = FVector2D::ZeroVector;
	FVBoRBuckCaptureResult Capture;
};

struct VBOR_API FVBoRBuckApplyDecision
{
	bool bApply = false;
	bool bNeedsCheck = false;
	FString Reason;
};

struct VBOR_API FVBoRBuckAlignment
{
	static bool ProjectYaw(const FQuat& Orientation, double MinHorizontalLength, double& OutYawDeg);
	static FTransform CalculatePawn(const FVBoRBuckCaptureResult& Capture, const FTransform& EyeWorld);
	static FVBoRBuckApplyDecision CheckEnvironment(const FVBoRBuckSavedAlignment& Saved, int32 CurrentOrigin,
		const FVector2D& CurrentPlayAreaCm, double ToleranceCm);
	static FString DescribeTip(bool bTracked, const FVector& WorldTipCm, const FTransform& EyeWorld);
};

struct VBOR_API FVBoRBuckCaptureWindow
{
	bool Begin(double DurationSeconds, bool bSave);
	bool Sample(double DeltaSeconds, const FVBoRBuckHeadFrame& Frame, double MinHorizontalLength);
	void Cancel(const FString& InReason);
	bool Finish(FVBoRBuckCaptureResult& Out) const;
	bool bActive = false;
	bool bSaving = false;
	double ElapsedSeconds = 0;
	double TargetSeconds = 0;
	int32 DiscardedFrames = 0;
	FString Reason;
private:
	TArray<FVector> Positions;
	TArray<double> Yaws;
};

struct VBOR_API FVBoRBuckAlignmentStore
{
	static bool Load(const FString& Path, FVBoRBuckSavedAlignment& Out, FString& Reason);
	static bool Save(const FString& Path, const FVBoRBuckSavedAlignment& Candidate, FString& Reason);
};
