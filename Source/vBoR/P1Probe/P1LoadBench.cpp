#include "P1LoadBench.h"

#include "P1TrunkMock.h"
#include "../Interaction/VBoRCarryObject.h"
#include "../Interaction/VBoRLoadTarget.h"
#include "../Interaction/VBoRHandRigPawn.h"
#include "Camera/CameraComponent.h"
#include "Components/InputComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
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
	for (const AVBoRCarryObject* Box : Boxes) { if (IsValid(Box)) { Total.Add(Box->GetMetrics()); } }
	return Total;
}

int32 AP1LoadBench::GetRestingBoxCount() const
{
	int32 Count = 0;
	for (const AVBoRCarryObject* Box : Boxes)
	{
		if (IsValid(Box) && Box->IsRestingInside() && Target && Target->IsObjectInside(Box)) { ++Count; }
	}
	return Count;
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
	ClearBoxes(false);
	for (AActor* Part : {static_cast<AActor*>(Target.Get()), static_cast<AActor*>(Trunk.Get())})
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
		ReplenishStand();
		FString State;
		for (const AVBoRCarryObject* Box : Boxes)
		{
			if (!IsValid(Box)) { continue; }
			State += Box->GetName() + TEXT(": ") + Box->GetStateText() + TEXT(" | ");
		}
		if (State != LastObservedState)
		{
			RecordEvent(FString::Printf(TEXT("Box states: %s"), *State));
			LastObservedState = State;
			RecordSnapshot(TEXT("State transition snapshot"));
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
	Crossings.Reset();
	EntranceCrossings = BlockedAtCrossings = 0;
}

void AP1LoadBench::ResetBagForNextTrial()
{
	if (!bPlaced) { return; }
	// 직전 시도를 먼저 보존함 (B 는 자리와 누적을 유지, P1 R1-6)
	RecordSnapshot(FString::Printf(TEXT("Trial %d summary"), TrialNumber));
	ClearBoxes(true);
	SpawnBox();
	++TrialNumber;
	ResetCrossing();
	LastObservedState.Reset();
	RecordEvent(FString::Printf(TEXT("Trial %d: all boxes cleared; one box on stand; bench unchanged"), TrialNumber));
	UpdatePanel();
}

FTransform AP1LoadBench::BagStartPose() const
{
	// 새 박스는 받침 윗면보다 0.1 mm 위에 둠 (접촉 수치 오차로 출고가 막히지 않게, P1b D5)
	return FTransform(GetActorQuat(), GetActorTransform().TransformPosition(
		FVector(-StandBehindOpeningCm, 0, StandTopHeightCm + BoxSizeCm.Z * 0.5f + 0.01f)));
}

FTransform AP1LoadBench::GoalPose() const
{
	const float FloorZ = Trunk ? Trunk->FloorHeightCm : 75.0f;
	return FTransform(GetActorQuat(), GetActorTransform().TransformPosition(
		FVector(GoalOffsetCm.X, GoalOffsetCm.Y, FloorZ + GoalOffsetCm.Z)));
}

void AP1LoadBench::PlaceFromHead(const FTransform& Head)
{
	const FRotator Yaw(0, Head.Rotator().Yaw, 0);
	if (!HandPawn) { HandPawn = Cast<AVBoRHandRigPawn>(UGameplayStatics::GetPlayerPawn(this, 0)); }
	if (!HandPawn) { RecordEvent(TEXT("Placement waiting for hand pawn")); return; }
	BoxSizeCm = BoxSizeCm.GetAbs().ComponentMax(FVector(1));
	FVector Origin = Head.GetLocation() + Yaw.RotateVector(FVector(OpeningForwardCm, 0, 0));
	// Stage 의 바닥은 Pawn 원점. 머리 높이로 들기 높이를 바꾸지 않음 (P1 R1-3)
	Origin.Z = HandPawn->GetActorLocation().Z;
	ClearBoxes(false);
	SetActorTransform(FTransform(Yaw, Origin));
	Stand->SetRelativeLocation(FVector(-StandBehindOpeningCm, 0, StandTopHeightCm - StandSizeCm.Z * 0.5f));
	Stand->SetRelativeScale3D(StandSizeCm / 100.0f);
	if (!bPlaced) { SpawnParts(); }
	if (Target) { Target->SetActorTransform(GoalPose()); }
	if (Target && Trunk)
	{
		// 바닥 윗면부터 천장 아랫면까지, 입구에서 안쪽 벽까지의 공간 (P1 R4-1, cm)
		const FVector Size(FMath::Max(1.0f, Trunk->InsideDepthCm), FMath::Max(1.0f, Trunk->InsideWidthCm), FMath::Max(1.0f, Trunk->InsideHeightCm));
		const FVector Center(Size.X * 0.5, 0, Trunk->FloorHeightCm + Size.Z * 0.5);
		const FTransform RegionPose(Trunk->GetActorQuat(), Trunk->GetActorTransform().TransformPosition(Center));
		Target->LoadingRegion->SetBoxExtent(Size * 0.5);
		Target->LoadingRegion->SetWorldTransform(RegionPose);
	}
	if (Target)
	{
		Target->GoalSizeCm = BoxSizeCm;
		Target->OnConstruction(Target->GetActorTransform());
	}
	SpawnBox();
	// 초기 배치와 R 을 입구 통과로 세지 않음 (P1 §2 경로)
	ResetCrossing();
	TrialNumber = 1;
	CompletedTrials = FP1TrialTotals();
	RecentEvents.Reset();
	LastObservedState.Reset();
	bPlaced = true;
	Panel->SetVisibility(true);
	RecordEvent(TEXT("Bench placed/reset at head yaw; opening +130 cm; stand -100 cm; geometry approximate"));
	RecordEvent(FString::Printf(TEXT("Physical floor reference: Pawn origin Z=%.2f cm; trial=1; totals cleared"), Origin.Z));
	RecordEvent(FString::Printf(TEXT("Static target fit=%d; approximate box %s cm; assistance off"),
		Target && Bag && Target->IsStaticFit(Bag) ? 1 : 0, *BoxSizeCm.ToString()));
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
		Target->GoalSizeCm = BoxSizeCm;
		Target->bShowGuide = false;
		Target->FinishSpawning(DesiredGoal);
		Target->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);
	}
}

