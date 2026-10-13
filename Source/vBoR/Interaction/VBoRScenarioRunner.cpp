#include "VBoRScenarioRunner.h"
#include "VBoRParticipantZones.h"
#include "VBoRVehicleAlignmentComponent.h"
#include "VBoRViewpointComponent.h"
#include "VBoRHingedDoor.h"
#include "VBoRPokeButton.h"
#include "Components/ArrowComponent.h"
#include "Components/InputComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Interaction/IsdkPokeInteractable.h"
#include "Kismet/GameplayStatics.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBorder.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	UObject* FindP3Tag(UWorld* World, FName Tag, FString& Problems)
	{
		if (Tag.IsNone()) { return nullptr; }
		TArray<UObject*> Matches;
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (It->ActorHasTag(Tag)) { Matches.Add(*It); }
			TArray<UActorComponent*> Components; It->GetComponents(Components);
			for (UActorComponent* Component : Components) { if (Component->ComponentHasTag(Tag)) { Matches.Add(Component); } }
		}
		if (Matches.Num() != 1) { Problems += FString::Printf(TEXT("태그 %s: %d개 발견 (1개 필요)\n"), *Tag.ToString(), Matches.Num()); return nullptr; }
		return Matches[0];
	}
	template<class T> T* P3Component(UObject* Object)
	{
		if (T* Component = Cast<T>(Object)) { return Component; }
		if (AActor* Actor = Cast<AActor>(Object))
		{
			TArray<T*> Components; Actor->GetComponents(Components);
			return Components.Num() == 1 ? Components[0] : nullptr;
		}
		return nullptr;
	}
	AActor* P3Actor(UObject* Object)
	{
		if (AActor* Actor = Cast<AActor>(Object)) { return Actor; }
		if (UActorComponent* Component = Cast<UActorComponent>(Object)) { return Component->GetOwner(); }
		return nullptr;
	}
	USceneComponent* P3Scene(UObject* Object)
	{
		if (USceneComponent* Component = Cast<USceneComponent>(Object)) { return Component; }
		AActor* Actor = P3Actor(Object); return Actor ? Actor->GetRootComponent() : nullptr;
	}
	void SetP3ButtonVisible(AVBoRPokeButton* Button, bool bVisible)
	{
		if (!Button) { return; }
		Button->SetActorHiddenInGame(!bVisible);
		// 보이지 않는 버튼이 손 입력을 받지 않게 ISDK 도 함께 끔 (P3 §2)
		if (bVisible) { Button->Poke->Activate(false); } else { Button->Poke->Deactivate(); }
	}
	TSharedRef<SBorder> P3TextPlate(const TSharedRef<STextBlock>& Text)
	{
		return SNew(SBorder).Padding(24).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
			.BorderBackgroundColor(FLinearColor(0.95, 0.95, 0.95, 1))[Text];
	}
}

AVBoRScenarioRunner::AVBoRScenarioRunner()
{
	ButtonDistanceCm = 45; ButtonBelowCm = 30; ConfirmLockSeconds = 0.6; SeatedPanelSeconds = 1.0;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("FlowOrigin")));
	PrimaryActorTick.bCanEverTick = true; PrimaryActorTick.TickGroup = TG_PostUpdateWork;
	Panel = CreateDefaultSubobject<UWidgetComponent>(TEXT("KoreanGuidance")); Panel->SetupAttachment(RootComponent);
	Panel->SetWidgetSpace(EWidgetSpace::World); Panel->SetDrawSize(FVector2D(1000, 640)); Panel->SetTwoSided(true);
	Panel->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	OperatorPanel = CreateDefaultSubobject<UWidgetComponent>(TEXT("OperatorInfo")); OperatorPanel->SetupAttachment(RootComponent);
	OperatorPanel->SetWidgetSpace(EWidgetSpace::World); OperatorPanel->SetDrawSize(FVector2D(1000, 700)); OperatorPanel->SetTwoSided(true);
	OperatorPanel->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	for (int32 I = 0; I < 2; ++I)
	{
		UWidgetComponent* Label = CreateDefaultSubobject<UWidgetComponent>(*FString::Printf(TEXT("ButtonLabel%d"), I));
		Label->SetupAttachment(RootComponent); Label->SetWidgetSpace(EWidgetSpace::World); Label->SetDrawSize(FVector2D(500, 120));
		Label->SetTwoSided(true); Label->SetCollisionEnabled(ECollisionEnabled::NoCollision); ButtonLabels.Add(Label);
	}
	MarkerArrow = CreateDefaultSubobject<UArrowComponent>(TEXT("TargetArrow")); MarkerArrow->SetupAttachment(RootComponent);
	MarkerArrow->ArrowColor = FColor::Yellow; MarkerArrow->SetHiddenInGame(true);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	// 네 변으로 바닥 고리 표시함 (별도 렌더링 구조 없이 위치 유도, P3 §5)
	for (int32 I = 0; I < 4; ++I)
	{
		UStaticMeshComponent* Part = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("MarkerRing%d"), I));
		Part->SetupAttachment(RootComponent); Part->SetStaticMesh(Cube.Object); Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Part->SetHiddenInGame(true); MarkerRing.Add(Part);
	}
}

