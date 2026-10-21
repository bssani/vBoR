#include "VBoRFoldingMirror.h"

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
	AngleDeg = FMath::Clamp(FoldAngleDeg, -170.f, 170.f);
	State = EVBoRFoldingMirrorState::Folded; ApplyAngle();
}

void AVBoRFoldingMirror::HandlePowerChanged(bool bOn)
{
	State = bOn ? EVBoRFoldingMirrorState::Unfolding : EVBoRFoldingMirrorState::Folding;
}

void AVBoRFoldingMirror::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (State != EVBoRFoldingMirrorState::Unfolding && State != EVBoRFoldingMirrorState::Folding) { return; }
	const bool bUnfold = State == EVBoRFoldingMirrorState::Unfolding;
	const float Target = bUnfold ? 0 : FMath::Clamp(FoldAngleDeg, -170.f, 170.f);
	AngleDeg = FMath::FInterpConstantTo(AngleDeg, Target, FMath::Max(0.f, DeltaSeconds), FMath::Max(.1f, SpeedDegPerSecond));
	ApplyAngle();
	if (AngleDeg == Target) { State = bUnfold ? EVBoRFoldingMirrorState::Unfolded : EVBoRFoldingMirrorState::Folded; }
}
