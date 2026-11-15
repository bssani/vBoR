#include "VBoRObjectSpawnPoint.h"
#include "VBoRCarryObject.h"
#include "VBoRVehicleAlignmentComponent.h"
#include "Components/ArrowComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"

namespace
{
FVector SpawnBoxSizeCm(const AVBoRCarryObject& Defaults)
{
	return Defaults.BoxSizeCm.GetAbs().ComponentMax(FVector(1));
}
}

AVBoRObjectSpawnPoint::AVBoRObjectSpawnPoint()
{
	PrimaryActorTick.bCanEverTick = true;
	// Preserve serialized root transform across the class redirect (R10 D6)
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("StandRoot")));
	SpawnClass = AVBoRCarryObject::StaticClass();

#if WITH_EDITORONLY_DATA
	SpawnDirection = CreateEditorOnlyDefaultSubobject<UArrowComponent>(TEXT("SpawnDirection"));
	if (SpawnDirection)
	{
		SpawnDirection->SetupAttachment(RootComponent);
		SpawnDirection->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		SpawnDirection->SetHiddenInGame(true);
	}
	SpawnBounds = CreateEditorOnlyDefaultSubobject<UBoxComponent>(TEXT("SpawnBounds"));
	if (SpawnBounds)
	{
		SpawnBounds->SetupAttachment(RootComponent);
		SpawnBounds->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		SpawnBounds->SetHiddenInGame(true);
	}
#endif
}

void AVBoRObjectSpawnPoint::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
#if WITH_EDITORONLY_DATA
	if (!SpawnBounds) { return; }
	const auto* Defaults = Cast<AVBoRCarryObject>(SpawnClass.GetDefaultObject());
	SpawnBounds->SetVisibility(Defaults != nullptr);
	if (Defaults)
	{
		const FVector HalfSize = SpawnBoxSizeCm(*Defaults) * 0.5;
		SpawnBounds->SetBoxExtent(HalfSize, false);
		SpawnBounds->SetRelativeLocation(FVector(0, 0, HalfSize.Z));
	}
#endif
}

void AVBoRObjectSpawnPoint::BeginPlay()
{
	Super::BeginPlay();
	if (!SpawnClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("Object spawn point '%s': SpawnClass is empty; nothing will be spawned"), *GetName());
	}
	ResetSpawn();
}

float AVBoRObjectSpawnPoint::ResolveFloorHeightCm()
{
	int32 AlignmentCount = 0;
	float FloorHeightCm = 0;
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		const TInlineComponentArray<UVBoRVehicleAlignmentComponent*> Alignments(*It);
		for (const auto* Alignment : Alignments)
		{
			FloorHeightCm = Alignment->FloorWorldZCm;
			++AlignmentCount;
		}
	}
	if (AlignmentCount == 1) { return FloorHeightCm; }
	if (!bWarnedMissingFloor)
	{
		UE_LOG(LogTemp, Warning, TEXT("Object spawn point '%s': Floor height from alignment unavailable — using 0 cm"), *GetName());
		bWarnedMissingFloor = true;
	}
	return 0;
}

FTransform AVBoRObjectSpawnPoint::StartPose(const FVector& BoxSizeCm) const
{
	// Leave 0.1 mm clearance (contact rounding, P1b D5/P4d D1)
	const FVector Center = GetActorLocation() + GetActorUpVector() * (BoxSizeCm.Z * 0.5 + 0.01);
	return FTransform(GetActorQuat(), Center);
}

