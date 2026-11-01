#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "../Interaction/VBoRPoweredPanel.h"
#include "R2aTestObserver.generated.h"

class AVBoRHingedPanel;

UCLASS()
class UR2aTestObserver : public UObject
{
	GENERATED_BODY()
public:
	int32 Events = 0;
	UFUNCTION() void Angle(float Value) { ++Events; }
	UFUNCTION() void Axis() { ++Events; }
	UFUNCTION() void Detent(int32 Index) { ++Events; }
	UFUNCTION() void Panel(AVBoRHingedPanel* Part) { ++Events; }
	UFUNCTION() void Powered(AVBoRPoweredPanel* Part, EVBoRPoweredState State) { ++Events; }
};
