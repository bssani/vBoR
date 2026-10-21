#include "VBoRPoweredPanel.h"

#include "VBoRCarryObject.h"
#include "VBoRPokeButton.h"
#include "Components/AudioComponent.h"
#include "Components/InputComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "IsdkContentAssetPaths.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "UObject/ConstructorHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogPoweredPanel, Log, All);

AVBoRPoweredPanel::AVBoRPoweredPanel()
{
	static ConstructorHelpers::FObjectFinder<USoundBase> ButtonSound(IsdkContentAssetPaths::Audio::Interaction_BasicPoke_ButtonPress);
	ButtonPressedCue.Sound = ButtonSound.Object;
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostUpdateWork;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("VehicleOrigin")));
	HingePivot = CreateDefaultSubobject<USceneComponent>(TEXT("HingePivot"));
	HingePivot->SetupAttachment(RootComponent);
	PanelMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ApproximatePoweredPanel"));
	PanelMesh->SetupAttachment(HingePivot);
	PanelMesh->SetMobility(EComponentMobility::Movable);
	PanelMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	PanelMesh->SetCollisionObjectType(ECC_WorldDynamic);
	PanelMesh->SetCollisionResponseToAllChannels(ECR_Block);
	PanelMesh->SetSimulatePhysics(false);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(IsdkContentAssetPaths::Engine::BasicShapes::Cube);
	PanelMesh->SetStaticMesh(Cube.Object);
	WarningAudio = CreateDefaultSubobject<UAudioComponent>(TEXT("WarningAudio"));
	WarningAudio->SetupAttachment(HingePivot);
	WarningAudio->bAutoActivate = false;
	SoundAudio.SetNum(static_cast<int32>(EVBoRPoweredSound::Count));
	const TCHAR* Names[] = {TEXT("OpenAcceptedAudio"), TEXT("CloseAcceptedAudio"), TEXT("WarningAudio"), TEXT("MotorAudio"), TEXT("OpenedAudio"), TEXT("ClosedAudio"), TEXT("ObstructedAudio")};
	for (int32 I = 0; I < SoundAudio.Num(); ++I)
	{
		SoundAudio[I] = I == static_cast<int32>(EVBoRPoweredSound::Warning) ? WarningAudio.Get() : CreateDefaultSubobject<UAudioComponent>(Names[I]);
		SoundAudio[I]->SetupAttachment(HingePivot);
		SoundAudio[I]->bAutoActivate = false;
	}
	static ConstructorHelpers::FObjectFinder<USoundBase> Sound(IsdkContentAssetPaths::Audio::Interaction_BasicPoke_ButtonPress);
	WarningCue.Sound = Sound.Object;
	WarningCue.RepeatSeconds = 0.5f;
}

void AVBoRPoweredPanel::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	HingePivot->SetRelativeLocationAndRotation(HingeLocationCm, FRotator::ZeroRotator);
	PanelMesh->SetRelativeTransform(MeshRestFromRoot.GetRelativeTransform(FTransform(HingeLocationCm)));
	Motion.AngleDeg = bStartOpen ? FMath::Clamp(OpenAngleDeg, 1.0f, 170.0f) : 0;
	Motion.State = bStartOpen ? EVBoRPoweredState::Open : EVBoRPoweredState::Closed;
	ApplyAngle();
}

AVBoRPokeButton* AVBoRPoweredPanel::CreateButton(const FTransform& Relative)
{
	const FTransform Pose = Relative * HingePivot->GetComponentTransform();
	AVBoRPokeButton* Button = GetWorld()->SpawnActorDeferred<AVBoRPokeButton>(AVBoRPokeButton::StaticClass(), Pose, this, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Button) { return nullptr; }
	Button->ButtonMesh->SetRelativeScale3D(ButtonSizeCm.ComponentMax(FVector(0.1)) / 100.0);
	Button->PressedCue = ButtonPressedCue; Button->ReleasedCue = ButtonReleasedCue;
	Button->FinishSpawning(Pose);
	Button->AttachToComponent(HingePivot, FAttachmentTransformRules::KeepWorldTransform);
	return Button;
}

