#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "P0ProbeMeasurement.h"
#include "P0ProbeDialMeasurement.h"
#include "P0ProbePawn.generated.h"

class UCameraComponent;
class UMotionControllerComponent;
class UOculusXRHandComponent;
class UTextRenderComponent;

DECLARE_LOG_CATEGORY_EXTERN(LogP0Probe, Log, All);

UCLASS()
class VBOR_API AP0ProbePawn : public APawn
{
	GENERATED_BODY()

public:
	AP0ProbePawn();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "P0 Probe")
	FTransform PanelWorldTransform = FTransform(FRotator(0, 180, 0), FVector(180, -100, 190));

	// 0 이면 BeginPlay 의 Pawn 정면 방향 사용. 월드 방향이며 단위 없음 (P0-a §2)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "P0 Probe")
	FVector DialWorldAxis = FVector::ZeroVector;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "P0 Probe")
	TObjectPtr<USceneComponent> VROrigin;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "P0 Probe")
	TObjectPtr<UCameraComponent> Camera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "P0 Probe")
	TObjectPtr<UMotionControllerComponent> LeftMotion;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "P0 Probe")
	TObjectPtr<UMotionControllerComponent> RightMotion;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "P0 Probe")
	TObjectPtr<UOculusXRHandComponent> LeftHand;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "P0 Probe")
	TObjectPtr<UOculusXRHandComponent> RightHand;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "P0 Probe")
	TObjectPtr<UTextRenderComponent> Panel;

private:
	FP0ProbeMeasurement Measurements[2];
	FP0ProbeDialMeasurement DialMeasurements[2];
	FVector IndexPositions[2] = {};
	FVector ThumbPositions[2] = {};
	bool bBonesAvailable[2] = {};
	bool bPinched[2][5] = {};
	float PinchStrengths[2][5] = {};
	FName IndexBone;
	FName ThumbBone;
	FName WristBone;
	FString XRSystemName;
	FString LastKeyboardKey = TEXT("none");
	TArray<FString> RecentEvents;
	double ProbeStartedAt = 0.0;
	double LastContinuousLogAt = -1.0;

	double ProbeTime() const;
	void RecordEvent(const FString& Event);
	void OnKeyPressed(FKey Key);
	void OnKeyReleased(FKey Key);
	void OnKeyboardPressed(FKey Key);
	FString FormatHand(int32 HandIndex, double Time) const;
};
