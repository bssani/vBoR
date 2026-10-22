#pragma once

#include "CoreMinimal.h"
#include "StructTypes.h"

enum class EP0ButtonSampleResult { Ignored, Updated, TrackingCanceled };

struct FP0ButtonPressMeasurement
{
	bool bHover = false;
	bool bSelected = false;
	bool bHasDepth = false;
	bool bHasDepthAtSelect = false;
	double HoverAt = -1.0;
	double HoverToSelectSeconds = -1.0;
	double MaxDepthMm = 0.0;
	double DepthAtSelectMm = 0.0;
	int32 TrackingCanceled = 0;
	int32 Missed = 0;
	void Hover(double Time)
	{
		if (bHover || bSelected) { return; }
		bHover = true;
		HoverAt = Time;
		HoverToSelectSeconds = -1.0;
		MaxDepthMm = 0.0;
		bHasDepth = false;
		bHasDepthAtSelect = false;
		DepthAtSelectMm = 0.0;
	}
	bool Select(double Time)
	{
		if (bSelected) { return false; }
		HoverToSelectSeconds = bHover ? FMath::Max(0.0, Time - HoverAt) : -1.0;
		bHasDepthAtSelect = false;
		bSelected = true;
		return true;
	}
	bool Unselect()
	{
		if (!bSelected) { return false; }
		bSelected = false;
		bHover = false;
		return true;
	}
	bool Unhover()
	{
		const bool bMissed = bHover && !bSelected;
		if (bMissed) { ++Missed; }
		bHover = false;
		return bMissed;
	}
	void Cancel() { bHover = false; bSelected = false; }
	EP0ButtonSampleResult Sample(bool bValid, const FVector& Tip, const FVector& Front, const FVector& Normal,
		double MmPerUnit, bool bAtSelect = false)
	{
		if (!bHover && !bSelected) { return EP0ButtonSampleResult::Ignored; }
		if (bAtSelect) { bHasDepthAtSelect = bValid; }
		// Count tracking loss once per measurement (exclude repeated invalid frames, P0-b R1-5)
		if (!bValid) { ++TrackingCanceled; Cancel(); return EP0ButtonSampleResult::TrackingCanceled; }
		const double SignedDepth = FVector::DotProduct(Front - Tip, Normal.GetSafeNormal()) * MmPerUnit;
		// Preserve sign at select (compare ISDK/raw fingertip difference, P0-b R1-4)
		if (bAtSelect) { DepthAtSelectMm = SignedDepth; }
		bHasDepth = true;
		MaxDepthMm = FMath::Max(MaxDepthMm, FMath::Max(0.0, SignedDepth));
		return EP0ButtonSampleResult::Updated;
	}
};

struct FP0ButtonAttemptTotals
{
	int32 TrackingCanceled = 0;
	int32 MissedSingle = 0;
	int32 ClusterPass = 0;
	static FP0ButtonAttemptTotals Calculate(TConstArrayView<FP0ButtonPressMeasurement> Presses, int32 ClusterStart, int32 FarIndex)
	{
		FP0ButtonAttemptTotals Totals;
		for (int32 Index = 0; Index < Presses.Num(); ++Index)
		{
			Totals.TrackingCanceled += Presses[Index].TrackingCanceled;
			// Separate neighbor-crossing hover from single-button miss. Count far-button hover separately (P0-b R2-2)
			if (Index < ClusterStart) { Totals.MissedSingle += Presses[Index].Missed; }
			else if (Index < FarIndex) { Totals.ClusterPass += Presses[Index].Missed; }
		}
		return Totals;
	}
};

struct FP0ButtonActiveMeasurement
{
	int32 Current = 0;
	int32 Maximum = 0;
	int32 MultipleEpisodes = 0;
	static int32 CountActive(TConstArrayView<EIsdkInteractableState> States)
	{
		int32 Count = 0;
		for (const auto State : States)
		{
			if (State == EIsdkInteractableState::Hover || State == EIsdkInteractableState::Select) { ++Count; }
		}
		return Count;
	}
	void Update(int32 ActiveCount)
	{
		if (Current < 2 && ActiveCount >= 2) { ++MultipleEpisodes; }
		Current = ActiveCount;
		Maximum = FMath::Max(Maximum, Current);
	}
};

struct FP0ButtonBenchPlacement
{
	static void CancelMeasurements(TArrayView<FP0ButtonPressMeasurement> Presses)
	{
		for (auto& Press : Presses) { Press.Cancel(); }
	}
	static FTransform Calculate(const FTransform& Head, double ForwardCm, double BelowCm)
	{
		// Exclude Pitch/Roll (fixed horizontal forward even when looking down, P0-b R1-3)
		const FRotator Yaw(0, Head.Rotator().Yaw, 0);
		return FTransform(Yaw, Head.GetLocation() + Yaw.RotateVector(FVector(ForwardCm, 0, 0)) - FVector(0, 0, BelowCm));
	}
};

struct FP0ButtonTargetMeasurement
{
	int32 LastTarget = INDEX_NONE;
	int32 Switches = 0;
	void Hover(int32 Target)
	{
		if (LastTarget != INDEX_NONE && LastTarget != Target) { ++Switches; }
		LastTarget = Target;
	}
	void Reset() { LastTarget = INDEX_NONE; Switches = 0; }
};
