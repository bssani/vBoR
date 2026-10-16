#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "VBoRSpectatorPart.generated.h"

UINTERFACE(meta=(CannotImplementInterfaceInBlueprint))
class VBOR_API UVBoRSpectatorPart : public UInterface
{
	GENERATED_BODY()
};

// 입력·이벤트를 건드리지 않고 관전 화면에서 상태만 읽음 (P5h D6)
class VBOR_API IVBoRSpectatorPart
{
	GENERATED_BODY()
public:
	virtual FText GetSpectatorName() const = 0;
	virtual bool IsSpectatorInteracting() const = 0;
};
