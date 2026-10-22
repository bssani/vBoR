#pragma once

#include "CoreMinimal.h"
#include "HeadMountedDisplayTypes.h"

class AVBoRScenarioRunner;
struct FVBoRScenarioStep;

struct FVBoRSpectatorLabels
{
	FText Ready = FText::FromString(TEXT("GETTING READY"));
	FText Mission = FText::FromString(TEXT("MISSION {0} / {1}"));
	FText Free = FText::FromString(TEXT("FREE MODE"));
	FText Completed = FText::FromString(TEXT("All missions complete. Explore the vehicle freely."));
	FText Explore = FText::FromString(TEXT("Explore the vehicle freely."));
	FText Unknown = FText::FromString(TEXT("Mission in progress"));
};

struct FVBoRSpectatorBanner
{
	FText Label, Mission;
	int32 Completed = 0, Total = 0;
	bool bShowProgress = true;
	float Progress() const { return Total > 0 ? static_cast<float>(Completed) / Total : 0; }
};

namespace VBoRSpectator
{
	FText MissionText(const FVBoRScenarioStep& Step, const FText& Unknown);
	FVBoRSpectatorBanner Banner(const AVBoRScenarioRunner& Runner, const FVBoRSpectatorLabels& Labels);
}

// Keep prior usage/release time per name (momentary buttons/shared part names, P5h D6/D7)
struct FVBoRSpectatorNames
{
	void Sample(const TArray<FString>& ActiveNames, double NowSeconds, double HoldSeconds);
	FString Line(const FString& Prefix) const;
	TArray<FString> RecentNames() const;
private:
	struct FEntry
	{
		FString Name;
		double ReleasedAt = 0;
		uint64 Order = 0;
		bool bActive = false;
	};
	TArray<FEntry> Entries;
	uint64 NextOrder = 0;
};

// No engine layout update if window size/ratio unchanged (P5h E15/E16)
struct FVBoRSpectatorLayout
{
	bool Update(FIntPoint WindowSize, float EyeAspect, float MinPanelWidth);
	FSpectatorScreenModeTexturePlusEyeLayout Layout;
	FIntPoint PanelPixels = FIntPoint::ZeroValue;
	float DrawScale = 1;
	FVector2D DesignSize() const { return FVector2D(PanelPixels) / DrawScale; }
private:
	FIntPoint LastSize = FIntPoint::ZeroValue;
	float LastAspect = 0, LastPanelWidth = 0;
};
