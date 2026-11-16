#include "VBoREyePointComponent.h"
#include "Components/ArrowComponent.h"

UVBoREyePointComponent::UVBoREyePointComponent()
{
	Forward = CreateDefaultSubobject<UArrowComponent>(TEXT("EyeForward"));
	Forward->SetupAttachment(this);
	Forward->SetArrowColor(FLinearColor::Yellow);
	Forward->SetArrowLength(12);
	Forward->SetArrowSize(0.5f);
	Forward->SetHiddenInGame(true);
	Forward->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void UVBoREyePointComponent::OnRegister()
{
	Super::OnRegister();
	if (Forward) { Forward->SetHiddenInGame(!bShowInGame); }
}
