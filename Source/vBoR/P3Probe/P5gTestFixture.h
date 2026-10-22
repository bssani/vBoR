#pragma once

#include "CoreMinimal.h"
#include "../Interaction/VBoRScenarioRunner.h"
#include "P5gTestFixture.generated.h"

// Observe travel request only in temporary world (P5g E15–E17)
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