void AVBoRScenarioRunner::BeginPlay()
{
	Super::BeginPlay();
	PanelText = SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Bold", 40)).ColorAndOpacity(FLinearColor::Black).AutoWrapText(true);
	PanelBorder = P3TextPlate(PanelText.ToSharedRef()); Panel->SetSlateWidget(PanelBorder);
	Panel->SetWorldScale3D(FVector(1, PanelSizeCm.X / 1000.0, PanelSizeCm.Y / 640.0));
	OperatorText = SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Bold", 24)).ColorAndOpacity(FLinearColor::Black).AutoWrapText(true);
	OperatorPanel->SetSlateWidget(P3TextPlate(OperatorText.ToSharedRef())); OperatorPanel->SetWorldScale3D(FVector(1, 0.05, 0.05));
	for (int32 I = 0; I < 2; ++I)
	{
		// 메시·판정 영역 크기를 함께 정하고 실제 poke 경로를 재사용함 (P3 D4)
		AVBoRPokeButton* Button = GetWorld()->SpawnActorDeferred<AVBoRPokeButton>(AVBoRPokeButton::StaticClass(), FTransform::Identity, this);
		Button->ButtonMesh->SetRelativeScale3D(FVector(0.08, 0.08, 0.015)); Button->HitAreaScale = 1;
		Button->FinishSpawning(FTransform::Identity); Button->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepWorldTransform);
		if (I == 0) { ConfirmButton = Button; Button->OnButtonPressed.AddUniqueDynamic(this, &AVBoRScenarioRunner::HandleConfirmPoke); }
		else { ActionButton = Button; Button->OnButtonPressed.AddUniqueDynamic(this, &AVBoRScenarioRunner::HandleActionPoke); }
		SetP3ButtonVisible(Button, false);
		TSharedRef<STextBlock> Label = SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Bold", 32)).ColorAndOpacity(FLinearColor::Black).AutoWrapText(true);
		ButtonLabelTexts.Add(Label); ButtonLabels[I]->SetSlateWidget(P3TextPlate(Label)); ButtonLabels[I]->SetWorldScale3D(FVector(1, 0.04, 0.04));
	}
	if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
	{
		EnableInput(PC);
		InputComponent->BindKey(EKeys::SpaceBar, IE_Pressed, this, &AVBoRScenarioRunner::KeyConfirm);
		InputComponent->BindKey(EKeys::Right, IE_Pressed, this, &AVBoRScenarioRunner::KeyNext);
		InputComponent->BindKey(EKeys::Left, IE_Pressed, this, &AVBoRScenarioRunner::KeyPrevious);
		InputComponent->BindKey(EKeys::One, IE_Pressed, this, &AVBoRScenarioRunner::KeyPassenger);
		InputComponent->BindKey(EKeys::Two, IE_Pressed, this, &AVBoRScenarioRunner::KeyRear);
		InputComponent->BindKey(EKeys::Home, IE_Pressed, this, &AVBoRScenarioRunner::KeyReturn);
		InputComponent->BindKey(EKeys::F5, IE_Pressed, this, &AVBoRScenarioRunner::KeyRestart);
		InputComponent->BindKey(EKeys::O, IE_Pressed, this, &AVBoRScenarioRunner::KeyIgnore);
		InputComponent->BindKey(EKeys::F1, IE_Pressed, this, &AVBoRScenarioRunner::KeyOperatorInfo);
	}
	PlacePanel();
}

void AVBoRScenarioRunner::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	for (const auto& Entry : ButtonCounts)
	{ if (Entry.Key.IsValid()) { Entry.Key->OnButtonPressed.RemoveDynamic(this, &AVBoRScenarioRunner::HandleTargetPoke); } }
	if (ConfirmButton) { ConfirmButton->Destroy(); }
	if (ActionButton) { ActionButton->Destroy(); }
	if (Alignment) { Alignment->OnPawnMoved.RemoveAll(this); }
	Panel->SetSlateWidget(nullptr); PanelText.Reset(); PanelBorder.Reset();
	OperatorPanel->SetSlateWidget(nullptr); OperatorText.Reset();
	for (auto& Label : ButtonLabels) { Label->SetSlateWidget(nullptr); } ButtonLabelTexts.Reset();
	if (InputComponent) { InputComponent->KeyBindings.Reset(); }
	Super::EndPlay(EndPlayReason);
}

