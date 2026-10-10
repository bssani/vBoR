#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "InputCoreTypes.h"
#include "P1LoadBench.generated.h"

class AP1TrunkMock;
class AVBoRCarryObject;
class AVBoRLoadTarget;
class AVBoRHandRigPawn;
class UStaticMeshComponent;
class UTextRenderComponent;

DECLARE_LOG_CATEGORY_EXTERN(LogP1Probe, Log, All);

UCLASS()
class VBOR_API AP1LoadBench : public AActor
{
	GENERATED_BODY()
public:
	AP1LoadBench();
	virtual void Tick(float DeltaSeconds) override;
	void PlaceFromHead(const FTransform& Head);
	void ResetAndReposition();

	UPROPERTY(EditAnywhere, Category = "Bench", meta = (Units = "cm")) float OpeningForwardCm = 130.0f;
	UPROPERTY(EditAnywhere, Category = "Bench", meta = (Units = "cm")) float AssumedEyeHeightCm = 165.0f;
	UPROPERTY(EditAnywhere, Category = "Bench", meta = (Units = "cm")) float StandBehindOpeningCm = 100.0f;
	UPROPERTY(EditAnywhere, Category = "Bench", meta = (Units = "cm")) float StandTopHeightCm = 75.0f;
	UPROPERTY(EditAnywhere, Category = "Bench", meta = (Units = "cm")) FVector StandSizeCm = FVector(60, 130, 5);
	UPROPERTY(EditAnywhere, Category = "Bench", meta = (Units = "cm")) float BagLengthCm = 120.0f;
	UPROPERTY(EditAnywhere, Category = "Bench", meta = (Units = "cm")) float BagDiameterCm = 28.0f;
	UPROPERTY(EditAnywhere, Category = "Bench", meta = (Units = "cm")) FVector GoalOffsetCm = FVector(45, 0, 14);
	UPROPERTY(EditAnywhere, Category = "Bench") FKey RepositionKey = EKeys::R;
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
	double LastLogAt = -1.0;
	FString LastEvent = TEXT("Waiting for tracked head");
	FString LastObservedState;
	TArray<FString> RecentEvents;
	bool TryPlaceFromHead();
	void SpawnParts();
	FTransform BagStartPose() const;
	FTransform GoalPose() const;
	void UpdateCrossing();
	void UpdatePanel();
	void RecordEvent(const FString& Event);
	void RecordSnapshot(const FString& Reason);
	UFUNCTION() void HandleGrabbed(AVBoRCarryObject* Object);
	UFUNCTION() void HandleCarryReleased(AVBoRCarryObject* Object);
	UFUNCTION() void HandleSeated(AVBoRCarryObject* Object);
};
