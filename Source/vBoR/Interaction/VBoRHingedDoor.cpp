#include "VBoRHingedDoor.h"
#include "VBoRHandRigPawn.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Components/AudioComponent.h"
#include "Engine/StaticMesh.h"
#include "Interaction/IsdkGrabbableComponent.h"
#include "Interaction/IsdkGrabberComponent.h"
#include "Interaction/Pointable/IsdkInteractionPointerEvent.h"
#include "IsdkFunctionLibrary.h"
#include "IsdkContentAssetPaths.h"
#include "OculusXRInputFunctionLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Sound/SoundBase.h"
#include "UObject/ConstructorHelpers.h"

UVBoRDoorGrabTransformer::UVBoRDoorGrabTransformer() { ConfigureDoor(); }
void UVBoRDoorGrabTransformer::ConfigureDoor()
{
	bIsThrowable = false;
	MultiGrabBehavior = EIsdkMultiGrabBehavior::MultiGrab;
}

FTransform UVBoRDoorTransformer::UpdateTransform(const TArray<FIsdkGrabPose>& Poses, const FIsdkTargetTransform& Target)
{
	const AVBoRHingedDoor* Door = GetTypedOuter<AVBoRHingedDoor>();
	return Door ? Door->HingePivot->GetComponentTransform().GetRelativeTransform(Target.ParentWorldTransform) : Target.RelativeTransform;
}

FTransform UVBoRDoorTransformer::EndTransform(const FIsdkTargetTransform& Target)
{
	bActive = false;
	return UpdateTransform({}, Target);
}

AVBoRHingedDoor::AVBoRHingedDoor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostUpdateWork;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("DoorOrigin")));
	HingePivot = CreateDefaultSubobject<USceneComponent>(TEXT("HingePivot"));
	HingePivot->SetupAttachment(RootComponent);
	DoorMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DoorMesh"));
	DoorMesh->SetupAttachment(HingePivot);
	DoorMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	DoorMesh->SetSimulatePhysics(false);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	DoorMesh->SetStaticMesh(Cube.Object);
	DoorMesh->SetRelativeLocation(FVector(55, 0, 52.5));
	DoorMesh->SetRelativeScale3D(FVector(1.1, 0.1, 1.05));
	OutsideHandle = CreateDefaultSubobject<UBoxComponent>(TEXT("OutsideHandle"));
	InsideHandle = CreateDefaultSubobject<UBoxComponent>(TEXT("InsideHandle"));
	for (UBoxComponent* Handle : {OutsideHandle.Get(), InsideHandle.Get()})
	{
		Handle->SetupAttachment(HingePivot);
		Handle->SetBoxExtent(FVector(9, 4, 4));
		Handle->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		Handle->SetCollisionResponseToAllChannels(ECR_Ignore);
		// Palm detector 의 overlap 만 허용함 (손잡이 감지, P2 E20)
		Handle->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);
		Handle->SetGenerateOverlapEvents(true);
	}
	OutsideHandle->SetRelativeLocation(FVector(100, 9, 60));
	InsideHandle->SetRelativeLocation(FVector(85, -9, 50));
	UStaticMeshComponent* OutsideVisual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("OutsideHandleMesh"));
	UStaticMeshComponent* InsideVisual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("InsideHandleMesh"));
	OutsideVisual->SetupAttachment(OutsideHandle); InsideVisual->SetupAttachment(InsideHandle);
	for (UStaticMeshComponent* Visual : {OutsideVisual, InsideVisual})
	{
		Visual->SetStaticMesh(Cube.Object);
		Visual->SetRelativeScale3D(FVector(0.18, 0.04, 0.04));
		Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	OutsideGrab = CreateDefaultSubobject<UIsdkGrabbableComponent>(TEXT("OutsideGrab"));
	OutsideGrab->SetupAttachment(OutsideHandle);
	InsideGrab = CreateDefaultSubobject<UIsdkGrabbableComponent>(TEXT("InsideGrab"));
	InsideGrab->SetupAttachment(InsideHandle);
	OutsideTransformer = CreateDefaultSubobject<UVBoRDoorGrabTransformer>(TEXT("OutsideTransformer"));
	InsideTransformer = CreateDefaultSubobject<UVBoRDoorGrabTransformer>(TEXT("InsideTransformer"));
	DoorTransformer = CreateDefaultSubobject<UVBoRDoorTransformer>(TEXT("HingeOnlyTransformer"));
	StateAudio = CreateDefaultSubobject<UAudioComponent>(TEXT("LockAudio"));
	StateAudio->SetupAttachment(HingePivot);
	StateAudio->bAutoActivate = false;
	static ConstructorHelpers::FObjectFinder<USoundBase> Sound(IsdkContentAssetPaths::Audio::Interaction_BasicPoke_ButtonPress);
	StateAudio->SetSound(Sound.Object);
}

