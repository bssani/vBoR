#include "VBoRCarryObject.h"
#include "VBoRLoadTarget.h"
#include "VBoRHandRigPawn.h"
#include "Components/StaticMeshComponent.h"
#include "Components/AudioComponent.h"
#include "Components/BoxComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "PhysicsEngine/BodySetup.h"
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

DEFINE_LOG_CATEGORY_STATIC(LogVBoRCarry, Log, All);

UVBoRCarryGrabTransformer::UVBoRCarryGrabTransformer() { ConfigureCarry(); }
void UVBoRCarryGrabTransformer::ConfigureCarry()
{
	bIsThrowable = false;
	MultiGrabBehavior = EIsdkMultiGrabBehavior::MultiGrab;
}

void UVBoRCarryTransformer::BeginTransform(const TArray<FIsdkGrabPose>& Poses, const FIsdkTargetTransform& Target)
{
	bActive = !Poses.IsEmpty();
}

FTransform UVBoRCarryTransformer::UpdateTransform(const TArray<FIsdkGrabPose>& Poses, const FIsdkTargetTransform& Target)
{
	AVBoRCarryObject* Object = GetTypedOuter<AVBoRCarryObject>();
	// 소유 손의 Tick 에서만 갱신함 (SDK End/Begin 순서와 복구 frame 분리, P1 D6)
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
	LandingPreview = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LandingPreview"));
	LandingPreview->SetupAttachment(Mesh);
	// 손의 이동·회전과 독립된 착지 자세를 표시함 (P1c D3)
	LandingPreview->SetAbsolute(true, true, true);
	LandingPreview->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	LandingPreview->SetGenerateOverlapEvents(false);
	LandingPreview->SetCastShadow(false);
	LandingPreview->SetVisibility(false);
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

void AVBoRCarryObject::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplyBoxSize();
}

void AVBoRCarryObject::ApplyBoxSize()
{
	const FBox Bounds = GetMeshLocalBounds();
	if (!Bounds.IsValid || Bounds.GetSize().GetMin() <= KINDA_SMALL_NUMBER) { return; }
	// 같은 root scale 을 메시·simple collision·적재 판정에서 공유함 (P1b D2, cm)
	BoxSizeCm = BoxSizeCm.GetAbs().ComponentMax(FVector(1));
	SetActorScale3D(BoxSizeCm / Bounds.GetSize());
}

void AVBoRCarryObject::PreInitializeComponents()
{
	if (!FMath::IsFinite(LevelLiftMaxCm) || LevelLiftMaxCm < 0)
	{
		UE_LOG(LogVBoRCarry, Warning, TEXT("Invalid LevelLiftMaxCm on '%s'; using 0 cm."), *GetName());
		LevelLiftMaxCm = 0;
	}
	// 자식 설정 반영 뒤 SDK BeginPlay 전에 Palm 만 허용하고 Throwable 을 끔 (P1 D1)
	Grabbable->SetGrabInputMethodAllowed(EIsdkGrabInputMethod::Palm, true);
	Grabbable->SetGrabInputMethodAllowed(EIsdkGrabInputMethod::Pinch, false);
	Grabbable->SetGrabCollider(Mesh);
	GrabTransformer->ConfigureCarry();
	GrabTransformer->SetSingleGrabTransformerObject(CarryTransformer);
	GrabTransformer->SetMultiGrabTransformerObject(CarryTransformer);
	GrabTransformer->SetTransformTarget(Mesh);
	Grabbable->SetGrabTransformer(GrabTransformer);
	Mesh->SetSimulatePhysics(false);
	Mesh->SetEnableGravity(false);
	Super::PreInitializeComponents();
}

void AVBoRCarryObject::BeginPlay()
{
	Super::BeginPlay();
	if (!HasSimpleCollision())
	{
		UE_LOG(LogVBoRCarry, Warning, TEXT("No simple collision on carry mesh '%s' (%s); movement disabled. Add simple collision to the child mesh."),
			*GetPathNameSafe(Mesh->GetStaticMesh()), *GetName());
	}
	Session.SetRestPose(GetActorTransform());
	UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, IsdkContentAssetPaths::Materials::GrabbableDefault);
	StateOverlay = Material ? UMaterialInstanceDynamic::Create(Material, this) : nullptr;
	PreviewMaterial = Material ? UMaterialInstanceDynamic::Create(Material, this) : nullptr;
	Mesh->SetOverlayMaterial(nullptr);
	Grabbable->GetInteractionPointerEventDelegate().AddUniqueDynamic(this, &AVBoRCarryObject::HandlePointer);
	UpdateVisual();
}

