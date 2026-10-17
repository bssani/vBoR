#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/Grabbable/IsdkTransformer.h"
#include "Interaction/Grabbable/IsdkGrabTransformerComponent.h"
#include "VBoRDoorMotion.h"
#include "VBoRSpectatorPart.h"
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

// ISDK 는 선택만 맡고 문 Tick 의 검증된 자세를 반환함 (P2 §5)
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
	UFUNCTION(CallInEditor, Category="Hinged panel|Setup", meta=(DisplayName="설정 검사")) void CheckSetup();
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category="Hinged panel|Setup", meta=(DisplayName="설정 검사 결과", MultiLine="true")) FString SetupReport;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Hinged panel|Push face", meta=(DisplayName="미는 면 상자 사용")) bool bUsePushFaceBox = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Hinged panel|Push face", meta=(Units="cm", ClampMin="0", DisplayName="메시 두께 경고 기준")) float PushFaceWarnThicknessCm = 20;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hinged panel|Push face", meta=(DisplayName="미는 면 상자", ToolTip="HingePivot 좌표의 상자. 사용을 켠 뒤 위치·크기·scale을 조절함. 회전은 0이어야 함 (cm).")) TObjectPtr<UBoxComponent> PushFaceBox;
	bool BeginGrabFromAxisPart(AVBoRAxisPart* Part);
	// +Z 경첩 축·+X 판 끝·+Y 닫는 면. 문의 저장 이름 유지함 (P2c D1·D3)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hinged panel|Latch", meta = (DisplayName = "닫히면 잠김 — 손잡이·버튼으로만 열기", ToolTip = "닫힌 판은 밀어도 움직이지 않음. 잡는 자리·연결한 손잡이 또는 버튼의 Unlock으로 풀고, 열었다 닫으면 다시 잠김.")) bool bUseLatch = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hinged panel|Grab areas", meta = (DisplayName = "바깥쪽 잡는 자리 사용")) bool bUseOutsideGrabArea = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hinged panel|Grab areas", meta = (DisplayName = "안쪽 잡는 자리 사용")) bool bUseInsideGrabArea = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hinged panel", meta = (DisplayName = "손바닥으로 열기 허용", ToolTip = "끄면 풀린 판도 닫는 방향의 손바닥 밀기만 받음. 잡아서 여닫기는 그대로.")) bool bAllowPalmOpening = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hinged panel|Grab areas", meta = (DisplayName = "잡는 동안 손잡이 표시 들어감", Units = "cm", ToolTip = "잡는 자리 로컬 좌표의 이동 거리·방향 (cm). 표시 메시만 움직이고 잡는 상자는 그대로. 0이면 표시 이동 없음.")) FVector HandleVisualPressOffsetCm = FVector::ZeroVector;
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
	friend struct FP2bTestAccess;
	friend struct FP2cTestAccess;
	friend struct FP2dTestAccess;
	friend struct FP2eTestAccess;
	struct FSetupAssessment
	{
		AVBoRAxisPart* Handles[2] = {nullptr, nullptr};
		TArray<FString> Problems;
	};
	FSetupAssessment EvaluateSetup() const;
	void CollectHandleCandidates(TArray<AVBoRAxisPart*> (&Candidates)[2]) const;
	TArray<AVBoRAxisPart*> GetHandleClaims() const;
	bool IsHandleShared(const AVBoRAxisPart* Part) const;
	void RememberHandleClaims();
	void ApplyAxisHandle(int32 Handle, AVBoRAxisPart* Part);
	FBox MeshFaceBounds() const;
	// 칸을 비운 뒤에도 최초 소유를 남김 (BeginPlay 순서로 중복 소유 판정이 바뀌지 않게, P2e E10)
	TArray<TWeakObjectPtr<AVBoRAxisPart>> InitialHandleClaims;
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
