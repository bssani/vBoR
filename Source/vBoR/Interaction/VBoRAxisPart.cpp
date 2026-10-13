#include "VBoRAxisPart.h"
#include "VBoRHingedDoor.h"
#include "VBoRHandRigPawn.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Components/AudioComponent.h"
#include "Engine/StaticMesh.h"
#include "Interaction/IsdkGrabbableComponent.h"
#include "Interaction/IsdkGrabberComponent.h"
#include "Interaction/Pointable/IsdkInteractionPointerEvent.h"
#include "Interaction/Grabbable/IsdkOneGrabRotateTransformer.h"
#include "Interaction/Grabbable/IsdkOneGrabTranslateTransformer.h"
#include "IsdkFunctionLibrary.h"
#include "IsdkContentAssetPaths.h"
#include "OculusXRInputFunctionLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Sound/SoundBase.h"
#include "UObject/ConstructorHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogAxisPart, Log, All);

UVBoRAxisGrabComponent::UVBoRAxisGrabComponent() { Configure(); }
void UVBoRAxisGrabComponent::Configure()
{
	bIsThrowable = false;
	MultiGrabBehavior = EIsdkMultiGrabBehavior::SingleGrabFirstRetained;
}

UIsdkTransformer* UVBoRAxisTransformer::Selected() const
{
	const AVBoRAxisPart* Part = GetTypedOuter<AVBoRAxisPart>();
	return Part->Movement == EVBoRAxisMovement::Rotate ? static_cast<UIsdkTransformer*>(Rotate.Get()) : Translate.Get();
}

FIsdkTargetTransform UVBoRAxisTransformer::AxisTarget() const
{
	const AVBoRAxisPart* Part = GetTypedOuter<AVBoRAxisPart>();
	FIsdkTargetTransform Target;
	Target.ParentWorldTransform = Part->RestPivot * Part->GetActorTransform();
	Target.RelativeTransform = Part->ValueTransform();
	Target.WorldTransform = Target.RelativeTransform * Target.ParentWorldTransform;
	return Target;
}

void UVBoRAxisTransformer::UpdateConstraints(const FIsdkTargetTransform& Target)
{
	AVBoRAxisPart* Part = GetTypedOuter<AVBoRAxisPart>();
	if (!Rotate) { Rotate = NewObject<UIsdkOneGrabRotateTransformer>(this); Translate = NewObject<UIsdkOneGrabTranslateTransformer>(this); }
	const float SignedLimit = static_cast<float>(Part->Limit() * (Part->bReverse ? -1 : 1));
	const FIsdkAxisConstraints Range(true, FMath::Min(0.f, SignedLimit), FMath::Max(0.f, SignedLimit));
	Rotate->Constraint.RotationAxis = Part->Axis;
	Rotate->Constraint.bUseAngleFromAxisConstraint = false;
	// SDK twist clamp 는 quaternion 각도의 반대 부호임 (IsdkTransformerUtils.h:295-297, deg)
	Rotate->Constraint.AxisAngleRange = FIsdkAxisConstraints(true, -Range.Max, -Range.Min);
	Translate->bIsRelativeConstraint = false;
	Translate->PositionConstraint.SetAxisConstraint(EAxis::None, FIsdkAxisConstraints(true, 0, 0));
	Translate->PositionConstraint.SetAxisConstraint(Part->Axis, Range);
	Selected()->UpdateConstraints(AxisTarget());
}

void UVBoRAxisTransformer::BeginTransform(const TArray<FIsdkGrabPose>& Poses, const FIsdkTargetTransform& Target)
{
	if (Poses.IsEmpty()) { return; }
	UpdateConstraints(Target);
	bActive = true;
	LastPose = Poses[0];
	Selected()->BeginTransform({LastPose}, AxisTarget());
}

