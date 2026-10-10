#include "P1LoadBench.h"

#include "P1TrunkMock.h"
#include "../Interaction/VBoRCarryObject.h"
#include "../Interaction/VBoRLoadTarget.h"
#include "../Interaction/VBoRHandRigPawn.h"
#include "Camera/CameraComponent.h"
#include "Components/InputComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "IXRTrackingSystem.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"

DEFINE_LOG_CATEGORY(LogP1Probe);

AP1LoadBench::AP1LoadBench()
{
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("OpeningOrigin")));
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostUpdateWork; // HMD camera 갱신 뒤 배치 (P1 §2)
	Stand = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BagStand"));
	Stand->SetupAttachment(RootComponent);
	Stand->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Stand->SetCollisionResponseToAllChannels(ECR_Block);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (Cube.Succeeded()) { Stand->SetStaticMesh(Cube.Object); }
	Panel = CreateDefaultSubobject<UTextRenderComponent>(TEXT("MeasurementPanel"));
	Panel->SetupAttachment(RootComponent);
	Panel->SetHorizontalAlignment(EHTA_Left);
	Panel->SetVerticalAlignment(EVRTA_TextTop);
	Panel->SetWorldSize(1.3f);
	Panel->SetTextRenderColor(FColor::White);
	TrunkClass = AP1TrunkMock::StaticClass();
	BagClass = AVBoRCarryObject::StaticClass();
	TargetClass = AVBoRLoadTarget::StaticClass();
}

void AP1LoadBench::BeginPlay()
{
	Super::BeginPlay();
	HandPawn = Cast<AVBoRHandRigPawn>(UGameplayStatics::GetPlayerPawn(this, 0));
	Panel->SetRelativeLocation(FVector(-55, -105, 145));
	Panel->SetRelativeRotation(FRotator(0, 180, 0));
	Panel->SetVisibility(false);
	if (APlayerController* Controller = UGameplayStatics::GetPlayerController(this, 0))
	{
		EnableInput(Controller);
		InputComponent->BindKey(RepositionKey, IE_Pressed, this, &AP1LoadBench::ResetAndReposition);
	}
	RecordEvent(FString::Printf(TEXT("Rig: palm grab allowed=%d; palm only; distance/ray blocked; throwing off | %s"),
		HandPawn && HandPawn->bAllowPalmGrab ? 1 : 0, HandPawn ? *HandPawn->DescribeRig() : TEXT("hand rig unavailable")));
	if (!GEngine || !GEngine->XRSystem.IsValid()) { TryPlaceFromHead(); }
	UpdatePanel();
}

void AP1LoadBench::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (Bag)
	{
		Bag->OnGrabbed.RemoveDynamic(this, &AP1LoadBench::HandleGrabbed);
		Bag->OnCarryReleased.RemoveDynamic(this, &AP1LoadBench::HandleCarryReleased);
		Bag->OnSeated.RemoveDynamic(this, &AP1LoadBench::HandleSeated);
	}
	for (AActor* Part : {static_cast<AActor*>(Bag.Get()), static_cast<AActor*>(Target.Get()), static_cast<AActor*>(Trunk.Get())})
	{
		if (IsValid(Part)) { Part->Destroy(); }
	}
	Super::EndPlay(EndPlayReason);
}

void AP1LoadBench::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bPlaced) { TryPlaceFromHead(); }
	if (bPlaced)
	{
		UpdateCrossing();
		if (Bag)
		{
			const FString State = Bag->GetStateText();
			if (State != LastObservedState)
			{
				RecordEvent(FString::Printf(TEXT("State %s -> %s"), LastObservedState.IsEmpty() ? TEXT("none") : *LastObservedState, *State));
				LastObservedState = State;
				RecordSnapshot(TEXT("State transition snapshot"));
			}
		}
	}
	UpdatePanel();
}

bool AP1LoadBench::TryPlaceFromHead()
{
	if (GEngine && GEngine->XRSystem.IsValid() && !GEngine->XRSystem->IsTracking(IXRTrackingSystem::HMDDeviceId)) { return false; }
	if (!HandPawn) { HandPawn = Cast<AVBoRHandRigPawn>(UGameplayStatics::GetPlayerPawn(this, 0)); }
	if (!HandPawn || !HandPawn->Camera) { return false; }
	PlaceFromHead(HandPawn->Camera->GetComponentTransform());
	return true;
}

void AP1LoadBench::ResetAndReposition()
{
	if (!TryPlaceFromHead()) { RecordEvent(TEXT("Reset waiting for tracked head")); }
}

FTransform AP1LoadBench::BagStartPose() const
{
	// Cylinder local Z 를 차량 +Y 로 눕힘. 받침대 윗면에 바닥이 닿음 (D5)
	return FTransform(GetActorQuat() * FRotator(0, 0, 90).Quaternion(), GetActorTransform().TransformPosition(
		FVector(-StandBehindOpeningCm, 0, StandTopHeightCm + BagDiameterCm * 0.5f)),
		Bag ? Bag->GetActorScale3D() : FVector::OneVector);
}

