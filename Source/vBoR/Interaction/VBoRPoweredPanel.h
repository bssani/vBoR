#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "InputCoreTypes.h"
#include "Interaction/Pointable/IsdkInteractionPointerEvent.h"
#include "VBoRSoundCue.h"
#include "VBoRPoweredPanel.generated.h"

class AVBoRPokeButton;
class AVBoRCarryObject;
class UStaticMeshComponent;
class UAudioComponent;
class USoundBase;
class AVBoRPoweredPanel;

enum class EVBoRPoweredSound : uint8 { OpenAccepted, CloseAccepted, Warning, Motor, Opened, Closed, Obstructed, Count };

UENUM(BlueprintType)
enum class EVBoRPoweredState : uint8 { Closed, OpeningWait, Opening, Open, ClosingWait, Closing };

// Separate acceptance from delay only (no motion/SDK changes in pointer callback, P4b D3/E11)
struct FVBoRPoweredMotion
{
	EVBoRPoweredState State = EVBoRPoweredState::Closed;
	double WaitRemaining = 0;
	float AngleDeg = 0;
	bool Request(bool bOpen, double DelaySeconds)
	{
		if (State != (bOpen ? EVBoRPoweredState::Closed : EVBoRPoweredState::Open)) { return false; }
		State = bOpen ? EVBoRPoweredState::OpeningWait : EVBoRPoweredState::ClosingWait;
		WaitRemaining = FMath::Max(0.0, DelaySeconds);
		return true;
	}
	bool IsWaiting() const { return State == EVBoRPoweredState::OpeningWait || State == EVBoRPoweredState::ClosingWait; }
	bool IsActive() const { return State != EVBoRPoweredState::Closed && State != EVBoRPoweredState::Open; }
	bool ConsumeWait(double& Seconds)
	{
		const double Used = FMath::Min(Seconds, WaitRemaining);
		WaitRemaining -= Used; Seconds -= Used;
		return WaitRemaining <= UE_DOUBLE_SMALL_NUMBER;
	}
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FVBoRPoweredStateEvent, AVBoRPoweredPanel*, Panel, EVBoRPoweredState, State);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FVBoRPoweredEvent, AVBoRPoweredPanel*, Panel);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FVBoRPoweredObstructionEvent, AVBoRPoweredPanel*, Panel, AVBoRCarryObject*, Box);

UCLASS(Blueprintable)
class VBOR_API AVBoRPoweredPanel : public AActor
{
	GENERATED_BODY()
public:
	AVBoRPoweredPanel();
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Powered Panel|Sound") FVBoRSoundCue ButtonPressedCue;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Powered Panel|Sound") FVBoRSoundCue ButtonReleasedCue;
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void Tick(float DeltaSeconds) override;
	UFUNCTION(BlueprintCallable, Category = "Powered Panel") void RequestOpen();
	UFUNCTION(BlueprintCallable, Category = "Powered Panel") void RequestClose();
	UFUNCTION(BlueprintCallable, Category = "Powered Panel") void ResetPanel(bool bClearCounts = true);
	UFUNCTION(BlueprintPure, Category = "Powered Panel") EVBoRPoweredState GetState() const { return Motion.State; }
	UFUNCTION(BlueprintPure, Category = "Powered Panel") float GetAngleDeg() const { return Motion.AngleDeg; }
	FString GetStateText() const;
	int32 GetObstructionCount() const { return ObstructionCount; }
	int32 GetWarningRequests() const { return SoundPlayback[static_cast<int32>(EVBoRPoweredSound::Warning)].Requests; }
#if WITH_DEV_AUTOMATION_TESTS
	const FVBoRSoundPlayback& GetSoundPlaybackForTest(EVBoRPoweredSound Slot) const { return SoundPlayback[static_cast<int32>(Slot)]; }
	UAudioComponent* GetSoundAudioForTest(EVBoRPoweredSound Slot) const { return SoundAudio[static_cast<int32>(Slot)]; }
	uint64 GetSoundFrameForTest() const { return SoundFrame; }
