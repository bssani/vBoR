#include "VBoRSteeringWheel.h"
#include "VBoRHandRigPawn.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Components/AudioComponent.h"
#include "Engine/StaticMesh.h"
#include "Interaction/IsdkGrabbableComponent.h"
#include "Interaction/IsdkGrabberComponent.h"
#include "Interaction/GrabDetectors/IsdkHandGrabDetector.h"
#include "Interaction/Pointable/IsdkInteractionPointerEvent.h"
#include "IsdkFunctionLibrary.h"
#include "IsdkContentAssetPaths.h"
#include "OculusXRInputFunctionLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Sound/SoundBase.h"
#include "UObject/ConstructorHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogSteeringWheel, Log, All);

UVBoRWheelGrabComponent::UVBoRWheelGrabComponent() { Configure(); }
void UVBoRWheelGrabComponent::Configure() { bIsThrowable = false; MultiGrabBehavior = EIsdkMultiGrabBehavior::MultiGrab; }
void UVBoRWheelGrabComponent::CancelSelection()
{
#if WITH_DEV_AUTOMATION_TESTS
	++CancelCalls;
#endif
	ForceCancel();
}
FTransform UVBoRWheelTransformer::UpdateTransform(const TArray<FIsdkGrabPose>& Poses, const FIsdkTargetTransform& Target)
{
	const auto* Wheel = GetTypedOuter<AVBoRSteeringWheel>();
	return Wheel ? Wheel->AxisPivot->GetComponentTransform().GetRelativeTransform(Target.ParentWorldTransform) : Target.RelativeTransform;
}
FTransform UVBoRWheelTransformer::EndTransform(const FIsdkTargetTransform& Target)
{
	bActive = false; return UpdateTransform({},Target);
}

AVBoRSteeringWheel::AVBoRSteeringWheel()
{
	PrimaryActorTick.bCanEverTick = true; PrimaryActorTick.TickGroup = TG_PostUpdateWork;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("WheelOrigin")));
	AxisPivot = CreateDefaultSubobject<USceneComponent>(TEXT("WheelAxis")); AxisPivot->SetupAttachment(RootComponent);
	WheelMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WheelMesh")); WheelMesh->SetupAttachment(AxisPivot);
	WheelMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision); WheelMesh->SetSimulatePhysics(false);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder")); WheelMesh->SetStaticMesh(Cylinder.Object);
	// 원기둥 +Z 를 축 +X 로 놓음 (근사 원판 지름 36 cm·두께 3 cm, P5d D8)
	MeshRestFromRoot = FTransform(FRotator(90,0,0),FVector::ZeroVector,FVector(0.36,0.36,0.03));
	TopMarker = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TwelveOClock")); TopMarker->SetupAttachment(AxisPivot);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube")); TopMarker->SetStaticMesh(Cube.Object);
	TopMarker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GrabBox = CreateDefaultSubobject<UBoxComponent>(TEXT("RimGrabBox")); GrabBox->SetupAttachment(AxisPivot);
	GrabBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly); GrabBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	GrabBox->SetCollisionResponseToChannel(ECC_WorldDynamic,ECR_Overlap); GrabBox->SetGenerateOverlapEvents(true);
	Grabbable = CreateDefaultSubobject<UIsdkGrabbableComponent>(TEXT("Grabbable")); Grabbable->SetupAttachment(GrabBox);
	GrabComponent = CreateDefaultSubobject<UVBoRWheelGrabComponent>(TEXT("WheelGrab"));
	Transformer = CreateDefaultSubobject<UVBoRWheelTransformer>(TEXT("WheelTransformer"));
	StateAudio = CreateDefaultSubobject<UAudioComponent>(TEXT("LimitClick")); StateAudio->SetupAttachment(AxisPivot); StateAudio->bAutoActivate = false;
	static ConstructorHelpers::FObjectFinder<USoundBase> Sound(IsdkContentAssetPaths::Audio::Interaction_BasicPoke_ButtonPress); StateAudio->SetSound(Sound.Object);
	HoldingGrabbers.SetNum(2);
}

