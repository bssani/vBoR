#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "P0ButtonBenchGameMode.generated.h"

class AP0ButtonBench;

UCLASS()
class VBOR_API AP0ButtonBenchGameMode : public AGameModeBase
{
	GENERATED_BODY()
public:
	AP0ButtonBenchGameMode();
	UPROPERTY(EditAnywhere, Category = "Bench")
	TSubclassOf<AP0ButtonBench> BenchClass;
	UPROPERTY(EditAnywhere, Category = "Bench", meta = (Units = "cm"))
	FVector BenchOffsetCm = FVector(45, 0, 120);
protected:
	virtual void StartPlay() override;
};
