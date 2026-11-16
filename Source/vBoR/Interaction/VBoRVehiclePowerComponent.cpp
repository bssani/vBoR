#include "VBoRVehiclePowerComponent.h"
#include "Algo/AnyOf.h"
#include "Components/AudioComponent.h"

#include "VBoRPokeButton.h"
#include "VBoRParticipantZones.h"
#include "CoreGlobals.h"
#include "Components/SceneComponent.h"
#include "GameFramework/Actor.h"
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

UVBoRVehiclePowerComponent::UVBoRVehiclePowerComponent() { PrimaryComponentTick.bCanEverTick = true; PrimaryComponentTick.bStartWithTickEnabled = false; SoundAudio.SetNum(SoundSlotCount); }

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
		// Report first component only (avoid duplicate warnings from multiple components, P5q D1)
		if (!Components.IsEmpty() && Components[0] == this)
		{ UE_LOG(LogTemp, Warning, TEXT("Vehicle power: expected one component, found %d; power disabled"), Components.Num()); }
		return;
	}
	CabinZone = UVBoRCabinZoneComponent::FindFirst(GetWorld());
	for (TActorIterator<AVBoRPokeButton> It(GetWorld()); It; ++It)
	{
		if (It->ActorHasTag(TEXT("Car.StartButton")))
		{ Buttons.Add(*It); It->OnButtonPressed.AddUniqueDynamic(this, &UVBoRVehiclePowerComponent::HandleStartButton); }
	}
	if (Buttons.IsEmpty()) { UE_LOG(LogTemp, Warning, TEXT("Vehicle power: no Car.StartButton; operator power remains available")); }
}

void UVBoRVehiclePowerComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	for (int32 I = 0; I < SoundSlotCount; ++I) { SoundPlayback[I].Reset(SoundAudio[I], false); }
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
	// R with same off value also clears old delayed requests (P5s D11)
	if (Source == EVBoRPowerSource::Restart)
	{
		for (int32 I = 0; I < SoundSlotCount; ++I) { SoundPlayback[I].Reset(SoundAudio[I], true); }
		SetComponentTickEnabled(false);
	}
	if (!bAvailable || bPowerOn == bOn) { return; }
	bPowerOn = bOn;
	const TCHAR* Who = Source == EVBoRPowerSource::Participant ? TEXT("Participant") : Source == EVBoRPowerSource::Operator ? TEXT("Operator") : Source == EVBoRPowerSource::Restart ? TEXT("Restart") : TEXT("Direct");
	UE_LOG(LogTemp, Display, TEXT("Vehicle power: %s | %s"), bOn ? TEXT("on") : TEXT("off"), Who);
	if (Source != EVBoRPowerSource::Restart)
	{
		const bool bInside = bOn && CabinZone.IsValid() && CabinZone->IsHeadInside();
		const ESoundSlot Slot = bOn ? (bInside && PowerOnInsideCue.Sound ? PowerOnInside : PowerOn) : PowerOff;
		const auto& Cue = Slot == PowerOnInside ? PowerOnInsideCue : (bOn ? PowerOnCue : PowerOffCue);
		VBoREnsureCueAudio(SoundAudio[Slot], Cue, GetOwner(), GetOwner()->GetRootComponent());
		SoundPlayback[Slot].Queue(Cue, SoundAudio[Slot], SoundFrame);
		if (bOn) { BeginIdle(bInside); }
		else { EndIdle(); }
		UpdateSoundTick();
	}
	OnPowerChanged.Broadcast(bOn);
}

void UVBoRVehiclePowerComponent::BeginIdle(bool bInside, bool bSkipDelay)
{
	IdleSlot = bInside && IdleInsideCue.Sound ? IdleInside : Idle;
	FVBoRHeldSoundCue Cue = IdleSlot == IdleInside ? IdleInsideCue : IdleCue;
	if (bSkipDelay) { Cue.StartDelaySeconds = 0; }
	VBoREnsureCueAudio(SoundAudio[IdleSlot], Cue, GetOwner(), GetOwner()->GetRootComponent());
	SoundPlayback[IdleSlot].BeginHeld(Cue, SoundAudio[IdleSlot], SoundFrame);
}

void UVBoRVehiclePowerComponent::EndIdle()
{
	SoundPlayback[IdleSlot].EndHeld(IdleSlot == IdleInside ? IdleInsideCue : IdleCue, SoundAudio[IdleSlot]);
}

void UVBoRVehiclePowerComponent::UpdateSoundTick()
{
	const bool bCheckCabin = bPowerOn && CabinZone.IsValid() && IdleInsideCue.Sound;
	SetComponentTickEnabled(bCheckCabin || Algo::AnyOf(SoundPlayback, [](const auto& Slot) { return Slot.HasPending(); }));
}

void UVBoRVehiclePowerComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction); ++SoundFrame;
	SoundPlayback[PowerOn].Advance(PowerOnCue, SoundAudio[PowerOn], DeltaTime, SoundFrame);
	SoundPlayback[PowerOff].Advance(PowerOffCue, SoundAudio[PowerOff], DeltaTime, SoundFrame);
	SoundPlayback[PowerOnInside].Advance(PowerOnInsideCue, SoundAudio[PowerOnInside], DeltaTime, SoundFrame);
	const bool bInside = bPowerOn && IdleInsideCue.Sound && CabinZone.IsValid() && CabinZone->IsHeadInside();
	if (bPowerOn && (bInside ? IdleInside : Idle) != IdleSlot)
	{
		EndIdle();
		BeginIdle(bInside, true);
	}
	else
	{
		SoundPlayback[IdleSlot].Advance(IdleSlot == IdleInside ? IdleInsideCue : IdleCue, SoundAudio[IdleSlot], DeltaTime, SoundFrame);
	}
	UpdateSoundTick();
}
