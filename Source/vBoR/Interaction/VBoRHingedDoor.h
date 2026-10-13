#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/Grabbable/IsdkTransformer.h"
#include "Interaction/Grabbable/IsdkGrabTransformerComponent.h"
#include "VBoRDoorMotion.h"
#include "VBoRHingedDoor.generated.h"

class UStaticMeshComponent;
class UBoxComponent;
class UIsdkGrabbableComponent;
class UIsdkGrabberComponent;
class AVBoRHandRigPawn;
class UMaterialInstanceDynamic;
class UAudioComponent;
struct FIsdkInteractionPointerEvent;
class AVBoRHingedDoor;
class AVBoRAxisPart;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FVBoRDoorEvent, AVBoRHingedDoor*, Door);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FVBoRDoorAngleEvent, float, AngleDeg);

UCLASS()
class VBOR_API UVBoRDoorGrabTransformer : public UIsdkGrabTransformerComponent
{
	GENERATED_BODY()
public:
	UVBoRDoorGrabTransformer();
	void ConfigureDoor();
	bool IsThrowingEnabled() const { return bIsThrowable; }
};

// ISDK 는 선택만 맡고 문 Tick 의 검증된 자세를 반환함 (P2 §5)
UCLASS()
class VBOR_API UVBoRDoorTransformer : public UIsdkTransformer
{
	GENERATED_BODY()
public:
	virtual void BeginTransform(const TArray<FIsdkGrabPose>& Poses, const FIsdkTargetTransform& Target) override { bActive = !Poses.IsEmpty(); }
	virtual FTransform UpdateTransform(const TArray<FIsdkGrabPose>& Poses, const FIsdkTargetTransform& Target) override;
	virtual FTransform EndTransform(const FIsdkTargetTransform& Target) override;
	virtual void UpdateConstraints(const FIsdkTargetTransform& Target) override {}
	virtual bool IsTransformerActive() const override { return bActive; }
	virtual int MaxGrabPoints() const override { return 2; }
private:
	bool bActive = false;
};

UCLASS(Blueprintable)
class VBOR_API AVBoRHingedDoor : public AActor
{
	GENERATED_BODY()
public:
	AVBoRHingedDoor();
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void PreInitializeComponents() override;
	virtual void Tick(float DeltaSeconds) override;
	UFUNCTION(BlueprintCallable, Category = "Door") void ResetDoor();
	const FVBoRDoorMotion& GetMotion() const { return Motion; }
	FString GetStateText() const;
	FString GetMeasurementText(const FVBoRDoorMetrics* Baseline = nullptr) const;
	void SetAxisHandle(int32 Handle, AVBoRAxisPart* Part);
	bool BeginGrabFromAxisPart(AVBoRAxisPart* Part);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door") FTransform MeshRestFromRoot = FTransform(FQuat::Identity, FVector(55,0,52.5), FVector(1.1,0.1,1.05));
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Door") TObjectPtr<AVBoRAxisPart> OutsideAxisHandle;
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Door") TObjectPtr<AVBoRAxisPart> InsideAxisHandle;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Door") TObjectPtr<USceneComponent> HingePivot;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Door") TObjectPtr<UStaticMeshComponent> DoorMesh;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Door") TObjectPtr<UBoxComponent> OutsideHandle;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Door") TObjectPtr<UBoxComponent> InsideHandle;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Door") TObjectPtr<UIsdkGrabbableComponent> OutsideGrab;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Door") TObjectPtr<UIsdkGrabbableComponent> InsideGrab;
	UPROPERTY(VisibleAnywhere, Category = "Door") TObjectPtr<UVBoRDoorGrabTransformer> OutsideTransformer;
	UPROPERTY(VisibleAnywhere, Category = "Door") TObjectPtr<UVBoRDoorGrabTransformer> InsideTransformer;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door", meta = (Units = "deg", ClampMin = "0", ClampMax = "180")) float MaxAngleDeg = 70;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door", meta = (Units = "deg/s", ClampMin = "0")) float MaxSpeedDegPerSecond = 180;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door", meta = (Units = "deg", ClampMin = "0")) float LatchAngleDeg = 2;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door", meta = (Units = "deg", ClampMin = "0")) float RearmAngleDeg = 5;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door", meta = (Units = "deg", ClampMin = "0", ClampMax = "90")) float PalmLimitDeg = 60;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door", meta = (Units = "cm", ClampMin = "0")) float MinAxisDistanceCm = 15;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door") FLinearColor LockedColor = FLinearColor(0.65, 0.65, 0.65, 0.25);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door") FLinearColor UnlockedColor = FLinearColor(0, 1, 0.25, 0.3);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door") FLinearColor PushingColor = FLinearColor(0, 0.6, 1, 0.4);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door") FLinearColor GrabbedColor = FLinearColor(0.5, 0, 1, 0.4);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door") FLinearColor PausedColor = FLinearColor(1, 0.35, 0, 0.5);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door") FLinearColor MaximumColor = FLinearColor(1, 0.75, 0, 0.4);
	UPROPERTY(BlueprintAssignable, Category = "Door") FVBoRDoorEvent OnUnlocked;
	UPROPERTY(BlueprintAssignable, Category = "Door") FVBoRDoorEvent OnLocked;
	UPROPERTY(BlueprintAssignable, Category = "Door") FVBoRDoorAngleEvent OnAngleChanged;
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual bool ReadHand(int32 Hand, FVBoRDoorHand& Input);
	virtual bool ReadPalmClosed(int32 Hand, bool& bClosed) const;
private:
	friend struct FP2bTestAccess;
	UPROPERTY() TObjectPtr<AVBoRAxisPart> HoldingAxisPart;
	int32 AxisHoldHand = INDEX_NONE, AxisHoldHandle = INDEX_NONE;
	double AxisHoldStartSeconds = 0;
	void EndAxisHold(const TCHAR* Reason, bool bLatch);
	bool BeginAxisGrab(AVBoRAxisPart* Part, const FVBoRDoorHand& Input);
	UPROPERTY() TObjectPtr<UVBoRDoorTransformer> DoorTransformer;
	UPROPERTY() TObjectPtr<AVBoRHandRigPawn> HandPawn;
	UPROPERTY() TObjectPtr<UIsdkGrabberComponent> HoldingGrabber;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> StateOverlay;
	UPROPERTY() TObjectPtr<UAudioComponent> StateAudio;
	TSet<TWeakObjectPtr<UIsdkGrabberComponent>> Hovering[2];
	bool bNeedsPalmOpen[2] = {false, false};
	bool bCancelPending = false;
	FVBoRDoorMotion Motion;
	FQuat ClosedHingeRotation = FQuat::Identity;
	FTransform HingeFrame() const;
	void SyncSettings();
	void ApplyState(double BeforeAngle, int32 BeforeUnlocks, int32 BeforeLatches);
	void CancelSelections(const TCHAR* Reason);
	UFUNCTION() void HandleOutsidePointer(const FIsdkInteractionPointerEvent& Event);
	UFUNCTION() void HandleInsidePointer(const FIsdkInteractionPointerEvent& Event);
	void HandlePointer(int32 Handle, const FIsdkInteractionPointerEvent& Event);
};
