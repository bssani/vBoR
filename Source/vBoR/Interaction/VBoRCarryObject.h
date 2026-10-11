#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/Grabbable/IsdkTransformer.h"
#include "Interaction/Grabbable/IsdkGrabTransformerComponent.h"
#include "VBoRCarryMotion.h"
#include "VBoRCarryObject.generated.h"

class UStaticMeshComponent;
class UIsdkGrabbableComponent;
class UIsdkGrabTransformerComponent;
class UIsdkGrabberComponent;
class UIsdkHandRigComponent;
class UIsdkRigComponent;
class UMaterialInstanceDynamic;
class UAudioComponent;
class AVBoRLoadTarget;
struct FIsdkInteractionPointerEvent;
class AVBoRCarryObject;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FVBoRCarryEvent, AVBoRCarryObject*, Object);

enum class EVBoRCarryOutcome : uint8 { None, Assisted, DroppedInside, DroppedOutside };

struct FVBoRCarryMetrics
{
	int32 Grabbed = 0;
	int32 TwoHandEntries = 0;
	double TwoHandSeconds = 0;
	int32 BlockedCount = 0;
	int32 PauseCount = 0;
	int32 Assisted = 0;
	int32 DroppedInside = 0;
	int32 DroppedOutside = 0;
	double HeldSeconds = 0;
	double BlockedSeconds = 0;
	double PausedSeconds = 0;
	EVBoRCarryOutcome Outcome = EVBoRCarryOutcome::None;
};

// protected 정책을 C++ 에서 고정하기 위한 얇은 자식 (P1 §5)
UCLASS()
class VBOR_API UVBoRCarryGrabTransformer : public UIsdkGrabTransformerComponent
{
	GENERATED_BODY()
public:
	UVBoRCarryGrabTransformer();
	void ConfigureCarry();
	bool IsThrowingEnabled() const { return bIsThrowable; }
};

UCLASS()
class VBOR_API UVBoRCarryTransformer : public UIsdkTransformer
{
	GENERATED_BODY()
public:
	virtual void BeginTransform(const TArray<FIsdkGrabPose>& Poses, const FIsdkTargetTransform& Target) override;
	virtual FTransform UpdateTransform(const TArray<FIsdkGrabPose>& Poses, const FIsdkTargetTransform& Target) override;
	virtual FTransform EndTransform(const FIsdkTargetTransform& Target) override;
	virtual void UpdateConstraints(const FIsdkTargetTransform& Target) override {}
	virtual bool IsTransformerActive() const override { return bActive; }
	virtual int MaxGrabPoints() const override { return 2; }
private:
	bool bActive = false;
};