bool AVBoRScenarioRunner::InitializeFlow()
{
	bInitialized = true; bRunning = false; bFinished = false; ValidationProblems.Reset(); Targets.Reset(); Markers.Reset();
	for (const auto& Entry : ButtonCounts)
	{ if (Entry.Key.IsValid()) { Entry.Key->OnButtonPressed.RemoveDynamic(this, &AVBoRScenarioRunner::HandleTargetPoke); } }
	ButtonCounts.Reset();
	if (Alignment) { Alignment->OnPawnMoved.RemoveAll(this); }
	Alignment = nullptr; Approach = nullptr; Seat = nullptr;
	int32 AlignmentCount = 0;
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		TArray<UVBoRVehicleAlignmentComponent*> Components; It->GetComponents(Components);
		for (UVBoRVehicleAlignmentComponent* C : Components) { Alignment = C; ++AlignmentCount; }
	}
	if (AlignmentCount != 1) { ValidationProblems += TEXT("정렬 컴포넌트 1개 필요\n"); }
	if (!Scenario || Scenario->Steps.IsEmpty()) { ValidationProblems += TEXT("시나리오 단계 없음\n"); }
	else for (const FVBoRScenarioStep& Step : Scenario->Steps)
	{
		UObject* Raw = FindP3Tag(GetWorld(), Step.TargetTag, ValidationProblems);
		UObject* Target = nullptr;
		switch (Step.Rule)
		{
		case EVBoRStepRule::Confirm: Target = Raw; break;
		case EVBoRStepRule::Approach: Target = P3Component<UVBoRApproachZoneComponent>(Raw); if (Target) { Approach = Cast<UVBoRApproachZoneComponent>(Target); } break;
		case EVBoRStepRule::Seated: Target = P3Component<UVBoRSeatZoneComponent>(Raw); if (Target) { Seat = Cast<UVBoRSeatZoneComponent>(Target); } break;
		case EVBoRStepRule::ButtonPressed: Target = Cast<AVBoRPokeButton>(P3Actor(Raw)); break;
		case EVBoRStepRule::DoorUnlocked: case EVBoRStepRule::DoorOpened: case EVBoRStepRule::DoorLatched: Target = Cast<AVBoRHingedDoor>(P3Actor(Raw)); break;
		case EVBoRStepRule::Viewpoint: Target = P3Component<UVBoRViewpointComponent>(Raw); break;
		}
		if (Step.Rule != EVBoRStepRule::Confirm && !Target) { ValidationProblems += FString::Printf(TEXT("%s: 태그 %s 종류 불일치 또는 없음\n"), *Step.StepId.ToString(), *Step.TargetTag.ToString()); }
		const FString Id = Step.StepId.ToString(); const int32 Number = FCString::Atoi(*Id);
		if (Number < 1 || Number > 20 || Number == 17 || Id != FString::Printf(TEXT("%02d"), Number)) { ValidationProblems += TEXT("원본 StepId 필요 (01–16, 18–20)\n"); }
		Targets.Add(Target);
		UObject* Marker = FindP3Tag(GetWorld(), Step.MarkerTag, ValidationProblems);
		USceneComponent* Scene = P3Scene(Marker); Markers.Add(Scene);
		if (!Step.MarkerTag.IsNone() && !Scene) { ValidationProblems += FString::Printf(TEXT("%s: 표시 태그 종류 불일치\n"), *Step.StepId.ToString()); }
		if (AVBoRPokeButton* Button = Cast<AVBoRPokeButton>(Target))
		{
			ButtonCounts.FindOrAdd(Button) = 0; Button->OnButtonPressed.AddUniqueDynamic(this, &AVBoRScenarioRunner::HandleTargetPoke);
		}
	}
	if (Alignment)
	{
		AddTickPrerequisiteComponent(Alignment);
		Alignment->OnPawnMoved.AddUObject(this, &AVBoRScenarioRunner::HandlePawnMoved);
		if (!Seat) { Seat = Alignment->GetOwner()->FindComponentByClass<UVBoRSeatZoneComponent>(); }
	}
	if (Approach) { AddTickPrerequisiteComponent(Approach); }
	if (Seat) { AddTickPrerequisiteComponent(Seat); }
	if (!ValidationProblems.IsEmpty()) { Record(TEXT("시작 차단:\n") + ValidationProblems); RefreshPanel(); return false; }
	if (Alignment->State != EVBoRAlignmentState::Applied && !bAlignmentIgnored)
	{ Record(TEXT("정렬 필요 — P0-c 레벨에서 C로 저장; O로 무시 가능 | ") + Alignment->Describe()); RefreshPanel(); return false; }
	bRunning = true; EnterStep(0); RefreshPanel(); return true;
}

const FVBoRScenarioStep* AVBoRScenarioRunner::CurrentStep() const
{ return Scenario && Scenario->Steps.IsValidIndex(StepIndex) ? &Scenario->Steps[StepIndex] : nullptr; }

