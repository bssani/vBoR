#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "../Interaction/VBoRDoorMotion.h"
#include "P2DoorBench.generated.h"

class AVBoRHingedDoor;
class AVBoRHandRigPawn;
class UTextRenderComponent;
class UStaticMeshComponent;

DECLARE_LOG_CATEGORY_EXTERN(LogP2Door, Log, All);

UCLASS()
class VBOR_API AP2DoorBench : public AActor
{
	GENERATED_BODY()
public:
	AP2DoorBench();
	virtual void Tick(float DeltaSeconds) override;
	void PlaceFromHead(const FTransform& Head, double FloorZ);
	void Reposition();
	void NextTrial();
	int32 GetTrialNumber() const { return TrialNumber; }
	UPROPERTY(EditAnywhere, Category = "Bench", meta = (Units = "cm")) FVector DoorSizeCm = FVector(110, 10, 105);
	UPROPERTY(EditAnywhere, Category = "Bench", meta = (Units = "cm")) float BottomHeightCm = 35;
	UPROPERTY(EditAnywhere, Category = "Bench", meta = (Units = "cm")) float OutsideHandleInsetCm = 10;
	UPROPERTY(EditAnywhere, Category = "Bench", meta = (Units = "cm")) float OutsideHandleHeightCm = 95;
	UPROPERTY(EditAnywhere, Category = "Bench", meta = (Units = "cm")) float InsideHandleInsetCm = 25;
	UPROPERTY(EditAnywhere, Category = "Bench", meta = (Units = "cm")) float InsideHandleHeightCm = 85;
	UPROPERTY(EditAnywhere, Category = "Bench", meta = (Units = "cm")) float HeadDistanceCm = 60;
	UPROPERTY(EditAnywhere, Category = "Bench", meta = (Units = "cm")) float HeadFromTrailingEdgeCm = 30;
	UPROPERTY(EditAnywhere, Category = "Bench") TSubclassOf<AVBoRHingedDoor> DoorClass;
	UPROPERTY(VisibleAnywhere, Category = "Bench") TObjectPtr<AVBoRHingedDoor> Door;
	UPROPERTY(VisibleAnywhere, Category = "Bench") TObjectPtr<UTextRenderComponent> Panel;
	UPROPERTY(VisibleAnywhere, Category = "Bench") TObjectPtr<UStaticMeshComponent> FrameParts[4];
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
	bool bPlaced = false;
	int32 TrialNumber = 1;
	double LastLogAt = -1;
	FString LastState;
	FVBoRDoorMetrics TrialBaseline;
	bool TryPlace();
	void Snapshot(const FString& Reason, bool bTrial);
	void UpdatePanel();
};