void AVBoRPoweredPanel::BeginPlay()
{
	Super::BeginPlay();
	ResetPanel();
	OpenButton = CreateButton(OpenButtonFromHinge);
	CloseButton = CreateButton(CloseButtonFromHinge);
	if (OpenButton) { OpenButton->OnButtonPressed.AddDynamic(this, &AVBoRPoweredPanel::OpenPressed); }
	if (CloseButton) { CloseButton->OnButtonPressed.AddDynamic(this, &AVBoRPoweredPanel::ClosePressed); }
	if (APlayerController* Controller = UGameplayStatics::GetPlayerController(this, 0))
	{
		EnableInput(Controller);
		InputComponent->BindKey(OpenKey, IE_Pressed, this, &AVBoRPoweredPanel::RequestOpen);
		InputComponent->BindKey(CloseKey, IE_Pressed, this, &AVBoRPoweredPanel::RequestClose);
	}
}

void AVBoRPoweredPanel::EndPlay(const EEndPlayReason::Type Reason)
{
	if (OpenButton) { OpenButton->Destroy(); }
	if (CloseButton) { CloseButton->Destroy(); }
	for (int32 I = 0; I < SoundAudio.Num(); ++I) { SoundPlayback[I].Reset(SoundAudio[I], false); }
	Super::EndPlay(Reason);
}

void AVBoRPoweredPanel::RequestOpen() { Request(true); }
void AVBoRPoweredPanel::RequestClose() { Request(false); }
void AVBoRPoweredPanel::OpenPressed(const FIsdkInteractionPointerEvent& Event) { RequestOpen(); }
void AVBoRPoweredPanel::ClosePressed(const FIsdkInteractionPointerEvent& Event) { RequestClose(); }

void AVBoRPoweredPanel::Request(bool bOpen)
{
	if (!Motion.Request(bOpen, DelaySeconds))
	{
		UE_LOG(LogPoweredPanel, Log, TEXT("%s ignored %s in %s"), *GetName(), bOpen ? TEXT("open") : TEXT("close"), *GetStateText());
		return;
	}
	// 첫 입력이 대기 상태를 예약함. 소리·이벤트·이동은 다음 Tick 에서만 처리됨 (P4b D3·E11)
	bPendingCommand = true;
}

void AVBoRPoweredPanel::ResetPanel(bool bClearCounts)
{
	Motion = FVBoRPoweredMotion();
	Motion.State = bStartOpen ? EVBoRPoweredState::Open : EVBoRPoweredState::Closed;
	Motion.AngleDeg = bStartOpen ? FMath::Clamp(OpenAngleDeg, 1.0f, 170.0f) : 0;
	bPendingCommand = false;
	// B 는 다음 시도에도 누적을 이어감 (P4b R1-1)
	if (bClearCounts) { ObstructionCount = 0; }
	for (int32 I = 0; I < SoundAudio.Num(); ++I) { SoundPlayback[I].Reset(SoundAudio[I], bClearCounts); }
	ApplyAngle();
	PublishState();
}

void AVBoRPoweredPanel::ApplyAngle()
{
	// +Y 축 양의 회전: 닫힌 판 아래쪽이 트렁크 바깥 -X 로 올라감 (P4b D8, deg)
	HingePivot->SetRelativeRotation(FQuat(FVector::YAxisVector, FMath::DegreesToRadians(Motion.AngleDeg)));
}

FTransform AVBoRPoweredPanel::MeshPoseAt(float Angle) const
{
	const FTransform Hinge(FQuat(FVector::YAxisVector, FMath::DegreesToRadians(Angle)), HingeLocationCm);
	return PanelMesh->GetRelativeTransform() * Hinge * GetActorTransform();
}

