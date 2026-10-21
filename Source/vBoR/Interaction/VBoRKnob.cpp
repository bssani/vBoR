#include "VBoRKnob.h"
#include "VBoRGrabCollider.h"
#include "VBoRHandRigPawn.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
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

DEFINE_LOG_CATEGORY_STATIC(LogKnob, Log, All);

UVBoRKnobGrabComponent::UVBoRKnobGrabComponent() { Configure(); }
void UVBoRKnobGrabComponent::Configure() { bIsThrowable = false; MultiGrabBehavior = EIsdkMultiGrabBehavior::SingleGrabFirstRetained; }
FTransform UVBoRKnobTransformer::UpdateTransform(const TArray<FIsdkGrabPose>& Poses, const FIsdkTargetTransform& Target)
{
	const auto* Knob = GetTypedOuter<AVBoRKnob>();
	return Knob ? Knob->RotationAxis->GetComponentTransform().GetRelativeTransform(Target.ParentWorldTransform) : Target.RelativeTransform;
}

AVBoRKnob::AVBoRKnob()
{
	PrimaryActorTick.bCanEverTick = true; PrimaryActorTick.TickGroup = TG_PostUpdateWork;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("KnobOrigin")));
	Pivot = CreateDefaultSubobject<USceneComponent>(TEXT("KnobPivot")); Pivot->SetupAttachment(RootComponent);
	RotationAxis = CreateDefaultSubobject<USceneComponent>(TEXT("RotationAxis")); RotationAxis->SetupAttachment(Pivot);
	KnobMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ApproximateKnob")); KnobMesh->SetupAttachment(RotationAxis);
	KnobMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision); KnobMesh->SetSimulatePhysics(false);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder")); KnobMesh->SetStaticMesh(Cylinder.Object);
	RotationMarker = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RotationMarker")); RotationMarker->SetupAttachment(RotationAxis);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube")); RotationMarker->SetStaticMesh(Cube.Object); RotationMarker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GrabBox = CreateDefaultSubobject<UBoxComponent>(TEXT("GrabBox")); GrabBox->SetupAttachment(RotationAxis);
	GrabBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly); GrabBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	GrabBox->SetCollisionResponseToChannel(ECC_WorldDynamic,ECR_Overlap); GrabBox->SetGenerateOverlapEvents(true);
	Grabbable = CreateDefaultSubobject<UIsdkGrabbableComponent>(TEXT("Grabbable")); Grabbable->SetupAttachment(GrabBox);
	GrabComponent = CreateDefaultSubobject<UVBoRKnobGrabComponent>(TEXT("KnobGrab")); Transformer = CreateDefaultSubobject<UVBoRKnobTransformer>(TEXT("KnobTransformer"));
	StateAudio = CreateDefaultSubobject<UAudioComponent>(TEXT("StepClick")); StateAudio->SetupAttachment(Pivot); StateAudio->bAutoActivate = false;
	static ConstructorHelpers::FObjectFinder<USoundBase> Sound(IsdkContentAssetPaths::Audio::Interaction_BasicPoke_ButtonPress); StateAudio->SetSound(Sound.Object);
	SoundAudio.SetNum(1); SoundAudio[0] = StateAudio;
	StepCue.Sound = StateAudio->Sound;

}

void AVBoRKnob::Layout()
{
	KnobMesh->SetRelativeTransform(MeshRestFromRoot.GetRelativeTransform(Pivot->GetRelativeTransform())); RotationMarker->SetRelativeTransform(MarkerRestFromPivot);
	if (KnobMesh->GetStaticMesh())
	{
		const FBox Bounds = KnobMesh->GetStaticMesh()->GetBoundingBox().TransformBy(KnobMesh->GetRelativeTransform());
		GrabBox->SetRelativeLocation(Bounds.GetCenter()); GrabBox->SetBoxExtent(Bounds.GetExtent()+FVector(FMath::Max(0.f,GrabMarginCm)));
	}
}

void AVBoRKnob::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform); Motion = FVBoRKnobMotion(); RotationAxis->SetRelativeRotation(FQuat::Identity); Layout();
}

