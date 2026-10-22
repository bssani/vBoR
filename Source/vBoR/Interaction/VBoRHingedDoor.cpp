#include "VBoRHingedDoor.h"

AVBoRHingedDoor::AVBoRHingedDoor()
{
	bUseLatch = true;
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
