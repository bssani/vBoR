#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "VBoRBuckAlignment.h"
#include "VBoRVehicleAlignmentComponent.generated.h"

class APawn;
class UVBoREyePointComponent;
class UVBoRViewpointComponent;

UENUM(BlueprintType)
enum class EVBoRAlignmentState : uint8 { Applied, NoSavedValue, NeedsCheck };

UCLASS(ClassGroup=(VBoR), meta=(BlueprintSpawnableComponent))
class VBOR_API UVBoRVehicleAlignmentComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UVBoRVehicleAlignmentComponent();
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	UFUNCTION(BlueprintCallable, Category="정렬") bool SwitchViewpoint(UVBoRViewpointComponent* Viewpoint);
	UFUNCTION(BlueprintCallable, Category="정렬") bool ReturnToBase();
	UFUNCTION(BlueprintPure, Category="정렬") bool IsBaseViewpoint() const { return CurrentViewpoint == nullptr && !bSwitching; }
	bool IsSwitching() const { return bSwitching; }
	bool IsAtViewpoint(const UVBoRViewpointComponent* Viewpoint) const { return !bSwitching && CurrentViewpoint == Viewpoint; }
	FString Describe() const;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="정렬") TObjectPtr<UVBoREyePointComponent> BaseEye;
	UPROPERTY(EditAnywhere, Category="정렬", meta=(Units="s", ClampMin="0.01")) float FadeSeconds = 0.3;
	UPROPERTY(EditAnywhere, Category="정렬", meta=(Units="cm", ClampMin="0")) float PlayAreaToleranceCm = 1;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="정렬") EVBoRAlignmentState State = EVBoRAlignmentState::NoSavedValue;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="정렬") FString Reason;
	int32 SwitchRequests = 0, SwitchCompletions = 0, SwitchDiscards = 0;
	void InitializeAlignment(APawn* Pawn, const FVBoRBuckSavedAlignment* Value, int32 Origin, const FVector2D& PlayArea);
	void AdvanceSwitch(double DeltaSeconds);
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
private:
	UPROPERTY() TObjectPtr<APawn> AlignmentPawn;
	UPROPERTY() TObjectPtr<UVBoRViewpointComponent> CurrentViewpoint;
	UPROPERTY() TObjectPtr<UVBoRViewpointComponent> PendingViewpoint;
	FVBoRBuckSavedAlignment Saved;
	bool bHasSaved = false, bSwitching = false, bMoved = false;
	double SwitchElapsed = 0;
	FDelegateHandle RecenterHandle;
	void HandleRecenter();
	bool RequestSwitch(UVBoRViewpointComponent* Viewpoint);
};