AVBoRCarryObject* AVBoRPoweredPanel::FindObstruction(float FromAngle, float ToAngle) const
{
	if (!PanelMesh->GetStaticMesh() || !GetWorld()) { return nullptr; }
	const FBox Bounds = PanelMesh->GetStaticMesh()->GetBoundingBox();
	const FTransform Mid = MeshPoseAt((FromAngle + ToAngle) * 0.5f);
	const FVector Scale = Mid.GetScale3D().GetAbs();
	const FVector Center = Mid.TransformPosition(Bounds.GetCenter());
	const FVector Pivot = GetActorTransform().TransformPosition(HingeLocationCm);
	const double Radius = FVector::Distance(Center, Pivot) + (Bounds.GetExtent() * Scale).Size();
	// 1 deg 이하 구간 전체를 덮는 보수적 여유 cm. 끝점이 비어 있는 큰 Tick 도 관통하지 않음 (P4b D5·E9)
	const double MarginCm = Radius * FMath::DegreesToRadians(FMath::Abs(ToAngle - FromAngle) * 0.5) + 0.001;
	const FCollisionShape Shape = FCollisionShape::MakeBox(Bounds.GetExtent() * Scale + FVector(MarginCm));
	TArray<FOverlapResult> Hits;
	FCollisionObjectQueryParams Objects;
	Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
	Objects.AddObjectTypesToQuery(ECC_WorldStatic);
	GetWorld()->OverlapMultiByObjectType(Hits, Center, Mid.GetRotation(), Objects, Shape, FCollisionQueryParams(SCENE_QUERY_STAT(PoweredPanel), false, this));
	for (const FOverlapResult& Hit : Hits)
	{
		AVBoRCarryObject* Box = Cast<AVBoRCarryObject>(Hit.GetActor());
		if (IsValid(Box) && Hit.GetComponent() == Box->Mesh) { return Box; }
	}
	return nullptr;
}

const FVBoRSoundCue& AVBoRPoweredPanel::GetSoundCue(EVBoRPoweredSound Slot) const
{
	switch (Slot)
	{
	case EVBoRPoweredSound::OpenAccepted: return OpenAcceptedCue;
	case EVBoRPoweredSound::CloseAccepted: return CloseAcceptedCue;
	case EVBoRPoweredSound::Warning: return WarningCue;
	case EVBoRPoweredSound::Motor: return MotorCue;
	case EVBoRPoweredSound::Opened: return OpenedCue;
	case EVBoRPoweredSound::Closed: return ClosedCue;
	case EVBoRPoweredSound::Obstructed: return ObstructedCue;
	default: checkNoEntry(); return OpenAcceptedCue;
	}
}

void AVBoRPoweredPanel::QueueSound(EVBoRPoweredSound Slot)
{
	const int32 I = static_cast<int32>(Slot);
	SoundPlayback[I].Queue(GetSoundCue(Slot), SoundAudio[I], SoundFrame);
}

void AVBoRPoweredPanel::BeginHeldSound(EVBoRPoweredSound Slot, const FVBoRHeldSoundCue& Cue)
{
	const int32 I = static_cast<int32>(Slot);
	SoundPlayback[I].BeginHeld(Cue, SoundAudio[I], SoundFrame);
}

void AVBoRPoweredPanel::AdvanceSounds(double Seconds, bool bIncludeHeld)
{
	for (int32 I = 0; I < SoundAudio.Num(); ++I)
	{
		const EVBoRPoweredSound Slot = static_cast<EVBoRPoweredSound>(I);
		if (!bIncludeHeld && (Slot == EVBoRPoweredSound::Warning || Slot == EVBoRPoweredSound::Motor)) { continue; }
		SoundPlayback[I].Advance(GetSoundCue(Slot), SoundAudio[I], Seconds, SoundFrame);
	}
}

void AVBoRPoweredPanel::StopHeldSounds()
{
	const int32 Warning = static_cast<int32>(EVBoRPoweredSound::Warning), Motor = static_cast<int32>(EVBoRPoweredSound::Motor);
	SoundPlayback[Warning].EndHeld(WarningCue, SoundAudio[Warning]);
	SoundPlayback[Motor].EndHeld(MotorCue, SoundAudio[Motor]);
}

