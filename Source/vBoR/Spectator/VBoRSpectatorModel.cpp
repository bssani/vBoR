#include "VBoRSpectatorModel.h"
#include "../Interaction/VBoRScenarioRunner.h"

FText VBoRSpectator::MissionText(const FVBoRScenarioStep& Step, const FText& Unknown)
{
	if (!Step.SpectatorText.IsEmpty()) { return Step.SpectatorText; }
	// New fields empty in saved assets; use ID defaults (P5h D5)
	static const TCHAR* Texts[] = {
		TEXT("Approach the vehicle with the key."), TEXT("Check the exterior lights as you approach."),
		TEXT("Walk around the vehicle."), TEXT("Open the liftgate."), TEXT("Load the boxes into the trunk."),
		TEXT("Close the liftgate."), TEXT("Operate the door handle."), TEXT("Open the door."),
		TEXT("Get into the vehicle."), TEXT("Sit in the seat."), TEXT("Close the door."),
		TEXT("Look around the interior."), TEXT("Reach for the farthest button."),
		TEXT("Check the view from the passenger seat."), TEXT("Check the view from the rear seat."),
		TEXT("Check the seat folding angle."), nullptr, TEXT("Open the door to get out."),
		TEXT("Get out of the vehicle."), TEXT("Close the door.")};
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Texts); ++Index)
	{
		if (Texts[Index] && Step.StepId == FName(*FString::Printf(TEXT("%02d"), Index + 1)))
		{ return FText::FromString(Texts[Index]); }
	}
	return Unknown;
}

FVBoRSpectatorBanner VBoRSpectator::Banner(const AVBoRScenarioRunner& Runner, const FVBoRSpectatorLabels& Labels)
{
	FVBoRSpectatorBanner Result;
	Result.Total = Runner.Scenario ? Runner.Scenario->Steps.Num() : 0;
	Result.Label = Labels.Ready;
	if (Runner.IsFreeMode())
	{
		Result.Label = Labels.Free;
		Result.Mission = Runner.IsFinished() ? Labels.Completed : Labels.Explore;
		Result.Completed = Runner.IsFinished() ? Result.Total : 0;
		Result.bShowProgress = Runner.IsFinished();
	}
	else if (Runner.IsRunning() && Runner.Scenario && Runner.Scenario->Steps.IsValidIndex(Runner.GetStepIndex()))
	{
		Result.Completed = Runner.GetStepIndex();
		Result.Label = FText::Format(Labels.Mission, FText::FromString(FString::FromInt(Result.Completed + 1)),
			FText::FromString(FString::FromInt(Result.Total)));
		Result.Mission = MissionText(Runner.Scenario->Steps[Runner.GetStepIndex()], Labels.Unknown);
	}
	return Result;
}

void FVBoRSpectatorNames::Sample(const TArray<FString>& ActiveNames, double NowSeconds, double HoldSeconds)
{
	for (FEntry& Entry : Entries)
	{
		const bool bNowActive = ActiveNames.Contains(Entry.Name);
		if (Entry.bActive && !bNowActive) { Entry.ReleasedAt = NowSeconds; }
		if (!Entry.bActive && bNowActive) { Entry.Order = ++NextOrder; }
		Entry.bActive = bNowActive;
	}
	for (const FString& Name : ActiveNames)
	{
		if (!Name.TrimStartAndEnd().IsEmpty() && !Entries.ContainsByPredicate([&](const FEntry& Entry) { return Entry.Name == Name; }))
		{ Entries.Add({Name, 0, ++NextOrder, true}); }
	}
	Entries.RemoveAll([&](const FEntry& Entry) { return !Entry.bActive && NowSeconds - Entry.ReleasedAt >= FMath::Max(0.0, HoldSeconds); });
	Entries.Sort([](const FEntry& A, const FEntry& B) { return A.Order > B.Order; });
}

FString FVBoRSpectatorNames::Line(const FString& Prefix) const
{
	FString Result;
	for (int32 Index = 0; Index < FMath::Min(2, Entries.Num()); ++Index)
	{ Result += (Index == 0 ? Prefix : TEXT(" · ")) + Entries[Index].Name; }
	return Result;
}

TArray<FString> FVBoRSpectatorNames::RecentNames() const
{
	TArray<FString> Result;
	for (int32 Index = 0; Index < FMath::Min(2, Entries.Num()); ++Index) { Result.Add(Entries[Index].Name); }
	return Result;
}

bool FVBoRSpectatorLayout::Update(FIntPoint WindowSize, float EyeAspect, float MinPanelWidth)
{
	if (WindowSize.X <= 0 || WindowSize.Y <= 0) { return false; }
	EyeAspect = FMath::IsFinite(EyeAspect) ? FMath::Max(0.01f, EyeAspect) : 4.f / 3;
	MinPanelWidth = FMath::IsFinite(MinPanelWidth) ? FMath::Max(0.01f, MinPanelWidth) : 0.37f;
	if (WindowSize == LastSize && EyeAspect == LastAspect && MinPanelWidth == LastPanelWidth) { return false; }
	LastSize = WindowSize; LastAspect = EyeAspect; LastPanelWidth = MinPanelWidth;

	const double Aspect = static_cast<double>(WindowSize.X) / WindowSize.Y;
	// Clamp even very narrow windows (no inverted view/panel regions, P5k D1)
	const double Panel = FMath::Min(static_cast<double>(MinPanelWidth), Aspect * 0.99);
	const double Height = FMath::Min(1.0, (Aspect - Panel) / EyeAspect);
	const double Width = Height * EyeAspect / Aspect;
	// Draw panel first. View-first lets panel bClearBlack clear whole window and erase view
	// (XRBase DefaultSpectatorScreenController.cpp:284-291, XRCopyTexture.cpp:250-257). Nonoverlapping regions, ordering otherwise irrelevant
	Layout = FSpectatorScreenModeTexturePlusEyeLayout(FVector2D(0,(1-Height)/2), FVector2D(Width,(1+Height)/2),
		FVector2D(Width,0), FVector2D(1,1), false, true, false);
	PanelPixels = FIntPoint(FMath::Max(1,FMath::RoundToInt((1-Width)*WindowSize.X)),WindowSize.Y);
	DrawScale = WindowSize.Y / 1080.f;
	return true;
}
