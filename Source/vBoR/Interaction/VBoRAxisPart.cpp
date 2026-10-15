#include "VBoRAxisPart.h"
#include "VBoRHingedPanel.h"
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

void UVBoRAxisGrabComponent::CancelSelection()
{
#if WITH_DEV_AUTOMATION_TESTS
	++CancelCalls;
#endif
	ForceCancel();
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
	FIsdkAxisConstraints Range(true, FMath::Min(0.f, SignedLimit), FMath::Max(0.f, SignedLimit));
	if (Part->Release == EVBoRAxisRelease::Detent)
	{
		const float A = Part->bDetentValid ? Part->DetentAnglesDeg[0] * (Part->bReverse ? -1 : 1) : 0;
		const float B = Part->bDetentValid ? Part->DetentAnglesDeg.Last() * (Part->bReverse ? -1 : 1) : 0;
		Range = FIsdkAxisConstraints(true, FMath::Min(A,B), FMath::Max(A,B));
	}
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
	if (Release == EVBoRAxisRelease::Detent)
	{
		const FString Problem = DetentProblem(); bDetentValid = Problem.IsEmpty();
		if (!bDetentValid)
		{
			UE_LOG(LogAxisPart, Warning, TEXT("%s: Invalid detent: %s; grabbing disabled"), *GetName(), *Problem);
			GrabBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
	}
	Grabbable->SetGrabCollider(GrabBox);
	Grabbable->SetGrabInputMethodAllowed(EIsdkGrabInputMethod::Palm, PartMesh->GetStaticMesh() != nullptr && bDetentValid);
	Grabbable->SetGrabInputMethodAllowed(EIsdkGrabInputMethod::Pinch, false);
	GrabComponent->Configure();
	GrabComponent->SetTransformTarget(AxisPivot);
	GrabComponent->SetSingleGrabTransformerObject(Transformer);
	Super::PreInitializeComponents();
}

void AVBoRAxisPart::BeginPlay()
{
	Super::BeginPlay();
	if (Release == EVBoRAxisRelease::Detent)
	{
		// RestPivot 확정 뒤 시작 각도를 놓음 (construction 누적 방지, P5b D7)
		DetentIndex = DetentAnglesDeg.IsValidIndex(DetentStartIndex) ? DetentStartIndex : INDEX_NONE;
		bNudgeArmed = true;
		Value = DetentIndex != INDEX_NONE && FMath::IsFinite(DetentAnglesDeg[DetentIndex]) ? DetentAnglesDeg[DetentIndex] : 0;
		AxisPivot->SetRelativeTransform(ValueTransform() * RestPivot);
	}
	UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, IsdkContentAssetPaths::Materials::GrabbableDefault);
	StateOverlay = Material ? UMaterialInstanceDynamic::Create(Material, this) : nullptr;
	Grabbable->GetInteractionPointerEventDelegate().AddUniqueDynamic(this, &AVBoRAxisPart::HandlePointer);
	EvaluateState();
}

double AVBoRAxisPart::Limit() const { return Movement == EVBoRAxisMovement::Rotate ? FMath::Clamp(MaxAngleDeg, 0.f, 170.f) : FMath::Max(0.f, MaxTravelCm); }

FString AVBoRAxisPart::DetentProblem() const
{
	if (Movement != EVBoRAxisMovement::Rotate) { return TEXT("Movement must be Rotate"); }
	if (DetentAnglesDeg.Num() < 2) { return TEXT("at least two angles required"); }
	if (!DetentAnglesDeg.IsValidIndex(DetentStartIndex)) { return TEXT("start index outside angle list"); }
	if (!FMath::IsFinite(DetentUpThresholdDeg) || !FMath::IsFinite(DetentDownThresholdDeg) || DetentUpThresholdDeg <= 0 || DetentDownThresholdDeg <= 0) { return TEXT("thresholds must be finite and > 0 deg"); }
	if (!FMath::IsFinite(DetentNudgeDeg) || DetentNudgeDeg < 0 || (DetentNudgeDeg > 0 && (DetentNudgeDeg >= DetentUpThresholdDeg || DetentNudgeDeg >= DetentDownThresholdDeg)))
	{ return TEXT("nudge must be 0 (off) or finite > 0 and below both thresholds in deg"); }
	for (int32 I = 0; I < DetentAnglesDeg.Num(); ++I)
	{
		if (!FMath::IsFinite(DetentAnglesDeg[I]) || FMath::Abs(DetentAnglesDeg[I]) > 170) { return TEXT("angles must be finite within +/-170 deg"); }
		if (!I) { continue; }
		const float Gap = DetentAnglesDeg[I] - DetentAnglesDeg[I-1];
		if (Gap <= 0) { return TEXT("angles must be strictly increasing"); }
		if (DetentUpThresholdDeg > Gap || DetentDownThresholdDeg > Gap) { return TEXT("threshold exceeds adjacent gap"); }
		if (DetentUpThresholdDeg + DetentDownThresholdDeg <= Gap) { return TEXT("threshold sum must exceed adjacent gap"); }
	}
	return FString();
}

