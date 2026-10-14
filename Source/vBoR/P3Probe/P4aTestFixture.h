#pragma once

#include "CoreMinimal.h"
#include "../Interaction/VBoRKeyFob.h"
#include "P4aTestFixture.generated.h"

// 추적 High 여부만 합성함. 선택·이동·물리 복원은 SDK 경로 사용 (P4a E1·E6·E7)
UCLASS()
class AP4aTrackedKey : public AVBoRKeyFob
{
	GENERATED_BODY()
public:
	bool bHigh[2] = {true, true};
protected:
	virtual bool IsHandTracked() const override { return GetHoldingHand() != INDEX_NONE && bHigh[GetHoldingHand()]; }
};

UCLASS()
class UP4aKeyObserver : public UObject
{
	GENERATED_BODY()
public:
	int32 Unlocks = 0, Locks = 0, CarUnlocks = 0, CarLocks = 0, ButtonPresses = 0;
	UPROPERTY() TArray<TObjectPtr<AVBoRKeyFob>> CarKeys;
	UFUNCTION() void Unlock(AVBoRKeyFob* Key) { ++Unlocks; }
	UFUNCTION() void Lock(AVBoRKeyFob* Key) { ++Locks; }
	UFUNCTION() void CarUnlock(AVBoRKeyFob* Key) { ++CarUnlocks; CarKeys.Add(Key); }
	UFUNCTION() void CarLock(AVBoRKeyFob* Key) { ++CarLocks; CarKeys.Add(Key); }
	UFUNCTION() void Button(const FIsdkInteractionPointerEvent& Event) { ++ButtonPresses; }
};
