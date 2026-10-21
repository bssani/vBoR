#pragma once

#include "CoreMinimal.h"
#include "VBoRSoundCue.generated.h"

class UAudioComponent;
class USoundBase;
class USoundAttenuation;

USTRUCT(BlueprintType)
struct VBOR_API FVBoRSoundCue
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sound", meta = (ToolTip = "Sound played for this event. Empty means silent.")) TObjectPtr<USoundBase> Sound;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sound", meta = (Units = "s", ClampMin = "0", ToolTip = "Seconds after the event before playback. Accepted commands start counting on the next Tick.")) float StartDelaySeconds = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sound", meta = (ClampMin = "0", ToolTip = "Volume multiplier applied when playback starts.")) float VolumeMultiplier = 1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sound", meta = (ClampMin = "0.001", ToolTip = "Positive pitch multiplier applied when playback starts.")) float PitchMultiplier = 1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sound", meta = (ToolTip = "Optional spatial attenuation. Empty preserves the audio component and sound asset settings.")) TObjectPtr<USoundAttenuation> Attenuation;
};

USTRUCT(BlueprintType)
struct VBOR_API FVBoRHeldSoundCue : public FVBoRSoundCue
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sound", meta = (Units = "s", ClampMin = "0", ToolTip = "Seconds between requests after the first playback. Zero plays once, including looping assets. Positive values below 0.05 s use 0.05 s.")) float RepeatSeconds = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sound", meta = (Units = "s", ClampMin = "0", ToolTip = "Seconds to fade out when the state ends. Zero stops immediately. Reset always stops immediately.")) float FadeOutSeconds = 0;
};

// 칸 하나의 예약만 보관함 (장치·타이머 없이 Tick 시간과 요청을 함께 검증, P5r D2·D3)
struct VBOR_API FVBoRSoundPlayback
{
	int32 Requests = 0;
	void Queue(const FVBoRSoundCue& Cue, UAudioComponent* Audio, uint64 Frame);
	void BeginHeld(const FVBoRHeldSoundCue& Cue, UAudioComponent* Audio, uint64 Frame);
	void Advance(const FVBoRSoundCue& Cue, UAudioComponent* Audio, double Seconds, uint64 Frame);
	void EndHeld(const FVBoRHeldSoundCue& Cue, UAudioComponent* Audio);
	void Reset(UAudioComponent* Audio, bool bClearCounts);
#if WITH_DEV_AUTOMATION_TESTS
	TArray<uint64> RequestFrames;
	int32 StopRequests = 0;
	float LastFadeOutSeconds = 0;
#endif
private:
	TArray<double> Pending;
	double HeldRemaining = 0;
	double RepeatSeconds = 0;
	bool bHeld = false;
	bool bHeldPlayed = false;
	uint64 LastPlayFrame = MAX_uint64;
	void Play(const FVBoRSoundCue& Cue, UAudioComponent* Audio, int32 Count, uint64 Frame);
};
