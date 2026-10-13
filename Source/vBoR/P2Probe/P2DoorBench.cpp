#include "P2DoorBench.h"
#include "../Interaction/VBoRHingedDoor.h"
#include "../Interaction/VBoRHandRigPawn.h"
#include "Camera/CameraComponent.h"
#include "Components/InputComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "IXRTrackingSystem.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"

DEFINE_LOG_CATEGORY(LogP2Door);

AP2DoorBench::AP2DoorBench()
{
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("BenchOrigin")));
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostUpdateWork;
	DoorClass = AVBoRHingedDoor::StaticClass();
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	const FName Names[] = {TEXT("FrameBottom"), TEXT("FrameTop"), TEXT("FrameFront"), TEXT("FrameBack")};
	for (int32 I = 0; I < 4; ++I)
	{
		FrameParts[I] = CreateDefaultSubobject<UStaticMeshComponent>(Names[I]);
		FrameParts[I]->SetupAttachment(RootComponent);
		FrameParts[I]->SetStaticMesh(Cube.Object);
		FrameParts[I]->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		FrameParts[I]->SetVisibility(false);
	}
	Panel = CreateDefaultSubobject<UTextRenderComponent>(TEXT("MeasurementPanel"));
	Panel->SetupAttachment(RootComponent);
	Panel->SetHorizontalAlignment(EHTA_Left);
	Panel->SetVerticalAlignment(EVRTA_TextTop);
	Panel->SetWorldSize(1.5f);
	Panel->SetTextRenderColor(FColor::White);
	Panel->SetVisibility(false);
}

void AP2DoorBench::BeginPlay()
{
	Super::BeginPlay();
	if (APlayerController* Controller = UGameplayStatics::GetPlayerController(this, 0))
	{
		EnableInput(Controller);
		InputComponent->BindKey(EKeys::R, IE_Pressed, this, &AP2DoorBench::Reposition);
		InputComponent->BindKey(EKeys::B, IE_Pressed, this, &AP2DoorBench::NextTrial);
	}
	TryPlace();
}

bool AP2DoorBench::TryPlace()
{
	if (GEngine && GEngine->XRSystem.IsValid() && !GEngine->XRSystem->IsTracking(IXRTrackingSystem::HMDDeviceId)) { return false; }
	AVBoRHandRigPawn* Pawn = Cast<AVBoRHandRigPawn>(UGameplayStatics::GetPlayerPawn(this, 0));
	if (!Pawn || !Pawn->Camera) { return false; }
	PlaceFromHead(Pawn->Camera->GetComponentTransform(), Pawn->GetActorLocation().Z);
	return true;
}

