#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "P1TrunkMock.generated.h"

class UStaticMeshComponent;
class UTextRenderComponent;

UCLASS()
class VBOR_API AP1TrunkMock : public AActor
{
	GENERATED_BODY()
public:
	AP1TrunkMock();
	virtual void OnConstruction(const FTransform& Transform) override;

	// UE cm. Opening local X=0, inside +X (P1 §2 D3)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Approximate Trunk", meta = (Units = "cm", ClampMin = "1"))
	float InsideWidthCm = 130.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Approximate Trunk", meta = (Units = "cm", ClampMin = "1"))
	float InsideDepthCm = 90.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Approximate Trunk", meta = (Units = "cm", ClampMin = "1"))
	float InsideHeightCm = 70.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Approximate Trunk", meta = (Units = "cm"))
	float FloorHeightCm = 75.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Approximate Trunk", meta = (Units = "cm", ClampMin = "0"))
	float LipHeightCm = 5.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Approximate Trunk", meta = (Units = "cm", ClampMin = "0.1"))
	float PanelThicknessCm = 5.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Approximate Trunk") TObjectPtr<UStaticMeshComponent> Floor;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Approximate Trunk") TObjectPtr<UStaticMeshComponent> LeftWall;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Approximate Trunk") TObjectPtr<UStaticMeshComponent> RightWall;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Approximate Trunk") TObjectPtr<UStaticMeshComponent> FrontWall;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Approximate Trunk") TObjectPtr<UStaticMeshComponent> Ceiling;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Approximate Trunk") TObjectPtr<UStaticMeshComponent> BumperLip;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Approximate Trunk") TObjectPtr<UTextRenderComponent> ApproximateLabel;

	FVector GetOpeningCenterLocal() const { return FVector(0, 0, FloorHeightCm + InsideHeightCm * 0.5f); }
};
