#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "InputCoreTypes.h"
#include "P0ButtonBenchMeasurement.h"
#include "Interaction/IsdkIInteractableState.h"
#include "Interaction/Pointable/IsdkInteractionPointerEvent.h"
#include "P0ButtonBench.generated.h"

class AVBoRPokeButton;
class AVBoRHandRigPawn;
class UTextRenderComponent;

UCLASS()
class VBOR_API AP0ButtonBench : public AActor
{
	GENERATED_BODY()
public:
	AP0ButtonBench();
	virtual void Tick(float DeltaSeconds) override;
	void PlaceFromHead(const FTransform& Head);
	UPROPERTY(EditAnywhere, Category = "Bench", meta = (Units = "cm"))
	float ForwardDistanceCm = 45.0f;
	UPROPERTY(EditAnywhere, Category = "Bench", meta = (Units = "cm"))
	float BelowEyeCm = 25.0f;
	UPROPERTY(EditAnywhere, Category = "Bench")
	FKey RepositionKey = EKeys::R;
	UPROPERTY(EditAnywhere, Category = "Bench")
	TArray<float> SingleSizesMm = {10.0f, 15.0f, 20.0f};
	UPROPERTY(EditAnywhere, Category = "Bench", meta = (Units = "cm"))
	float SingleSpacingCm = 10.0f;
	UPROPERTY(EditAnywhere, Category = "Bench", meta = (ClampMin = "1", ClampMax = "12"))
	int32 ClusterRows = 3;
	UPROPERTY(EditAnywhere, Category = "Bench", meta = (ClampMin = "1", ClampMax = "12"))
	int32 ClusterColumns = 4;
	UPROPERTY(EditAnywhere, Category = "Bench", meta = (Units = "mm"))
	float ClusterSizeMm = 12.0f;
	UPROPERTY(EditAnywhere, Category = "Bench", meta = (Units = "mm"))
	float ClusterSpacingMm = 15.0f;
	UPROPERTY(EditAnywhere, Category = "Bench", meta = (Units = "cm"))
	FVector ClusterOffsetCm = FVector(0, 0, -12);
	UPROPERTY(EditAnywhere, Category = "Bench", meta = (Units = "cm"))
	FVector FarOffsetCm = FVector(155, 0, 0);
	UPROPERTY(EditAnywhere, Category = "Bench", meta = (Units = "mm"))
	float FarSizeMm = 20.0f;
	UPROPERTY(EditAnywhere, Category = "Bench", meta = (Units = "mm"))
	float PressDepthMm = 3.0f;
	UPROPERTY(EditAnywhere, Category = "Bench", meta = (ClampMin = "0.01"))
	float HitAreaScale = 1.0f;
	UPROPERTY(EditAnywhere, Category = "Bench")
	TSubclassOf<AVBoRPokeButton> ButtonClass;
	UPROPERTY(EditAnywhere, Category = "Bench", meta = (Units = "cm"))
	FVector PanelOffsetCm = FVector(-5, -50, 25);
	UPROPERTY(VisibleAnywhere, Category = "Bench")
	TObjectPtr<UTextRenderComponent> Panel;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
private:
	UPROPERTY()
	TArray<TObjectPtr<AVBoRPokeButton>> Buttons;
	UPROPERTY()
	TObjectPtr<AVBoRHandRigPawn> HandPawn;
	TArray<FString> Names;
	TArray<FP0ButtonPressMeasurement> Presses[2];
	FP0ButtonTargetMeasurement Targets[2];
	FP0ButtonActiveMeasurement Active;
	int32 ClusterStart = 0;
	int32 FarIndex = 0;
	int32 FarHoverCount = 0;
	int32 FarSelectCount = 0;
	FString RigStatus;
	FString LastPress = TEXT("No completed press");
	TArray<FString> RecentEvents;
	double LastLogAt = -1.0;
	bool bPlaced = false;
	void CreateButtons();
	void Reposition();
	bool TryPlaceFromHead();
	void SpawnButton(const FString& Name, const FVector& Offset, float SizeMm);
	void RecordEvent(const FString& Text);
	void SamplePress(int32 Hand, int32 Button, bool bAtSelect = false);
	UFUNCTION()
	void HandlePointer(const FIsdkInteractionPointerEvent& Event);
	UFUNCTION()
	void HandleState(const FIsdkInteractableStateEvent& Event);
	UFUNCTION()
	void HandleFrameFinished();
};
