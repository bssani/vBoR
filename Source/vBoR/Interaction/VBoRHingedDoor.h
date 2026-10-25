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
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Door|Coast", meta=(DisplayName="Coast after release", ToolTip="Approximate feel, not real door force"))
	bool bCoastAfterRelease = false;
	// Approximate feel; tune in Quest 3 (deg/s^2, P5za D1)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Door|Coast", meta=(DisplayName="Coast deceleration (deg/s^2)", ClampMin="0.01", ToolTip="Approximate feel, not real door force. Deceleration in deg/s^2"))
	float CoastDecelerationDegPerSecond2 = 180;
	UPROPERTY(BlueprintAssignable, Category = "Door") FVBoRDoorEvent OnUnlocked;
	UPROPERTY(BlueprintAssignable, Category = "Door") FVBoRDoorEvent OnLocked;
protected:
	virtual void SyncMotionSettings(FVBoRDoorMotion& State) const override;
	virtual void NotifyUnlocked() override;
	virtual void NotifyLocked() override;
};