FBox AVBoRCarryObject::GetMeshLocalBounds() const
{
	return Mesh->GetStaticMesh() ? Mesh->GetStaticMesh()->GetBoundingBox() : FBox(ForceInit);
}

FBox AVBoRCarryObject::GetCollisionBoundsInPose(const FTransform& Pose, const FTransform& Reference) const
{
	FBox Bounds(ForceInit);
	const UBodySetup* Body = Mesh->GetBodySetup();
	if (!Body || Pose.ContainsNaN()) { return Bounds; }
	const FKAggregateGeom& Geometry = Body->AggGeom;
	FTransform Frame = Pose.GetRelativeTransform(Reference);
	Frame.RemoveScaling();
	// Convex 의 실제 꼭짓점으로 목표 안 여부를 잼 (외접 상자 회전으로 원통이 커지지 않게, P1 R3-2)
	for (const FKConvexElem& Hull : Geometry.ConvexElems)
	{
		for (const FVector& Vertex : Hull.VertexData)
		{
			Bounds += Reference.InverseTransformPosition(Pose.TransformPosition(Hull.GetTransform().TransformPosition(Vertex)));
		}
	}
	for (const FKBoxElem& Box : Geometry.BoxElems) { Bounds += Box.GetFinalScaled(Pose.GetScale3D(), FTransform::Identity).CalcAABB(Frame, 1.0f); }
	for (const FKSphereElem& Sphere : Geometry.SphereElems) { Bounds += Sphere.GetFinalScaled(Pose.GetScale3D(), FTransform::Identity).CalcAABB(Frame, 1.0f); }
	for (const FKSphylElem& Capsule : Geometry.SphylElems) { Bounds += Capsule.GetFinalScaled(Pose.GetScale3D(), FTransform::Identity).CalcAABB(Frame, 1.0f); }
	return Bounds;
}

bool AVBoRCarryObject::HasSimpleCollision() const
{
	const UBodySetup* Body = Mesh->GetBodySetup();
	return Body && Body->AggGeom.GetElementCount() > 0;
}

bool AVBoRCarryObject::IsPoseClear(const FTransform& Pose) const
{
	if (!GetWorld() || Pose.ContainsNaN() || !HasSimpleCollision()) { return false; }
	// 등록된 Mesh 의 simple collision 을 그대로 씀 (원통 외접 상자 모서리 제거, P1 R3-2)
	FComponentQueryParams Params(SCENE_QUERY_STAT(VBoRCarryOverlap), this);
	TArray<FOverlapResult> Overlaps;
	GetWorld()->ComponentOverlapMulti(Overlaps, Mesh, Pose.GetLocation(), Pose.GetRotation(), Params);
	for (const FOverlapResult& Overlap : Overlaps)
	{
		if (!Overlap.bBlockingHit) { continue; }
		TArray<FHitResult> Contacts;
		GetWorld()->ComponentSweepMulti(Contacts, Mesh, Pose.GetLocation(), Pose.GetLocation(), Pose.GetRotation(), Params);
		bool bContactFound = false;
		for (const FHitResult& Contact : Contacts)
		{
			if (!Contact.bBlockingHit || Contact.GetComponent() != Overlap.GetComponent()) { continue; }
			bContactFound = true;
			// 바닥에 딱 닿은 자세의 수치 오차만 허용함 (기존 접촉 여유 0.05 mm, P1 R3-2)
			if (!Contact.bStartPenetrating || Contact.PenetrationDepth > 0.005) { return false; }
		}
		if (!bContactFound) { return false; }
	}
	return true;
}

