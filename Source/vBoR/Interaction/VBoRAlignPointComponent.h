#pragma once
#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "VBoRAlignPointComponent.generated.h"
class UStaticMeshComponent;
class UTextRenderComponent;

UCLASS(ClassGroup=(VBoR), meta=(BlueprintSpawnableComponent))
class VBOR_API UVBoRAlignPointComponent : public USceneComponent
{
	GENERATED_BODY()
public:
	UVBoRAlignPointComponent();
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Alignment points", meta=(ClampMin="1")) int32 Order = 1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Alignment points") FString Label;
	void ShowPoint(bool bShow);
protected:
	virtual void OnRegister() override;
private:
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Sphere;
	UPROPERTY() TObjectPtr<UTextRenderComponent> Number;
};
