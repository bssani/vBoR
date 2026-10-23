#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "Rig/IsdkHandRigComponent.h"
#include "VBoRHandRigPawn.generated.h"

class UCameraComponent;
class UMotionControllerComponent;
struct FVBoRDoorHand;

// Thin rig child to add required limiter to protected modifier list (P0-b §3)
UCLASS()
class VBOR_API UVBoRHandRigLeft : public UIsdkHandRigComponentLeft
{
	GENERATED_BODY()
public:
	UVBoRHandRigLeft();
	bool ReadPalmGrab(bool& bOutClosed) const;
	bool ReadPinchGrab(bool& bOutClosed) const;
};

UCLASS()
class VBOR_API UVBoRHandRigRight : public UIsdkHandRigComponentRight
{
	GENERATED_BODY()
public:
	UVBoRHandRigRight();
	bool ReadPalmGrab(bool& bOutClosed) const;
	bool ReadPinchGrab(bool& bOutClosed) const;
};

UCLASS(Blueprintable)
class VBOR_API AVBoRHandRigPawn : public APawn
{
	GENERATED_BODY()
public:
	AVBoRHandRigPawn();
	virtual void PreRegisterAllComponents() override;
	virtual void PreInitializeComponents() override;
	UIsdkHandRigComponent* GetHandRig(int32 HandIndex) const;
	bool ReadRawIndexTip(int32 HandIndex, FVector& OutWorldTip) const;
	bool ReadRawPalmAndTips(int32 HandIndex, FVBoRDoorHand& OutHand) const;
	FString DescribeRig() const;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hand Rig")
	bool bAllowPalmGrab = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hand Rig")
	TObjectPtr<UCameraComponent> Camera;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hand Rig")
	TObjectPtr<UMotionControllerComponent> LeftMotion;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hand Rig")
	TObjectPtr<UMotionControllerComponent> RightMotion;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hand Rig")
	TObjectPtr<UVBoRHandRigLeft> LeftRig;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hand Rig")
	TObjectPtr<UVBoRHandRigRight> RightRig;
};