bool AVBoRCarryObject::TryMoveToPose(const FTransform& Requested)
{
	if (!GetWorld() || Requested.ContainsNaN()) { return false; }
	const FTransform Current = GetActorTransform();
	FTransform Goal = Requested;
	// 안착 보조·양손 간격도 등록된 충돌 형상 크기를 바꾸지 않음 (P1 D6)
	Goal.SetScale3D(Current.GetScale3D());
	bool bRotationBlocked = false;
	if (!Goal.GetRotation().Equals(Current.GetRotation()) &&
		!IsPoseClear(FTransform(Goal.GetRotation(), Current.GetLocation(), Current.GetScale3D())))
	{
		Goal.SetRotation(Current.GetRotation());
		bRotationBlocked = true;
	}
	FComponentQueryParams Params(SCENE_QUERY_STAT(VBoRCarrySweep), this);
	TArray<FHitResult> Hits;
	GetWorld()->ComponentSweepMulti(Hits, Mesh, Current.GetLocation(), Goal.GetLocation(), Goal.GetRotation(), Params);
	const FVector Travel = Goal.GetLocation() - Current.GetLocation();
	double StopTime = 1.0;
	bool bHit = false;
	for (const FHitResult& Hit : Hits)
	{
		if (!Hit.bBlockingHit) { continue; }
		// 끝점의 얕은 접촉은 안착을 막지 않음. 최종 겹침을 별도로 확인함 (여유 0.05 mm, P1 R3-2)
		if (!Hit.bStartPenetrating && (1.0 - Hit.Time) * Travel.Size() <= 0.005 && IsPoseClear(Goal)) { continue; }
		// 접촉 오차 0.05 mm 안에서 옆으로 빼거나 표면에서 멀어지는 이동 허용 (P1 R3-2)
		if (Hit.bStartPenetrating && FVector::DotProduct(Travel, Hit.Normal) >= 0 &&
			(Hit.PenetrationDepth <= 0.005 || FVector::DotProduct(Travel, Hit.Normal) > KINDA_SMALL_NUMBER)) { continue; }
		bHit = true;
		StopTime = FMath::Min(StopTime, FMath::Max(0.0, Hit.Time - (Travel.Size() > SMALL_NUMBER ? 0.005 / Travel.Size() : 0.0)));
	}
	if (bHit) { Goal.SetLocation(FVBoRCarryMotion::SweepStop(Current.GetLocation(), Goal.GetLocation(), StopTime)); }
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

bool AVBoRCarryObject::ReadHand(int32 Hand, FTransform& Pose) const
{
	if (Hand < 0 || Hand > 1) { return false; }
	UIsdkGrabberComponent* Grabber = HoldingGrabbers[Hand];
	if (!Grabber || !IsHandTracked(Hand) || !Grabber->GetHandGrabDetector() ||
		!Grabber->GetHandGrabDetector()->GetPalmGrabCollider()) { return false; }
	Grabber->GetPalmGrabTransform(Pose);
	Pose.SetScale3D(FVector::OneVector);
	return !Pose.ContainsNaN();
}

bool AVBoRCarryObject::ReadHands(FTransform* Poses) const
{
	bool bTracked = true;
	for (int32 Hand = 0; Hand < 2; ++Hand)
	{
		Poses[Hand] = Session.LastHands[Hand];
		if (Session.HasHand(Hand) && !ReadHand(Hand, Poses[Hand])) { bTracked = false; }
	}
	return bTracked;
}

void AVBoRCarryObject::BindHoldingRig(int32 Hand, UIsdkGrabberComponent* Grabber)
{
	ClearHoldingRig(Hand);
	HoldingGrabbers[Hand] = Grabber;
	HoldingRigs[Hand] = Grabber ? Grabber->GetTypedOuter<UIsdkHandRigComponent>() : nullptr;
	if (HoldingRigs[Hand])
	{
		// SDK selection 이 사라져도 각 소유 손의 Palm 놓기를 읽음 (P1 D6)
		PalmSelectHandles[Hand] = HoldingRigs[Hand]->OnPalmSelectDelegate.AddUObject(this, &AVBoRCarryObject::HandlePalmSelect);
		PalmReleaseHandles[Hand] = HoldingRigs[Hand]->OnPalmUnselectDelegate.AddUObject(this, &AVBoRCarryObject::HandlePalmRelease);
	}
	bPalmOpen[Hand] = false;
}

void AVBoRCarryObject::ClearHoldingRig(int32 Hand)
{
	for (int32 Id = 0; Id < 2; ++Id)
	{
		if (Hand != INDEX_NONE && Hand != Id) { continue; }
		if (HoldingRigs[Id])
		{
			HoldingRigs[Id]->OnPalmSelectDelegate.Remove(PalmSelectHandles[Id]);
			HoldingRigs[Id]->OnPalmUnselectDelegate.Remove(PalmReleaseHandles[Id]);
		}
		HoldingRigs[Id] = nullptr;
		HoldingGrabbers[Id] = nullptr;
		bPalmOpen[Id] = false;
	}
}

void AVBoRCarryObject::HandlePalmSelect(UIsdkRigComponent* Rig)
{
	for (int32 Id = 0; Id < 2; ++Id) { if (HoldingRigs[Id] == Rig) { bPalmOpen[Id] = false; } }
}
void AVBoRCarryObject::HandlePalmRelease(UIsdkRigComponent* Rig)
{
	for (int32 Id = 0; Id < 2; ++Id) { if (HoldingRigs[Id] == Rig) { bPalmOpen[Id] = true; } }
}

bool AVBoRCarryObject::BeginCarry(int32 Hand, UIsdkGrabberComponent* Grabber, const FTransform& Pose)
{
	if (Pose.ContainsNaN() || !Session.Begin(Hand, Pose, GetActorTransform(), GetWorld()->GetTimeSeconds())) { return false; }
	BindHoldingRig(Hand, Grabber);
	bDropping = bAssisting = bSeated = bBlocked = bReleasedOutside = false;
	Outcome = EVBoRCarryOutcome::None;
	++Successes[Hand];
	OnGrabbed.Broadcast(this);
	return true;
}

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
		if (Session.HasHand(Hand)) { if (HoldingGrabbers[Hand] == Grabber) { bPalmOpen[Hand] = false; } return; }
		FTransform OwnedPoses[2];
		if (!ReadHands(OwnedPoses)) { Session.Pause(Time); return; }
		if (Session.bPaused || !IsHandTracked(Hand)) { return; }
		if (!Grabber->GetHandGrabDetector() || !Grabber->GetHandGrabDetector()->GetPalmGrabCollider()) { return; }
		FTransform Pose;
		Grabber->GetPalmGrabTransform(Pose);
		Pose.SetScale3D(FVector::OneVector);
		// 두 번째 손이 잡는 순간 첫 손도 다시 읽음 (직전 Tick 위치로 피벗이 튀지 않게, P1 D6)
		if (Session.Hand != INDEX_NONE) { Session.Rebase(OwnedPoses, GetActorTransform()); }
		if (!BeginCarry(Hand, Grabber, Pose)) { return; }
	}
	else if ((Event.Type == EIsdkPointerEventType::Unselect || Event.Type == EIsdkPointerEventType::Cancel) &&
		Session.HasHand(Hand) && Grabber == HoldingGrabbers[Hand])
	{
		FTransform Poses[2];
		if (!ReadHands(Poses) || Session.bPaused) { Session.Release(Hand, false, Time); }
		else { ReleaseCarry(Hand, Poses); }
	}
	UpdateVisual();
}

