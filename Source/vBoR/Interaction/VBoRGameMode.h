#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "VBoRPawn.h"
#include "VBoRGameMode.generated.h"

UCLASS()
class VBOR_API AVBoRGameMode : public AGameModeBase
{
	GENERATED_BODY()
public:
	AVBoRGameMode() { DefaultPawnClass = AVBoRPawn::StaticClass(); }
};