void AVBoRSteeringWheel::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	AngleDeg = 0; RestPivot = AxisPivot->GetRelativeTransform();
	WheelMesh->SetRelativeTransform(MeshRestFromRoot.GetRelativeTransform(RestPivot)); TopMarker->SetRelativeTransform(MarkerRestFromPivot);
	bGrabValid = WheelMesh->GetStaticMesh() && FMath::IsFinite(LimitAngleDeg) && LimitAngleDeg > 0 &&
		FMath::IsFinite(MinGrabRadiusCm) && FMath::IsFinite(MaxGrabRadiusCm) && MinGrabRadiusCm >= 0 && MaxGrabRadiusCm >= MinGrabRadiusCm &&
		FMath::IsFinite(MinAxisDistanceCm) && MinAxisDistanceCm > 0 && FMath::IsFinite(MinPairDistanceCm) && MinPairDistanceCm > 0;
	if (bGrabValid)
	{
		const FBox Bounds = WheelMesh->GetStaticMesh()->GetBoundingBox().TransformBy(WheelMesh->GetRelativeTransform());
		GrabBox->SetRelativeLocation(Bounds.GetCenter()); GrabBox->SetBoxExtent(Bounds.GetExtent()+FVector(FMath::Max(0.f,GrabMarginCm)));
	}
	else { UE_LOG(LogSteeringWheel, Warning, TEXT("%s: Missing mesh or invalid wheel limits/radii; grabbing disabled | approximate geometry | not-tested"),*GetName()); }
	GrabBox->SetCollisionEnabled(bGrabValid ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
}

void AVBoRSteeringWheel::PreInitializeComponents()
{
	RestPivot = AxisPivot->GetRelativeTransform();
	Grabbable->SetGrabCollider(GrabBox); Grabbable->SetGrabInputMethodAllowed(EIsdkGrabInputMethod::Palm,bGrabValid);
	Grabbable->SetGrabInputMethodAllowed(EIsdkGrabInputMethod::Pinch,false);
	GrabComponent->Configure(); GrabComponent->SetTransformTarget(AxisPivot);
	GrabComponent->SetSingleGrabTransformerObject(Transformer); GrabComponent->SetMultiGrabTransformerObject(Transformer);
	Grabbable->SetGrabTransformer(GrabComponent);
	Super::PreInitializeComponents();
}

void AVBoRSteeringWheel::BeginPlay()
{
	Super::BeginPlay();
	auto* Material = LoadObject<UMaterialInterface>(nullptr,IsdkContentAssetPaths::Materials::GrabbableDefault);
	StateOverlay = Material ? UMaterialInstanceDynamic::Create(Material,this) : nullptr;
	if (Material)
	{
		auto* MarkerMaterial = UMaterialInstanceDynamic::Create(Material,this);
		MarkerMaterial->SetVectorParameterValue(TEXT("ColorParam"),FLinearColor(1,0.75,0,1)); TopMarker->SetMaterial(0,MarkerMaterial);
	}
	Grabbable->GetInteractionPointerEventDelegate().AddUniqueDynamic(this,&AVBoRSteeringWheel::HandlePointer); UpdateVisual();
}

bool AVBoRSteeringWheel::ReadHand(int32 Hand, FTransform& Pose) const
{
	const EOculusXRHandType Type = Hand == 0 ? EOculusXRHandType::HandLeft : EOculusXRHandType::HandRight;
	auto* Grabber = HoldingGrabbers[Hand].Get();
	if (!Grabber || !UOculusXRInputFunctionLibrary::IsHandPositionValid(Type) ||
		UOculusXRInputFunctionLibrary::GetTrackingConfidence(Type) != EOculusXRTrackingConfidence::High ||
		!Grabber->GetHandGrabDetector() || !Grabber->GetHandGrabDetector()->GetPalmGrabCollider()) { return false; }
	Grabber->GetPalmGrabTransform(Pose); return !Pose.ContainsNaN();
}
bool AVBoRSteeringWheel::ReadPalmClosed(int32 Hand, bool& Closed) const
{
	const auto* Pawn = Cast<AVBoRHandRigPawn>(UGameplayStatics::GetPlayerPawn(this,0));
	return Pawn && (Hand == 0 ? Pawn->LeftRig->ReadPalmGrab(Closed) : Pawn->RightRig->ReadPalmGrab(Closed));
}

