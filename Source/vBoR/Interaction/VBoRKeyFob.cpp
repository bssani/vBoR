#include "VBoRKeyFob.h"
#include "Sound/SoundBase.h"
#include "VBoRKeyTable.h"
#include "VBoRPokeButton.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Interaction/Grabbable/IsdkGrabFreeTransformer.h"
#include "Interaction/IsdkPokeInteractable.h"
#include "Interaction/IsdkThrowable.h"
#include "IsdkFunctionLibrary.h"
#include "IsdkContentAssetPaths.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "OculusXRInputFunctionLibrary.h"
#include "UObject/ConstructorHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogKeyFob, Log, All);

UVBoRKeyGrabComponent::UVBoRKeyGrabComponent() { Configure(); }
void UVBoRKeyGrabComponent::Configure()
{
	bIsThrowable = true; bRetainGravityWhenThrown = true;
	MultiGrabBehavior = EIsdkMultiGrabBehavior::SingleGrabTransferToSecond;
}

void UVBoRKeyGrabbableComponent::PostEvent(const FIsdkInteractionPointerEvent& Event)
{
	AVBoRKeyFob* Key = Cast<AVBoRKeyFob>(GetOwner());
	if (Key) { Key->ObserveTracking(); }
	Super::PostEvent(Event);
	if (Key) { Key->AfterPointer(Event); }
}

AVBoRKeyFob::AVBoRKeyFob()
{
	static ConstructorHelpers::FObjectFinder<USoundBase> ButtonSound(IsdkContentAssetPaths::Audio::Interaction_BasicPoke_ButtonPress);
	ButtonPressedCue.Sound = ButtonSound.Object;
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PrePhysics;
	KeyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ApproximateKey")); SetRootComponent(KeyMesh);
	KeyMesh->SetMobility(EComponentMobility::Movable);
	KeyMesh->SetCollisionProfileName(TEXT("PhysicsActor"));
	KeyMesh->SetGenerateOverlapEvents(true);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(IsdkContentAssetPaths::Engine::BasicShapes::Cube);
	KeyMesh->SetStaticMesh(Cube.Object);
	Grabbable = CreateDefaultSubobject<UVBoRKeyGrabbableComponent>(TEXT("Grabbable")); Grabbable->SetupAttachment(KeyMesh);
	GrabComponent = CreateDefaultSubobject<UVBoRKeyGrabComponent>(TEXT("GrabComponent"));
	FreeTransformer = CreateDefaultSubobject<UIsdkGrabFreeTransformer>(TEXT("KeyFreeTransformer"));
}

void AVBoRKeyFob::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	if (KeyMesh->GetStaticMesh())
	{
		// Size from child mesh bounds in cm (approximate geometry, P4a §2)
		const FVector BoundsSize = KeyMesh->GetStaticMesh()->GetBoundingBox().GetSize().ComponentMax(FVector(0.01));
		KeyMesh->SetWorldScale3D(KeySizeCm.ComponentMax(FVector(0.1)) / BoundsSize);
	}
}

void AVBoRKeyFob::PreInitializeComponents()
{
	Grabbable->SetGrabInputMethodAllowed(EIsdkGrabInputMethod::Palm, true);
	Grabbable->SetGrabInputMethodAllowed(EIsdkGrabInputMethod::Pinch, true);
	Grabbable->SetGrabCollider(KeyMesh);
	GrabComponent->Configure();
	// SDK scale clamp applies X to all axes. Single grab preserves nonuniform scale (P4a D4)
	FreeTransformer->Config.ScaleConstraint.bUseConstraint = false;
	GrabComponent->SetSingleGrabTransformerObject(FreeTransformer);
	GrabComponent->SetTransformTarget(KeyMesh);
	Grabbable->SetGrabTransformer(GrabComponent);
	KeyMesh->SetSimulatePhysics(true); KeyMesh->SetEnableGravity(true);
	Super::PreInitializeComponents();
}

