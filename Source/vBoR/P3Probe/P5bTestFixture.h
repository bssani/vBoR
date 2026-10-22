#pragma once

#include "CoreMinimal.h"
#include "../P2Probe/P2bTestFixture.h"
#include "Components/AudioComponent.h"
#include "P5bTestFixture.generated.h"

// Count Play requests instead of actual audio (listen on Quest, P5b E3)
UCLASS()
class UP5bAudioObserver : public UAudioComponent
{
	GENERATED_BODY()
public:
	int32 Plays = 0;
	virtual void Play(float StartTime = 0) override { ++Plays; }
};

UCLASS()
class UP5bEventObserver : public UObject
{
	GENERATED_BODY()
public:
	TArray<int32> Indices;
	int32 Opens = 0, Closes = 0, Releases = 0, Presses = 0;
	UFUNCTION() void Changed(int32 Index) { Indices.Add(Index); }
	UFUNCTION() void Opened() { ++Opens; }
	UFUNCTION() void Closed() { ++Closes; }
	UFUNCTION() void Released() { ++Releases; }
	UFUNCTION() void Pressed(const FIsdkInteractionPointerEvent& Event) { ++Presses; }
};
