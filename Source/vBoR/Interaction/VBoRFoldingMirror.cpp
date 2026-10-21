#include "VBoRFoldingMirror.h"
#include "Components/AudioComponent.h"

#include "VBoRHingedPanel.h"
#include "VBoRVehiclePowerComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

AVBoRFoldingMirror::AVBoRFoldingMirror()
{
	PrimaryActorTick.bCanEverTick = true;
	SpectatorName = FText::FromString(TEXT("Side Mirror"));
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("VehicleOrigin")));
	HingePivot = CreateDefaultSubobject<USceneComponent>(TEXT("HingePivot")); HingePivot->SetupAttachment(RootComponent);
	MirrorMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MirrorMesh")); MirrorMesh->SetupAttachment(HingePivot);
	MirrorMesh->SetMobility(EComponentMobility::Movable);
	MirrorMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	MirrorMesh->SetStaticMesh(Cube.Object);
	SoundAudio.SetNum(4);
	SoundAudio[0] = CreateDefaultSubobject<UAudioComponent>(TEXT("UnfoldMotorCueAudio")); SoundAudio[0]->SetupAttachment(MirrorMesh); SoundAudio[0]->bAutoActivate = false;
	SoundAudio[1] = CreateDefaultSubobject<UAudioComponent>(TEXT("FoldMotorCueAudio")); SoundAudio[1]->SetupAttachment(MirrorMesh); SoundAudio[1]->bAutoActivate = false;
	SoundAudio[2] = CreateDefaultSubobject<UAudioComponent>(TEXT("UnfoldedCueAudio")); SoundAudio[2]->SetupAttachment(MirrorMesh); SoundAudio[2]->bAutoActivate = false;
	SoundAudio[3] = CreateDefaultSubobject<UAudioComponent>(TEXT("FoldedCueAudio")); SoundAudio[3]->SetupAttachment(MirrorMesh); SoundAudio[3]->bAutoActivate = false;

}

void AVBoRFoldingMirror::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	AngleDeg = 0; State = EVBoRFoldingMirrorState::Unfolded;
	const FTransform RestHinge(HingeRotation, HingeLocationCm);
	HingePivot->SetRelativeTransform(RestHinge);
	MirrorMesh->SetRelativeTransform(MeshRestFromRoot.GetRelativeTransform(RestHinge));
}

void AVBoRFoldingMirror::BeginPlay()
{
	Super::BeginPlay();
	if (auto* Door = Cast<AVBoRHingedPanel>(GetAttachParentActor()))
	{ AttachToComponent(Door->HingePivot, FAttachmentTransformRules::KeepWorldTransform); }
	ResetMirror();
	Power = UVBoRVehiclePowerComponent::FindUnique(GetWorld());
	if (Power)
	{
		Power->OnPowerChanged.AddUniqueDynamic(this, &AVBoRFoldingMirror::HandlePowerChanged);
		if (Power->IsPowerOn()) { HandlePowerChanged(true); }
	}
	else { UE_LOG(LogTemp, Warning, TEXT("Side mirror: No vehicle power; staying folded")); }
}

void AVBoRFoldingMirror::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	for (int32 I = 0; I < 4; ++I) { SoundPlayback[I].Reset(SoundAudio[I], false); }
	if (Power) { Power->OnPowerChanged.RemoveDynamic(this, &AVBoRFoldingMirror::HandlePowerChanged); }
	Super::EndPlay(EndPlayReason);
}

void AVBoRFoldingMirror::ApplyAngle()
{
	const FQuat Rotation = HingeRotation.Quaternion() * FQuat(FVector::ZAxisVector, FMath::DegreesToRadians(AngleDeg));
	HingePivot->SetRelativeLocationAndRotation(HingeLocationCm, Rotation);
}

void AVBoRFoldingMirror::ResetMirror()
{
	for (int32 I = 0; I < 4; ++I) { SoundPlayback[I].Reset(SoundAudio[I], true); }
	AngleDeg = FMath::Clamp(FoldAngleDeg, -170.f, 170.f);
	State = EVBoRFoldingMirrorState::Folded; ApplyAngle();
}

void AVBoRFoldingMirror::HandlePowerChanged(bool bOn)
{
	if (State == EVBoRFoldingMirrorState::Unfolding) { SoundPlayback[0].EndHeld(UnfoldMotorCue, SoundAudio[0]); }
	if (State == EVBoRFoldingMirrorState::Folding) { SoundPlayback[1].EndHeld(FoldMotorCue, SoundAudio[1]); }
	SoundPlayback[bOn ? 0 : 1].BeginHeld(bOn ? UnfoldMotorCue : FoldMotorCue, SoundAudio[bOn ? 0 : 1], SoundFrame);
	State = bOn ? EVBoRFoldingMirrorState::Unfolding : EVBoRFoldingMirrorState::Folding;
}

void AVBoRFoldingMirror::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	++SoundFrame; AdvanceSounds(DeltaSeconds);
	if (State != EVBoRFoldingMirrorState::Unfolding && State != EVBoRFoldingMirrorState::Folding) { return; }
	const bool bUnfold = State == EVBoRFoldingMirrorState::Unfolding;
	const float Target = bUnfold ? 0 : FMath::Clamp(FoldAngleDeg, -170.f, 170.f);
	AngleDeg = FMath::FInterpConstantTo(AngleDeg, Target, FMath::Max(0.f, DeltaSeconds), FMath::Max(.1f, SpeedDegPerSecond));
	ApplyAngle();
	if (AngleDeg == Target)
	{
		State = bUnfold ? EVBoRFoldingMirrorState::Unfolded : EVBoRFoldingMirrorState::Folded;
		SoundPlayback[bUnfold ? 0 : 1].EndHeld(bUnfold ? UnfoldMotorCue : FoldMotorCue, SoundAudio[bUnfold ? 0 : 1]);
		SoundPlayback[bUnfold ? 2 : 3].Queue(bUnfold ? UnfoldedCue : FoldedCue, SoundAudio[bUnfold ? 2 : 3], SoundFrame);
	}
}

void AVBoRFoldingMirror::AdvanceSounds(double Seconds)
{
	SoundPlayback[0].Advance(UnfoldMotorCue, SoundAudio[0], Seconds, SoundFrame);
	SoundPlayback[1].Advance(FoldMotorCue, SoundAudio[1], Seconds, SoundFrame);
	SoundPlayback[2].Advance(UnfoldedCue, SoundAudio[2], Seconds, SoundFrame);
	SoundPlayback[3].Advance(FoldedCue, SoundAudio[3], Seconds, SoundFrame);
}
