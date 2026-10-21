#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "P5rTestFixture.h"
#include "P4bTestObserver.h"
#include "../Interaction/VBoRCarryObject.h"
#include "../Interaction/VBoRPokeButton.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/WorldSettings.h"
#include "Interaction/IsdkPokeInteractable.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundAttenuation.h"

namespace
{
	using ESound = EVBoRPoweredSound;
	struct FSoundWorld
	{
		UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
		AVBoRPoweredPanel* Gate = nullptr;
		UP4bTestObserver* Events = nullptr;
		TArray<UP5rAudioObserver*> Audio;
		bool bAudioAttachedToHinge = true;
		FSoundWorld()
		{
			World->SpawnActor<APlayerController>();
			Gate = World->SpawnActor<AVBoRPoweredPanel>();
			World->InitializeActorsForPlay(FURL());
			World->GetWorldSettings()->NotifyBeginPlay();
			Events = NewObject<UP4bTestObserver>(World);
			Gate->OnOpened.AddDynamic(Events, &UP4bTestObserver::Open);
			Gate->OnClosed.AddDynamic(Events, &UP4bTestObserver::Close);
			Gate->OnObstructed.AddDynamic(Events, &UP4bTestObserver::Obstruction);
			for (int32 I = 0; I < static_cast<int32>(ESound::Count); ++I)
			{
				bAudioAttachedToHinge &= Gate->GetSoundAudioForTest(static_cast<ESound>(I))->GetAttachParent() == Gate->HingePivot;
				Audio.Add(FP5rTestAccess::Observe(Gate, static_cast<ESound>(I)));
			}
		}
		~FSoundWorld() { World->EndPlay(EEndPlayReason::Quit); World->DestroyWorld(false); }
		const FVBoRSoundPlayback& Stats(ESound Slot) const { return Gate->GetSoundPlaybackForTest(Slot); }
		int32 Count(ESound Slot) const { return Stats(Slot).Requests; }
		UP5rAudioObserver* Device(ESound Slot) const { return Audio[static_cast<int32>(Slot)]; }
		void EnableAll()
		{
			USoundBase* Sound = Gate->WarningCue.Sound;
			Gate->OpenAcceptedCue.Sound = Sound; Gate->CloseAcceptedCue.Sound = Sound;
			Gate->MotorCue.Sound = Sound; Gate->OpenedCue.Sound = Sound;
			Gate->ClosedCue.Sound = Sound; Gate->ObstructedCue.Sound = Sound;
		}
		void Poke(bool bOpen)
		{
			AVBoRPokeButton* Button = bOpen ? Gate->OpenButton.Get() : Gate->CloseButton.Get();
			FIsdkInteractionPointerEvent Event;
			Event.Type = EIsdkPointerEventType::Select; Event.Interactable = Button->Poke.Get();
			Button->Poke->GetInteractionPointerEventDelegate().Broadcast(Event);
		}
		void OpenFast() { Gate->DelaySeconds = 0; Gate->SpeedDegPerSecond = 85; Gate->RequestOpen(); Gate->Tick(1); }
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5rDefaults, "vBoR.P5r.E1Defaults", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5rDefaults::RunTest(const FString& Parameters)
{
	FSoundWorld F;
	TestNotNull(TEXT("warning sound"), F.Gate->WarningCue.Sound.Get());
	TestEqual(TEXT("warning repeat s"), F.Gate->WarningCue.RepeatSeconds, 0.5f);
	TestEqual(TEXT("warning delay s"), F.Gate->WarningCue.StartDelaySeconds, 0.0f);
	TestTrue(TEXT("all native audio follows hinge"), F.bAudioAttachedToHinge);
	TestTrue(TEXT("six silent defaults"), !F.Gate->OpenAcceptedCue.Sound && !F.Gate->CloseAcceptedCue.Sound && !F.Gate->MotorCue.Sound && !F.Gate->OpenedCue.Sound && !F.Gate->ClosedCue.Sound && !F.Gate->ObstructedCue.Sound);
	F.Gate->RequestOpen(); F.Gate->Tick(6); F.Gate->RequestClose(); F.Gate->Tick(6);
	for (int32 I = 0; I < static_cast<int32>(ESound::Count); ++I)
	{
		if (static_cast<ESound>(I) != ESound::Warning) { TestEqual(TEXT("silent slot requests"), F.Count(static_cast<ESound>(I)), 0); }
		TestTrue(TEXT("audio follows moving hinge"), F.Gate->GetSoundAudioForTest(static_cast<ESound>(I)) == F.Audio[I]);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5rAccepted, "vBoR.P5r.E2AcceptedDelay", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5rAccepted::RunTest(const FString& Parameters)
{
	for (bool bOpen : {true, false})
	{
		FSoundWorld F; F.EnableAll(); F.Gate->bStartOpen = !bOpen; F.Gate->ResetPanel();
		F.Gate->OpenAcceptedCue.StartDelaySeconds = F.Gate->CloseAcceptedCue.StartDelaySeconds = 0.3f;
		const ESound Slot = bOpen ? ESound::OpenAccepted : ESound::CloseAccepted;
		F.Poke(bOpen);
		TestEqual(TEXT("callback silent"), F.Count(Slot), 0);
		TestEqual(TEXT("callback warning silent"), F.Gate->GetWarningRequests(), 0);
		F.Gate->Tick(0.1f); F.Gate->Tick(0.1f);
		TestEqual(TEXT("before delay"), F.Count(Slot), 0);
		F.Gate->Tick(0.1f);
		TestEqual(TEXT("delay reached"), F.Count(Slot), 1);
		TestEqual(TEXT("request frame"), F.Stats(Slot).RequestFrames[0], F.Gate->GetSoundFrameForTest());
		TestEqual(TEXT("other direction silent"), F.Count(bOpen ? ESound::CloseAccepted : ESound::OpenAccepted), 0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5rIgnored, "vBoR.P5r.E3IgnoredInputs", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5rIgnored::RunTest(const FString& Parameters)
{
	FSoundWorld F; F.EnableAll();
	F.Gate->RequestClose(); F.Gate->Tick(0.1f);
	TestEqual(TEXT("same endpoint ignored"), F.Count(ESound::CloseAccepted), 0);
	F.Gate->RequestOpen(); F.Gate->Tick(0);
	const int32 Warnings = F.Gate->GetWarningRequests();
	for (int32 I = 0; I < 3; ++I) { F.Gate->RequestOpen(); F.Gate->RequestClose(); }
	F.Gate->Tick(0);
	TestEqual(TEXT("wait ignores commands"), F.Count(ESound::OpenAccepted), 1);
	TestEqual(TEXT("no extra warning"), F.Gate->GetWarningRequests(), Warnings);
	F.Gate->Tick(1); F.Gate->RequestOpen(); F.Gate->RequestClose(); F.Gate->Tick(0);
	TestEqual(TEXT("motion ignores commands"), F.Count(ESound::CloseAccepted), 0);
	TestEqual(TEXT("only accepted open"), F.Count(ESound::OpenAccepted), 1);
	F.Gate->Tick(4); F.Gate->RequestOpen(); F.Gate->Tick(0);
	TestEqual(TEXT("open endpoint ignored"), F.Count(ESound::OpenAccepted), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5rWarning, "vBoR.P5r.E4Warning", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5rWarning::RunTest(const FString& Parameters)
{
	FSoundWorld F;
	F.Gate->RequestOpen(); TestEqual(TEXT("request defers warning"), F.Gate->GetWarningRequests(), 0);
	F.Gate->Tick(0); TestEqual(TEXT("first tick warning"), F.Gate->GetWarningRequests(), 1);
	F.Gate->Tick(0.5f); TestEqual(TEXT("repeat 0.5 s"), F.Gate->GetWarningRequests(), 2);
	F.Gate->Tick(4.5f); TestEqual(TEXT("wait and travel boundaries"), F.Gate->GetWarningRequests(), 11);
	const int32 Before = F.Gate->GetWarningRequests(); F.Gate->Tick(10);
	TestEqual(TEXT("arrival silent"), F.Gate->GetWarningRequests(), Before);
	F.Gate->ResetPanel(); F.Gate->WarningCue.StartDelaySeconds = 0.2f;
	F.Gate->RequestOpen(); F.Gate->Tick(0.1f);
	TestEqual(TEXT("delayed warning pending"), F.Gate->GetWarningRequests(), 0);
	F.Gate->Tick(0.1f); TestEqual(TEXT("delayed warning fires"), F.Gate->GetWarningRequests(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5rMotor, "vBoR.P5r.E5Motor", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5rMotor::RunTest(const FString& Parameters)
{
	for (float Repeat : {0.0f, 0.4f})
	{
		FSoundWorld F; F.EnableAll(); F.Gate->MotorCue.RepeatSeconds = Repeat;
		F.Gate->RequestOpen(); F.Gate->Tick(0.5f);
		TestEqual(TEXT("no motor in wait"), F.Count(ESound::Motor), 0);
		F.Gate->Tick(0.5f);
		TestEqual(TEXT("motor begins at wait boundary"), F.Count(ESound::Motor), 1);
		TestEqual(TEXT("motor begins this frame"), F.Stats(ESound::Motor).RequestFrames[0], F.Gate->GetSoundFrameForTest());
		F.Gate->Tick(0.4f);
		TestEqual(TEXT("motor repetition"), F.Count(ESound::Motor), Repeat == 0 ? 1 : 2);
		F.Gate->Tick(4);
		TestEqual(TEXT("arrival stops motor"), F.Device(ESound::Motor)->LifecycleStops(), 1);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5rFade, "vBoR.P5r.E6Fade", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5rFade::RunTest(const FString& Parameters)
{
	for (float Fade : {0.0f, 0.5f})
	{
		FSoundWorld F; F.EnableAll(); F.Gate->MotorCue.FadeOutSeconds = Fade;
		F.OpenFast();
		TestEqual(TEXT("fade request"), F.Device(ESound::Motor)->Fades, Fade > 0 ? 1 : 0);
		TestEqual(TEXT("stop request"), F.Device(ESound::Motor)->LifecycleStops(), Fade == 0 ? 1 : 0);
		TestEqual(TEXT("fade duration s"), F.Device(ESound::Motor)->FadeSeconds, Fade);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5rArrivals, "vBoR.P5r.E7Arrivals", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5rArrivals::RunTest(const FString& Parameters)
{
	for (float Delay : {0.0f, 0.25f})
	{
		FSoundWorld F; F.EnableAll(); F.Gate->OpenedCue.StartDelaySeconds = F.Gate->ClosedCue.StartDelaySeconds = Delay;
		F.OpenFast(); TestEqual(TEXT("opened delegate"), F.Events->Opened, 1);
		TestEqual(TEXT("opened sound on event frame or delayed"), F.Count(ESound::Opened), Delay == 0 ? 1 : 0);
		if (Delay == 0) { TestEqual(TEXT("opened frame"), F.Stats(ESound::Opened).RequestFrames[0], F.Gate->GetSoundFrameForTest()); }
		F.Gate->RequestClose(); F.Gate->Tick(0.25f);
		TestEqual(TEXT("opened survives close request"), F.Count(ESound::Opened), 1);
		F.Gate->Tick(0.75f); TestEqual(TEXT("closed delegate"), F.Events->Closed, 1);
		TestEqual(TEXT("closed sound on event frame or delayed"), F.Count(ESound::Closed), Delay == 0 ? 1 : 0);
		if (Delay == 0 && F.Count(ESound::Closed) > 0) { TestEqual(TEXT("closed frame"), F.Stats(ESound::Closed).RequestFrames[0], F.Gate->GetSoundFrameForTest()); }
		F.Gate->RequestOpen(); F.Gate->Tick(0.25f);
		TestEqual(TEXT("closed survives open request"), F.Count(ESound::Closed), 1);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5rObstructed, "vBoR.P5r.E8Obstruction", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5rObstructed::RunTest(const FString& Parameters)
{
	FSoundWorld F; F.EnableAll(); F.OpenFast();
	const FTransform Pose(FVector(-25, 15, 110));
	AVBoRCarryObject* Box = F.World->SpawnActorDeferred<AVBoRCarryObject>(AVBoRCarryObject::StaticClass(), Pose, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	Box->BoxSizeCm = FVector(20); Box->FinishSpawning(Pose);
	F.Gate->RequestClose();
	for (int32 I = 0; I < 120 && F.Events->Obstructed == 0; ++I) { F.Gate->Tick(1.0f / 60); }
	TestEqual(TEXT("obstruction delegate"), F.Events->Obstructed, 1);
	TestEqual(TEXT("obstruction sound"), F.Count(ESound::Obstructed), 1);
	if (F.Count(ESound::Obstructed) > 0) { TestEqual(TEXT("obstruction same frame"), F.Stats(ESound::Obstructed).RequestFrames[0], F.Gate->GetSoundFrameForTest()); }
	TestEqual(TEXT("motor no restart on reversal"), F.Count(ESound::Motor), 2);
	TestEqual(TEXT("only earlier open stopped motor"), F.Device(ESound::Motor)->LifecycleStops(), 1);
	F.Gate->Tick(2);
	TestEqual(TEXT("reopened event"), F.Events->Opened, 2);
	TestEqual(TEXT("reopened sound"), F.Count(ESound::Opened), 2);
	TestEqual(TEXT("motor stopped at reopen"), F.Device(ESound::Motor)->LifecycleStops(), 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5rShortState, "vBoR.P5r.E9ShortState", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5rShortState::RunTest(const FString& Parameters)
{
	FSoundWorld F; F.EnableAll(); F.Gate->MotorCue.StartDelaySeconds = 2;
	F.OpenFast(); F.Gate->Tick(10);
	TestEqual(TEXT("motor delay cancelled at arrival"), F.Count(ESound::Motor), 0);
	TestEqual(TEXT("pending motor not faded"), F.Device(ESound::Motor)->Fades, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5rReset, "vBoR.P5r.E10Reset", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5rReset::RunTest(const FString& Parameters)
{
	for (bool bClear : {true, false})
	{
		for (int32 Phase = 0; Phase < 3; ++Phase)
		{
			FSoundWorld F; F.EnableAll(); F.Gate->MotorCue.FadeOutSeconds = 0.5f;
			F.Gate->OpenAcceptedCue.StartDelaySeconds = 8; F.Gate->OpenedCue.StartDelaySeconds = 8;
			F.Gate->RequestOpen(); F.Gate->Tick(Phase == 0 ? 0.25f : Phase == 1 ? 2.0f : 5.0f);
			int32 Before[7]; for (int32 I = 0; I < 7; ++I) { Before[I] = F.Count(static_cast<ESound>(I)); }
			F.Gate->ResetPanel(bClear); F.Gate->Tick(10);
			for (int32 I = 0; I < 7; ++I)
			{
				TestEqual(TEXT("reset clears schedules and preserves count policy"), F.Count(static_cast<ESound>(I)), bClear ? 0 : Before[I]);
				TestTrue(TEXT("reset direct stop"), F.Audio[I]->Stops > 0);
				TestEqual(TEXT("reset no fade"), F.Stats(static_cast<ESound>(I)).LastFadeOutSeconds, 0.0f);
			}
			TestEqual(TEXT("legacy warning count"), F.Gate->GetWarningRequests(), bClear ? 0 : Before[static_cast<int32>(ESound::Warning)]);
		}
	}
	FSoundWorld F; F.EnableAll(); F.Gate->OpenedCue.StartDelaySeconds = 8; F.OpenFast();
	F.Gate->Destroy(); F.Gate->Tick(10);
	TestEqual(TEXT("EndPlay cancels instant schedule"), F.Count(ESound::Opened), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5rFrameTime, "vBoR.P5r.E11FrameTime", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5rFrameTime::RunTest(const FString& Parameters)
{
	FSoundWorld Large, Small; Large.EnableAll(); Small.EnableAll();
	for (FSoundWorld* F : {&Large, &Small})
	{
		F->Gate->DelaySeconds = 0.2f; F->Gate->SpeedDegPerSecond = 212.5f;
		F->Gate->OpenAcceptedCue.StartDelaySeconds = 0.3f;
		F->Gate->WarningCue.RepeatSeconds = 0.1f; F->Gate->MotorCue.RepeatSeconds = 0.1f;
		F->Gate->RequestOpen();
	}
	Large.Gate->Tick(1); for (int32 I = 0; I < 10; ++I) { Small.Gate->Tick(0.1f); }
	for (int32 I = 0; I < 7; ++I) { TestEqual(TEXT("equal request counts across frame sizes"), Large.Count(static_cast<ESound>(I)), Small.Count(static_cast<ESound>(I))); }
	TestEqual(TEXT("long frame counts repeats"), Large.Count(ESound::Motor), 5);
	TestEqual(TEXT("one physical play per long frame"), Large.Device(ESound::Motor)->Plays, 1);
	TestEqual(TEXT("accepted delay in long frame"), Large.Stats(ESound::OpenAccepted).RequestFrames[0], uint64(1));
	TestEqual(TEXT("arrival in long frame"), Large.Stats(ESound::Opened).RequestFrames[0], uint64(1));
	// 양수 최소 반복 간격도 긴 frame 요청 수로 확인함 (P5r D1)
	FSoundWorld Clamp; Clamp.EnableAll(); Clamp.Gate->WarningCue.RepeatSeconds = 0.001f;
	Clamp.Gate->RequestOpen(); Clamp.Gate->Tick(0.2f);
	TestEqual(TEXT("repeat clamps to 0.05 s"), Clamp.Gate->GetWarningRequests(), 5);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5rSettings, "vBoR.P5r.E12Settings", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5rSettings::RunTest(const FString& Parameters)
{
	FSoundWorld F; F.EnableAll();
	USoundAttenuation* Attenuation = NewObject<USoundAttenuation>(F.Gate);
	F.Gate->OpenAcceptedCue.VolumeMultiplier = 0.3f; F.Gate->OpenAcceptedCue.PitchMultiplier = 1.4f; F.Gate->OpenAcceptedCue.Attenuation = Attenuation;
	F.Gate->RequestOpen(); F.Gate->Tick(0);
	UAudioComponent* Audio = F.Device(ESound::OpenAccepted);
	TestEqual(TEXT("volume applied"), Audio->VolumeMultiplier, 0.3f);
	TestEqual(TEXT("pitch applied"), Audio->PitchMultiplier, 1.4f);
	TestTrue(TEXT("attenuation applied"), Audio->AttenuationSettings == Attenuation);
	F.Gate->ResetPanel(); F.Gate->OpenAcceptedCue.Attenuation = nullptr;
	F.Gate->RequestOpen(); F.Gate->Tick(0);
	TestTrue(TEXT("empty attenuation preserves component"), Audio->AttenuationSettings == Attenuation);
	TestTrue(TEXT("separate slot untouched"), F.Device(ESound::Motor)->AttenuationSettings == nullptr);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5rIndependent, "vBoR.P5r.E13IndependentSlots", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5rIndependent::RunTest(const FString& Parameters)
{
	FSoundWorld F; F.EnableAll(); F.Gate->CloseAcceptedCue.StartDelaySeconds = 1;
	F.OpenFast(); F.Gate->RequestClose(); F.Gate->Tick(1);
	TestEqual(TEXT("closed arrival"), F.Count(ESound::Closed), 1);
	TestEqual(TEXT("accepted delay overlaps arrival"), F.Count(ESound::CloseAccepted), 1);
	TestEqual(TEXT("warning stopped twice"), F.Device(ESound::Warning)->LifecycleStops(), 2);
	TestEqual(TEXT("closed slot never stopped"), F.Device(ESound::Closed)->LifecycleStops(), 0);
	TestEqual(TEXT("accepted slot never stopped"), F.Device(ESound::CloseAccepted)->LifecycleStops(), 0);
	TestEqual(TEXT("closed and accepted same frame"), F.Stats(ESound::Closed).RequestFrames[0], F.Stats(ESound::CloseAccepted).RequestFrames[0]);
	// 같은 칸의 지연 예약도 이벤트마다 남음 (P5r D2)
	F.Gate->OpenAcceptedCue.StartDelaySeconds = 4;
	F.Gate->RequestOpen(); F.Gate->Tick(1); F.Gate->RequestClose(); F.Gate->Tick(1);
	F.Gate->RequestOpen(); F.Gate->Tick(1);
	const int32 Before = F.Count(ESound::OpenAccepted);
	F.Gate->Tick(1); TestEqual(TEXT("first reservation retained"), F.Count(ESound::OpenAccepted), Before + 1);
	F.Gate->Tick(2); TestEqual(TEXT("second reservation retained"), F.Count(ESound::OpenAccepted), Before + 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5rLegacy, "vBoR.P5r.E14LegacyWarningContract", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5rLegacy::RunTest(const FString& Parameters)
{
	FSoundWorld F;
	F.Gate->RequestOpen(); F.Gate->Tick(0.75f);
	TestEqual(TEXT("legacy first and repeat"), F.Gate->GetWarningRequests(), 2);
	F.Gate->Tick(4.25f); TestEqual(TEXT("legacy full cycle"), F.Gate->GetWarningRequests(), 11);
	F.Gate->ResetPanel(false); TestEqual(TEXT("B keeps warnings"), F.Gate->GetWarningRequests(), 11);
	F.Gate->ResetPanel(true); TestEqual(TEXT("R clears warnings"), F.Gate->GetWarningRequests(), 0);
	return true;
}
#endif