bool AVBoRScenarioRunner::RuleSatisfied() const
{
	const auto* Step = CurrentStep(); if (!Step || !Targets.IsValidIndex(StepIndex)) { return false; }
	if (Alignment && Alignment->IsSwitching()) { return false; }
	if (Step->bRequireBaseViewpoint && (!Alignment || !Alignment->IsBaseViewpoint())) { return false; }
	UObject* Target = Targets[StepIndex];
	switch (Step->Rule)
	{
	case EVBoRStepRule::Confirm: return true;
	case EVBoRStepRule::Approach: { const auto* C = Cast<UVBoRApproachZoneComponent>(Target); return C && C->IsParticipantNear(); }
	case EVBoRStepRule::Seated: { const auto* C = Cast<UVBoRSeatZoneComponent>(Target); return C && C->DwellSeconds >= Step->SeatedSeconds; }
	case EVBoRStepRule::ButtonPressed: return ButtonCounts.FindRef(Cast<AVBoRPokeButton>(Target)) > ButtonBaseline;
	case EVBoRStepRule::DoorUnlocked: { const auto* C = Cast<AVBoRHingedDoor>(Target); return C && !C->GetMotion().bLocked; }
	case EVBoRStepRule::DoorOpened: { const auto* C = Cast<AVBoRHingedDoor>(Target); return C && C->GetMotion().AngleDeg >= Step->DoorOpenDeg; }
	case EVBoRStepRule::DoorLatched: { const auto* C = Cast<AVBoRHingedDoor>(Target); return C && C->GetMotion().bLocked && C->GetMotion().AngleDeg <= C->LatchAngleDeg; }
	case EVBoRStepRule::Viewpoint: return Alignment && Alignment->IsAtViewpoint(Cast<UVBoRViewpointComponent>(Target));
	}
	return false;
}

bool AVBoRScenarioRunner::NeedsConfirmation() const
{
	const auto* Step = CurrentStep();
	return Step && (Step->ConfirmBy != EVBoRConfirmBy::None || Step->Rule == EVBoRStepRule::Confirm || Step->Rule == EVBoRStepRule::Viewpoint);
}

bool AVBoRScenarioRunner::CanParticipantConfirm() const
{
	const auto* Step = CurrentStep();
	return bRunning && !bPanelPlacementPending && ConfirmLockRemaining <= 0 && Step && NeedsConfirmation() && Step->ConfirmBy != EVBoRConfirmBy::Operator && RuleSatisfied();
}

void AVBoRScenarioRunner::EnterStep(int32 Index)
{
	StepIndex = Index; ++Generation; StepElapsed = 0; ConfirmLockRemaining = FMath::Max(0.0f, ConfirmLockSeconds); bSeatHeadRecorded = false;
	if (Index == 0) { bStartHeadPending = true; }
	if (!CurrentStep())
	{
		bRunning = false; bFinished = true;
		Record(FString::Printf(TEXT("안내 완료 | 요약 이전 %d 재시도 %d 건너뛰기 %d | 버림 지난단계 %d 권한 %d 규칙 %d 전환 %d"), PreviousCount, RetryCount, SkipCount, StaleDiscards, PermissionDiscards, RuleDiscards, TransitionDiscards));
	}
	else
	{
		ButtonBaseline = ButtonCounts.FindRef(Cast<AVBoRPokeButton>(Targets[Index]));
		Record(FString::Printf(TEXT("단계 시작 %s %d/%d generation=%llu | 이전 %d 재시도 %d 건너뛰기 %d"), *CurrentStep()->StepId.ToString(), Index + 1, Scenario->Steps.Num(), Generation, PreviousCount, RetryCount, SkipCount));
	}
	PlacePanel(); UpdateMarker();
}

void AVBoRScenarioRunner::CompleteStep(const TCHAR* Who)
{
	LastCompletion = FString::Printf(TEXT("단계 완료 %s | %s | %.3f s 이전 %d 재시도 %d 건너뛰기 %d"), *CurrentStep()->StepId.ToString(), Who, StepElapsed, PreviousCount, RetryCount, SkipCount);
	Record(LastCompletion);
	EnterStep(StepIndex + 1);
}

void AVBoRScenarioRunner::QueueCommand(const FVBoRFlowCommand& Command)
{
	FVBoRFlowCommand Queued = Command;
	// 처리 frame 의 dt 가 길어도 잠금 중 받은 입력은 살리지 않음 (R1-1)
	Queued.bConfirmLocked = Command.Action == EVBoRFlowCommand::Confirm && (ConfirmLockRemaining > 0 || bPanelPlacementPending);
	PendingCommands.Add(Queued);
}

void AVBoRScenarioRunner::FreezeLiveToken()
{
	if (LiveFrame != GFrameCounter) { LiveFrame = GFrameCounter; LiveToken = GetToken(); }
}

void AVBoRScenarioRunner::QueueLive(EVBoRFlowCommand Action, EVBoRCommandSource Source)
{
	FreezeLiveToken(); QueueCommand({Action, Source, LiveToken});
}

void AVBoRScenarioRunner::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds); FreezeLiveToken();
	// 모든 Actor 의 BeginPlay 뒤 한 번 검사함 (P3 레벨 배치 순서와 무관)
	if (!bInitialized) { InitializeFlow(); }
	ProcessFlow(DeltaSeconds);
}

