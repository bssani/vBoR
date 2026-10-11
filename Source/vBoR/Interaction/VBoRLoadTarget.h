#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VBoRLoadTarget.generated.h"

class UStaticMeshComponent;
class UBoxComponent;
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
	// 목표 자세와 별도로 공간 전체를 잼 (비켜 놓인 짐도 안착 판정, P1 R4-1)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Load Target")
	TObjectPtr<UBoxComponent> LoadingRegion;
protected:
	virtual void BeginPlay() override;
private:
	bool ContainsPose(const AVBoRCarryObject* Object, const FTransform& Pose, const FTransform& Reference, const FVector& Half) const;
};