void AVBoRCarryObject::AdvanceHeldPose(float DeltaSeconds)
{
	if (Session.Hand == INDEX_NONE) { return; }
	FTransform Poses[2];
	const bool bTracked = ReadHands(Poses);
	FTransform Desired;
	if (!Session.UpdateHands(bTracked, Poses, GetActorTransform(), GetWorld()->GetTimeSeconds(), TiltLimitDeg, Desired)) { UpdateVisual(); return; }
	MoveHeldPose(Desired, DeltaSeconds);
	UpdateVisual();
}
void AVBoRCarryObject::MoveHeldPose(const FTransform& Desired, float DeltaSeconds)
{
	if (Session.bPaused) { return; }
	if (Desired.ContainsNaN()) { return; }
	FTransform Request = Desired;
	Request.SetScale3D(GetActorScale3D());
	// 같은 요청도 남은 회전을 이어 풂 (초당 예산 안에서 요청 쪽으로 수렴, P1 R6-1)
	ResolveHeldContact(Request, DeltaSeconds);
	const double Distance = FVector::Distance(Request.GetLocation(), GetActorLocation());
	const double Angle = FVBoRCarryMotion::AngleDeg(Request, GetActorTransform());
	const bool bNewBlocked = bBlocked ? !(Distance < BlockedExitDistanceCm && Angle < BlockedExitAngleDeg) :
		Distance >= BlockedEnterDistanceCm || Angle >= BlockedEnterAngleDeg;
	if (bNewBlocked && !bBlocked) { ++BlockedCount; StateAudio->Play(); }
	bBlocked = bNewBlocked;
	// 잡은 자리 오프셋을 빼고 막혀서 못 간 거리만 잼 (P1 R1-5, cm)
	if (bBlocked) { MaxRequestedGapCm = FMath::Max(MaxRequestedGapCm, FVector::Distance(Desired.GetLocation(), GetActorLocation())); }
}

