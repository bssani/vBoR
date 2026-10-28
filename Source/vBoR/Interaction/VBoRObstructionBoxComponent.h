#pragma once

#include "CoreMinimal.h"
#include "Components/BoxComponent.h"
#include "VBoRObstructionBoxComponent.generated.h"

// Shape data only; powered panel performs obstruction queries (P5zg D1)
UCLASS(ClassGroup=(VBoR), meta=(BlueprintSpawnableComponent, DisplayName="Obstruction Box"))
class VBOR_API UVBoRObstructionBoxComponent : public UBoxComponent
{
	GENERATED_BODY()
public:
	UVBoRObstructionBoxComponent();
};
