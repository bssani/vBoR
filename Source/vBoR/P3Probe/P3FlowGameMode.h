#pragma once
#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "../Interaction/VBoRPawn.h"
#include "P3FlowGameMode.generated.h"

UCLASS()
class VBOR_API AP3PalmGrabPawn : public AVBoRPawn
{
	GENERATED_BODY()
};

UCLASS()
class VBOR_API AP3FlowGameMode : public AGameModeBase
{
	GENERATED_BODY()
public:
	AP3FlowGameMode() { DefaultPawnClass = AP3PalmGrabPawn::StaticClass(); }
};
