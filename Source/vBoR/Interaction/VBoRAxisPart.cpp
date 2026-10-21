#include "VBoRAxisPart.h"
#include "VBoRGrabCollider.h"
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
	SoundAudio.SetNum(4); SoundAudio[0] = StateAudio;
	SoundAudio[1] = CreateDefaultSubobject<UAudioComponent>(TEXT("ClosedCueAudio")); SoundAudio[1]->SetupAttachment(AxisPivot); SoundAudio[1]->bAutoActivate = false;
	SoundAudio[2] = CreateDefaultSubobject<UAudioComponent>(TEXT("DetentCueAudio")); SoundAudio[2]->SetupAttachment(AxisPivot); SoundAudio[2]->bAutoActivate = false;
	SoundAudio[3] = CreateDefaultSubobject<UAudioComponent>(TEXT("NudgeCueAudio")); SoundAudio[3]->SetupAttachment(AxisPivot); SoundAudio[3]->bAutoActivate = false;
	OpenedCue.Sound = StateAudio->Sound;
	ClosedCue.Sound = StateAudio->Sound;
	DetentCue.Sound = StateAudio->Sound;
	NudgeCue.Sound = StateAudio->Sound;

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
		}
	}
	Grabbable->SetGrabCollider(ConfigureVBoRGrabCollider(PartMesh, GrabBox, bGrabWithMesh,
		PartMesh->GetStaticMesh() != nullptr && bDetentValid, MeshRestFromRoot));
	Grabbable->SetGrabInputMethodAllowed(EIsdkGrabInputMethod::Palm, bAllowPalmGrab && PartMesh->GetStaticMesh() != nullptr && bDetentValid);
	Grabbable->SetGrabInputMethodAllowed(EIsdkGrabInputMethod::Pinch, bAllowPinchGrab && PartMesh->GetStaticMesh() != nullptr && bDetentValid);
	ConfigurePush();
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
	if (!bDetentValid || (HoldingHand == INDEX_NONE && PushHand == INDEX_NONE) || bPaused || !DetentAnglesDeg.IsValidIndex(DetentIndex)) { return; }
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

void AVBoRAxisPart::ReadHoldingPose(FTransform& Pose) const
{
	if (bPinch) { HoldingGrabber->GetPinchGrabTransform(Pose); }
	else { HoldingGrabber->GetPalmGrabTransform(Pose); }
}

bool AVBoRAxisPart::ReadGrabClosed(int32 Hand, bool Pinch, bool& Closed) const
{
	const auto* Pawn = Cast<AVBoRHandRigPawn>(UGameplayStatics::GetPlayerPawn(this,0));
	const EOculusXRHandType Type = Hand == 0 ? EOculusXRHandType::HandLeft : EOculusXRHandType::HandRight;
	if (!Pawn || !UOculusXRInputFunctionLibrary::IsHandPositionValid(Type) ||
		UOculusXRInputFunctionLibrary::GetTrackingConfidence(Type) != EOculusXRTrackingConfidence::High) { return false; }
	if (Pinch) { return Hand == 0 ? Pawn->LeftRig->ReadPinchGrab(Closed) : Pawn->RightRig->ReadPinchGrab(Closed); }
	return Hand == 0 ? Pawn->LeftRig->ReadPalmGrab(Closed) : Pawn->RightRig->ReadPalmGrab(Closed);
}

UIsdkGrabberComponent* AVBoRAxisPart::GetPushGrabber(int32 Hand) const
{
	const auto* Pawn = Cast<AVBoRHandRigPawn>(UGameplayStatics::GetPlayerPawn(this,0));
	return Pawn ? Pawn->GetHandRig(Hand)->GetGrabber() : nullptr;
}

void AVBoRAxisPart::ReadPushHands(FVBoRDoorHand (&Hands)[2]) const
{
	const auto* Pawn = Cast<AVBoRHandRigPawn>(UGameplayStatics::GetPlayerPawn(this,0));
	if (!Pawn) { return; }
	const FTransform RestWorld = RestPivot * GetActorTransform();
	for (int32 H=0; H<2; ++H)
	{
		const EOculusXRHandType Type = H == 0 ? EOculusXRHandType::HandLeft : EOculusXRHandType::HandRight;
		if (!UOculusXRInputFunctionLibrary::IsHandPositionValid(Type) ||
			UOculusXRInputFunctionLibrary::GetTrackingConfidence(Type) != EOculusXRTrackingConfidence::High || !Pawn->ReadRawPalmAndTips(H,Hands[H]))
		{ Hands[H].bTracked = false; continue; }
		for (FVector& Point : Hands[H].Points) { Point = RestWorld.InverseTransformPosition(Point); }
	}
}