void AVBoRHingedDoor::SyncSettings()
{
	Motion.WidthCm = DoorSizeCm.X; Motion.HeightCm = DoorSizeCm.Z; Motion.ThicknessCm = DoorSizeCm.Y;
	Motion.MaxAngleDeg = MaxAngleDeg; Motion.MaxSpeedDegPerSecond = MaxSpeedDegPerSecond;
	Motion.LatchAngleDeg = LatchAngleDeg; Motion.RearmAngleDeg = RearmAngleDeg;
	Motion.PalmLimitDeg = PalmLimitDeg; Motion.MinAxisDistanceCm = MinAxisDistanceCm;
}

void AVBoRHingedDoor::PreInitializeComponents()
{
	SyncSettings();
	ClosedHingeRotation = HingePivot->GetRelativeRotation().Quaternion();
	for (int32 I = 0; I < 2; ++I)
	{
		UIsdkGrabbableComponent* Grab = I == 0 ? OutsideGrab.Get() : InsideGrab.Get();
		UVBoRDoorGrabTransformer* Transformer = I == 0 ? OutsideTransformer.Get() : InsideTransformer.Get();
		Grab->SetGrabInputMethodAllowed(EIsdkGrabInputMethod::Palm, true);
		Grab->SetGrabInputMethodAllowed(EIsdkGrabInputMethod::Pinch, false);
		Grab->SetGrabCollider(I == 0 ? OutsideHandle.Get() : InsideHandle.Get());
		Transformer->ConfigureDoor();
		Transformer->SetSingleGrabTransformerObject(DoorTransformer);
		Transformer->SetMultiGrabTransformerObject(DoorTransformer);
		Transformer->SetTransformTarget(HingePivot);
		Grab->SetGrabTransformer(Transformer);
	}
	Super::PreInitializeComponents();
}

void AVBoRHingedDoor::BeginPlay()
{
	Super::BeginPlay();
	UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, IsdkContentAssetPaths::Materials::GrabbableDefault);
	StateOverlay = Material ? UMaterialInstanceDynamic::Create(Material, this) : nullptr;
	OutsideGrab->GetInteractionPointerEventDelegate().AddUniqueDynamic(this, &AVBoRHingedDoor::HandleOutsidePointer);
	InsideGrab->GetInteractionPointerEventDelegate().AddUniqueDynamic(this, &AVBoRHingedDoor::HandleInsidePointer);
	ApplyState(Motion.AngleDeg, Motion.Metrics.Unlocks, Motion.Metrics.Latches);
}

FTransform AVBoRHingedDoor::HingeFrame() const
{
	FTransform Frame = HingePivot->GetComponentTransform();
	Frame.SetRotation(HingePivot->GetAttachParent()->GetComponentQuat() * ClosedHingeRotation);
	return Frame;
}

bool AVBoRHingedDoor::ReadHand(int32 Hand, FVBoRDoorHand& Input)
{
	Input = FVBoRDoorHand();
	if (!HandPawn) { HandPawn = Cast<AVBoRHandRigPawn>(UGameplayStatics::GetPlayerPawn(this, 0)); }
	const EOculusXRHandType Type = Hand == 0 ? EOculusXRHandType::HandLeft : EOculusXRHandType::HandRight;
	if (!HandPawn || !UOculusXRInputFunctionLibrary::IsHandPositionValid(Type) ||
		UOculusXRInputFunctionLibrary::GetTrackingConfidence(Type) != EOculusXRTrackingConfidence::High ||
		!HandPawn->ReadRawPalmAndTips(Hand, Input)) { Input.bTracked = false; return false; }
	const FTransform Frame = HingeFrame();
	for (FVector& Point : Input.Points) { Point = Frame.InverseTransformPosition(Point); }
	Input.PalmNormal = Frame.InverseTransformVectorNoScale(Input.PalmNormal).GetSafeNormal();
	return true;
}