void AVBoRAxisPart::EvaluateDetent()
{
	if (!bDetentValid || HoldingHand == INDEX_NONE || bPaused || !DetentAnglesDeg.IsValidIndex(DetentIndex)) { return; }
	const double NudgeOffset = Value - DetentAnglesDeg[DetentStartIndex];
	// 먼저 차선 변경을 대기시킴 (한 Move 에 끝까지 밀어도 알림 순서 유지, P5c D4·D5)
	const double NudgeTolerance = FMath::Min(1.e-4, static_cast<double>(DetentNudgeDeg) / 4);
	if (DetentNudgeDeg > 0 && DetentIndex == DetentStartIndex)
	{
		if (FMath::Abs(NudgeOffset) <= DetentNudgeDeg / 2 + NudgeTolerance) { bNudgeArmed = true; }
		if (bNudgeArmed && FMath::Abs(NudgeOffset) >= DetentNudgeDeg - NudgeTolerance)
		{
			bNudgeArmed = false;
			PendingDetents.Add({true, NudgeOffset > 0 ? 1 : -1});
		}
	}
	auto Reached = [this](int32 Direction)
	{
		if (!DetentAnglesDeg.IsValidIndex(DetentIndex + Direction)) { return false; }
		const double Gap = FMath::Abs(static_cast<double>(DetentAnglesDeg[DetentIndex + Direction]) - DetentAnglesDeg[DetentIndex]);
		// FVector3f 각도 경계 오차만 받음. 왕복 방지 여유의 1/4 이내로 제한함 (P5b D5·E4, deg)
		const double Tolerance = FMath::Min(1.e-4, (static_cast<double>(DetentUpThresholdDeg) + DetentDownThresholdDeg - Gap) / 4);
		const double Threshold = static_cast<double>(DetentAnglesDeg[DetentIndex]) + (Direction > 0 ? DetentUpThresholdDeg : -DetentDownThresholdDeg);
		return Direction > 0 ? Value >= Threshold - Tolerance : Value <= Threshold + Tolerance;
	};
	// 같은 입력에서 인접 위치를 순서대로 확정함 (한 frame 에 두 기준을 넘을 수 있음, P5b D3)
	int32 Direction = 0;
	if (Reached(1)) { Direction = 1; }
	else if (Reached(-1)) { Direction = -1; }
	while (Direction && Reached(Direction))
	{
		DetentIndex += Direction;
		// 가운데를 지나친 끄기 동작은 차선 변경으로 보지 않음 (P5c D3·E7)
		if (DetentNudgeDeg > 0 && DetentIndex == DetentStartIndex) { bNudgeArmed = FMath::Abs(NudgeOffset) <= DetentNudgeDeg / 2 + NudgeTolerance; }
		PendingDetents.Add({false, DetentIndex});
		UE_LOG(LogAxisPart, Log, TEXT("%s detent=%d angle=%.3f deg | not-tested"), *GetName(), DetentIndex, DetentAnglesDeg[DetentIndex]);
	}
}
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
	if (Release == EVBoRAxisRelease::Detent)
	{
		if (!bDetentValid) { return; }
		Value = FMath::Clamp(Raw * (bReverse ? -1 : 1), static_cast<double>(DetentAnglesDeg[0]), static_cast<double>(DetentAnglesDeg.Last()));
		EvaluateDetent();
	}
	else { Value = FMath::Clamp(Raw * (bReverse ? -1 : 1), 0.0, Limit()); }
}
float AVBoRAxisPart::GetOpenRatio() const
{
	if (Release == EVBoRAxisRelease::Detent) { return 0.f; }
	return Limit() > SMALL_NUMBER ? static_cast<float>(Value / Limit()) : 0.f;
}
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
		if (HoldingHand != INDEX_NONE || bNeedsPalmOpen[Hand] || !PartMesh->GetStaticMesh() || !bDetentValid) { return; }
		HoldingHand = Hand; HoldingGrabber = Grabber;
		if (!IsHandTracked()) { HoldingHand = INDEX_NONE; HoldingGrabber = nullptr; bCancelPending = true; return; }
		++Grabs;
		bPaused = bRebase = false;
		UE_LOG(LogAxisPart, Log, TEXT("%s grab hand=%d"), *GetName(), Hand);
	}
	else if ((Event.Type == EIsdkPointerEventType::Unselect || Event.Type == EIsdkPointerEventType::Cancel) && HoldingGrabber == Grabber && !bDoorHold)
	{
		if (IsHandTracked() && !bPaused) { ReleasePart(false); }
		else { if (!bPaused) { ++Pauses; } bPaused = bRebase = true; }
	}
}

