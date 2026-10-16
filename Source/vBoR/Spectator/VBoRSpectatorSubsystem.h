#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "VBoRSpectatorModel.h"
#include "VBoRSpectatorSubsystem.generated.h"

class AVBoRScenarioRunner;
class FWidgetRenderer;
class SWidget;
class UTextureRenderTarget2D;

UCLASS(Config=Game)
class VBOR_API UVBoRSpectatorSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()
public:
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category="Spectator", meta=(ClampMin="1")) float RedrawsPerSecond = 15;
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category="Spectator", meta=(ClampMin="0.01")) float EyeAspect = 4.f / 3;
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category="Spectator", meta=(ClampMin="0.01")) float MinPanelWidth = 0.37f;
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category="Spectator", meta=(Units="s", ClampMin="0")) float NameHoldSeconds = 1;
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category="Spectator|Text") FText ReadyLabel = FText::FromString(TEXT("GETTING READY"));
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category="Spectator|Text") FText MissionLabel = FText::FromString(TEXT("MISSION {0} / {1}"));
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category="Spectator|Text") FText FreeLabel = FText::FromString(TEXT("FREE MODE"));
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category="Spectator|Text") FText CompletedMission = FText::FromString(TEXT("All missions complete. Explore the vehicle freely."));
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category="Spectator|Text") FText ExploreMission = FText::FromString(TEXT("Explore the vehicle freely."));
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category="Spectator|Text") FText UnknownMission = FText::FromString(TEXT("Mission in progress"));
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category="Spectator|Text") FText InteractingPrefix = FText::FromString(TEXT("INTERACTING"));
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category="Spectator|Style") FLinearColor BackgroundColor = FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("131a26")));
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category="Spectator|Style") FLinearColor TextColor = FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("eef2f7")));
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category="Spectator|Style") FLinearColor AccentColor = FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("5b8cff")));
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category="Spectator|Style") FLinearColor SecondaryTextColor = FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("9aa6b8")));
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category="Spectator|Style") FLinearColor ProgressBackgroundColor = FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("1c2534")));
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category="Spectator|Style") FLinearColor LineColor = FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("2e3a4e")));
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category="Spectator|Style", meta=(ClampMin="1")) int32 LabelFontSize = 40;
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category="Spectator|Style", meta=(ClampMin="1")) int32 MissionFontSize = 36;
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category="Spectator|Style", meta=(ClampMin="1")) int32 PartFontSize = 27;
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category="Spectator|Style", meta=(ClampMin="1")) int32 HeadingFontSize = 16;
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category="Spectator|Style", meta=(ClampMin="1")) int32 ProgressFontSize = 19;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override { return bPending || WidgetRenderer != nullptr; }
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UVBoRSpectatorSubsystem, STATGROUP_Tickables); }
	virtual void Deinitialize() override;
	bool IsShowing() const { return RenderTarget != nullptr; }
	void SampleParts(double NowSeconds);
protected:
	virtual bool DoesSupportWorldType(EWorldType::Type WorldType) const override;
private:
	friend struct FP5hTestAccess;
	UPROPERTY() TObjectPtr<UTextureRenderTarget2D> RenderTarget;
	UPROPERTY() TWeakObjectPtr<AVBoRScenarioRunner> Runner;
	FWidgetRenderer* WidgetRenderer = nullptr;
	TSharedPtr<SWidget> SlateWidget;
	FVBoRSpectatorBanner CurrentBanner;
	FVBoRSpectatorNames Names;
	FVBoRSpectatorLayout ScreenLayout;
	bool bPending = false;
	float PendingSeconds = 0, RetryCountdown = 0, RedrawCountdown = 0;
	bool TryStart();
	void Stop();
	TSharedRef<SWidget> MakeWidget();
};
