#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Interaction/Pointable/IsdkInteractionPointerEvent.h"
#include "VBoRSoundCue.h"
#include "VBoRVehiclePowerComponent.generated.h"

class UAudioComponent;

UENUM(BlueprintType)
enum class EVBoRPowerSource : uint8 { Direct, Participant, Operator, Restart };

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FVBoRPowerChanged, bool, bOn);

UCLASS(ClassGroup=(Vehicle), meta=(BlueprintSpawnableComponent))
class VBOR_API UVBoRVehiclePowerComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UVBoRVehiclePowerComponent();
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vehicle power|Sound") FVBoRSoundCue PowerOnCue;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vehicle power|Sound") FVBoRSoundCue PowerOffCue;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vehicle power|Sound", meta=(ToolTip="Cabin start sound. Empty = Power On Cue")) FVBoRSoundCue PowerOnInsideCue;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vehicle power|Sound", meta=(ToolTip="Idle sound while powered on")) FVBoRHeldSoundCue IdleCue;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vehicle power|Sound", meta=(ToolTip="Cabin idle sound. Empty = Idle Cue")) FVBoRHeldSoundCue IdleInsideCue;
	// Slot order = property order above (P5s D12)
#if WITH_DEV_AUTOMATION_TESTS
	const FVBoRSoundPlayback& GetSoundPlaybackForTest(int32 Slot) const { return SoundPlayback[Slot]; }
	UAudioComponent* GetSoundAudioForTest(int32 Slot) const { return SoundAudio[Slot]; }
	uint64 GetSoundFrameForTest() const { return SoundFrame; }
#endif

	UFUNCTION(BlueprintCallable, Category="Vehicle power") void TogglePower(EVBoRPowerSource Source = EVBoRPowerSource::Direct);
	UFUNCTION(BlueprintCallable, Category="Vehicle power") void SetPowerOn(bool bOn, EVBoRPowerSource Source = EVBoRPowerSource::Direct);
	UFUNCTION(BlueprintPure, Category="Vehicle power") bool IsPowerOn() const { return bPowerOn; }
	UPROPERTY(BlueprintAssignable, Category="Vehicle power") FVBoRPowerChanged OnPowerChanged;
	bool IsAvailable() const { return bAvailable; }
	// Find once at startup; caller caches (no per-frame scan, P5q D1/D5)
	static UVBoRVehiclePowerComponent* FindUnique(UWorld* World, int32* Count = nullptr);
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
private:
#if WITH_DEV_AUTOMATION_TESTS
	friend struct FP5sTestAccess;
#endif
	UPROPERTY() TArray<TObjectPtr<UAudioComponent>> SoundAudio;
	enum ESoundSlot { PowerOn, PowerOff, PowerOnInside, Idle, IdleInside, SoundSlotCount };
	FVBoRSoundPlayback SoundPlayback[SoundSlotCount];
	uint64 SoundFrame = 0;
	TWeakObjectPtr<class UVBoRCabinZoneComponent> CabinZone;
	ESoundSlot IdleSlot = Idle;
	void BeginIdle(bool bInside, bool bSkipDelay = false);
	void EndIdle();
	void UpdateSoundTick();

	bool bPowerOn = false, bAvailable = false;
	uint64 LastToggleFrame = MAX_uint64;
	UPROPERTY() TArray<TObjectPtr<class AVBoRPokeButton>> Buttons;
	UFUNCTION() void HandleStartButton(const FIsdkInteractionPointerEvent& Event);
};
