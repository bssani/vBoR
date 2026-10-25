#include "VBoRHingedDoor.h"

AVBoRHingedDoor::AVBoRHingedDoor()
{
	bUseLatch = true;
	SyncSettings();
}

void AVBoRHingedDoor::SyncMotionSettings(FVBoRDoorMotion& State) const
{
	Super::SyncMotionSettings(State);
	State.bCoast = bCoastAfterRelease;
	State.CoastDecelerationDegPerSecond2 = FMath::Max(0.01f, CoastDecelerationDegPerSecond2);
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
