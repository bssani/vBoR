#pragma once

#include "P5cTestFixture.h"
#include "Interaction/IsdkGrabberComponent.h"
#include "P5mTestFixture.generated.h"

// 광학 손 입력만 합성함. SDK 선택·변환·걸림은 제품 경로 사용 (P5m E2–E13)
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
