#include "VBoRHingedDoor.h"
#include "Components/BoxComponent.h"

AVBoRHingedDoor::AVBoRHingedDoor()
{
	bUseLatch = true;
	OutsideHandle->SetRelativeLocation(FVector(100, 9, 60));
	InsideHandle->SetRelativeLocation(FVector(85, -9, 50));
	SyncSettings();
}

void AVBoRHingedDoor::NotifyUnlocked()
{
	Super::NotifyUnlocked();
	OnUnlocked.Broadcast(this);
}

void AVBoRHingedDoor::NotifyLocked()
{
	Super::NotifyLocked();
	OnLocked.Broadcast(this);
}