void AVBoRCarryObject::ReleaseCarry(int32 Hand, const FTransform* TrackedPoses)
{
	if (Hand == INDEX_NONE) { Hand = Session.Hand; }
	if (!Session.HasHand(Hand)) { return; }
	// Pointer/Tick 에서 두 소유 손 유효성을 확인한 뒤 들어옴 (보류 release 는 Session 에서 재차 차단, P1 D6)
	const FTransform* Poses = TrackedPoses ? TrackedPoses : Session.LastHands;
	const FTransform Object = GetActorTransform();
	const bool bLastReleased = Session.Release(Hand, true, GetWorld()->GetTimeSeconds(), Poses, &Object);
	if (!Session.HasHand(Hand)) { ClearHoldingRig(Hand); }
	if (!bLastReleased) { UpdateVisual(); return; }
	// 같은 시도의 재잡기에서도 앞선 잡기 시간을 남김 (P1 R1-6)
	CompletedMetrics.HeldSeconds += Session.HeldSeconds;
	ClearHoldingRig();
	bBlocked = false;
	FTransform Goal = LoadTarget ? LoadTarget->GetGoalPose() : GetActorTransform();
	Goal.SetScale3D(GetActorScale3D());
	bAssisting = bUsePlacementAssist && LoadTarget && FVBoRCarryMotion::CanAssist(GetActorTransform(), Goal, LoadTarget->IsStaticFit(this), AssistDistanceCm, AssistAngleDeg);
	bDropping = !bAssisting;
	if (bDropping && !bUsePlacementAssist)
	{
		FTransform DropStart;
		FTransform Landing;
		bool bLevelBlocked = false;
		CalculateReleaseDrop(DropStart, Landing, bLevelBlocked);
		// 검사한 끝 자세로 바로 바꿈 (중간 회전의 관통 방지, P1c D1·D2)
		if (IsPoseClear(DropStart)) { SetActorTransform(DropStart, false, nullptr, ETeleportType::TeleportPhysics); }
		LevelResult = !bLevelOnRelease ? TEXT("level off") : bLevelBlocked ? TEXT("level blocked") : TEXT("level");
		if (bLevelBlocked) { UE_LOG(LogVBoRCarry, Log, TEXT("level blocked on '%s'; retaining tilt for vertical drop."), *GetName()); }
	}
	else { LevelResult = TEXT("level off (placement assist)"); }
	ReleaseResult = bAssisting ? TEXT("assisted seating (in progress)") : TEXT("vertical drop");
	if (bAssisting) { AssistFrom = GetActorTransform(); AssistGoal = Goal; AssistStartedAt = GetWorld()->GetTimeSeconds(); }
	OnCarryReleased.Broadcast(this);
	UpdateVisual();
}

void AVBoRCarryObject::FinishPlacement()
{
	// 목표 밖 낙하를 성공 색·이벤트로 보이지 않게 함 (P1 R1-4)
	bSeated = LoadTarget && LoadTarget->IsObjectInside(this);
	if (bAssisting && !bSeated && LoadTarget)
	{
		UE_LOG(LogVBoRCarry, Warning, TEXT("Assisted seating finished outside loading region: target '%s', goal size %s cm, region size %s cm. Check target configuration."),
			*LoadTarget->GetName(), *LoadTarget->GoalSizeCm.ToString(), *(LoadTarget->LoadingRegion->GetScaledBoxExtent() * 2).ToString());
	}
	bReleasedOutside = !bSeated;
	Outcome = bSeated ? (bAssisting ? EVBoRCarryOutcome::Assisted : EVBoRCarryOutcome::DroppedInside) : EVBoRCarryOutcome::DroppedOutside;
	CompletedMetrics.Assisted += Outcome == EVBoRCarryOutcome::Assisted ? 1 : 0;
	CompletedMetrics.DroppedInside += Outcome == EVBoRCarryOutcome::DroppedInside ? 1 : 0;
	CompletedMetrics.DroppedOutside += Outcome == EVBoRCarryOutcome::DroppedOutside ? 1 : 0;
	ReleaseResult = Outcome == EVBoRCarryOutcome::Assisted ? TEXT("assisted seating") : bSeated ?
		TEXT("vertical drop -> inside target") : TEXT("vertical drop -> outside target");
	bDropping = bAssisting = false;
	if (bSeated) { OnSeated.Broadcast(this); }
	UpdateVisual();
}