FTransform AP1LoadBench::GoalPose() const
{
	const float FloorZ = Trunk ? Trunk->FloorHeightCm : 75.0f;
	return FTransform(GetActorQuat() * FRotator(0, 0, 90).Quaternion(), GetActorTransform().TransformPosition(
		FVector(GoalOffsetCm.X, GoalOffsetCm.Y, FloorZ + GoalOffsetCm.Z)));
}

void AP1LoadBench::PlaceFromHead(const FTransform& Head)
{
	const FRotator Yaw(0, Head.Rotator().Yaw, 0);
	const FVector Origin = Head.GetLocation() + Yaw.RotateVector(FVector(OpeningForwardCm, 0, 0)) - FVector(0, 0, AssumedEyeHeightCm);
	if (Bag) { Bag->ResetToPose(Bag->GetActorTransform()); } // 손 세션을 먼저 끝냄 (P1 §2 R)
	SetActorTransform(FTransform(Yaw, Origin));
	Stand->SetRelativeLocation(FVector(-StandBehindOpeningCm, 0, StandTopHeightCm - StandSizeCm.Z * 0.5f));
	Stand->SetRelativeScale3D(StandSizeCm / 100.0f);
	if (!bPlaced) { SpawnParts(); }
	if (Target) { Target->SetActorTransform(GoalPose()); }
	if (Bag) { Bag->ResetToPose(BagStartPose()); }
	// 초기 배치와 R 을 입구 통과로 세지 않음 (P1 §2 경로)
	bPreviousBodyBeforeOpening = false;
	bCrossingRecorded = false;
	bBlockedAtCrossingRecorded = false;
	EntranceCrossings = 0;
	BlockedAtCrossings = 0;
	LastObservedState = Bag ? Bag->GetStateText() : FString();
	bPlaced = true;
	Panel->SetVisibility(true);
	RecordEvent(TEXT("Bench placed/reset at head yaw; opening +130 cm; stand -100 cm; geometry approximate"));
	RecordEvent(FString::Printf(TEXT("Static target fit=%d (bag length %.1f cm, inside width %.1f cm)"),
		Target && Bag && Target->IsStaticFit(Bag) ? 1 : 0, BagLengthCm, Trunk ? Trunk->InsideWidthCm : 0.0f));
	UpdatePanel();
}

void AP1LoadBench::SpawnParts()
{
	UWorld* World = GetWorld();
	if (!World) { return; }
	Trunk = World->SpawnActor<AP1TrunkMock>(TrunkClass, GetActorTransform());
	if (Trunk) { Trunk->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform); }
	const FTransform DesiredGoal = GoalPose();
	Target = World->SpawnActorDeferred<AVBoRLoadTarget>(TargetClass, DesiredGoal, this, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (Target)
	{
		Target->GoalSizeCm = FVector(BagDiameterCm, BagDiameterCm, BagLengthCm);
		Target->bShowGuide = true;
		Target->FinishSpawning(DesiredGoal);
		Target->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);
	}
	UStaticMesh* Cylinder = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (!Cylinder) { RecordEvent(TEXT("Engine Cylinder mesh unavailable")); return; }
	const FVector Extent = Cylinder->GetBoundingBox().GetExtent();
	if (Extent.GetMin() <= KINDA_SMALL_NUMBER) { RecordEvent(TEXT("Engine Cylinder has invalid bounds")); return; }
	FTransform Start = BagStartPose();
	const FVector BagScale(BagDiameterCm / (2 * Extent.X), BagDiameterCm / (2 * Extent.Y), BagLengthCm / (2 * Extent.Z));
	Start.SetScale3D(BagScale);
	Bag = World->SpawnActorDeferred<AVBoRCarryObject>(BagClass, Start, this, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (Bag)
	{
		Bag->LoadTarget = Target;
		Bag->Mesh->SetStaticMesh(Cylinder);
		// Mesh 가 Actor root 임. FinishSpawning 에도 원통 scale 을 넘겨 형상 유지 (P1 D3)
		Bag->FinishSpawning(Start);
		Bag->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);
		Bag->OnGrabbed.AddUniqueDynamic(this, &AP1LoadBench::HandleGrabbed);
		Bag->OnCarryReleased.AddUniqueDynamic(this, &AP1LoadBench::HandleCarryReleased);
		Bag->OnSeated.AddUniqueDynamic(this, &AP1LoadBench::HandleSeated);
	}
}

