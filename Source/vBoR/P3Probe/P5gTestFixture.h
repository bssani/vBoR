#pragma once

#include "CoreMinimal.h"
#include "../Interaction/VBoRScenarioRunner.h"
#include "P5gTestFixture.generated.h"

// 임시 world 에서는 travel 대신 요청만 관찰함 (P5g E15–E17)
UCLASS()
class AP5gReloadObserver : public AVBoRScenarioRunner
{
	GENERATED_BODY()
public:
	int32 ReloadRequests = 0;
	FName ReloadLevel;
protected:
	virtual void RequestLevelReload(FName LevelName) override { ++ReloadRequests; ReloadLevel = LevelName; }
};
