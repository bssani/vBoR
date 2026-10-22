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
	const TCHAR* EyeDisabledReason = TEXT("No saved value — eye alignment (home test) disabled by vBoR.EyeAlignment 0. Use P in operator panel for point alignment, or save in P0-c and enable vBoR.EyeAlignment 1");
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
	// No transition without reference seat point (avoid staying at reference with zero difference, P4h review R1-1)
	if (!Reference) { if (Failure.IsEmpty()) { Failure = TEXT("Driver seat reference missing"); } return false; }
	// Add seat-point difference only, not viewpoint Yaw (P4h D6, SgRP difference, cm)
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
	if (!Value) { Reason = TEXT("No saved value — P in operator panel for point alignment or C in P0-c level to save"); }
	else
	{
		Saved = *Value;
		const auto Decision = FVBoRBuckAlignment::CheckEnvironment(Saved, Origin, PlayArea, PlayAreaToleranceCm);
		State = EVBoRAlignmentState::NeedsCheck; Reason = Decision.Reason;
		FTransform Base; FString Failure;
		if (!AlignmentPawn || (Saved.Version == 1 && !BaseEye) || AlignmentPawn->bUseControllerRotationYaw ||
			!FMath::IsNearlyEqual(GetWorld()->GetWorldSettings()->WorldToMeters, 100.0f))
		{ Reason = TEXT("Pawn/eye point/WorldToMeters check required"); }
		else if (!BasePawn(Base,Failure)) { Reason = Failure; }
		else if (Decision.bApply)
		{
			// Ignore participant's current head pose (preserve body-size differences, P3 D2)
			AlignmentPawn->SetActorTransform(Base, false, nullptr, ETeleportType::TeleportPhysics);
			bApplied = true;
			HomeShiftCm = FVector2D::ZeroVector;
			State = Decision.bNeedsCheck ? EVBoRAlignmentState::NeedsCheck : EVBoRAlignmentState::Applied;
		}
	}
	bFileEyeAlignment &= bApplied;
	FString SeatFailure; DriverSeatReference(SeatFailure);
	if (!SeatFailure.IsEmpty()) { State = EVBoRAlignmentState::NeedsCheck; Reason = SeatFailure; }
	UE_LOG(LogP3Flow, Log, TEXT("Alignment %s"), *Describe());
}

void UVBoRVehicleAlignmentComponent::HandleRecenter()
{
	CancelPoints(TEXT("Recenter — points cleared"));
	State = EVBoRAlignmentState::NeedsCheck; Reason = TEXT("Recenter — installation alignment check required");
	UE_LOG(LogP3Flow, Log, TEXT("Alignment %s; Pawn unchanged"), *Describe());
}

void UVBoRVehicleAlignmentComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	UpdateEyeAlignment();
	if (bHasSaved && State == EVBoRAlignmentState::Applied && GEngine && GEngine->XRSystem.IsValid())
	{
		const auto Check = FVBoRBuckAlignment::CheckEnvironment(Saved, GEngine->XRSystem->GetTrackingOrigin(),
			UHeadMountedDisplayFunctionLibrary::GetPlayAreaBounds(EHMDTrackingOrigin::Stage), PlayAreaToleranceCm);
		if (Check.bNeedsCheck) { State = EVBoRAlignmentState::NeedsCheck; Reason = Check.Reason; UE_LOG(LogP3Flow, Log, TEXT("Alignment %s; Pawn unchanged"), *Describe()); }
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
	// Revert file-loaded eye alignment only. Exclude direct initialization/four-point alignment (P5u D6)
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
		UE_LOG(LogP3Flow, Log, TEXT("Viewpoint discarded — %s"), !IsValid(Viewpoint) ? TEXT("No target") : bSwitching ? TEXT("Transitioning") : TEXT("Alignment required")); return false;
	}
	return RequestSwitch(Viewpoint->ComponentHasTag(TEXT("Car.DriverView")) ? nullptr : Viewpoint);
}

