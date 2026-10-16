#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/Grabbable/IsdkTransformer.h"
#include "Interaction/Grabbable/IsdkGrabTransformerComponent.h"
#include "VBoRTwoAxisMotion.h"
#include "VBoRSpectatorPart.h"
#include "VBoRTwoAxisPart.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class UIsdkGrabbableComponent;
class UIsdkGrabberComponent;
class UMaterialInstanceDynamic;
struct FIsdkInteractionPointerEvent;

UCLASS()
class VBOR_API UVBoRTwoAxisGrabComponent : public UIsdkGrabTransformerComponent
{
	GENERATED_BODY()
public:
	UVBoRTwoAxisGrabComponent();
	void Configure();
	void CancelSelection() { ForceCancel(); }
	bool IsThrowingEnabled() const { return bIsThrowable; }
};

// SDK 의 자유 회전을 막고 Tick 의 2축 자세를 돌려줌 (P5e D1·D3)
UCLASS()
class VBOR_API UVBoRTwoAxisTransformer : public UIsdkTransformer
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
class VBOR_API AVBoRTwoAxisPart : public AActor, public IVBoRSpectatorPart
{
	GENERATED_BODY()
public:
	AVBoRTwoAxisPart();
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spectator") FText SpectatorName;
	virtual FText GetSpectatorName() const override { return SpectatorName; }
	virtual bool IsSpectatorInteracting() const override { return Motion.GrabHand != INDEX_NONE || Motion.PushHand != INDEX_NONE; }

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void PreInitializeComponents() override;
	virtual void Tick(float DeltaSeconds) override;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Two axis part") TObjectPtr<USceneComponent> Pivot;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Two axis part") TObjectPtr<USceneComponent> YawAxis;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Two axis part") TObjectPtr<USceneComponent> PitchAxis;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Two axis part") TObjectPtr<UStaticMeshComponent> PartMesh;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Two axis part") TObjectPtr<UBoxComponent> GrabBox;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Two axis part") TObjectPtr<UIsdkGrabbableComponent> Grabbable;
	UPROPERTY(VisibleAnywhere, Category="Two axis part") TObjectPtr<UVBoRTwoAxisGrabComponent> GrabComponent;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Two axis part") FTransform MeshRestFromRoot = FTransform(FQuat::Identity,FVector(-3,0,0),FVector(0.02,0.25,0.07));
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Two axis part", meta=(Units="deg", ClampMin="0")) float YawLimitDeg = 25;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Two axis part", meta=(Units="deg", ClampMin="0")) float PitchLimitDeg = 15;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Two axis part", meta=(Units="cm", ClampMin="0")) float CenterExclusionCm = 2;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Two axis part", meta=(Units="cm", ClampMin="0")) float FrontToleranceCm = 0.5;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Two axis part", meta=(Units="cm", ClampMin="0")) float DepthMarginCm = 1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Two axis part", meta=(Units="cm", ClampMin="0")) float EdgeMarginCm = 0.5;
	UPROPERTY(EditAnywhere, Category="Two axis part") FLinearColor HoverColor = FLinearColor(0,0.6,1,0.4);
	UPROPERTY(EditAnywhere, Category="Two axis part") FLinearColor GrabColor = FLinearColor(0.5,0,1,0.4);
	UPROPERTY(EditAnywhere, Category="Two axis part") FLinearColor PushingColor = FLinearColor(0,1,0.3,0.4);
	UFUNCTION(BlueprintPure, Category="Two axis part") float GetYawDeg() const { return static_cast<float>(Motion.YawDeg); }
	UFUNCTION(BlueprintPure, Category="Two axis part") float GetPitchDeg() const { return static_cast<float>(Motion.PitchDeg); }
	UFUNCTION(BlueprintPure, Category="Two axis part") FString GetDebugText() const;
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual bool ReadGrabPose(FTransform& Pose) const;
	virtual bool ReadClosed(bool& Closed) const;
	virtual void ReadPushHands(FVBoRDoorHand (&Hands)[2]) const;
private:
	friend struct FP5eTestAccess;
	UPROPERTY() TObjectPtr<UVBoRTwoAxisTransformer> Transformer;
	UPROPERTY() TObjectPtr<UIsdkGrabberComponent> HoldingGrabber;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> StateOverlay;
	TSet<TWeakObjectPtr<UIsdkGrabberComponent>> Hovering;
	FVBoRTwoAxisMotion Motion;
	bool bValid = true, bPinch = false, bRetainUntilOpen = false, bCancelPending = false, bCancelling = false;
	bool BeginHold(int32 Hand, UIsdkGrabberComponent* Grabber);
	void AdvanceGrab();
	void ApplyPose();
	void UpdateVisual();
	UFUNCTION() void HandlePointer(const FIsdkInteractionPointerEvent& Event);
};