void AVBoRScenarioRunner::ProcessFlow(double DeltaSeconds)
{
	const uint64 Before = Generation;
	StepElapsed += FMath::Max(0.0, DeltaSeconds);
	// 같은 Tick 앞에서 Pawn 을 옮겼으면 이동 전 dt 로 새 잠금을 줄이지 않음 (R1-7)
	if (!bPawnMovedBeforeFlow) { ConfirmLockRemaining = FMath::Max(0.0, ConfirmLockRemaining - FMath::Max(0.0, DeltaSeconds)); }
	bPawnMovedBeforeFlow = false;
	FTransform Head; const bool bTracked = Alignment && Alignment->ReadWorldHead(Head);
	SampleHead(bTracked, Head, DeltaSeconds);
	TArray<FVBoRFlowCommand> Commands = MoveTemp(PendingCommands); PendingCommands.Reset();
	for (const auto& Command : Commands) { Dispatch(Command); }
	if (bRestartPending && Alignment && !Alignment->IsSwitching())
	{ bRestartPending = false; bRunning = true; bFinished = false; EnterStep(0); }
	// 한 frame 에 규칙으로 여러 단계를 넘기지 않음 (P3 E1·E2)
	if (bRunning && Generation == Before && RuleSatisfied() && !NeedsConfirmation()) { CompleteStep(TEXT("참가자 규칙")); }
	RefreshPanel();
}

void AVBoRScenarioRunner::Dispatch(const FVBoRFlowCommand& Command)
{
	if (Command.Token.StepIndex != StepIndex || Command.Token.Generation != Generation || (bFinished && Command.Action != EVBoRFlowCommand::Restart))
	{ ++StaleDiscards; Record(TEXT("명령 버림 — 지난 단계 또는 안내 완료")); return; }
	const bool bOperator = Command.Source == EVBoRCommandSource::Operator;
	if (!bOperator && Command.Action != EVBoRFlowCommand::Confirm && Command.Action != EVBoRFlowCommand::Viewpoint && Command.Action != EVBoRFlowCommand::ReturnBase)
	{ ++PermissionDiscards; Record(TEXT("명령 버림 — 확인 권한 없음")); return; }
	if (Command.Action == EVBoRFlowCommand::IgnoreAlignment)
	{
		if (!bRunning && !ValidationProblems.IsEmpty()) { Record(TEXT("명령 버림 — 태그 검사 실패는 무시할 수 없음")); return; }
		if (!bRunning) { bAlignmentIgnored = true; Record(TEXT("진행자 정렬 무시하고 시작")); bRunning = true; EnterStep(0); }
		return;
	}
	if (!bRunning && Command.Action != EVBoRFlowCommand::Restart) { ++RuleDiscards; Record(TEXT("명령 버림 — 시작 전")); return; }
	if (Command.Action == EVBoRFlowCommand::Viewpoint || Command.Action == EVBoRFlowCommand::Passenger || Command.Action == EVBoRFlowCommand::Rear || Command.Action == EVBoRFlowCommand::ReturnBase)
	{
		bool bAccepted = false;
		if (Alignment)
		{
			if (Command.Action == EVBoRFlowCommand::ReturnBase) { bAccepted = Alignment->ReturnToBase(); }
			else
			{
				UVBoRViewpointComponent* View = nullptr;
				if (Command.Action == EVBoRFlowCommand::Viewpoint) { View = CurrentStep() && CurrentStep()->Rule == EVBoRStepRule::Viewpoint ? Cast<UVBoRViewpointComponent>(Targets[StepIndex]) : nullptr; }
				else { FString Problems; View = P3Component<UVBoRViewpointComponent>(FindP3Tag(GetWorld(), Command.Action == EVBoRFlowCommand::Passenger ? FName(TEXT("Car.PassengerView")) : FName(TEXT("Car.RearView")), Problems)); }
				bAccepted = Alignment->SwitchViewpoint(View);
			}
		}
		if (!bAccepted) { ++TransitionDiscards; }
		LastViewRequest = FString::Printf(TEXT("시점 명령 %s | %s"), bOperator ? TEXT("진행자") : TEXT("참가자"), bAccepted ? TEXT("요청됨") : TEXT("버림"));
		Record(LastViewRequest); return;
	}
	if (Alignment && Alignment->IsSwitching()) { ++TransitionDiscards; Record(TEXT("명령 버림 — 전환 중")); return; }
	switch (Command.Action)
	{
	case EVBoRFlowCommand::Confirm:
	{
		const auto* Step = CurrentStep();
		if (!Step || !NeedsConfirmation() || (Step->ConfirmBy == EVBoRConfirmBy::Operator && !bOperator) || (Step->ConfirmBy == EVBoRConfirmBy::Participant && bOperator))
		{ ++PermissionDiscards; Record(TEXT("명령 버림 — 확인 권한 없음")); return; }
		if (!RuleSatisfied()) { ++RuleDiscards; Record(TEXT("명령 버림 — 규칙 미충족")); return; }
		if (Command.bConfirmLocked || ConfirmLockRemaining > 0 || bPanelPlacementPending)
		{ ++ConfirmLockDiscards; Record(TEXT("명령 버림 — 단계 시작 직후 (확인 잠금·패널 다시 놓기)")); return; }
		CompleteStep(bOperator ? TEXT("진행자 확인") : TEXT("참가자 확인")); break;
	}
	case EVBoRFlowCommand::Next:
		if (CurrentStep()->bRequireBaseViewpoint && (!Alignment || !Alignment->IsBaseViewpoint()))
		{ ++RuleDiscards; Record(TEXT("명령 버림 — 하차 전 기준 복귀 필요")); return; }
		++SkipCount; CompleteStep(TEXT("진행자 건너뛰기")); break;
	case EVBoRFlowCommand::Previous: ++PreviousCount; Record(TEXT("진행자 이전")); EnterStep(FMath::Max(0, StepIndex - 1)); break;
	case EVBoRFlowCommand::Restart:
		if (!ValidationProblems.IsEmpty()) { Record(TEXT("재시도 버림 — 태그 검사 실패")); return; }
		if (!Alignment || Alignment->State != EVBoRAlignmentState::Applied)
		{ ++RuleDiscards; Record(TEXT("재시도 버림 — 기준 복귀에 저장 정렬 필요")); return; }
		++RetryCount;
		{
			TSet<AVBoRHingedDoor*> Doors;
			for (UObject* Target : Targets) { if (auto* Door = Cast<AVBoRHingedDoor>(Target)) { Doors.Add(Door); } }
			FString Problems;
			if (auto* Door = Cast<AVBoRHingedDoor>(P3Actor(FindP3Tag(GetWorld(), TEXT("Car.DriverDoor"), Problems)))) { Doors.Add(Door); }
			for (auto* Door : Doors) { Door->ResetDoor(); }
		}
		Record(TEXT("진행자 처음부터 — 문 닫힘·잠김, 기준 복귀"));
		if (!Alignment->IsBaseViewpoint()) { bRestartPending = Alignment->ReturnToBase(); bRunning = false; ++Generation; }
		else { bRunning = true; bFinished = false; EnterStep(0); }
		break;
	default: break;
	}
}

