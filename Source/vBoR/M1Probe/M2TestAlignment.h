#pragma once

#include "../P3Probe/P4hTestFixture.h"
#include "M2TestAlignment.generated.h"

UCLASS()
class UM2TestAlignment : public UP4hTestAlignment
{
	GENERATED_BODY()
public:
	FString Directory;
	virtual FString SavedAlignmentPath() const override { return UVBoRVehicleAlignmentComponent::SavedAlignmentPath(); }
protected:
	virtual FString CalibrationDirectory() const override { return Directory; }
};
