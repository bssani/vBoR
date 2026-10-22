#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/Grabbable/IsdkTransformer.h"
#include "Interaction/Grabbable/IsdkGrabTransformerComponent.h"
#include "VBoRDoorMotion.h"
#include "VBoRSpectatorPart.h"
#include "VBoRSoundCue.h"
#include "VBoRHingedPanel.generated.h"

class UStaticMeshComponent;
class UBoxComponent;
class UIsdkGrabbableComponent;
class UIsdkGrabberComponent;
class AVBoRHandRigPawn;
class UMaterialInstanceDynamic;
class UAudioComponent;
struct FIsdkInteractionPointerEvent;
class AVBoRHingedPanel;
class AVBoRAxisPart;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FVBoRPanelEvent, AVBoRHingedPanel*, Panel);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FVBoRDoorAngleEvent, float, AngleDeg);

UCLASS()
class VBOR_API UVBoRDoorGrabTransformer : public UIsdkGrabTransformerComponent
{
	GENERATED_BODY()
public:
	UVBoRDoorGrabTransformer();
	void ConfigureDoor();
	bool IsThrowingEnabled() const { return bIsThrowable; }
};

// ISDK selects only; return door Tick validated pose (P2 §5)
UCLASS()
class VBOR_API UVBoRDoorTransformer : public UIsdkTransformer
{
	GENERATED_BODY()
public:
	virtual void BeginTransform(const TArray<FIsdkGrabPose>& Poses, const FIsdkTargetTransform& Target) override { bActive = !Poses.IsEmpty(); }
	virtual FTransform UpdateTransform(const TArray<FIsdkGrabPose>& Poses, const FIsdkTargetTransform& Target) override;
	virtual FTransform EndTransform(const FIsdkTargetTransform& Target) override;
	virtual void UpdateConstraints(const FIsdkTargetTransform& Target) override {}
	virtual bool IsTransformerActive() const override { return bActive; }
	virtual int MaxGrabPoints() const override { return 2; }
private:
	bool bActive = false;
};

UCLASS(Blueprintable)
class VBOR_API AVBoRHingedPanel : public AActor, public IVBoRSpectatorPart
{
	GENERATED_BODY()
public:
	AVBoRHingedPanel();
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Hinged panel|Sound") FVBoRSoundCue UnlockedCue;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Hinged panel|Sound") FVBoRSoundCue LatchedCue;
	// Slot order = property order above (P5s D12)
#if WITH_DEV_AUTOMATION_TESTS
	const FVBoRSoundPlayback& GetSoundPlaybackForTest(int32 Slot) const { return SoundPlayback[Slot]; }
	UAudioComponent* GetSoundAudioForTest(int32 Slot) const { return SoundAudio[Slot]; }
	uint64 GetSoundFrameForTest() const { return SoundFrame; }
