#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/Grabbable/IsdkTransformer.h"
#include "Interaction/Grabbable/IsdkGrabTransformerComponent.h"
#include "VBoRAxisPart.generated.h"

class UStaticMeshComponent;
class UBoxComponent;
class UIsdkGrabbableComponent;
class UIsdkGrabberComponent;
class UIsdkOneGrabRotateTransformer;
class UIsdkOneGrabTranslateTransformer;
class UMaterialInstanceDynamic;
class UAudioComponent;
class AVBoRHingedDoor;
struct FIsdkInteractionPointerEvent;

UENUM(BlueprintType)
enum class EVBoRAxisMovement : uint8 { Rotate, Translate };
UENUM(BlueprintType)
enum class EVBoRAxisRelease : uint8 { Return, Stay };
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FVBoRAxisEvent);

UCLASS()
class VBOR_API UVBoRAxisGrabComponent : public UIsdkGrabTransformerComponent
{
	GENERATED_BODY()
public:
	UVBoRAxisGrabComponent();
	void Configure();
	void CancelSelection();
#if WITH_DEV_AUTOMATION_TESTS
	int32 CancelCalls = 0;
#endif
};

// ISDK 에 쉬는 축 좌표를 넘김 (회전·직선의 기준을 같게, P2b D3·D7)
UCLASS()
class VBOR_API UVBoRAxisTransformer : public UIsdkTransformer
{
	GENERATED_BODY()
public:
	virtual void BeginTransform(const TArray<FIsdkGrabPose>& Poses, const FIsdkTargetTransform& Target) override;
	virtual FTransform UpdateTransform(const TArray<FIsdkGrabPose>& Poses, const FIsdkTargetTransform& Target) override;
	virtual FTransform EndTransform(const FIsdkTargetTransform& Target) override;
	virtual void UpdateConstraints(const FIsdkTargetTransform& Target) override;
	virtual bool IsTransformerActive() const override { return bActive; }
	virtual int MaxGrabPoints() const override { return 1; }
	void Rebase(const FIsdkGrabPose& Pose);
	FTransform UpdateTracked(const TArray<FIsdkGrabPose>& Poses, const FIsdkTargetTransform& Target, bool bTracked);
private:
	friend class AVBoRAxisPart;
	UPROPERTY() TObjectPtr<UIsdkOneGrabRotateTransformer> Rotate;
	UPROPERTY() TObjectPtr<UIsdkOneGrabTranslateTransformer> Translate;
	bool bActive = false;
	FIsdkGrabPose LastPose;
	FIsdkTargetTransform AxisTarget() const;
	UIsdkTransformer* Selected() const;
};

UCLASS(Blueprintable)
class VBOR_API AVBoRAxisPart : public AActor
{
	GENERATED_BODY()
public:
	AVBoRAxisPart();
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void PreInitializeComponents() override;
	virtual void Tick(float DeltaSeconds) override;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Axis part") TObjectPtr<USceneComponent> AxisPivot;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Axis part") TObjectPtr<UStaticMeshComponent> PartMesh;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Axis part") TObjectPtr<UBoxComponent> GrabBox;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Axis part") TObjectPtr<UIsdkGrabbableComponent> Grabbable;
	UPROPERTY(VisibleAnywhere, Category="Axis part") TObjectPtr<UVBoRAxisGrabComponent> GrabComponent;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Axis part") FTransform MeshRestFromRoot;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Axis part") EVBoRAxisMovement Movement = EVBoRAxisMovement::Rotate;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Axis part") EVBoRAxisRelease Release = EVBoRAxisRelease::Return;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Axis part") TEnumAsByte<EAxis::Type> Axis = EAxis::X;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Axis part") bool bReverse = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Axis part", meta=(Units="deg", ClampMin="0", ClampMax="170")) float MaxAngleDeg = 30;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Axis part", meta=(Units="cm", ClampMin="0")) float MaxTravelCm = 4;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Axis part", meta=(Units="deg/s", ClampMin="0")) float ReturnDegPerSecond = 180;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Axis part", meta=(Units="cm/s", ClampMin="0")) float ReturnCmPerSecond = 20;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Axis part", meta=(Units="cm", ClampMin="0")) float GrabMarginCm = 1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Axis part", meta=(ClampMin="0", ClampMax="1")) float OpenThreshold = 0.8f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Axis part", meta=(ClampMin="0", ClampMax="1")) float OpenRearm = 0.3f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Axis part", meta=(ClampMin="0", ClampMax="1")) float ClosedThreshold = 0.05f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Axis part", meta=(ClampMin="0", ClampMax="1")) float ClosedRearm = 0.2f;
	UPROPERTY(EditAnywhere, Category="Axis part") FLinearColor HoverColor = FLinearColor(0,0.6,1,0.4);
	UPROPERTY(EditAnywhere, Category="Axis part") FLinearColor GrabColor = FLinearColor(0.5,0,1,0.4);
	UPROPERTY(EditAnywhere, Category="Axis part") FLinearColor OpenColor = FLinearColor(0,1,0.25,0.4);
	UPROPERTY(BlueprintAssignable, Category="Axis part") FVBoRAxisEvent OnPartOpened;
	UPROPERTY(BlueprintAssignable, Category="Axis part") FVBoRAxisEvent OnPartClosed;
	UPROPERTY(BlueprintAssignable, Category="Axis part") FVBoRAxisEvent OnPartReleased;
	UFUNCTION(BlueprintPure, Category="Axis part") float GetOpenRatio() const;
	UFUNCTION(BlueprintPure, Category="Axis part") FString GetDebugText() const;
	void ConnectDoor(AVBoRHingedDoor* Door, int32 Handle);
	void EndDoorHold(bool bLatch);
	int32 GetHoldingHand() const { return HoldingHand; }
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual bool IsHandTracked() const;
private:
	friend class UVBoRAxisTransformer;
	friend class AVBoRHingedDoor;
	friend struct FP2bTestAccess;
	UPROPERTY() TObjectPtr<UVBoRAxisTransformer> Transformer;
	UPROPERTY() TObjectPtr<UIsdkGrabberComponent> HoldingGrabber;
	UPROPERTY() TObjectPtr<AVBoRHingedDoor> LinkedDoor;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> StateOverlay;
	UPROPERTY() TObjectPtr<UAudioComponent> StateAudio;
	FTransform RestPivot;
	TSet<TWeakObjectPtr<UIsdkGrabberComponent>> Hovering;
	int32 HoldingHand = INDEX_NONE, DoorHandle = INDEX_NONE;
	bool bPaused = false, bRebase = false, bDoorHold = false;
	bool bCancelPending = false;
	bool bNeedsPalmOpen[2] = {false,false};
	bool bOpenArmed = true, bClosedArmed = false;
	double Value = 0, MaximumRatio = 0, PausedSeconds = 0, RecoveryDiscard = 0;
	int32 Attempts = 0, Grabs = 0, Opens = 0, Closes = 0, Pauses = 0;
	double Limit() const;
	FVector LocalAxis() const;
	FTransform ValueTransform() const;
	void AcceptTransform(const FTransform& Transform);
	void EvaluateState();
	void ReleasePart(bool bCancelSDK = true);
	UFUNCTION() void HandlePointer(const FIsdkInteractionPointerEvent& Event);
};
