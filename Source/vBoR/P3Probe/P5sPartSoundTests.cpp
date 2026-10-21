#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "../P1Probe/P5rTestFixture.h"
#include "../Interaction/VBoRAxisPart.h"
#include "../Interaction/VBoRHingedPanel.h"
#include "../Interaction/VBoRKnob.h"
#include "../Interaction/VBoRSteeringWheel.h"
#include "../Interaction/VBoRCarryObject.h"
#include "../Interaction/VBoRFoldingMirror.h"
#include "../Interaction/VBoRVehiclePowerComponent.h"
#include "../Interaction/VBoRPokeButton.h"
#include "../Interaction/VBoRKeyFob.h"
#include "Audio/IsdkPointerEventAudioPlayer.h"
#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "CoreGlobals.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "Interaction/IsdkPokeInteractable.h"
#include "Interaction/IsdkPokeButtonVisual.h"
#include "IsdkContentAssetPaths.h"
#include "Sound/SoundBase.h"

// 합성 이벤트는 기존 확정/알림 경계로 넣음 (실제 손·청각 시험과 구분, P5s §4)
struct FP5sTestAccess
{
	template<class T> static UP5rAudioObserver* Observe(T* Part, int32 Slot)
	{
		auto* A = NewObject<UP5rAudioObserver>(Part); Part->SoundAudio[Slot] = A; return A;
	}
	static void Open(AVBoRAxisPart* P, bool bOpen)
	{ P->Value = bOpen ? P->MaxAngleDeg : 0; P->EvaluateState(); }
	static void Detents(AVBoRAxisPart* P)
	{ P->PendingDetents.Add({false, 0}); P->PendingDetents.Add({false, 1}); }
	static void Nudge(AVBoRAxisPart* P)
	{ P->PendingDetents.Add({true, 1}); P->PendingDetents.Add({false, 2}); }
	static void Latch(AVBoRHingedPanel* P)
	{ const int32 Before = P->Motion.Metrics.Latches++; P->ApplyState(P->Motion.AngleDeg, P->Motion.Metrics.Unlocks, Before); }
	static void Step(AVBoRKnob* P) { P->Motion.PendingSteps.Add(1); }
	static void Limit(AVBoRSteeringWheel* P) { P->PendingLimits.Add(1); }
	static void Pause(AVBoRCarryObject* P, bool bPause, bool bHeld = true)
	{ P->Session.Hand = bHeld ? 0 : INDEX_NONE; P->Session.bPaused = bPause; P->UpdateVisual(); }
	static void Block(AVBoRCarryObject* P, const FTransform& Pose) { P->MoveHeldPose(Pose, .1f); }
};