bool UVBoRVehicleAlignmentComponent::ReturnToBase() { return RequestSwitch(nullptr); }

bool UVBoRVehicleAlignmentComponent::MoveHomeShift(FVector2D DeltaCm)
{
	if (bSwitching || !AlignmentPawn || DeltaCm.IsZero()) { return false; }
	// Horizontal car axes only (keep height/rotation during home test, P3c D2/D3, cm)
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
	if (Reason.StartsWith(TEXT("No saved value"))) { return TEXT("Capture points with P, or save the Buck alignment in P0-c"); }
	if (Reason.StartsWith(TEXT("Recenter"))) { return TEXT("Recenter detected; check the setup alignment"); }
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
		UE_LOG(LogP3Flow, Log, TEXT("Viewpoint discarded — %s"), bSwitching ? TEXT("Transitioning") : TEXT("Alignment required")); return false;
	}
	PendingViewpoint = Viewpoint; bSwitching = true; bMoved = false; SwitchElapsed = 0;
	if (APlayerController* PC = Cast<APlayerController>(AlignmentPawn->GetController()))
	{ if (PC->PlayerCameraManager) { PC->PlayerCameraManager->StartCameraFade(0, 1, FadeSeconds, FLinearColor::Black, false, true); } }
	UE_LOG(LogP3Flow, Log, TEXT("Viewpoint request %s"), Viewpoint ? *Viewpoint->GetName() : TEXT("Reference"));
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
		UE_LOG(LogP3Flow, Log, TEXT("Viewpoint discarded — alignment check required; Pawn unchanged")); return;
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
			// Reload installation save only. Restore blackout after InitializeAlignment transition reset (P5i D8)
			LoadSetupAlignment(); bReapplying = true; bSwitching = true;
		}
		else
		{
			// Compute directly from Stage save every time (no transition error accumulation, P3 E11)
			FTransform Target; FString Failure;
			// On failure keep Pawn, require check; discard transition next Tick (no jump to origin, P4h review R1-1)
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
		UE_LOG(LogP3Flow, Log, TEXT("Viewpoint complete %s requests=%d complete=%d discarded=%d"), CurrentViewpoint ? *CurrentViewpoint->GetName() : TEXT("Reference"), SwitchRequests, SwitchCompletions, SwitchDiscards);
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
	// P3 camera attached to Pawn root. No XR cached world transform immediately after motion (R1-7)
	OutHead = FTransform(Rotation, Position) * AlignmentPawn->GetActorTransform();
	return !OutHead.ContainsNaN();
}

FString UVBoRVehicleAlignmentComponent::DescribeHead(bool bTracked, const FVector& WorldHead) const
{
	if (!bTracked || !BaseEye || WorldHead.ContainsNaN()) { return FString(); }
	const FTransform Eye = CurrentViewpoint ? CurrentViewpoint->GetComponentTransform() : BaseEye->GetComponentTransform();
	return FString::Printf(TEXT("Viewpoint %s | %s"), CurrentViewpoint ? *CurrentViewpoint->GetName() : TEXT("Reference"),
		*FVBoRBuckAlignment::DescribeTip(true, WorldHead, Eye));
}

FString UVBoRVehicleAlignmentComponent::Describe() const
{
	const TCHAR* Name = State == EVBoRAlignmentState::Applied ? TEXT("Applied") : State == EVBoRAlignmentState::NoSavedValue ? TEXT("No saved value") : TEXT("Check required");
	return FString::Printf(TEXT("%s%s %s | saved %s | viewpoint %s%s | requests %d complete %d discarded %d"), Name, bFileEyeAlignment ? TEXT(" eye alignment (home test)") : TEXT(""), *Reason, *Saved.SavedAt,
		CurrentViewpoint ? *CurrentViewpoint->GetName() : TEXT("Reference"), bSwitching ? TEXT(" (transitioning)") : TEXT(""), SwitchRequests, SwitchCompletions, SwitchDiscards);
}