void AVBoRCarryObject::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (Session.Hand != INDEX_NONE)
	{
		const bool bRecovering = Session.bPaused;
		AdvanceHeldPose(DeltaSeconds);
		for (int32 Hand = 0; Hand < 2; ++Hand)
		{
			if (!Session.HasHand(Hand)) { continue; }
			bool bPalmClosed = true;
			bool bHasPalmState = false;
			if (const UVBoRHandRigLeft* Rig = Cast<UVBoRHandRigLeft>(HoldingRigs[Hand])) { bHasPalmState = Rig->ReadPalmGrab(bPalmClosed); }
			else if (const UVBoRHandRigRight* RightRig = Cast<UVBoRHandRigRight>(HoldingRigs[Hand])) { bHasPalmState = RightRig->ReadPalmGrab(bPalmClosed); }
			if (bHasPalmState) { bPalmOpen[Hand] = !bPalmClosed; }
			// 복구 frame 은 양손 모두 rebase 만 함 (loss 때 온 옛 release 적용 방지, P1 D6)
			if (!bRecovering && bPalmOpen[Hand] && !Session.bPaused) { ReleaseCarry(Hand); }
		}
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
	// 손실 보류·잡은 상태는 제외함. 레벨 바닥 대신 시험대의 물리 바닥 기준 (P1b D6, cm)
	if (bDestroyAtFloor && Session.Hand == INDEX_NONE && (bDropping || bReleasedOutside))
	{
		const FBox Bounds = GetCollisionBoundsInPose(GetActorTransform(), FTransform::Identity);
		if (Bounds.IsValid && Bounds.Min.Z <= FloorHeightCm + FloorToleranceCm)
		{
			if (bDropping) { FinishPlacement(); }
			Destroy();
			return;
		}
	}
	UpdateVisual();
}

void AVBoRCarryObject::ResetToPose(const FTransform& Pose)
{
	// 소유 손을 먼저 지움 (ForceCancel 의 동기 callback 을 놓기로 세지 않음, P1 R reset)
	Session = FVBoRCarrySession();
	Session.SetRestPose(Pose);
	CompletedMetrics = FVBoRCarryMetrics();
	ClearHoldingRig();
	GrabTransformer->ForceCancel();
	Hovering.Empty();
	Attempts[0] = Attempts[1] = Successes[0] = Successes[1] = 0;
	BlockedCount = 0;
	BlockedSeconds = MaxRequestedGapCm = 0;
	bDropping = bAssisting = bSeated = bBlocked = bReleasedOutside = false;
	Outcome = EVBoRCarryOutcome::None;
	ReleaseResult = TEXT("none (reset)");
	LevelResult = TEXT("level idle");
	FTransform Restored = Pose;
	Restored.SetScale3D(GetActorScale3D());
	SetActorTransform(Restored, false, nullptr, ETeleportType::TeleportPhysics);
	UpdateVisual();
}

FString AVBoRCarryObject::GetStateText() const
{
	// 잡힘·막힘 상태보다 원인을 먼저 표시함 (자식 메시 교체 시 조용한 실패 방지, P1 R4-2)
	if (!HasSimpleCollision()) { return TEXT("No simple collision"); }
	if (Session.Hand != INDEX_NONE)
	{
		return Session.bPaused ? TEXT("Paused (tracking)") : bBlocked ? TEXT("Blocked") : Session.IsTwoHanded() ? TEXT("Held (two hands)") : TEXT("Held (one hand)");
	}
	if (bAssisting) { return TEXT("Assisted seating"); }
	if (bDropping) { return TEXT("Dropping vertically"); }
	if (bSeated) { return TEXT("Seated"); }
	if (bReleasedOutside) { return TEXT("Released (outside target)"); }
	return Hovering.IsEmpty() ? TEXT("Normal") : TEXT("Palm nearby");
}

