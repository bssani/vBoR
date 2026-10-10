#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VBoRLoadTarget.generated.h"

class UStaticMeshComponent;
class AVBoRCarryObject;

UCLASS(Blueprintable)
class VBOR_API AVBoRLoadTarget : public AActor
{
	GENERATED_BODY()
public:
	AVBoRLoadTarget();
	virtual void OnConstruction(const FTransform& Transform) override;
	FTransform GetGoalPose() const;
	bool IsStaticFit(const AVBoRCarryObject* Object) const;
	bool IsObjectInside(const AVBoRCarryObject* Object) const;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Load Target", meta = (Units = "cm"))
	FVector GoalSizeCm = FVector(28, 28, 120);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Load Target")
	bool bShowGuide = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Load Target")
	FLinearColor GuideColor = FLinearColor(0, 0.8, 0.4, 0.15);
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Load Target")
	TObjectPtr<UStaticMeshComponent> Guide;
protected:
	virtual void BeginPlay() override;
private:
	bool ContainsPose(const AVBoRCarryObject* Object, const FTransform& Pose) const;
};
