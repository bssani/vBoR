#pragma once

#include "CoreMinimal.h"
#include "Components/BoxComponent.h"
#include "VBoRParticipantZones.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FVBoRApproachEvent);

// 추적된 실제 HMD 자세만 읽음 (limiter·시선 조준과 무관, P3 §2)
VBOR_API bool VBoRReadWorldHead(UWorld* World, FTransform& OutHead);
DECLARE_LOG_CATEGORY_EXTERN(LogP3Flow, Log, All);

UCLASS(ClassGroup=(VBoR), meta=(BlueprintSpawnableComponent))
class VBOR_API UVBoRApproachZoneComponent : public UBoxComponent
{
	GENERATED_BODY()
public:
	UVBoRApproachZoneComponent();
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	void SampleHead(bool bTracked, const FVector& WorldHead, double Now);
	UFUNCTION(BlueprintPure, Category="접근") bool IsParticipantNear() const { return bNear; }
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="접근", meta=(Units="cm", ClampMin="0")) float DepartMarginCm = 30;
	UPROPERTY(BlueprintAssignable, Category="접근") FVBoRApproachEvent OnParticipantApproached;
	UPROPERTY(BlueprintAssignable, Category="접근") FVBoRApproachEvent OnParticipantDeparted;
	int32 Approaches = 0, Departures = 0;
	double LastApproachSeconds = 0, LastDepartureSeconds = 0;
private:
	bool bNear = false;
};

UCLASS(ClassGroup=(VBoR), meta=(BlueprintSpawnableComponent))
class VBOR_API UVBoRSeatZoneComponent : public UBoxComponent
{
	GENERATED_BODY()
public:
	UVBoRSeatZoneComponent();
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	void SampleHead(bool bTracked, const FVector& WorldHead, double DeltaSeconds);
	bool IsHeadInside() const { return bInside; }
	bool IsHeadTracked() const { return bWasTracked; }
	double DwellSeconds = 0;
	int32 TrackingPauses = 0;
private:
	bool bWasTracked = true;
	bool bInside = false;
};