bool AVBoRSteeringWheel::BeginHold(int32 Hand, UIsdkGrabberComponent* Grabber)
{
	if (HandOrder.Contains(Hand)) { return true; }
	HoldingGrabbers[Hand] = Grabber;
	FTransform Pose;
	if (!bGrabValid || !ReadHand(Hand,Pose)) { HoldingGrabbers[Hand] = nullptr; bCancelPending = true; return false; }
	const FVector Point = (RestPivot * GetActorTransform()).InverseTransformPosition(Pose.GetLocation());
	const double Radius = FVector(0,Point.Y,Point.Z).Size();
	if (Radius < MinGrabRadiusCm || Radius > MaxGrabRadiusCm) { HoldingGrabbers[Hand] = nullptr; bCancelPending = true; return false; }
	HandOrder.Add(Hand); bBasisValid = false; AdvanceMotion(); return true;
}
void AVBoRSteeringWheel::EndHold(int32 Hand)
{
	HandOrder.Remove(Hand); HoldingGrabbers[Hand] = nullptr; bBasisValid = false;
	if (HandOrder.IsEmpty()) { bPaused = false; }
	else { AdvanceMotion(); }
}

void AVBoRSteeringWheel::HandlePointer(const FIsdkInteractionPointerEvent& Event)
{
	auto* Grabber = Cast<UIsdkGrabberComponent>(Event.Interactor); EIsdkHandedness Handedness;
	if (!Grabber || !UIsdkFunctionLibrary::GetHandednessFromInteractor(Grabber,Handedness)) { return; }
	const int32 Hand = Handedness == EIsdkHandedness::Left ? 0 : 1;
	if (Event.Type == EIsdkPointerEventType::Hover) { Hovering.Add(Grabber); }
	else if (Event.Type == EIsdkPointerEventType::Unhover) { Hovering.Remove(Grabber); }
	else if (Event.Type == EIsdkPointerEventType::Select) { BeginHold(Hand,Grabber); }
	else if ((Event.Type == EIsdkPointerEventType::Unselect || Event.Type == EIsdkPointerEventType::Cancel) &&
		!bCancelling && HandOrder.Contains(Hand) && HoldingGrabbers[Hand] == Grabber)
	{
		FTransform Pose; const bool WasPaused = bPaused;
		if (!ReadHand(Hand,Pose)) { bPaused = true; return; }
		// 놓기 전 마지막 자세도 반영함. 알림은 Tick 까지 보관함 (P5d E14)
		AdvanceMotion();
		if (!WasPaused && !bPaused) { EndHold(Hand); }
	}
}

void AVBoRSteeringWheel::AdvanceMotion()
{
	if (HandOrder.IsEmpty()) { return; }
	const FTransform RestWorld = RestPivot * GetActorTransform(); FVector Points[2];
	for (int32 Hand : HandOrder)
	{
		FTransform Pose;
		if (!ReadHand(Hand,Pose) || Pose.ContainsNaN()) { bPaused = true; return; }
		Points[Hand] = RestWorld.InverseTransformPosition(Pose.GetLocation()); Points[Hand].X = 0;
	}
	const int32 First = HandOrder[0]; int32 Mode = First;
	FVector Direction = Points[First]; bool Valid = Direction.Size() >= MinAxisDistanceCm;
	if (HandOrder.Num() == 2)
	{
		const FVector Segment = Points[HandOrder[1]]-Points[First];
		if (Segment.Size() >= MinPairDistanceCm) { Direction = Segment; Mode = 2+First; Valid = true; }
	}
	Direction = Direction.GetSafeNormal();
	// +X 를 바라보는 운전자 기준 +Z → +Y 가 시계 방향임 (P5d D2, deg)
	const double Delta = FMath::RadiansToDegrees(FMath::Atan2(LastDirection.Z*Direction.Y-LastDirection.Y*Direction.Z,FVector::DotProduct(LastDirection,Direction)));
	if (bPaused)
	{
		RecoveryDiscardDeg = bBasisValid && Valid && Mode == LastMode ? FMath::Abs(Delta) : 0;
		UE_LOG(LogSteeringWheel, Log, TEXT("%s recovery discarded %.3f deg | not-tested"),*GetName(),RecoveryDiscardDeg);
	}
	if (!bPaused && bBasisValid && Valid && Mode == LastMode && FMath::Abs(Delta) <= 90.0+1.e-6) { ApplyAngle(Delta); }
	// 축 근처·계산 방식 변경·추적 복구 frame 은 기준만 갱신함 (P5d D3·D4·D6)
	LastDirection = Direction; LastMode = Mode; bBasisValid = Valid; bPaused = false;
}

