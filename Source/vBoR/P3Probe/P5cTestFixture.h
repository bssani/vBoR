#pragma once

#include "P5bTestFixture.h"
#include "P5cTestFixture.generated.h"

UCLASS()
class UP5cEventObserver : public UObject
{
	GENERATED_BODY()
public:
	TArray<int32> Directions, Indices;
	TArray<FString> Order;
	UFUNCTION() void Nudged(int32 Direction) { Directions.Add(Direction); Order.Add(FString::Printf(TEXT("N%d"),Direction)); }
	UFUNCTION() void Changed(int32 Index) { Indices.Add(Index); Order.Add(FString::Printf(TEXT("D%d"),Index)); }
};