void AVBoRKnob::PreInitializeComponents()
{
	Motion.StepDeg = StepDeg; Motion.StepHysteresisDeg = StepHysteresisDeg; Motion.JumpThresholdDeg = JumpThresholdDeg;
	Motion.TwistGain = TwistGain;
	bValid = KnobMesh->GetStaticMesh() && !MeshRestFromRoot.ContainsNaN() && !Pivot->GetRelativeTransform().ContainsNaN() && !MarkerRestFromPivot.ContainsNaN();
	for (float Value : {StepDeg,StepHysteresisDeg,JumpThresholdDeg,GrabMarginCm}) { bValid = bValid && FMath::IsFinite(Value) && Value >= 0; }
	bValid = bValid && (StepDeg == 0 || StepHysteresisDeg < StepDeg/2);
	bValid = bValid && FMath::IsFinite(TwistGain) && TwistGain >= 0.1f;
	// 저장된 레벨도 실행 때 검사함. construction 은 배치만 맡음 (P5f §2·E12)
	if (!bValid) { UE_LOG(LogKnob,Warning,TEXT("%s: Missing mesh or invalid knob settings; grabbing disabled | approximate geometry | not-tested"),*GetName()); }
	if (bValid) { Layout(); }
	Grabbable->SetGrabCollider(ConfigureVBoRGrabCollider(KnobMesh, GrabBox, bGrabWithMesh, bValid, MeshRestFromRoot)); Grabbable->SetGrabInputMethodAllowed(EIsdkGrabInputMethod::Palm,bValid); Grabbable->SetGrabInputMethodAllowed(EIsdkGrabInputMethod::Pinch,false);
	GrabComponent->Configure(); GrabComponent->SetTransformTarget(RotationAxis); GrabComponent->SetSingleGrabTransformerObject(Transformer); Grabbable->SetGrabTransformer(GrabComponent);
	Super::PreInitializeComponents();
}

void AVBoRKnob::BeginPlay()
{
	Super::BeginPlay();
	auto* Material = LoadObject<UMaterialInterface>(nullptr,IsdkContentAssetPaths::Materials::GrabbableDefault);
	if (Material)
	{
		StateOverlay = UMaterialInstanceDynamic::Create(Material,this);
		auto* MarkerMaterial = UMaterialInstanceDynamic::Create(Material,this); MarkerMaterial->SetVectorParameterValue(TEXT("ColorParam"),FLinearColor(1,0.75,0,1)); RotationMarker->SetMaterial(0,MarkerMaterial);
	}
	Grabbable->GetInteractionPointerEventDelegate().AddUniqueDynamic(this,&AVBoRKnob::HandlePointer); UpdateVisual();
}

bool AVBoRKnob::ReadGrabPose(FTransform& Pose) const
{
	const EOculusXRHandType Type = Motion.GrabHand == 0 ? EOculusXRHandType::HandLeft : EOculusXRHandType::HandRight;
	if (!HoldingGrabber || !UOculusXRInputFunctionLibrary::IsHandPositionValid(Type) || UOculusXRInputFunctionLibrary::GetTrackingConfidence(Type) != EOculusXRTrackingConfidence::High ||
		!HoldingGrabber->GetHandGrabDetector() || !HoldingGrabber->GetHandGrabDetector()->GetPalmGrabCollider()) { return false; }
	HoldingGrabber->GetPalmGrabTransform(Pose); Pose = Pose.GetRelativeTransform(Pivot->GetComponentTransform()); return !Pose.ContainsNaN();
}

bool AVBoRKnob::ReadClosed(bool& Closed) const
{
	const auto* Pawn = Cast<AVBoRHandRigPawn>(UGameplayStatics::GetPlayerPawn(this,0));
	return Pawn && (Motion.GrabHand == 0 ? Pawn->LeftRig->ReadPalmGrab(Closed) : Pawn->RightRig->ReadPalmGrab(Closed));
}

bool AVBoRKnob::BeginHold(int32 Hand, UIsdkGrabberComponent* Grabber)
{
	if (Motion.GrabHand != INDEX_NONE) { return Motion.GrabHand == Hand && HoldingGrabber == Grabber; }
	if (!bValid || Hand < 0 || Hand > 1 || (Grabber && Grabber->IsInputMethodActive(EIsdkGrabInputMethod::Pinch) && !Grabber->IsInputMethodActive(EIsdkGrabInputMethod::Palm))) { bCancelPending = true; return false; }
	HoldingGrabber = Grabber; Motion.GrabHand = Hand; FTransform Pose; const bool Tracked = ReadGrabPose(Pose); Motion.GrabHand = INDEX_NONE;
	if (!Tracked || !Motion.BeginGrab(Hand,Pose.GetRotation())) { HoldingGrabber = nullptr; bCancelPending = true; return false; }
	bRetainUntilOpen = false; return true;
}

void AVBoRKnob::AdvanceGrab()
{
	if (Motion.GrabHand == INDEX_NONE) { return; }
	FTransform Pose; const bool Tracked = ReadGrabPose(Pose), WasPaused = Motion.bPaused; Motion.AdvanceGrab(Tracked,Pose.GetRotation());
	if (!Tracked) { bRetainUntilOpen = true; }
	if (Tracked && WasPaused) { UE_LOG(LogKnob,Log,TEXT("%s recovery discarded %.3f deg | not-tested"),*GetName(),Motion.RecoveryDiscardDeg); }
}

