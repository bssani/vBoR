#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "VBoRScenarioAsset.generated.h"

UENUM(BlueprintType)
enum class EVBoRStepRule : uint8 { Confirm, Approach, Seated, ButtonPressed, DoorUnlocked, DoorOpened, DoorLatched, Viewpoint, KeyHeld, KeyUnlockAndLock, PoweredOpen, PoweredClosed };
UENUM(BlueprintType)
enum class EVBoRConfirmBy : uint8 { None, Participant, Operator, Either };

USTRUCT(BlueprintType)
struct VBOR_API FVBoRScenarioStep
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ToolTip="Original scenario ID. No 17")) FName StepId;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ToolTip="Participant instruction")) FText Instruction;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ToolTip="English TV spectator text. Empty uses step ID defaults")) FText SpectatorText;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ToolTip="Current state or confirmation rule to complete step")) EVBoRStepRule Rule = EVBoRStepRule::Confirm;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ToolTip="Rule target Actor or Component tag. Exactly one match required")) FName TargetTag;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(Units="deg", ClampMin="0", ToolTip="Door open target angle. Placeholder, not acceptance threshold")) float DoorOpenDeg = 45;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(Units="s", ClampMin="0", ToolTip="Tracked head dwell time inside seat. Placeholder")) float SeatedSeconds = 1.5;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ToolTip="Who confirms after rule is satisfied. Observation always needs confirmation")) EVBoRConfirmBy ConfirmBy = EVBoRConfirmBy::Either;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ToolTip="Return to driver reference viewpoint before completion")) bool bRequireBaseViewpoint = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ToolTip="Target tag for floor ring and arrow. Optional")) FName MarkerTag;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ToolTip="Current step floor route Actor tag. Optional (P3b D2)")) FName RouteTag;
};

UCLASS(BlueprintType)
class VBOR_API UVBoRScenarioAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()
public:
	UVBoRScenarioAsset();
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Guidance", meta=(ToolTip="Guidance in array order. Edit entries/order/text as needed")) TArray<FVBoRScenarioStep> Steps;
};
