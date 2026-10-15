#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/IsdkGrabbableComponent.h"
#include "Interaction/Grabbable/IsdkGrabTransformerComponent.h"
#include "VBoRKeyFob.generated.h"

class UStaticMeshComponent;
class UIsdkGrabFreeTransformer;
class UIsdkThrowable;
class AVBoRPokeButton;
class AVBoRKeyTable;
class AVBoRKeyFob;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FVBoRKeyEvent, AVBoRKeyFob*, Key);

UCLASS()
class VBOR_API UVBoRKeyGrabComponent : public UIsdkGrabTransformerComponent
{
	GENERATED_BODY()
public:
	UVBoRKeyGrabComponent();
	void Configure();
};

// SDK 가 속도를 넣은 뒤 검사함 (PostEvent 의 delegate 는 물리 복원보다 먼저 불림, P4a D5)
UCLASS()
class VBOR_API UVBoRKeyGrabbableComponent : public UIsdkGrabbableComponent
{
	GENERATED_BODY()
public:
	virtual void PostEvent(const FIsdkInteractionPointerEvent& Event) override;
};

UCLASS(Blueprintable)
class VBOR_API AVBoRKeyFob : public AActor
{
	GENERATED_BODY()
public:
	AVBoRKeyFob();
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void PreInitializeComponents() override;
	virtual void Tick(float DeltaSeconds) override;
	void SetHome(AVBoRKeyTable* Table);
	int32 GetHoldingHand() const { return HoldingHand; }
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Key") TObjectPtr<UStaticMeshComponent> KeyMesh;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Key") TObjectPtr<UVBoRKeyGrabbableComponent> Grabbable;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Key") TObjectPtr<UVBoRKeyGrabComponent> GrabComponent;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Key") TObjectPtr<UIsdkGrabFreeTransformer> FreeTransformer;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Key") TObjectPtr<AVBoRPokeButton> UnlockButton;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Key") TObjectPtr<AVBoRPokeButton> LockButton;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Approximation", meta=(Units="cm", ClampMin="0.1")) FVector KeySizeCm = FVector(6,3,1.5);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Approximation", meta=(Units="cm")) FVector UnlockButtonCm = FVector(1.5,0,0.95);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Approximation", meta=(Units="cm")) FVector LockButtonCm = FVector(-1.5,0,0.95);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Approximation", meta=(Units="cm", ClampMin="0.1")) FVector ButtonSizeCm = FVector(1.2,1.2,0.4);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Key", meta=(Units="cm", ClampMin="0")) float FloorMarginCm = 2;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Key") TWeakObjectPtr<AVBoRKeyTable> HomeTable;
	UPROPERTY(BlueprintAssignable, Category="Key") FVBoRKeyEvent OnUnlockPressed;
	UPROPERTY(BlueprintAssignable, Category="Key") FVBoRKeyEvent OnLockPressed;
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual bool IsHandTracked() const;
private:
	friend class UVBoRKeyGrabbableComponent;
	friend struct FP4aTestAccess;
	int32 HoldingHand = INDEX_NONE;
	TWeakObjectPtr<UObject> HoldingInteractor;
	TArray<bool> RecentHighFrames;
	bool bLowSinceLastTick = false;
	void ObserveTracking();
	void RecordTrackingFrame();
	void AfterPointer(const FIsdkInteractionPointerEvent& Event);
	bool CanPress(const FIsdkInteractionPointerEvent& Event) const;
	AVBoRPokeButton* CreateButton(FVector OffsetCm, FLinearColor Color);
	void ClearVelocity();
	UFUNCTION() void UnlockPressed(const FIsdkInteractionPointerEvent& Event);
	UFUNCTION() void LockPressed(const FIsdkInteractionPointerEvent& Event);
};