void AP2DoorBench::PlaceFromHead(const FTransform& Head, double FloorZ)
{
	if (Door) { Snapshot(FString::Printf(TEXT("Trial %d segment before R"), TrialNumber), true); }
	const double HeadYaw = Head.Rotator().Yaw;
	const FQuat HeadRotation = FRotator(0, HeadYaw, 0).Quaternion();
	const FVector Forward = HeadRotation.GetForwardVector(), Right = HeadRotation.GetRightVector();
	FVector Origin = Head.GetLocation() + Forward * (HeadDistanceCm + DoorSizeCm.Y * 0.5) - Right * (DoorSizeCm.X - HeadFromTrailingEdgeCm);
	Origin.Z = FloorZ;
	SetActorTransform(FTransform(FRotator(0, HeadYaw + 90, 0), Origin));
	const FTransform Pose(GetActorQuat(), GetActorTransform().TransformPosition(FVector(0, 0, BottomHeightCm)));
	if (!Door && DoorClass)
	{
		Door = GetWorld()->SpawnActorDeferred<AVBoRHingedDoor>(DoorClass, Pose, this, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (Door)
		{
			const FVector MeshSize = Door->DoorMesh->GetStaticMesh()->GetBoundingBox().GetSize();
			Door->MeshRestFromRoot = FTransform(FQuat::Identity, FVector(DoorSizeCm.X * 0.5, 0, DoorSizeCm.Z * 0.5), DoorSizeCm / MeshSize);
			Door->OutsideHandle->SetRelativeLocation(FVector(DoorSizeCm.X - OutsideHandleInsetCm, DoorSizeCm.Y * 0.5 + 4, OutsideHandleHeightCm - BottomHeightCm));
			Door->InsideHandle->SetRelativeLocation(FVector(DoorSizeCm.X - InsideHandleInsetCm, -DoorSizeCm.Y * 0.5 - 4, InsideHandleHeightCm - BottomHeightCm));
			Door->FinishSpawning(Pose);
			Door->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);
			AddTickPrerequisiteActor(Door);
		}
	}
	if (Door) { Door->SetActorTransform(Pose); Door->ResetDoor(); }
	const FVector Centers[] = {FVector(DoorSizeCm.X * 0.5, 0, BottomHeightCm - 2),
		FVector(DoorSizeCm.X * 0.5, 0, BottomHeightCm + DoorSizeCm.Z + 2),
		FVector(-2, 0, BottomHeightCm + DoorSizeCm.Z * 0.5), FVector(DoorSizeCm.X + 2, 0, BottomHeightCm + DoorSizeCm.Z * 0.5)};
	for (int32 I = 0; I < 4; ++I)
	{
		FrameParts[I]->SetRelativeLocation(Centers[I]);
		FrameParts[I]->SetRelativeScale3D((I < 2 ? FVector(DoorSizeCm.X + 8, DoorSizeCm.Y, 4) : FVector(4, DoorSizeCm.Y, DoorSizeCm.Z)) / 100);
		FrameParts[I]->SetVisibility(true);
	}
	Panel->SetRelativeLocation(FVector(DoorSizeCm.X + 15, 8, BottomHeightCm + DoorSizeCm.Z + 25));
	Panel->SetRelativeRotation(FRotator(0, 90, 0));
	Panel->SetVisibility(true);
	bPlaced = true;
	UE_LOG(LogP2Door, Log, TEXT("Placed from head yaw %.2f; physical floor Pawn Z %.2f cm; outside face %.2f cm ahead; approximate geometry, cumulative metrics retained"), HeadYaw, FloorZ, HeadDistanceCm);
	UpdatePanel();
}

void AP2DoorBench::Reposition() { if (!TryPlace()) { UE_LOG(LogP2Door, Log, TEXT("R waiting for tracked head")); } }

void AP2DoorBench::NextTrial()
{
	if (!Door) { return; }
	Snapshot(FString::Printf(TEXT("Trial %d summary"), TrialNumber), true);
	TrialBaseline = Door->GetMotion().Metrics;
	Door->ResetDoor();
	++TrialNumber;
	UpdatePanel();
}

void AP2DoorBench::Snapshot(const FString& Reason, bool bTrial)
{
	if (!Door) { return; }
	FString Text = Reason + TEXT(" | ") + Door->GetStateText() + TEXT(" | ") + Door->GetMeasurementText(bTrial ? &TrialBaseline : nullptr);
	Text.ReplaceInline(TEXT("\n"), TEXT(" | "));
	UE_LOG(LogP2Door, Log, TEXT("%s"), *Text);
}

void AP2DoorBench::UpdatePanel()
{
	const FString Text = FString::Printf(TEXT("P2 | not-tested | Approximate geometry / placeholder values\nDoor %s cm | R: re-place (keep totals) | B: next trial\nTrial %d | %s\nCumulative:\n%s\nCurrent trial:\n%s"),
		*DoorSizeCm.ToString(), TrialNumber, Door ? *Door->GetStateText() : TEXT("waiting for door"),
		Door ? *Door->GetMeasurementText() : TEXT("unavailable"), Door ? *Door->GetMeasurementText(&TrialBaseline) : TEXT("unavailable"));
	Panel->SetText(FText::FromString(Text));
}

void AP2DoorBench::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bPlaced) { TryPlace(); }
	UpdatePanel();
	if (!Door) { return; }
	// 상태 전이는 즉시, 연속 측정은 1 s 간격으로 남김 (P2 §2 측정)
	const FString State = Door->GetStateText();
	if (State != LastState || GetWorld()->GetTimeSeconds() - LastLogAt >= 1)
	{
		Snapshot(FString::Printf(TEXT("Trial %d snapshot"), TrialNumber), false);
		LastLogAt = GetWorld()->GetTimeSeconds(); LastState = State;
	}
}

void AP2DoorBench::EndPlay(const EEndPlayReason::Type Reason)
{
	Snapshot(FString::Printf(TEXT("Trial %d end summary"), TrialNumber), true);
	if (IsValid(Door)) { Door->Destroy(); }
	Super::EndPlay(Reason);
}
