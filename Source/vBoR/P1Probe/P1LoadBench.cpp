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

void FP1TrialTotals::Add(const FVBoRCarryMetrics& Metrics)
{
	Grabbed += Metrics.Grabbed;
	TwoHandEntries += Metrics.TwoHandEntries;
	TwoHandSeconds += Metrics.TwoHandSeconds;
	BlockedCount += Metrics.BlockedCount;
	PauseCount += Metrics.PauseCount;
	HeldSeconds += Metrics.HeldSeconds;
	BlockedSeconds += Metrics.BlockedSeconds;
	PausedSeconds += Metrics.PausedSeconds;
	Assisted += Metrics.Assisted;
	DroppedInside += Metrics.DroppedInside;
	DroppedOutside += Metrics.DroppedOutside;
}

FP1TrialTotals AP1LoadBench::GetTotals() const
{
	FP1TrialTotals Total = CompletedTrials;
	if (Bag) { Total.Add(Bag->GetMetrics()); }
	return Total;
}

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
		InputComponent->BindKey(BagResetKey, IE_Pressed, this, &AP1LoadBench::ResetBagForNextTrial);
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

void AP1LoadBench::ResetCrossing()
{
	bPreviousBodyBeforeOpening = false;
	bCrossingRecorded = false;
	bBlockedAtCrossingRecorded = false;
	EntranceCrossings = BlockedAtCrossings = 0;
}

void AP1LoadBench::ResetBagForNextTrial()
{
	if (!bPlaced || !Bag) { return; }
	// 직전 시도를 먼저 보존함 (B 는 자리와 누적을 유지, P1 R1-6)
	RecordSnapshot(FString::Printf(TEXT("Trial %d summary"), TrialNumber));
	CompletedTrials.Add(Bag->GetMetrics());
	Bag->ResetToPose(BagStartPose());
	++TrialNumber;
	ResetCrossing();
	LastObservedState = Bag->GetStateText();
	RecordEvent(FString::Printf(TEXT("Trial %d: bag returned to stand; bench unchanged"), TrialNumber));
	UpdatePanel();
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
	if (!HandPawn) { HandPawn = Cast<AVBoRHandRigPawn>(UGameplayStatics::GetPlayerPawn(this, 0)); }
	if (!HandPawn) { RecordEvent(TEXT("Placement waiting for hand pawn")); return; }
	FVector Origin = Head.GetLocation() + Yaw.RotateVector(FVector(OpeningForwardCm, 0, 0));
	// Stage 의 바닥은 Pawn 원점. 머리 높이로 들기 높이를 바꾸지 않음 (P1 R1-3)
	Origin.Z = HandPawn->GetActorLocation().Z;
	if (Bag) { Bag->ResetToPose(Bag->GetActorTransform()); } // 손 세션을 먼저 끝냄 (P1 §2 R)
	SetActorTransform(FTransform(Yaw, Origin));
	Stand->SetRelativeLocation(FVector(-StandBehindOpeningCm, 0, StandTopHeightCm - StandSizeCm.Z * 0.5f));
	Stand->SetRelativeScale3D(StandSizeCm / 100.0f);
	if (!bPlaced) { SpawnParts(); }
	if (Target) { Target->SetActorTransform(GoalPose()); }
	if (Bag) { Bag->ResetToPose(BagStartPose()); }
	// 초기 배치와 R 을 입구 통과로 세지 않음 (P1 §2 경로)
	ResetCrossing();
	TrialNumber = 1;
	CompletedTrials = FP1TrialTotals();
	RecentEvents.Reset();
	LastObservedState = Bag ? Bag->GetStateText() : FString();
	bPlaced = true;
	Panel->SetVisibility(true);
	RecordEvent(TEXT("Bench placed/reset at head yaw; opening +130 cm; stand -100 cm; geometry approximate"));
	RecordEvent(FString::Printf(TEXT("Physical floor reference: Pawn origin Z=%.2f cm; trial=1; totals cleared"), Origin.Z));
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
	if (!Bag || !Trunk) { bPreviousBodyBeforeOpening = false; return; }
	// 충돌·안착과 같은 형상으로 입구를 잼 (원통 축 회전 때 외접 상자가 먼저 통과하지 않게, P1 R3 F1)
	const FBox Bounds = Bag->GetCollisionBoundsInPose(Bag->GetActorTransform(), Trunk->GetActorTransform());
	if (!Bounds.IsValid)
	{
		// 형상이 없던 구간을 통과로 연결하지 않음. 누적 수는 유지 (P1 R3 F1)
		bPreviousBodyBeforeOpening = false;
		return;
	}
	const double MinX = Bounds.Min.X;
	const double MaxX = Bounds.Max.X;
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
	const FP1TrialTotals Total = GetTotals();
	FString Text = FString::Printf(TEXT("P1 | not-tested | Approximate geometry - not vehicle data\n"
		"Rig: %s\nStatic target: %s | current: %s\nEntrance crossing: %d | blocked at crossing: %d\n"
		"Bag: %s\n%s\nR: reset and re-place | B: next trial, bag only"), *Rig, *StaticFit, *CurrentFit,
		EntranceCrossings, BlockedAtCrossings, Bag ? *Bag->GetStateText() : TEXT("unavailable"),
		Bag ? *Bag->GetMeasurementText() : TEXT("measurements unavailable"));
	Text += FString::Printf(TEXT("\nTrial: %d | Total grabbed=%d, assisted=%d, drop inside=%d, outside=%d\nTotal held %.3f s | blocked=%d / %.3f s | pauses=%d / %.3f s"),
		TrialNumber, Total.Grabbed, Total.Assisted, Total.DroppedInside, Total.DroppedOutside, Total.HeldSeconds,
		Total.BlockedCount, Total.BlockedSeconds, Total.PauseCount, Total.PausedSeconds);
	Text += FString::Printf(TEXT("\nTotal two-hand entries=%d / %.3f s"), Total.TwoHandEntries, Total.TwoHandSeconds);
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
	const FVBoRCarryMetrics Metrics = Bag->GetMetrics();
	FString Snapshot = FString::Printf(TEXT("%s | state=%s | static fit=%d | current fit=%d | entrance crossings=%d | blocked at crossing=%d | %s"),
		*Reason, *Bag->GetStateText(), Target && Target->IsStaticFit(Bag) ? 1 : 0,
		Target && Target->IsObjectInside(Bag) ? 1 : 0, EntranceCrossings, BlockedAtCrossings, *Bag->GetMeasurementText());
	Snapshot += FString::Printf(TEXT(" | Trial %d totals: grabbed=%d, assisted=%d, drop inside=%d, outside=%d, held=%.3f s"),
		TrialNumber, Metrics.Grabbed, Metrics.Assisted, Metrics.DroppedInside, Metrics.DroppedOutside, Metrics.HeldSeconds);
	Snapshot += FString::Printf(TEXT(" | two-hand entries=%d / %.3f s"), Metrics.TwoHandEntries, Metrics.TwoHandSeconds);
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