bool AVBoRHingedDoor::ReadPalmClosed(int32 Hand, bool& bClosed) const
{
	if (!HandPawn) { return false; }
	return Hand == 0 ? HandPawn->LeftRig->ReadPalmGrab(bClosed) : HandPawn->RightRig->ReadPalmGrab(bClosed);
}

void AVBoRHingedDoor::HandleOutsidePointer(const FIsdkInteractionPointerEvent& Event) { HandlePointer(0, Event); }
void AVBoRHingedDoor::HandleInsidePointer(const FIsdkInteractionPointerEvent& Event) { HandlePointer(1, Event); }

void AVBoRHingedDoor::HandlePointer(int32 Handle, const FIsdkInteractionPointerEvent& Event)
{
	UIsdkGrabberComponent* Grabber = Cast<UIsdkGrabberComponent>(Event.Interactor);
	EIsdkHandedness Handedness;
	if (!Grabber || !UIsdkFunctionLibrary::GetHandednessFromInteractor(Grabber, Handedness)) { return; }
	const int32 Hand = Handedness == EIsdkHandedness::Left ? 0 : 1;
	if (Event.Type == EIsdkPointerEventType::Hover)
	{
		if (!Hovering[Handle].Contains(Grabber)) { Hovering[Handle].Add(Grabber); ++Motion.Metrics.Attempts[Hand][Handle]; }
	}
	else if (Event.Type == EIsdkPointerEventType::Unhover) { Hovering[Handle].Remove(Grabber); }
	else if (Event.Type == EIsdkPointerEventType::Select)
	{
		if (bNeedsPalmOpen[Hand] || Motion.GrabHand == Hand) { return; }
		const int32 Unlocks = Motion.Metrics.Unlocks;
		FVBoRDoorHand Input;
		ReadHand(Hand, Input);
		// 선택 취소·걸쇠 뒤 닫힌 손이 재선택되지 않게 함 (P2 D4)
		bNeedsPalmOpen[Hand] = true;
		if (Motion.BeginGrab(Hand, Handle, Input)) { HoldingGrabber = Grabber; }
		ApplyState(Motion.AngleDeg, Unlocks, Motion.Metrics.Latches);
	}
	else if ((Event.Type == EIsdkPointerEventType::Unselect || Event.Type == EIsdkPointerEventType::Cancel) &&
		HoldingGrabber == Grabber && Motion.GrabHand == Hand && Motion.GrabHandle == Handle)
	{
		FVBoRDoorHand Input;
		if (Motion.ReleaseGrab(ReadHand(Hand, Input))) { HoldingGrabber = nullptr; }
	}
}

void AVBoRHingedDoor::CancelSelections()
{
	HoldingGrabber = nullptr;
	OutsideTransformer->ForceCancel(); InsideTransformer->ForceCancel();
	Hovering[0].Reset(); Hovering[1].Reset();
}

void AVBoRHingedDoor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	SyncSettings();
	FVBoRDoorHand Hands[2];
	ReadHand(0, Hands[0]); ReadHand(1, Hands[1]);
	const double Before = Motion.AngleDeg;
	const int32 Unlocks = Motion.Metrics.Unlocks, Latches = Motion.Metrics.Latches;
	const bool bRecovering = Motion.bPaused;
	Motion.Advance(Hands, DeltaSeconds);
	for (int32 H = 0; H < 2; ++H)
	{
		bool bClosed = true;
		if (Hands[H].bTracked && ReadPalmClosed(H, bClosed) && !bClosed && !bRecovering && !Motion.bPaused)
		{
			bNeedsPalmOpen[H] = false;
			if (Motion.GrabHand == H && Motion.ReleaseGrab(true)) { HoldingGrabber = nullptr; }
		}
	}
	ApplyState(Before, Unlocks, Latches);
}

void AVBoRHingedDoor::ApplyState(double BeforeAngle, int32 BeforeUnlocks, int32 BeforeLatches)
{
	HingePivot->SetRelativeRotation(ClosedHingeRotation * FQuat(FVector::UpVector, FMath::DegreesToRadians(Motion.AngleDeg)));
	if (Motion.Metrics.Unlocks != BeforeUnlocks) { StateAudio->Play(); OnUnlocked.Broadcast(this); }
	if (Motion.Metrics.Latches != BeforeLatches) { CancelSelections(); StateAudio->Play(); OnLocked.Broadcast(this); }
	if (Motion.AngleDeg != BeforeAngle) { OnAngleChanged.Broadcast(static_cast<float>(Motion.AngleDeg)); }
	if (StateOverlay)
	{
		const FLinearColor Color = Motion.bPaused ? PausedColor : Motion.bLocked ? LockedColor :
			Motion.AngleDeg >= Motion.MaxAngleDeg ? MaximumColor : Motion.GrabHand != INDEX_NONE ? GrabbedColor :
			Motion.PushHand != INDEX_NONE ? PushingColor : UnlockedColor;
		StateOverlay->SetVectorParameterValue(TEXT("ColorParam"), Color);
		DoorMesh->SetOverlayMaterial(StateOverlay);
	}
}

