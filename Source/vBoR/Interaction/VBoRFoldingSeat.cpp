#include "VBoRFoldingSeat.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

AVBoRFoldingSeat::AVBoRFoldingSeat()
{
	PrimaryActorTick.bCanEverTick = true;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("VehicleOrigin")));
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
	Super::OnConstruction(Transform);
	HingePivot->SetRelativeLocationAndRotation(HingeLocationCm, FRotator::ZeroRotator);
	SeatMesh->SetRelativeTransform(MeshRestFromRoot.GetRelativeTransform(FTransform(HingeLocationCm)));
	ResetSeat();
}

bool AVBoRFoldingSeat::Fold()
{
	if (State != EVBoRFoldingSeatState::Standing) { return false; }
	State = EVBoRFoldingSeatState::Folding;
	return true;
}

bool AVBoRFoldingSeat::Unfold()
{
	if (State != EVBoRFoldingSeatState::Folded) { return false; }
	State = EVBoRFoldingSeatState::Unfolding;
	return true;
}

void AVBoRFoldingSeat::ResetSeat()
{
	State = EVBoRFoldingSeatState::Standing;
	AngleDeg = 0;
	ApplyAngle();
}

void AVBoRFoldingSeat::ApplyAngle()
{
	// +Y 축 양의 회전으로 등받이 위쪽이 차 앞 +X 로 숙여짐 (P4e D1, deg)
	HingePivot->SetRelativeRotation(FQuat(FVector::YAxisVector, FMath::DegreesToRadians(AngleDeg)));
}

void AVBoRFoldingSeat::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (State != EVBoRFoldingSeatState::Folding && State != EVBoRFoldingSeatState::Unfolding) { return; }
	const bool bFolding = State == EVBoRFoldingSeatState::Folding;
	const float Limit = FMath::Clamp(FoldAngleDeg, 1.0f, 170.0f);
	const float Target = bFolding ? Limit : 0;
	AngleDeg = FMath::Clamp(FMath::FInterpConstantTo(AngleDeg, Target, FMath::Max(0.0f, DeltaSeconds), FMath::Max(0.1f, SpeedDegPerSecond)), 0.0f, Limit);
	ApplyAngle();
	if (AngleDeg == Target) { State = bFolding ? EVBoRFoldingSeatState::Folded : EVBoRFoldingSeatState::Standing; }
}
