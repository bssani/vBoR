#include "VBoRFoldingSeat.h"

#include "Components/AudioComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

AVBoRFoldingSeat::AVBoRFoldingSeat()
{
	PrimaryActorTick.bCanEverTick = true;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("VehicleOrigin")));
#if WITH_EDITORONLY_DATA
	MotionDisplay = CreateEditorOnlyDefaultSubobject<UVBoRMotionGuideComponent>(TEXT("MotionDisplay"));
	if (MotionDisplay) { MotionDisplay->SetupAttachment(RootComponent); }
#endif
	HingePivot = CreateDefaultSubobject<USceneComponent>(TEXT("HingePivot"));
	HingePivot->SetupAttachment(RootComponent);
	SeatMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ApproximateSeatBack"));
	SeatMesh->SetupAttachment(HingePivot);
	SeatMesh->SetMobility(EComponentMobility::Movable);
	SeatMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	SeatMesh->SetStaticMesh(Cube.Object);
}

void AVBoRFoldingSeat::OnConstruction(const FTransform& Transform)
{
#if WITH_EDITORONLY_DATA
	if (MotionDisplay) { MotionDisplay->ClearPreview(); }
#endif
	Super::OnConstruction(Transform);
	HingePivot->SetRelativeLocationAndRotation(HingeLocationCm, FRotator::ZeroRotator);
	SeatMesh->SetRelativeTransform(MeshRestFromRoot.GetRelativeTransform(FTransform(HingeLocationCm)));
	ResetSeat();

#if WITH_EDITORONLY_DATA
	if (MotionDisplay) { MotionDisplay->Refresh(); }
#endif
}

bool AVBoRFoldingSeat::Fold()
{
	if (State != EVBoRFoldingSeatState::Standing) { return false; }
	SetState(EVBoRFoldingSeatState::Folding);
	return true;
}

bool AVBoRFoldingSeat::Unfold()
{
	if (State != EVBoRFoldingSeatState::Folded) { return false; }
	SetState(EVBoRFoldingSeatState::Unfolding);
	return true;
}

void AVBoRFoldingSeat::ResetSeat()
{
	for (int32 I = 0; I < UE_ARRAY_COUNT(SoundPlayback); ++I) { SoundPlayback[I].Reset(SoundAudio[I], true); }
	AngleDeg = 0;
	ApplyAngle();
	SetState(EVBoRFoldingSeatState::Standing);
	if (HasActorBegunPlay()) { OnReset.Broadcast(this); }
}

void AVBoRFoldingSeat::EndPlay(const EEndPlayReason::Type Reason)
{
	for (int32 I = 0; I < UE_ARRAY_COUNT(SoundPlayback); ++I) { SoundPlayback[I].Reset(SoundAudio[I], false); }
	Super::EndPlay(Reason);
}

void AVBoRFoldingSeat::SetState(EVBoRFoldingSeatState NewState)
{
	if (State == NewState) { return; }
	State = NewState;
	if (HasActorBegunPlay()) { OnStateChanged.Broadcast(this, State); }
}

void AVBoRFoldingSeat::ApplyAngle()
{
	// Positive +Y rotation tilts backrest top toward car front +X (P4e D1, deg)
	HingePivot->SetRelativeRotation(HingePoseAt(AngleDeg).GetRotation());
}

void AVBoRFoldingSeat::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	++SoundFrame;
	if (State != EVBoRFoldingSeatState::Folding && State != EVBoRFoldingSeatState::Unfolding) { return; }
	const bool bFolding = State == EVBoRFoldingSeatState::Folding;
	const float Limit = FMath::Clamp(FoldAngleDeg, 1.0f, 170.0f);
	const float Target = bFolding ? Limit : 0;
	const float TravelSeconds = bFolding ? FoldSeconds : UnfoldSeconds;
	const float Speed = TravelSeconds > 0 ? Limit / TravelSeconds : FMath::Max(0.1f, SpeedDegPerSecond);
	const float Before = AngleDeg;
	AngleDeg = FMath::Clamp(FMath::FInterpConstantTo(AngleDeg, Target, FMath::Max(0.0f, DeltaSeconds), Speed), 0.0f, Limit);
	ApplyAngle();
	const int32 Slot = bFolding ? 0 : 1;
	const auto& Cue = bFolding ? FoldingCue : UnfoldingCue;
	const float Used = FMath::Abs(AngleDeg - Before) / Speed;
	if (Used > 0 && Cue.Sound && !SoundPlayback[Slot].HasPending())
	{
		VBoREnsureCueAudio(SoundAudio[Slot], Cue, this, HingePivot);
		SoundPlayback[Slot].BeginHeld(Cue, SoundAudio[Slot], SoundFrame);
	}
	SoundPlayback[Slot].Advance(Cue, SoundAudio[Slot], Used, SoundFrame);
	if (AngleDeg == Target)
	{
		SoundPlayback[Slot].EndHeld(Cue, SoundAudio[Slot]);
		SetState(bFolding ? EVBoRFoldingSeatState::Folded : EVBoRFoldingSeatState::Standing);
	}
}

FTransform AVBoRFoldingSeat::HingePoseAt(double Angle) const
{
	return FTransform(FQuat(FVector::YAxisVector, FMath::DegreesToRadians(Angle)), HingeLocationCm);
}
