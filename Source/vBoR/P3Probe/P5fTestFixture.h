#pragma once

#include "../Interaction/VBoRKnob.h"
#include "P5fTestFixture.generated.h"

// 광학 입력만 합성함. 소유·단계·Tick 알림은 실제 경로 사용 (P5f §4)
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
