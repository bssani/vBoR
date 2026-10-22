#pragma once

#include "CoreMinimal.h"
#include "P5qTestFixture.generated.h"

// Count calls/values through same reflected receiver path as BPAssignable (P5q E2/E11)
UCLASS()
class UP5qPowerObserver : public UObject
{
	GENERATED_BODY()
public:
	TArray<bool> Values;
	UFUNCTION() void Changed(bool bOn) { Values.Add(bOn); }
};
