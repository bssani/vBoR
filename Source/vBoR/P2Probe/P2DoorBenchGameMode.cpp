#include "P2DoorBenchGameMode.h"
#include "P2DoorBench.h"
#include "Engine/World.h"

AP2DoorBenchGameMode::AP2DoorBenchGameMode() { DefaultPawnClass = AP2PalmGrabPawn::StaticClass(); }

void AP2DoorBenchGameMode::StartPlay()
{
	Super::StartPlay();
	GetWorld()->SpawnActor<AP2DoorBench>();
}
