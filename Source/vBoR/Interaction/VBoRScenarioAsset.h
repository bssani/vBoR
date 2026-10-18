#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "VBoRScenarioAsset.generated.h"

UENUM(BlueprintType)
enum class EVBoRStepRule : uint8 { Confirm, Approach, Seated, ButtonPressed, DoorUnlocked, DoorOpened, DoorLatched, Viewpoint, KeyHeld, KeyUnlockAndLock };
UENUM(BlueprintType)
enum class EVBoRConfirmBy : uint8 { None, Participant, Operator, Either };

USTRUCT(BlueprintType)
struct VBOR_API FVBoRScenarioStep
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ToolTip="원본 시나리오 번호. 17번 없음")) FName StepId;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ToolTip="참가자에게 보이는 한국어 안내")) FText Instruction;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ToolTip="TV 관전용 영어 문구. 비어 있으면 단계 ID 기본표 사용")) FText SpectatorText;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ToolTip="단계를 완료할 현재 상태 또는 확인 규칙")) EVBoRStepRule Rule = EVBoRStepRule::Confirm;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ToolTip="규칙 대상의 Actor 또는 Component 태그. 하나만 일치해야 함")) FName TargetTag;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(Units="deg", ClampMin="0", ToolTip="문 열림 목표 각도. 합격 기준이 아닌 자리값")) float DoorOpenDeg = 45;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(Units="s", ClampMin="0", ToolTip="추적된 머리가 좌석 안에 머물 시간. 자리값")) float SeatedSeconds = 1.5;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ToolTip="규칙 만족 뒤 확인할 사람. 관찰은 반드시 확인 필요")) EVBoRConfirmBy ConfirmBy = EVBoRConfirmBy::Either;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ToolTip="운전석 기준 시점으로 복귀해야 완료 가능")) bool bRequireBaseViewpoint = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ToolTip="바닥 고리와 화살표를 표시할 대상 태그. 생략 가능")) FName MarkerTag;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ToolTip="현재 단계의 바닥 경로 Actor 태그. 생략 가능 (P3b D2)")) FName RouteTag;
};

UCLASS(BlueprintType)
class VBOR_API UVBoRScenarioAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()
public:
	UVBoRScenarioAsset();
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="안내", meta=(ToolTip="이 배열의 순서대로 안내함. 추가·삭제·순서·문구 편집 가능")) TArray<FVBoRScenarioStep> Steps;
};
