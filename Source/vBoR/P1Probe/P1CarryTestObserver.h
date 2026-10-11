#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "P1CarryTestObserver.generated.h"

class AVBoRCarryObject;

// dynamic delegate 호출 횟수만 받는 시험 fixture (P1 R1-4)
UCLASS()
class UP1CarryTestObserver : public UObject
{
	GENERATED_BODY()
public:
	int32 SeatedCount = 0;
	int32 GrabbedCount = 0;
	int32 ReleasedCount = 0;
	UFUNCTION() void OnSeated(AVBoRCarryObject* Object) { ++SeatedCount; }
	UFUNCTION() void OnGrabbed(AVBoRCarryObject* Object) { ++GrabbedCount; }
	UFUNCTION() void OnReleased(AVBoRCarryObject* Object) { ++ReleasedCount; }
};