void AVBoRHingedDoor::ResetDoor()
{
	const double Before = Motion.AngleDeg;
	Motion.Reset();
	for (int32 H = 0; H < 2; ++H)
	{
		bool bClosed = true;
		bNeedsPalmOpen[H] = !ReadPalmClosed(H, bClosed) || bClosed;
	}
	CancelSelections();
	ApplyState(Before, Motion.Metrics.Unlocks, Motion.Metrics.Latches);
}

FString AVBoRHingedDoor::GetStateText() const
{
	return FString::Printf(TEXT("%s | %.2f deg | grab=%s push=%s/%s | paused=%d | maximum=%d"),
		Motion.bLocked ? TEXT("Locked") : TEXT("Unlocked"), Motion.AngleDeg,
		Motion.GrabHand == 0 ? TEXT("L") : Motion.GrabHand == 1 ? TEXT("R") : TEXT("none"),
		Motion.PushHand == 0 ? TEXT("L") : Motion.PushHand == 1 ? TEXT("R") : TEXT("none"),
		Motion.PushFace == 0 ? TEXT("outside") : Motion.PushFace == 1 ? TEXT("inside") : TEXT("none"), Motion.bPaused, Motion.AngleDeg >= MaxAngleDeg);
}

FString AVBoRHingedDoor::GetMeasurementText(const FVBoRDoorMetrics* Baseline) const
{
	const FVBoRDoorMetrics M = Baseline ? Motion.Metrics.Since(*Baseline) : Motion.Metrics;
	return FString::Printf(TEXT("Unlock=%d latch=%d last unlock=%s\nPush L outside/inside=%d/%d R=%d/%d | moved %.2f deg\n"
		"Ignored contacts: locked=%d palm direction=%d behind=%d near axis=%d outside=%d tracking Low=%d\n"
		"Handle attempts/grabs: L outside=%d/%d inside=%d/%d R outside=%d/%d inside=%d/%d | held %.3f s\n"
		"Rate capped frames=%d | grab pauses=%d %.3f s | recovery unapplied=%.2f cm | ignored SDK release=%d | push tracking losses=%d\n"
		"Palm-to-face angle L=%.1f R=%.1f deg (-1: no candidate) | palm axis -Z inferred; not-tested"),
		M.Unlocks, M.Latches, M.LastUnlockHandle == 0 ? TEXT("outside") : M.LastUnlockHandle == 1 ? TEXT("inside") : TEXT("none"),
		M.PushStarts[0][0], M.PushStarts[0][1], M.PushStarts[1][0], M.PushStarts[1][1], M.PushedDeg,
		M.Ignored[0], M.Ignored[1], M.Ignored[2], M.Ignored[3], M.Ignored[4], M.Ignored[5],
		M.Attempts[0][0], M.Grabs[0][0], M.Attempts[0][1], M.Grabs[0][1], M.Attempts[1][0], M.Grabs[1][0], M.Attempts[1][1], M.Grabs[1][1],
		M.HeldSeconds, M.RateLimitedFrames, M.Pauses, M.PausedSeconds, M.RecoveryDistanceCm, M.IgnoredReleases, M.PushTrackingLosses, Motion.PalmAngleDeg[0], Motion.PalmAngleDeg[1]);
}

void AVBoRHingedDoor::EndPlay(const EEndPlayReason::Type Reason)
{
	OutsideGrab->GetInteractionPointerEventDelegate().RemoveDynamic(this, &AVBoRHingedDoor::HandleOutsidePointer);
	InsideGrab->GetInteractionPointerEventDelegate().RemoveDynamic(this, &AVBoRHingedDoor::HandleInsidePointer);
	DoorMesh->SetOverlayMaterial(nullptr);
	Super::EndPlay(Reason);
}
