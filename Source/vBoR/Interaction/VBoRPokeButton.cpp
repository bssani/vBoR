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
#include "Subsystem/IsdkWorldSubsystem.h"
#include "UObject/ConstructorHelpers.h"

AVBoRPokeButton::AVBoRPokeButton()
{
	PrimaryActorTick.bCanEverTick = true; PrimaryActorTick.bStartWithTickEnabled = false;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("FixedRoot")));
	BaseMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BaseMesh"));
	BaseMesh->SetupAttachment(RootComponent);
	BaseMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Poke = CreateDefaultSubobject<UIsdkPokeInteractable>(TEXT("Poke"));
	Poke->SetupAttachment(RootComponent);
	// Align ISDK +X with Actor -Z (press from top +Z, P0-b D4)
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
	// 15 mm test placeholder. Actual size from child mesh/scale (P0-b D2)
	ButtonMesh->SetRelativeScale3D(FVector(0.015, 0.015, 0.006));
	PressAudio = CreateDefaultSubobject<UAudioComponent>(TEXT("PressAudio"));
	PressAudio->SetupAttachment(RootComponent);
	PressAudio->bAutoActivate = false;
	static ConstructorHelpers::FObjectFinder<USoundBase> PressSound(IsdkContentAssetPaths::Audio::Interaction_BasicPoke_ButtonPress);
	PressAudio->SetSound(PressSound.Object);
	SoundAudio.SetNum(2); SoundAudio[0] = PressAudio;
	PressAudio->SetupAttachment(ButtonVisual);
	SoundAudio[1] = CreateDefaultSubobject<UAudioComponent>(TEXT("ReleasedCueAudio")); SoundAudio[1]->SetupAttachment(ButtonVisual); SoundAudio[1]->bAutoActivate = false;
	PressedCue.Sound = PressAudio->Sound;

	PointerAudio = CreateDefaultSubobject<UIsdkPointerEventAudioPlayer>(TEXT("PointerAudio"));
}

void AVBoRPokeButton::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	if (!ButtonMesh->GetStaticMesh()) { return; }
	// Move visual pivot to top, preserve mesh rest position (P0-b D3/D4)
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
	// No subsystem in thumbnail/BP preview worlds (P4i D7/Q2)
	if (!UIsdkWorldSubsystem::TryGet(GetWorld())) { return; }
	ButtonVisual->SetPokeInteractable(Poke);
	PointerAudio->SetPointable(Poke.Get());
	PointerAudio->SetSelectAudio(nullptr);
}

void AVBoRPokeButton::BeginPlay()
{
	Super::BeginPlay();
	// Overlay active state, preserve original label/texture (P0-b R1-1, U-04)
	UMaterialInterface* ColorMaterial = LoadObject<UMaterialInterface>(nullptr, IsdkContentAssetPaths::Materials::GrabbableDefault);
	StateOverlay = ColorMaterial ? UMaterialInstanceDynamic::Create(ColorMaterial, this) : nullptr;
	ButtonMesh->SetOverlayMaterial(nullptr);
	Poke->GetInteractableStateChangedDelegate()->AddUniqueDynamic(this, &AVBoRPokeButton::HandleState);
	Poke->GetInteractionPointerEventDelegate().AddUniqueDynamic(this, &AVBoRPokeButton::HandlePointer);
}

void AVBoRPokeButton::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	for (int32 I = 0; I < 2; ++I) { SoundPlayback[I].Reset(SoundAudio[I], false); }
	Poke->GetInteractableStateChangedDelegate()->RemoveDynamic(this, &AVBoRPokeButton::HandleState);
	Poke->GetInteractionPointerEventDelegate().RemoveDynamic(this, &AVBoRPokeButton::HandlePointer);
	ButtonMesh->SetOverlayMaterial(nullptr);
	Super::EndPlay(EndPlayReason);
}

FVector AVBoRPokeButton::GetRestFrontWorld() const
{
	return GetActorTransform().TransformPosition(RestFront);
}

void AVBoRPokeButton::HandleState(const FIsdkInteractableStateEvent& Event)
{
	const bool bSelected = Event.Args.NewState == EIsdkInteractableState::Select;
	const bool bActive = bSelected || Event.Args.NewState == EIsdkInteractableState::Hover;
	if (!bActive || !StateOverlay) { ButtonMesh->SetOverlayMaterial(nullptr); return; }
	FLinearColor Color = bSelected ? SelectColor : HoverColor;
	// GrabbableLit ColorParam alpha feeds Opacity (asset inspection test, P0-b R1-1)
	Color.A = FMath::Clamp(bSelected ? SelectOpacity : HoverOpacity, 0.0f, 1.0f);
	StateOverlay->SetVectorParameterValue(TEXT("ColorParam"), Color);
	ButtonMesh->SetOverlayMaterial(StateOverlay);
}

void AVBoRPokeButton::HandlePointer(const FIsdkInteractionPointerEvent& Event)
{
	// Keep Select feedback on filter rejection (including key-holding hand, P5s D4)
	if (Event.Type == EIsdkPointerEventType::Select) { SoundPlayback[0].Queue(PressedCue, SoundAudio[0], SoundFrame); }
	else if (Event.Type == EIsdkPointerEventType::Unselect) { SoundPlayback[1].Queue(ReleasedCue, SoundAudio[1], SoundFrame); }
	SetActorTickEnabled(SoundPlayback[0].HasPending() || SoundPlayback[1].HasPending());
	if (PressFilter.IsBound() && !PressFilter.Execute(Event)) { return; }
	if (Event.Type == EIsdkPointerEventType::Select) { OnButtonPressed.Broadcast(Event); }
	else if (Event.Type == EIsdkPointerEventType::Unselect) { OnButtonReleased.Broadcast(Event); }
}

bool AVBoRPokeButton::IsSpectatorInteracting() const
{
	return Poke && Poke->GetInteractableState() == EIsdkInteractableState::Select;
}

void AVBoRPokeButton::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds); ++SoundFrame;
	SoundPlayback[0].Advance(PressedCue, SoundAudio[0], DeltaSeconds, SoundFrame);
	SoundPlayback[1].Advance(ReleasedCue, SoundAudio[1], DeltaSeconds, SoundFrame);
	SetActorTickEnabled(SoundPlayback[0].HasPending() || SoundPlayback[1].HasPending());
}
