#pragma once

#include "VBoRHingedPanel.h"
#include "VBoRHingedDoor.generated.h"

class AVBoRHingedDoor;
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FVBoRDoorEvent, AVBoRHingedDoor*, Door);

// 기존 문 클래스·이벤트 인자는 유지함 (레벨·Blueprint 참조, P2c D1)
UCLASS(Blueprintable)
class VBOR_API AVBoRHingedDoor : public AVBoRHingedPanel
{
	GENERATED_BODY()
public:
	AVBoRHingedDoor();
	UPROPERTY(BlueprintAssignable, Category = "Door") FVBoRDoorEvent OnUnlocked;
	UPROPERTY(BlueprintAssignable, Category = "Door") FVBoRDoorEvent OnLocked;
protected:
	virtual void NotifyUnlocked() override;
	virtual void NotifyLocked() override;
};
