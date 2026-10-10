#include "P1LoadBenchGameMode.h"
#include "P1LoadBench.h"
#include "Engine/World.h"

AP1LoadBenchGameMode::AP1LoadBenchGameMode()
{
	DefaultPawnClass = AP1PalmGrabPawn::StaticClass();
	BenchClass = AP1LoadBench::StaticClass();
}

void AP1LoadBenchGameMode::StartPlay()
{
	Super::StartPlay();
	if (BenchClass) { GetWorld()->SpawnActor<AP1LoadBench>(BenchClass); }
}