#endif
	double GetWaitRemaining() const { return Motion.WaitRemaining; }

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Powered Panel") TObjectPtr<USceneComponent> HingePivot;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Powered Panel") TObjectPtr<UStaticMeshComponent> PanelMesh;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Powered Panel") TObjectPtr<UAudioComponent> WarningAudio;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Powered Panel") TObjectPtr<AVBoRPokeButton> OpenButton;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Powered Panel") TObjectPtr<AVBoRPokeButton> CloseButton;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Powered Panel", meta = (Units = "cm")) FVector HingeLocationCm = FVector(0, 0, 145);
	// Closed mesh pose at Actor origin. Preserve car-origin mesh pivot position (P4b D1)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Powered Panel") FTransform MeshRestFromRoot = FTransform(FQuat::Identity, FVector(-2.5, 0, 110), FVector(0.05, 1.3, 0.7));
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Powered Panel", meta = (Units = "deg", ClampMin = "1", ClampMax = "170")) float OpenAngleDeg = 85;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Powered Panel", meta = (Units = "deg/s", ClampMin = "0.1")) float SpeedDegPerSecond = 21.25f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Powered Panel", meta = (Units = "s", ClampMin = "0")) float DelaySeconds = 1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Powered Panel") bool bStartOpen = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Powered Panel|Sound", meta = (ToolTip = "Open request accepted: starts on the next Tick.")) FVBoRSoundCue OpenAcceptedCue;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Powered Panel|Sound", meta = (ToolTip = "Close request accepted: starts on the next Tick.")) FVBoRSoundCue CloseAcceptedCue;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Powered Panel|Sound", meta = (ToolTip = "From the next Tick after acceptance until open or closed arrival, including wait and motion.")) FVBoRHeldSoundCue WarningCue;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Powered Panel|Sound", meta = (ToolTip = "From motion start until arrival. Continues through obstruction reversal.")) FVBoRHeldSoundCue MotorCue;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Powered Panel|Sound", meta = (ToolTip = "Starts in the OnOpened arrival frame.")) FVBoRSoundCue OpenedCue;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Powered Panel|Sound", meta = (ToolTip = "Starts in the OnClosed arrival frame.")) FVBoRSoundCue ClosedCue;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Powered Panel|Sound", meta = (ToolTip = "Starts in the OnObstructed detection frame.")) FVBoRSoundCue ObstructedCue;
	// Button Actor origin = face center, +Z outward (P5zc D6)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Powered Panel|Buttons", meta = (DisplayName = "Open button class", ToolTip = "Empty = default cube button. Set = this BP's mesh/size/material/press depth kept.")) TSubclassOf<AVBoRPokeButton> OpenButtonClass;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Powered Panel|Buttons", meta = (DisplayName = "Close button class", ToolTip = "Empty = default cube button. Set = this BP's mesh/size/material/press depth kept.")) TSubclassOf<AVBoRPokeButton> CloseButtonClass;
	// Gate-axis cm/deg. Button top +Z faces user -X (P4b D6)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Powered Panel|Buttons") FTransform OpenButtonFromHinge = FTransform(FRotator(90, 0, 0), FVector(-6, 20, -60));
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Powered Panel|Buttons") FTransform CloseButtonFromHinge = FTransform(FRotator(90, 0, 0), FVector(-6, -20, -60));
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Powered Panel|Buttons", meta = (Units = "cm")) FVector ButtonSizeCm = FVector(3, 3, 1);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Powered Panel|Input") FKey OpenKey = EKeys::PageUp;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Powered Panel|Input") FKey CloseKey = EKeys::PageDown;
	UPROPERTY(BlueprintAssignable, Category = "Powered Panel") FVBoRPoweredStateEvent OnStateChanged;
	UPROPERTY(BlueprintAssignable, Category = "Powered Panel") FVBoRPoweredEvent OnOpened;
	UPROPERTY(BlueprintAssignable, Category = "Powered Panel") FVBoRPoweredEvent OnClosed;
	UPROPERTY(BlueprintAssignable, Category = "Powered Panel") FVBoRPoweredObstructionEvent OnObstructed;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
#if WITH_DEV_AUTOMATION_TESTS
	friend struct FP5rTestAccess;
#endif
	FVBoRPoweredMotion Motion;
	bool bPendingCommand = false;
	UPROPERTY() TArray<TObjectPtr<UAudioComponent>> SoundAudio;
	FVBoRSoundPlayback SoundPlayback[static_cast<int32>(EVBoRPoweredSound::Count)];
	uint64 SoundFrame = 0;
	int32 ObstructionCount = 0;
	void Request(bool bOpen);
	void PublishState();
	void ApplyAngle();
	const FVBoRSoundCue& GetSoundCue(EVBoRPoweredSound Slot) const;
	void QueueSound(EVBoRPoweredSound Slot);
	void BeginHeldSound(EVBoRPoweredSound Slot, const FVBoRHeldSoundCue& Cue);
	void AdvanceSounds(double Seconds, bool bIncludeHeld = true);
	void StopHeldSounds();
	FTransform MeshPoseAt(float Angle) const;
	AVBoRCarryObject* FindObstruction(float FromAngle, float ToAngle) const;
	AVBoRPokeButton* CreateButton(const FTransform& Relative, TSubclassOf<AVBoRPokeButton> ButtonClass, const TCHAR* Slot);
	UFUNCTION() void OpenPressed(const FIsdkInteractionPointerEvent& Event);
	UFUNCTION() void ClosePressed(const FIsdkInteractionPointerEvent& Event);
};
