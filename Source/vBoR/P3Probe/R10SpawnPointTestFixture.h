#pragma once

#include "GameFramework/Actor.h"
#include "../Interaction/VBoRCarryObject.h"
#include "R10SpawnPointTestFixture.generated.h"

UCLASS()
class AR10SizedBox : public AVBoRCarryObject
{
	GENERATED_BODY()

public:
	AR10SizedBox() { BoxSizeCm = FVector(50, 40, 20); }
};

UCLASS()
class AR10CollisionActor : public AActor
{
	GENERATED_BODY()

public:
	AR10CollisionActor();
};
