#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "../Interaction/VBoRFoldingMirror.h"
#include "../Interaction/VBoRFoldingSeat.h"
#include "../Interaction/VBoRPoweredPanel.h"
#include "../Interaction/VBoRHingedPanel.h"
#include "R3TestObserver.generated.h"

UCLASS()
class UR3TestObserver : public UObject
{
	GENERATED_BODY()
public:
	TMap<AVBoRFoldingMirror*, TArray<EVBoRFoldingMirrorState>> MirrorStates;
	TMap<AVBoRFoldingSeat*, TArray<EVBoRFoldingSeatState>> SeatStates;
	TMap<AActor*, int32> Resets;
	TArray<FString> MirrorOrder;
	TArray<FString> SeatOrder;
	TArray<FString> GateOrder;
	int32 GateStates = 0;
	UFUNCTION() void MirrorChanged(AVBoRFoldingMirror* Mirror, EVBoRFoldingMirrorState State)
	{
		MirrorStates.FindOrAdd(Mirror).Add(State);
		MirrorOrder.Add(StaticEnum<EVBoRFoldingMirrorState>()->GetNameStringByValue(static_cast<int64>(State)));
	}
	UFUNCTION() void SeatChanged(AVBoRFoldingSeat* Seat, EVBoRFoldingSeatState State)
	{
		SeatStates.FindOrAdd(Seat).Add(State);
		SeatOrder.Add(StaticEnum<EVBoRFoldingSeatState>()->GetNameStringByValue(static_cast<int64>(State)));
	}
	UFUNCTION() void MirrorReset(AVBoRFoldingMirror* Mirror) { ++Resets.FindOrAdd(Mirror); MirrorOrder.Add(TEXT("Reset")); }
	UFUNCTION() void SeatReset(AVBoRFoldingSeat* Seat) { ++Resets.FindOrAdd(Seat); SeatOrder.Add(TEXT("Reset")); }
	UFUNCTION() void GateReset(AVBoRPoweredPanel* Panel) { ++Resets.FindOrAdd(Panel); GateOrder.Add(TEXT("Reset")); }
	UFUNCTION() void DoorReset(AVBoRHingedPanel* Panel) { ++Resets.FindOrAdd(Panel); }
	UFUNCTION() void GateChanged(AVBoRPoweredPanel* Panel, EVBoRPoweredState State) { ++GateStates; GateOrder.Add(TEXT("State")); }
};
