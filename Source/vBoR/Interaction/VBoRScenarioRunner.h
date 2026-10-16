#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VBoRScenarioAsset.h"
#include "VBoROperatorPanel.h"
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
class SBorder;

enum class EVBoRFlowCommand : uint8 { Confirm, Next, Previous, Viewpoint, Passenger, Rear, ReturnBase, Restart, IgnoreAlignment, EnterFreeMode, ReloadLevel, ReapplyAlignment };
enum class EVBoRCommandSource : uint8 { Participant, Operator };
struct FVBoRFlowToken { int32 StepIndex = INDEX_NONE; uint64 Generation = 0; };
struct FVBoRFlowCommand
{
	EVBoRFlowCommand Action = EVBoRFlowCommand::Confirm;
	EVBoRCommandSource Source = EVBoRCommandSource::Participant;
	FVBoRFlowToken Token;
	bool bConfirmLocked = false;
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
	UPROPERTY(EditAnywhere, Category="안내", meta=(Units="cm")) float PanelDistanceCm = 70;
	UPROPERTY(EditAnywhere, Category="안내", meta=(Units="cm")) float PanelBelowCm = 0;
	UPROPERTY(EditAnywhere, Category="안내", meta=(Units="cm")) FVector2D PanelSizeCm = FVector2D(50, 32);
	UPROPERTY(EditAnywhere, Category="안내", meta=(Units="cm")) float ButtonDistanceCm;
	UPROPERTY(EditAnywhere, Category="안내", meta=(Units="cm")) float ButtonBelowCm;
	UPROPERTY(EditAnywhere, Category="안내", meta=(Units="s", ClampMin="0")) float ConfirmLockSeconds;
	UPROPERTY(EditAnywhere, Category="안내", meta=(Units="s", ClampMin="0.01")) float SeatedPanelSeconds;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="안내") FText FreeModeInstruction;
	bool InitializeFlow();
	void QueueCommand(const FVBoRFlowCommand& Command);
	void ProcessFlow(double DeltaSeconds);
	FVBoRFlowToken GetToken() const { return {StepIndex, Generation}; }
	int32 GetStepIndex() const { return StepIndex; }
	bool IsFinished() const { return bFinished; }
	UFUNCTION(BlueprintPure, Category="안내") bool IsFreeMode() const { return bFreeMode; }
	bool IsRunning() const { return bRunning; }
	FString GetPanelText() const;
	FString GetOperatorText() const;
	FText GetInstructionText() const;
	FString LastRecord, LastCompletion, LastViewRequest, ValidationProblems;
	int32 StaleDiscards = 0, PermissionDiscards = 0, RuleDiscards = 0, TransitionDiscards = 0;
	int32 ConfirmLockDiscards = 0;
	int32 PreviousCount = 0, RetryCount = 0, SkipCount = 0;
	int32 EndCount = 0;
	bool bAlignmentIgnored = false;
	int32 ReapplyCount = 0;
	FVBoRCommandDecision EvaluateCommand(const FVBoRFlowCommand& Command) const;
	FVBoRCommandDecision EvaluateOperatorButton(EVBoROperatorButton Button) const;
	void PressOperatorButton(EVBoROperatorButton Button);
	bool HandleOperatorKey(FKey Key, bool bRepeat = false);
	FVBoROperatorStatus GetOperatorStatus() const;
	EVBoROperatorStyle GetOperatorStyle(EVBoROperatorButton Button) const;
	FString OperatorButtonBadge(EVBoROperatorButton Button) const;
	FString ReapplyHelp() const;
	UFUNCTION(BlueprintCallable, Category="안내") void ShowOperatorWindow();
	bool HasOperatorWindow() const { return OperatorWindow.IsValid(); }
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void RequestLevelReload(FName LevelName);
private:
	friend struct FP3FlowTestAccess;
	friend struct FP5bTestAccess;
	friend struct FP5gTestAccess;
	friend struct FP5iTestAccess;
	UPROPERTY() TObjectPtr<UVBoRVehicleAlignmentComponent> Alignment;
	UPROPERTY() TObjectPtr<UVBoRApproachZoneComponent> Approach;
	UPROPERTY() TObjectPtr<UVBoRSeatZoneComponent> Seat;
	UPROPERTY() TObjectPtr<AVBoRPokeButton> ConfirmButton;
	UPROPERTY() TObjectPtr<AVBoRPokeButton> ActionButton;
	UPROPERTY() TObjectPtr<UWidgetComponent> OperatorPanel;
	UPROPERTY() TArray<TObjectPtr<UWidgetComponent>> ButtonLabels;
	UPROPERTY() TObjectPtr<UArrowComponent> MarkerArrow;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> MarkerRing;
	UPROPERTY() TArray<TObjectPtr<UObject>> Targets;
	UPROPERTY() TArray<TObjectPtr<USceneComponent>> Markers;
	TMap<TWeakObjectPtr<AVBoRPokeButton>, int32> ButtonCounts;
	TSharedPtr<STextBlock> PanelText;
	TSharedPtr<STextBlock> OperatorText;
	TSharedPtr<SBorder> PanelBorder;
	TArray<TSharedPtr<STextBlock>> ButtonLabelTexts;
	TArray<FVBoRFlowCommand> PendingCommands;
	FVBoRFlowToken LiveToken;
	uint64 LiveFrame = MAX_uint64, Generation = 0;
	int32 StepIndex = INDEX_NONE, ButtonBaseline = 0;
	bool bInitialized = false, bRunning = false, bFinished = false, bRestartPending = false;
	bool bFreeMode = false, bReloadPending = false, bReloadRequested = false;
	double ReloadRemaining = 0;
	double StepElapsed = 0;
	double ConfirmLockRemaining = 0, HeadLogElapsed = 0;
	bool bOperatorInfo = false, bPanelPlacementPending = true, bSeatedPanelPlaced = false;
	bool bStartHeadPending = false, bSeatHeadRecorded = false;
	bool bPawnMovedBeforeFlow = false;
	bool bReapplyPending = false;
	FString LastOperatorCommand;
	int32 OperatorCompletedSteps = 0;
	TSharedPtr<SWindow> OperatorWindow;
	void CloseOperatorWindow();
	void FinishReapply();
	UVBoRViewpointComponent* CommandViewpoint(EVBoRFlowCommand Action) const;
	void RecordDecision(const FVBoRFlowCommand& Command, const FVBoRCommandDecision& Decision);
	EVBoRFlowCommand PanelAction = EVBoRFlowCommand::ReturnBase;
	EVBoRFlowCommand FreeSeatActions[2] = {EVBoRFlowCommand::Passenger, EVBoRFlowCommand::Rear};
	const FVBoRScenarioStep* CurrentStep() const;
	bool RuleSatisfied() const;
	bool NeedsConfirmation() const;
	bool CanParticipantConfirm() const;
	void EnterStep(int32 Index);
	void EnterFreeMode(bool bCompleted);
	void CompleteStep(const TCHAR* Who);
	void Dispatch(const FVBoRFlowCommand& Command);
	void QueueLive(EVBoRFlowCommand Action, EVBoRCommandSource Source);
	void FreezeLiveToken();
	void PlacePanel();
	void HandlePawnMoved();
	void SampleHead(bool bTracked, const FTransform& Head, double DeltaSeconds);
	void PlacePanelAtHead(const FTransform& Head);
	bool RecordHead(const TCHAR* Label, bool bTracked, const FTransform& Head);
	FString ActionLabel() const;
	void RefreshPanel();
	void UpdateMarker();
	void Record(const FString& Text);
	UFUNCTION() void HandleConfirmPoke(const FIsdkInteractionPointerEvent& Event);
	UFUNCTION() void HandleActionPoke(const FIsdkInteractionPointerEvent& Event);
	UFUNCTION() void HandleTargetPoke(const FIsdkInteractionPointerEvent& Event);
	void KeyConfirm(); void KeyNext(); void KeyPrevious(); void KeyPassenger(); void KeyRear(); void KeyReturn(); void KeyRestart(); void KeyIgnore();
	void KeyOperatorInfo();
	void KeyFreeMode(); void KeyReloadLevel();
	void KeyReapply();
};
