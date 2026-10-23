#include "VBoRPanelAnchor.h"
#include "VBoRScenarioRunner.h"
#include "Components/ArrowComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

AVBoRPanelAnchor::AVBoRPanelAnchor()
{
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("PanelOrigin")));
	auto* Arrow = CreateDefaultSubobject<UArrowComponent>(TEXT("ParticipantDirection")); Arrow->SetupAttachment(RootComponent);
	Arrow->ArrowColor = FColor::Yellow; Arrow->SetHiddenInGame(true); Arrow->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	const auto* Defaults = GetDefault<AVBoRScenarioRunner>();
	for (int32 I = 0; I < 3; ++I)
	{
		auto* Preview = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("Preview%d"),I));
		Preview->SetupAttachment(RootComponent); Preview->SetStaticMesh(Cube.Object);
		Preview->SetCollisionEnabled(ECollisionEnabled::NoCollision); Preview->SetGenerateOverlapEvents(false);
		Preview->SetHiddenInGame(true); Preview->SetCastShadow(false);
		// Editor size reference only; +X faces participant (P5z D2/D4, cm)
		Preview->SetRelativeScale3D(I == 0 ? FVector(0.005,Defaults->PanelSizeCm.X/100,Defaults->PanelSizeCm.Y/100) : FVector(0.015,0.08,0.08));
		if (I > 0) { Preview->SetRelativeLocation(FVector(0,I == 1 ? -12 : 12,-Defaults->PanelSizeCm.Y/2-Defaults->PanelButtonGapCm)); }
	}
}
