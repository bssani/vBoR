#include "VBoRBoxStand.h"
#include "VBoRCarryObject.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "UObject/ConstructorHelpers.h"

AVBoRBoxStand::AVBoRBoxStand()
{
	PrimaryActorTick.bCanEverTick = true;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("StandRoot")));
	Stand = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ApproximateStand"));
	Stand->SetupAttachment(RootComponent);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	Stand->SetStaticMesh(Cube.Object); Stand->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	BoxClass = AVBoRCarryObject::StaticClass();
}

void AVBoRBoxStand::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	StandSizeCm = StandSizeCm.GetAbs().ComponentMax(FVector(1));
	BoxSizeCm = BoxSizeCm.GetAbs().ComponentMax(FVector(1));
	Stand->SetRelativeLocation(FVector(0,0,StandTopHeightCm-StandSizeCm.Z*0.5));
	Stand->SetRelativeScale3D(StandSizeCm/100);
}

void AVBoRBoxStand::BeginPlay()
{
	Super::BeginPlay(); ResetStand();
	UE_LOG(LogTemp, Log, TEXT("Box stand — approximate geometry and values (P4d D1, cm)"));
}

FTransform AVBoRBoxStand::StartPose() const
{
	// 받침 접촉 오차를 피하려고 윗면에서 0.1 mm 띄움 (P1b D5·P4d D1)
	return FTransform(GetActorQuat(), GetActorTransform().TransformPosition(FVector(0,0,StandTopHeightCm+BoxSizeCm.Z*0.5+0.01)));
}

void AVBoRBoxStand::SpawnBox()
{
	if (!BoxClass || !GetWorld()) { return; }
	const FTransform Pose = StartPose();
	SourceBox = GetWorld()->SpawnActorDeferred<AVBoRCarryObject>(BoxClass, Pose, this, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!SourceBox) { return; }
	SourceBox->BoxSizeCm = BoxSizeCm; SourceBox->LoadTarget = LoadTarget;
	SourceBox->bUsePlacementAssist = false; SourceBox->bDestroyAtFloor = true;
	SourceBox->FloorHeightCm = GetActorLocation().Z;
	SourceBox->FinishSpawning(Pose); SourceBox->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);
	Boxes.Add(SourceBox); AddTickPrerequisiteActor(SourceBox);
	SourceBox->OnGrabbed.AddUniqueDynamic(this, &AVBoRBoxStand::HandleGrabbed);
	SourceBox->OnDestroyed.AddUniqueDynamic(this, &AVBoRBoxStand::HandleDestroyed);
	bSourceLifted = false;
}

void AVBoRBoxStand::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bSourceLifted || (IsValid(SourceBox) && SourceBox->GetCarrySession().bPaused)) { return; }
	const FTransform Pose = StartPose();
	const FBox Slot(-BoxSizeCm*0.5-FVector(1), BoxSizeCm*0.5+FVector(1));
	// 손에 든 박스·되돌린 박스가 출고 공간에서 1 cm 벗어나야 보충함 (P1b D5·P4d D1)
	for (const AVBoRCarryObject* Box : Boxes)
	{
		if (!IsValid(Box)) { continue; }
		const FBox Bounds = Box->GetCollisionBoundsInPose(Box->GetActorTransform(), Pose);
		if (Bounds.IsValid && Bounds.Intersect(Slot)) { return; }
	}
	SpawnBox();
}

void AVBoRBoxStand::HandleGrabbed(AVBoRCarryObject* Box) { if (Box == SourceBox) { bSourceLifted = true; } }

void AVBoRBoxStand::HandleDestroyed(AActor* Actor)
{
	AVBoRCarryObject* Box = Cast<AVBoRCarryObject>(Actor);
	RemoveTickPrerequisiteActor(Actor); Boxes.Remove(Box);
	if (Box == SourceBox) { SourceBox = nullptr; }
}

void AVBoRBoxStand::ClearBoxes()
{
	for (AVBoRCarryObject* Box : Boxes)
	{
		if (!IsValid(Box)) { continue; }
		Box->OnGrabbed.RemoveDynamic(this, &AVBoRBoxStand::HandleGrabbed);
		Box->OnDestroyed.RemoveDynamic(this, &AVBoRBoxStand::HandleDestroyed);
		RemoveTickPrerequisiteActor(Box);
		Box->ResetToPose(Box->GetActorTransform()); Box->Destroy();
	}
	Boxes.Reset(); SourceBox = nullptr; bSourceLifted = false;
}

void AVBoRBoxStand::ResetStand() { ClearBoxes(); SpawnBox(); }
void AVBoRBoxStand::EndPlay(const EEndPlayReason::Type Reason) { ClearBoxes(); Super::EndPlay(Reason); }
