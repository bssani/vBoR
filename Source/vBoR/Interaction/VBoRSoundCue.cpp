#include "VBoRSoundCue.h"

#include "Components/AudioComponent.h"

void FVBoRSoundPlayback::Play(const FVBoRSoundCue& Cue, UAudioComponent* Audio, int32 Count, uint64 Frame, bool bCoalesce)
{
	if (!Cue.Sound || Count <= 0) { return; }
	Requests += Count;
#if WITH_DEV_AUTOMATION_TESTS
	for (int32 I = 0; I < Count; ++I) { RequestFrames.Add(Frame); }
#endif
	// Advance 의 긴 frame 만 합침. 지연 0 이벤트는 호출마다 재생함 (P5s D2)
	if (!Audio || (bCoalesce && LastPlayFrame == Frame)) { return; }
	if (bCoalesce) { LastPlayFrame = Frame; }
	Audio->SetSound(Cue.Sound);
	Audio->SetVolumeMultiplier(FMath::Max(0.0f, Cue.VolumeMultiplier));
	Audio->SetPitchMultiplier(FMath::Max(0.001f, Cue.PitchMultiplier));
	if (Cue.Attenuation) { Audio->SetAttenuationSettings(Cue.Attenuation); }
	Audio->Play();
}

void FVBoRSoundPlayback::Queue(const FVBoRSoundCue& Cue, UAudioComponent* Audio, uint64 Frame)
{
	if (!Cue.Sound) { return; }
	if (Cue.StartDelaySeconds <= 0) { Play(Cue, Audio, 1, Frame, false); }
	else { Pending.Add(Cue.StartDelaySeconds); }
}

void FVBoRSoundPlayback::BeginHeld(const FVBoRHeldSoundCue& Cue, UAudioComponent* Audio, uint64 Frame)
{
	bHeld = Cue.Sound != nullptr;
	bHeldPlayed = false;
	HeldRemaining = FMath::Max(0.0f, Cue.StartDelaySeconds);
	RepeatSeconds = Cue.RepeatSeconds > 0 ? FMath::Max(0.05f, Cue.RepeatSeconds) : 0;
	Advance(Cue, Audio, 0, Frame);
}

void FVBoRSoundPlayback::Advance(const FVBoRSoundCue& Cue, UAudioComponent* Audio, double Seconds, uint64 Frame)
{
	Seconds = FMath::Max(0.0, Seconds);
	int32 Due = 0;
	for (int32 I = Pending.Num() - 1; I >= 0; --I)
	{
		Pending[I] -= Seconds;
		if (Pending[I] <= UE_DOUBLE_SMALL_NUMBER) { Pending.RemoveAt(I); ++Due; }
	}
	if (bHeld && (!bHeldPlayed || RepeatSeconds > 0))
	{
		HeldRemaining -= Seconds;
		if (HeldRemaining <= UE_DOUBLE_SMALL_NUMBER)
		{
			const int32 Count = RepeatSeconds > 0 ? 1 + FMath::FloorToInt((-HeldRemaining + UE_DOUBLE_SMALL_NUMBER) / RepeatSeconds) : 1;
			Due += Count;
			HeldRemaining += Count * RepeatSeconds;
			bHeldPlayed = true;
		}
	}
	Play(Cue, Audio, Due, Frame);
}

void FVBoRSoundPlayback::EndHeld(const FVBoRHeldSoundCue& Cue, UAudioComponent* Audio)
{
	bHeld = false;
	HeldRemaining = 0;
	if (!bHeldPlayed) { return; }
	bHeldPlayed = false;
	const float Fade = FMath::Max(0.0f, Cue.FadeOutSeconds);
#if WITH_DEV_AUTOMATION_TESTS
	++StopRequests; LastFadeOutSeconds = Fade;
#endif
	if (Audio)
	{
		if (Fade > 0) { Audio->FadeOut(Fade, 0); }
		else { Audio->Stop(); }
	}
}

void FVBoRSoundPlayback::Reset(UAudioComponent* Audio, bool bClearCounts)
{
	Pending.Reset(); bHeld = false; bHeldPlayed = false;
	HeldRemaining = 0; RepeatSeconds = 0; LastPlayFrame = MAX_uint64;
	if (Audio) { Audio->Stop(); }
	if (bClearCounts) { Requests = 0; }
#if WITH_DEV_AUTOMATION_TESTS
	++StopRequests; LastFadeOutSeconds = 0;
	if (bClearCounts) { RequestFrames.Reset(); }
#endif
}
