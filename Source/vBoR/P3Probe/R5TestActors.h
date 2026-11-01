#pragma once

#include "CoreMinimal.h"
#include "../Interaction/VBoRPokeButton.h"
#include "../Interaction/VBoRKeyFob.h"
#include "R5TestActors.generated.h"

UCLASS()
class AR5ReachButton : public AVBoRPokeButton
{
	GENERATED_BODY()
public:
	AR5ReachButton() { Tags.Add(TEXT("Car.ReachButton")); }
};

UCLASS()
class AR5FrontKeyButton : public AVBoRPokeButton
{
	GENERATED_BODY()
public:
	AR5FrontKeyButton() { Tags.Add(TEXT("Car.FrontKey")); }
};

UCLASS()
class AR5FrontKey : public AVBoRKeyFob
{
	GENERATED_BODY()
public:
	AR5FrontKey() { Tags.Add(TEXT("Car.FrontKey")); }
};
