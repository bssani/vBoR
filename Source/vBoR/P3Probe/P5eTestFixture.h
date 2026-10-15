#pragma once

#include "../Interaction/VBoRTwoAxisPart.h"
#include "P5eTestFixture.generated.h"

// 광학 자세·원래 손점만 합성함 (부품의 Tick·소유·피드백은 실제 경로, P5e §4)
UCLASS()
class AP5eTrackedPart : public AVBoRTwoAxisPart
{
	GENERATED_BODY()
public:
	FTransform Wrist;
	FVBoRDoorHand Hands[2];
	bool bTracked = true, bClosed = true;
protected:
	virtual bool ReadGrabPose(FTransform& Pose) const override { Pose = Wrist; return bTracked; }
	virtual bool ReadClosed(bool& Closed) const override { Closed = bClosed; return true; }
	virtual void ReadPushHands(FVBoRDoorHand (&Output)[2]) const override { Output[0] = Hands[0]; Output[1] = Hands[1]; }
};
