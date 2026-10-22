#pragma once

#include "CoreMinimal.h"
#include "../Interaction/VBoRAxisPart.h"
#include "../Interaction/VBoRHingedDoor.h"
#include "Interaction/Pointable/IsdkInteractionPointerEvent.h"
#include "P2bTestFixture.generated.h"

// Synthesize tracking only; grab/release through real SDK (P2b R1-3)
UCLASS()
class AP2bTrackedPart : public AVBoRAxisPart
{
	GENERATED_BODY()
public:
	bool bTracked = true;
protected:
	virtual bool IsHandTracked() const override { return bTracked; }
};

UCLASS()
class AP2bTrackedDoor : public AVBoRHingedDoor
{
	GENERATED_BODY()
public:
	FVBoRDoorHand Hands[2];
	bool bPalmClosed[2] = {true, true};
protected:
	virtual bool ReadHand(int32 Hand, FVBoRDoorHand& Input) override { Input = Hands[Hand]; return Input.bTracked; }
	virtual bool ReadPalmClosed(int32 Hand, bool& bClosed) const override { bClosed = bPalmClosed[Hand]; return true; }
};

UCLASS()
class UP2bPointerObserver : public UObject
{
	GENERATED_BODY()
public:
	UPROPERTY() TObjectPtr<UVBoRAxisGrabComponent> Grab;
	int32 UnselectPoses = INDEX_NONE, CancelEvents = 0, ReleaseEvents = 0;
	UFUNCTION() void Pointer(const FIsdkInteractionPointerEvent& Event)
	{
		if (Event.Type == EIsdkPointerEventType::Unselect) { UnselectPoses = Grab->GetNumGrabbers(); }
	}
	UFUNCTION() void Cancel(int32 Identifier, UIsdkGrabTransformerComponent* Component) { ++CancelEvents; }
	UFUNCTION() void Released() { ++ReleaseEvents; }
};
