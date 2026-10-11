#pragma once
#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "VBoREyePointComponent.generated.h"

class UArrowComponent;

UCLASS(ClassGroup = (VBoR), meta = (BlueprintSpawnableComponent))
class VBOR_API UVBoREyePointComponent : public USceneComponent
{
	GENERATED_BODY()
public:
	UVBoREyePointComponent();
private:
	UPROPERTY()
	TObjectPtr<UArrowComponent> Forward;
};