void AVBoRAxisPart::ConfigurePush()
{
	bPushEnabled = bAllowPush && bDetentValid && PartMesh->GetStaticMesh() && Movement == EVBoRAxisMovement::Rotate;
	if (bAllowPush && Movement != EVBoRAxisMovement::Rotate)
	{ UE_LOG(LogAxisPart,Warning,TEXT("%s: Push requires Rotate; push disabled"),*GetName()); }
	if (!bPushEnabled) { ClearPush(); return; }
	const FBox Bounds = PartMesh->GetStaticMesh()->GetBoundingBox().TransformBy(PartMesh->GetRelativeTransform());
	// 축에 수직인 두 bounds 축 중 긴 쪽이 레버 길이임 (메시 피벗 대신 쉬는 bounds, P5m D3)
	const int32 A = Axis == EAxis::Y ? 1 : Axis == EAxis::Z ? 2 : 0;
	const int32 B = (A+1)%3, C = (A+2)%3;
	const int32 R = Bounds.GetExtent()[B] >= Bounds.GetExtent()[C] ? B : C;
	PushRadial = FVector::ZeroVector; PushRadial[R] = Bounds.GetCenter()[R] < 0 ? -1 : 1;
	PushNormal = FVector::CrossProduct(LocalAxis(),PushRadial);
	PushBounds = FBox(ForceInit);
	for (int32 Corner=0; Corner<8; ++Corner)
	{
		const FVector P(Corner&1 ? Bounds.Max.X : Bounds.Min.X,Corner&2 ? Bounds.Max.Y : Bounds.Min.Y,Corner&4 ? Bounds.Max.Z : Bounds.Min.Z);
		PushBounds += FVector(FVector::DotProduct(P,PushRadial),FVector::DotProduct(P,PushNormal),FVector::DotProduct(P,LocalAxis()));
	}
	ClearPush();
}

void AVBoRAxisPart::ClearPush()
{
	if (PushHand != INDEX_NONE) { bNudgeArmed = true; }
	PushHand = PushPoint = PushFace = INDEX_NONE;
	PreviousPushHands[0] = PreviousPushHands[1] = FVBoRDoorHand(); PreviousPushValue = Value;
}

bool AVBoRAxisPart::FindPush(int32 Hand, const FVBoRDoorHand& Input, FVector& Point, int32& PointIndex, int32& FaceIndex) const
{
	if (!Input.bTracked) { return false; }
	const bool Continuing = PushHand == Hand;
	const FQuat Rotation = ValueTransform().GetRotation();
	const FQuat PreviousRotation(LocalAxis(),FMath::DegreesToRadians(PreviousPushValue*(bReverse?-1:1)));
	auto ToFace = [this](const FVector& P) { return FVector(FVector::DotProduct(P,PushRadial),FVector::DotProduct(P,PushNormal),FVector::DotProduct(P,LocalAxis())); };
	const double Margin = FMath::Max(0.f,PushMarginCm);
	auto Inside = [this,Margin](const FVector& P)
	{
		return P.X >= PushBounds.Min.X-Margin && P.X <= PushBounds.Max.X+Margin &&
			P.Z >= PushBounds.Min.Z-Margin && P.Z <= PushBounds.Max.Z+Margin;
	};
	bool Found = false; double Deepest = -Margin;
	for (int32 I=0; I<5; ++I)
	{
		if (Continuing && I != PushPoint) { continue; }
		const FVector P = ToFace(Rotation.UnrotateVector(Input.Points[I]));
		if (P.ContainsNaN() || !Inside(P) || FMath::Square(P.X)+FMath::Square(P.Y) < FMath::Square(FMath::Max(0.f,PushMinAxisDistanceCm))) { continue; }
		const FVector Prev = ToFace(PreviousRotation.UnrotateVector(PreviousPushHands[Hand].Points[I]));
		for (int32 Face=0; Face<2; ++Face)
		{
			if (Continuing && Face != PushFace) { continue; }
			const double Depth = Face == 0 ? P.Y-PushBounds.Min.Y : PushBounds.Max.Y-P.Y;
			const double PreviousDepth = Face == 0 ? Prev.Y-PushBounds.Min.Y : PushBounds.Max.Y-Prev.Y;
			if (Depth < (Continuing ? -Margin : 1.e-6) || Depth > PushBounds.Max.Y-PushBounds.Min.Y+Margin) { continue; }
			// 면 위·안쪽의 복구 점은 새 crossing 이 아님 (P5m D3·E9·E13)
			if (!Continuing && (!PreviousPushHands[Hand].bTracked || Prev.ContainsNaN() || !Inside(Prev) || PreviousDepth >= -1.e-6)) { continue; }
			if (Depth > Deepest) { Deepest=Depth; Point=Input.Points[I]; PointIndex=I; FaceIndex=Face; Found=true; }
		}
	}
	return Found;
}

