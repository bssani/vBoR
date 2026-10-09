#include "P0ButtonBenchGameMode.h"
#include "P0ButtonBench.h"
#include "../Interaction/VBoRHandRigPawn.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

AP0ButtonBenchGameMode::AP0ButtonBenchGameMode()
{
	DefaultPawnClass = AVBoRHandRigPawn::StaticClass();
	BenchClass = AP0ButtonBench::StaticClass();
}

void AP0ButtonBenchGameMode::StartPlay()
{
	// Pawn 의 modifier·data source 초기화 뒤 시험대를 생성함 (P0-b §2)
	Super::StartPlay();
	if (APlayerController* Controller = UGameplayStatics::GetPlayerController(this, 0))
	{
		const FRotator Rotation(0, Controller->GetControlRotation().Yaw, 0);
		const FVector Location = Controller->GetPawn() ? Controller->GetPawn()->GetActorLocation() : Controller->GetSpawnLocation();
		GetWorld()->SpawnActor<AP0ButtonBench>(BenchClass, Location + Rotation.RotateVector(BenchOffsetCm), Rotation);
	}
}
