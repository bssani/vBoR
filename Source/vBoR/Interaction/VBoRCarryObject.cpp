#include "VBoRCarryObject.h"
#include "VBoRLoadTarget.h"
#include "VBoRHandRigPawn.h"
#include "Components/StaticMeshComponent.h"
#include "Components/AudioComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Interaction/IsdkGrabbableComponent.h"
#include "Interaction/IsdkGrabberComponent.h"
#include "Interaction/GrabDetectors/IsdkHandGrabDetector.h"
#include "Interaction/Grabbable/IsdkGrabTransformerComponent.h"
#include "Interaction/Pointable/IsdkInteractionPointerEvent.h"
#include "Rig/IsdkHandRigComponent.h"
#include "IsdkFunctionLibrary.h"
#include "IsdkContentAssetPaths.h"
#include "OculusXRInputFunctionLibrary.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Sound/SoundBase.h"
#include "UObject/ConstructorHelpers.h"

UVBoRCarryGrabTransformer::UVBoRCarryGrabTransformer() { ConfigureCarry(); }
void UVBoRCarryGrabTransformer::ConfigureCarry()
{
	bIsThrowable = false;
	MultiGrabBehavior = EIsdkMultiGrabBehavior::SingleGrabFirstRetained;
}

void UVBoRCarryTransformer::BeginTransform(const TArray<FIsdkGrabPose>& Poses, const FIsdkTargetTransform& Target)
{
	bActive = !Poses.IsEmpty();
}

FTransform UVBoRCarryTransformer::UpdateTransform(const TArray<FIsdkGrabPose>& Poses, const FIsdkTargetTransform& Target)
{
	AVBoRCarryObject* Object = GetTypedOuter<AVBoRCarryObject>();
	// SDK 가 보류 중 다른 손을 선택해도 소유 손만 읽음 (P1 §4 양손 잡기 금지)
	if (Object && !Poses.IsEmpty()) { Object->AdvanceHeldPose(); }
	// SDK 의 SetRelativeTransform 에 검사한 자세만 반환함 (P1 §5)
	return Object ? Object->GetActorTransform().GetRelativeTransform(Target.ParentWorldTransform) : Target.RelativeTransform;
}

FTransform UVBoRCarryTransformer::EndTransform(const FIsdkTargetTransform& Target)
{
	bActive = false;
	// End 는 추적 취소·선택 변경 때도 옴. 여기서 낙하를 시작하지 않음 (P1 §5)
	const AVBoRCarryObject* Object = GetTypedOuter<AVBoRCarryObject>();
	return Object ? Object->GetActorTransform().GetRelativeTransform(Target.ParentWorldTransform) : Target.RelativeTransform;
}

AVBoRCarryObject::AVBoRCarryObject()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostUpdateWork;
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CarryMesh"));
	SetRootComponent(Mesh);
	Mesh->SetMobility(EComponentMobility::Movable);
	Mesh->SetSimulatePhysics(false);
	Mesh->SetEnableGravity(false);
	Mesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Mesh->SetCollisionObjectType(ECC_WorldDynamic);
	Mesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	Mesh->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
	Mesh->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(IsdkContentAssetPaths::Engine::BasicShapes::Cube);
	Mesh->SetStaticMesh(Cube.Object);
	Grabbable = CreateDefaultSubobject<UIsdkGrabbableComponent>(TEXT("Grabbable"));
	Grabbable->SetupAttachment(Mesh);
	GrabTransformer = CreateDefaultSubobject<UVBoRCarryGrabTransformer>(TEXT("GrabTransformer"));
	CarryTransformer = CreateDefaultSubobject<UVBoRCarryTransformer>(TEXT("ConstrainedCarry"));
	StateAudio = CreateDefaultSubobject<UAudioComponent>(TEXT("StateAudio"));
	StateAudio->SetupAttachment(Mesh);
	StateAudio->bAutoActivate = false;
	static ConstructorHelpers::FObjectFinder<USoundBase> Sound(IsdkContentAssetPaths::Audio::Interaction_BasicPoke_ButtonPress);
	StateAudio->SetSound(Sound.Object);
}