void AVBoRAxisPart::AdvancePush(const FVBoRDoorHand (&Input)[2])
{
	FVBoRDoorHand Hands[2] = {Input[0],Input[1]};
	for (int32 H=0; H<2; ++H)
	{
		const auto* Grabber = GetPushGrabber(H);
		if (Grabber && Grabber->HasSelectedInteractable()) { Hands[H].bTracked = false; }
	}
	if (!bPushEnabled || HoldingHand != INDEX_NONE) { ClearPush(); return; }
	FVector Point = FVector::ZeroVector;
	bool Found = PushHand != INDEX_NONE && FindPush(PushHand,Hands[PushHand],Point,PushPoint,PushFace);
	if (!Found)
	{
		if (PushHand != INDEX_NONE) { bNudgeArmed = true; }
		PushHand = PushPoint = PushFace = INDEX_NONE;
		for (int32 H=0; H<2; ++H)
		{
			if (FindPush(H,Hands[H],Point,PushPoint,PushFace)) { PushHand=H; ++PushStarts; Found=true; break; }
		}
	}
	if (Found)
	{
		const double R = FVector::DotProduct(Point,PushRadial), N = FVector::DotProduct(Point,PushNormal);
		const double Radius = FMath::Sqrt(R*R+N*N), Plane = PushFace == 0 ? PushBounds.Min.Y : PushBounds.Max.Y;
		if (Radius > FMath::Abs(Plane)+UE_SMALL_NUMBER)
		{
			const double Current = Value*(bReverse?-1:1);
			const double Target = FMath::RadiansToDegrees(FMath::Atan2(N,R)-FMath::Asin(Plane/Radius));
			const double Delta = FMath::FindDeltaAngleDegrees(Current,Target);
			// 처음 닿은 면의 법선 방향으로만 밂. 걸림 확정은 잡기와 같은 경로 사용 (P5m D3·D4, deg)
			const double Angle = Current + (PushFace == 0 ? FMath::Max(0.,Delta) : FMath::Min(0.,Delta));
			AcceptTransform(FTransform(FQuat(LocalAxis(),FMath::DegreesToRadians(Angle))));
			AxisPivot->SetRelativeTransform(ValueTransform()*RestPivot);
		}
	}
	PreviousPushHands[0]=Hands[0]; PreviousPushHands[1]=Hands[1]; PreviousPushValue=Value;
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
		const bool Pinch = Grabber->IsInputMethodActive(EIsdkGrabInputMethod::Pinch);
		if (Pinch ? !bAllowPinchGrab : !bAllowPalmGrab) { bCancelPending = true; return; }
		HoldingHand = Hand; HoldingGrabber = Grabber; bPinch = Pinch;
		if (!IsHandTracked()) { HoldingHand = INDEX_NONE; HoldingGrabber = nullptr; bCancelPending = true; return; }
		ClearPush();
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
	if (bLatch && HoldingHand != INDEX_NONE) { bNeedsPalmOpen[HoldingHand] = true; bNeedsPinchOpen[HoldingHand] = bPinch; }
	ReleasePart();
	bCancelPending = true;
}

