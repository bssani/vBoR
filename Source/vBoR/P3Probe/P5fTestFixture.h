#pragma once

#include "../Interaction/VBoRKnob.h"
#include "P5fTestFixture.generated.h"

// Synthesize optical input only. Real ownership/step/Tick notification path (P5f §4)
UCLASS()
class AP5fTrackedKnob : public AVBoRKnob
{
	GENERATED_BODY()
public:
	FTransform Palm;
	bool bTracked = true, bClosed = true;
	int32 SoundCount = 0;
	TArray<int32> Steps;
	UFUNCTION() void RecordStep(int32 Direction) { Steps.Add(Direction); }
protected:
	virtual bool ReadGrabPose(FTransform& Pose) const override { Pose = Palm; return bTracked; }
	virtual bool ReadClosed(bool& Closed) const override { Closed = bClosed; return true; }
	virtual void PlayStepSound() override { ++SoundCount; }
};