void AVBoRCarryObject::PreInitializeComponents()
{
	// 자식 설정 반영 뒤 SDK BeginPlay 전에 Palm 만 허용하고 Throwable 을 끔 (P1 D1)
	Grabbable->SetGrabInputMethodAllowed(EIsdkGrabInputMethod::Palm, true);
	Grabbable->SetGrabInputMethodAllowed(EIsdkGrabInputMethod::Pinch, false);
	Grabbable->SetGrabCollider(Mesh);
	GrabTransformer->ConfigureCarry();
	GrabTransformer->SetSingleGrabTransformerObject(CarryTransformer);
	GrabTransformer->SetTransformTarget(Mesh);
	Grabbable->SetGrabTransformer(GrabTransformer);
	Mesh->SetSimulatePhysics(false);
	Mesh->SetEnableGravity(false);
	Super::PreInitializeComponents();
}

void AVBoRCarryObject::BeginPlay()
{
	Super::BeginPlay();
	UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, IsdkContentAssetPaths::Materials::GrabbableDefault);
	StateOverlay = Material ? UMaterialInstanceDynamic::Create(Material, this) : nullptr;
	Mesh->SetOverlayMaterial(nullptr);
	Grabbable->GetInteractionPointerEventDelegate().AddUniqueDynamic(this, &AVBoRCarryObject::HandlePointer);
	UpdateVisual();
}

FBox AVBoRCarryObject::GetMeshLocalBounds() const
{
	return Mesh->GetStaticMesh() ? Mesh->GetStaticMesh()->GetBoundingBox() : FBox(ForceInit);
}

bool AVBoRCarryObject::IsPoseClear(const FTransform& Pose) const
{
	const FBox Bounds = GetMeshLocalBounds();
	if (!Bounds.IsValid || !GetWorld() || Pose.ContainsNaN()) { return false; }
	// 원통도 외접 OBB 로 보수적으로 검사함 (모서리에서 일찍 멈춰도 보이는 형상 관통은 막음, P1 §5)
	const FVector Extent = (Bounds.GetExtent() * Pose.GetScale3D().GetAbs() - FVector(0.005)).ComponentMax(FVector(0.001));
	FCollisionQueryParams Params(SCENE_QUERY_STAT(VBoRCarryOverlap), false, this);
	return !GetWorld()->OverlapBlockingTestByChannel(Pose.TransformPosition(Bounds.GetCenter()), Pose.GetRotation(),
		ECC_WorldDynamic, FCollisionShape::MakeBox(Extent), Params);
}

bool AVBoRCarryObject::TryMoveToPose(const FTransform& Requested)
{
	const FTransform Current = GetActorTransform();
	FTransform Goal = Requested;
	// 안착 보조에서도 원래 스케일을 유지함 (P1 §3)
	Goal.SetScale3D(Current.GetScale3D());
	bool bRotationBlocked = false;
	if (!IsPoseClear(FTransform(Goal.GetRotation(), Current.GetLocation(), Current.GetScale3D())))
	{
		Goal.SetRotation(Current.GetRotation());
		bRotationBlocked = true;
	}
	const FBox Bounds = GetMeshLocalBounds();
	if (!Bounds.IsValid) { return false; }
	FCollisionQueryParams Params(SCENE_QUERY_STAT(VBoRCarrySweep), false, this);
	FHitResult Hit;
	// 받침대에 닿은 시작 자세도 움직일 수 있게 overlap 과 같은 0.05 mm 접촉 여유 (P1 D5)
	const FVector Extent = (Bounds.GetExtent() * Current.GetScale3D().GetAbs() - FVector(0.005)).ComponentMax(FVector(0.001));
	const FVector CenterOffset = Goal.GetRotation().RotateVector(Bounds.GetCenter() * Current.GetScale3D());
	const bool bHit = GetWorld()->SweepSingleByChannel(Hit, Current.GetLocation() + CenterOffset,
		Goal.GetLocation() + CenterOffset, Goal.GetRotation(), ECC_WorldDynamic, FCollisionShape::MakeBox(Extent), Params);
	if (bHit)
	{
		// 접촉면에서 0.05 mm 앞에 멈춤 (다음 sweep 의 시작 겹침으로 후퇴까지 막히는 것 방지)
		const double Length = FVector::Distance(Current.GetLocation(), Goal.GetLocation());
		const double StopTime = Hit.bStartPenetrating ? 0.0 : FMath::Max(0.0, Hit.Time - 0.005 / FMath::Max(Length, 0.005));
		Goal.SetLocation(FVBoRCarryMotion::SweepStop(Current.GetLocation(), Goal.GetLocation(), StopTime));
	}
	if (!IsPoseClear(Goal)) { return false; }
	SetActorTransform(Goal, false, nullptr, ETeleportType::TeleportPhysics);
	return !bHit && !bRotationBlocked;
}

