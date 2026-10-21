#pragma once

#include "CoreMinimal.h"
#include "P5qTestFixture.generated.h"

// BPAssignable 과 같은 reflected 수신 경로로 횟수·값을 셈 (P5q E2·E11)
UCLASS()
class UP5qPowerObserver : public UObject
{
	GENERATED_BODY()
public:
	TArray<bool> Values;
	UFUNCTION() void Changed(bool bOn) { Values.Add(bOn); }
};
