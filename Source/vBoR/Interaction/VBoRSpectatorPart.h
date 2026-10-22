#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "VBoRSpectatorPart.generated.h"

UINTERFACE(meta=(CannotImplementInterfaceInBlueprint))
class VBOR_API UVBoRSpectatorPart : public UInterface
{
	GENERATED_BODY()
};

// Read spectator state only, leave input/events unchanged (P5h D6)
class VBOR_API IVBoRSpectatorPart
{
	GENERATED_BODY()
public:
	virtual FText GetSpectatorName() const = 0;
	virtual bool IsSpectatorInteracting() const = 0;
};
