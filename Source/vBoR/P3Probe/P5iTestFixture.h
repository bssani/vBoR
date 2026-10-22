#pragma once

#include "CoreMinimal.h"
#include "../Interaction/VBoRVehicleAlignmentComponent.h"
#include "P5iTestFixture.generated.h"

// Real save/read path without touching user BuckEye.json (P5i E15–E19)
UCLASS()
class UP5iStoredAlignment : public UVBoRVehicleAlignmentComponent
{
	GENERATED_BODY()
public:
	FString TestPath;
protected:
	virtual FString SavedAlignmentPath() const override { return TestPath; }
};