#endif

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spectator") FText SpectatorName;
	virtual FText GetSpectatorName() const override { return SpectatorName; }
	virtual bool IsSpectatorInteracting() const override { return Motion.GrabHand != INDEX_NONE || Motion.PushHand != INDEX_NONE; }

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void PreInitializeComponents() override;
	virtual void Tick(float DeltaSeconds) override;
	UFUNCTION(BlueprintCallable, Category = "Hinged panel") void ResetDoor();
	UFUNCTION(BlueprintCallable, Category = "Hinged panel|Latch") bool Unlock();
	const FVBoRDoorMotion& GetMotion() const { return Motion; }
	FString GetStateText() const;
	FString GetMeasurementText(const FVBoRDoorMetrics* Baseline = nullptr) const;
	void SetAxisHandle(int32 Handle, AVBoRAxisPart* Part);
	UFUNCTION(CallInEditor, Category="Hinged panel|Setup", meta=(DisplayName="Check setup")) void CheckSetup();
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category="Hinged panel|Setup", meta=(DisplayName="Setup check result", MultiLine="true")) FString SetupReport;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Hinged panel|Push face", meta=(DisplayName="Use push face box")) bool bUsePushFaceBox = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Hinged panel|Push face", meta=(DisplayName="Push on mesh surface")) bool bPushOnMeshSurface = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Hinged panel|Push face", meta=(Units="cm", ClampMin="0", DisplayName="Mesh thickness warning threshold")) float PushFaceWarnThicknessCm = 20;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hinged panel|Push face", meta=(DisplayName="Push face box", ToolTip="Box in HingePivot coordinates. Enable, then adjust position/size/scale. Rotation must be 0 (cm).")) TObjectPtr<UBoxComponent> PushFaceBox;
	bool BeginGrabFromAxisPart(AVBoRAxisPart* Part);
	// +Z hinge axis/+X panel edge/+Y closing face. Keep door saved names (P2c D1/D3)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hinged panel|Latch", meta = (DisplayName = "Lock when closed — open by handle/button only", ToolTip = "Closed panel ignores pushing. Unlock by grab area/connected handle or button Unlock; relock after opening then closing.")) bool bUseLatch = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hinged panel|Grab areas", meta = (DisplayName = "Use outside grab area")) bool bUseOutsideGrabArea = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hinged panel|Grab areas", meta = (DisplayName = "Use inside grab area")) bool bUseInsideGrabArea = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hinged panel", meta = (DisplayName = "Allow palm opening", ToolTip = "When off, unlocked panel accepts closing palm push only. Grab opening/closing unchanged.")) bool bAllowPalmOpening = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hinged panel|Grab areas", meta = (DisplayName = "Recess handle visual while grabbed", Units = "cm", ToolTip = "Offset/direction in grab-area local coordinates (cm). Move visual mesh only, keep grab box fixed. 0 disables visual movement.")) FVector HandleVisualPressOffsetCm = FVector::ZeroVector;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hinged panel") FTransform MeshRestFromRoot = FTransform(FQuat::Identity, FVector(55,0,52.5), FVector(1.1,0.1,1.05));
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Hinged panel") TObjectPtr<AVBoRAxisPart> OutsideAxisHandle;
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Hinged panel") TObjectPtr<AVBoRAxisPart> InsideAxisHandle;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hinged panel") TObjectPtr<USceneComponent> HingePivot;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hinged panel") TObjectPtr<UStaticMeshComponent> DoorMesh;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hinged panel", meta = (DisplayName = "Outside face grab area")) TObjectPtr<UBoxComponent> OutsideHandle;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hinged panel", meta = (DisplayName = "Inside face grab area")) TObjectPtr<UBoxComponent> InsideHandle;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hinged panel") TObjectPtr<UStaticMeshComponent> OutsideHandleMesh;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hinged panel") TObjectPtr<UStaticMeshComponent> InsideHandleMesh;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hinged panel") TObjectPtr<UIsdkGrabbableComponent> OutsideGrab;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hinged panel") TObjectPtr<UIsdkGrabbableComponent> InsideGrab;
	UPROPERTY(VisibleAnywhere, Category = "Hinged panel") TObjectPtr<UVBoRDoorGrabTransformer> OutsideTransformer;
	UPROPERTY(VisibleAnywhere, Category = "Hinged panel") TObjectPtr<UVBoRDoorGrabTransformer> InsideTransformer;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hinged panel", meta = (Units = "deg", ClampMin = "0", ClampMax = "180")) float MaxAngleDeg = 70;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hinged panel", meta = (Units = "deg/s", ClampMin = "0")) float MaxSpeedDegPerSecond = 180;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hinged panel", meta = (Units = "deg", ClampMin = "0")) float LatchAngleDeg = 2;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hinged panel", meta = (Units = "deg", ClampMin = "0")) float RearmAngleDeg = 5;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hinged panel", meta = (Units = "deg", ClampMin = "0", ClampMax = "90")) float PalmLimitDeg = 60;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hinged panel", meta = (Units = "cm", ClampMin = "0")) float MinAxisDistanceCm = 15;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hinged panel") FLinearColor LockedColor = FLinearColor(0.65, 0.65, 0.65, 0.25);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hinged panel") FLinearColor UnlockedColor = FLinearColor(0, 1, 0.25, 0.3);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hinged panel") FLinearColor PushingColor = FLinearColor(0, 0.6, 1, 0.4);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hinged panel") FLinearColor GrabbedColor = FLinearColor(0.5, 0, 1, 0.4);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hinged panel") FLinearColor PausedColor = FLinearColor(1, 0.35, 0, 0.5);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hinged panel") FLinearColor MaximumColor = FLinearColor(1, 0.75, 0, 0.4);
	UPROPERTY(BlueprintAssignable, Category = "Hinged panel") FVBoRPanelEvent OnPanelUnlocked;
	UPROPERTY(BlueprintAssignable, Category = "Hinged panel") FVBoRPanelEvent OnPanelLocked;
	UPROPERTY(BlueprintAssignable, Category = "Hinged panel") FVBoRDoorAngleEvent OnAngleChanged;
