#pragma once

#include "CoreMinimal.h"
#include "Components/AudioComponent.h"
#include "../Interaction/VBoRPoweredPanel.h"
#include "P5rTestFixture.generated.h"

// 실제 오디오 장치 없이 Play·Stop·FadeOut 전달을 확인함 (P5r D3·E6)
UCLASS()
class UP5rAudioObserver : public UAudioComponent
{
	GENERATED_BODY()
public:
#if WITH_DEV_AUTOMATION_TESTS
	int32 Plays = 0, Stops = 0, Fades = 0;
	float FadeSeconds = 0;
	// SetSound 내부 Stop 을 제외함. 관찰자는 IsPlaying 을 켜지 않아 Play 당 정확히 한 번임 (UE AudioComponent.cpp:326-350)
	int32 LifecycleStops() const { return Stops - Plays; }
	virtual void Play(float StartTime = 0) override { ++Plays; }
	virtual void Stop() override { ++Stops; }
	virtual void FadeOut(float Duration, float Volume, EAudioFaderCurve Curve = EAudioFaderCurve::Linear) override { ++Fades; FadeSeconds = Duration; }
#endif
};

#if WITH_DEV_AUTOMATION_TESTS
struct FP5rTestAccess
{
	static UP5rAudioObserver* Observe(AVBoRPoweredPanel* Gate, EVBoRPoweredSound Slot)
	{
		UP5rAudioObserver* Audio = NewObject<UP5rAudioObserver>(Gate);
		Gate->SoundAudio[static_cast<int32>(Slot)] = Audio;
		if (Slot == EVBoRPoweredSound::Warning) { Gate->WarningAudio = Audio; }
		return Audio;
	}
};
#endif