void AP1LoadBench::SpawnBox()
{
	const FTransform Start = BagStartPose();
	UWorld* World = GetWorld();
	if (!World) { return; }
	Bag = World->SpawnActorDeferred<AVBoRCarryObject>(BagClass, Start, this, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (Bag)
	{
		Bag->BoxSizeCm = BoxSizeCm;
		Bag->LoadTarget = Target;
		Bag->bUsePlacementAssist = false;
		Bag->bDestroyAtFloor = true;
		Bag->FloorHeightCm = GetActorLocation().Z;
		Bag->FinishSpawning(Start);
		Bag->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);
		Boxes.Add(Bag);
		AddTickPrerequisiteActor(Bag);
		Bag->OnGrabbed.AddUniqueDynamic(this, &AP1LoadBench::HandleGrabbed);
		Bag->OnCarryReleased.AddUniqueDynamic(this, &AP1LoadBench::HandleCarryReleased);
		Bag->OnSeated.AddUniqueDynamic(this, &AP1LoadBench::HandleSeated);
		Bag->OnDestroyed.AddUniqueDynamic(this, &AP1LoadBench::HandleBoxDestroyed);
	}
	bSourceLifted = false;
}

void AP1LoadBench::ClearBoxes(bool bPreserveMetrics)
{
	for (AVBoRCarryObject* Box : Boxes)
	{
		if (!IsValid(Box)) { continue; }
		if (bPreserveMetrics) { CompletedTrials.Add(Box->GetMetrics()); }
		Box->OnDestroyed.RemoveDynamic(this, &AP1LoadBench::HandleBoxDestroyed);
		Box->OnGrabbed.RemoveDynamic(this, &AP1LoadBench::HandleGrabbed);
		Box->OnCarryReleased.RemoveDynamic(this, &AP1LoadBench::HandleCarryReleased);
		Box->OnSeated.RemoveDynamic(this, &AP1LoadBench::HandleSeated);
		RemoveTickPrerequisiteActor(Box);
		Box->ResetToPose(Box->GetActorTransform());
		Box->Destroy();
	}
	Boxes.Reset();
	Crossings.Reset();
	Bag = nullptr;
	bSourceLifted = false;
}

