#include "VBoRVehicleAlignmentComponent.h"
#include "VBoREyePointComponent.h"
#include "VBoRViewpointComponent.h"
#include "VBoRParticipantZones.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/WorldSettings.h"
#include "HeadMountedDisplayFunctionLibrary.h"
#include "IXRTrackingSystem.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CoreDelegates.h"
#include "Misc/Paths.h"

UVBoRVehicleAlignmentComponent::UVBoRVehicleAlignmentComponent()
{
	PrimaryComponentTick.bCanEverTick = true; PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
}

void UVBoRVehicleAlignmentComponent::BeginPlay()
{
	Super::BeginPlay();
	if (!BaseEye) { BaseEye = GetOwner()->FindComponentByClass<UVBoREyePointComponent>(); }
	RecenterHandle = FCoreDelegates::VRHeadsetRecenter.AddUObject(this, &UVBoRVehicleAlignmentComponent::HandleRecenter);
	FVBoRBuckSavedAlignment Loaded; FString LoadReason;
	const bool bLoaded = FVBoRBuckAlignmentStore::Load(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Calibration/BuckEye.json")), Loaded, LoadReason);
	int32 Origin = INDEX_NONE;
	if (GEngine && GEngine->XRSystem.IsValid()) { Origin = GEngine->XRSystem->GetTrackingOrigin(); }
	const FVector2D Bounds = UHeadMountedDisplayFunctionLibrary::GetPlayAreaBounds(EHMDTrackingOrigin::Stage);
	InitializeAlignment(UGameplayStatics::GetPlayerPawn(this, 0), bLoaded ? &Loaded : nullptr, Origin, Bounds);
	if (!bLoaded) { Reason += TEXT(" — ") + LoadReason; }
}

void UVBoRVehicleAlignmentComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	FCoreDelegates::VRHeadsetRecenter.Remove(RecenterHandle); RecenterHandle.Reset();
	if (AlignmentPawn)
	{
		if (APlayerController* PC = Cast<APlayerController>(AlignmentPawn->GetController()))
		{ if (PC->PlayerCameraManager) { PC->PlayerCameraManager->StopCameraFade(); } }
	}
	Super::EndPlay(EndPlayReason);
}

void UVBoRVehicleAlignmentComponent::InitializeAlignment(APawn* Pawn, const FVBoRBuckSavedAlignment* Value, int32 Origin, const FVector2D& PlayArea)
{
	AlignmentPawn = Pawn; bHasSaved = Value != nullptr;
	CurrentViewpoint = nullptr; PendingViewpoint = nullptr; bSwitching = false;
	State = EVBoRAlignmentState::NoSavedValue;
	if (!Value) { Reason = TEXT("저장값 없음 — P0-c 레벨에서 C로 저장"); }
	else
	{
		Saved = *Value;
		const auto Decision = FVBoRBuckAlignment::CheckEnvironment(Saved, Origin, PlayArea, PlayAreaToleranceCm);
		State = EVBoRAlignmentState::NeedsCheck; Reason = Decision.Reason;
		if (!AlignmentPawn || !BaseEye || AlignmentPawn->bUseControllerRotationYaw ||
			!FMath::IsNearlyEqual(GetWorld()->GetWorldSettings()->WorldToMeters, 100.0f))
		{ Reason = TEXT("Pawn·eye point·WorldToMeters 확인 필요"); }
		else if (Decision.bApply)
		{
			// 참가자의 현재 머리 자세를 읽지 않음 (체격 차이 유지, P3 D2)
			AlignmentPawn->SetActorTransform(FVBoRBuckAlignment::CalculatePawn(Saved.Capture, BaseEye->GetComponentTransform()), false, nullptr, ETeleportType::TeleportPhysics);
			State = Decision.bNeedsCheck ? EVBoRAlignmentState::NeedsCheck : EVBoRAlignmentState::Applied;
		}
	}
	UE_LOG(LogP3Flow, Log, TEXT("정렬 %s"), *Describe());
}

void UVBoRVehicleAlignmentComponent::HandleRecenter()
{
	State = EVBoRAlignmentState::NeedsCheck; Reason = TEXT("재중심 — 설치 정렬 확인 필요");
	UE_LOG(LogP3Flow, Log, TEXT("정렬 %s; Pawn 유지"), *Describe());
}

void UVBoRVehicleAlignmentComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (bHasSaved && State == EVBoRAlignmentState::Applied && GEngine && GEngine->XRSystem.IsValid())
	{
		const auto Check = FVBoRBuckAlignment::CheckEnvironment(Saved, GEngine->XRSystem->GetTrackingOrigin(),
			UHeadMountedDisplayFunctionLibrary::GetPlayAreaBounds(EHMDTrackingOrigin::Stage), PlayAreaToleranceCm);
		if (Check.bNeedsCheck) { State = EVBoRAlignmentState::NeedsCheck; Reason = Check.Reason; UE_LOG(LogP3Flow, Log, TEXT("정렬 %s; Pawn 유지"), *Describe()); }
	}
	AdvanceSwitch(DeltaTime);
}

