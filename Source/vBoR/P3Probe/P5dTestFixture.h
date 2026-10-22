#pragma once

#include "../Interaction/VBoRSteeringWheel.h"
#include "P5bTestFixture.h"
#include "P5dTestFixture.generated.h"

// Synthesize optical input only (real rotation/ownership/notification path, P5d §4)
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
