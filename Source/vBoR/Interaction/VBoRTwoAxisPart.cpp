#include "VBoRTwoAxisPart.h"
#include "VBoRHandRigPawn.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
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
#include "UObject/ConstructorHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogTwoAxisPart, Log, All);

UVBoRTwoAxisGrabComponent::UVBoRTwoAxisGrabComponent() { Configure(); }
void UVBoRTwoAxisGrabComponent::Configure() { bIsThrowable = false; MultiGrabBehavior = EIsdkMultiGrabBehavior::SingleGrabFirstRetained; }
FTransform UVBoRTwoAxisTransformer::UpdateTransform(const TArray<FIsdkGrabPose>& Poses, const FIsdkTargetTransform& Target)
{
	const auto* Part = GetTypedOuter<AVBoRTwoAxisPart>();
	return Part ? Part->PitchAxis->GetComponentTransform().GetRelativeTransform(Target.ParentWorldTransform) : Target.RelativeTransform;
}

AVBoRTwoAxisPart::AVBoRTwoAxisPart()
{
	PrimaryActorTick.bCanEverTick = true; PrimaryActorTick.TickGroup = TG_PostUpdateWork;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("PartOrigin")));
	Pivot = CreateDefaultSubobject<USceneComponent>(TEXT("BallJoint")); Pivot->SetupAttachment(RootComponent);
	YawAxis = CreateDefaultSubobject<USceneComponent>(TEXT("YawAxis")); YawAxis->SetupAttachment(Pivot);
	PitchAxis = CreateDefaultSubobject<USceneComponent>(TEXT("PitchAxis")); PitchAxis->SetupAttachment(YawAxis);
	PartMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ApproximateMirrorNoReflection")); PartMesh->SetupAttachment(PitchAxis);
	PartMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision); PartMesh->SetSimulatePhysics(false);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube")); PartMesh->SetStaticMesh(Cube.Object);
	GrabBox = CreateDefaultSubobject<UBoxComponent>(TEXT("GrabBox")); GrabBox->SetupAttachment(PitchAxis);
	GrabBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly); GrabBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	GrabBox->SetCollisionResponseToChannel(ECC_WorldDynamic,ECR_Overlap); GrabBox->SetGenerateOverlapEvents(true);
	Grabbable = CreateDefaultSubobject<UIsdkGrabbableComponent>(TEXT("Grabbable")); Grabbable->SetupAttachment(GrabBox);
	GrabComponent = CreateDefaultSubobject<UVBoRTwoAxisGrabComponent>(TEXT("TwoAxisGrab"));
	Transformer = CreateDefaultSubobject<UVBoRTwoAxisTransformer>(TEXT("TwoAxisTransformer"));
}

void AVBoRTwoAxisPart::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	Motion = FVBoRTwoAxisMotion(); YawAxis->SetRelativeRotation(FQuat::Identity); PitchAxis->SetRelativeRotation(FQuat::Identity);
	PartMesh->SetRelativeTransform(MeshRestFromRoot.GetRelativeTransform(Pivot->GetRelativeTransform()));
	if (PartMesh->GetStaticMesh())
	{
		const FBox Bounds = PartMesh->GetStaticMesh()->GetBoundingBox().TransformBy(PartMesh->GetRelativeTransform());
		GrabBox->SetRelativeLocation(Bounds.GetCenter()); GrabBox->SetBoxExtent(Bounds.GetExtent());
	}
}

