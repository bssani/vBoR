#include "VBoRKeyTable.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "UObject/ConstructorHelpers.h"

AVBoRKeyTable::AVBoRKeyTable()
{
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("TableFloorOrigin")));
	TopMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ApproximateTop")); TopMesh->SetupAttachment(RootComponent);
	StandMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ApproximateStand")); StandMesh->SetupAttachment(RootComponent);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	TopMesh->SetStaticMesh(Cube.Object); StandMesh->SetStaticMesh(Cube.Object);
	TopMesh->SetCollisionProfileName(TEXT("BlockAllDynamic")); StandMesh->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	KeyClass = AVBoRKeyFob::StaticClass();
}

void AVBoRKeyTable::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	const FVector Size = TableSizeCm.ComponentMax(FVector(1));
	const double Thickness = FMath::Clamp(static_cast<double>(TopThicknessCm), 0.1, Size.Z);
	// 원점 Z 는 바닥 높이, 상판 윗면은 TableSizeCm.Z (근사 형상, P4a D6)
	TopMesh->SetRelativeLocation(FVector(0,0,Size.Z - Thickness / 2)); TopMesh->SetRelativeScale3D(FVector(Size.X,Size.Y,Thickness) / 100);
	StandMesh->SetRelativeLocation(FVector(0,0,(Size.Z - Thickness) / 2)); StandMesh->SetRelativeScale3D(FVector(6,6,Size.Z - Thickness) / 100);
}

FTransform AVBoRKeyTable::GetKeyHomeTransform(const AVBoRKeyFob* InKey) const
{
	const FBox TopBounds = TopMesh->GetStaticMesh()->GetBoundingBox().TransformBy(TopMesh->GetRelativeTransform());
	const FBox KeyBounds = InKey->KeyMesh->GetStaticMesh()->GetBoundingBox().TransformBy(FTransform(FQuat::Identity, FVector::ZeroVector, InKey->GetActorScale3D()));
	// 키 아랫면을 상판보다 0.1 cm 띄움 (simple collision 에 물리로 올라앉게, P4a D6)
	return FTransform(GetActorQuat(), GetActorTransform().TransformPosition(FVector(0,0,TopBounds.Max.Z - KeyBounds.Min.Z + 0.1)), InKey->GetActorScale3D());
}

void AVBoRKeyTable::BeginPlay()
{
	Super::BeginPlay();
	if (KeyClass)
	{
		FActorSpawnParameters Params; Params.Owner = this; Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Key = GetWorld()->SpawnActor<AVBoRKeyFob>(KeyClass, GetActorLocation(), GetActorRotation(), Params);
		if (Key) { Key->SetHome(this); }
	}
}

void AVBoRKeyTable::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (Key) { Key->Destroy(); }
	Super::EndPlay(EndPlayReason);
}
