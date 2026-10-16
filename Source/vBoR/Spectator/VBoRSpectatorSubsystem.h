#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "VBoRSpectatorModel.h"
#include "VBoRSpectatorSubsystem.generated.h"

class AVBoRScenarioRunner;
class FWidgetRenderer;
class SWidget;
class UTextureRenderTarget2D;

UCLASS()
class VBOR_API UVBoRSpectatorSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spectator", meta=(ClampMin="1")) float RedrawsPerSecond = 15;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spectator", meta=(ClampMin="0.01")) float EyeAspect = 4.f / 3;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spectator", meta=(ClampMin="0.01", ClampMax="0.99")) float BandHeight = 0.22f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spectator", meta=(Units="s", ClampMin="0")) float NameHoldSeconds = 1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spectator|Text") FText ReadyLabel = FText::FromString(TEXT("GETTING READY"));
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spectator|Text") FText MissionLabel = FText::FromString(TEXT("MISSION {0} / {1}"));
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spectator|Text") FText FreeLabel = FText::FromString(TEXT("FREE MODE"));
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spectator|Text") FText CompletedMission = FText::FromString(TEXT("All missions complete. Explore the vehicle freely."));
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spectator|Text") FText ExploreMission = FText::FromString(TEXT("Explore the vehicle freely."));
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spectator|Text") FText UnknownMission = FText::FromString(TEXT("Mission in progress"));
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spectator|Text") FText InteractingPrefix = FText::FromString(TEXT("Interacting: "));
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spectator|Style") FLinearColor BackgroundColor = FLinearColor(0.015,0.025,0.045,1);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spectator|Style") FLinearColor TextColor = FLinearColor::White;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spectator|Style") FLinearColor AccentColor = FLinearColor(0.15,0.8,0.95,1);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spectator|Style", meta=(ClampMin="1")) int32 LabelFontSize = 40;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spectator|Style", meta=(ClampMin="1")) int32 MissionFontSize = 50;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spectator|Style", meta=(ClampMin="1")) int32 PartFontSize = 32;
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
	FString PartLine;
	bool bPending = false;
	float PendingSeconds = 0, RetryCountdown = 0, RedrawCountdown = 0;
	bool TryStart();
	void Stop();
	TSharedRef<SWidget> MakeWidget();
};