FVBoRCarryMetrics AVBoRCarryObject::GetMetrics() const
{
	FVBoRCarryMetrics Metrics = CompletedMetrics;
	Metrics.Grabbed = Successes[0] + Successes[1];
	Metrics.TwoHandEntries = Session.TwoHandEntries;
	Metrics.TwoHandSeconds = Session.TwoHandSeconds;
	Metrics.BlockedCount = BlockedCount;
	Metrics.PauseCount = Session.PauseCount;
	Metrics.HeldSeconds += Session.Hand != INDEX_NONE ? Session.HeldSeconds : 0;
	Metrics.BlockedSeconds = BlockedSeconds;
	Metrics.PausedSeconds = Session.PausedSeconds + (Session.bPaused ? FMath::Max(0.0, GetWorld()->GetTimeSeconds() - Session.PauseAt) : 0.0);
	Metrics.Outcome = Outcome;
	return Metrics;
}

FString AVBoRCarryObject::GetMeasurementText() const
{
	const double Paused = Session.PausedSeconds + (Session.bPaused ? FMath::Max(0.0, GetWorld()->GetTimeSeconds() - Session.PauseAt) : 0.0);
	const double Distance = LoadTarget ? FVector::Distance(GetActorLocation(), LoadTarget->GetGoalPose().GetLocation()) : -1;
	const double Angle = LoadTarget ? FVBoRCarryMotion::AngleDeg(GetActorTransform(), LoadTarget->GetGoalPose()) : -1;
	return FString::Printf(TEXT("Attempts L=%d R=%d | grabbed L=%d R=%d | holding hand=%s | mode=%s | two-hand=%d / %.3f s\nHeld %.3f s | blocked=%d, %.3f s, max requested/actual gap %.2f cm\nTracking pauses=%d, %.3f s | recovery delta %.2f cm | ignored SDK releases=%d\nRelease: %s | final pose: %s / %s\nTarget pose error %.2f cm / %.2f deg | loading region inside and clear=%d\nPalm only | HandGrab only | throwable=0 | scripted drop %.1f cm/s\n%s | preview %s"),
		Attempts[0], Attempts[1], Successes[0], Successes[1], Session.Hand == 0 ? TEXT("Left") : Session.Hand == 1 ? TEXT("Right") : TEXT("none"),
	Session.IsTwoHanded() ? TEXT("two hands") : Session.Hand != INDEX_NONE ? TEXT("one hand") : TEXT("none"), Session.TwoHandEntries, Session.TwoHandSeconds,
		Session.HeldSeconds, BlockedCount, BlockedSeconds, MaxRequestedGapCm, Session.PauseCount, Paused, Session.RecoveryDistanceCm,
		Session.IgnoredReleases, *ReleaseResult, *GetActorLocation().ToString(), *GetActorRotation().ToString(), Distance, Angle,
		LoadTarget && LoadTarget->IsObjectInside(this), DropSpeedCmPerSecond, *LevelResult,
		LandingPreview->IsVisible() ? (bPreviewInside ? TEXT("inside") : TEXT("outside")) : TEXT("hidden"));
}

void AVBoRCarryObject::UpdateVisual()
{
	// Held 상태가 같아도 그 frame 의 실제 박스 자세로 다시 계산함 (P1c E11)
	UpdateLandingPreview();
	const FString State = GetStateText();
	if (State == LastState) { return; }
	LastState = State;
	const bool bPaused = Session.bPaused && Session.Hand != INDEX_NONE;
	const bool bHasOverlay = Session.Hand != INDEX_NONE || !Hovering.IsEmpty() || bAssisting || bSeated || bReleasedOutside;
	FLinearColor Color = bPaused ? PausedColor : bBlocked ? BlockedColor : Session.Hand != INDEX_NONE ? HeldColor :
		bSeated ? SeatedColor : bReleasedOutside ? ReleasedColor : bAssisting ? HeldColor : HoverColor;
	if (StateOverlay && bHasOverlay)
	{
		StateOverlay->SetVectorParameterValue(TEXT("ColorParam"), Color);
		Mesh->SetOverlayMaterial(StateOverlay);
	}
	else { Mesh->SetOverlayMaterial(nullptr); }
	if (bPaused) { StateAudio->Play(); }
}

void AVBoRCarryObject::EndPlay(const EEndPlayReason::Type Reason)
{
	ClearHoldingRig();
	Grabbable->GetInteractionPointerEventDelegate().RemoveDynamic(this, &AVBoRCarryObject::HandlePointer);
	Mesh->SetOverlayMaterial(nullptr);
	Super::EndPlay(Reason);
}
