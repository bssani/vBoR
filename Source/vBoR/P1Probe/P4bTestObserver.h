#pragma once

#include "CoreMinimal.h"
#include "../Interaction/VBoRPoweredPanel.h"
#include "../Interaction/VBoRCarryObject.h"
#include "P4bTestObserver.generated.h"

// 실제 dynamic delegate 의 횟수·순서만 관찰함 (P4b §4)
UCLASS()
class UP4bTestObserver : public UObject
{
	GENERATED_BODY()
public:
	int32 Opened = 0, Closed = 0, Obstructed = 0;
	TArray<EVBoRPoweredState> States;
	TWeakObjectPtr<AVBoRCarryObject> HitBox;
	UFUNCTION() void State(AVBoRPoweredPanel* Panel, EVBoRPoweredState Value) { States.Add(Value); }
	UFUNCTION() void Open(AVBoRPoweredPanel* Panel) { ++Opened; }
	UFUNCTION() void Close(AVBoRPoweredPanel* Panel) { ++Closed; }
	UFUNCTION() void Obstruction(AVBoRPoweredPanel* Panel, AVBoRCarryObject* Box) { ++Obstructed; HitBox = Box; }
};
