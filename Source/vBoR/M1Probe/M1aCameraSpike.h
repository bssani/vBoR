#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "M1aCameraSpike.generated.h"

class UMRUKPassthroughCameraAccess;
class UMRUKPassthroughCameraAccessTexture;
class UMRUKSubsystem;

UCLASS()
class VBOR_API AM1aCameraSpike : public AActor
{
	GENERATED_BODY()

public:
	AM1aCameraSpike();
	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditAnywhere, Category = "Camera Spike", meta = (ClampMin = "1.0"))
	double MarkerSizeMm = 60.0;

private:
	void ToggleCamera();
	void StopCamera();
	void TickCamera(double Now);
	void StartTipSample();
	void TickTipSample(double Now);

	UPROPERTY(Transient)
	TObjectPtr<UMRUKPassthroughCameraAccess> Camera;

	UPROPERTY(Transient)
	TObjectPtr<UMRUKPassthroughCameraAccessTexture> CameraTexture;

	TWeakObjectPtr<UMRUKSubsystem> WorldLockSubsystem;
	bool bPreviousWorldLock = false;

	FDateTime LastTimestamp;
	double NextSummary = 0.0;
	int32 FramesRead = 0;
	int32 FramesWithMarkers = 0;
	bool bSawNonBlackFrame = false;
	FVector CameraStageCm = FVector::ZeroVector;
	bool bHasMarker = false;
	int32 MarkerId = INDEX_NONE;
	FVector MarkerStageCm = FVector::ZeroVector;
	double ReprojectionPx = 0.0;

	bool bSamplingTip = false;
	double TipSampleEnd = 0.0;
	FVector TipSumStageCm = FVector::ZeroVector;
	int32 TipSamples = 0;
};
