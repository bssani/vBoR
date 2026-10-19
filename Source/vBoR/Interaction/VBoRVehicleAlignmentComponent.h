#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "VBoRBuckAlignment.h"
#include "VBoRVehicleAlignmentComponent.generated.h"

class APawn;
class UVBoREyePointComponent;
class UVBoRAlignPointComponent;
class UVBoRViewpointComponent;
DECLARE_MULTICAST_DELEGATE(FVBoRPawnMoved);
DECLARE_MULTICAST_DELEGATE_OneParam(FVBoRPointRecord, const FString&);

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
	FString SwitchRejection(const UVBoRViewpointComponent* Viewpoint, bool bReturnBase = false) const;
	FString ReapplyRejection() const;
	bool RequestReapply();
	bool MoveHomeShift(FVector2D DeltaCm);
	bool ResetHomeShift();
	FVector2D GetHomeShiftCm() const { return HomeShiftCm; }
	FString EnglishReason() const;
	FString SeatName() const;
	int32 ReapplyCompletions = 0;
	FString Describe() const;
	bool ReadWorldHead(FTransform& OutHead) const;
	USceneComponent* GetCurrentEye() const;
	FString DescribeHead(bool bTracked, const FVector& WorldHead) const;
	FVBoRPawnMoved OnPawnMoved;
	FVBoRPointRecord OnPointRecord;
	FString CapturePointRejection() const;
	FString SavePointsRejection() const;
	bool CapturePoint();
	bool SavePoints(FString& Failure);
	void CancelPoints(const FString& Message = TEXT("Point alignment canceled"));
	void FlowStarted();
	bool IsPointAligning() const { return bPointSession; }
	bool IsCapturingPoint() const { return bPointCapturing; }
	bool ArePointsVisible() const { return bPointsVisible; }
	const TArray<FVBoRAlignPointSample>& GetCapturedPoints() const { return CapturedPoints; }
	FString PointStatus() const;
	void AdvancePointCapture();
	UPROPERTY(EditAnywhere, Category="점 정렬", meta=(Units="s", ClampMin="0.01")) double PointCaptureSeconds = 1;
	UPROPERTY(EditAnywhere, Category="점 정렬", meta=(ClampMin="0", ClampMax="1")) int32 PointHand = 0;
	UPROPERTY(EditAnywhere, Category="점 정렬", meta=(Units="mm", ClampMin="0")) double PointSpreadLimitMm = 10;
	UPROPERTY(EditAnywhere, Category="점 정렬", meta=(Units="cm", ClampMin="0")) double PointMinSpanCm = 20;
	UPROPERTY(EditAnywhere, Category="점 정렬", meta=(Units="cm", ClampMin="0")) double PointFitLimitCm = 2;
	UPROPERTY(EditAnywhere, Category="점 정렬", meta=(Units="cm", ClampMin="0")) double PointHeightWarnCm = 3;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="정렬") TObjectPtr<UVBoREyePointComponent> BaseEye;
	UPROPERTY(EditAnywhere, Category="정렬", meta=(Units="s", ClampMin="0.01")) float FadeSeconds = 0.3;
	UPROPERTY(EditAnywhere, Category="집 시험", meta=(Units="cm", ClampMin="0")) float HomeShiftStepCm = 100;
	UPROPERTY(EditAnywhere, Category="정렬", meta=(Units="cm", ClampMin="0")) float PlayAreaToleranceCm = 1;
	UPROPERTY(EditAnywhere, Category="정렬", meta=(ToolTip="Pawn height from the virtual floor, not from the saved head height (physical seat at real car height, U-17)")) bool bHeightFromFloor = true;
	UPROPERTY(EditAnywhere, Category="정렬", meta=(Units="cm", EditCondition="bHeightFromFloor")) float FloorWorldZCm = 0;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="정렬") EVBoRAlignmentState State = EVBoRAlignmentState::NoSavedValue;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="정렬") FString Reason;
	int32 SwitchRequests = 0, SwitchCompletions = 0, SwitchDiscards = 0;
	void InitializeAlignment(APawn* Pawn, const FVBoRBuckSavedAlignment* Value, int32 Origin, const FVector2D& PlayArea);
	void AdvanceSwitch(double DeltaSeconds);
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual FString SavedAlignmentPath() const;
	virtual bool CanReadPointTip() const;
	virtual bool ReadPointTip(FVector& WorldTip) const;
	virtual double PointTime() const;
	virtual void ReadPointEnvironment(int32& Origin, FVector2D& PlayArea) const;
private:
	UPROPERTY() TObjectPtr<APawn> AlignmentPawn;
	UPROPERTY() TObjectPtr<UVBoRViewpointComponent> CurrentViewpoint;
	UPROPERTY() TObjectPtr<UVBoRViewpointComponent> PendingViewpoint;
	FVBoRBuckSavedAlignment Saved;
	FVector2D HomeShiftCm = FVector2D::ZeroVector;
	bool bHasSaved = false, bSwitching = false, bMoved = false;
	bool bReapplying = false;
	double SwitchElapsed = 0;
	FDelegateHandle RecenterHandle;
	void HandleRecenter();
	bool RequestSwitch(UVBoRViewpointComponent* Viewpoint);
	void LoadSetupAlignment();
	FTransform PawnFor(const FTransform& Eye) const;
	bool BasePawn(FTransform& Out, FString& Failure) const;
	USceneComponent* DriverSeatReference(FString& Failure) const;
	FString FindAlignPoints(TArray<UVBoRAlignPointComponent*>& Out) const;
	bool FitCaptured(const TArray<FVBoRAlignPointSample>& Samples, FVBoRPointFit& Fit, FString& Failure) const;
	void ShowPoints(bool bShow);
	void PointResult(const FString& Message);
	TArray<FVBoRAlignPointSample> CapturedPoints;
	TArray<FVector> PointWindow;
	bool bPointSession = false, bPointCapturing = false, bPointsVisible = false;
	double PointElapsed = 0, LastPointTime = 0;
	int32 PointOrigin = INDEX_NONE;
	FVector2D PointPlayArea = FVector2D::ZeroVector;
	FString LastPointResult;

};
