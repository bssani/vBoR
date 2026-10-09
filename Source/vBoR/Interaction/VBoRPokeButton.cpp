#include "VBoRPokeButton.h"
#include "VBoRPokeButtonLayout.h"

#include "Audio/IsdkPointerEventAudioPlayer.h"
#include "Components/AudioComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Interaction/IsdkClippedPlaneSurface.h"
#include "Interaction/IsdkPokeButtonVisual.h"
#include "Interaction/IsdkPokeInteractable.h"
#include "Interaction/Surfaces/IsdkPointablePlane.h"
#include "IsdkContentAssetPaths.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Sound/SoundBase.h"
#include "UObject/ConstructorHelpers.h"

AVBoRPokeButton::AVBoRPokeButton()
{
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("FixedRoot")));
	BaseMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BaseMesh"));
	BaseMesh->SetupAttachment(RootComponent);
	BaseMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Poke = CreateDefaultSubobject<UIsdkPokeInteractable>(TEXT("Poke"));
	Poke->SetupAttachment(RootComponent);
	// ISDK +X 를 Actor -Z 로 맞춤 (윗면 +Z 에서 누름, P0-b D4)
	Poke->SetRelativeRotation(FRotator(-90, 0, 0));
	Plane = CreateDefaultSubobject<UIsdkPointablePlane>(TEXT("PressedPlane"));
	Plane->SetupAttachment(Poke);
	Surface = CreateDefaultSubobject<UIsdkClippedPlaneSurface>(TEXT("ClippedSurface"));
	ButtonVisual = CreateDefaultSubobject<UIsdkPokeButtonVisual>(TEXT("ButtonVisual"));
	ButtonVisual->SetupAttachment(RootComponent);
	ButtonMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ButtonMesh"));
	ButtonMesh->SetupAttachment(ButtonVisual);
	ButtonMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(IsdkContentAssetPaths::Engine::BasicShapes::Cube);
	ButtonMesh->SetStaticMesh(Cube.Object);
	// 15 mm 시험용 자리값. 실제 크기는 자식의 메시·스케일에서 나옴 (P0-b D2)
	ButtonMesh->SetRelativeScale3D(FVector(0.015, 0.015, 0.006));
	PressAudio = CreateDefaultSubobject<UAudioComponent>(TEXT("PressAudio"));
	PressAudio->SetupAttachment(RootComponent);
	PressAudio->bAutoActivate = false;
	static ConstructorHelpers::FObjectFinder<USoundBase> PressSound(IsdkContentAssetPaths::Audio::Interaction_BasicPoke_ButtonPress);
	PressAudio->SetSound(PressSound.Object);
	PointerAudio = CreateDefaultSubobject<UIsdkPointerEventAudioPlayer>(TEXT("PointerAudio"));
}

void AVBoRPokeButton::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	if (!ButtonMesh->GetStaticMesh()) { return; }
	// 메시의 쉬는 위치를 보존하면서 visual pivot 만 윗면으로 옮김 (P0-b D3·D4)
	const FTransform MeshToActor = ButtonMesh->GetComponentTransform().GetRelativeTransform(GetActorTransform());
	const auto Layout = FVBoRPokeButtonLayout::Calculate(ButtonMesh->GetStaticMesh()->GetBoundingBox(), MeshToActor, PressDepthMm, HitAreaScale);
	RestFront = Layout.FrontCenter;
	ButtonVisual->SetRelativeLocation(RestFront);
	ButtonMesh->SetRelativeTransform(MeshToActor.GetRelativeTransform(ButtonVisual->GetRelativeTransform()));
	Poke->SetRelativeLocation(Layout.PlaneCenter);
	Plane->SetSize(FVector2D(Layout.PlaneSize.Y, Layout.PlaneSize.X) / 2.0);
	if (Surface->GetPointablePlane() != Plane.Get()) { Surface->SetPointablePlane(Plane); }
	FIsdkBoundsClipper Clipper;
	Clipper.PoseProvider = Plane.Get();
	Clipper.Size = FVector3f(FMath::Max(1.0f, PressDepthMm / 10.0f), Layout.PlaneSize.Y, Layout.PlaneSize.X);
	Surface->SetBoundsClippers({Clipper});
	Poke->SetSurfacePatch(Surface.Get());
	FIsdkPokeInteractableConfigOffsets Offsets;
	Offsets.NormalOffset = PressDepthMm / 10.0f;
	Offsets.TangentOffset = 0.0f;
	Poke->SetConfigOffsets(Offsets);
}

void AVBoRPokeButton::PreInitializeComponents()
{
	Super::PreInitializeComponents();
	// setter 는 WorldSubsystem 을 읽으므로 CDO 에서 호출하지 않음 (P0-b §3)
	ButtonVisual->SetPokeInteractable(Poke);
	PointerAudio->SetPointable(Poke.Get());
	PointerAudio->SetSelectAudio(PressAudio);
}

void AVBoRPokeButton::BeginPlay()
{
	Super::BeginPlay();
	// 메시의 모든 slot 에 상태 색을 표시함 (촉감 대신 활성 상태 표시, U-04)
	UMaterialInterface* ColorMaterial = LoadObject<UMaterialInterface>(nullptr, IsdkContentAssetPaths::Materials::GrabbableDefault);
	for (int32 Slot = 0; Slot < ButtonMesh->GetNumMaterials(); ++Slot)
	{
		UMaterialInstanceDynamic* Material = ButtonMesh->CreateDynamicMaterialInstance(Slot, ColorMaterial);
		if (Material)
		{
			Material->SetVectorParameterValue(TEXT("ColorParam"), NormalColor);
			ColorMaterials.Add(Material);
		}
	}
	Poke->GetInteractableStateChangedDelegate()->AddUniqueDynamic(this, &AVBoRPokeButton::HandleState);
	Poke->GetInteractionPointerEventDelegate().AddUniqueDynamic(this, &AVBoRPokeButton::HandlePointer);
}

void AVBoRPokeButton::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Poke->GetInteractableStateChangedDelegate()->RemoveDynamic(this, &AVBoRPokeButton::HandleState);
	Poke->GetInteractionPointerEventDelegate().RemoveDynamic(this, &AVBoRPokeButton::HandlePointer);
	Super::EndPlay(EndPlayReason);
}

FVector AVBoRPokeButton::GetRestFrontWorld() const
{
	return GetActorTransform().TransformPosition(RestFront);
}

void AVBoRPokeButton::HandleState(const FIsdkInteractableStateEvent& Event)
{
	const FLinearColor Color = Event.Args.NewState == EIsdkInteractableState::Select ? SelectColor
		: Event.Args.NewState == EIsdkInteractableState::Hover ? HoverColor : NormalColor;
	for (UMaterialInstanceDynamic* Material : ColorMaterials) { Material->SetVectorParameterValue(TEXT("ColorParam"), Color); }
}

void AVBoRPokeButton::HandlePointer(const FIsdkInteractionPointerEvent& Event)
{
	if (Event.Type == EIsdkPointerEventType::Select) { OnButtonPressed.Broadcast(Event); }
	else if (Event.Type == EIsdkPointerEventType::Unselect) { OnButtonReleased.Broadcast(Event); }
}
