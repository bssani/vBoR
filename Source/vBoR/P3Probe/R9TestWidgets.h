#pragma once

#include "../Interaction/VBoRButtonLabelWidget.h"
#include "../Interaction/VBoRGuidanceWidget.h"
#include "R9TestWidgets.generated.h"

UCLASS()
class UR9TestGuidanceWidget : public UVBoRGuidanceWidget
{
	GENERATED_BODY()

public:
	bool Initialize() override;
};

UCLASS()
class UR9TestButtonLabelWidget : public UVBoRButtonLabelWidget
{
	GENERATED_BODY()

public:
	bool Initialize() override;
};

UCLASS()
class UR9TestMissingPlateWidget : public UVBoRGuidanceWidget
{
	GENERATED_BODY()

public:
	bool Initialize() override;
};
