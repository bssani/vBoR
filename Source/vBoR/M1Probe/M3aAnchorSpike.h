#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "OculusXRAnchorTypes.h"
#include "M3aAnchorSpike.generated.h"

class ATargetPoint;
class UOculusXRAnchorComponent;

UCLASS()
class AM3aAnchorSpike : public AActor
{
	GENERATED_BODY()

public:
	AM3aAnchorSpike();
	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void BeginDiscovery();
	void OnDiscovered(const TArray<FOculusXRAnchorsDiscoverResult>& Results);
	void OnDiscoveryComplete(EOculusXRAnchorResult::Type Result);
	void StartNewAnchorSample();
	void StartCheckPointSample();
	void StartSample(bool bCreateAnchor);
	void CreateAnchor(const FTransform& PawnWorld, const FVector& StageTip);
	void OnCreated(EOculusXRAnchorResult::Type Result, UOculusXRAnchorComponent* Anchor);
	void OnSaved(EOculusXRAnchorResult::Type Result, const TArray<UOculusXRAnchorComponent*>& Anchors);
	void UseAnchor(UOculusXRAnchorComponent* Anchor);
	void ClearActiveAnchor();
	bool ReadAnchorStage(const FTransform& PawnWorld, FTransform& OutStage) const;

	TWeakObjectPtr<UOculusXRAnchorComponent> ActiveAnchor;
	TWeakObjectPtr<ATargetPoint> PendingAnchorActor;
	FOculusXRUUID SavedUuid;
	FTransform SavedStage = FTransform::Identity;
	FTransform NewAnchorStage = FTransform::Identity;
	double DiscoveryStarted = 0.0;
	double NextSummary = 0.0;
	double SampleEnds = 0.0;
	FVector TipSum = FVector::ZeroVector;
	int32 TipSamples = 0;
	bool bDiscovering = false;
	bool bSampling = false;
	bool bCreateFromSample = false;
};
