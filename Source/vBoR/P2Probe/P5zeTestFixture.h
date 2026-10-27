#pragma once

#include "P2bTestFixture.h"
#include "P5zeTestFixture.generated.h"

// Inject optical tracking/hand opening only; keep SDK selection (P5ze)
UCLASS()
class AP5zeTrackedPart : public AP2bTrackedPart
{
	GENERATED_BODY()
public:
	bool bClosed[2] = {true, true};
protected:
	virtual bool ReadGrabClosed(int32 Hand, bool Pinch, bool& Closed) const override
	{
		Closed = bClosed[Hand]; return bTracked;
	}
};

UCLASS()
class UP5zePartObserver : public UObject
{
	GENERATED_BODY()
public:
	int32 Opens = 0, Closes = 0, Detents = 0, Nudges = 0;
	UFUNCTION() void Opened() { ++Opens; }
	UFUNCTION() void Closed() { ++Closes; }
	UFUNCTION() void Detent(int32 Index) { ++Detents; }
	UFUNCTION() void Nudge(int32 Direction) { ++Nudges; }
};
