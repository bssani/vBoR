#include "VBoREyePointComponent.h"
#include "Components/ArrowComponent.h"

UVBoREyePointComponent::UVBoREyePointComponent()
{
	Forward = CreateDefaultSubobject<UArrowComponent>(TEXT("EyeForward"));
	Forward->SetupAttachment(this);
	Forward->SetArrowColor(FLinearColor::Yellow);
	Forward->SetArrowLength(12);
	Forward->SetArrowSize(0.5f);
	Forward->SetHiddenInGame(false);
	Forward->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}
