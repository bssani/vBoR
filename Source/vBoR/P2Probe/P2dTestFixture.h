#pragma once

#include "CoreMinimal.h"
#include "P2cTestFixture.h"
#include "P2bTestFixture.h"
#include "Components/AudioComponent.h"
#include "P2dTestFixture.generated.h"

// headless 에서 소리 재생 요청만 셈 (실제 들리는 소리는 Quest 확인, P2d E5)
UCLASS()
class UP2dAudioObserver : public UAudioComponent
{
	GENERATED_BODY()
public:
	int32 Plays = 0;
	virtual void Play(float StartTime = 0) override { ++Plays; }
};

UCLASS()
class UP2dButtonObserver : public UObject
{
	GENERATED_BODY()
public:
	UPROPERTY() TObjectPtr<AVBoRHingedPanel> Panel;
	int32 Cancels = 0, Presses = 0;
	UFUNCTION() void Pressed(const FIsdkInteractionPointerEvent& Event) { ++Presses; Panel->Unlock(); }
	UFUNCTION() void Cancel(int32 Identifier, UIsdkGrabTransformerComponent* Component) { ++Cancels; }
};

#if WITH_DEV_AUTOMATION_TESTS
struct FP2dTestAccess
{
	static FVBoRDoorMotion& Motion(AVBoRHingedPanel* Panel) { return Panel->Motion; }
	static void Apply(AVBoRHingedPanel* Panel, double Before, int32 Unlocks, int32 Latches) { Panel->ApplyState(Before, Unlocks, Latches); }
	static void Advance(AVBoRHingedPanel* Panel, const FVBoRDoorHand (&Hands)[2], double Dt)
	{
		const double Before = Panel->Motion.AngleDeg;
		const int32 Unlocks = Panel->Motion.Metrics.Unlocks, Latches = Panel->Motion.Metrics.Latches;
		Panel->Motion.Advance(Hands, Dt); Panel->ApplyState(Before, Unlocks, Latches);
	}
	static void Begin(AVBoRAxisPart* Part, int32 Hand, const FIsdkGrabPose& Pose)
	{
		Part->HoldingHand = Hand; Part->Transformer->BeginTransform({Pose}, FIsdkTargetTransform(Part->AxisPivot));
	}
	static void Move(AVBoRAxisPart* Part, const FIsdkGrabPose& Pose)
	{
		Part->AxisPivot->SetRelativeTransform(Part->Transformer->UpdateTracked({Pose}, FIsdkTargetTransform(Part->AxisPivot), true));
	}
	static bool Transfer(AVBoRHingedPanel* Panel, AVBoRAxisPart* Part, const FVBoRDoorHand& Hand)
	{
		if (!Panel->BeginAxisGrab(Part, Hand)) { return false; }
		Part->bDoorHold = true; return true;
	}
	static void PalmOpen(AVBoRHingedPanel* Panel, AVBoRAxisPart* Part, int32 Hand)
	{
		Panel->bNeedsPalmOpen[Hand] = Part->bNeedsPalmOpen[Hand] = false;
	}
	static UP2dAudioObserver* ObserveAudio(AVBoRHingedPanel* Panel)
	{
		UP2dAudioObserver* Audio = NewObject<UP2dAudioObserver>(Panel);
		Audio->SetSound(Panel->StateAudio->Sound); Panel->StateAudio = Audio; for (auto& Slot : Panel->SoundAudio) { Slot = Audio; } return Audio;
	}
};
#endif