bool AVBoRCarryObject::IsHandTracked(int32 Hand) const
{
	if (Hand < 0 || Hand > 1) { return false; }
	const EOculusXRHandType Type = Hand == 0 ? EOculusXRHandType::HandLeft : EOculusXRHandType::HandRight;
	return UOculusXRInputFunctionLibrary::IsHandPositionValid(Type) &&
		UOculusXRInputFunctionLibrary::GetTrackingConfidence(Type) == EOculusXRTrackingConfidence::High;
}

bool AVBoRCarryObject::ReadHand(FTransform& Pose) const
{
	if (!HoldingGrabber || !IsHandTracked(Session.Hand) || !HoldingGrabber->GetHandGrabDetector() ||
		!HoldingGrabber->GetHandGrabDetector()->GetPalmGrabCollider()) { return false; }
	HoldingGrabber->GetPalmGrabTransform(Pose);
	Pose.SetScale3D(FVector::OneVector);
	return !Pose.ContainsNaN();
}

void AVBoRCarryObject::BindHoldingRig(UIsdkGrabberComponent* Grabber)
{
	ClearHoldingRig();
	HoldingGrabber = Grabber;
	HoldingRig = Grabber ? Grabber->GetTypedOuter<UIsdkHandRigComponent>() : nullptr;
	if (HoldingRig)
	{
		// Palm 입력을 직접 관찰함 (SDK selection 이 사라져도 손을 펴면 종료, P1 §5)
		PalmSelectHandle = HoldingRig->OnPalmSelectDelegate.AddUObject(this, &AVBoRCarryObject::HandlePalmSelect);
		PalmReleaseHandle = HoldingRig->OnPalmUnselectDelegate.AddUObject(this, &AVBoRCarryObject::HandlePalmRelease);
	}
	bPalmOpen = false;
}

void AVBoRCarryObject::ClearHoldingRig()
{
	if (HoldingRig)
	{
		HoldingRig->OnPalmSelectDelegate.Remove(PalmSelectHandle);
		HoldingRig->OnPalmUnselectDelegate.Remove(PalmReleaseHandle);
	}
	HoldingRig = nullptr;
	HoldingGrabber = nullptr;
}

void AVBoRCarryObject::HandlePalmSelect(UIsdkRigComponent* Rig) { bPalmOpen = false; }
void AVBoRCarryObject::HandlePalmRelease(UIsdkRigComponent* Rig) { bPalmOpen = true; }

void AVBoRCarryObject::HandlePointer(const FIsdkInteractionPointerEvent& Event)
{
	UIsdkGrabberComponent* Grabber = Cast<UIsdkGrabberComponent>(Event.Interactor);
	EIsdkHandedness Handedness;
	if (!Grabber || !UIsdkFunctionLibrary::GetHandednessFromInteractor(Grabber, Handedness)) { return; }
	const int32 Hand = Handedness == EIsdkHandedness::Left ? 0 : 1;
	const double Time = GetWorld()->GetTimeSeconds();
	if (Event.Type == EIsdkPointerEventType::Hover)
	{
		if (!Hovering.Contains(Grabber)) { Hovering.Add(Grabber); ++Attempts[Hand]; }
	}
	else if (Event.Type == EIsdkPointerEventType::Unhover) { Hovering.Remove(Grabber); }
	else if (Event.Type == EIsdkPointerEventType::Select)
	{
		if (Session.Hand != INDEX_NONE)
		{
			if (HoldingGrabber == Grabber) { bPalmOpen = false; }
			return;
		}
		if (!IsHandTracked(Hand)) { return; }
		if (!Grabber->GetHandGrabDetector() || !Grabber->GetHandGrabDetector()->GetPalmGrabCollider()) { return; }
		FTransform Pose;
		Grabber->GetPalmGrabTransform(Pose);
		Pose.SetScale3D(FVector::OneVector);
		if (Pose.ContainsNaN() || !Session.Begin(Hand, Pose, GetActorTransform(), Time)) { return; }
		BindHoldingRig(Grabber);
		bDropping = bAssisting = bSeated = bBlocked = false;
		++Successes[Hand];
		OnGrabbed.Broadcast(this);
	}
	else if ((Event.Type == EIsdkPointerEventType::Unselect || Event.Type == EIsdkPointerEventType::Cancel) && Grabber == HoldingGrabber)
	{
		if (!IsHandTracked(Hand) || Session.bPaused) { Session.Release(Hand, false, Time); }
		else { ReleaseCarry(); }
	}
	UpdateVisual();
}