AVBoRPokeButton* AVBoRKeyFob::CreateButton(FVector OffsetCm, FLinearColor Color, TSubclassOf<AVBoRPokeButton> ButtonClass, const TCHAR* Slot)
{
	if (ButtonClass && ButtonClass->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists))
	{
		UE_LOG(LogKeyFob, Warning, TEXT("%s %s: unusable button class %s; using default cube button"), *GetName(), Slot, *ButtonClass->GetName());
		ButtonClass = nullptr;
	}
	// Attach at world size (root mesh scale must not alter button cm size/spacing, P4a D3)
	const FTransform Frame(GetActorQuat(), GetActorLocation());
	const FTransform ButtonWorld(GetActorQuat(), Frame.TransformPosition(OffsetCm));
	AVBoRPokeButton* Button = GetWorld()->SpawnActorDeferred<AVBoRPokeButton>(ButtonClass ? ButtonClass.Get() : AVBoRPokeButton::StaticClass(), ButtonWorld, this);
	if (!ButtonClass)
	{
		Button->ButtonMesh->SetRelativeScale3D(ButtonSizeCm.ComponentMax(FVector(0.1)) / 100);
		Button->PressDepthMm = 1;
	}
	Button->PressFilter.BindUObject(this, &AVBoRKeyFob::CanPress);
	Button->PressedCue = ButtonPressedCue; Button->ReleasedCue = ButtonReleasedCue;
	Button->FinishSpawning(ButtonWorld);
	Button->SpectatorName = FText::GetEmpty();
	Button->Poke->Deactivate();
	Button->AttachToComponent(KeyMesh, FAttachmentTransformRules::KeepWorldTransform);
	UMaterialInterface* Material = !ButtonClass ? LoadObject<UMaterialInterface>(nullptr, IsdkContentAssetPaths::Materials::GrabbableDefault) : nullptr;
	if (Material)
	{
		UMaterialInstanceDynamic* ColorMaterial = UMaterialInstanceDynamic::Create(Material, Button);
		ColorMaterial->SetVectorParameterValue(TEXT("ColorParam"), Color); Button->ButtonMesh->SetMaterial(0, ColorMaterial);
	}
	return Button;
}

void AVBoRKeyFob::BeginPlay()
{
	Super::BeginPlay();
	UnlockButton = CreateButton(UnlockButtonCm, FLinearColor(0.1,0.7,0.2,1), UnlockButtonClass, TEXT("UnlockButtonClass"));
	LockButton = CreateButton(LockButtonCm, FLinearColor(0.8,0.15,0.1,1), LockButtonClass, TEXT("LockButtonClass"));
	UnlockButton->OnButtonPressed.AddDynamic(this, &AVBoRKeyFob::UnlockPressed);
	LockButton->OnButtonPressed.AddDynamic(this, &AVBoRKeyFob::LockPressed);
	UE_LOG(LogKeyFob, Log, TEXT("%s approximate key size=%s cm"), *GetName(), *KeySizeCm.ToCompactString());
}

bool AVBoRKeyFob::IsHandTracked() const
{
	if (HoldingHand == INDEX_NONE) { return false; }
	const EOculusXRHandType Hand = HoldingHand == 0 ? EOculusXRHandType::HandLeft : EOculusXRHandType::HandRight;
	return UOculusXRInputFunctionLibrary::IsHandPositionValid(Hand) &&
		UOculusXRInputFunctionLibrary::GetTrackingConfidence(Hand) == EOculusXRTrackingConfidence::High;
}

void AVBoRKeyFob::ObserveTracking()
{
	if (HoldingHand != INDEX_NONE && !IsHandTracked()) { bLowSinceLastTick = true; }
}

void AVBoRKeyFob::RecordTrackingFrame()
{
	FIsdkThrowableSettings Settings;
	if (const UIsdkThrowable* Throwable = FindComponentByClass<UIsdkThrowable>()) { Throwable->GetThrowableSettings(Settings); }
	RecentHighFrames.Add(IsHandTracked() && !bLowSinceLastTick);
	const int32 SampleSize = FMath::Clamp(Settings.SampleSize, 1, 100);
	if (RecentHighFrames.Num() > SampleSize) { RecentHighFrames.RemoveAt(0, RecentHighFrames.Num() - SampleSize, EAllowShrinking::No); }
	bLowSinceLastTick = false;
}

