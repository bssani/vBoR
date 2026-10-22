#pragma once

#include "../Interaction/VBoRTwoAxisPart.h"
#include "P5eTestFixture.generated.h"

// Synthesize optical pose/raw hand points only (real part Tick/ownership/feedback, P5e §4)
UCLASS()
class AP5eTrackedPart : public AVBoRTwoAxisPart
{
	GENERATED_BODY()
public:
	FTransform Wrist = FTransform(FVector(-20,0,0));
	FVBoRDoorHand Hands[2];
	bool bTracked = true, bClosed = true;
protected:
	virtual bool ReadGrabPose(FTransform& Pose) const override { Pose = Wrist; return bTracked; }
	virtual bool ReadClosed(bool& Closed) const override { Closed = bClosed; return true; }
	virtual void ReadPushHands(FVBoRDoorHand (&Output)[2]) const override { Output[0] = Hands[0]; Output[1] = Hands[1]; }
};
