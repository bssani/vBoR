#pragma once
#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "../Interaction/VBoRHandRigPawn.h"
#include "Interaction/IsdkGrabberComponent.h"
#include "P3FlowGameMode.generated.h"

UCLASS()
class VBOR_API AP3PalmGrabPawn : public AVBoRHandRigPawn
{
	GENERATED_BODY()
public:
	AP3PalmGrabPawn() { bAllowPalmGrab = true; }
	virtual void PreInitializeComponents() override
	{
		Super::PreInitializeComponents();
		// P3 키만 Pinch 를 받음. 다른 부품의 Pinch 금지·P1 rig 는 그대로 둠 (P4a D4)
		if (bAllowPalmGrab)
		{
			for (int32 Hand = 0; Hand < 2; ++Hand) { GetHandRig(Hand)->GetGrabber()->SetGrabInputMethodAllowed(EIsdkGrabInputMethod::Pinch, true); }
		}
	}
};

UCLASS()
class VBOR_API AP3FlowGameMode : public AGameModeBase
{
	GENERATED_BODY()
public:
	AP3FlowGameMode() { DefaultPawnClass = AP3PalmGrabPawn::StaticClass(); }
};
