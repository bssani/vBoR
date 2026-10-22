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
#include "HAL/IConsoleManager.h"

namespace
{
	TAutoConsoleVariable<int32> CVarEyeAlignment(TEXT("vBoR.EyeAlignment"), 0,
		TEXT("Home testing only: 1 = apply the P0-c version 1 save with saved eye height. 0 = point alignment only. Changes take effect while playing."));
	const TCHAR* EyeDisabledReason = TEXT("저장값 없음 — 눈 방식 저장값(집 시험용)은 vBoR.EyeAlignment 0이라 쓰지 않음. 진행자 창 P로 점 정렬 또는 P0-c 저장 후 vBoR.EyeAlignment 1로 켬");
}

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
	if (AlignmentPawn) { BeforeSetupPawn = AlignmentPawn->GetActorTransform(); }
	bEyeEnabled = CVarEyeAlignment.GetValueOnGameThread() != 0;
	LoadSetupAlignment();
}

FString UVBoRVehicleAlignmentComponent::SavedAlignmentPath() const
{ return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Calibration/BuckEye.json")); }

bool UVBoRVehicleAlignmentComponent::PawnFor(const UVBoRViewpointComponent* Viewpoint, FTransform& Out, FString& Failure) const
{
	if (!BasePawn(Out,Failure)) { return false; }
	if (!Viewpoint) { return true; }
	const USceneComponent* Reference = DriverSeatReference(Failure);
	// 기준 좌석점 없으면 전환 안 함 (차이 없이 기준에 머무는 것 방지, P4h 리뷰 R1-1)
	if (!Reference) { if (Failure.IsEmpty()) { Failure = TEXT("Driver seat reference missing"); } return false; }
	// 시점의 Yaw 대신 좌석점 차이만 더함 (P4h D6, SgRP 차이, cm)
	Out.AddToTranslation(Viewpoint->GetComponentLocation() - Reference->GetComponentLocation());
	return true;
}

void UVBoRVehicleAlignmentComponent::LoadSetupAlignment()
{
	FVBoRBuckSavedAlignment Loaded; FString LoadReason;
	const bool bLoaded = FVBoRBuckAlignmentStore::Load(SavedAlignmentPath(), Loaded, LoadReason);
	const bool bEyeBlocked = bLoaded && Loaded.Version == 1 && CVarEyeAlignment.GetValueOnGameThread() == 0;
	int32 Origin; FVector2D Bounds; ReadPointEnvironment(Origin,Bounds);
	InitializeSetup(AlignmentPawn, bLoaded && !bEyeBlocked ? &Loaded : nullptr, Origin, Bounds, true);
	if (bEyeBlocked) { Reason = EyeDisabledReason; }
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
{ InitializeSetup(Pawn, Value, Origin, PlayArea, false); }

void UVBoRVehicleAlignmentComponent::InitializeSetup(APawn* Pawn, const FVBoRBuckSavedAlignment* Value, int32 Origin, const FVector2D& PlayArea, bool bFromFile)
{
	bAlignmentFromFile = bFromFile;
	bFileEyeAlignment = bFromFile && Value && Value->Version == 1;
	bPendingEyeEnable = false; bRestoringEye = false;
	bool bApplied = false;
	AlignmentPawn = Pawn; bHasSaved = Value != nullptr;
	CurrentViewpoint = nullptr; PendingViewpoint = nullptr; bSwitching = false; bReapplying = false;
	State = EVBoRAlignmentState::NoSavedValue;
	if (!Value) { Reason = TEXT("저장값 없음 — 진행자 창 P로 점 정렬 또는 P0-c 레벨에서 C로 저장"); }
	else
	{
		Saved = *Value;
		const auto Decision = FVBoRBuckAlignment::CheckEnvironment(Saved, Origin, PlayArea, PlayAreaToleranceCm);
		State = EVBoRAlignmentState::NeedsCheck; Reason = Decision.Reason;
		FTransform Base; FString Failure;
		if (!AlignmentPawn || (Saved.Version == 1 && !BaseEye) || AlignmentPawn->bUseControllerRotationYaw ||
			!FMath::IsNearlyEqual(GetWorld()->GetWorldSettings()->WorldToMeters, 100.0f))
		{ Reason = TEXT("Pawn·eye point·WorldToMeters 확인 필요"); }
		else if (!BasePawn(Base,Failure)) { Reason = Failure; }
		else if (Decision.bApply)
		{
			// 참가자의 현재 머리 자세를 읽지 않음 (체격 차이 유지, P3 D2)
			AlignmentPawn->SetActorTransform(Base, false, nullptr, ETeleportType::TeleportPhysics);
			bApplied = true;
			HomeShiftCm = FVector2D::ZeroVector;
			State = Decision.bNeedsCheck ? EVBoRAlignmentState::NeedsCheck : EVBoRAlignmentState::Applied;
		}
	}
	bFileEyeAlignment &= bApplied;
	FString SeatFailure; DriverSeatReference(SeatFailure);
	if (!SeatFailure.IsEmpty()) { State = EVBoRAlignmentState::NeedsCheck; Reason = SeatFailure; }
	UE_LOG(LogP3Flow, Log, TEXT("정렬 %s"), *Describe());
}

void UVBoRVehicleAlignmentComponent::HandleRecenter()
{
	CancelPoints(TEXT("Recenter — points cleared"));
	State = EVBoRAlignmentState::NeedsCheck; Reason = TEXT("재중심 — 설치 정렬 확인 필요");
	UE_LOG(LogP3Flow, Log, TEXT("정렬 %s; Pawn 유지"), *Describe());
}

void UVBoRVehicleAlignmentComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	UpdateEyeAlignment();
	if (bHasSaved && State == EVBoRAlignmentState::Applied && GEngine && GEngine->XRSystem.IsValid())
	{
		const auto Check = FVBoRBuckAlignment::CheckEnvironment(Saved, GEngine->XRSystem->GetTrackingOrigin(),
			UHeadMountedDisplayFunctionLibrary::GetPlayAreaBounds(EHMDTrackingOrigin::Stage), PlayAreaToleranceCm);
		if (Check.bNeedsCheck) { State = EVBoRAlignmentState::NeedsCheck; Reason = Check.Reason; UE_LOG(LogP3Flow, Log, TEXT("정렬 %s; Pawn 유지"), *Describe()); }
	}
	AdvancePointCapture();
	AdvanceSwitch(DeltaTime);
}

void UVBoRVehicleAlignmentComponent::UpdateEyeAlignment()
{
	const bool bEnabled = CVarEyeAlignment.GetValueOnGameThread() != 0;
	if (bEnabled != bEyeEnabled)
	{
		bEyeEnabled = bEnabled;
		bPendingEyeEnable = bEnabled && bAlignmentFromFile;
		if (!bEnabled && bFileEyeAlignment) { RestoreBeforeEyeAlignment(); }
	}
	if (!bPendingEyeEnable || !ReapplyRejection().IsEmpty()) { return; }
	bPendingEyeEnable = false;
	FVBoRBuckSavedAlignment Value; FString Failure;
	if (!bFileEyeAlignment && FVBoRBuckAlignmentStore::Load(SavedAlignmentPath(), Value, Failure) && Value.Version == 1)
	{ RequestReapply(); }
}

void UVBoRVehicleAlignmentComponent::RestoreBeforeEyeAlignment()
{
	// 파일의 눈 정렬만 되돌림. 직접 초기화·4점 정렬은 대상 아님 (P5u D6)
	State = EVBoRAlignmentState::NoSavedValue; Reason = EyeDisabledReason;
	bHasSaved = false; bFileEyeAlignment = false;
	if (bSwitching && !bReapplying && !bRestoringEye) { ++SwitchDiscards; }
	PendingViewpoint = nullptr; bReapplying = false;
	bRestoringEye = true; bSwitching = true; bMoved = false; SwitchElapsed = 0;
	if (APlayerController* PC = AlignmentPawn ? Cast<APlayerController>(AlignmentPawn->GetController()) : nullptr)
	{ if (PC->PlayerCameraManager) { PC->PlayerCameraManager->StartCameraFade(0, 1, FadeSeconds, FLinearColor::Black, false, true); } }
}

bool UVBoRVehicleAlignmentComponent::SwitchViewpoint(UVBoRViewpointComponent* Viewpoint)
{
	if (!SwitchRejection(Viewpoint).IsEmpty())
	{
		++SwitchRequests; ++SwitchDiscards;
		UE_LOG(LogP3Flow, Log, TEXT("시점 버림 — %s"), !IsValid(Viewpoint) ? TEXT("대상 없음") : bSwitching ? TEXT("전환 중") : TEXT("정렬 필요")); return false;
	}
	return RequestSwitch(Viewpoint->ComponentHasTag(TEXT("Car.DriverView")) ? nullptr : Viewpoint);
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
	if (!bHasSaved || !AlignmentPawn || (Saved.Version == 1 && !BaseEye) || State != EVBoRAlignmentState::Applied) { return TEXT("Saved alignment required"); }
	FString Failure;
	if (!bReturnBase && !DriverSeatReference(Failure)) { return Failure.IsEmpty() ? FString(TEXT("Driver seat reference missing")) : Failure; }
	return FString();
}

FString UVBoRVehicleAlignmentComponent::ReapplyRejection() const
{
	if (bPointSession) { return TEXT("Point alignment in progress"); }
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
	if (Reason == EyeDisabledReason) { return TEXT("P0-c head save ignored: vBoR.EyeAlignment is 0. Use P for point alignment, or vBoR.EyeAlignment 1 for home testing."); }
	if (Reason.StartsWith(TEXT("저장값 없음"))) { return TEXT("Capture points with P, or save the Buck alignment in P0-c"); }
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
	if (!bReapplying && !bRestoringEye && State != EVBoRAlignmentState::Applied)
	{
		bSwitching = false; ++SwitchDiscards;
		if (APlayerController* PC = Cast<APlayerController>(AlignmentPawn->GetController()))
		{ if (PC->PlayerCameraManager) { PC->PlayerCameraManager->StopCameraFade(); } }
		UE_LOG(LogP3Flow, Log, TEXT("시점 버림 — 정렬 확인 필요; Pawn 유지")); return;
	}
	SwitchElapsed += FMath::Max(0.0, DeltaSeconds);
	if (!bMoved && SwitchElapsed >= FadeSeconds)
	{
		if (bRestoringEye)
		{
			if (AlignmentPawn) { AlignmentPawn->SetActorTransform(BeforeSetupPawn, false, nullptr, ETeleportType::TeleportPhysics); }
			HomeShiftCm = FVector2D::ZeroVector; CurrentViewpoint = nullptr;
		}
		else if (bReapplying)
		{
			// 설치 저장값만 다시 읽음. InitializeAlignment 의 전환 초기화 뒤 암전 상태 복구 (P5i D8)
			LoadSetupAlignment(); bReapplying = true; bSwitching = true;
		}
		else
		{
			// 매번 Stage 저장값에서 직접 계산함 (전환 오차 누적 방지, P3 E11)
			FTransform Target; FString Failure;
			// 못 구하면 Pawn 그대로 두고 확인 필요로 바꿈. 다음 Tick 에 전환 버림 (원점으로 튀지 않게, P4h 리뷰 R1-1)
			if (!PawnFor(PendingViewpoint, Target, Failure)) { State = EVBoRAlignmentState::NeedsCheck; Reason = Failure; return; }
			AlignmentPawn->SetActorTransform(Target, false, nullptr, ETeleportType::TeleportPhysics);
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
		if (bRestoringEye) { bRestoringEye = false; return; }
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
	return FString::Printf(TEXT("%s%s %s | 저장 %s | 시점 %s%s | 요청 %d 완료 %d 버림 %d"), Name, bFileEyeAlignment ? TEXT(" 눈 방식(집 시험)") : TEXT(""), *Reason, *Saved.SavedAt,
		CurrentViewpoint ? *CurrentViewpoint->GetName() : TEXT("기준"), bSwitching ? TEXT(" (전환 중)") : TEXT(""), SwitchRequests, SwitchCompletions, SwitchDiscards);
}
