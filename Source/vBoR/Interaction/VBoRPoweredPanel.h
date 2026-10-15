#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "InputCoreTypes.h"
#include "Interaction/Pointable/IsdkInteractionPointerEvent.h"
#include "VBoRPoweredPanel.generated.h"

class AVBoRPokeButton;
class AVBoRCarryObject;
class UStaticMeshComponent;
class UAudioComponent;
class USoundBase;
class AVBoRPoweredPanel;

UENUM(BlueprintType)
enum class EVBoRPoweredState : uint8 { Closed, OpeningWait, Opening, Open, ClosingWait, Closing };

// 입력 접수와 대기 시간만 분리함 (포인터 callback 에서 이동·SDK 변경 방지, P4b D3·E11)
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
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void Tick(float DeltaSeconds) override;
	UFUNCTION(BlueprintCallable, Category = "Powered Panel") void RequestOpen();
	UFUNCTION(BlueprintCallable, Category = "Powered Panel") void RequestClose();
	UFUNCTION(BlueprintCallable, Category = "Powered Panel") void ResetPanel();
	UFUNCTION(BlueprintPure, Category = "Powered Panel") EVBoRPoweredState GetState() const { return Motion.State; }
	UFUNCTION(BlueprintPure, Category = "Powered Panel") float GetAngleDeg() const { return Motion.AngleDeg; }
	FString GetStateText() const;
	int32 GetObstructionCount() const { return ObstructionCount; }
	int32 GetWarningRequests() const { return WarningRequests; }
	double GetWaitRemaining() const { return Motion.WaitRemaining; }

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Powered Panel") TObjectPtr<USceneComponent> HingePivot;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Powered Panel") TObjectPtr<UStaticMeshComponent> PanelMesh;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Powered Panel") TObjectPtr<UAudioComponent> WarningAudio;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Powered Panel") TObjectPtr<AVBoRPokeButton> OpenButton;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Powered Panel") TObjectPtr<AVBoRPokeButton> CloseButton;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Powered Panel", meta = (Units = "cm")) FVector HingeLocationCm = FVector(0, 0, 145);
	// Actor 원점에서의 닫힌 메시 자세. 메시 피벗이 차 원점이어도 자리를 보존함 (P4b D1)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Powered Panel") FTransform MeshRestFromRoot = FTransform(FQuat::Identity, FVector(-2.5, 0, 110), FVector(0.05, 1.3, 0.7));
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Powered Panel", meta = (Units = "deg", ClampMin = "1", ClampMax = "170")) float OpenAngleDeg = 85;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Powered Panel", meta = (Units = "deg/s", ClampMin = "0.1")) float SpeedDegPerSecond = 21.25f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Powered Panel", meta = (Units = "s", ClampMin = "0")) float DelaySeconds = 1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Powered Panel") bool bStartOpen = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Powered Panel|Warning") TObjectPtr<USoundBase> WarningSound;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Powered Panel|Warning", meta = (Units = "s", ClampMin = "0.05")) float WarningIntervalSeconds = 0.5f;
	// 게이트 축 기준 cm·deg. 버튼 윗면 +Z 를 사용자 쪽 -X 로 향하게 둠 (P4b D6)
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
	FVBoRPoweredMotion Motion;
	bool bPendingCommand = false;
	double WarningElapsed = 0;
	int32 WarningRequests = 0;
	int32 ObstructionCount = 0;
	void Request(bool bOpen);
	void PublishState();
	void ApplyAngle();
	void WarnFor(double Seconds);
	FTransform MeshPoseAt(float Angle) const;
	AVBoRCarryObject* FindObstruction(float FromAngle, float ToAngle) const;
	AVBoRPokeButton* CreateButton(const FTransform& Relative);
	UFUNCTION() void OpenPressed(const FIsdkInteractionPointerEvent& Event);
	UFUNCTION() void ClosePressed(const FIsdkInteractionPointerEvent& Event);
};
