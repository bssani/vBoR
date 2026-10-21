#include "VBoRVehiclePowerComponent.h"

#include "VBoRPokeButton.h"
#include "CoreGlobals.h"
#include "EngineUtils.h"
#include "UObject/UObjectIterator.h"

namespace
{
TArray<UVBoRVehiclePowerComponent*> PowerComponents(UWorld* World)
{
	TArray<UVBoRVehiclePowerComponent*> Result;
	for (TObjectIterator<UVBoRVehiclePowerComponent> It; It; ++It)
	{
		if (!It->IsTemplate() && It->GetWorld() == World && It->IsRegistered()) { Result.Add(*It); }
	}
	return Result;
}
}

UVBoRVehiclePowerComponent::UVBoRVehiclePowerComponent() { PrimaryComponentTick.bCanEverTick = false; }

UVBoRVehiclePowerComponent* UVBoRVehiclePowerComponent::FindUnique(UWorld* World, int32* Count)
{
	const auto Components = PowerComponents(World);
	if (Count) { *Count = Components.Num(); }
	return Components.Num() == 1 ? Components[0] : nullptr;
}

void UVBoRVehiclePowerComponent::BeginPlay()
{
	Super::BeginPlay();
	bPowerOn = false;
	const auto Components = PowerComponents(GetWorld());
	bAvailable = Components.Num() == 1;
	if (!bAvailable)
	{
		// 여러 컴포넌트가 같은 경고를 되풀이하지 않게 첫 것만 기록함 (P5q D1)
		if (!Components.IsEmpty() && Components[0] == this)
		{ UE_LOG(LogTemp, Warning, TEXT("Vehicle power: expected one component, found %d; power disabled"), Components.Num()); }
		return;
	}
	for (TActorIterator<AVBoRPokeButton> It(GetWorld()); It; ++It)
	{
		if (It->ActorHasTag(TEXT("Car.StartButton")))
		{ Buttons.Add(*It); It->OnButtonPressed.AddUniqueDynamic(this, &UVBoRVehiclePowerComponent::HandleStartButton); }
	}
	if (Buttons.IsEmpty()) { UE_LOG(LogTemp, Warning, TEXT("Vehicle power: no Car.StartButton; operator power remains available")); }
}

void UVBoRVehiclePowerComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	for (AVBoRPokeButton* Button : Buttons)
	{ if (IsValid(Button)) { Button->OnButtonPressed.RemoveDynamic(this, &UVBoRVehiclePowerComponent::HandleStartButton); } }
	Buttons.Reset(); bAvailable = false;
	Super::EndPlay(EndPlayReason);
}

void UVBoRVehiclePowerComponent::HandleStartButton(const FIsdkInteractionPointerEvent& Event)
{
	TogglePower(EVBoRPowerSource::Participant);
}

void UVBoRVehiclePowerComponent::TogglePower(EVBoRPowerSource Source)
{
	if (!bAvailable || LastToggleFrame == GFrameCounter) { return; }
	LastToggleFrame = GFrameCounter;
	SetPowerOn(!bPowerOn, Source);
}

void UVBoRVehiclePowerComponent::SetPowerOn(bool bOn, EVBoRPowerSource Source)
{
	if (!bAvailable || bPowerOn == bOn) { return; }
	bPowerOn = bOn;
	const TCHAR* Who = Source == EVBoRPowerSource::Participant ? TEXT("Participant") : Source == EVBoRPowerSource::Operator ? TEXT("Operator") : Source == EVBoRPowerSource::Restart ? TEXT("Restart") : TEXT("Direct");
	UE_LOG(LogTemp, Display, TEXT("Vehicle power: %s | %s"), bOn ? TEXT("on") : TEXT("off"), Who);
	OnPowerChanged.Broadcast(bOn);
}