UCLASS(Blueprintable)
class VBOR_API AVBoRCarryObject : public AActor
{
	GENERATED_BODY()
public:
	AVBoRCarryObject();
	virtual void PreInitializeComponents() override;
	virtual void Tick(float DeltaSeconds) override;
	void ResetToPose(const FTransform& Pose);
	FString GetStateText() const;
	FString GetMeasurementText() const;
	FVBoRCarryMetrics GetMetrics() const;
	bool IsPoseClear(const FTransform& Pose) const;
	FBox GetCollisionBoundsInPose(const FTransform& Pose, const FTransform& Reference) const;
	bool TryMoveToPose(const FTransform& Pose);
	FBox GetMeshLocalBounds() const;
	void AdvanceHeldPose();
	bool IsBlocked() const { return bBlocked; }
	const FVBoRCarrySession& GetCarrySession() const { return Session; }
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Carry")
	TObjectPtr<UStaticMeshComponent> Mesh;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Carry")
	TObjectPtr<UIsdkGrabbableComponent> Grabbable;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Carry")
	TObjectPtr<UVBoRCarryGrabTransformer> GrabTransformer;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Carry")
	TObjectPtr<AVBoRLoadTarget> LoadTarget;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Carry", meta = (Units = "deg", ClampMin = "0", ClampMax = "90"))
	float TiltLimitDeg = 45;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Carry|Contact", meta = (Units = "cm", ClampMin = "0"))
	float MaxRotationPushCm = 0.5f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Carry|Contact", meta = (Units = "cm", ClampMin = "0"))
	float BlockedEnterDistanceCm = 2;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Carry|Contact", meta = (Units = "cm", ClampMin = "0"))
	float BlockedExitDistanceCm = 1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Carry|Contact", meta = (Units = "deg", ClampMin = "0"))
	float BlockedEnterAngleDeg = 5;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Carry|Contact", meta = (Units = "deg", ClampMin = "0"))
	float BlockedExitAngleDeg = 2.5f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Carry", meta = (Units = "cm", ClampMin = "0"))
	float AssistDistanceCm = 10;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Carry", meta = (Units = "deg", ClampMin = "0"))
	float AssistAngleDeg = 20;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Carry", meta = (Units = "s", ClampMin = "0.001"))
	float AssistDurationSeconds = 0.2f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Carry", meta = (Units = "cm/s", ClampMin = "1"))
	float DropSpeedCmPerSecond = 200;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Carry")
	FLinearColor HoverColor = FLinearColor(1, 0.75, 0, 0.35);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Carry")
	FLinearColor HeldColor = FLinearColor(0, 0.5, 1, 0.4);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Carry")
	FLinearColor BlockedColor = FLinearColor(1, 0, 0, 0.5);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Carry")
	FLinearColor PausedColor = FLinearColor(1, 0.35, 0, 0.5);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Carry")
	FLinearColor SeatedColor = FLinearColor(0, 1, 0.25, 0.35);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Carry")
	FLinearColor ReleasedColor = FLinearColor(0.65, 0.65, 0.65, 0.35);
	UPROPERTY(BlueprintAssignable, Category = "Carry")
	FVBoRCarryEvent OnGrabbed;
	UPROPERTY(BlueprintAssignable, Category = "Carry")
	FVBoRCarryEvent OnCarryReleased;
	UPROPERTY(BlueprintAssignable, Category = "Carry")
	FVBoRCarryEvent OnSeated;
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
	friend class FP1CarryPlacementTest;
	friend class FP1CarryBlockedGapTest;
	friend class FP1CarryCumulativeTest;
	friend class FP1BagRepeatTest;
	friend class FP1CarryTwoHandLifecycleTest;
	friend class FP1CarryDefaultAssistanceTest;
	friend class FP1CarryContactSlideTest;
	friend class FP1CarryContactRotationTest;
	friend class FP1CarryContactStateTest;
	friend class FP1CarryDefaultTiltTest;
	friend class FP1CarryContactDropTest;
	UPROPERTY()
	TObjectPtr<UVBoRCarryTransformer> CarryTransformer;
	UPROPERTY()
	TObjectPtr<UIsdkGrabberComponent> HoldingGrabbers[2];
	UPROPERTY()
	TObjectPtr<UIsdkHandRigComponent> HoldingRigs[2];
	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> StateOverlay;
	UPROPERTY()
	TObjectPtr<UAudioComponent> StateAudio;
	FVBoRCarrySession Session;
	FVBoRCarryMetrics CompletedMetrics;
	TSet<TWeakObjectPtr<UIsdkGrabberComponent>> Hovering;
	int32 Attempts[2] = {0, 0};
	int32 Successes[2] = {0, 0};
	int32 BlockedCount = 0;
	double BlockedSeconds = 0;
	double MaxRequestedGapCm = 0;
	bool bBlocked = false;
	bool bHasHeldRequest = false;
	FTransform LastHeldRequest;
	bool bPalmOpen[2] = {false, false};
	bool bDropping = false;
	bool bAssisting = false;
	bool bSeated = false;
	bool bReleasedOutside = false;
	EVBoRCarryOutcome Outcome = EVBoRCarryOutcome::None;
	double AssistStartedAt = 0;
	FTransform AssistFrom;
	FTransform AssistGoal;
	FString ReleaseResult = TEXT("none");
	FString LastState;
	FDelegateHandle PalmSelectHandles[2];
	FDelegateHandle PalmReleaseHandles[2];
	void BindHoldingRig(int32 Hand, UIsdkGrabberComponent* Grabber);
	bool BeginCarry(int32 Hand, UIsdkGrabberComponent* Grabber, const FTransform& Pose);
	void ClearHoldingRig(int32 Hand = INDEX_NONE);
	bool ReadHand(int32 Hand, FTransform& Pose) const;
	bool ReadHands(FTransform* Poses) const;
	bool IsHandTracked(int32 Hand) const;
	void ReleaseCarry(int32 Hand = INDEX_NONE, const FTransform* TrackedPoses = nullptr);
	void MoveHeldPose(const FTransform& Desired);
	void ResolveHeldContact(const FTransform& Desired);
	void FinishPlacement();
	void UpdateVisual();
	bool HasSimpleCollision() const;
	void HandlePalmSelect(UIsdkRigComponent* Rig);
	void HandlePalmRelease(UIsdkRigComponent* Rig);
	UFUNCTION()
	void HandlePointer(const FIsdkInteractionPointerEvent& Event);
};
