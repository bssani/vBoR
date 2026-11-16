#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "P5fTestFixture.h"
#include "../Interaction/VBoRCarryObject.h"
#include "../Interaction/VBoRFoldingSeat.h"
#include "../Interaction/VBoRHingedPanel.h"
#include "../Interaction/VBoRKeyFob.h"
#include "../Interaction/VBoRKnob.h"
#include "../Interaction/VBoRParticipantZones.h"
#include "../Interaction/VBoRPokeButton.h"
#include "../Interaction/VBoRPoweredPanel.h"
#include "../Interaction/VBoRScenarioRunner.h"
#include "../Interaction/VBoRVehiclePowerComponent.h"
#include "Components/AudioComponent.h"
#include "CoreGlobals.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "Interaction/IsdkGrabberComponent.h"
#include "Interaction/IsdkPokeInteractable.h"
#include "IsdkRuntimeSettings.h"
#include "Sound/SoundBase.h"
#include <initializer_list>

struct FR11TestAccess
{
	static void Latch(AVBoRHingedPanel* Door)
	{
		const int32 Before = Door->Motion.Metrics.Latches++;
		Door->ApplyState(Door->Motion.AngleDeg, Door->Motion.Metrics.Unlocks, Before);
	}

	static void Turn(AVBoRKnob* Knob, std::initializer_list<double> Angles)
	{
		Knob->Motion.BeginGrab(0, FQuat::Identity);
		for (double Angle : Angles)
		{
			Knob->Motion.AdvanceGrab(true, FQuat(FVector::UpVector, FMath::DegreesToRadians(Angle)), Knob->SoundStepDeg);
		}
		Knob->Motion.ReleaseGrab();
	}

	static void HoldKey(AVBoRKeyFob* Key) { Key->HoldingHand = 0; }
	static bool BeginKnob(AVBoRKnob* Knob) { return Knob->BeginHold(0, nullptr); }
};

namespace
{
constexpr auto R11Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;
using EGateSound = EVBoRPoweredSound;

struct FR11World
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	uint64 Frame = GFrameCounter;
	AActor* Car = Spawn<AActor>();

	FR11World()
	{
		Car->SetRootComponent(NewObject<USceneComponent>(Car));
		Car->GetRootComponent()->RegisterComponent();
	}

	~FR11World()
	{
		World->EndPlay(EEndPlayReason::Quit);
		World->DestroyWorld(false);
		GFrameCounter = Frame;
	}

	template<class T> T* Spawn() { return World->SpawnActor<T>(); }

	template<class T> T* AddCarComponent()
	{
		auto* Component = NewObject<T>(Car);
		Car->AddInstanceComponent(Component);
		Component->RegisterComponent();
		return Component;
	}

	UVBoRVehiclePowerComponent* AddPower()
	{
		Spawn<AVBoRPokeButton>()->Tags.Add(TEXT("Car.StartButton"));
		return AddCarComponent<UVBoRVehiclePowerComponent>();
	}

	void Play()
	{
		World->InitializeActorsForPlay(FURL());
		World->GetWorldSettings()->NotifyBeginPlay();
	}

	static void Poke(AVBoRPokeButton* Button, UIsdkGrabberComponent* Hand = nullptr)
	{
		FIsdkInteractionPointerEvent Event;
		Event.Type = EIsdkPointerEventType::Select;
		Event.Interactor = Hand;
		Event.Interactable = Button->Poke.Get();
		Button->Poke->GetInteractionPointerEventDelegate().Broadcast(Event);
	}

	void PressKey(AVBoRKeyFob* Key)
	{
		auto* Hand = NewObject<UIsdkGrabberComponent>(Car);
		Hand->InteractorTags.AddTag(IsdkGameplayTags::TAG_Isdk_Type_Device_Right);
		FR11TestAccess::HoldKey(Key);
		Key->Tick(.01f);
		Poke(Key->UnlockButton, Hand);
		Poke(Key->LockButton, Hand);
	}
};

