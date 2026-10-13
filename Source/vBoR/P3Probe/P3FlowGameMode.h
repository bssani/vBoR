#pragma once
#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "../Interaction/VBoRHandRigPawn.h"
#include "P3FlowGameMode.generated.h"

UCLASS()
class VBOR_API AP3PalmGrabPawn : public AVBoRHandRigPawn
{
	GENERATED_BODY()
public:
	AP3PalmGrabPawn() { bAllowPalmGrab = true; }
};

UCLASS()
class VBOR_API AP3FlowGameMode : public AGameModeBase
{
	GENERATED_BODY()
public:
	AP3FlowGameMode() { DefaultPawnClass = AP3PalmGrabPawn::StaticClass(); }
};
