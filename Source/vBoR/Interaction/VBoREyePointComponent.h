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

	// Off for participants (editor-only arrow). P0 bench turns on (P0-c)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Eye Point", meta=(DisplayName="Show In Game"))
	bool bShowInGame = false;

protected:
	virtual void OnRegister() override;
private:
	UPROPERTY()
	TObjectPtr<UArrowComponent> Forward;
};