void AVBoRAxisPart::ReleasePart(bool bCancelSDK)
{
	if (HoldingHand == INDEX_NONE) { return; }
	HoldingHand = INDEX_NONE; HoldingGrabber = nullptr;
	bPaused = bRebase = bDoorHold = false;
	bNudgeArmed = true;
	// SDK 이벤트 중에도 들어옴. 취소는 Tick 에서만 실행함 (grabber 재진입 방지, P2b R1-3)
	bCancelPending |= bCancelSDK;
	OnPartReleased.Broadcast();
	UE_LOG(LogAxisPart, Log, TEXT("%s released | %s"), *GetName(), *GetDebugText());
}

void AVBoRAxisPart::ConnectDoor(AVBoRHingedPanel* Door, int32 Handle)
{
	LinkedDoor = Door; DoorHandle = Handle; Release = EVBoRAxisRelease::Return;
}
void AVBoRAxisPart::EndDoorHold(bool bLatch)
{
	if (bLatch && HoldingHand != INDEX_NONE) { bNeedsPalmOpen[HoldingHand] = true; }
	ReleasePart();
	bCancelPending = true;
}

void AVBoRAxisPart::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!FMath::IsFinite(DeltaSeconds) || DeltaSeconds <= 0) { return; }
	// 확정은 Move 에서 유지하고 알림만 미룸 (SDK 선택 배열 재진입 방지, P5b R1-1)
	const TArray<FDetentNotification> Notifications = MoveTemp(PendingDetents);
	for (const FDetentNotification& Notification : Notifications)
	{
		StateAudio->Play();
		if (Notification.bNudge) { OnDetentNudge.Broadcast(Notification.Value); }
		else { OnDetentChanged.Broadcast(Notification.Value); }
		if (IsActorBeingDestroyed()) { return; }
	}
	if (bCancelPending || bDoorHold || (HoldingHand == INDEX_NONE && GrabComponent->GetNumGrabbers()))
	{
		bCancelPending = false;
		GrabComponent->CancelSelection();
	}
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
	if (HoldingHand == INDEX_NONE && Release == EVBoRAxisRelease::Detent && bDetentValid && DetentAnglesDeg.IsValidIndex(DetentIndex))
	{
		const double Target = DetentAnglesDeg[DetentIndex];
		const double Step = FMath::Max(0.f, ReturnDegPerSecond) * DeltaSeconds;
		Value += FMath::Clamp(Target - Value, -Step, Step);
		AxisPivot->SetRelativeTransform(ValueTransform() * RestPivot);
	}
	EvaluateState();
	// Move delegate 안에서 ForceCancel 을 부르지 않음 (SDK 선택 배열 재진입 방지, P2b D5)
	// 풀린 문도 손잡이를 먼저 젖힘 (같은 손 넘김은 문턱에서만, P2c D4)
	if (LinkedDoor && HoldingHand != INDEX_NONE && !bDoorHold && GetOpenRatio() >= OpenThreshold)
	{
		if (LinkedDoor->BeginGrabFromAxisPart(this)) { bDoorHold = true; GrabComponent->CancelSelection(); }
	}
}

void AVBoRAxisPart::EvaluateState()
{
	if (bPaused) { return; }
	if (Release == EVBoRAxisRelease::Detent)
	{
		if (StateOverlay)
		{
			StateOverlay->SetVectorParameterValue(TEXT("ColorParam"), HoldingHand != INDEX_NONE ? GrabColor : Hovering.IsEmpty() ? FLinearColor::Transparent : HoverColor);
			PartMesh->SetOverlayMaterial(StateOverlay);
		}
		return;
	}
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
	if (Release == EVBoRAxisRelease::Detent)
	{
		return FString::Printf(TEXT("Approximate geometry | detent=%d angle=%.3f deg confirmed=%.3f deg | nudge=%.3f deg armed=%d | pauses=%d %.3f s | rebase discarded=%.3f deg | not-tested"),
			DetentIndex, Value, DetentAnglesDeg.IsValidIndex(DetentIndex) ? DetentAnglesDeg[DetentIndex] : 0.f, DetentNudgeDeg, bNudgeArmed, Pauses, PausedSeconds, RecoveryDiscard);
	}
	return FString::Printf(TEXT("Approximate geometry | ratio=%.3f | attempts=%d grabs=%d open/close=%d/%d max=%.3f | pauses=%d %.3f s | rebase discarded=%.3f %s | not-tested"),
		GetOpenRatio(), Attempts, Grabs, Opens, Closes, MaximumRatio, Pauses, PausedSeconds, RecoveryDiscard, Movement == EVBoRAxisMovement::Rotate ? TEXT("deg") : TEXT("cm"));
}
void AVBoRAxisPart::EndPlay(const EEndPlayReason::Type Reason)
{
	Grabbable->GetInteractionPointerEventDelegate().RemoveDynamic(this, &AVBoRAxisPart::HandlePointer);
	PartMesh->SetOverlayMaterial(nullptr);
	Super::EndPlay(Reason);
}