void AVBoRScenarioRunner::PlacePanel()
{
	bPanelPlacementPending = true;
	FTransform Head; const bool bTracked = Alignment && Alignment->ReadWorldHead(Head);
	if (bTracked) { PlacePanelAtHead(Head); }
	RefreshPanel();
}

void AVBoRScenarioRunner::HandlePawnMoved()
{
	bPawnMovedBeforeFlow = true; PlacePanel();
}

void AVBoRScenarioRunner::PlacePanelAtHead(const FTransform& Head)
{
	const FRotator Facing(0, Head.Rotator().Yaw, 0);
	const FVector Position = Head.GetLocation() + Facing.Vector() * PanelDistanceCm - FVector(0, 0, PanelBelowCm);
	Panel->SetWorldLocationAndRotation(Position, FRotator(0, Facing.Yaw + 180, 0));
	for (int32 I = 0; I < 2; ++I)
	{
		AVBoRPokeButton* Button = I == 0 ? ConfirmButton.Get() : ActionButton.Get(); if (!Button) { continue; }
		const FVector ButtonPosition = Head.GetLocation() + Facing.RotateVector(FVector(ButtonDistanceCm, I == 0 ? -12 : 12, -ButtonBelowCm));
		// Actor +Z 의 누름 면을 머리 쪽으로 향하게 함 (P0-b D4)
		Button->SetActorLocationAndRotation(ButtonPosition, FRotator(90, Facing.Yaw, 0));
		ButtonLabels[I]->SetWorldLocationAndRotation(ButtonPosition + FVector(0, 0, 7), Panel->GetComponentRotation());
	}
	bPanelPlacementPending = false; ConfirmLockRemaining = FMath::Max(0.0f, ConfirmLockSeconds);
	Record(TEXT("패널 다시 놓음 — 확인 잠금"));
}

bool AVBoRScenarioRunner::RecordHead(const TCHAR* Label, bool bTracked, const FTransform& Head)
{
	const FString Position = Alignment ? Alignment->DescribeHead(bTracked, Head.GetLocation()) : FString();
	if (Position.IsEmpty()) { return false; }
	Record(FString(Label) + TEXT(" | ") + Position); return true;
}

void AVBoRScenarioRunner::SampleHead(bool bTracked, const FTransform& Head, double DeltaSeconds)
{
	// 좌석 이탈 뒤에만 다시 허용함 (고개 회전·추적 복구만으로 따라가지 않게, R1-7)
	if (Seat && Seat->IsHeadTracked() && !Seat->IsHeadInside()) { bSeatedPanelPlaced = false; }
	if (Seat && Seat->IsHeadTracked() && Seat->IsHeadInside() && Seat->DwellSeconds >= SeatedPanelSeconds && !bSeatedPanelPlaced)
	{ bSeatedPanelPlaced = true; bPanelPlacementPending = true; }
	if (!bTracked) { return; }
	if (bPanelPlacementPending) { PlacePanelAtHead(Head); }
	if (bStartHeadPending && bRunning && RecordHead(TEXT("시작 위치"), true, Head)) { bStartHeadPending = false; }
	const auto* Step = CurrentStep();
	if (bRunning && Step && Step->Rule == EVBoRStepRule::Seated && RuleSatisfied() && !bSeatHeadRecorded)
	{ bSeatHeadRecorded = RecordHead(TEXT("착석 위치"), true, Head); }
	if (bOperatorInfo)
	{
		HeadLogElapsed += FMath::Max(0.0, DeltaSeconds);
		if (HeadLogElapsed >= 1.0) { HeadLogElapsed = 0; RecordHead(TEXT("머리 위치"), true, Head); }
	}
}

