#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/Grabbable/IsdkTransformer.h"
#include "Interaction/Grabbable/IsdkGrabTransformerComponent.h"
#include "VBoRSpectatorPart.h"
#include "VBoRSoundCue.h"
#include "VBoRSteeringWheel.generated.h"

class UStaticMeshComponent;
class UBoxComponent;
class UIsdkGrabbableComponent;
class UIsdkGrabberComponent;
class UMaterialInstanceDynamic;
class UAudioComponent;
struct FIsdkInteractionPointerEvent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FVBoRWheelLimitEvent, int32, Direction);

UCLASS()
class VBOR_API UVBoRWheelGrabComponent : public UIsdkGrabTransformerComponent
{
	GENERATED_BODY()
public:
	UVBoRWheelGrabComponent();
	void Configure();
	void CancelSelection();
	bool IsThrowingEnabled() const { return bIsThrowable; }
#if WITH_DEV_AUTOMATION_TESTS
	int32 CancelCalls = 0;
#endif
};

// SDK manages selection only; wheel computes actual angle (P5d D1/D4)
UCLASS()
class VBOR_API UVBoRWheelTransformer : public UIsdkTransformer
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
class VBOR_API AVBoRSteeringWheel : public AActor, public IVBoRSpectatorPart
{
	GENERATED_BODY()
public:
	AVBoRSteeringWheel();
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Steering wheel|Sound") FVBoRSoundCue LimitCue;
	// Slot order = property order above (P5s D12)
#if WITH_DEV_AUTOMATION_TESTS
	const FVBoRSoundPlayback& GetSoundPlaybackForTest(int32 Slot) const { return SoundPlayback[Slot]; }
	UAudioComponent* GetSoundAudioForTest(int32 Slot) const { return SoundAudio[Slot]; }
	uint64 GetSoundFrameForTest() const { return SoundFrame; }
#endif

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spectator") FText SpectatorName;
	virtual FText GetSpectatorName() const override { return SpectatorName; }
	virtual bool IsSpectatorInteracting() const override { return !HandOrder.IsEmpty(); }

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void PreInitializeComponents() override;
	virtual void Tick(float DeltaSeconds) override;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Steering wheel") TObjectPtr<USceneComponent> AxisPivot;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Steering wheel") TObjectPtr<UStaticMeshComponent> WheelMesh;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Steering wheel") TObjectPtr<UStaticMeshComponent> TopMarker;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Steering wheel") TObjectPtr<UBoxComponent> GrabBox;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Steering wheel") TObjectPtr<UIsdkGrabbableComponent> Grabbable;
	UPROPERTY(VisibleAnywhere, Category="Steering wheel") TObjectPtr<UVBoRWheelGrabComponent> GrabComponent;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Steering wheel|Input", meta=(DisplayName="Grab with mesh collision")) bool bGrabWithMesh = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Steering wheel") FTransform MeshRestFromRoot;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Steering wheel") FTransform MarkerRestFromPivot = FTransform(FQuat::Identity,FVector(-1.6,0,16.5),FVector(0.008,0.025,0.03));
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Steering wheel", meta=(Units="deg", ClampMin="0")) float LimitAngleDeg = 450;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Steering wheel", meta=(Units="cm", ClampMin="0.1")) float MinAxisDistanceCm = 6;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Steering wheel", meta=(Units="cm", ClampMin="0.1")) float MinPairDistanceCm = 10;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Steering wheel", meta=(Units="cm", ClampMin="0")) float MinGrabRadiusCm = 12;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Steering wheel", meta=(Units="cm", ClampMin="0")) float MaxGrabRadiusCm = 24;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Steering wheel", meta=(Units="cm", ClampMin="0")) float GrabMarginCm = 1;
	UPROPERTY(EditAnywhere, Category="Steering wheel") FLinearColor HoverColor = FLinearColor(0,0.6,1,0.4);
	UPROPERTY(EditAnywhere, Category="Steering wheel") FLinearColor GrabColor = FLinearColor(0.5,0,1,0.4);
	UPROPERTY(BlueprintAssignable, Category="Steering wheel") FVBoRWheelLimitEvent OnLimitReached;
	UFUNCTION(BlueprintPure, Category="Steering wheel") float GetAngleDeg() const { return static_cast<float>(AngleDeg); }
	UFUNCTION(BlueprintPure, Category="Steering wheel") FString GetDebugText() const;
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual bool ReadHand(int32 Hand, FTransform& Pose) const;
	virtual bool ReadPalmClosed(int32 Hand, bool& Closed) const;
private:
#if WITH_DEV_AUTOMATION_TESTS
	friend struct FP5sTestAccess;
#endif
	UPROPERTY() TArray<TObjectPtr<UAudioComponent>> SoundAudio;
	FVBoRSoundPlayback SoundPlayback[1];
	uint64 SoundFrame = 0;
	void AdvanceSounds(double Seconds);

	friend struct FP5dTestAccess;
	UPROPERTY() TObjectPtr<UVBoRWheelTransformer> Transformer;
	UPROPERTY() TArray<TObjectPtr<UIsdkGrabberComponent>> HoldingGrabbers;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> StateOverlay;
	UPROPERTY() TObjectPtr<UAudioComponent> StateAudio;
	TSet<TWeakObjectPtr<UIsdkGrabberComponent>> Hovering;
	TArray<int32> HandOrder;
	TArray<int32> PendingLimits;
	FTransform RestPivot;
	FVector LastDirection = FVector::ZeroVector;
	double AngleDeg = 0, RecoveryDiscardDeg = 0;
	int32 LastMode = INDEX_NONE;
	bool bBasisValid = false, bPaused = false, bGrabValid = true;
	bool bCancelPending = false, bCancelling = false;
	bool bLimitArmed[2] = {true,true};
	bool BeginHold(int32 Hand, UIsdkGrabberComponent* Grabber);
	void EndHold(int32 Hand);
	void AdvanceMotion();
	void ApplyAngle(double DeltaDeg);
	void UpdateVisual();
	UFUNCTION() void HandlePointer(const FIsdkInteractionPointerEvent& Event);
};
