#include "VBoRObstructionBoxComponent.h"

UVBoRObstructionBoxComponent::UVBoRObstructionBoxComponent()
{
	SetHiddenInGame(true);
	SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetCollisionResponseToAllChannels(ECR_Ignore);
	SetGenerateOverlapEvents(false);
	SetCanEverAffectNavigation(false);
	ShapeColor = FColor::Orange;
}