void AVBoRCarryObject::AdvanceHeldPose()
{
	if (Session.Hand == INDEX_NONE) { return; }
	FTransform HandPose;
	const bool bTracked = ReadHand(HandPose);
	FTransform Desired;
	if (!Session.Update(bTracked, HandPose, GetActorTransform(), GetWorld()->GetTimeSeconds(), TiltLimitDeg, Desired)) { UpdateVisual(); return; }
	const bool bNewBlocked = !TryMoveToPose(Desired);
	if (bNewBlocked && !bBlocked) { ++BlockedCount; }
	bBlocked = bNewBlocked;
	if (bBlocked) { MaxHandGapCm = FMath::Max(MaxHandGapCm, FVector::Distance(HandPose.GetLocation(), GetActorLocation())); }
	UpdateVisual();
}

void AVBoRCarryObject::ReleaseCarry()
{
	if (!Session.Release(Session.Hand, true, GetWorld()->GetTimeSeconds())) { return; }
	ClearHoldingRig();
	bBlocked = false;
	FTransform Goal = LoadTarget ? LoadTarget->GetGoalPose() : GetActorTransform();
	Goal.SetScale3D(GetActorScale3D());
	bAssisting = LoadTarget && FVBoRCarryMotion::CanAssist(GetActorTransform(), Goal, LoadTarget->IsStaticFit(this), AssistDistanceCm, AssistAngleDeg);
	bDropping = !bAssisting;
	ReleaseResult = bAssisting ? TEXT("assisted seating (in progress)") : TEXT("vertical drop");
	if (bAssisting) { AssistFrom = GetActorTransform(); AssistGoal = Goal; AssistStartedAt = GetWorld()->GetTimeSeconds(); }
	OnCarryReleased.Broadcast(this);
	UpdateVisual();
}

void AVBoRCarryObject::FinishPlacement()
{
	// 멈춘 면에 안착했음. 목표 적합성은 IsObjectInside 로 따로 판정 (P1 §2 놓기 결과)
	bSeated = true;
	bDropping = bAssisting = false;
	if (bSeated) { OnSeated.Broadcast(this); }
	UpdateVisual();
}

void AVBoRCarryObject::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (Session.Hand != INDEX_NONE)
	{
		FTransform HandPose;
		if (!ReadHand(HandPose))
		{
			Session.Update(false, HandPose, GetActorTransform(), GetWorld()->GetTimeSeconds(), TiltLimitDeg, HandPose);
			UpdateVisual();
			return;
		}
		const bool bRecovering = Session.bPaused;
		AdvanceHeldPose();
		// loss Unselect 의 옛 상태 대신 SDK 의 현재 인식 결과를 읽음 (P1 §4 보류 중 놓기 무시)
		bool bPalmClosed = true;
		bool bHasPalmState = false;
		if (const UVBoRHandRigLeft* LeftRig = Cast<UVBoRHandRigLeft>(HoldingRig)) { bHasPalmState = LeftRig->ReadPalmGrab(bPalmClosed); }
		else if (const UVBoRHandRigRight* RightRig = Cast<UVBoRHandRigRight>(HoldingRig)) { bHasPalmState = RightRig->ReadPalmGrab(bPalmClosed); }
		if (bHasPalmState) { bPalmOpen = !bPalmClosed; }
		if (!bRecovering && bPalmOpen && !Session.bPaused) { ReleaseCarry(); }
		if (bBlocked && !Session.bPaused) { BlockedSeconds += DeltaSeconds; }
	}
	else if (bAssisting)
	{
		const double Alpha = FMath::Clamp((GetWorld()->GetTimeSeconds() - AssistStartedAt) / FMath::Max(0.001f, AssistDurationSeconds), 0.0, 1.0);
		FTransform Pose;
		Pose.Blend(AssistFrom, AssistGoal, Alpha);
		if (!LoadTarget || !LoadTarget->IsStaticFit(this) || !TryMoveToPose(Pose))
		{
			bAssisting = false;
			bDropping = true;
			ReleaseResult = TEXT("assistance blocked -> vertical drop");
		}
		else if (Alpha >= 1.0) { ReleaseResult = TEXT("assisted seating"); FinishPlacement(); }
	}
	else if (bDropping)
	{
		FTransform Pose = GetActorTransform();
		Pose.SetLocation(FVBoRCarryMotion::Drop(Pose.GetLocation(), DropSpeedCmPerSecond, DeltaSeconds));
		if (!TryMoveToPose(Pose)) { FinishPlacement(); }
	}
	UpdateVisual();
}