void AVBoRAxisPart::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	++SoundFrame; AdvanceSounds(DeltaSeconds);
	if (!FMath::IsFinite(DeltaSeconds) || DeltaSeconds <= 0) { return; }
	// 확정은 Move 에서 유지하고 알림만 미룸 (SDK 선택 배열 재진입 방지, P5b R1-1)
	const TArray<FDetentNotification> Notifications = MoveTemp(PendingDetents);
	for (const FDetentNotification& Notification : Notifications)
	{
		const int32 Slot = Notification.bNudge ? 3 : 2;
		SoundPlayback[Slot].Queue(Notification.bNudge ? NudgeCue : DetentCue, SoundAudio[Slot], SoundFrame);
		if (Notification.bNudge) { OnDetentNudge.Broadcast(Notification.Value); }
		else { OnDetentChanged.Broadcast(Notification.Value); }
		if (IsActorBeingDestroyed()) { return; }
	}
	if (bCancelPending || bDoorHold || (HoldingHand == INDEX_NONE && GrabComponent->GetNumGrabbers()))
	{
		bCancelPending = false;
		GrabComponent->CancelSelection();
	}
	for (int32 H = 0; H < 2; ++H)
	{
		bool bClosed = true;
		if (bNeedsPalmOpen[H] &&
			ReadGrabClosed(H,bNeedsPinchOpen[H],bClosed) && !bClosed) { bNeedsPalmOpen[H] = false; }
	}
	if (HoldingHand != INDEX_NONE && !bDoorHold)
	{
		if (!IsHandTracked()) { if (!bPaused) { ++Pauses; } bPaused = bRebase = true; PausedSeconds += DeltaSeconds; return; }
		if (bRebase && HoldingGrabber)
		{
			FTransform Pose; ReadHoldingPose(Pose);
			FIsdkGrabPose GrabPose = Transformer->LastPose; GrabPose.Pose.Position = FVector3f(Pose.GetLocation()); GrabPose.Pose.Orientation = Pose.GetRotation();
			Transformer->Rebase(GrabPose);
			return;
		}
		bool bClosed = true;
		if (ReadGrabClosed(HoldingHand,bPinch,bClosed) && !bClosed) { ReleasePart(); }
		else if (!Transformer->IsTransformerActive() && HoldingGrabber)
		{
			// SDK 가 추적 손실 때 선택을 끝내도 실제 손을 펼 때까지 보유함 (P2b D7)
			FTransform Pose; ReadHoldingPose(Pose);
			FIsdkGrabPose GrabPose = Transformer->LastPose; GrabPose.Pose.Position = FVector3f(Pose.GetLocation()); GrabPose.Pose.Orientation = Pose.GetRotation();
			Transformer->BeginTransform({GrabPose}, FIsdkTargetTransform(AxisPivot));
		}
		else if (HoldingGrabber && !GrabComponent->GetNumGrabbers())
		{
			FTransform Pose; ReadHoldingPose(Pose);
			FIsdkGrabPose GrabPose = Transformer->LastPose; GrabPose.Pose.Position = FVector3f(Pose.GetLocation()); GrabPose.Pose.Orientation = Pose.GetRotation();
			AxisPivot->SetRelativeTransform(Transformer->UpdateTransform({GrabPose}, FIsdkTargetTransform(AxisPivot)));
		}
	}
	FVBoRDoorHand Hands[2];
	if (bPushEnabled) { ReadPushHands(Hands); }
	AdvancePush(Hands);
	if (HoldingHand == INDEX_NONE && PushHand == INDEX_NONE && Release == EVBoRAxisRelease::Return)
	{
		const double Speed = Movement == EVBoRAxisMovement::Rotate ? ReturnDegPerSecond : ReturnCmPerSecond;
		Value = FMath::Max(0.0, Value - FMath::Max(0.0, Speed) * DeltaSeconds);
		AxisPivot->SetRelativeTransform(ValueTransform() * RestPivot);
	}
	if (HoldingHand == INDEX_NONE && PushHand == INDEX_NONE && Release == EVBoRAxisRelease::Detent && bDetentValid && DetentAnglesDeg.IsValidIndex(DetentIndex))
	{
		const double Target = DetentAnglesDeg[DetentIndex];
		const double Step = FMath::Max(0.f, ReturnDegPerSecond) * DeltaSeconds;
		Value += FMath::Clamp(Target - Value, -Step, Step);
		AxisPivot->SetRelativeTransform(ValueTransform() * RestPivot);
	}
	PreviousPushValue = Value;
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
			StateOverlay->SetVectorParameterValue(TEXT("ColorParam"), HoldingHand != INDEX_NONE ? GrabColor : PushHand != INDEX_NONE ? PushingColor : Hovering.IsEmpty() ? FLinearColor::Transparent : HoverColor);
			PartMesh->SetOverlayMaterial(StateOverlay);
		}
		return;
	}
	const float Ratio = GetOpenRatio(); MaximumRatio = FMath::Max(MaximumRatio, static_cast<double>(Ratio));
	if (Ratio <= OpenRearm) { bOpenArmed = true; }
	if (Ratio >= ClosedRearm) { bClosedArmed = true; }
	if (bOpenArmed && Ratio >= OpenThreshold) { bOpenArmed = false; ++Opens; SoundPlayback[0].Queue(OpenedCue, SoundAudio[0], SoundFrame); OnPartOpened.Broadcast(); UE_LOG(LogAxisPart, Log, TEXT("%s opened | %s"), *GetName(), *GetDebugText()); }
	if (bClosedArmed && Ratio <= ClosedThreshold) { bClosedArmed = false; ++Closes; SoundPlayback[1].Queue(ClosedCue, SoundAudio[1], SoundFrame); OnPartClosed.Broadcast(); UE_LOG(LogAxisPart, Log, TEXT("%s closed | %s"), *GetName(), *GetDebugText()); }
	if (StateOverlay)
	{
		StateOverlay->SetVectorParameterValue(TEXT("ColorParam"), HoldingHand != INDEX_NONE ? GrabColor : PushHand != INDEX_NONE ? PushingColor : Ratio >= OpenThreshold ? OpenColor : Hovering.IsEmpty() ? FLinearColor::Transparent : HoverColor);
		PartMesh->SetOverlayMaterial(StateOverlay);
	}
}
FString AVBoRAxisPart::GetDebugText() const
{
	const FString Input = FString::Printf(TEXT("grab=%s hand=%d push=%d point=%d face=%d starts=%d | "), bPinch ? TEXT("pinch") : TEXT("palm"), HoldingHand, PushHand, PushPoint, PushFace, PushStarts);
	if (Release == EVBoRAxisRelease::Detent)
	{
		return Input + FString::Printf(TEXT("Approximate geometry | detent=%d angle=%.3f deg confirmed=%.3f deg | nudge=%.3f deg armed=%d | pauses=%d %.3f s | rebase discarded=%.3f deg | not-tested"),
			DetentIndex, Value, DetentAnglesDeg.IsValidIndex(DetentIndex) ? DetentAnglesDeg[DetentIndex] : 0.f, DetentNudgeDeg, bNudgeArmed, Pauses, PausedSeconds, RecoveryDiscard);
	}
	return Input + FString::Printf(TEXT("Approximate geometry | ratio=%.3f | attempts=%d grabs=%d open/close=%d/%d max=%.3f | pauses=%d %.3f s | rebase discarded=%.3f %s | not-tested"),
		GetOpenRatio(), Attempts, Grabs, Opens, Closes, MaximumRatio, Pauses, PausedSeconds, RecoveryDiscard, Movement == EVBoRAxisMovement::Rotate ? TEXT("deg") : TEXT("cm"));
}
void AVBoRAxisPart::EndPlay(const EEndPlayReason::Type Reason)
{
	for (int32 I = 0; I < 4; ++I) { SoundPlayback[I].Reset(SoundAudio[I], false); }
	Grabbable->GetInteractionPointerEventDelegate().RemoveDynamic(this, &AVBoRAxisPart::HandlePointer);
	PartMesh->SetOverlayMaterial(nullptr);
	ClearPush();
	Super::EndPlay(Reason);
}

void AVBoRAxisPart::AdvanceSounds(double Seconds)
{
	SoundPlayback[0].Advance(OpenedCue, SoundAudio[0], Seconds, SoundFrame);
	SoundPlayback[1].Advance(ClosedCue, SoundAudio[1], Seconds, SoundFrame);
	SoundPlayback[2].Advance(DetentCue, SoundAudio[2], Seconds, SoundFrame);
	SoundPlayback[3].Advance(NudgeCue, SoundAudio[3], Seconds, SoundFrame);
}