void UVBoRAxisTransformer::Rebase(const FIsdkGrabPose& Pose)
{
	AVBoRAxisPart* Part = GetTypedOuter<AVBoRAxisPart>();
	const FIsdkTargetTransform Target = AxisTarget();
	const FVector Before = Target.ParentWorldTransform.InverseTransformPosition(LastPose.Position());
	const FVector After = Target.ParentWorldTransform.InverseTransformPosition(Pose.Position());
	if (Part->Movement == EVBoRAxisMovement::Rotate)
	{
		const FVector A = FVector::VectorPlaneProject(Before, Part->LocalAxis()).GetSafeNormal();
		const FVector B = FVector::VectorPlaneProject(After, Part->LocalAxis()).GetSafeNormal();
		Part->RecoveryDiscard = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(A,B), -1.0, 1.0)));
	}
	else { Part->RecoveryDiscard = FMath::Abs(FVector::DotProduct(After-Before, Part->LocalAxis())); }
	Selected()->BeginTransform({Pose}, Target);
	LastPose = Pose;
	Part->bRebase = Part->bPaused = false;
	UE_LOG(LogAxisPart, Log, TEXT("%s rebase discarded %.3f %s"), *Part->GetName(), Part->RecoveryDiscard,
		Part->Movement == EVBoRAxisMovement::Rotate ? TEXT("deg") : TEXT("cm"));
}

FTransform UVBoRAxisTransformer::UpdateTransform(const TArray<FIsdkGrabPose>& Poses, const FIsdkTargetTransform& Target)
{
	AVBoRAxisPart* Part = GetTypedOuter<AVBoRAxisPart>();
	if (Part->HoldingHand == INDEX_NONE) { return Part->AxisPivot->GetRelativeTransform(); }
	return UpdateTracked(Poses, Target, Part->IsHandTracked());
}

FTransform UVBoRAxisTransformer::UpdateTracked(const TArray<FIsdkGrabPose>& Poses, const FIsdkTargetTransform& Target, bool bTracked)
{
	AVBoRAxisPart* Part = GetTypedOuter<AVBoRAxisPart>();
	if (!bActive || Poses.IsEmpty() || Part->bDoorHold) { return Part->AxisPivot->GetRelativeTransform(); }
	if (Part->LinkedDoor && !Part->LinkedDoor->GetMotion().bLocked) { return Part->AxisPivot->GetRelativeTransform(); }
	if (!bTracked)
	{
		if (!Part->bPaused) { ++Part->Pauses; }
		Part->bPaused = Part->bRebase = true;
		return Part->AxisPivot->GetRelativeTransform();
	}
	if (Part->bRebase) { Rebase(Poses[0]); return Part->AxisPivot->GetRelativeTransform(); }
	const FTransform Result = Selected()->UpdateTransform({Poses[0]}, AxisTarget());
	Part->AcceptTransform(Result);
	LastPose = Poses[0];
	return Part->ValueTransform() * Part->RestPivot;
}

FTransform UVBoRAxisTransformer::EndTransform(const FIsdkTargetTransform& Target)
{
	bActive = false;
	if (Selected()) { Selected()->EndTransform(AxisTarget()); }
	return GetTypedOuter<AVBoRAxisPart>()->AxisPivot->GetRelativeTransform();
}

AVBoRAxisPart::AVBoRAxisPart()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostUpdateWork;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("PartOrigin")));
	AxisPivot = CreateDefaultSubobject<USceneComponent>(TEXT("AxisPivot")); AxisPivot->SetupAttachment(RootComponent);
	PartMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PartMesh")); PartMesh->SetupAttachment(AxisPivot);
	PartMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PartMesh->SetSimulatePhysics(false);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	PartMesh->SetStaticMesh(Cube.Object);
	GrabBox = CreateDefaultSubobject<UBoxComponent>(TEXT("GrabBox")); GrabBox->SetupAttachment(AxisPivot);
	GrabBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	GrabBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	GrabBox->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);
	GrabBox->SetGenerateOverlapEvents(true);
	Grabbable = CreateDefaultSubobject<UIsdkGrabbableComponent>(TEXT("Grabbable")); Grabbable->SetupAttachment(GrabBox);
	GrabComponent = CreateDefaultSubobject<UVBoRAxisGrabComponent>(TEXT("GrabComponent"));
	Transformer = CreateDefaultSubobject<UVBoRAxisTransformer>(TEXT("AxisTransformer"));
	StateAudio = CreateDefaultSubobject<UAudioComponent>(TEXT("StateAudio")); StateAudio->SetupAttachment(AxisPivot); StateAudio->bAutoActivate = false;
	static ConstructorHelpers::FObjectFinder<USoundBase> Sound(IsdkContentAssetPaths::Audio::Interaction_BasicPoke_ButtonPress);
	StateAudio->SetSound(Sound.Object);
}