void AVBoRKeyFob::AfterPointer(const FIsdkInteractionPointerEvent& Event)
{
	if (Event.Type == EIsdkPointerEventType::Select)
	{
		EIsdkHandedness Hand;
		if (!UIsdkFunctionLibrary::GetHandednessFromInteractor(Event.Interactor, Hand)) { return; }
		HoldingHand = Hand == EIsdkHandedness::Left ? 0 : 1; HoldingInteractor = Event.Interactor;
		RecentHighFrames.Reset(); bLowSinceLastTick = false; RecordTrackingFrame();
		UE_LOG(LogKeyFob, Log, TEXT("%s grab hand=%d"), *GetName(), HoldingHand);
	}
	else if ((Event.Type == EIsdkPointerEventType::Unselect || Event.Type == EIsdkPointerEventType::Cancel) &&
		HoldingInteractor == Event.Interactor && GrabComponent->GetNumGrabbers() == 0)
	{
		const TCHAR* Reason = !IsHandTracked() ? TEXT("tracking lost or Low") :
			(bLowSinceLastTick || RecentHighFrames.Contains(false)) ? TEXT("non-High frame in throw sample window") : TEXT("High samples");
		if (!IsHandTracked() || bLowSinceLastTick || RecentHighFrames.Contains(false)) { ClearVelocity(); }
		UE_LOG(LogKeyFob, Log, TEXT("%s release hand=%d velocity=%s cm/s | %s"), *GetName(), HoldingHand,
			*KeyMesh->GetPhysicsLinearVelocity().ToCompactString(), Reason);
		HoldingHand = INDEX_NONE; HoldingInteractor.Reset(); RecentHighFrames.Reset(); bLowSinceLastTick = false;
	}
}

bool AVBoRKeyFob::CanPress(const FIsdkInteractionPointerEvent& Event) const
{
	EIsdkHandedness Hand;
	return HoldingHand != INDEX_NONE && UIsdkFunctionLibrary::GetHandednessFromInteractor(Event.Interactor, Hand) &&
		(Hand == EIsdkHandedness::Left ? 0 : 1) != HoldingHand;
}

void AVBoRKeyFob::UnlockPressed(const FIsdkInteractionPointerEvent& Event)
{
	if (!CanPress(Event)) { return; }
	UE_LOG(LogKeyFob, Log, TEXT("%s Unlock pressed hand=%d"), *GetName(), 1 - HoldingHand);
	OnUnlockPressed.Broadcast(this);
}

void AVBoRKeyFob::LockPressed(const FIsdkInteractionPointerEvent& Event)
{
	if (!CanPress(Event)) { return; }
	UE_LOG(LogKeyFob, Log, TEXT("%s Lock pressed hand=%d"), *GetName(), 1 - HoldingHand);
	OnLockPressed.Broadcast(this);
}

void AVBoRKeyFob::ClearVelocity()
{
	KeyMesh->SetPhysicsLinearVelocity(FVector::ZeroVector);
	KeyMesh->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
}

void AVBoRKeyFob::SetHome(AVBoRKeyTable* Table)
{
	HomeTable = Table;
	if (Table) { ClearVelocity(); SetActorTransform(Table->GetKeyHomeTransform(this), false, nullptr, ETeleportType::TeleportPhysics); }
}

void AVBoRKeyFob::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	// Switch outside pointer callback. Block unheld key press visual/sound too (P4a R1-1)
	const bool bHeld = HoldingHand != INDEX_NONE;
	for (AVBoRPokeButton* Button : {UnlockButton.Get(), LockButton.Get()})
	{
		if (Button && Button->Poke->IsActive() != bHeld)
		{
			if (bHeld) { Button->Poke->Activate(false); }
			else { Button->Poke->Deactivate(); }
		}
	}
	if (HoldingHand != INDEX_NONE) { RecordTrackingFrame(); return; }
	if (HomeTable.IsValid() && KeyMesh->Bounds.GetBox().Min.Z <= HomeTable->GetActorLocation().Z + FloorMarginCm)
	{
		ClearVelocity(); SetActorTransform(HomeTable->GetKeyHomeTransform(this), false, nullptr, ETeleportType::TeleportPhysics);
		UE_LOG(LogKeyFob, Log, TEXT("%s return to %s"), *GetName(), *HomeTable->GetName());
	}
}

void AVBoRKeyFob::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UnlockButton) { UnlockButton->PressFilter.Unbind(); UnlockButton->Destroy(); }
	if (LockButton) { LockButton->PressFilter.Unbind(); LockButton->Destroy(); }
	Super::EndPlay(EndPlayReason);
}