bool UVBoRVehicleAlignmentComponent::SwitchViewpoint(UVBoRViewpointComponent* Viewpoint)
{
	if (!IsValid(Viewpoint)) { ++SwitchRequests; ++SwitchDiscards; UE_LOG(LogP3Flow, Log, TEXT("시점 버림 — 대상 없음")); return false; }
	return RequestSwitch(Viewpoint);
}

bool UVBoRVehicleAlignmentComponent::ReturnToBase() { return RequestSwitch(nullptr); }

bool UVBoRVehicleAlignmentComponent::RequestSwitch(UVBoRViewpointComponent* Viewpoint)
{
	++SwitchRequests;
	if (bSwitching || !bHasSaved || !AlignmentPawn || !BaseEye || State != EVBoRAlignmentState::Applied)
	{
		++SwitchDiscards;
		UE_LOG(LogP3Flow, Log, TEXT("시점 버림 — %s"), bSwitching ? TEXT("전환 중") : TEXT("정렬 필요")); return false;
	}
	PendingViewpoint = Viewpoint; bSwitching = true; bMoved = false; SwitchElapsed = 0;
	if (APlayerController* PC = Cast<APlayerController>(AlignmentPawn->GetController()))
	{ if (PC->PlayerCameraManager) { PC->PlayerCameraManager->StartCameraFade(0, 1, FadeSeconds, FLinearColor::Black, false, true); } }
	UE_LOG(LogP3Flow, Log, TEXT("시점 요청 %s"), Viewpoint ? *Viewpoint->GetName() : TEXT("기준"));
	return true;
}

void UVBoRVehicleAlignmentComponent::AdvanceSwitch(double DeltaSeconds)
{
	if (!bSwitching) { return; }
	if (State != EVBoRAlignmentState::Applied)
	{
		bSwitching = false; ++SwitchDiscards;
		if (APlayerController* PC = Cast<APlayerController>(AlignmentPawn->GetController()))
		{ if (PC->PlayerCameraManager) { PC->PlayerCameraManager->StopCameraFade(); } }
		UE_LOG(LogP3Flow, Log, TEXT("시점 버림 — 정렬 확인 필요; Pawn 유지")); return;
	}
	SwitchElapsed += FMath::Max(0.0, DeltaSeconds);
	if (!bMoved && SwitchElapsed >= FadeSeconds)
	{
		// 매번 Stage 저장값에서 직접 계산함 (전환 오차 누적 방지, P3 E11)
		const FTransform Eye = PendingViewpoint ? PendingViewpoint->GetComponentTransform() : BaseEye->GetComponentTransform();
		AlignmentPawn->SetActorTransform(FVBoRBuckAlignment::CalculatePawn(Saved.Capture, Eye), false, nullptr, ETeleportType::TeleportPhysics);
		CurrentViewpoint = PendingViewpoint; bMoved = true;
		SwitchElapsed = FadeSeconds;
		if (APlayerController* PC = Cast<APlayerController>(AlignmentPawn->GetController()))
		{ if (PC->PlayerCameraManager) { PC->PlayerCameraManager->StartCameraFade(1, 0, FadeSeconds, FLinearColor::Black, false, false); } }
		return;
	}
	if (bMoved && SwitchElapsed >= 2 * FadeSeconds)
	{
		bSwitching = false; ++SwitchCompletions;
		UE_LOG(LogP3Flow, Log, TEXT("시점 완료 %s requests=%d complete=%d discarded=%d"), CurrentViewpoint ? *CurrentViewpoint->GetName() : TEXT("기준"), SwitchRequests, SwitchCompletions, SwitchDiscards);
	}
}

FString UVBoRVehicleAlignmentComponent::Describe() const
{
	const TCHAR* Name = State == EVBoRAlignmentState::Applied ? TEXT("적용됨") : State == EVBoRAlignmentState::NoSavedValue ? TEXT("저장값 없음") : TEXT("확인 필요");
	return FString::Printf(TEXT("%s %s | 저장 %s | 시점 %s%s | 요청 %d 완료 %d 버림 %d"), Name, *Reason, *Saved.SavedAt,
		CurrentViewpoint ? *CurrentViewpoint->GetName() : TEXT("기준"), bSwitching ? TEXT(" (전환 중)") : TEXT(""), SwitchRequests, SwitchCompletions, SwitchDiscards);
}