void AP1LoadBench::UpdateCrossing()
{
	if (!Bag || !Bag->Mesh || !Bag->Mesh->GetStaticMesh() || !Trunk) { return; }
	const FBox LocalBox = Bag->Mesh->GetStaticMesh()->GetBoundingBox();
	const FTransform MeshToWorld = Bag->Mesh->GetComponentTransform();
	float MinX = TNumericLimits<float>::Max();
	float MaxX = -TNumericLimits<float>::Max();
	for (int32 X = 0; X < 2; ++X)
	for (int32 Y = 0; Y < 2; ++Y)
	for (int32 Z = 0; Z < 2; ++Z)
	{
		const FVector Corner(X ? LocalBox.Max.X : LocalBox.Min.X,
			Y ? LocalBox.Max.Y : LocalBox.Min.Y, Z ? LocalBox.Max.Z : LocalBox.Min.Z);
		const float TrunkX = Trunk->GetActorTransform().InverseTransformPosition(MeshToWorld.TransformPosition(Corner)).X;
		MinX = FMath::Min(MinX, TrunkX);
		MaxX = FMath::Max(MaxX, TrunkX);
	}
	const bool bBlocked = Bag->IsBlocked();
	const bool bBodyAtPlane = MinX <= 0.0f && MaxX >= -2.0f;
	if (!bCrossingRecorded && bPreviousBodyBeforeOpening && MaxX >= 0.0f)
	{
		bCrossingRecorded = true;
		++EntranceCrossings;
		RecordEvent(FString::Printf(TEXT("Entrance crossing #%d, blocked=%d, bag bounds X [%.1f, %.1f] cm"),
			EntranceCrossings, bBlocked ? 1 : 0, MinX, MaxX));
	}
	if (!bBlockedAtCrossingRecorded && bPreviousBodyBeforeOpening && bBlocked && bBodyAtPlane)
	{
		bBlockedAtCrossingRecorded = true;
		++BlockedAtCrossings;
		RecordEvent(FString::Printf(TEXT("Blocked at entrance #%d, bag bounds X [%.1f, %.1f] cm"),
			BlockedAtCrossings, MinX, MaxX));
	}
	if (MaxX < -2.0f)
	{
		bPreviousBodyBeforeOpening = true;
		bCrossingRecorded = false;
		bBlockedAtCrossingRecorded = false;
	}
}

void AP1LoadBench::UpdatePanel()
{
	if (!Panel) { return; }
	const FString Rig = HandPawn ? HandPawn->DescribeRig() : TEXT("Hand rig unavailable");
	const FString StaticFit = Target && Bag ? (Target->IsStaticFit(Bag) ? TEXT("clear") : TEXT("overlap / does not fit")) : TEXT("n/a");
	const FString CurrentFit = Target && Bag ? (Target->IsObjectInside(Bag) ? TEXT("inside and clear") : TEXT("not inside / overlap")) : TEXT("n/a");
	FString Text = FString::Printf(TEXT("P1 | not-tested | Approximate geometry - not vehicle data\n"
		"Rig: %s\nStatic target: %s | current: %s\nEntrance crossing: %d | blocked at crossing: %d\n"
		"Bag: %s\n%s\nR: reset and re-place"), *Rig, *StaticFit, *CurrentFit,
		EntranceCrossings, BlockedAtCrossings, Bag ? *Bag->GetStateText() : TEXT("unavailable"),
		Bag ? *Bag->GetMeasurementText() : TEXT("measurements unavailable"));
	Text += TEXT("\nRecent events:");
	for (const FString& Event : RecentEvents) { Text += TEXT("\n") + Event; }
	Panel->SetText(FText::FromString(Text));
	if (GetWorld() && GetWorld()->GetTimeSeconds() - LastLogAt >= 1.0)
	{
		LastLogAt = GetWorld()->GetTimeSeconds();
		FString Line = Text;
		Line.ReplaceInline(TEXT("\n"), TEXT(" | "));
		UE_LOG(LogP1Probe, Log, TEXT("%s"), *Line);
	}
}

void AP1LoadBench::RecordEvent(const FString& Event)
{
	LastEvent = FString::Printf(TEXT("[%.3f s] %s"), GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0, *Event);
	UE_LOG(LogP1Probe, Log, TEXT("%s"), *LastEvent);
	RecentEvents.Add(LastEvent);
	if (RecentEvents.Num() > 6) { RecentEvents.RemoveAt(0); }
}

void AP1LoadBench::RecordSnapshot(const FString& Reason)
{
	if (!Bag) { return; }
	FString Snapshot = FString::Printf(TEXT("%s | state=%s | static fit=%d | current fit=%d | entrance crossings=%d | blocked at crossing=%d | %s"),
		*Reason, *Bag->GetStateText(), Target && Target->IsStaticFit(Bag) ? 1 : 0,
		Target && Target->IsObjectInside(Bag) ? 1 : 0, EntranceCrossings, BlockedAtCrossings, *Bag->GetMeasurementText());
	Snapshot.ReplaceInline(TEXT("\n"), TEXT(" | "));
	UE_LOG(LogP1Probe, Log, TEXT("%s"), *Snapshot);
	UpdatePanel();
}

void AP1LoadBench::HandleGrabbed(AVBoRCarryObject* Object)
{
	if (Object != Bag) { return; }
	RecordEvent(TEXT("Palm grab"));
	RecordSnapshot(TEXT("Grab snapshot"));
}

void AP1LoadBench::HandleCarryReleased(AVBoRCarryObject* Object)
{
	if (Object != Bag) { return; }
	RecordEvent(TEXT("Carry released"));
	RecordSnapshot(TEXT("Release snapshot"));
}

void AP1LoadBench::HandleSeated(AVBoRCarryObject* Object)
{
	if (Object != Bag) { return; }
	RecordEvent(TEXT("Bag seated"));
	RecordSnapshot(TEXT("Final placement snapshot"));
}