void AVBoRPoweredPanel::PublishState()
{
	UE_LOG(LogPoweredPanel, Log, TEXT("%s %s | %.2f deg | obstructions=%d | approximate geometry"), *GetName(), *GetStateText(), Motion.AngleDeg, ObstructionCount);
	OnStateChanged.Broadcast(this, Motion.State);
}

FString AVBoRPoweredPanel::GetStateText() const
{
	return StaticEnum<EVBoRPoweredState>()->GetNameStringByValue(static_cast<int64>(Motion.State));
}

void AVBoRPoweredPanel::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	++SoundFrame;
	if (!Motion.IsActive()) { AdvanceSounds(DeltaSeconds); return; }
	double Remaining = FMath::Max(0.0f, DeltaSeconds);
	if (bPendingCommand)
	{
		bPendingCommand = false;
		QueueSound(Motion.State == EVBoRPoweredState::OpeningWait ? EVBoRPoweredSound::OpenAccepted : EVBoRPoweredSound::CloseAccepted);
		BeginHeldSound(EVBoRPoweredSound::Warning, WarningCue);
		PublishState();
	}
	if (Motion.IsWaiting())
	{
		const double Before = Remaining;
		const bool bReady = Motion.ConsumeWait(Remaining);
		AdvanceSounds(Before - Remaining);
		if (!bReady) { return; }
		Motion.State = Motion.State == EVBoRPoweredState::OpeningWait ? EVBoRPoweredState::Opening : EVBoRPoweredState::Closing;
		BeginHeldSound(EVBoRPoweredSound::Motor, MotorCue);
		PublishState();
	}
	const bool bOpening = Motion.State == EVBoRPoweredState::Opening;
	const float Target = bOpening ? FMath::Clamp(OpenAngleDeg, 1.0f, 170.0f) : 0;
	const float Speed = FMath::Max(0.1f, SpeedDegPerSecond);
	const float Travel = FMath::Min(static_cast<float>(Remaining * Speed), FMath::Abs(Target - Motion.AngleDeg));
	const float Start = Motion.AngleDeg;
	const int32 Steps = FMath::Max(1, FMath::CeilToInt(Travel));
	for (int32 I = 1; I <= Steps; ++I)
	{
		const float Next = Start + (bOpening ? Travel : -Travel) * I / Steps;
		if (!bOpening)
		{
			if (AVBoRCarryObject* Box = FindObstruction(Motion.AngleDeg, Next))
			{
				ApplyAngle();
				const double Used = FMath::Abs(Motion.AngleDeg - Start) / Speed;
				AdvanceSounds(Used);
				Motion.State = EVBoRPoweredState::Opening;
				++ObstructionCount;
				PublishState();
				QueueSound(EVBoRPoweredSound::Obstructed);
				OnObstructed.Broadcast(this, Box);
				// 움직임이 버린 frame 나머지는 순간 소리 예약에만 적용함 (기존 끼임 동작 유지, P5r §3)
				AdvanceSounds(FMath::Max(0.0, Remaining - Used), false);
				return;
			}
		}
		Motion.AngleDeg = Next;
	}
	ApplyAngle();
	const double Used = Travel / Speed;
	AdvanceSounds(Used);
	if (FMath::IsNearlyEqual(Motion.AngleDeg, Target))
	{
		Motion.State = bOpening ? EVBoRPoweredState::Open : EVBoRPoweredState::Closed;
		StopHeldSounds();
		PublishState();
		QueueSound(bOpening ? EVBoRPoweredSound::Opened : EVBoRPoweredSound::Closed);
		if (bOpening) { OnOpened.Broadcast(this); } else { OnClosed.Broadcast(this); }
	}
	AdvanceSounds(FMath::Max(0.0, Remaining - Used), false);
}