void AVBoRObjectSpawnPoint::SpawnObject()
{
	if (!SpawnClass || !GetWorld()) { return; }
	const auto* Defaults = Cast<AVBoRCarryObject>(SpawnClass.GetDefaultObject());
	if (!Defaults)
	{
		const FTransform Pose(GetActorQuat(), GetActorLocation());
		SpawnedActor = GetWorld()->SpawnActorDeferred<AActor>(SpawnClass, Pose, this, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!SpawnedActor) { return; }
		SpawnedActor->FinishSpawning(Pose);
		if (!IsValid(SpawnedActor)) { return; }
		const FBox Bounds = SpawnedActor->GetComponentsBoundingBox();
		if (Bounds.IsValid)
		{
			FVector BottomCenter = Bounds.GetCenter();
			BottomCenter.Z = Bounds.Min.Z;
			SpawnedActor->SetActorLocation(SpawnedActor->GetActorLocation() + GetActorLocation() - BottomCenter,
				false, nullptr, ETeleportType::TeleportPhysics);
		}
		return;
	}

	const FTransform Pose = StartPose(SpawnBoxSizeCm(*Defaults));
	SourceBox = GetWorld()->SpawnActorDeferred<AVBoRCarryObject>(SpawnClass, Pose, this, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!SourceBox) { return; }
	SourceBox->LoadTarget = LoadTarget;
	SourceBox->bUsePlacementAssist = false;
	SourceBox->bDestroyAtFloor = true;
	SourceBox->FloorHeightCm = ResolveFloorHeightCm();
	SourceBox->FinishSpawning(Pose);
	SourceBox->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);
	Boxes.Add(SourceBox);
	AddTickPrerequisiteActor(SourceBox);
	SourceBox->OnGrabbed.AddUniqueDynamic(this, &AVBoRObjectSpawnPoint::HandleGrabbed);
	SourceBox->OnDestroyed.AddUniqueDynamic(this, &AVBoRObjectSpawnPoint::HandleDestroyed);
	bSourceLifted = false;
}

void AVBoRObjectSpawnPoint::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bSourceLifted) { return; }
	const auto* Defaults = Cast<AVBoRCarryObject>(SpawnClass.GetDefaultObject());
	if (!Defaults) { return; }
	const FVector BoxSizeCm = SpawnBoxSizeCm(*Defaults);
	const FTransform Pose = StartPose(BoxSizeCm);
	const FBox Slot(-BoxSizeCm * 0.5 - FVector(1), BoxSizeCm * 0.5 + FVector(1));
	// Wait for all hands, including paused sessions (P5zf D1)
	for (const AVBoRCarryObject* Box : Boxes)
	{
		if (!IsValid(Box)) { continue; }
		if (Box->GetCarrySession().Hand != INDEX_NONE) { return; }
		const FBox Bounds = Box->GetCollisionBoundsInPose(Box->GetActorTransform(), Pose);
		if (Bounds.IsValid && Bounds.Intersect(Slot)) { return; }
	}
	SpawnObject();
}

void AVBoRObjectSpawnPoint::HandleGrabbed(AVBoRCarryObject* Box)
{
	if (Box == SourceBox) { bSourceLifted = true; }
}

void AVBoRObjectSpawnPoint::HandleDestroyed(AActor* Actor)
{
	AVBoRCarryObject* Box = Cast<AVBoRCarryObject>(Actor);
	RemoveTickPrerequisiteActor(Actor);
	Boxes.Remove(Box);
	if (Box == SourceBox) { SourceBox = nullptr; }
}

void AVBoRObjectSpawnPoint::ClearSpawnedObjects()
{
	if (IsValid(SpawnedActor)) { SpawnedActor->Destroy(); }
	SpawnedActor = nullptr;
	for (AVBoRCarryObject* Box : Boxes)
	{
		if (!IsValid(Box)) { continue; }
		Box->OnGrabbed.RemoveDynamic(this, &AVBoRObjectSpawnPoint::HandleGrabbed);
		Box->OnDestroyed.RemoveDynamic(this, &AVBoRObjectSpawnPoint::HandleDestroyed);
		RemoveTickPrerequisiteActor(Box);
		Box->ResetToPose(Box->GetActorTransform());
		Box->Destroy();
	}
	Boxes.Reset();
	SourceBox = nullptr;
	bSourceLifted = false;
}

void AVBoRObjectSpawnPoint::ResetSpawn()
{
	ClearSpawnedObjects();
	SpawnObject();
}

void AVBoRObjectSpawnPoint::EndPlay(const EEndPlayReason::Type Reason)
{
	ClearSpawnedObjects();
	Super::EndPlay(Reason);
}