void AP1LoadBench::ReplenishStand()
{
	if (!bSourceLifted || (IsValid(Bag) && Bag->GetCarrySession().bPaused)) { return; }
	// 출고 공간에 1 cm 여유가 생겨야 보충함 (손에 든 박스·되돌려 둔 박스와 겹치지 않게, P1b D5)
	const FTransform Start = BagStartPose();
	const FBox Slot(-BoxSizeCm * 0.5 - FVector(1), BoxSizeCm * 0.5 + FVector(1));
	for (const AVBoRCarryObject* Box : Boxes)
	{
		if (!IsValid(Box)) { continue; }
		const FBox Bounds = Box->GetCollisionBoundsInPose(Box->GetActorTransform(), Start);
		if (Bounds.IsValid && Bounds.Intersect(Slot)) { return; }
	}
	SpawnBox();
	RecordEvent(TEXT("Stand replenished: one approximate box"));
}

void AP1LoadBench::HandleBoxDestroyed(AActor* Object)
{
	AVBoRCarryObject* Box = Cast<AVBoRCarryObject>(Object);
	if (!Box || !Boxes.Contains(Box)) { return; }
	CompletedTrials.Add(Box->GetMetrics());
	RemoveTickPrerequisiteActor(Box);
	Crossings.Remove(Box);
	Boxes.Remove(Box);
	if (Box == Bag) { Bag = nullptr; }
	RecordEvent(TEXT("Released box reached physical floor; removed"));
}

