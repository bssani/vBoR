#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "VBoRScenarioAsset.generated.h"

UENUM(BlueprintType)
enum class EVBoRStepRule : uint8 { Confirm, Approach, Seated, ButtonPressed, DoorUnlocked, DoorOpened, DoorLatched, Viewpoint, KeyHeld, KeyUnlockAndLock, PoweredOpen, PoweredClosed };
UENUM(BlueprintType)
enum class EVBoRConfirmBy : uint8
{
	None UMETA(DisplayName="Not needed"),
	Participant UMETA(Hidden),
	Operator UMETA(DisplayName="Operator"),
	Either UMETA(Hidden)
};

USTRUCT(BlueprintType)
struct VBOR_API FVBoRScenarioStep
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ToolTip="Shown in log, operator window, TV. Any value, not empty")) FName StepId;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ToolTip="Participant instruction")) FText Instruction;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ToolTip="TV instruction override. Empty uses participant instruction, then original ID text")) FText SpectatorText;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ToolTip="Current state or confirmation rule to complete step")) EVBoRStepRule Rule = EVBoRStepRule::Confirm;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(DisplayName="Rule target", ToolTip="Actor or component checked by the rule", GetOptions="GetTargetTagOptions", EditCondition="Rule != EVBoRStepRule::Confirm", EditConditionHides)) FName TargetTag;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(DisplayName="Operator confirm", ToolTip="Operator confirms satisfied rule. Observation always needs confirmation")) EVBoRConfirmBy ConfirmBy = EVBoRConfirmBy::Either;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(DisplayName="Panel at", ToolTip="Fixed location for the guidance panel", GetOptions="GetPanelTagOptions")) FName PanelTag;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(DisplayName="Floor route", ToolTip="Floor route shown for this step", GetOptions="GetRouteTagOptions")) FName RouteTag;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(DisplayName="Target marker", ToolTip="Location for the floor ring and arrow", GetOptions="GetMarkerTagOptions")) FName MarkerTag;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, meta=(Units="deg", ClampMin="0", ToolTip="Door open target angle. Placeholder, not acceptance threshold", EditCondition="Rule == EVBoRStepRule::DoorOpened", EditConditionHides)) float DoorOpenDeg = 45;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, meta=(Units="s", ClampMin="0", ToolTip="Tracked head dwell time inside seat. Placeholder", EditCondition="Rule == EVBoRStepRule::Seated", EditConditionHides)) float SeatedSeconds = 1.5;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, meta=(ToolTip="Return to driver reference viewpoint before completion")) bool bRequireBaseViewpoint = false;
};

UCLASS(BlueprintType)
class VBOR_API UVBoRScenarioAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()
public:
	UVBoRScenarioAsset();
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Guidance", meta=(TitleProperty="{StepId} · {Instruction}", ToolTip="Guidance in array order. Edit entries/order/text as needed")) TArray<FVBoRScenarioStep> Steps;
	UFUNCTION() TArray<FString> GetPanelTagOptions() const;
	UFUNCTION() TArray<FString> GetRouteTagOptions() const;
	UFUNCTION() TArray<FString> GetMarkerTagOptions() const;
	UFUNCTION() TArray<FString> GetTargetTagOptions() const;

private:
	friend struct FP5zTestAccess;
	TArray<FString> TagOptions(FName FVBoRScenarioStep::* Field, UWorld* World, const UClass* ActorClass = nullptr) const;
};
