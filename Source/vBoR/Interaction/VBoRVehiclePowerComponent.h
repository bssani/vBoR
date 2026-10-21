#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Interaction/Pointable/IsdkInteractionPointerEvent.h"
#include "VBoRVehiclePowerComponent.generated.h"

UENUM(BlueprintType)
enum class EVBoRPowerSource : uint8 { Direct, Participant, Operator, Restart };

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FVBoRPowerChanged, bool, bOn);

UCLASS(ClassGroup=(Vehicle), meta=(BlueprintSpawnableComponent))
class VBOR_API UVBoRVehiclePowerComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UVBoRVehiclePowerComponent();
	UFUNCTION(BlueprintCallable, Category="Vehicle power") void TogglePower(EVBoRPowerSource Source = EVBoRPowerSource::Direct);
	UFUNCTION(BlueprintCallable, Category="Vehicle power") void SetPowerOn(bool bOn, EVBoRPowerSource Source = EVBoRPowerSource::Direct);
	UFUNCTION(BlueprintPure, Category="Vehicle power") bool IsPowerOn() const { return bPowerOn; }
	UPROPERTY(BlueprintAssignable, Category="Vehicle power") FVBoRPowerChanged OnPowerChanged;
	bool IsAvailable() const { return bAvailable; }
	// 시작 때만 찾고 호출자가 보관함 (frame 마다 순회하지 않게, P5q D1·D5)
	static UVBoRVehiclePowerComponent* FindUnique(UWorld* World, int32* Count = nullptr);
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
private:
	bool bPowerOn = false, bAvailable = false;
	uint64 LastToggleFrame = MAX_uint64;
	UPROPERTY() TArray<TObjectPtr<class AVBoRPokeButton>> Buttons;
	UFUNCTION() void HandleStartButton(const FIsdkInteractionPointerEvent& Event);
};
