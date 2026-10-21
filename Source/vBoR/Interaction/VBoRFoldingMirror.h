#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VBoRSpectatorPart.h"
#include "VBoRSoundCue.h"
#include "VBoRFoldingMirror.generated.h"

class UAudioComponent;
class UStaticMeshComponent;
class UVBoRVehiclePowerComponent;

UENUM(BlueprintType)
enum class EVBoRFoldingMirrorState : uint8 { Folded, Unfolding, Unfolded, Folding };

UCLASS(Blueprintable)
class VBOR_API AVBoRFoldingMirror : public AActor, public IVBoRSpectatorPart
{
	GENERATED_BODY()
public:
	AVBoRFoldingMirror();
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mirror folding|Sound") FVBoRHeldSoundCue UnfoldMotorCue;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mirror folding|Sound") FVBoRHeldSoundCue FoldMotorCue;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mirror folding|Sound") FVBoRSoundCue UnfoldedCue;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mirror folding|Sound") FVBoRSoundCue FoldedCue;
	// 칸 순서는 위 프로퍼티 순서와 같음 (P5s D12)
#if WITH_DEV_AUTOMATION_TESTS
	const FVBoRSoundPlayback& GetSoundPlaybackForTest(int32 Slot) const { return SoundPlayback[Slot]; }
	UAudioComponent* GetSoundAudioForTest(int32 Slot) const { return SoundAudio[Slot]; }
	uint64 GetSoundFrameForTest() const { return SoundFrame; }
#endif

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void Tick(float DeltaSeconds) override;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spectator") FText SpectatorName;
	virtual FText GetSpectatorName() const override { return SpectatorName; }
	virtual bool IsSpectatorInteracting() const override { return false; }
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Mirror folding") TObjectPtr<USceneComponent> HingePivot;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Mirror folding") TObjectPtr<UStaticMeshComponent> MirrorMesh;
	// 쉬는 경첩의 local +Z 둘레로 회전함 (기울어진 CAD 경첩도 편집 가능, P5q D5)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mirror folding", meta=(Units="cm")) FVector HingeLocationCm = FVector(40,-90,100);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mirror folding") FRotator HingeRotation = FRotator::ZeroRotator;
	// 펼친 CAD 의 차 원점 자리를 보존함 (P5q D5, U-09)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mirror folding") FTransform MeshRestFromRoot = FTransform(FQuat::Identity, FVector(40,-100,100), FVector(.16,.24,.1));
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mirror folding", meta=(Units="deg", ClampMin="-170", ClampMax="170")) float FoldAngleDeg = -65;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mirror folding", meta=(Units="deg/s", ClampMin="0.1")) float SpeedDegPerSecond = 30;
	UFUNCTION(BlueprintPure, Category="Mirror folding") EVBoRFoldingMirrorState GetState() const { return State; }
	UFUNCTION(BlueprintPure, Category="Mirror folding") float GetAngleDeg() const { return AngleDeg; }
	UFUNCTION(BlueprintCallable, Category="Mirror folding") void ResetMirror();
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
private:
#if WITH_DEV_AUTOMATION_TESTS
	friend struct FP5sTestAccess;
#endif
	UPROPERTY() TArray<TObjectPtr<UAudioComponent>> SoundAudio;
	FVBoRSoundPlayback SoundPlayback[4];
	uint64 SoundFrame = 0;
	void AdvanceSounds(double Seconds);

	UPROPERTY() TObjectPtr<UVBoRVehiclePowerComponent> Power;
	EVBoRFoldingMirrorState State = EVBoRFoldingMirrorState::Unfolded;
	float AngleDeg = 0;
	UFUNCTION() void HandlePowerChanged(bool bOn);
	void ApplyAngle();
};
