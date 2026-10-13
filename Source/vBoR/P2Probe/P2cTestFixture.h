#pragma once

#include "CoreMinimal.h"
#include "../Interaction/VBoRHingedDoor.h"
#include "P2cTestFixture.generated.h"

// 추적 대신 경첩 좌표의 손만 넣음 (실제 판 Tick 검증, P2c §4)
UCLASS()
class AP2cTrackedPanel : public AVBoRHingedPanel
{
	GENERATED_BODY()
public:
	FVBoRDoorHand Hands[2];
	bool bPalmClosed[2] = {true, true};
protected:
	virtual bool ReadHand(int32 Hand, FVBoRDoorHand& Input) override { Input = Hands[Hand]; return Input.bTracked; }
	virtual bool ReadPalmClosed(int32 Hand, bool& bClosed) const override { bClosed = bPalmClosed[Hand]; return true; }
};

UCLASS()
class UP2cEventObserver : public UObject
{
	GENERATED_BODY()
public:
	int32 Unlocks = 0, Locks = 0, DoorUnlocks = 0, DoorLocks = 0;
	UPROPERTY() TObjectPtr<AVBoRHingedDoor> LastDoor;
	UFUNCTION() void Unlocked(AVBoRHingedPanel* Panel) { ++Unlocks; }
	UFUNCTION() void Locked(AVBoRHingedPanel* Panel) { ++Locks; }
	UFUNCTION() void DoorUnlocked(AVBoRHingedDoor* Door) { ++DoorUnlocks; LastDoor = Door; }
	UFUNCTION() void DoorLocked(AVBoRHingedDoor* Door) { ++DoorLocks; LastDoor = Door; }
};
