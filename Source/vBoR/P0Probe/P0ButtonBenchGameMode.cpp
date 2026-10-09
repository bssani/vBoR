#include "P0ButtonBenchGameMode.h"
#include "P0ButtonBench.h"
#include "../Interaction/VBoRHandRigPawn.h"
#include "Engine/World.h"

AP0ButtonBenchGameMode::AP0ButtonBenchGameMode()
{
	DefaultPawnClass = AVBoRHandRigPawn::StaticClass();
	BenchClass = AP0ButtonBench::StaticClass();
}

void AP0ButtonBenchGameMode::StartPlay()
{
	// Pawn 의 modifier·data source 초기화 뒤 시험대를 생성함 (P0-b §2)
	Super::StartPlay();
	GetWorld()->SpawnActor<AP0ButtonBench>(BenchClass);
}
