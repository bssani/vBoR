#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "../Interaction/VBoRHandRigPawn.h"
#include "P1LoadBenchGameMode.generated.h"

class AP1LoadBench;

UCLASS()
class VBOR_API AP1PalmGrabPawn : public AVBoRHandRigPawn
{
	GENERATED_BODY()
public:
	AP1PalmGrabPawn() { bAllowPalmGrab = true; }
};

UCLASS()
class VBOR_API AP1LoadBenchGameMode : public AGameModeBase
{
	GENERATED_BODY()
public:
	AP1LoadBenchGameMode();
	UPROPERTY(EditAnywhere, Category = "Bench") TSubclassOf<AP1LoadBench> BenchClass;
protected:
	virtual void StartPlay() override;
};