void AVBoRAxisPart::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	Value = 0;
	RestPivot = AxisPivot->GetRelativeTransform();
	PartMesh->SetRelativeTransform(MeshRestFromRoot.GetRelativeTransform(RestPivot));
	if (PartMesh->GetStaticMesh())
	{
		const FBox Bounds = PartMesh->GetStaticMesh()->GetBoundingBox().TransformBy(PartMesh->GetRelativeTransform());
		GrabBox->SetRelativeLocation(Bounds.GetCenter());
		GrabBox->SetBoxExtent(Bounds.GetExtent() + FVector(FMath::Max(0.f, GrabMarginCm)));
		GrabBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	}
	else
	{
		UE_LOG(LogAxisPart, Warning, TEXT("%s: No part mesh; grabbing disabled"), *GetName());
		GrabBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
}

void AVBoRAxisPart::PreInitializeComponents()
{
	RestPivot = AxisPivot->GetRelativeTransform();
	Grabbable->SetGrabCollider(GrabBox);
	Grabbable->SetGrabInputMethodAllowed(EIsdkGrabInputMethod::Palm, PartMesh->GetStaticMesh() != nullptr);
	Grabbable->SetGrabInputMethodAllowed(EIsdkGrabInputMethod::Pinch, false);
	GrabComponent->Configure();
	GrabComponent->SetTransformTarget(AxisPivot);
	GrabComponent->SetSingleGrabTransformerObject(Transformer);
	Super::PreInitializeComponents();
}

void AVBoRAxisPart::BeginPlay()
{
	Super::BeginPlay();
	UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, IsdkContentAssetPaths::Materials::GrabbableDefault);
	StateOverlay = Material ? UMaterialInstanceDynamic::Create(Material, this) : nullptr;
	Grabbable->GetInteractionPointerEventDelegate().AddUniqueDynamic(this, &AVBoRAxisPart::HandlePointer);
	EvaluateState();
}

double AVBoRAxisPart::Limit() const { return Movement == EVBoRAxisMovement::Rotate ? FMath::Clamp(MaxAngleDeg, 0.f, 170.f) : FMath::Max(0.f, MaxTravelCm); }
FVector AVBoRAxisPart::LocalAxis() const { return Axis == EAxis::Y ? FVector::YAxisVector : Axis == EAxis::Z ? FVector::ZAxisVector : FVector::XAxisVector; }
FTransform AVBoRAxisPart::ValueTransform() const
{
	const double SignedValue = Value * (bReverse ? -1 : 1);
	return Movement == EVBoRAxisMovement::Rotate ? FTransform(FQuat(LocalAxis(), FMath::DegreesToRadians(SignedValue))) : FTransform(LocalAxis() * SignedValue);
}
void AVBoRAxisPart::AcceptTransform(const FTransform& Transform)
{
	if (Transform.ContainsNaN()) { return; }
	const double Raw = Movement == EVBoRAxisMovement::Translate ? FVector::DotProduct(Transform.GetLocation(), LocalAxis()) :
		FMath::RadiansToDegrees(2 * FMath::Atan2(FVector::DotProduct(FVector(Transform.GetRotation().X, Transform.GetRotation().Y, Transform.GetRotation().Z), LocalAxis()), Transform.GetRotation().W));
	Value = FMath::Clamp(Raw * (bReverse ? -1 : 1), 0.0, Limit());
}
float AVBoRAxisPart::GetOpenRatio() const { return Limit() > SMALL_NUMBER ? static_cast<float>(Value / Limit()) : 0.f; }
bool AVBoRAxisPart::IsHandTracked() const
{
	if (HoldingHand == INDEX_NONE) { return false; }
	const EOculusXRHandType Type = HoldingHand == 0 ? EOculusXRHandType::HandLeft : EOculusXRHandType::HandRight;
	return UOculusXRInputFunctionLibrary::IsHandPositionValid(Type) && UOculusXRInputFunctionLibrary::GetTrackingConfidence(Type) == EOculusXRTrackingConfidence::High;
}

