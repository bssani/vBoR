#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "Rig/IsdkHandRigComponent.h"
#include "VBoRHandRigPawn.generated.h"

class UCameraComponent;
class UMotionControllerComponent;

// protected modifier 목록에 필수 limiter 를 넣기 위한 얇은 리그 자식 (P0-b §3)
UCLASS()
class VBOR_API UVBoRHandRigLeft : public UIsdkHandRigComponentLeft
{
	GENERATED_BODY()
public:
	UVBoRHandRigLeft();
};

UCLASS()
class VBOR_API UVBoRHandRigRight : public UIsdkHandRigComponentRight
{
	GENERATED_BODY()
public:
	UVBoRHandRigRight();
};

UCLASS(Blueprintable)
class VBOR_API AVBoRHandRigPawn : public APawn
{
	GENERATED_BODY()
public:
	AVBoRHandRigPawn();
	UIsdkHandRigComponent* GetHandRig(int32 HandIndex) const;
	bool ReadRawIndexTip(int32 HandIndex, FVector& OutWorldTip) const;
	FString DescribeRig() const;

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