FText AVBoRScenarioRunner::GetInstructionText() const { return CurrentStep() ? CurrentStep()->Instruction : FText::GetEmpty(); }

FString AVBoRScenarioRunner::GetPanelText() const
{
	if (bFinished) { return TEXT("안내 완료"); }
	if (!bRunning) { return TEXT("준비 중입니다"); }
	if (const auto* Step = CurrentStep())
	{
		FString Text = FString::Printf(TEXT("%s\n%s · %d/%d\n"), *Step->Instruction.ToString(), *Step->StepId.ToString(), StepIndex + 1, Scenario->Steps.Num());
		if (Step->bRequireBaseViewpoint && Alignment && !Alignment->IsBaseViewpoint()) { Text += TEXT("운전석으로 돌아가세요"); }
		else if (bPanelPlacementPending || (Alignment && Alignment->IsSwitching()) || ConfirmLockRemaining > 0) { Text += TEXT("잠시 기다려 주세요"); }
		else if (RuleSatisfied() && NeedsConfirmation()) { Text += Step->ConfirmBy == EVBoRConfirmBy::Operator ? TEXT("잠시 기다려 주세요") : TEXT("확인을 눌러 주세요"); }
		else { Text += TEXT("안내에 따라 행동해 주세요"); }
		return Text;
	}
	return FString();
}

FString AVBoRScenarioRunner::GetOperatorText() const
{
	FString Text;
	if (!bRunning && !bFinished)
	{ Text = ValidationProblems.IsEmpty() ? TEXT("정렬 필요 — P0-c 레벨에서 C로 저장 (O: 무시)") : ValidationProblems; }
	else if (bRunning && CurrentStep())
	{
		const auto* Step = CurrentStep();
		if (NeedsConfirmation()) { Text = RuleSatisfied() ? TEXT("진행자 확인 대기 — Space") : TEXT("규칙 대기 — 만족 후 Space"); }
		else { Text = TEXT("규칙 대기 — → 건너뛰기"); }
		if (Alignment && !Alignment->IsBaseViewpoint()) { Text += TEXT(" | Home 운전석으로"); }
		if (Step->ConfirmBy == EVBoRConfirmBy::Participant) { Text = TEXT("참가자 확인 대기 — → 건너뛰기") + (Alignment && !Alignment->IsBaseViewpoint() ? FString(TEXT(" | Home 운전석으로")) : FString()); }
	}
	if (!bOperatorInfo) { return Text; }
	Text += TEXT("\n") + (Alignment ? Alignment->Describe() : FString(TEXT("정렬 컴포넌트 없음")));
	Text += FString::Printf(TEXT(" | 정렬 무시 %s\n이전 %d 재시도 %d 건너뛰기 %d | 버림 단계 %d 권한 %d 규칙 %d 전환 %d 잠금 %d"), bAlignmentIgnored ? TEXT("예") : TEXT("아니오"), PreviousCount, RetryCount, SkipCount, StaleDiscards, PermissionDiscards, RuleDiscards, TransitionDiscards, ConfirmLockDiscards);
	Text += TEXT("\n") + LastViewRequest + TEXT("\n") + LastCompletion;
	if (Approach) { Text += FString::Printf(TEXT("\n접근 %d (%.1f s) 이탈 %d (%.1f s) near=%d"), Approach->Approaches, Approach->LastApproachSeconds, Approach->Departures, Approach->LastDepartureSeconds, Approach->IsParticipantNear()); }
	if (Seat) { Text += FString::Printf(TEXT(" | 좌석 %.2f s 보류 %d"), Seat->DwellSeconds, Seat->TrackingPauses); }
	return Text;
}

FString AVBoRScenarioRunner::ActionLabel() const
{
	const auto* Step = CurrentStep();
	if (Step && Step->Rule == EVBoRStepRule::Viewpoint && !RuleSatisfied())
	{ return Step->TargetTag == TEXT("Car.PassengerView") ? TEXT("동승석으로 이동") : Step->TargetTag == TEXT("Car.RearView") ? TEXT("2열로 이동") : Step->TargetTag.ToString() + TEXT(" 시점으로 이동"); }
	return TEXT("운전석으로 돌아가기");
}