void AVBoRAxisPart::HandlePointer(const FIsdkInteractionPointerEvent& Event)
{
	UIsdkGrabberComponent* Grabber = Cast<UIsdkGrabberComponent>(Event.Interactor);
	EIsdkHandedness Handedness;
	if (!Grabber || !UIsdkFunctionLibrary::GetHandednessFromInteractor(Grabber, Handedness)) { return; }
	const int32 Hand = Handedness == EIsdkHandedness::Left ? 0 : 1;
	if (Event.Type == EIsdkPointerEventType::Hover) { if (!Hovering.Contains(Grabber)) { Hovering.Add(Grabber); ++Attempts; } }
	else if (Event.Type == EIsdkPointerEventType::Unhover) { Hovering.Remove(Grabber); }
	else if (Event.Type == EIsdkPointerEventType::Select)
	{
		if (HoldingHand != INDEX_NONE || bNeedsPalmOpen[Hand] || !PartMesh->GetStaticMesh()) { return; }
		HoldingHand = Hand; HoldingGrabber = Grabber;
		if (!IsHandTracked()) { HoldingHand = INDEX_NONE; HoldingGrabber = nullptr; GrabComponent->ForceCancel(); return; }
		++Grabs;
		bPaused = bRebase = false;
		UE_LOG(LogAxisPart, Log, TEXT("%s grab hand=%d"), *GetName(), Hand);
	}
	else if ((Event.Type == EIsdkPointerEventType::Unselect || Event.Type == EIsdkPointerEventType::Cancel) && HoldingGrabber == Grabber && !bDoorHold)
	{
		if (IsHandTracked() && !bPaused) { ReleasePart(); }
		else { if (!bPaused) { ++Pauses; } bPaused = bRebase = true; }
	}
}

void AVBoRAxisPart::ReleasePart()
{
	if (HoldingHand == INDEX_NONE) { return; }
	HoldingHand = INDEX_NONE; HoldingGrabber = nullptr;
	bPaused = bRebase = bDoorHold = false;
	GrabComponent->ForceCancel();
	OnPartReleased.Broadcast();
	UE_LOG(LogAxisPart, Log, TEXT("%s released | %s"), *GetName(), *GetDebugText());
}

void AVBoRAxisPart::ConnectDoor(AVBoRHingedDoor* Door, int32 Handle)
{
	LinkedDoor = Door; DoorHandle = Handle; Release = EVBoRAxisRelease::Return;
}
void AVBoRAxisPart::EndDoorHold(bool bLatch)
{
	if (bLatch && HoldingHand != INDEX_NONE) { bNeedsPalmOpen[HoldingHand] = true; }
	ReleasePart();
	GrabComponent->ForceCancel();
}

