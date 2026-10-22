#pragma once

#include "VBoRHingedPanel.h"
#include "VBoRHingedDoor.generated.h"

class AVBoRHingedDoor;
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FVBoRDoorEvent, AVBoRHingedDoor*, Door);

// Keep existing door class/event arguments (level/Blueprint references, P2c D1)
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
