#pragma once

#include "CoreMinimal.h"
#include "../Interaction/VBoRPokeButton.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"
#include "P5zcTestButton.generated.h"

// Distinct defaults expose parent overrides (P5zc E1-E5)
UCLASS()
class AP5zcTestButton : public AVBoRPokeButton
{
	GENERATED_BODY()
public:
	AP5zcTestButton()
	{
		static ConstructorHelpers::FObjectFinder<UStaticMesh> Mesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
		ButtonMesh->SetStaticMesh(Mesh.Object);
		ButtonMesh->SetRelativeScale3D(FVector(0.07, 0.05, 0.02));
		PressDepthMm = 4;
		HitAreaScale = 1.5f;
		SpectatorName = FText::FromString(TEXT("Test"));
	}
};

UCLASS(Abstract)
class AP5zcAbstractButton : public AVBoRPokeButton
{
	GENERATED_BODY()
};