namespace
{
struct FP5sWorld
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	AActor* Car;
	UVBoRVehiclePowerComponent* Power = nullptr;
	AVBoRPokeButton* Button;
	AVBoRAxisPart* Axis;
	AVBoRHingedPanel* Panel;
	AVBoRKnob* Knob;
	AVBoRSteeringWheel* Wheel;
	AVBoRCarryObject* Box;
	AVBoRFoldingMirror* Mirror;
	AVBoRPoweredPanel* Gate;
	AVBoRKeyFob* Key;
	uint64 Frame = GFrameCounter;
	FP5sWorld(int32 PowerCount = 1)
	{
		Car = World->SpawnActor<AActor>(); Car->SetRootComponent(NewObject<USceneComponent>(Car)); Car->GetRootComponent()->RegisterComponent();
		for (int32 I = 0; I < PowerCount; ++I)
		{ auto* P = NewObject<UVBoRVehiclePowerComponent>(Car); Car->AddInstanceComponent(P); P->RegisterComponent(); if (!Power) { Power = P; } }
		Button = World->SpawnActor<AVBoRPokeButton>(); Button->Tags.Add(TEXT("Car.StartButton"));
		Axis = World->SpawnActor<AVBoRAxisPart>(); Axis->Release = EVBoRAxisRelease::Stay;
		Panel = World->SpawnActor<AVBoRHingedPanel>(); Panel->bUseLatch = true; Knob = World->SpawnActor<AVBoRKnob>(); Wheel = World->SpawnActor<AVBoRSteeringWheel>();
		Box = World->SpawnActor<AVBoRCarryObject>(); Box->SetActorLocation(FVector(1000,0,100));
		Mirror = World->SpawnActor<AVBoRFoldingMirror>(); Gate = World->SpawnActor<AVBoRPoweredPanel>(); Key = World->SpawnActor<AVBoRKeyFob>();
	}
	void Play() { World->InitializeActorsForPlay(FURL()); World->GetWorldSettings()->NotifyBeginPlay(); }
	~FP5sWorld() { World->EndPlay(EEndPlayReason::Quit); World->DestroyWorld(false); GFrameCounter = Frame; }
	void Poke(AVBoRPokeButton* B, EIsdkPointerEventType Type = EIsdkPointerEventType::Select)
	{ FIsdkInteractionPointerEvent E; E.Type = Type; E.Interactable = B->Poke.Get(); B->Poke->GetInteractionPointerEventDelegate().Broadcast(E); }
	FTransform BlockedPose()
	{
		auto* Obstacle = World->SpawnActor<AActor>(); auto* Wall = NewObject<UBoxComponent>(Obstacle); Obstacle->SetRootComponent(Wall);
		Wall->SetBoxExtent(FVector(5,100,100)); Wall->SetCollisionEnabled(ECollisionEnabled::QueryOnly); Wall->SetCollisionResponseToAllChannels(ECR_Block); Wall->RegisterComponent(); Obstacle->SetActorLocation(FVector(1100,0,100));
		FTransform Desired = Box->GetActorTransform(); Desired.SetLocation(FVector(1200,0,100)); return Desired;
	}
};

