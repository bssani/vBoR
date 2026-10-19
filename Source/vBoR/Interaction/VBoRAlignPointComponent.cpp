#include "VBoRAlignPointComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

UVBoRAlignPointComponent::UVBoRAlignPointComponent()
{
	Sphere = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PointSphere")); Sphere->SetupAttachment(this);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Mesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	Sphere->SetStaticMesh(Mesh.Object); Sphere->SetRelativeScale3D(FVector(0.025));
	Sphere->SetCollisionEnabled(ECollisionEnabled::NoCollision); Sphere->SetGenerateOverlapEvents(false);
	Sphere->SetCastShadow(false);
	Number = CreateDefaultSubobject<UTextRenderComponent>(TEXT("PointNumber")); Number->SetupAttachment(this);
	Number->SetRelativeLocation(FVector(0,0,3)); Number->SetWorldSize(3); Number->SetHorizontalAlignment(EHTA_Center);
	Number->SetCollisionEnabled(ECollisionEnabled::NoCollision); Number->SetGenerateOverlapEvents(false);
	Number->SetTextRenderColor(FColor::Yellow);
	SetVisibility(false,true); SetHiddenInGame(true,true);
}

void UVBoRAlignPointComponent::OnRegister()
{
	Super::OnRegister();
	if (!Sphere->IsRegistered()) { Sphere->RegisterComponent(); }
	if (!Number->IsRegistered()) { Number->RegisterComponent(); }
	Number->SetText(FText::AsNumber(Order));
}

void UVBoRAlignPointComponent::ShowPoint(bool bShow)
{
	Number->SetText(FText::AsNumber(Order));
	SetVisibility(bShow,true); SetHiddenInGame(!bShow,true);
}