void AVBoRSteeringWheel::ApplyAngle(double DeltaDeg)
{
	AngleDeg = FMath::Clamp(AngleDeg+DeltaDeg,-static_cast<double>(LimitAngleDeg),static_cast<double>(LimitAngleDeg));
	AxisPivot->SetRelativeTransform(FTransform(FQuat(FVector::XAxisVector,FMath::DegreesToRadians(-AngleDeg)))*RestPivot);
	for (int32 Side = 0; Side < 2; ++Side)
	{
		const int32 Sign = Side == 0 ? -1 : 1;
		if (LimitAngleDeg-Sign*AngleDeg >= 10-1.e-6) { bLimitArmed[Side] = true; }
		if (bLimitArmed[Side] && Sign*AngleDeg >= LimitAngleDeg-1.e-6)
		{ bLimitArmed[Side] = false; PendingLimits.Add(Sign); }
	}
}

void AVBoRSteeringWheel::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!FMath::IsFinite(DeltaSeconds) || DeltaSeconds <= 0) { return; }
	// 한 손의 손실도 먼저 확인함 (다른 손의 놓기가 보류를 지우지 않게, P5d D6)
	AdvanceMotion();
	const TArray<int32> Hands = HandOrder;
	for (int32 Hand : Hands)
	{
		FTransform Pose; bool Closed = true;
		if (ReadHand(Hand,Pose) && ReadPalmClosed(Hand,Closed) && !Closed) { EndHold(Hand); bCancelPending = true; }
	}
	if (bCancelPending)
	{
		bCancelPending = false; bCancelling = true;
		// SDK 는 전체 선택만 취소함. 유효 손의 소유·기준은 유지함 (P5d D4·D5)
		GrabComponent->CancelSelection(); bCancelling = false;
	}
	UpdateVisual();
	const TArray<int32> Notifications = MoveTemp(PendingLimits);
	for (int32 Direction : Notifications)
	{
		StateAudio->Play(); OnLimitReached.Broadcast(Direction);
		if (IsActorBeingDestroyed()) { return; }
	}
}
void AVBoRSteeringWheel::UpdateVisual()
{
	if (!StateOverlay) { return; }
	StateOverlay->SetVectorParameterValue(TEXT("ColorParam"),!HandOrder.IsEmpty() ? GrabColor : Hovering.IsEmpty() ? FLinearColor::Transparent : HoverColor);
	WheelMesh->SetOverlayMaterial(StateOverlay);
}
FString AVBoRSteeringWheel::GetDebugText() const
{
	return FString::Printf(TEXT("Approximate geometry | angle=%.3f deg hands=%d paused=%d discarded=%.3f deg | not-tested"),AngleDeg,HandOrder.Num(),bPaused,RecoveryDiscardDeg);
}
void AVBoRSteeringWheel::EndPlay(const EEndPlayReason::Type Reason)
{
	Grabbable->GetInteractionPointerEventDelegate().RemoveDynamic(this,&AVBoRSteeringWheel::HandlePointer);
	WheelMesh->SetOverlayMaterial(nullptr); HandOrder.Empty(); HoldingGrabbers.Empty(); PendingLimits.Empty();
	Super::EndPlay(Reason);
}