void AVBoRScenarioRunner::RefreshPanel()
{
	if (PanelText) { PanelText->SetText(FText::FromString(GetPanelText())); }
	const FString Operator = GetOperatorText();
	if (OperatorText) { OperatorText->SetText(FText::FromString(Operator)); }
	OperatorPanel->SetDrawSize(FVector2D(1000, bOperatorInfo ? 700 : 120));
	// 진행자 줄도 핸들·등받이 앞에 둠 (문구 아래 시야·버튼 이름 사이, R1-4·R1-7)
	const FVector OperatorOffset = -Panel->GetForwardVector() * ((bOperatorInfo ? ButtonDistanceCm : ButtonDistanceCm - 5) - PanelDistanceCm)
		- FVector(0, 0, bOperatorInfo ? 38 : PanelSizeCm.Y * 0.5 - 1);
	OperatorPanel->SetWorldLocationAndRotation(Panel->GetComponentLocation() + OperatorOffset, Panel->GetComponentRotation());
	Panel->SetHiddenInGame(bPanelPlacementPending); OperatorPanel->SetHiddenInGame(bPanelPlacementPending || Operator.IsEmpty());
	SetP3ButtonVisible(ConfirmButton, CanParticipantConfirm());
	const auto* Step = CurrentStep();
	const bool bAway = Alignment && !Alignment->IsBaseViewpoint();
	const bool bNeedsView = Step && Step->Rule == EVBoRStepRule::Viewpoint && !RuleSatisfied();
	PanelAction = bNeedsView ? EVBoRFlowCommand::Viewpoint : EVBoRFlowCommand::ReturnBase;
	SetP3ButtonVisible(ActionButton, !bPanelPlacementPending && bRunning && Alignment && !Alignment->IsSwitching() && (bAway || bNeedsView));
	for (int32 I = 0; I < 2; ++I)
	{
		AVBoRPokeButton* Button = I == 0 ? ConfirmButton.Get() : ActionButton.Get();
		ButtonLabels[I]->SetHiddenInGame(!Button || Button->IsHidden());
		if (ButtonLabelTexts.IsValidIndex(I)) { ButtonLabelTexts[I]->SetText(FText::FromString(I == 0 ? FString(TEXT("확인")) : ActionLabel())); }
	}
}

void AVBoRScenarioRunner::UpdateMarker()
{
	USceneComponent* Target = bRunning && Markers.IsValidIndex(StepIndex) ? Markers[StepIndex].Get() : nullptr;
	MarkerArrow->SetHiddenInGame(!Target);
	for (auto& Part : MarkerRing) { Part->SetHiddenInGame(!Target); }
	if (!Target) { return; }
	FVector P = Target->GetComponentLocation();
	MarkerArrow->SetWorldLocationAndRotation(P + FVector(0, 0, 160), FRotator(-90, 0, 0));
	for (int32 I = 0; I < 4; ++I)
	{
		const bool bX = I < 2;
		MarkerRing[I]->SetWorldLocation(FVector(P.X, P.Y, 0.5) + (bX ? FVector(0, I == 0 ? -30 : 30, 0) : FVector(I == 2 ? -30 : 30, 0, 0)));
		MarkerRing[I]->SetWorldRotation(FRotator::ZeroRotator);
		MarkerRing[I]->SetWorldScale3D(bX ? FVector(0.62, 0.02, 0.005) : FVector(0.02, 0.62, 0.005));
	}
}

void AVBoRScenarioRunner::Record(const FString& Text) { LastRecord = Text; UE_LOG(LogP3Flow, Log, TEXT("%s"), *Text); }
void AVBoRScenarioRunner::HandleConfirmPoke(const FIsdkInteractionPointerEvent&) { QueueLive(EVBoRFlowCommand::Confirm, EVBoRCommandSource::Participant); }
void AVBoRScenarioRunner::HandleActionPoke(const FIsdkInteractionPointerEvent&) { QueueLive(PanelAction, EVBoRCommandSource::Participant); }
void AVBoRScenarioRunner::HandleTargetPoke(const FIsdkInteractionPointerEvent& Event)
{
	if (UObject* Object = Event.Interactable.GetObject())
	{
		if (UActorComponent* Component = Cast<UActorComponent>(Object))
		{ if (AVBoRPokeButton* Button = Cast<AVBoRPokeButton>(Component->GetOwner())) { if (int32* Count = ButtonCounts.Find(Button)) { ++*Count; } } }
	}
}
void AVBoRScenarioRunner::KeyConfirm() { QueueLive(EVBoRFlowCommand::Confirm, EVBoRCommandSource::Operator); }
void AVBoRScenarioRunner::KeyNext() { QueueLive(EVBoRFlowCommand::Next, EVBoRCommandSource::Operator); }
void AVBoRScenarioRunner::KeyPrevious() { QueueLive(EVBoRFlowCommand::Previous, EVBoRCommandSource::Operator); }
void AVBoRScenarioRunner::KeyPassenger() { QueueLive(EVBoRFlowCommand::Passenger, EVBoRCommandSource::Operator); }
void AVBoRScenarioRunner::KeyRear() { QueueLive(EVBoRFlowCommand::Rear, EVBoRCommandSource::Operator); }
void AVBoRScenarioRunner::KeyReturn() { QueueLive(EVBoRFlowCommand::ReturnBase, EVBoRCommandSource::Operator); }
void AVBoRScenarioRunner::KeyRestart() { QueueLive(EVBoRFlowCommand::Restart, EVBoRCommandSource::Operator); }
void AVBoRScenarioRunner::KeyIgnore() { QueueLive(EVBoRFlowCommand::IgnoreAlignment, EVBoRCommandSource::Operator); }
void AVBoRScenarioRunner::KeyOperatorInfo()
{
	bOperatorInfo = !bOperatorInfo; HeadLogElapsed = 0;
	Record(bOperatorInfo ? TEXT("진행자 정보 켬 (F1)\n") + GetOperatorText() : TEXT("진행자 정보 끔 (F1)")); RefreshPanel();
}
