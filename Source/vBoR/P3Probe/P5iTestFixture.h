#pragma once

#include "CoreMinimal.h"
#include "../Interaction/VBoRVehicleAlignmentComponent.h"
#include "P5iTestFixture.generated.h"

// 사용자 BuckEye.json 을 건드리지 않고 실제 저장·읽기 경로 시험함 (P5i E15–E19)
UCLASS()
class UP5iStoredAlignment : public UVBoRVehicleAlignmentComponent
{
	GENERATED_BODY()
public:
	FString TestPath;
protected:
	virtual FString SavedAlignmentPath() const override { return TestPath; }
};
