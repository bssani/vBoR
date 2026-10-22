#include "VBoRVehiclePowerComponent.h"
#include "Components/AudioComponent.h"

#include "VBoRPokeButton.h"
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

UVBoRVehiclePowerComponent::UVBoRVehiclePowerComponent() { PrimaryComponentTick.bCanEverTick = true; PrimaryComponentTick.bStartWithTickEnabled = false; SoundAudio.SetNum(2); }

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
	for (TActorIterator<AVBoRPokeButton> It(GetWorld()); It; ++It)
	{
		if (It->ActorHasTag(TEXT("Car.StartButton")))
		{ Buttons.Add(*It); It->OnButtonPressed.AddUniqueDynamic(this, &UVBoRVehiclePowerComponent::HandleStartButton); }
	}
	if (Buttons.IsEmpty()) { UE_LOG(LogTemp, Warning, TEXT("Vehicle power: no Car.StartButton; operator power remains available")); }
}

void UVBoRVehiclePowerComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	for (int32 I = 0; I < 2; ++I) { SoundPlayback[I].Reset(SoundAudio[I], false); }
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
		for (int32 I = 0; I < 2; ++I) { SoundPlayback[I].Reset(SoundAudio[I], true); }
		SetComponentTickEnabled(false);
	}
	if (!bAvailable || bPowerOn == bOn) { return; }
	bPowerOn = bOn;
	const TCHAR* Who = Source == EVBoRPowerSource::Participant ? TEXT("Participant") : Source == EVBoRPowerSource::Operator ? TEXT("Operator") : Source == EVBoRPowerSource::Restart ? TEXT("Restart") : TEXT("Direct");
	UE_LOG(LogTemp, Display, TEXT("Vehicle power: %s | %s"), bOn ? TEXT("on") : TEXT("off"), Who);
	if (Source != EVBoRPowerSource::Restart)
	{
		EnsureSoundAudio(bOn ? 0 : 1);
		SoundPlayback[bOn ? 0 : 1].Queue(bOn ? PowerOnCue : PowerOffCue, SoundAudio[bOn ? 0 : 1], SoundFrame);
		SetComponentTickEnabled(SoundPlayback[0].HasPending() || SoundPlayback[1].HasPending());
	}
	OnPowerChanged.Broadcast(bOn);
}

void UVBoRVehiclePowerComponent::EnsureSoundAudio(int32 Slot)
{
	// Create on first request only. Configured sound adds no body components at Play start (preserve P4i E2 Core comparison, P5s R1-1)
	if (SoundAudio[Slot] || !(Slot == 0 ? PowerOnCue.Sound : PowerOffCue.Sound)) { return; }
	SoundAudio[Slot] = NewObject<UAudioComponent>(GetOwner());
	SoundAudio[Slot]->bAutoActivate = false; SoundAudio[Slot]->SetupAttachment(GetOwner()->GetRootComponent());
	GetOwner()->AddInstanceComponent(SoundAudio[Slot]); SoundAudio[Slot]->RegisterComponent();
}

void UVBoRVehiclePowerComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction); ++SoundFrame;
	SoundPlayback[0].Advance(PowerOnCue, SoundAudio[0], DeltaTime, SoundFrame);
	SoundPlayback[1].Advance(PowerOffCue, SoundAudio[1], DeltaTime, SoundFrame);
	SetComponentTickEnabled(SoundPlayback[0].HasPending() || SoundPlayback[1].HasPending());
}