constexpr float FrameSeconds = 1.0f / 60;

template<class T, class State>
float TickUntil(T* Part, State Target)
{
	int32 Frames = 0;
	for (; Part->GetState() != Target && Frames < 1200; ++Frames) { Part->Tick(FrameSeconds); }
	return Frames * FrameSeconds;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FR11Defaults, "vBoR.R11.E1Defaults", R11Flags)
bool FR11Defaults::RunTest(const FString&)
{
	FR11World W;
	auto* Power = W.AddPower();
	auto* Door = W.Spawn<AVBoRHingedPanel>();
	Door->bUseLatch = true;
	auto* Gate = W.Spawn<AVBoRPoweredPanel>();
	auto* Seat = W.Spawn<AVBoRFoldingSeat>();
	auto* Knob = W.Spawn<AVBoRKnob>();
	auto* Key = W.Spawn<AVBoRKeyFob>();
	W.Play();
	const FVBoRSoundCue* NewCues[] = {&Door->UnlockedInsideCue, &Door->LatchedInsideCue, &Power->PowerOnInsideCue,
		&Power->IdleCue, &Power->IdleInsideCue, &Gate->OpeningCue, &Gate->ClosingCue,
		&Seat->FoldingCue, &Seat->UnfoldingCue, &Key->UnlockCarCue, &Key->LockCarCue};
	for (const auto* Cue : NewCues) { TestNull(TEXT("new cue silent"), Cue->Sound.Get()); }
	for (float Value : {Gate->OpenSeconds, Gate->CloseSeconds, Seat->FoldSeconds, Seat->UnfoldSeconds, Knob->SoundStepDeg})
	{ TestEqual(TEXT("new numeric default"), Value, 0.f); }
	Door->Unlock();
	FR11TestAccess::Latch(Door);
	Power->SetPowerOn(true);
	Power->SetPowerOn(false);
	Gate->RequestOpen();
	TestTrue(TEXT("default gate speed"), FMath::IsNearlyEqual(TickUntil(Gate, EVBoRPoweredState::Open),
		Gate->DelaySeconds + Gate->OpenAngleDeg / Gate->SpeedDegPerSecond, FrameSeconds + .001f));
	Gate->RequestClose();
	TestTrue(TEXT("default close speed"), FMath::IsNearlyEqual(TickUntil(Gate, EVBoRPoweredState::Closed),
		Gate->DelaySeconds + Gate->OpenAngleDeg / Gate->SpeedDegPerSecond, FrameSeconds + .001f));
	Seat->Fold();
	TestTrue(TEXT("default seat speed"), FMath::IsNearlyEqual(TickUntil(Seat, EVBoRFoldingSeatState::Folded),
		Seat->FoldAngleDeg / Seat->SpeedDegPerSecond, FrameSeconds));
	Seat->Unfold();
	TestTrue(TEXT("default unfold speed"), FMath::IsNearlyEqual(TickUntil(Seat, EVBoRFoldingSeatState::Standing),
		Seat->FoldAngleDeg / Seat->SpeedDegPerSecond, FrameSeconds));
	FR11TestAccess::Turn(Knob, {15});
	Knob->Tick(.01f);
	W.PressKey(Key);
	TestEqual(TEXT("silent door"), Door->GetSoundPlaybackForTest(2).Requests + Door->GetSoundPlaybackForTest(3).Requests, 0);
	TestEqual(TEXT("silent power"), Power->GetSoundPlaybackForTest(2).Requests + Power->GetSoundPlaybackForTest(3).Requests +
		Power->GetSoundPlaybackForTest(4).Requests, 0);
	TestEqual(TEXT("silent gate motion"), Gate->GetSoundPlaybackForTest(EGateSound::Opening).Requests +
		Gate->GetSoundPlaybackForTest(EGateSound::Closing).Requests, 0);
	TestEqual(TEXT("silent seat motion"), Seat->GetSoundPlaybackForTest(0).Requests + Seat->GetSoundPlaybackForTest(1).Requests, 0);
	TestNull(TEXT("gate lazily creates audio"), Gate->GetSoundAudioForTest(EGateSound::Opening));
	TestNull(TEXT("seat lazily creates audio"), Seat->GetSoundAudioForTest(0));
	TestEqual(TEXT("silent car sounds"), Key->GetSoundPlaybackForTest(0).Requests + Key->GetSoundPlaybackForTest(1).Requests, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FR11Cabin, "vBoR.R11.E2Cabin", R11Flags)
bool FR11Cabin::RunTest(const FString&)
{
	FR11World W;
	TestNull(TEXT("no box"), UVBoRCabinZoneComponent::FindFirst(W.World));
	auto* Zone = W.AddCarComponent<UVBoRCabinZoneComponent>();
	Zone->SetBoxExtent(FVector(35, 40, 45));
	Zone->SetWorldLocation(FVector(100, 20, 30));
	Zone->SampleHead(true, FVector(100, 20, 30));
	TestTrue(TEXT("inside"), Zone->IsHeadInside());
	Zone->SampleHead(false, FVector::ZeroVector);
	TestTrue(TEXT("tracking loss retains inside"), Zone->IsHeadInside());
	Zone->SampleHead(true, FVector(100, 20, 90));
	TestFalse(TEXT("outside in height"), Zone->IsHeadInside());
	auto* Second = W.AddCarComponent<UVBoRCabinZoneComponent>();
	Second->SampleHead(true, FVector::ZeroVector);
	int32 Count = 0;
	auto* First = UVBoRCabinZoneComponent::FindFirst(W.World, &Count);
	TestTrue(TEXT("first box wins"), First == Zone);
	TestEqual(TEXT("two authored boxes"), Count, 2);
	auto* Runner = W.Spawn<AVBoRScenarioRunner>();
	Runner->CheckSetup();
	TestTrue(TEXT("duplicate cabin warning"), Runner->SetupReport.Contains(TEXT("Cabin zone: found 2")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FR11Door, "vBoR.R11.E3Door", R11Flags)
bool FR11Door::RunTest(const FString&)
{
	FR11World W;
	auto* Zone = W.AddCarComponent<UVBoRCabinZoneComponent>();
	auto* Door = W.Spawn<AVBoRHingedPanel>();
	Door->bUseLatch = true;
	Door->UnlockedInsideCue.Sound = Door->UnlockedCue.Sound;
	Door->UnlockedInsideCue.StartDelaySeconds = .25f;
	Door->LatchedInsideCue.Sound = Door->UnlockedCue.Sound;
	W.Play();
	Zone->SampleHead(true, FVector::ZeroVector);
	TestTrue(TEXT("inside unlock accepted"), Door->Unlock());
	FR11TestAccess::Latch(Door);
	TestEqual(TEXT("inside latch cue"), Door->GetSoundPlaybackForTest(3).Requests, 1);
	Zone->SampleHead(true, FVector(1000));
	Door->Tick(.3f);
	TestEqual(TEXT("queued inside unlock survives crossing"), Door->GetSoundPlaybackForTest(2).Requests, 1);
	TestEqual(TEXT("queued unlock stays off exterior slot"), Door->GetSoundPlaybackForTest(0).Requests, 0);
	Door->ResetDoor();
	Door->Unlock();
	FR11TestAccess::Latch(Door);
	TestEqual(TEXT("exterior unlock"), Door->GetSoundPlaybackForTest(0).Requests, 1);
	TestEqual(TEXT("exterior latch"), Door->GetSoundPlaybackForTest(1).Requests, 1);
	Door->ResetDoor();
	Door->UnlockedInsideCue.Sound = nullptr;
	Door->LatchedInsideCue.Sound = nullptr;
	Zone->SampleHead(true, FVector::ZeroVector);
	Door->Unlock();
	FR11TestAccess::Latch(Door);
	TestEqual(TEXT("empty inside unlock falls back"), Door->GetSoundPlaybackForTest(0).Requests, 1);
	TestEqual(TEXT("empty inside latch falls back"), Door->GetSoundPlaybackForTest(1).Requests, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FR11Idle, "vBoR.R11.E4Idle", R11Flags)
bool FR11Idle::RunTest(const FString&)
{
	FR11World W;
	auto* Zone = W.AddCarComponent<UVBoRCabinZoneComponent>();
	auto* Power = W.AddPower();
	auto* Button = W.Spawn<AVBoRPokeButton>();
	Button->Tags.Add(TEXT("Car.StartButton"));
	Power->PowerOnInsideCue.Sound = Power->PowerOffCue.Sound = Power->IdleCue.Sound = Power->IdleInsideCue.Sound = Button->PressedCue.Sound;
	Power->IdleCue.StartDelaySeconds = Power->IdleInsideCue.StartDelaySeconds = .5f;
	Power->IdleCue.FadeOutSeconds = Power->IdleInsideCue.FadeOutSeconds = .25f;
	W.Play();
	Zone->SampleHead(true, FVector::ZeroVector);
	FR11World::Poke(Button);
	Power->TogglePower(EVBoRPowerSource::Operator);
	TestTrue(TEXT("same-frame input changes state once"), Power->IsPowerOn());
	TestEqual(TEXT("one inside start"), Power->GetSoundPlaybackForTest(2).Requests, 1);
	TestEqual(TEXT("idle waits for start delay"), Power->GetSoundPlaybackForTest(4).Requests, 0);
	Power->TickComponent(.6f, LEVELTICK_All, nullptr);
	TestEqual(TEXT("inside idle starts"), Power->GetSoundPlaybackForTest(4).Requests, 1);
	Zone->SampleHead(true, FVector(1000));
	Power->TickComponent(.01f, LEVELTICK_All, nullptr);
	TestEqual(TEXT("inside idle fades"), Power->GetSoundPlaybackForTest(4).LastFadeOutSeconds, .25f);
	TestEqual(TEXT("outside idle skips startup delay"), Power->GetSoundPlaybackForTest(3).Requests, 1);
	Zone->SampleHead(true, FVector::ZeroVector);
	Power->TickComponent(.01f, LEVELTICK_All, nullptr);
	TestEqual(TEXT("return inside skips startup delay"), Power->GetSoundPlaybackForTest(4).Requests, 2);
	Power->SetPowerOn(false);
	TestEqual(TEXT("off fades inside idle"), Power->GetSoundPlaybackForTest(4).LastFadeOutSeconds, .25f);
	TestEqual(TEXT("off cue plays"), Power->GetSoundPlaybackForTest(1).Requests, 1);
	Power->IdleInsideCue.Sound = nullptr;
	Power->SetPowerOn(true);
	Power->TickComponent(.6f, LEVELTICK_All, nullptr);
	const int32 Stops = Power->GetSoundPlaybackForTest(3).StopRequests;
	Zone->SampleHead(true, FVector(1000));
	Power->TickComponent(.01f, LEVELTICK_All, nullptr);
	TestEqual(TEXT("fallback stays on same idle slot"), Power->GetSoundPlaybackForTest(3).StopRequests, Stops);
	Power->SetPowerOn(false, EVBoRPowerSource::Restart);
	TestEqual(TEXT("restart clears off cue"), Power->GetSoundPlaybackForTest(1).Requests, 0);
	TestFalse(TEXT("restart cancels idle"), Power->GetSoundPlaybackForTest(3).HasPending());
	TestEqual(TEXT("restart stops without fade"), Power->GetSoundPlaybackForTest(3).LastFadeOutSeconds, 0.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FR11Knob, "vBoR.R11.E5Knob", R11Flags)
bool FR11Knob::RunTest(const FString&)
{
	FR11World W;
	auto* Knob = W.Spawn<AVBoRKnob>();
	auto* Default = W.Spawn<AVBoRKnob>();
	auto* Tracked = W.Spawn<AP5fTrackedKnob>();
	Tracked->OnKnobStep.AddDynamic(Tracked, &AP5fTrackedKnob::RecordStep);
	Tracked->SoundStepDeg = 5;
	Knob->StepDeg = Default->StepDeg = 15;
	Knob->SoundStepDeg = 5;
	W.Play();
	FR11TestAccess::Turn(Knob, {15, 30, 45});
	FR11TestAccess::Turn(Default, {15, 30, 45});
	Knob->Tick(.01f);
	Default->Tick(.01f);
	TestEqual(TEXT("nine sound steps for 45 degrees"), Knob->GetSoundPlaybackForTest(0).Requests, 9);
	TestEqual(TEXT("zero keeps detent playback"), Default->GetSoundPlaybackForTest(0).Requests, 3);
	TestEqual(TEXT("detent rotation unchanged"), Knob->GetAngleDeg(), Default->GetAngleDeg());
	TestTrue(TEXT("synthetic hand grabs knob"), FR11TestAccess::BeginKnob(Tracked));
	for (double Angle : {15.0, 30.0, 45.0})
	{
		Tracked->Palm.SetRotation(FQuat(FVector::UpVector, FMath::DegreesToRadians(Angle)));
		Tracked->Tick(.01f);
	}
	TestEqual(TEXT("three detent events for 45 degrees"), Tracked->Steps.Num(), 3);
	TestEqual(TEXT("nine sound calls through tracked path"), Tracked->SoundCount, 9);
	Tracked->bTracked = false;
	Tracked->Tick(.01f);
	Tracked->bTracked = true;
	Tracked->Palm.SetRotation(FQuat(FVector::UpVector, FMath::DegreesToRadians(60.0)));
	Tracked->Tick(.01f);
	TestEqual(TEXT("tracking pause and recovery emit no sound"), Tracked->SoundCount, 9);
	FR11TestAccess::Turn(Knob, {60, 50});
	Knob->Tick(.01f);
	TestEqual(TEXT("jump ignored and reverse motion counted"), Knob->GetSoundPlaybackForTest(0).Requests, 11);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FR11Gate, "vBoR.R11.E6Gate", R11Flags)
bool FR11Gate::RunTest(const FString&)
{
	FR11World W;
	auto* Gate = W.Spawn<AVBoRPoweredPanel>();
	Gate->OpeningCue.Sound = Gate->ClosingCue.Sound = Gate->WarningCue.Sound;
	Gate->ClosingCue.FadeOutSeconds = .2f;
	Gate->OpenSeconds = 6.773438f;
	Gate->CloseSeconds = 8.675333f;
	W.Play();
	Gate->RequestOpen();
	Gate->Tick(.25f);
	TestEqual(TEXT("no directional cue in wait"), Gate->GetSoundPlaybackForTest(EGateSound::Opening).Requests, 0);
	TestTrue(TEXT("recorded opening time"), FMath::IsNearlyEqual(.25f + TickUntil(Gate, EVBoRPoweredState::Open),
		Gate->DelaySeconds + Gate->OpenSeconds, FrameSeconds + .001f));
	Gate->RequestClose();
	TestTrue(TEXT("independent closing time"), FMath::IsNearlyEqual(TickUntil(Gate, EVBoRPoweredState::Closed),
		Gate->DelaySeconds + Gate->CloseSeconds, FrameSeconds + .001f));
	Gate->RequestOpen();
	TickUntil(Gate, EVBoRPoweredState::Open);
	const FTransform Pose(FVector(-25, 15, 110));
	auto* Box = W.World->SpawnActorDeferred<AVBoRCarryObject>(AVBoRCarryObject::StaticClass(), Pose, nullptr, nullptr,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	Box->BoxSizeCm = FVector(20);
	Box->FinishSpawning(Pose);
	Gate->RequestClose();
	TickUntil(Gate, EVBoRPoweredState::Opening);
	const auto& Opening = Gate->GetSoundPlaybackForTest(EGateSound::Opening);
	TestEqual(TEXT("reversal starts opening cue"), Opening.Requests, 3);
	if (!Opening.RequestFrames.IsEmpty())
	{ TestEqual(TEXT("opening cue in obstruction frame"), Opening.RequestFrames.Last(), Gate->GetSoundFrameForTest()); }
	TestEqual(TEXT("reversal fades closing cue"), Gate->GetSoundPlaybackForTest(EGateSound::Closing).LastFadeOutSeconds, .2f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FR11Seat, "vBoR.R11.E7Seat", R11Flags)
bool FR11Seat::RunTest(const FString&)
{
	FR11World W;
	auto* Seat = W.Spawn<AVBoRFoldingSeat>();
	auto* Gate = W.Spawn<AVBoRPoweredPanel>();
	Seat->FoldingCue.Sound = Seat->UnfoldingCue.Sound = Gate->WarningCue.Sound;
	Seat->FoldingCue.FadeOutSeconds = Seat->UnfoldingCue.FadeOutSeconds = .1f;
	Seat->FoldSeconds = 3.4f;
	Seat->UnfoldSeconds = 3.f;
	W.Play();
	Seat->Fold();
	Seat->Tick(FrameSeconds);
	TestEqual(TEXT("fold cue starts on motion"), Seat->GetSoundPlaybackForTest(0).Requests, 1);
	TestFalse(TEXT("opposite command rejected"), Seat->Unfold());
	TestTrue(TEXT("fold duration"), FMath::IsNearlyEqual(FrameSeconds + TickUntil(Seat, EVBoRFoldingSeatState::Folded), 3.4f, FrameSeconds + .001f));
	TestEqual(TEXT("fold fades at arrival"), Seat->GetSoundPlaybackForTest(0).LastFadeOutSeconds, .1f);
	Seat->Unfold();
	TestTrue(TEXT("unfold duration"), FMath::IsNearlyEqual(TickUntil(Seat, EVBoRFoldingSeatState::Standing), 3.f, FrameSeconds + .001f));
	TestEqual(TEXT("unfold cue starts"), Seat->GetSoundPlaybackForTest(1).Requests, 1);
	TestEqual(TEXT("unfold fades at arrival"), Seat->GetSoundPlaybackForTest(1).LastFadeOutSeconds, .1f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FR11Key, "vBoR.R11.E8Key", R11Flags)
bool FR11Key::RunTest(const FString&)
{
	for (bool bCarPower : {true, false})
	{
		FR11World W;
		if (bCarPower) { W.AddPower(); }
		auto* Key = W.Spawn<AVBoRKeyFob>();
		auto* Hand = NewObject<UIsdkGrabberComponent>(W.Car);
		Hand->InteractorTags.AddTag(IsdkGameplayTags::TAG_Isdk_Type_Device_Right);
		Key->UnlockCarCue.Sound = Key->LockCarCue.Sound = Key->ButtonPressedCue.Sound;
		W.Play();
		FR11TestAccess::HoldKey(Key);
		Key->Tick(.01f);
		FR11World::Poke(Key->UnlockButton, Hand);
		TestEqual(TEXT("unlock cue only"), Key->GetSoundPlaybackForTest(0).Requests, 1);
		TestEqual(TEXT("lock untouched"), Key->GetSoundPlaybackForTest(1).Requests, 0);
		FR11World::Poke(Key->LockButton, Hand);
		TestEqual(TEXT("lock cue"), Key->GetSoundPlaybackForTest(1).Requests, 1);
		for (int32 Slot = 0; Slot < 2; ++Slot)
		{
			auto* Audio = Key->GetSoundAudioForTest(Slot);
			if (TestNotNull(TEXT("cue has audio"), Audio))
			{ TestTrue(TEXT("cue source follows car or fallback key"), Audio->GetAttachParent() == (bCarPower ? W.Car->GetRootComponent() : Key->GetRootComponent())); }
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FR11Reset, "vBoR.R11.E9Reset", R11Flags)
bool FR11Reset::RunTest(const FString&)
{
	for (bool bReverse : {false, true})
	{
		FR11World W;
		auto* Power = W.AddPower();
		auto* Zone = W.AddCarComponent<UVBoRCabinZoneComponent>();
		auto* Gate = W.Spawn<AVBoRPoweredPanel>();
		auto* Seat = W.Spawn<AVBoRFoldingSeat>();
		auto& Idle = bReverse ? Power->IdleInsideCue : Power->IdleCue;
		auto& GateCue = bReverse ? Gate->ClosingCue : Gate->OpeningCue;
		auto& SeatCue = bReverse ? Seat->UnfoldingCue : Seat->FoldingCue;
		for (auto* Cue : {&Idle, &GateCue, &SeatCue})
		{
			Cue->Sound = Gate->WarningCue.Sound;
			Cue->FadeOutSeconds = 2;
		}
		GateCue.StartDelaySeconds = SeatCue.StartDelaySeconds = .5f;
		Gate->DelaySeconds = 0;
		Gate->bStartOpen = bReverse;
		W.Play();
		Zone->SampleHead(true, bReverse ? FVector::ZeroVector : FVector(1000));

		auto Move = [&](float Seconds)
		{
			if (bReverse)
			{
				Seat->Fold();
				Seat->Tick(2);
				Seat->Unfold();
				Gate->RequestClose();
			}
			else
			{
				Seat->Fold();
				Gate->RequestOpen();
			}
			Gate->Tick(Seconds);
			Seat->Tick(Seconds);
		};
		const auto& GateSlot = Gate->GetSoundPlaybackForTest(bReverse ? EGateSound::Closing : EGateSound::Opening);
		const auto& SeatSlot = Seat->GetSoundPlaybackForTest(bReverse ? 1 : 0);
		const auto& IdleSlot = Power->GetSoundPlaybackForTest(bReverse ? 4 : 3);
		Move(.1f);
		TestTrue(TEXT("gate has delayed motion cue"), GateSlot.HasPending());
		TestTrue(TEXT("seat has delayed motion cue"), SeatSlot.HasPending());
		Gate->ResetPanel();
		Seat->ResetSeat();
		Gate->Tick(1);
		Seat->Tick(1);
		TestFalse(TEXT("gate reset cancels pending cue"), GateSlot.HasPending());
		TestFalse(TEXT("seat reset cancels pending cue"), SeatSlot.HasPending());
		TestEqual(TEXT("gate reset stays silent"), GateSlot.Requests, 0);
		TestEqual(TEXT("seat reset stays silent"), SeatSlot.Requests, 0);

		Move(.7f);
		Power->SetPowerOn(true);
		for (const auto* Slot : {&GateSlot, &SeatSlot, &IdleSlot})
		{ TestEqual(TEXT("held sound is active before EndPlay"), Slot->Requests, 1); }
		Gate->Destroy();
		Seat->Destroy();
		W.Car->Destroy();
		for (const auto* Slot : {&GateSlot, &SeatSlot, &IdleSlot})
		{
			TestFalse(TEXT("EndPlay clears pending sound"), Slot->HasPending());
			TestEqual(TEXT("EndPlay stops without fade"), Slot->LastFadeOutSeconds, 0.f);
		}
	}
	return true;
}
#endif
