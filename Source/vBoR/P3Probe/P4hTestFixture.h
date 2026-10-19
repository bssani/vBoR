#pragma once
#include "CoreMinimal.h"
#include "../Interaction/VBoRVehicleAlignmentComponent.h"
#include "P4hTestFixture.generated.h"

// 실제 손 추적과 사용자 저장 파일을 건드리지 않게 함 (P4h E3–E13)
UCLASS()
class UP4hTestAlignment : public UVBoRVehicleAlignmentComponent
{
	GENERATED_BODY()
public:
	FString TestPath;
	bool bHandPawn = true, bTracked = true;
	FVector StageTip = FVector::ZeroVector;
	double Now = 100;
	int32 Origin = 3;
	FVector2D Area = FVector2D(300,400);
protected:
	virtual FString SavedAlignmentPath() const override { return TestPath; }
	virtual bool CanReadPointTip() const override { return bHandPawn; }
	virtual bool ReadPointTip(FVector& WorldTip) const override;
	virtual double PointTime() const override { return Now; }
	virtual void ReadPointEnvironment(int32& OutOrigin, FVector2D& OutArea) const override { OutOrigin = Origin; OutArea = Area; }
};
