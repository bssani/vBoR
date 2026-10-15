#pragma once

#include "../Interaction/VBoRSteeringWheel.h"
#include "P5bTestFixture.h"
#include "P5dTestFixture.generated.h"

// 광학 입력만 합성함 (회전·소유·알림은 실제 경로, P5d §4)
UCLASS()
class AP5dTrackedWheel : public AVBoRSteeringWheel
{
	GENERATED_BODY()
public:
	FTransform Hands[2];
	bool bTracked[2] = {true,true};
	bool bClosed[2] = {true,true};
protected:
	virtual bool ReadHand(int32 Hand, FTransform& Pose) const override { Pose = Hands[Hand]; return bTracked[Hand]; }
	virtual bool ReadPalmClosed(int32 Hand, bool& Closed) const override { Closed = bClosed[Hand]; return true; }
};

UCLASS()
class UP5dLimitObserver : public UObject
{
	GENERATED_BODY()
public:
	TArray<int32> Directions;
	UFUNCTION() void Reached(int32 Direction) { Directions.Add(Direction); }
};
