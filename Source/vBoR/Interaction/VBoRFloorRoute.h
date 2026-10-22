#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VBoRFloorRoute.generated.h"

class USplineComponent;
class UStaticMeshComponent;
class UMaterialInstanceDynamic;
class UStaticMesh;
class UMaterialInterface;

UCLASS(Blueprintable)
class VBOR_API AVBoRFloorRoute : public AActor
{
	GENERATED_BODY()
public:
	AVBoRFloorRoute();
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void Tick(float DeltaSeconds) override;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Floor guidance") TObjectPtr<USplineComponent> Spline;
	// Approximate geometry, placeholder values. UE length is cm (P3b D4)
	UPROPERTY(EditAnywhere, Category="Placeholder", meta=(Units="cm", ClampMin="1")) float ArrowWidthCm = 32;
	UPROPERTY(EditAnywhere, Category="Placeholder", meta=(Units="cm", ClampMin="1")) float ArrowLengthCm = 22;
	UPROPERTY(EditAnywhere, Category="Placeholder", meta=(Units="cm", ClampMin="0.1")) float StripeWidthCm = 8;
	UPROPERTY(EditAnywhere, Category="Placeholder", meta=(Units="cm", ClampMin="1")) float ArrowSpacingCm = 38;
	UPROPERTY(EditAnywhere, Category="Placeholder", meta=(Units="cm", ClampMin="0")) float FirstArrowCm = 25;
	UPROPERTY(EditAnywhere, Category="Placeholder", meta=(Units="cm", ClampMin="0")) float EndClearanceCm = 40;
	UPROPERTY(EditAnywhere, Category="Placeholder", meta=(Units="cm", ClampMin="0.1")) float FloorOffsetCm = 1;
	UPROPERTY(EditAnywhere, Category="Placeholder", meta=(Units="cm", ClampMin="1")) float RingRadiusCm = 30;
	UPROPERTY(EditAnywhere, Category="Placeholder", meta=(Units="cm", ClampMin="0.1")) float RingWidthCm = 4;
	UPROPERTY(EditAnywhere, Category="Placeholder", meta=(Units="s", ClampMin="0.01")) float PulsePeriodSeconds = 1.6;
	UPROPERTY(EditAnywhere, Category="Placeholder", meta=(Units="s", ClampMin="0")) float PulseDelaySeconds = 0.14;
	UPROPERTY(EditAnywhere, Category="Placeholder", meta=(ClampMin="0", ClampMax="1")) float DimBrightness = 0.35;
	UPROPERTY(EditAnywhere, Category="Placeholder") FLinearColor ArrowColor;
	UPROPERTY(EditAnywhere, Category="Placeholder") FLinearColor RingColor;
	UFUNCTION(BlueprintCallable, Category="Floor guidance") void SetRouteVisible(bool bVisible);
protected:
	virtual void BeginPlay() override;
private:
	friend struct FP3bTestAccess;
	UPROPERTY() TObjectPtr<UStaticMesh> Cube;
	UPROPERTY() TObjectPtr<UMaterialInterface> ColorMaterial;
	UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> Pieces;
	UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInstanceDynamic>> ArrowMaterials;
	double PulseElapsed = 0;
	void BuildVisuals();
	void UpdatePulse();
	void AddStripe(const FVector& A, const FVector& B, float Width, UMaterialInstanceDynamic* Material);
};
