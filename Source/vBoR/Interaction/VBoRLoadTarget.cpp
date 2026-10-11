#include "VBoRLoadTarget.h"
#include "VBoRCarryObject.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Engine/StaticMesh.h"
#include "IsdkContentAssetPaths.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

AVBoRLoadTarget::AVBoRLoadTarget()
{
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("GoalRoot")));
	Guide = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("GoalGuide"));
	Guide->SetupAttachment(RootComponent);
	Guide->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(IsdkContentAssetPaths::Engine::BasicShapes::Cube);
	Guide->SetStaticMesh(Cube.Object);
	LoadingRegion = CreateDefaultSubobject<UBoxComponent>(TEXT("LoadingRegion"));
	LoadingRegion->SetupAttachment(RootComponent);
	LoadingRegion->SetBoxExtent(FVector(50));
	LoadingRegion->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	LoadingRegion->SetHiddenInGame(true);
}

void AVBoRLoadTarget::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	Guide->SetRelativeScale3D(GoalSizeCm.GetAbs() / 100.0);
	Guide->SetVisibility(bShowGuide);
}

void AVBoRLoadTarget::BeginPlay()
{
	Super::BeginPlay();
	UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, IsdkContentAssetPaths::Materials::GrabbableDefault);
	if (Material)
	{
		UMaterialInstanceDynamic* Dynamic = UMaterialInstanceDynamic::Create(Material, this);
		Dynamic->SetVectorParameterValue(TEXT("ColorParam"), GuideColor);
		Guide->SetMaterial(0, Dynamic);
	}
}

FTransform AVBoRLoadTarget::GetGoalPose() const
{
	FTransform Goal = GetActorTransform();
	Goal.SetScale3D(FVector::OneVector);
	return Goal;
}

bool AVBoRLoadTarget::ContainsPose(const AVBoRCarryObject* Object, const FTransform& Pose, const FTransform& Reference, const FVector& Half) const
{
	if (!Object) { return false; }
	const FBox Bounds = Object->GetCollisionBoundsInPose(Pose, Reference);
	if (!Bounds.IsValid) { return false; }
	return Bounds.Min.X >= -Half.X - 0.01 && Bounds.Max.X <= Half.X + 0.01 &&
		Bounds.Min.Y >= -Half.Y - 0.01 && Bounds.Max.Y <= Half.Y + 0.01 &&
		Bounds.Min.Z >= -Half.Z - 0.01 && Bounds.Max.Z <= Half.Z + 0.01;
}

bool AVBoRLoadTarget::IsStaticFit(const AVBoRCarryObject* Object) const
{
	if (!Object) { return false; }
	FTransform Goal = GetGoalPose();
	Goal.SetScale3D(Object->GetActorScale3D());
	return ContainsPose(Object, Goal, GetGoalPose(), GoalSizeCm.GetAbs() / 2.0) && Object->IsPoseClear(Goal);
}

bool AVBoRLoadTarget::IsObjectInside(const AVBoRCarryObject* Object) const
{
	FTransform RegionFrame = LoadingRegion->GetComponentTransform();
	RegionFrame.RemoveScaling();
	return Object && ContainsPose(Object, Object->GetActorTransform(), RegionFrame, LoadingRegion->GetScaledBoxExtent().GetAbs()) &&
		Object->IsPoseClear(Object->GetActorTransform());
}