void AVBoRCarryObject::ResetToPose(const FTransform& Pose)
{
	// 소유 손을 먼저 지움 (ForceCancel 의 동기 callback 을 놓기로 세지 않음, P1 R reset)
	Session = FVBoRCarrySession();
	ClearHoldingRig();
	GrabTransformer->ForceCancel();
	Hovering.Empty();
	Attempts[0] = Attempts[1] = Successes[0] = Successes[1] = 0;
	BlockedCount = 0;
	BlockedSeconds = MaxHandGapCm = 0;
	bDropping = bAssisting = bSeated = bBlocked = false;
	ReleaseResult = TEXT("none (reset)");
	FTransform Restored = Pose;
	Restored.SetScale3D(GetActorScale3D());
	SetActorTransform(Restored, false, nullptr, ETeleportType::TeleportPhysics);
	UpdateVisual();
}

FString AVBoRCarryObject::GetStateText() const
{
	if (Session.Hand != INDEX_NONE)
	{
		return Session.bPaused ? TEXT("Paused (tracking)") : bBlocked ? TEXT("Blocked") : TEXT("Held");
	}
	if (bAssisting) { return TEXT("Assisted seating"); }
	if (bDropping) { return TEXT("Dropping vertically"); }
	if (bSeated) { return TEXT("Seated"); }
	return Hovering.IsEmpty() ? TEXT("Normal") : TEXT("Palm nearby");
}

FString AVBoRCarryObject::GetMeasurementText() const
{
	const double Paused = Session.PausedSeconds + (Session.bPaused ? FMath::Max(0.0, GetWorld()->GetTimeSeconds() - Session.PauseAt) : 0.0);
	const double Distance = LoadTarget ? FVector::Distance(GetActorLocation(), LoadTarget->GetGoalPose().GetLocation()) : -1;
	const double Angle = LoadTarget ? FVBoRCarryMotion::AngleDeg(GetActorTransform(), LoadTarget->GetGoalPose()) : -1;
	return FString::Printf(TEXT("Attempts L=%d R=%d | grabbed L=%d R=%d | holding hand=%s\nHeld %.3f s | blocked=%d, %.3f s, max hand/object gap %.2f cm\nTracking pauses=%d, %.3f s | recovery delta %.2f cm | ignored SDK releases=%d\nRelease: %s | final pose: %s / %s\nTarget error %.2f cm / %.2f deg | inside and clear=%d\nPalm only | HandGrab only | throwable=0 | scripted drop %.1f cm/s"),
		Attempts[0], Attempts[1], Successes[0], Successes[1], Session.Hand == 0 ? TEXT("Left") : Session.Hand == 1 ? TEXT("Right") : TEXT("none"),
		Session.HeldSeconds, BlockedCount, BlockedSeconds, MaxHandGapCm, Session.PauseCount, Paused, Session.RecoveryDistanceCm,
		Session.IgnoredReleases, *ReleaseResult, *GetActorLocation().ToString(), *GetActorRotation().ToString(), Distance, Angle,
		LoadTarget && LoadTarget->IsObjectInside(this), DropSpeedCmPerSecond);
}

void AVBoRCarryObject::UpdateVisual()
{
	const FString State = GetStateText();
	if (State == LastState) { return; }
	LastState = State;
	const bool bPaused = Session.bPaused && Session.Hand != INDEX_NONE;
	const bool bHasOverlay = Session.Hand != INDEX_NONE || !Hovering.IsEmpty() || bAssisting || bSeated;
	FLinearColor Color = bPaused ? PausedColor : bBlocked ? BlockedColor : Session.Hand != INDEX_NONE ? HeldColor :
		bAssisting || bSeated ? SeatedColor : HoverColor;
	if (StateOverlay && bHasOverlay)
	{
		StateOverlay->SetVectorParameterValue(TEXT("ColorParam"), Color);
		Mesh->SetOverlayMaterial(StateOverlay);
	}
	else { Mesh->SetOverlayMaterial(nullptr); }
	if (bPaused || bBlocked) { StateAudio->Play(); }
}

void AVBoRCarryObject::EndPlay(const EEndPlayReason::Type Reason)
{
	ClearHoldingRig();
	Grabbable->GetInteractionPointerEventDelegate().RemoveDynamic(this, &AVBoRCarryObject::HandlePointer);
	Mesh->SetOverlayMaterial(nullptr);
	Super::EndPlay(Reason);
}
