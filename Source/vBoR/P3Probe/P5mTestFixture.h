#pragma once

#include "P5cTestFixture.h"
#include "Interaction/IsdkGrabberComponent.h"
#include "P5mTestFixture.generated.h"

// Synthesize optical hand input only. Product SDK selection/transform/detent path (P5m E2–E13)
UCLASS()
class AP5mTrackedPart : public AP2bTrackedPart
{
	GENERATED_BODY()
public:
	FVBoRDoorHand Hands[2];
	UPROPERTY() TObjectPtr<UIsdkGrabberComponent> PushGrabbers[2];
	bool Closed[2] = {true,true};
	mutable bool LastReadPinch = false;
protected:
	virtual UIsdkGrabberComponent* GetPushGrabber(int32 Hand) const override { return PushGrabbers[Hand]; }
	virtual void ReadPushHands(FVBoRDoorHand (&Out)[2]) const override { Out[0] = Hands[0]; Out[1] = Hands[1]; }
	virtual bool ReadGrabClosed(int32 Hand, bool Pinch, bool& Out) const override { LastReadPinch = Pinch; Out = Closed[Hand]; return true; }
};
