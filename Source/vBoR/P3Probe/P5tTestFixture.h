#pragma once

#include "CoreMinimal.h"
#include "../Interaction/VBoRCarryObject.h"
#include "P5tTestFixture.generated.h"

// 받침이 클래스 크기를 쓰고 나머지 정책만 덮는지 확인함 (P5t E3·E4)
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
