#pragma once

#include "CoreMinimal.h"

struct FP0ButtonPressMeasurement
{
	bool bHover = false;
	bool bSelected = false;
	bool bHasDepth = false;
	double HoverAt = -1.0;
	double HoverToSelectSeconds = -1.0;
	double MaxDepthMm = 0.0;
	void Hover(double Time)
	{
		if (bHover || bSelected) { return; }
		bHover = true;
		HoverAt = Time;
		HoverToSelectSeconds = -1.0;
		MaxDepthMm = 0.0;
		bHasDepth = false;
	}
	void Select(double Time)
	{
		if (bSelected) { return; }
		HoverToSelectSeconds = bHover ? FMath::Max(0.0, Time - HoverAt) : -1.0;
		bSelected = true;
	}
	bool Unselect()
	{
		if (!bSelected) { return false; }
		bSelected = false;
		bHover = false;
		return true;
	}
	void Unhover() { bHover = false; }
	void Cancel() { bHover = false; bSelected = false; }
	void Sample(bool bValid, const FVector& Tip, const FVector& Front, const FVector& Normal, double MmPerUnit)
	{
		// 무효 추적과 완료된 누름에는 깊이를 더하지 않음 (P0-b §2, U-06)
		if (!bValid || (!bHover && !bSelected)) { return; }
		bHasDepth = true;
		MaxDepthMm = FMath::Max(MaxDepthMm, FMath::Max(0.0, FVector::DotProduct(Front - Tip, Normal.GetSafeNormal()) * MmPerUnit));
	}
};

struct FP0ButtonHoverMeasurement
{
	int32 Current = 0;
	int32 Maximum = 0;
	int32 MultipleEpisodes = 0;
	void Update(int32 HoverCount)
	{
		if (Current < 2 && HoverCount >= 2) { ++MultipleEpisodes; }
		Current = HoverCount;
		Maximum = FMath::Max(Maximum, Current);
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