void AVBoRKnob::ReleaseHold()
{
	Motion.ReleaseGrab(); HoldingGrabber = nullptr; bRetainUntilOpen = false; ApplyPose();
}

void AVBoRKnob::HandlePointer(const FIsdkInteractionPointerEvent& Event)
{
	auto* Grabber = Cast<UIsdkGrabberComponent>(Event.Interactor); EIsdkHandedness Handedness;
	if (!Grabber || !UIsdkFunctionLibrary::GetHandednessFromInteractor(Grabber,Handedness)) { return; }
	const int32 Hand = Handedness == EIsdkHandedness::Left ? 0 : 1;
	if (Event.Type == EIsdkPointerEventType::Hover) { Hovering.Add(Grabber); }
	else if (Event.Type == EIsdkPointerEventType::Unhover) { Hovering.Remove(Grabber); }
	else if (Event.Type == EIsdkPointerEventType::Select) { BeginHold(Hand,Grabber); }
	else if ((Event.Type == EIsdkPointerEventType::Unselect || Event.Type == EIsdkPointerEventType::Cancel) && !bCancelling && Motion.GrabHand == Hand && HoldingGrabber == Grabber)
	{ AdvanceGrab(); if (!Motion.bPaused && !bRetainUntilOpen) { ReleaseHold(); } }
}

void AVBoRKnob::ApplyPose()
{
	// 표시 각도만 한 바퀴로 줄임 (누적값은 유지하고 회전 계산 오차를 줄임, P5f D2, deg)
	RotationAxis->SetRelativeRotation(FQuat(FVector::ZAxisVector,FMath::DegreesToRadians(FMath::Fmod(Motion.AngleDeg,360.0))));
}

void AVBoRKnob::PlayStepSound() { SoundPlayback[0].Queue(StepCue, SoundAudio[0], SoundFrame); }

void AVBoRKnob::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	++SoundFrame; AdvanceSounds(DeltaSeconds); if (!FMath::IsFinite(DeltaSeconds) || DeltaSeconds <= 0) { return; }
	if (bValid)
	{
		AdvanceGrab();
		if (Motion.GrabHand != INDEX_NONE)
		{
			FTransform Pose; bool Closed = true;
			if (ReadGrabPose(Pose) && ReadClosed(Closed) && !Closed) { ReleaseHold(); bCancelPending = true; }
		}
		ApplyPose();
	}
	if (bCancelPending) { bCancelPending = false; bCancelling = true; GrabComponent->CancelSelection(); bCancelling = false; }
	UpdateVisual();
	// 포인터 처리에서는 단계만 확정함 (SDK 선택 배열 재진입 방지, P5f D4)
	if (!Motion.bPaused)
	{
		const TArray<int32> Steps = MoveTemp(Motion.PendingSteps);
		for (int32 Direction : Steps) { PlayStepSound(); OnKnobStep.Broadcast(Direction); if (IsActorBeingDestroyed()) { return; } }
	}
}

void AVBoRKnob::UpdateVisual()
{
	if (!StateOverlay) { return; }
	const FLinearColor Color = !bValid ? FLinearColor::Transparent : Motion.GrabHand != INDEX_NONE ? GrabColor : Hovering.IsEmpty() ? FLinearColor::Transparent : HoverColor;
	StateOverlay->SetVectorParameterValue(TEXT("ColorParam"),Color); KnobMesh->SetOverlayMaterial(StateOverlay);
}

FString AVBoRKnob::GetDebugText() const
{
	return FString::Printf(TEXT("Approximate geometry | angle=%.3f deg step=%lld grab=%d paused=%d discarded=%.3f deg | 손바닥 감싸 쥐기만(엄지·검지 굴림 없음) | not-tested"),Motion.AngleDeg,Motion.StepIndex,Motion.GrabHand,Motion.bPaused,Motion.RecoveryDiscardDeg);
}

void AVBoRKnob::EndPlay(const EEndPlayReason::Type Reason)
{
	for (int32 I = 0; I < 1; ++I) { SoundPlayback[I].Reset(SoundAudio[I], false); }
	Grabbable->GetInteractionPointerEventDelegate().RemoveDynamic(this,&AVBoRKnob::HandlePointer); KnobMesh->SetOverlayMaterial(nullptr);
	Motion.ReleaseGrab(); Motion.PendingSteps.Empty(); HoldingGrabber = nullptr; Hovering.Empty(); Super::EndPlay(Reason);
}

void AVBoRKnob::AdvanceSounds(double Seconds)
{
	SoundPlayback[0].Advance(StepCue, SoundAudio[0], Seconds, SoundFrame);
}
