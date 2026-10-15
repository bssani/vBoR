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
	static ConstructorHelpers::FObjectFinder<USoundBase> Sound(IsdkContentAssetPaths::Audio::Interaction_BasicPoke_ButtonPress);
	WarningSound = Sound.Object;
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
	WarningAudio->Stop();
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
	bPendingCommand = false; WarningElapsed = 0;
	// B 는 다음 시도에도 누적을 이어감 (P4b R1-1)
	if (bClearCounts) { WarningRequests = 0; ObstructionCount = 0; }
	WarningAudio->Stop();
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

void AVBoRPoweredPanel::WarnFor(double Seconds)
{
	WarningElapsed += Seconds;
	const double Interval = FMath::Max(0.05f, WarningIntervalSeconds);
	while (WarningElapsed + UE_DOUBLE_SMALL_NUMBER >= Interval)
	{
		WarningElapsed = FMath::Max(0.0, WarningElapsed - Interval);
		++WarningRequests;
		WarningAudio->SetSound(WarningSound); WarningAudio->Play();
	}
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
	if (!Motion.IsActive()) { return; }
	double Remaining = FMath::Max(0.0f, DeltaSeconds);
	if (bPendingCommand)
	{
		bPendingCommand = false;
		WarningElapsed = FMath::Max(0.05f, WarningIntervalSeconds);
		WarnFor(0);
		PublishState();
	}
	if (Motion.IsWaiting())
	{
		const double Before = Remaining;
		const bool bReady = Motion.ConsumeWait(Remaining);
		WarnFor(Before - Remaining);
		if (!bReady) { return; }
		Motion.State = Motion.State == EVBoRPoweredState::OpeningWait ? EVBoRPoweredState::Opening : EVBoRPoweredState::Closing;
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
				WarnFor(FMath::Abs(Motion.AngleDeg - Start) / Speed);
				Motion.State = EVBoRPoweredState::Opening;
				++ObstructionCount;
				PublishState();
				OnObstructed.Broadcast(this, Box);
				return;
			}
		}
		Motion.AngleDeg = Next;
	}
	ApplyAngle();
	WarnFor(Travel / Speed);
	if (FMath::IsNearlyEqual(Motion.AngleDeg, Target))
	{
		Motion.State = bOpening ? EVBoRPoweredState::Open : EVBoRPoweredState::Closed;
		WarningAudio->Stop();
		PublishState();
		if (bOpening) { OnOpened.Broadcast(this); } else { OnClosed.Broadcast(this); }
	}
}
