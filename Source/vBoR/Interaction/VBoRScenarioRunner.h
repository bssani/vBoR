#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VBoRScenarioAsset.h"
#include "Interaction/Pointable/IsdkInteractionPointerEvent.h"
#include "VBoRScenarioRunner.generated.h"

class UVBoRVehicleAlignmentComponent;
class UVBoRApproachZoneComponent;
class UVBoRSeatZoneComponent;
class UVBoRViewpointComponent;
class AVBoRPokeButton;
class UWidgetComponent;
class UArrowComponent;
class UStaticMeshComponent;
class STextBlock;

enum class EVBoRFlowCommand : uint8 { Confirm, Next, Previous, Viewpoint, Passenger, Rear, ReturnBase, Restart, IgnoreAlignment };
enum class EVBoRCommandSource : uint8 { Participant, Operator };
struct FVBoRFlowToken { int32 StepIndex = INDEX_NONE; uint64 Generation = 0; };
struct FVBoRFlowCommand
{
	EVBoRFlowCommand Action = EVBoRFlowCommand::Confirm;
	EVBoRCommandSource Source = EVBoRCommandSource::Participant;
	FVBoRFlowToken Token;
};

UCLASS(Blueprintable)
class VBOR_API AVBoRScenarioRunner : public AActor
{
	GENERATED_BODY()
public:
	AVBoRScenarioRunner();
	virtual void Tick(float DeltaSeconds) override;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="안내") TObjectPtr<UVBoRScenarioAsset> Scenario;
	UPROPERTY(VisibleAnywhere, Category="안내") TObjectPtr<UWidgetComponent> Panel;
	UPROPERTY(EditAnywhere, Category="안내", meta=(Units="cm")) float PanelDistanceCm = 60;
	UPROPERTY(EditAnywhere, Category="안내", meta=(Units="cm")) float PanelBelowCm = 15;
	UPROPERTY(EditAnywhere, Category="안내", meta=(Units="cm")) FVector2D PanelSizeCm = FVector2D(50, 32);
	bool InitializeFlow();
	void QueueCommand(const FVBoRFlowCommand& Command);
	void ProcessFlow(double DeltaSeconds);
	FVBoRFlowToken GetToken() const { return {StepIndex, Generation}; }
	int32 GetStepIndex() const { return StepIndex; }
	bool IsFinished() const { return bFinished; }
	bool IsRunning() const { return bRunning; }
	FString GetPanelText() const;
	FText GetInstructionText() const;
	FString LastRecord, LastCompletion, LastViewRequest, ValidationProblems;
	int32 StaleDiscards = 0, PermissionDiscards = 0, RuleDiscards = 0, TransitionDiscards = 0;
	int32 PreviousCount = 0, RetryCount = 0, SkipCount = 0;
	bool bAlignmentIgnored = false;
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
private:
	friend struct FP3FlowTestAccess;
	UPROPERTY() TObjectPtr<UVBoRVehicleAlignmentComponent> Alignment;
	UPROPERTY() TObjectPtr<UVBoRApproachZoneComponent> Approach;
	UPROPERTY() TObjectPtr<UVBoRSeatZoneComponent> Seat;
	UPROPERTY() TObjectPtr<AVBoRPokeButton> ConfirmButton;
	UPROPERTY() TObjectPtr<AVBoRPokeButton> ActionButton;
	UPROPERTY() TObjectPtr<UArrowComponent> MarkerArrow;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> MarkerRing;
	UPROPERTY() TArray<TObjectPtr<UObject>> Targets;
	UPROPERTY() TArray<TObjectPtr<USceneComponent>> Markers;
	TMap<TWeakObjectPtr<AVBoRPokeButton>, int32> ButtonCounts;
	TSharedPtr<STextBlock> PanelText;
	TArray<FVBoRFlowCommand> PendingCommands;
	FVBoRFlowToken LiveToken;
	uint64 LiveFrame = MAX_uint64, Generation = 0;
	int32 StepIndex = INDEX_NONE, ButtonBaseline = 0;
	bool bInitialized = false, bRunning = false, bFinished = false, bRestartPending = false;
	double StepElapsed = 0;
	EVBoRFlowCommand PanelAction = EVBoRFlowCommand::ReturnBase;
	const FVBoRScenarioStep* CurrentStep() const;
	bool RuleSatisfied() const;
	bool NeedsConfirmation() const;
	bool CanParticipantConfirm() const;
	void EnterStep(int32 Index);
	void CompleteStep(const TCHAR* Who);
	void Dispatch(const FVBoRFlowCommand& Command);
	void QueueLive(EVBoRFlowCommand Action, EVBoRCommandSource Source);
	void FreezeLiveToken();
	void PlacePanel();
	void RefreshPanel();
	void UpdateMarker();
	void Record(const FString& Text);
	UFUNCTION() void HandleConfirmPoke(const FIsdkInteractionPointerEvent& Event);
	UFUNCTION() void HandleActionPoke(const FIsdkInteractionPointerEvent& Event);
	UFUNCTION() void HandleTargetPoke(const FIsdkInteractionPointerEvent& Event);
	void KeyConfirm(); void KeyNext(); void KeyPrevious(); void KeyPassenger(); void KeyRear(); void KeyReturn(); void KeyRestart(); void KeyIgnore();
};
