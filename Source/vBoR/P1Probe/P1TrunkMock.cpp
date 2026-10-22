#include "P1TrunkMock.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	void SetPanel(UStaticMeshComponent* Panel, const FVector& Center, const FVector& Size)
	{
		Panel->SetRelativeLocation(Center);
		Panel->SetRelativeScale3D(Size / 100.0f); // Engine Cube side = 100 cm (P1 §2)
	}
}

AP1TrunkMock::AP1TrunkMock()
{
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("OpeningOrigin")));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	auto AddPanel = [this](const TCHAR* Name) -> UStaticMeshComponent*
	{
		UStaticMeshComponent* Panel = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		Panel->SetupAttachment(RootComponent);
		Panel->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		Panel->SetCollisionResponseToAllChannels(ECR_Block);
		Panel->SetMobility(EComponentMobility::Movable);
		return Panel;
	};
	Floor = AddPanel(TEXT("Floor"));
	LeftWall = AddPanel(TEXT("LeftWall"));
	RightWall = AddPanel(TEXT("RightWall"));
	FrontWall = AddPanel(TEXT("FrontWall"));
	Ceiling = AddPanel(TEXT("Ceiling"));
	BumperLip = AddPanel(TEXT("BumperLip"));
	for (UStaticMeshComponent* Panel : {Floor.Get(), LeftWall.Get(), RightWall.Get(), FrontWall.Get(), Ceiling.Get(), BumperLip.Get()})
	{
		if (Cube.Succeeded()) { Panel->SetStaticMesh(Cube.Object); }
	}
	ApproximateLabel = CreateDefaultSubobject<UTextRenderComponent>(TEXT("ApproximateLabel"));
	ApproximateLabel->SetupAttachment(RootComponent);
	ApproximateLabel->SetText(FText::FromString(TEXT("Approximate geometry - not vehicle data")));
	ApproximateLabel->SetTextRenderColor(FColor::Yellow);
	ApproximateLabel->SetWorldSize(3.0f);
}

void AP1TrunkMock::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	const float W = FMath::Max(1.0f, InsideWidthCm);
	const float D = FMath::Max(1.0f, InsideDepthCm);
	const float H = FMath::Max(1.0f, InsideHeightCm);
	const float T = FMath::Max(0.1f, PanelThicknessCm);
	const float Lip = FMath::Max(0.0f, LipHeightCm);
	SetPanel(Floor.Get(), FVector(D * 0.5f, 0, FloorHeightCm - T * 0.5f), FVector(D + T, W + 2 * T, T));
	SetPanel(LeftWall.Get(), FVector(D * 0.5f, -W * 0.5f - T * 0.5f, FloorHeightCm + H * 0.5f), FVector(D + T, T, H));
	SetPanel(RightWall.Get(), FVector(D * 0.5f, W * 0.5f + T * 0.5f, FloorHeightCm + H * 0.5f), FVector(D + T, T, H));
	SetPanel(FrontWall.Get(), FVector(D + T * 0.5f, 0, FloorHeightCm + H * 0.5f), FVector(T, W + 2 * T, H));
	SetPanel(Ceiling.Get(), FVector(D * 0.5f, 0, FloorHeightCm + H + T * 0.5f), FVector(D + T, W + 2 * T, T));
	SetPanel(BumperLip.Get(), FVector(-T * 0.5f, 0, FloorHeightCm + Lip * 0.5f), FVector(T, W + 2 * T, FMath::Max(0.1f, Lip)));
	BumperLip->SetVisibility(Lip > KINDA_SMALL_NUMBER);
	BumperLip->SetCollisionEnabled(Lip > KINDA_SMALL_NUMBER ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
	ApproximateLabel->SetRelativeLocation(FVector(-10, -W * 0.5f, FloorHeightCm + H + 12));
	ApproximateLabel->SetRelativeRotation(FRotator(0, 180, 0));
}
