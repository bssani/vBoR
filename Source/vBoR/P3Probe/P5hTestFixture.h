#pragma once

#include "Interaction/IsdkPokeInteractable.h"
#include "P5hTestFixture.generated.h"

// SDK 가 보내는 선택 상태만 합성함 (실제 읽기 함수·수집 경로 확인, P5h E10)
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