void AVBoRAxisPart::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!FMath::IsFinite(DeltaSeconds) || DeltaSeconds <= 0) { return; }
	if (bDoorHold || (HoldingHand == INDEX_NONE && GrabComponent->GetNumGrabbers())) { GrabComponent->ForceCancel(); }
	AVBoRHandRigPawn* Pawn = Cast<AVBoRHandRigPawn>(UGameplayStatics::GetPlayerPawn(this,0));
	for (int32 H = 0; H < 2; ++H)
	{
		bool bClosed = true;
		const EOculusXRHandType Type = H == 0 ? EOculusXRHandType::HandLeft : EOculusXRHandType::HandRight;
		if (Pawn && UOculusXRInputFunctionLibrary::IsHandPositionValid(Type) && UOculusXRInputFunctionLibrary::GetTrackingConfidence(Type) == EOculusXRTrackingConfidence::High &&
			(H == 0 ? Pawn->LeftRig->ReadPalmGrab(bClosed) : Pawn->RightRig->ReadPalmGrab(bClosed)) && !bClosed) { bNeedsPalmOpen[H] = false; }
	}
	if (HoldingHand != INDEX_NONE && !bDoorHold)
	{
		if (!IsHandTracked()) { if (!bPaused) { ++Pauses; } bPaused = bRebase = true; PausedSeconds += DeltaSeconds; return; }
		if (bRebase && HoldingGrabber)
		{
			FTransform Pose; HoldingGrabber->GetPalmGrabTransform(Pose);
			FIsdkGrabPose GrabPose = Transformer->LastPose; GrabPose.Pose.Position = FVector3f(Pose.GetLocation()); GrabPose.Pose.Orientation = Pose.GetRotation();
			Transformer->Rebase(GrabPose);
			return;
		}
		bool bClosed = true;
		if (Pawn && (HoldingHand == 0 ? Pawn->LeftRig->ReadPalmGrab(bClosed) : Pawn->RightRig->ReadPalmGrab(bClosed)) && !bClosed) { ReleasePart(); }
		else if (!Transformer->IsTransformerActive() && HoldingGrabber)
		{
			// SDK 가 추적 손실 때 선택을 끝내도 실제 손을 펼 때까지 보유함 (P2b D7)
			FTransform Pose; HoldingGrabber->GetPalmGrabTransform(Pose);
			FIsdkGrabPose GrabPose = Transformer->LastPose; GrabPose.Pose.Position = FVector3f(Pose.GetLocation()); GrabPose.Pose.Orientation = Pose.GetRotation();
			Transformer->BeginTransform({GrabPose}, FIsdkTargetTransform(AxisPivot));
		}
		else if (HoldingGrabber && !GrabComponent->GetNumGrabbers())
		{
			FTransform Pose; HoldingGrabber->GetPalmGrabTransform(Pose);
			FIsdkGrabPose GrabPose = Transformer->LastPose; GrabPose.Pose.Position = FVector3f(Pose.GetLocation()); GrabPose.Pose.Orientation = Pose.GetRotation();
			AxisPivot->SetRelativeTransform(Transformer->UpdateTransform({GrabPose}, FIsdkTargetTransform(AxisPivot)));
		}
	}
	if (HoldingHand == INDEX_NONE && Release == EVBoRAxisRelease::Return)
	{
		const double Speed = Movement == EVBoRAxisMovement::Rotate ? ReturnDegPerSecond : ReturnCmPerSecond;
		Value = FMath::Max(0.0, Value - FMath::Max(0.0, Speed) * DeltaSeconds);
		AxisPivot->SetRelativeTransform(ValueTransform() * RestPivot);
	}
	EvaluateState();
	// Move delegate 안에서 ForceCancel 을 부르지 않음 (SDK 선택 배열 재진입 방지, P2b D5)
	if (LinkedDoor && HoldingHand != INDEX_NONE && !bDoorHold && (GetOpenRatio() >= OpenThreshold || !LinkedDoor->GetMotion().bLocked))
	{
		if (LinkedDoor->BeginGrabFromAxisPart(this)) { bDoorHold = true; GrabComponent->ForceCancel(); }
	}
}

void AVBoRAxisPart::EvaluateState()
{
	if (bPaused) { return; }
	const float Ratio = GetOpenRatio(); MaximumRatio = FMath::Max(MaximumRatio, static_cast<double>(Ratio));
	if (Ratio <= OpenRearm) { bOpenArmed = true; }
	if (Ratio >= ClosedRearm) { bClosedArmed = true; }
	if (bOpenArmed && Ratio >= OpenThreshold) { bOpenArmed = false; ++Opens; StateAudio->Play(); OnPartOpened.Broadcast(); UE_LOG(LogAxisPart, Log, TEXT("%s opened | %s"), *GetName(), *GetDebugText()); }
	if (bClosedArmed && Ratio <= ClosedThreshold) { bClosedArmed = false; ++Closes; StateAudio->Play(); OnPartClosed.Broadcast(); UE_LOG(LogAxisPart, Log, TEXT("%s closed | %s"), *GetName(), *GetDebugText()); }
	if (StateOverlay)
	{
		StateOverlay->SetVectorParameterValue(TEXT("ColorParam"), HoldingHand != INDEX_NONE ? GrabColor : Ratio >= OpenThreshold ? OpenColor : Hovering.IsEmpty() ? FLinearColor::Transparent : HoverColor);
		PartMesh->SetOverlayMaterial(StateOverlay);
	}
}
FString AVBoRAxisPart::GetDebugText() const
{
	return FString::Printf(TEXT("Approximate geometry | ratio=%.3f | attempts=%d grabs=%d open/close=%d/%d max=%.3f | pauses=%d %.3f s | rebase discarded=%.3f %s | not-tested"),
		GetOpenRatio(), Attempts, Grabs, Opens, Closes, MaximumRatio, Pauses, PausedSeconds, RecoveryDiscard, Movement == EVBoRAxisMovement::Rotate ? TEXT("deg") : TEXT("cm"));
}
void AVBoRAxisPart::EndPlay(const EEndPlayReason::Type Reason)
{
	Grabbable->GetInteractionPointerEventDelegate().RemoveDynamic(this, &AVBoRAxisPart::HandlePointer);
	PartMesh->SetOverlayMaterial(nullptr);
	Super::EndPlay(Reason);
}
