#pragma once

#include "CoreMinimal.h"
#include "Components/PrimitiveComponent.h"
#include "P0BuckGroundGridComponent.generated.h"

struct FP0BuckGridLine
{
	FVector Start = FVector::ZeroVector;
	FVector End = FVector::ZeroVector;
};

struct FP0BuckGridSettings
{
	double HalfSizeCm = 200;
	double SpacingCm = 25;
	FLinearColor Color = FLinearColor::Yellow;
	float ThicknessPx = 2;
	float DepthBias = 0.00001f;
};

UCLASS()
class VBOR_API UP0BuckGroundGridComponent : public UPrimitiveComponent
{
	GENERATED_BODY()
public:
	UP0BuckGroundGridComponent();
	void Configure(double HalfSizeCm, double SpacingCm, const FLinearColor& Color, float ThicknessPx, float DepthBias);
	TArray<FP0BuckGridLine> BuildLines() const;
	const FP0BuckGridSettings& GetSettings() const { return Settings; }
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
private:
	FP0BuckGridSettings Settings;
};
