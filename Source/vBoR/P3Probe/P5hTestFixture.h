#pragma once

#include "Interaction/IsdkPokeInteractable.h"
#include "P5hTestFixture.generated.h"

// Synthesize SDK selection state only (real reader/collection path, P5h E10)
UCLASS()
class UP5hPokeState : public UIsdkPokeInteractable
{
	GENERATED_BODY()
public:
	virtual EIsdkInteractableState GetInteractableState() const override { return SimulatedState; }
	void SetState(EIsdkInteractableState State) { SimulatedState = State; }
private:
	EIsdkInteractableState SimulatedState = EIsdkInteractableState::Disabled;
};