void AP1LoadBench::UpdateCrossing()
{
	if (!Trunk) { return; }
	for (AVBoRCarryObject* Box : Boxes)
	{
		if (!IsValid(Box)) { continue; }
		FP1BoxCrossing& Crossing = Crossings.FindOrAdd(Box);
		// 충돌·안착과 같은 형상으로 입구를 잼 (원통 축 회전 때 외접 상자가 먼저 통과하지 않게, P1 R3 F1)
		const FBox Bounds = Box->GetCollisionBoundsInPose(Box->GetActorTransform(), Trunk->GetActorTransform());
		if (!Bounds.IsValid)
		{
			// 형상이 없던 구간을 통과로 연결하지 않음. 누적 수는 유지 (P1 R3 F1)
			Crossing.bPreviousBodyBeforeOpening = false;
			continue;
		}
		const double MinX = Bounds.Min.X;
		const double MaxX = Bounds.Max.X;
		const bool bBlocked = Box->IsBlocked();
		const bool bBodyAtPlane = MinX <= 0.0f && MaxX >= -2.0f;
		if (!Crossing.bCrossingRecorded && Crossing.bPreviousBodyBeforeOpening && MaxX >= 0.0f)
		{
			Crossing.bCrossingRecorded = true;
			++EntranceCrossings;
			RecordEvent(FString::Printf(TEXT("Entrance crossing #%d, blocked=%d, box bounds X [%.1f, %.1f] cm"),
				EntranceCrossings, bBlocked ? 1 : 0, MinX, MaxX));
		}
		if (!Crossing.bBlockedAtCrossingRecorded && Crossing.bPreviousBodyBeforeOpening && bBlocked && bBodyAtPlane)
		{
			Crossing.bBlockedAtCrossingRecorded = true;
			++BlockedAtCrossings;
			RecordEvent(FString::Printf(TEXT("Blocked at entrance #%d, box bounds X [%.1f, %.1f] cm"),
				BlockedAtCrossings, MinX, MaxX));
		}
		if (MaxX < -2.0f)
		{
			Crossing.bPreviousBodyBeforeOpening = true;
			Crossing.bCrossingRecorded = false;
			Crossing.bBlockedAtCrossingRecorded = false;
		}
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
		"Rig: %s\nStatic target pose: %s | loading region: %s\nEntrance crossing: %d | blocked at crossing: %d\n"
		"Stand box: %s\n%s\nR: reset and re-place | B: clear all boxes, refill stand"), *Rig, *StaticFit, *CurrentFit,
		EntranceCrossings, BlockedAtCrossings, Bag ? *Bag->GetStateText() : TEXT("unavailable"),
		Bag ? *Bag->GetMeasurementText() : TEXT("measurements unavailable"));
	Text += FString::Printf(TEXT("\nTrial: %d | Total grabbed=%d, assisted=%d, drop inside=%d, outside=%d\nTotal held %.3f s | blocked=%d / %.3f s | pauses=%d / %.3f s"),
		TrialNumber, Total.Grabbed, Total.Assisted, Total.DroppedInside, Total.DroppedOutside, Total.HeldSeconds,
		Total.BlockedCount, Total.BlockedSeconds, Total.PauseCount, Total.PausedSeconds);
	Text += FString::Printf(TEXT("\nTotal two-hand entries=%d / %.3f s"), Total.TwoHandEntries, Total.TwoHandSeconds);
	Text += FString::Printf(TEXT("\nBoxes=%d | resting inside trunk=%d | approximate box %s cm"), Boxes.Num(), GetRestingBoxCount(), *BoxSizeCm.ToString());
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
	const FP1TrialTotals Metrics = GetTotals();
	FString Snapshot = FString::Printf(TEXT("%s | state=%s | static goal pose fit=%d | loading region inside and clear=%d | entrance crossings=%d | blocked at crossing=%d | %s"),
		*Reason, *Bag->GetStateText(), Target && Target->IsStaticFit(Bag) ? 1 : 0,
		Target && Target->IsObjectInside(Bag) ? 1 : 0, EntranceCrossings, BlockedAtCrossings, *Bag->GetMeasurementText());
	Snapshot += FString::Printf(TEXT(" | Trial %d totals: grabbed=%d, assisted=%d, drop inside=%d, outside=%d, held=%.3f s"),
		TrialNumber, Metrics.Grabbed, Metrics.Assisted, Metrics.DroppedInside, Metrics.DroppedOutside, Metrics.HeldSeconds);
	Snapshot += FString::Printf(TEXT(" | two-hand entries=%d / %.3f s"), Metrics.TwoHandEntries, Metrics.TwoHandSeconds);
	Snapshot += FString::Printf(TEXT(" | boxes=%d | resting inside trunk=%d"), Boxes.Num(), GetRestingBoxCount());
	Snapshot.ReplaceInline(TEXT("\n"), TEXT(" | "));
	UE_LOG(LogP1Probe, Log, TEXT("%s"), *Snapshot);
	UpdatePanel();
}

void AP1LoadBench::HandleGrabbed(AVBoRCarryObject* Object)
{
	if (!Boxes.Contains(Object)) { return; }
	if (Object == Bag) { bSourceLifted = true; }
	RecordEvent(TEXT("Palm grab"));
	RecordSnapshot(TEXT("Grab snapshot"));
}

void AP1LoadBench::HandleCarryReleased(AVBoRCarryObject* Object)
{
	if (!Boxes.Contains(Object)) { return; }
	RecordEvent(TEXT("Carry released"));
	RecordSnapshot(TEXT("Release snapshot"));
}

void AP1LoadBench::HandleSeated(AVBoRCarryObject* Object)
{
	if (!Boxes.Contains(Object)) { return; }
	RecordEvent(TEXT("Box seated"));
	RecordSnapshot(TEXT("Final placement snapshot"));
}
