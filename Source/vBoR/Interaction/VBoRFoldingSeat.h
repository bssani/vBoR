#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VBoRMotionGuideComponent.h"
#include "VBoRFoldingSeat.generated.h"

class UStaticMeshComponent;
class AVBoRFoldingSeat;

UENUM(BlueprintType)
enum class EVBoRFoldingSeatState : uint8 { Standing, Folding, Folded, Unfolding };

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FVBoRFoldingSeatStateEvent, AVBoRFoldingSeat*, Seat, EVBoRFoldingSeatState, State);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FVBoRFoldingSeatEvent, AVBoRFoldingSeat*, Seat);

UCLASS(Blueprintable)
class VBOR_API AVBoRFoldingSeat : public AActor
{
	GENERATED_BODY()
public:
	AVBoRFoldingSeat();
	FTransform HingePoseAt(double Angle) const;
	UPROPERTY(EditAnywhere, Transient, Category="Seat folding|Guide", meta=(DisplayName="Preview (0–1)", ClampMin="0", ClampMax="1", UIMin="0", UIMax="1", ToolTip="Editor pose only. Zero keeps normal editor pose")) float Preview = 0;
	UPROPERTY(EditAnywhere, Category="Seat folding|Guide", meta=(DisplayName="Motion guide")) EVBoRMotionGuide MotionGuide = EVBoRMotionGuide::Always;
#if WITH_EDITORONLY_DATA
	UPROPERTY(VisibleAnywhere, Category="Seat folding|Guide") TObjectPtr<UVBoRMotionGuideComponent> MotionDisplay;
#endif

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void Tick(float DeltaSeconds) override;
	UFUNCTION(BlueprintCallable, Category="Seat folding") bool Fold();
	UFUNCTION(BlueprintCallable, Category="Seat folding") bool Unfold();
	UFUNCTION(BlueprintCallable, Category="Seat folding") void ResetSeat();
	UFUNCTION(BlueprintPure, Category="Seat folding") EVBoRFoldingSeatState GetState() const { return State; }
	UFUNCTION(BlueprintPure, Category="Seat folding") float GetAngleDeg() const { return AngleDeg; }
	UPROPERTY(BlueprintAssignable, Category="Seat folding") FVBoRFoldingSeatStateEvent OnStateChanged;
	UPROPERTY(BlueprintAssignable, Category="Seat folding") FVBoRFoldingSeatEvent OnReset;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Seat folding") TObjectPtr<USceneComponent> HingePivot;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Seat folding") TObjectPtr<UStaticMeshComponent> SeatMesh;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Seat folding", meta=(Units="cm")) FVector HingeLocationCm = FVector(0,0,48);
	// Preserve upright CAD position with car-origin pivot (P4e D1, U-09)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Seat folding") FTransform MeshRestFromRoot = FTransform(FQuat::Identity, FVector(0,0,78), FVector(0.08,1.7,0.6));
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Seat folding", meta=(Units="deg", ClampMin="1", ClampMax="170")) float FoldAngleDeg = 80;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Seat folding", meta=(Units="deg/s", ClampMin="0.1")) float SpeedDegPerSecond = 45;
private:
	friend class UVBoRMotionGuideComponent;

#if WITH_DEV_AUTOMATION_TESTS
	friend struct FR2aTestAccess;
#endif
	EVBoRFoldingSeatState State = EVBoRFoldingSeatState::Standing;
	float AngleDeg = 0;
	void SetState(EVBoRFoldingSeatState NewState);
	void ApplyAngle();
};
