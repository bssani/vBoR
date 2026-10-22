#pragma once

#include "CoreMinimal.h"
#include "../Interaction/VBoRCarryObject.h"
#include "P5tTestFixture.generated.h"

// Stand uses class size, overrides other policy only (P5t E3/E4)
UCLASS()
class AP5tSizedBox : public AVBoRCarryObject
{
	GENERATED_BODY()
public:
	AP5tSizedBox()
	{
		BoxSizeCm = FVector(20, 30, 40);
		bUsePlacementAssist = true;
	}
};
