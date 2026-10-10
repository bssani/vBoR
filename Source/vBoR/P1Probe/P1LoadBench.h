#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "InputCoreTypes.h"
#include "../Interaction/VBoRCarryObject.h"
#include "P1LoadBench.generated.h"

class AP1TrunkMock;
class AVBoRCarryObject;
class AVBoRLoadTarget;
class AVBoRHandRigPawn;
class UStaticMeshComponent;
class UTextRenderComponent;

DECLARE_LOG_CATEGORY_EXTERN(LogP1Probe, Log, All);

struct FP1TrialTotals
{
	int32 Grabbed = 0;
	int32 Assisted = 0;
	int32 DroppedInside = 0;
	int32 DroppedOutside = 0;
	int32 BlockedCount = 0;
	int32 PauseCount = 0;
	double HeldSeconds = 0;
	double BlockedSeconds = 0;
	double PausedSeconds = 0;
	void Add(const FVBoRCarryMetrics& Metrics);
};

UCLASS()
class VBOR_API AP1LoadBench : public AActor
{
	GENERATED_BODY()
public:
	AP1LoadBench();
	virtual void Tick(float DeltaSeconds) override;
	void PlaceFromHead(const FTransform& Head);
	void ResetAndReposition();
	UFUNCTION(BlueprintCallable, Category = "Bench") void ResetBagForNextTrial();
	int32 GetTrialNumber() const { return TrialNumber; }
	FP1TrialTotals GetTotals() const;

	UPROPERTY(EditAnywhere, Category = "Bench", meta = (Units = "cm")) float OpeningForwardCm = 130.0f;
	UPROPERTY(EditAnywhere, Category = "Bench", meta = (Units = "cm")) float StandBehindOpeningCm = 100.0f;
	UPROPERTY(EditAnywhere, Category = "Bench", meta = (Units = "cm")) float StandTopHeightCm = 75.0f;
	UPROPERTY(EditAnywhere, Category = "Bench", meta = (Units = "cm")) FVector StandSizeCm = FVector(60, 130, 5);
	UPROPERTY(EditAnywhere, Category = "Bench", meta = (Units = "cm")) float BagLengthCm = 120.0f;
	UPROPERTY(EditAnywhere, Category = "Bench", meta = (Units = "cm")) float BagDiameterCm = 28.0f;
	UPROPERTY(EditAnywhere, Category = "Bench", meta = (Units = "cm")) FVector GoalOffsetCm = FVector(45, 0, 14);
	UPROPERTY(EditAnywhere, Category = "Bench") FKey RepositionKey = EKeys::R;
	UPROPERTY(EditAnywhere, Category = "Bench") FKey BagResetKey = EKeys::B;
	UPROPERTY(EditAnywhere, Category = "Bench") TSubclassOf<AP1TrunkMock> TrunkClass;
	UPROPERTY(EditAnywhere, Category = "Bench") TSubclassOf<AVBoRCarryObject> BagClass;
	UPROPERTY(EditAnywhere, Category = "Bench") TSubclassOf<AVBoRLoadTarget> TargetClass;

	UPROPERTY(VisibleAnywhere, Category = "Bench") TObjectPtr<UStaticMeshComponent> Stand;
	UPROPERTY(VisibleAnywhere, Category = "Bench") TObjectPtr<UTextRenderComponent> Panel;
	UPROPERTY(VisibleAnywhere, Category = "Bench") TObjectPtr<AP1TrunkMock> Trunk;
	UPROPERTY(VisibleAnywhere, Category = "Bench") TObjectPtr<AVBoRCarryObject> Bag;
	UPROPERTY(VisibleAnywhere, Category = "Bench") TObjectPtr<AVBoRLoadTarget> Target;
	bool HasPlaced() const { return bPlaced; }
	int32 GetEntranceCrossings() const { return EntranceCrossings; }
	int32 GetBlockedAtCrossings() const { return BlockedAtCrossings; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
private:
	UPROPERTY() TObjectPtr<AVBoRHandRigPawn> HandPawn;
	bool bPlaced = false;
	bool bPreviousBodyBeforeOpening = false;
	bool bCrossingRecorded = false;
	bool bBlockedAtCrossingRecorded = false;
	int32 EntranceCrossings = 0;
	int32 BlockedAtCrossings = 0;
	int32 TrialNumber = 1;
	FP1TrialTotals CompletedTrials;
	double LastLogAt = -1.0;
	FString LastEvent = TEXT("Waiting for tracked head");
	FString LastObservedState;
	TArray<FString> RecentEvents;
	bool TryPlaceFromHead();
	void SpawnParts();
	FTransform BagStartPose() const;
	FTransform GoalPose() const;
	void UpdateCrossing();
	void ResetCrossing();
	void UpdatePanel();
	void RecordEvent(const FString& Event);
	void RecordSnapshot(const FString& Reason);
	UFUNCTION() void HandleGrabbed(AVBoRCarryObject* Object);
	UFUNCTION() void HandleCarryReleased(AVBoRCarryObject* Object);
	UFUNCTION() void HandleSeated(AVBoRCarryObject* Object);
};