template<class T> void P5sOne(FAutomationTestBase& Test, T* Part, int32 Slot, UP5rAudioObserver* Audio)
{
	Test.TestEqual(TEXT("event requests one"), Part->GetSoundPlaybackForTest(Slot).Requests, 1);
	Test.TestEqual(TEXT("event calls Play once"), Audio->Plays, 1);
	const auto& Frames = Part->GetSoundPlaybackForTest(Slot).RequestFrames;
	if (Test.TestFalse(TEXT("event has request frame"), Frames.IsEmpty()))
	{ Test.TestEqual(TEXT("request in event frame"), Frames.Last(), Part->GetSoundFrameForTest()); }
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5sDefaults, "vBoR.P5s.E1Defaults", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5sDefaults::RunTest(const FString&)
{
	FP5sWorld W; W.Play();
	auto* S = LoadObject<USoundBase>(nullptr, IsdkContentAssetPaths::Audio::Interaction_BasicPoke_ButtonPress);
	for (const FVBoRSoundCue* Cue : {&W.Button->PressedCue, &W.Gate->ButtonPressedCue, &W.Key->ButtonPressedCue, &W.Axis->OpenedCue, &W.Axis->ClosedCue, &W.Axis->DetentCue, &W.Axis->NudgeCue, &W.Panel->UnlockedCue, &W.Panel->LatchedCue, &W.Knob->StepCue, &W.Wheel->LimitCue, &W.Box->BlockedCue, static_cast<FVBoRSoundCue*>(&W.Box->PausedCue)})
	{ TestTrue(TEXT("existing sound preserved"), Cue->Sound == S); TestEqual(TEXT("zero delay"), Cue->StartDelaySeconds, 0.f); }
	for (const FVBoRSoundCue* Cue : {&W.Button->ReleasedCue, &W.Gate->ButtonReleasedCue, &W.Key->ButtonReleasedCue, static_cast<FVBoRSoundCue*>(&W.Mirror->UnfoldMotorCue), static_cast<FVBoRSoundCue*>(&W.Mirror->FoldMotorCue), &W.Mirror->UnfoldedCue, &W.Mirror->FoldedCue, &W.Power->PowerOnCue, &W.Power->PowerOffCue})
	{ TestNull(TEXT("new event silent"), Cue->Sound.Get()); }
	TestEqual(TEXT("pause repeat s"), W.Box->PausedCue.RepeatSeconds, .25f);
	TestTrue(TEXT("button audio on moving visual"), W.Button->GetSoundAudioForTest(0)->GetAttachParent() == W.Button->ButtonVisual);
	TestTrue(TEXT("mirror audio on housing"), W.Mirror->GetSoundAudioForTest(0)->GetAttachParent() == W.Mirror->MirrorMesh);
	TestNull(TEXT("silent power does not add audio to core"), W.Power->GetSoundAudioForTest(0));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5sEvents, "vBoR.P5s.E2ExistingEvents", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5sEvents::RunTest(const FString&)
{
	FP5sWorld W; W.Play();
	auto* B = FP5sTestAccess::Observe(W.Button, 0); W.Poke(W.Button); P5sOne(*this,W.Button,0,B);
	auto* O = FP5sTestAccess::Observe(W.Axis, 0); auto* C = FP5sTestAccess::Observe(W.Axis, 1);
	FP5sTestAccess::Open(W.Axis,true); P5sOne(*this,W.Axis,0,O); TestEqual(TEXT("other axis slot silent"),C->Plays,0);
	FP5sTestAccess::Open(W.Axis,false); P5sOne(*this,W.Axis,1,C);
	auto* U = FP5sTestAccess::Observe(W.Panel,0); auto* L = FP5sTestAccess::Observe(W.Panel,1);
	W.Panel->Unlock(); P5sOne(*this,W.Panel,0,U); TestEqual(TEXT("latch untouched"), L->Plays,0);
	FP5sTestAccess::Latch(W.Panel); P5sOne(*this,W.Panel,1,L);
	auto* K = FP5sTestAccess::Observe(W.Knob,0); FP5sTestAccess::Step(W.Knob); TestEqual(TEXT("knob waits Tick"),K->Plays,0); W.Knob->Tick(.01f); P5sOne(*this,W.Knob,0,K);
	auto* H = FP5sTestAccess::Observe(W.Wheel,0); FP5sTestAccess::Limit(W.Wheel); TestEqual(TEXT("wheel waits Tick"),H->Plays,0); W.Wheel->Tick(.01f); P5sOne(*this,W.Wheel,0,H);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5sImmediate, "vBoR.P5s.E3ZeroDelay", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5sImmediate::RunTest(const FString&)
{
	FP5sWorld W; W.Play(); auto* B = FP5sTestAccess::Observe(W.Button,0);
	W.Poke(W.Button); W.Poke(W.Button); TestEqual(TEXT("two Selects without Tick"),B->Plays,2);
	auto* D = FP5sTestAccess::Observe(W.Axis,2); FP5sTestAccess::Detents(W.Axis);
	TestEqual(TEXT("detents wait Tick"),D->Plays,0); W.Axis->Tick(.01f);
	TestEqual(TEXT("two detents same Tick"),D->Plays,2); TestEqual(TEXT("two requests"),W.Axis->GetSoundPlaybackForTest(2).Requests,2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5sDelays, "vBoR.P5s.E4Delays", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5sDelays::RunTest(const FString&)
{
	FP5sWorld W; W.Play(); auto* S = W.Button->PressedCue.Sound.Get();
	W.Button->PressedCue.StartDelaySeconds = W.Axis->OpenedCue.StartDelaySeconds = W.Panel->LatchedCue.StartDelaySeconds = W.Knob->StepCue.StartDelaySeconds = W.Wheel->LimitCue.StartDelaySeconds = W.Box->BlockedCue.StartDelaySeconds = .3f;
	W.Mirror->UnfoldedCue.Sound = W.Power->PowerOnCue.Sound = S; W.Mirror->UnfoldedCue.StartDelaySeconds = W.Power->PowerOnCue.StartDelaySeconds = .3f;
	auto* B = FP5sTestAccess::Observe(W.Button,0); auto* A = FP5sTestAccess::Observe(W.Axis,0); auto* P = FP5sTestAccess::Observe(W.Panel,1);
	auto* K = FP5sTestAccess::Observe(W.Knob,0); auto* H = FP5sTestAccess::Observe(W.Wheel,0); auto* X = FP5sTestAccess::Observe(W.Box,0);
	auto* M = FP5sTestAccess::Observe(W.Mirror,2); auto* V = FP5sTestAccess::Observe(W.Power,0);
	W.Poke(W.Button); FP5sTestAccess::Open(W.Axis,true); FP5sTestAccess::Latch(W.Panel); FP5sTestAccess::Step(W.Knob); W.Knob->Tick(.01f); FP5sTestAccess::Limit(W.Wheel); W.Wheel->Tick(.01f);
	// 충돌 world 에서 막힘을 일으킴 (예약 자체를 직접 넣지 않음, P5s E4·E10)
	FP5sTestAccess::Block(W.Box,W.BlockedPose());
	W.Mirror->Tick(3); // 펼침 도착 뒤 지연 시작
	TestTrue(TEXT("button delay Tick enabled"),W.Button->IsActorTickEnabled()); TestTrue(TEXT("power delay Tick enabled"),W.Power->IsComponentTickEnabled());
	auto Tick = [&](float Dt) { W.Button->Tick(Dt); W.Axis->Tick(Dt); W.Panel->Tick(Dt); W.Knob->Tick(Dt); W.Wheel->Tick(Dt); W.Box->Tick(Dt); W.Mirror->Tick(Dt); W.Power->TickComponent(Dt,LEVELTICK_All,nullptr); };
	Tick(.2f); for (auto* Audio : {B,A,P,K,H,X,M,V}) { TestEqual(TEXT("before .3 s"),Audio->Plays,0); }
	Tick(.1f); for (auto* Audio : {B,A,P,K,H,X,M,V}) { TestEqual(TEXT("at .3 s"),Audio->Plays,1); }
	TestFalse(TEXT("button idle Tick off"),W.Button->IsActorTickEnabled()); TestFalse(TEXT("power idle Tick off"),W.Power->IsComponentTickEnabled());
	// NaN frame 시간은 0 으로 봄. 예약이 영영 안 끝나지 않게 함 (P5s 리뷰 참고 1)
	FP5sTestAccess::Open(W.Axis,false); FP5sTestAccess::Open(W.Axis,true);
	W.Axis->Tick(std::numeric_limits<float>::quiet_NaN()); TestEqual(TEXT("NaN frame keeps delay"),A->Plays,1);
	W.Axis->Tick(.3f); TestEqual(TEXT("delay after NaN frame"),A->Plays,2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5sButton, "vBoR.P5s.E5Button", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5sButton::RunTest(const FString&)
{
	FP5sWorld W; W.Play(); W.Button->ReleasedCue.Sound = W.Button->PressedCue.Sound;
	auto* P = FP5sTestAccess::Observe(W.Button,0); auto* R = FP5sTestAccess::Observe(W.Button,1);
	W.Button->PressFilter.BindLambda([](const FIsdkInteractionPointerEvent&) { return false; });
	W.Poke(W.Button); W.Poke(W.Button,EIsdkPointerEventType::Unselect);
	TestEqual(TEXT("rejected Select still sounds"),P->Plays,1); TestEqual(TEXT("every Unselect sounds"),R->Plays,1);
	TestNull(TEXT("SDK Select audio empty"),W.Button->PointerAudio->GetSelectAudio());
	W.Button->Poke->Deactivate(); W.Button->Tick(1); TestEqual(TEXT("inactive Poke has no autonomous sound"),P->Plays,1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5sChildButtons, "vBoR.P5s.E6ChildButtons", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5sChildButtons::RunTest(const FString&)
{
	FP5sWorld W; auto* S = W.Button->PressedCue.Sound.Get(); W.Gate->ButtonPressedCue.StartDelaySeconds = .4f; W.Key->ButtonPressedCue.StartDelaySeconds = .6f;
	W.Gate->ButtonReleasedCue.Sound = W.Key->ButtonReleasedCue.Sound = S; W.Gate->OpenAcceptedCue.Sound = S; W.Play();
	for (auto* B : {W.Gate->OpenButton.Get(),W.Gate->CloseButton.Get()}) { TestEqual(TEXT("gate delay copied"),B->PressedCue.StartDelaySeconds,.4f); TestTrue(TEXT("gate release copied"),B->ReleasedCue.Sound==S); }
	for (auto* B : {W.Key->UnlockButton.Get(),W.Key->LockButton.Get()}) { TestEqual(TEXT("key delay copied"),B->PressedCue.StartDelaySeconds,.6f); TestTrue(TEXT("key release copied"),B->ReleasedCue.Sound==S); }
	auto* B = FP5sTestAccess::Observe(W.Gate->OpenButton.Get(),0); auto* G = FP5rTestAccess::Observe(W.Gate,EVBoRPoweredSound::OpenAccepted);
	W.Poke(W.Gate->OpenButton); W.Gate->Tick(.01f); TestEqual(TEXT("gate accepted separate"),G->Plays,1); TestEqual(TEXT("button still delayed"),B->Plays,0);
	W.Gate->OpenButton->Tick(.4f); TestEqual(TEXT("button plays its own slot"),B->Plays,1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5sAxisSlots, "vBoR.P5s.E7AxisSlots", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5sAxisSlots::RunTest(const FString&)
{
	FP5sWorld W; W.Play(); auto* D = FP5sTestAccess::Observe(W.Axis,2); auto* N = FP5sTestAccess::Observe(W.Axis,3);
	FP5sTestAccess::Nudge(W.Axis); W.Axis->Tick(.01f); P5sOne(*this,W.Axis,2,D); P5sOne(*this,W.Axis,3,N);
	TestEqual(TEXT("detent not stopped by nudge"),D->LifecycleStops(),0); TestEqual(TEXT("nudge not stopped by detent"),N->LifecycleStops(),0);
	TestEqual(TEXT("opened untouched"),W.Axis->GetSoundPlaybackForTest(0).Requests,0); TestEqual(TEXT("closed untouched"),W.Axis->GetSoundPlaybackForTest(1).Requests,0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5sMirror, "vBoR.P5s.E8Mirror", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5sMirror::RunTest(const FString&)
{
	FP5sWorld W; auto* S = W.Button->PressedCue.Sound.Get();
	W.Mirror->UnfoldMotorCue.Sound = W.Mirror->FoldMotorCue.Sound = W.Mirror->UnfoldedCue.Sound = W.Mirror->FoldedCue.Sound = S;
	W.Mirror->UnfoldMotorCue.FadeOutSeconds = .2f; W.Mirror->FoldMotorCue.FadeOutSeconds = .4f; W.Play();
	auto* U = FP5sTestAccess::Observe(W.Mirror,0); auto* F = FP5sTestAccess::Observe(W.Mirror,1); auto* O = FP5sTestAccess::Observe(W.Mirror,2); auto* C = FP5sTestAccess::Observe(W.Mirror,3);
	TestEqual(TEXT("BeginPlay silent"),W.Mirror->GetSoundPlaybackForTest(0).Requests,0);
	W.Power->SetPowerOn(true); P5sOne(*this,W.Mirror,0,U); W.Mirror->Tick(3); P5sOne(*this,W.Mirror,2,O); TestEqual(TEXT("unfold motor fade"),U->FadeSeconds,.2f);
	W.Power->SetPowerOn(false); P5sOne(*this,W.Mirror,1,F); W.Mirror->Tick(3); P5sOne(*this,W.Mirror,3,C); TestEqual(TEXT("fold motor fade"),F->FadeSeconds,.4f);
	W.Power->SetPowerOn(true); W.Mirror->Tick(.5f); const uint64 Frame = W.Mirror->GetSoundFrameForTest(); W.Power->SetPowerOn(false);
	TestEqual(TEXT("reversal starts fold same frame"),W.Mirror->GetSoundPlaybackForTest(1).RequestFrames.Last(),Frame); TestEqual(TEXT("reversal fades unfolding"),U->Fades,2);
	W.Mirror->ResetMirror(); TestEqual(TEXT("reset immediate stop"),W.Mirror->GetSoundPlaybackForTest(1).LastFadeOutSeconds,0.f);
	W.Mirror->Tick(10); TestEqual(TEXT("reset clears counts"),W.Mirror->GetSoundPlaybackForTest(1).Requests,0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5sPower, "vBoR.P5s.E9Power", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5sPower::RunTest(const FString&)
{
	FP5sWorld W; auto* S = W.Button->PressedCue.Sound.Get(); W.Power->PowerOnCue.Sound = W.Power->PowerOffCue.Sound = S; W.Play();
	// 소리를 넣어도 Play 시작 때는 차체에 오디오가 없음. 첫 요청 때 하나 생김 (P4i E2 Core 비교 유지, P5s R1-1)
	TArray<UAudioComponent*> CarAudio; W.Car->GetComponents(CarAudio);
	TestEqual(TEXT("no vehicle audio before first request"), CarAudio.Num(), 0);
	W.Power->SetPowerOn(true); W.Car->GetComponents(CarAudio);
	TestEqual(TEXT("first request adds one vehicle audio"), CarAudio.Num(), 1);
	if (TestNotNull(TEXT("power on audio"), W.Power->GetSoundAudioForTest(0)))
	{ TestTrue(TEXT("power audio on vehicle root"), W.Power->GetSoundAudioForTest(0)->GetAttachParent() == W.Car->GetRootComponent()); }
	W.Power->SetPowerOn(false, EVBoRPowerSource::Restart);
	auto* On = FP5sTestAccess::Observe(W.Power,0); auto* Off = FP5sTestAccess::Observe(W.Power,1);
	W.Poke(W.Button); W.Power->TogglePower(EVBoRPowerSource::Operator); TestEqual(TEXT("same frame coalesced"),On->Plays,1); TestEqual(TEXT("no duplicate off"),Off->Plays,0);
	++GFrameCounter; W.Power->TogglePower(EVBoRPowerSource::Operator); TestEqual(TEXT("operator off"),Off->Plays,1);
	W.Power->SetPowerOn(true); W.Power->SetPowerOn(true); TestEqual(TEXT("direct only changed value"),On->Plays,2);
	W.Power->SetPowerOn(false,EVBoRPowerSource::Restart); TestEqual(TEXT("Restart no off sound"),Off->Plays,1);
	W.Power->PowerOnCue.StartDelaySeconds = .3f; W.Power->SetPowerOn(true); W.Power->SetPowerOn(false,EVBoRPowerSource::Restart); W.Power->TickComponent(10,LEVELTICK_All,nullptr); TestEqual(TEXT("Restart cancels delayed on"),On->Plays,2);
	for (int32 Count : {0,2})
	{
		AddExpectedMessage(TEXT("Side mirror: No vehicle power"),ELogVerbosity::Warning,EAutomationExpectedMessageFlags::Contains,1);
		if (Count == 2) { AddExpectedMessage(TEXT("Vehicle power: expected one component, found 2"),ELogVerbosity::Warning,EAutomationExpectedMessageFlags::Contains,1); }
		FP5sWorld Invalid(Count); Invalid.Play(); Invalid.Mirror->UnfoldMotorCue.Sound = S; Invalid.Mirror->Tick(10); TestEqual(TEXT("unavailable mirror silent"),Invalid.Mirror->GetSoundPlaybackForTest(0).Requests,0);
		if (Invalid.Power) { Invalid.Power->PowerOnCue.Sound = S; Invalid.Power->SetPowerOn(true); TestEqual(TEXT("unavailable power silent"),Invalid.Power->GetSoundPlaybackForTest(0).Requests,0); }
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5sBox, "vBoR.P5s.E10Box", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5sBox::RunTest(const FString&)
{
	int32 Counts[2] = {};
	for (int32 Mode = 0; Mode < 2; ++Mode)
	{
		FP5sWorld W; W.Play(); auto* A = FP5sTestAccess::Observe(W.Box,1); FP5sTestAccess::Pause(W.Box,true);
		for (int32 I=0; I < (Mode ? 10 : 1); ++I) { W.Box->Tick(Mode ? .1f : 1.f); }
		Counts[Mode] = W.Box->GetSoundPlaybackForTest(1).Requests; TestEqual(TEXT("initial plus four repeats"),Counts[Mode],5);
		FP5sTestAccess::Pause(W.Box,false); TestTrue(TEXT("recovery stops"),A->LifecycleStops()>0);
		FP5sTestAccess::Pause(W.Box,true); FP5sTestAccess::Pause(W.Box,false,false); TestFalse(TEXT("release ends hold"),W.Box->GetSoundPlaybackForTest(1).HasPending());
		FP5sTestAccess::Pause(W.Box,true); W.Box->ResetToPose(W.Box->GetActorTransform()); W.Box->Tick(10); TestEqual(TEXT("reset clears paused sound"),W.Box->GetSoundPlaybackForTest(1).Requests,0);
	}
	TestEqual(TEXT("partition independent request count"),Counts[0],Counts[1]);
	FP5sWorld W; W.Play(); auto* Blocked = FP5sTestAccess::Observe(W.Box,0); const FTransform Desired = W.BlockedPose();
	FP5sTestAccess::Block(W.Box,Desired); TestTrue(TEXT("actual wall contact"),W.Box->IsBlocked()); P5sOne(*this,W.Box,0,Blocked);
	FP5sTestAccess::Block(W.Box,Desired); TestEqual(TEXT("persistent block does not repeat"),Blocked->Plays,1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5sCleanup, "vBoR.P5s.E11Cleanup", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5sCleanup::RunTest(const FString&)
{
	FP5sWorld W; W.Play();
	USoundBase* Sound = W.Button->PressedCue.Sound;
	W.Button->PressedCue.StartDelaySeconds = W.Axis->OpenedCue.StartDelaySeconds = W.Panel->LatchedCue.StartDelaySeconds = W.Knob->StepCue.StartDelaySeconds = W.Wheel->LimitCue.StartDelaySeconds = W.Box->BlockedCue.StartDelaySeconds = .3f;
	auto* B=FP5sTestAccess::Observe(W.Button,0); auto* A=FP5sTestAccess::Observe(W.Axis,0); auto* P=FP5sTestAccess::Observe(W.Panel,1); auto* K=FP5sTestAccess::Observe(W.Knob,0); auto* H=FP5sTestAccess::Observe(W.Wheel,0);
	W.Poke(W.Button); FP5sTestAccess::Open(W.Axis,true); FP5sTestAccess::Latch(W.Panel); FP5sTestAccess::Step(W.Knob); W.Knob->Tick(.01f); FP5sTestAccess::Limit(W.Wheel); W.Wheel->Tick(.01f);
	W.Panel->ResetDoor(); W.Panel->Tick(10); TestEqual(TEXT("ResetDoor cancels"),P->Plays,0);
	auto* X=FP5sTestAccess::Observe(W.Box,0); FP5sTestAccess::Block(W.Box,W.BlockedPose());
	auto* M=FP5sTestAccess::Observe(W.Mirror,2); W.Mirror->UnfoldedCue.Sound=Sound; W.Mirror->UnfoldedCue.StartDelaySeconds=.3f;
	W.Mirror->Tick(3);
	auto* V=FP5sTestAccess::Observe(W.Power,0); W.Power->PowerOnCue.Sound=Sound; W.Power->PowerOnCue.StartDelaySeconds=.3f;
	W.Power->SetPowerOn(false); W.Power->SetPowerOn(true);
	W.World->EndPlay(EEndPlayReason::Quit);
	W.Button->Tick(10); W.Axis->Tick(10); W.Panel->Tick(10); W.Knob->Tick(10); W.Wheel->Tick(10); W.Box->Tick(10); W.Mirror->Tick(10); W.Power->TickComponent(10,LEVELTICK_All,nullptr);
	for (auto* Audio : {B,A,P,K,H,X,M,V}) { TestEqual(TEXT("EndPlay cancels pending"),Audio->Plays,0); TestTrue(TEXT("EndPlay stops immediately"),Audio->Stops>0); TestEqual(TEXT("no fade on EndPlay"),Audio->Fades,0); }
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5sRegression, "vBoR.P5s.E12RegressionDefaults", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5sRegression::RunTest(const FString&)
{
	FP5sWorld W; W.Play(); TestNull(TEXT("SDK no second Select playback"),W.Button->PointerAudio->GetSelectAudio());
	TestEqual(TEXT("P5r warning repeat unchanged"),W.Gate->WarningCue.RepeatSeconds,.5f);
	TestEqual(TEXT("P5r accepted default silent"),W.Gate->OpenAcceptedCue.Sound.Get(),static_cast<USoundBase*>(nullptr));
	return true;
}
#endif
