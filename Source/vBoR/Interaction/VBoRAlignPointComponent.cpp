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
	// 에디터에서는 보임 (CAD 메시 자리에 붙이게). 게임에서만 숨김 (P4h D1·D8, 리뷰 R1-2)
	// 생성자에서는 AttachChildren 이 비어 전파가 안 됨. 자식도 직접 숨김 (참가자 흐름에 표시가 보이지 않게, 리뷰 R1-3)
	SetHiddenInGame(true); Sphere->SetHiddenInGame(true); Number->SetHiddenInGame(true);
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
	SetHiddenInGame(!bShow,true);
}
