#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "../Interaction/VBoRHandRigPawn.h"
#include "P2DoorBenchGameMode.generated.h"

UCLASS()
class VBOR_API AP2PalmGrabPawn : public AVBoRHandRigPawn
{
	GENERATED_BODY()
public:
	AP2PalmGrabPawn() { bAllowPalmGrab = true; }
};

UCLASS()
class VBOR_API AP2DoorBenchGameMode : public AGameModeBase
{
	GENERATED_BODY()
public:
	AP2DoorBenchGameMode();
protected:
	virtual void StartPlay() override;
};
