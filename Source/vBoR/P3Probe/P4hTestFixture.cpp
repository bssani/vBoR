#include "P4hTestFixture.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Pawn.h"

bool UP4hTestAlignment::ReadPointTip(FVector& WorldTip) const
{
	const APawn* Pawn = UGameplayStatics::GetPlayerPawn(this,0);
	if (!Pawn || !bTracked) { return false; }
	WorldTip = Pawn->GetActorTransform().TransformPosition(StageTip); return true;
}