void AVBoRTwoAxisPart::PreInitializeComponents()
{
	Motion.YawLimitDeg = YawLimitDeg; Motion.PitchLimitDeg = PitchLimitDeg; Motion.CenterExclusionCm = CenterExclusionCm;
	Motion.FrontToleranceCm = FrontToleranceCm; Motion.DepthMarginCm = DepthMarginCm; Motion.EdgeMarginCm = EdgeMarginCm;
	bValid = PartMesh->GetStaticMesh() && !MeshRestFromRoot.ContainsNaN() && !Pivot->GetRelativeTransform().ContainsNaN();
	for (float Value : {YawLimitDeg,PitchLimitDeg,CenterExclusionCm,FrontToleranceCm,DepthMarginCm,EdgeMarginCm})
	{ bValid = bValid && FMath::IsFinite(Value) && Value >= 0; }
	if (PartMesh->GetStaticMesh())
	{
		const FBox Bounds = PartMesh->GetStaticMesh()->GetBoundingBox().TransformBy(MeshRestFromRoot.GetRelativeTransform(Pivot->GetRelativeTransform()));
		Motion.FaceMinCm = Bounds.Min; Motion.FaceMaxCm = Bounds.Max;
		bValid = bValid && !Bounds.Min.ContainsNaN() && !Bounds.Max.ContainsNaN() && Bounds.GetSize().GetMin() > 0;
		GrabBox->SetRelativeLocation(Bounds.GetCenter()); GrabBox->SetBoxExtent(Bounds.GetExtent());
	}
	// 저장된 레벨도 실행 때 검사함. construction 은 경고를 내지 않음 (P5e E15)
	if (!bValid) { UE_LOG(LogTwoAxisPart,Warning,TEXT("%s: Missing mesh or invalid two-axis settings; grabbing and pushing disabled | approximate geometry | no reflection | not-tested"),*GetName()); }
	GrabBox->SetCollisionEnabled(bValid ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
	GrabBox->SetGenerateOverlapEvents(bValid); GrabBox->SetVisibility(bValid,true);
	Grabbable->SetGrabCollider(GrabBox); Grabbable->SetGrabInputMethodAllowed(EIsdkGrabInputMethod::Palm,bValid);
	Grabbable->SetGrabInputMethodAllowed(EIsdkGrabInputMethod::Pinch,bValid);
	GrabComponent->Configure(); GrabComponent->SetTransformTarget(PitchAxis); GrabComponent->SetSingleGrabTransformerObject(Transformer);
	Grabbable->SetGrabTransformer(GrabComponent);
	Super::PreInitializeComponents();
}

void AVBoRTwoAxisPart::BeginPlay()
{
	Super::BeginPlay();
	auto* Material = LoadObject<UMaterialInterface>(nullptr,IsdkContentAssetPaths::Materials::GrabbableDefault);
	if (Material)
	{
		StateOverlay = UMaterialInstanceDynamic::Create(Material,this);
		auto* FaceMaterial = UMaterialInstanceDynamic::Create(Material,this);
		FaceMaterial->SetVectorParameterValue(TEXT("ColorParam"),FLinearColor(0.15,0.3,0.4,1)); PartMesh->SetMaterial(0,FaceMaterial);
	}
	Grabbable->GetInteractionPointerEventDelegate().AddUniqueDynamic(this,&AVBoRTwoAxisPart::HandlePointer); UpdateVisual();
}

bool AVBoRTwoAxisPart::ReadGrabPose(FTransform& Pose) const
{
	const EOculusXRHandType Type = Motion.GrabHand == 0 ? EOculusXRHandType::HandLeft : EOculusXRHandType::HandRight;
	if (!HoldingGrabber || !UOculusXRInputFunctionLibrary::IsHandPositionValid(Type) ||
		UOculusXRInputFunctionLibrary::GetTrackingConfidence(Type) != EOculusXRTrackingConfidence::High || !HoldingGrabber->GetHandGrabDetector()) { return false; }
	const auto* Detector = HoldingGrabber->GetHandGrabDetector();
	if (bPinch ? !Detector->GetPinchCollider() : !Detector->GetPalmGrabCollider()) { return false; }
	if (bPinch) { HoldingGrabber->GetPinchGrabTransform(Pose); } else { HoldingGrabber->GetPalmGrabTransform(Pose); }
	Pose = Pose.GetRelativeTransform(Pivot->GetComponentTransform()); return !Pose.ContainsNaN();
}

bool AVBoRTwoAxisPart::ReadClosed(bool& Closed) const
{
	const auto* Pawn = Cast<AVBoRHandRigPawn>(UGameplayStatics::GetPlayerPawn(this,0));
	if (!Pawn) { return false; }
	if (bPinch)
	{
		return Motion.GrabHand == 0 ? Pawn->LeftRig->ReadPinchGrab(Closed) : Pawn->RightRig->ReadPinchGrab(Closed);
	}
	return Motion.GrabHand == 0 ? Pawn->LeftRig->ReadPalmGrab(Closed) : Pawn->RightRig->ReadPalmGrab(Closed);
}

void AVBoRTwoAxisPart::ReadPushHands(FVBoRDoorHand (&Hands)[2]) const
{
	const auto* Pawn = Cast<AVBoRHandRigPawn>(UGameplayStatics::GetPlayerPawn(this,0));
	if (!Pawn) { return; }
	for (int32 Hand=0; Hand<2; ++Hand)
	{
		const EOculusXRHandType Type = Hand == 0 ? EOculusXRHandType::HandLeft : EOculusXRHandType::HandRight;
		if (!UOculusXRInputFunctionLibrary::IsHandPositionValid(Type) || UOculusXRInputFunctionLibrary::GetTrackingConfidence(Type) != EOculusXRTrackingConfidence::High ||
			!Pawn->ReadRawPalmAndTips(Hand,Hands[Hand])) { Hands[Hand].bTracked = false; continue; }
		for (FVector& Point : Hands[Hand].Points) { Point = Pivot->GetComponentTransform().InverseTransformPosition(Point); }
	}
}

bool AVBoRTwoAxisPart::BeginHold(int32 Hand, UIsdkGrabberComponent* Grabber)
{
	if (Motion.GrabHand != INDEX_NONE) { return Motion.GrabHand == Hand && HoldingGrabber == Grabber; }
	if (!bValid || Hand < 0 || Hand > 1) { bCancelPending = true; return false; }
	HoldingGrabber = Grabber; bPinch = Grabber && Grabber->IsInputMethodActive(EIsdkGrabInputMethod::Pinch);
	// 광학 입력을 읽을 손만 임시 지정함 (무효 선택은 Tick 에서 취소, P5e D6)
	Motion.GrabHand = Hand; FTransform Pose; const bool Tracked = ReadGrabPose(Pose); Motion.GrabHand = INDEX_NONE;
	if (!Tracked || !Motion.BeginGrab(Hand,Pose.GetRotation())) { HoldingGrabber = nullptr; bCancelPending = true; return false; }
	bRetainUntilOpen = false; return true;
}

void AVBoRTwoAxisPart::AdvanceGrab()
{
	if (Motion.GrabHand == INDEX_NONE) { return; }
	FTransform Pose; const bool Tracked = ReadGrabPose(Pose), WasPaused = Motion.bPaused;
	Motion.AdvanceGrab(Tracked,Pose.GetRotation());
	if (!Tracked) { bRetainUntilOpen = true; }
	if (Tracked && WasPaused) { UE_LOG(LogTwoAxisPart,Log,TEXT("%s recovery discarded %.3f deg | not-tested"),*GetName(),Motion.RecoveryDiscardDeg); }
}

void AVBoRTwoAxisPart::HandlePointer(const FIsdkInteractionPointerEvent& Event)
{
	auto* Grabber = Cast<UIsdkGrabberComponent>(Event.Interactor); EIsdkHandedness Handedness;
	if (!Grabber || !UIsdkFunctionLibrary::GetHandednessFromInteractor(Grabber,Handedness)) { return; }
	const int32 Hand = Handedness == EIsdkHandedness::Left ? 0 : 1;
	if (Event.Type == EIsdkPointerEventType::Hover) { Hovering.Add(Grabber); }
	else if (Event.Type == EIsdkPointerEventType::Unhover) { Hovering.Remove(Grabber); }
	else if (Event.Type == EIsdkPointerEventType::Select) { BeginHold(Hand,Grabber); }
	else if ((Event.Type == EIsdkPointerEventType::Unselect || Event.Type == EIsdkPointerEventType::Cancel) &&
		!bCancelling && Motion.GrabHand == Hand && HoldingGrabber == Grabber)
	{
		AdvanceGrab();
		if (!Motion.bPaused && !bRetainUntilOpen) { Motion.ReleaseGrab(); HoldingGrabber = nullptr; }
	}
}

void AVBoRTwoAxisPart::ApplyPose()
{
	YawAxis->SetRelativeRotation(FQuat(FVector::ZAxisVector,FMath::DegreesToRadians(Motion.YawDeg)));
	PitchAxis->SetRelativeRotation(FQuat(FVector::YAxisVector,FMath::DegreesToRadians(-Motion.PitchDeg)));
}

void AVBoRTwoAxisPart::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!FMath::IsFinite(DeltaSeconds) || DeltaSeconds <= 0) { return; }
	if (bValid)
	{
		AdvanceGrab();
		if (Motion.GrabHand != INDEX_NONE)
		{
			FTransform Pose; bool Closed = true;
			if (ReadGrabPose(Pose) && ReadClosed(Closed) && !Closed)
			{ Motion.ReleaseGrab(); HoldingGrabber = nullptr; bRetainUntilOpen = false; bCancelPending = true; }
		}
		FVBoRDoorHand Hands[2]; ReadPushHands(Hands); Motion.AdvancePush(Hands); ApplyPose();
	}
	if (bCancelPending) { bCancelPending = false; bCancelling = true; GrabComponent->CancelSelection(); bCancelling = false; }
	UpdateVisual();
}

