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
	AlignmentPawn = UGameplayStatics::GetPlayerPawn(this, 0);
	LoadSetupAlignment();
}

FString UVBoRVehicleAlignmentComponent::SavedAlignmentPath() const
{ return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Calibration/BuckEye.json")); }

FTransform UVBoRVehicleAlignmentComponent::PawnFor(const FTransform& Eye) const
{
	// 끄면 저장 때 머리 높이를 그대로 씀 (지그에 헤드셋을 올린 U-07 원래 방식)
	return bHeightFromFloor ? FVBoRBuckAlignment::CalculatePawnOnFloor(Saved.Capture, Eye, BaseEye->GetComponentTransform(), FloorWorldZCm)
		: FVBoRBuckAlignment::CalculatePawn(Saved.Capture, Eye);
}

void UVBoRVehicleAlignmentComponent::LoadSetupAlignment()
{
	FVBoRBuckSavedAlignment Loaded; FString LoadReason;
	const bool bLoaded = FVBoRBuckAlignmentStore::Load(SavedAlignmentPath(), Loaded, LoadReason);
	int32 Origin = INDEX_NONE;
	if (GEngine && GEngine->XRSystem.IsValid()) { Origin = GEngine->XRSystem->GetTrackingOrigin(); }
	const FVector2D Bounds = UHeadMountedDisplayFunctionLibrary::GetPlayAreaBounds(EHMDTrackingOrigin::Stage);
	InitializeAlignment(AlignmentPawn, bLoaded ? &Loaded : nullptr, Origin, Bounds);
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
	CurrentViewpoint = nullptr; PendingViewpoint = nullptr; bSwitching = false; bReapplying = false;
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
			AlignmentPawn->SetActorTransform(PawnFor(BaseEye->GetComponentTransform()), false, nullptr, ETeleportType::TeleportPhysics);
			HomeShiftCm = FVector2D::ZeroVector;
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
	if (!SwitchRejection(Viewpoint).IsEmpty())
	{
		++SwitchRequests; ++SwitchDiscards;
		UE_LOG(LogP3Flow, Log, TEXT("시점 버림 — %s"), !IsValid(Viewpoint) ? TEXT("대상 없음") : bSwitching ? TEXT("전환 중") : TEXT("정렬 필요")); return false;
	}
	return RequestSwitch(Viewpoint);
}

bool UVBoRVehicleAlignmentComponent::ReturnToBase() { return RequestSwitch(nullptr); }

bool UVBoRVehicleAlignmentComponent::MoveHomeShift(FVector2D DeltaCm)
{
	if (bSwitching || !AlignmentPawn || DeltaCm.IsZero()) { return false; }
	// 수평 차 축만 씀 (집 시험 중 높이·회전 유지, P3c D2·D3, cm)
	const FVector Delta = GetOwner()->GetActorForwardVector().GetSafeNormal2D() * DeltaCm.X +
		GetOwner()->GetActorRightVector().GetSafeNormal2D() * DeltaCm.Y;
	AlignmentPawn->SetActorLocation(AlignmentPawn->GetActorLocation() + Delta, false, nullptr, ETeleportType::TeleportPhysics);
	HomeShiftCm += DeltaCm;
	if (APlayerController* PC = Cast<APlayerController>(AlignmentPawn->GetController()))
	{ if (PC->PlayerCameraManager) { PC->PlayerCameraManager->StartCameraFade(1, 0, FadeSeconds, FLinearColor::Black, false, false); } }
	OnPawnMoved.Broadcast();
	return true;
}

bool UVBoRVehicleAlignmentComponent::ResetHomeShift()
{
	if (!MoveHomeShift(-HomeShiftCm)) { return false; }
	HomeShiftCm = FVector2D::ZeroVector;
	return true;
}

FString UVBoRVehicleAlignmentComponent::SwitchRejection(const UVBoRViewpointComponent* Viewpoint, bool bReturnBase) const
{
	if (!bReturnBase && !IsValid(Viewpoint)) { return TEXT("Seat not found"); }
	if (bSwitching) { return TEXT("View transition in progress"); }
	if (!bHasSaved || !AlignmentPawn || !BaseEye || State != EVBoRAlignmentState::Applied) { return TEXT("Saved alignment required"); }
	return FString();
}

FString UVBoRVehicleAlignmentComponent::ReapplyRejection() const
{
	if (bSwitching) { return TEXT("View transition in progress"); }
	if (!IsBaseViewpoint()) { return TEXT("Return to the driver seat first"); }
	return FString();
}

bool UVBoRVehicleAlignmentComponent::RequestReapply()
{
	if (!ReapplyRejection().IsEmpty()) { return false; }
	bReapplying = true; bSwitching = true; bMoved = false; SwitchElapsed = 0;
	if (AlignmentPawn)
	{ if (APlayerController* PC = Cast<APlayerController>(AlignmentPawn->GetController()))
		{ if (PC->PlayerCameraManager) { PC->PlayerCameraManager->StartCameraFade(0, 1, FadeSeconds, FLinearColor::Black, false, true); } } }
	return true;
}

FString UVBoRVehicleAlignmentComponent::EnglishReason() const
{
	if (Reason.StartsWith(TEXT("저장값 없음"))) { return TEXT("Save the Buck alignment in P0-c"); }
	if (Reason.StartsWith(TEXT("재중심"))) { return TEXT("Recenter detected; check the setup alignment"); }
	if (Reason.StartsWith(TEXT("Pawn"))) { return TEXT("Check Pawn, eye point and WorldToMeters"); }
	return Reason;
}

FString UVBoRVehicleAlignmentComponent::SeatName() const
{
	if (bSwitching) { return TEXT("Switching…"); }
	if (!CurrentViewpoint) { return TEXT("Driver"); }
	return CurrentViewpoint->ComponentHasTag(TEXT("Car.PassengerView")) ? TEXT("Passenger") : TEXT("Rear");
}

bool UVBoRVehicleAlignmentComponent::RequestSwitch(UVBoRViewpointComponent* Viewpoint)
{
	++SwitchRequests;
	if (!SwitchRejection(Viewpoint, Viewpoint == nullptr).IsEmpty())
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
	if (!bReapplying && State != EVBoRAlignmentState::Applied)
	{
		bSwitching = false; ++SwitchDiscards;
		if (APlayerController* PC = Cast<APlayerController>(AlignmentPawn->GetController()))
		{ if (PC->PlayerCameraManager) { PC->PlayerCameraManager->StopCameraFade(); } }
		UE_LOG(LogP3Flow, Log, TEXT("시점 버림 — 정렬 확인 필요; Pawn 유지")); return;
	}
	SwitchElapsed += FMath::Max(0.0, DeltaSeconds);
	if (!bMoved && SwitchElapsed >= FadeSeconds)
	{
		if (bReapplying)
		{
			// 설치 저장값만 다시 읽음. InitializeAlignment 의 전환 초기화 뒤 암전 상태 복구 (P5i D8)
			LoadSetupAlignment(); bReapplying = true; bSwitching = true;
		}
		else
		{
			// 매번 Stage 저장값에서 직접 계산함 (전환 오차 누적 방지, P3 E11)
			const FTransform Eye = PendingViewpoint ? PendingViewpoint->GetComponentTransform() : BaseEye->GetComponentTransform();
			AlignmentPawn->SetActorTransform(PawnFor(Eye), false, nullptr, ETeleportType::TeleportPhysics);
			HomeShiftCm = FVector2D::ZeroVector;
			CurrentViewpoint = PendingViewpoint;
		}
		bMoved = true;
		SwitchElapsed = FadeSeconds;
		OnPawnMoved.Broadcast();
		if (APlayerController* PC = AlignmentPawn ? Cast<APlayerController>(AlignmentPawn->GetController()) : nullptr)
		{ if (PC->PlayerCameraManager) { PC->PlayerCameraManager->StartCameraFade(1, 0, FadeSeconds, FLinearColor::Black, false, false); } }
		return;
	}
	if (bMoved && SwitchElapsed >= 2 * FadeSeconds)
	{
		bSwitching = false;
		if (bReapplying) { bReapplying = false; ++ReapplyCompletions; return; }
		++SwitchCompletions;
		UE_LOG(LogP3Flow, Log, TEXT("시점 완료 %s requests=%d complete=%d discarded=%d"), CurrentViewpoint ? *CurrentViewpoint->GetName() : TEXT("기준"), SwitchRequests, SwitchCompletions, SwitchDiscards);
	}
}

USceneComponent* UVBoRVehicleAlignmentComponent::GetCurrentEye() const
{
	return CurrentViewpoint ? static_cast<USceneComponent*>(CurrentViewpoint.Get()) : BaseEye.Get();
}

bool UVBoRVehicleAlignmentComponent::ReadWorldHead(FTransform& OutHead) const
{
	if (!AlignmentPawn || !GEngine || !GEngine->XRSystem.IsValid()) { return false; }
	const auto& XR = GEngine->XRSystem;
	FQuat Rotation; FVector Position;
	if (!XR->IsTracking(IXRTrackingSystem::HMDDeviceId) || !XR->GetCurrentPose(IXRTrackingSystem::HMDDeviceId, Rotation, Position)) { return false; }
	// P3 camera 는 Pawn root 에 붙음. 이동 직후 XR 의 cached world transform 을 쓰지 않음 (R1-7)
	OutHead = FTransform(Rotation, Position) * AlignmentPawn->GetActorTransform();
	return !OutHead.ContainsNaN();
}

FString UVBoRVehicleAlignmentComponent::DescribeHead(bool bTracked, const FVector& WorldHead) const
{
	if (!bTracked || !BaseEye || WorldHead.ContainsNaN()) { return FString(); }
	const FTransform Eye = CurrentViewpoint ? CurrentViewpoint->GetComponentTransform() : BaseEye->GetComponentTransform();
	return FString::Printf(TEXT("시점 %s | %s"), CurrentViewpoint ? *CurrentViewpoint->GetName() : TEXT("기준"),
		*FVBoRBuckAlignment::DescribeTip(true, WorldHead, Eye));
}

FString UVBoRVehicleAlignmentComponent::Describe() const
{
	const TCHAR* Name = State == EVBoRAlignmentState::Applied ? TEXT("적용됨") : State == EVBoRAlignmentState::NoSavedValue ? TEXT("저장값 없음") : TEXT("확인 필요");
	return FString::Printf(TEXT("%s %s | 저장 %s | 시점 %s%s | 요청 %d 완료 %d 버림 %d"), Name, *Reason, *Saved.SavedAt,
		CurrentViewpoint ? *CurrentViewpoint->GetName() : TEXT("기준"), bSwitching ? TEXT(" (전환 중)") : TEXT(""), SwitchRequests, SwitchCompletions, SwitchDiscards);
}
