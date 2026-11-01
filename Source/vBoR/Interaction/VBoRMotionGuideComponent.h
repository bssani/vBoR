#pragma once

#include "CoreMinimal.h"
#include "Components/LineBatchComponent.h"
#include "VBoRMotionGuideComponent.generated.h"

UENUM()
enum class EVBoRMotionGuide : uint8
{
	Always,
	SelectedOnly UMETA(DisplayName="Selected only"),
	Off
};

enum class EVBoRGuideShape : uint8 { Axis, Arc, Rest, Travel, Detent, Push };

struct FVBoRGuideLine
{
	FVector Start, End;
	FColor Color;
	EVBoRGuideShape Kind;
	float Thickness = 1;
	bool bArrow = false;
};

struct FVBoRGuidePush
{
	FVector Center, Inward;
	bool bOpening = false;
};

// Snapshot only; geometry generation never changes a component (R2a D6)
struct FVBoRMotionGuideInput
{
	FTransform Frame;
	FBox MeshBounds = FBox(ForceInit);
	TFunction<FTransform(double)> PoseAt;
	double First = 0, Last = 0;
	bool bRotate = true;
	TArray<float> Detents;
	int32 StartDetent = INDEX_NONE;
	TArray<FVBoRGuidePush> Pushes;
};

namespace VBoRMotionGuide
{
	VBOR_API TArray<FVBoRGuideLine> Build(const FVBoRMotionGuideInput& Input);
}

// Engine line batcher owns rendering; no project scene proxy (R2a §5)
UCLASS(NotBlueprintable, HideCategories=(Collision, Physics, Navigation, Rendering, Activation, Tags))
class VBOR_API UVBoRMotionGuideComponent : public ULineBatchComponent
{
	GENERATED_BODY()
public:
	UVBoRMotionGuideComponent();
	TArray<FVBoRGuideLine> BuildShapes() const;
	void Refresh();
	void ClearPreview();
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
protected:
	virtual void OnRegister() override;
private:
	bool ReadSource(FVBoRMotionGuideInput& Input, USceneComponent*& Pivot, double& Preview, double& EditorValue, EVBoRMotionGuide& Mode) const;
};