protected:
	virtual void NotifyUnlocked();
	virtual void NotifyLocked();
	void SyncSettings();
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual bool ReadHand(int32 Hand, FVBoRDoorHand& Input);
	virtual bool ReadPalmClosed(int32 Hand, bool& bClosed) const;
private:
#if WITH_DEV_AUTOMATION_TESTS
	friend struct FP5sTestAccess;
#endif
	UPROPERTY() TArray<TObjectPtr<UAudioComponent>> SoundAudio;
	FVBoRSoundPlayback SoundPlayback[2];
	uint64 SoundFrame = 0;
	void AdvanceSounds(double Seconds);

	friend struct FP2bTestAccess;
	friend struct FP2cTestAccess;
	friend struct FP2dTestAccess;
	friend struct FP2eTestAccess;
	friend struct FP2gTestAccess;
	struct FSetupAssessment
	{
		AVBoRAxisPart* Handles[2] = {nullptr, nullptr};
		TArray<FString> Problems;
		TArray<FString> Notices;
		bool bMeshTraceable = false;
	};
	FSetupAssessment EvaluateSetup() const;
	void CollectHandleCandidates(TArray<AVBoRAxisPart*> (&Candidates)[2]) const;
	TArray<AVBoRAxisPart*> GetHandleClaims() const;
	bool IsHandleShared(const AVBoRAxisPart* Part) const;
	bool CanConnectAxisHandle(AVBoRAxisPart* Part, TArray<FString>& Problems) const;
	void ApplyAxisHandle(int32 Handle, AVBoRAxisPart* Part);
	FBox MeshFaceBounds() const;
	void ConfigurePushCollision();
	bool QueryMeshFace(const FVector& DoorPoint, int32 Face, double& OutY) const;
	bool HasTraceableMesh() const;
	bool bMeshPushReady = false;
	UPROPERTY() TObjectPtr<AVBoRAxisPart> HoldingAxisPart;
	int32 AxisHoldHand = INDEX_NONE, AxisHoldHandle = INDEX_NONE;
	double AxisHoldStartSeconds = 0;
	void EndAxisHold(const TCHAR* Reason, bool bLatch);
	bool BeginAxisGrab(AVBoRAxisPart* Part, const FVBoRDoorHand& Input);
	UPROPERTY() TObjectPtr<UVBoRDoorTransformer> DoorTransformer;
	UPROPERTY() TObjectPtr<AVBoRHandRigPawn> HandPawn;
	UPROPERTY() TObjectPtr<UIsdkGrabberComponent> HoldingGrabber;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> StateOverlay;
	UPROPERTY() TObjectPtr<UAudioComponent> StateAudio;
	TSet<TWeakObjectPtr<UIsdkGrabberComponent>> Hovering[2];
	bool bNeedsPalmOpen[2] = {false, false};
	bool bCancelPending = false;
	FVBoRDoorMotion Motion;
	FQuat ClosedHingeRotation = FQuat::Identity;
	FTransform HingeFrame() const;
	void ApplyState(double BeforeAngle, int32 BeforeUnlocks, int32 BeforeLatches);
	void CancelSelections(const TCHAR* Reason);
	UFUNCTION() void HandleOutsidePointer(const FIsdkInteractionPointerEvent& Event);
	UFUNCTION() void HandleInsidePointer(const FIsdkInteractionPointerEvent& Event);
	void HandlePointer(int32 Handle, const FIsdkInteractionPointerEvent& Event);
	bool IsGrabAreaEnabled(int32 Handle) const;
	void UpdateGrabAreas();
	FVector HandleVisualRestCm[2] = {};
	void UpdateHandleVisuals();
};
