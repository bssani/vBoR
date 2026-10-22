#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VBoRFoldingSeat.generated.h"

class UStaticMeshComponent;

UENUM(BlueprintType)
enum class EVBoRFoldingSeatState : uint8 { Standing, Folding, Folded, Unfolding };

UCLASS(Blueprintable)
class VBOR_API AVBoRFoldingSeat : public AActor
{
	GENERATED_BODY()
public:
	AVBoRFoldingSeat();
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void Tick(float DeltaSeconds) override;
	UFUNCTION(BlueprintCallable, Category="Seat folding") bool Fold();
	UFUNCTION(BlueprintCallable, Category="Seat folding") bool Unfold();
	UFUNCTION(BlueprintCallable, Category="Seat folding") void ResetSeat();
	UFUNCTION(BlueprintPure, Category="Seat folding") EVBoRFoldingSeatState GetState() const { return State; }
	UFUNCTION(BlueprintPure, Category="Seat folding") float GetAngleDeg() const { return AngleDeg; }
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Seat folding") TObjectPtr<USceneComponent> HingePivot;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Seat folding") TObjectPtr<UStaticMeshComponent> SeatMesh;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Seat folding", meta=(Units="cm")) FVector HingeLocationCm = FVector(0,0,48);
	// Preserve upright CAD position with car-origin pivot (P4e D1, U-09)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Seat folding") FTransform MeshRestFromRoot = FTransform(FQuat::Identity, FVector(0,0,78), FVector(0.08,1.7,0.6));
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Seat folding", meta=(Units="deg", ClampMin="1", ClampMax="170")) float FoldAngleDeg = 80;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Seat folding", meta=(Units="deg/s", ClampMin="0.1")) float SpeedDegPerSecond = 45;
private:
	EVBoRFoldingSeatState State = EVBoRFoldingSeatState::Standing;
	float AngleDeg = 0;
	void ApplyAngle();
};