void AVBoRTwoAxisPart::UpdateVisual()
{
	if (!StateOverlay) { return; }
	const FLinearColor Color = !bValid ? FLinearColor::Transparent : Motion.GrabHand != INDEX_NONE ? GrabColor : Motion.PushHand != INDEX_NONE ? PushingColor : Hovering.IsEmpty() ? FLinearColor::Transparent : HoverColor;
	StateOverlay->SetVectorParameterValue(TEXT("ColorParam"),Color); PartMesh->SetOverlayMaterial(StateOverlay);
}

FString AVBoRTwoAxisPart::GetDebugText() const
{
	return FString::Printf(TEXT("Approximate geometry | yaw=%.3f pitch=%.3f deg grab=%d push=%d paused=%d discarded=%.3f deg | no reflection (반사 없음) | not-tested"),Motion.YawDeg,Motion.PitchDeg,Motion.GrabHand,Motion.PushHand,Motion.bPaused,Motion.RecoveryDiscardDeg);
}

void AVBoRTwoAxisPart::EndPlay(const EEndPlayReason::Type Reason)
{
	Grabbable->GetInteractionPointerEventDelegate().RemoveDynamic(this,&AVBoRTwoAxisPart::HandlePointer);
	PartMesh->SetOverlayMaterial(nullptr); Motion.ReleaseGrab(); HoldingGrabber = nullptr; Hovering.Empty();
	Super::EndPlay(Reason);
}
