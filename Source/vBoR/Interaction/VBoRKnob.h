#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/Grabbable/IsdkTransformer.h"
#include "Interaction/Grabbable/IsdkGrabTransformerComponent.h"
#include "VBoRKnobMotion.h"
#include "VBoRSpectatorPart.h"
#include "VBoRSoundCue.h"
#include "VBoRKnob.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class UAudioComponent;
class UIsdkGrabbableComponent;
class UIsdkGrabberComponent;
class UMaterialInstanceDynamic;
struct FIsdkInteractionPointerEvent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FVBoRKnobStep, int32, Direction);

UCLASS()
class VBOR_API UVBoRKnobGrabComponent : public UIsdkGrabTransformerComponent
{
	GENERATED_BODY()
public:
	UVBoRKnobGrabComponent();
	void Configure();
	void CancelSelection() { ForceCancel(); }
	bool IsThrowingEnabled() const { return bIsThrowable; }
};

// SDK 는 잡기만 맡고 축 회전은 Tick 에서 정함 (축 위 잡기 점 허용, P5f D1·D3)
UCLASS()
class VBOR_API UVBoRKnobTransformer : public UIsdkTransformer
{
	GENERATED_BODY()
public:
	virtual void BeginTransform(const TArray<FIsdkGrabPose>& Poses, const FIsdkTargetTransform& Target) override { bActive = !Poses.IsEmpty(); }
	virtual FTransform UpdateTransform(const TArray<FIsdkGrabPose>& Poses, const FIsdkTargetTransform& Target) override;
	virtual FTransform EndTransform(const FIsdkTargetTransform& Target) override { bActive = false; return UpdateTransform({},Target); }
	virtual void UpdateConstraints(const FIsdkTargetTransform& Target) override {}
	virtual bool IsTransformerActive() const override { return bActive; }
	virtual int MaxGrabPoints() const override { return 1; }
private:
	bool bActive = false;
};

UCLASS(Blueprintable)
class VBOR_API AVBoRKnob : public AActor, public IVBoRSpectatorPart
{
	GENERATED_BODY()
public:
	AVBoRKnob();
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Knob|Sound") FVBoRSoundCue StepCue;
	// 칸 순서는 위 프로퍼티 순서와 같음 (P5s D12)
#if WITH_DEV_AUTOMATION_TESTS
	const FVBoRSoundPlayback& GetSoundPlaybackForTest(int32 Slot) const { return SoundPlayback[Slot]; }
	UAudioComponent* GetSoundAudioForTest(int32 Slot) const { return SoundAudio[Slot]; }
	uint64 GetSoundFrameForTest() const { return SoundFrame; }
#endif

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spectator") FText SpectatorName;
	virtual FText GetSpectatorName() const override { return SpectatorName; }
	virtual bool IsSpectatorInteracting() const override { return Motion.GrabHand != INDEX_NONE; }

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void PreInitializeComponents() override;
	virtual void Tick(float DeltaSeconds) override;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Knob") TObjectPtr<USceneComponent> Pivot;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Knob") TObjectPtr<USceneComponent> RotationAxis;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Knob") TObjectPtr<UStaticMeshComponent> KnobMesh;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Knob") TObjectPtr<UStaticMeshComponent> RotationMarker;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Knob") TObjectPtr<UBoxComponent> GrabBox;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Knob") TObjectPtr<UIsdkGrabbableComponent> Grabbable;
	UPROPERTY(VisibleAnywhere, Category="Knob") TObjectPtr<UVBoRKnobGrabComponent> GrabComponent;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Knob") TObjectPtr<UAudioComponent> StateAudio;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Knob|Input", meta=(DisplayName="메시 충돌로 잡기")) bool bGrabWithMesh = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Knob") FTransform MeshRestFromRoot = FTransform(FQuat::Identity,FVector(0,0,1.25),FVector(0.06,0.06,0.025));
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Knob") FTransform MarkerRestFromPivot = FTransform(FQuat::Identity,FVector(2.2,0,2.65),FVector(0.012,0.006,0.003));
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Knob", meta=(Units="deg", ClampMin="0")) float StepDeg = 15;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Knob", meta=(Units="deg", ClampMin="0")) float StepHysteresisDeg = 3;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Knob", meta=(Units="deg", ClampMin="0")) float JumpThresholdDeg = 30;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Knob", meta=(ClampMin="0.1")) float TwistGain = 1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Knob", meta=(Units="cm", ClampMin="0")) float GrabMarginCm = 1;
	UPROPERTY(EditAnywhere, Category="Knob") FLinearColor HoverColor = FLinearColor(0,0.6,1,0.4);
	UPROPERTY(EditAnywhere, Category="Knob") FLinearColor GrabColor = FLinearColor(0.5,0,1,0.4);
	UPROPERTY(BlueprintAssignable, Category="Knob") FVBoRKnobStep OnKnobStep;
	UFUNCTION(BlueprintPure, Category="Knob") double GetAngleDeg() const { return Motion.AngleDeg; }
	UFUNCTION(BlueprintPure, Category="Knob") int64 GetStepIndex() const { return Motion.StepIndex; }
	UFUNCTION(BlueprintPure, Category="Knob") FString GetDebugText() const;
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual bool ReadGrabPose(FTransform& Pose) const;
	virtual bool ReadClosed(bool& Closed) const;
	virtual void PlayStepSound();
private:
#if WITH_DEV_AUTOMATION_TESTS
	friend struct FP5sTestAccess;
#endif
	UPROPERTY() TArray<TObjectPtr<UAudioComponent>> SoundAudio;
	FVBoRSoundPlayback SoundPlayback[1];
	uint64 SoundFrame = 0;
	void AdvanceSounds(double Seconds);

	friend struct FP5fTestAccess;
	UPROPERTY() TObjectPtr<UVBoRKnobTransformer> Transformer;
	UPROPERTY() TObjectPtr<UIsdkGrabberComponent> HoldingGrabber;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> StateOverlay;
	TSet<TWeakObjectPtr<UIsdkGrabberComponent>> Hovering;
	FVBoRKnobMotion Motion;
	bool bValid = true, bRetainUntilOpen = false, bCancelPending = false, bCancelling = false;
	void Layout();
	bool BeginHold(int32 Hand, UIsdkGrabberComponent* Grabber);
	void AdvanceGrab();
	void ReleaseHold();
	void ApplyPose();
	void UpdateVisual();
	UFUNCTION() void HandlePointer(const FIsdkInteractionPointerEvent& Event);
};
