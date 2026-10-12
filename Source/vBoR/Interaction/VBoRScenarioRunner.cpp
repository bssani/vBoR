#include "VBoRScenarioRunner.h"
#include "VBoRParticipantZones.h"
#include "VBoRVehicleAlignmentComponent.h"
#include "VBoRViewpointComponent.h"
#include "VBoRHingedDoor.h"
#include "VBoRPokeButton.h"
#include "Camera/CameraComponent.h"
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
}

AVBoRScenarioRunner::AVBoRScenarioRunner()
{
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("FlowOrigin")));
	PrimaryActorTick.bCanEverTick = true; PrimaryActorTick.TickGroup = TG_PostUpdateWork;
	Panel = CreateDefaultSubobject<UWidgetComponent>(TEXT("KoreanGuidance")); Panel->SetupAttachment(RootComponent);
	Panel->SetWidgetSpace(EWidgetSpace::World); Panel->SetDrawSize(FVector2D(1000, 640)); Panel->SetTwoSided(true);
	Panel->SetCollisionEnabled(ECollisionEnabled::NoCollision);
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
	PanelText = SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Regular", 20)).ColorAndOpacity(FLinearColor::White).AutoWrapText(true);
	Panel->SetSlateWidget(SNew(SBorder).Padding(24).BorderBackgroundColor(FLinearColor(0.015, 0.025, 0.04, 1))[PanelText.ToSharedRef()]);
	Panel->SetWorldScale3D(FVector(1, PanelSizeCm.X / 1000.0, PanelSizeCm.Y / 640.0));
	for (int32 I = 0; I < 2; ++I)
	{
		// 메시·판정 영역 크기를 함께 정하고 실제 poke 경로를 재사용함 (P3 D4)
		AVBoRPokeButton* Button = GetWorld()->SpawnActorDeferred<AVBoRPokeButton>(AVBoRPokeButton::StaticClass(), FTransform::Identity, this);
		Button->ButtonMesh->SetRelativeScale3D(FVector(0.08, 0.08, 0.015)); Button->HitAreaScale = 1;
		Button->FinishSpawning(FTransform::Identity); Button->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepWorldTransform);
		if (I == 0) { ConfirmButton = Button; Button->OnButtonPressed.AddUniqueDynamic(this, &AVBoRScenarioRunner::HandleConfirmPoke); }
		else { ActionButton = Button; Button->OnButtonPressed.AddUniqueDynamic(this, &AVBoRScenarioRunner::HandleActionPoke); }
		SetP3ButtonVisible(Button, false);
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
	}
	PlacePanel();
}

void AVBoRScenarioRunner::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	for (const auto& Entry : ButtonCounts)
	{ if (Entry.Key.IsValid()) { Entry.Key->OnButtonPressed.RemoveDynamic(this, &AVBoRScenarioRunner::HandleTargetPoke); } }
	if (ConfirmButton) { ConfirmButton->Destroy(); }
	if (ActionButton) { ActionButton->Destroy(); }
	Panel->SetSlateWidget(nullptr); PanelText.Reset();
	if (InputComponent) { InputComponent->KeyBindings.Reset(); }
	Super::EndPlay(EndPlayReason);
}

bool AVBoRScenarioRunner::InitializeFlow()
{
	bInitialized = true; bRunning = false; bFinished = false; ValidationProblems.Reset(); Targets.Reset(); Markers.Reset();
	for (const auto& Entry : ButtonCounts)
	{ if (Entry.Key.IsValid()) { Entry.Key->OnButtonPressed.RemoveDynamic(this, &AVBoRScenarioRunner::HandleTargetPoke); } }
	ButtonCounts.Reset();
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
	if (Alignment) { AddTickPrerequisiteComponent(Alignment); }
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
	return bRunning && Step && NeedsConfirmation() && Step->ConfirmBy != EVBoRConfirmBy::Operator && RuleSatisfied();
}

void AVBoRScenarioRunner::EnterStep(int32 Index)
{
	StepIndex = Index; ++Generation; StepElapsed = 0;
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

void AVBoRScenarioRunner::QueueCommand(const FVBoRFlowCommand& Command) { PendingCommands.Add(Command); }

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
	FTransform Head;
	if (!VBoRReadWorldHead(GetWorld(), Head))
	{
		if (AActor* Pawn = UGameplayStatics::GetPlayerPawn(this, 0))
		{
			if (UCameraComponent* Camera = Pawn->FindComponentByClass<UCameraComponent>()) { Head = Camera->GetComponentTransform(); }
			else { Head = Pawn->GetActorTransform(); }
		}
	}
	const FRotator Facing(0, Head.Rotator().Yaw, 0);
	const FVector Position = Head.GetLocation() + Facing.Vector() * PanelDistanceCm - FVector(0, 0, PanelBelowCm);
	Panel->SetWorldLocationAndRotation(Position, FRotator(0, Facing.Yaw + 180, 0));
	for (int32 I = 0; I < 2; ++I)
	{
		AVBoRPokeButton* Button = I == 0 ? ConfirmButton.Get() : ActionButton.Get(); if (!Button) { continue; }
		const FVector Offset = Facing.RotateVector(FVector(-1, I == 0 ? -12 : 12, -PanelSizeCm.Y * 0.5 - 7));
		// Actor +Z 의 누름 면을 머리 쪽으로 향하게 함 (P0-b D4)
		Button->SetActorLocationAndRotation(Position + Offset, FRotator(90, Facing.Yaw, 0));
	}
}

FText AVBoRScenarioRunner::GetInstructionText() const { return CurrentStep() ? CurrentStep()->Instruction : FText::GetEmpty(); }

FString AVBoRScenarioRunner::GetPanelText() const
{
	FString Text = TEXT("근사 형상·자리값\n");
	Text += Alignment ? Alignment->Describe() : TEXT("정렬 컴포넌트 없음");
	Text += FString::Printf(TEXT(" | 정렬 무시 %s\n"), bAlignmentIgnored ? TEXT("예") : TEXT("아니오"));
	if (bFinished) { Text += TEXT("안내 완료\n"); }
	else if (!bRunning) { Text += ValidationProblems.IsEmpty() ? TEXT("정렬 필요 — P0-c 레벨에서 C로 저장 (O: 무시)\n") : ValidationProblems; }
	else if (const auto* Step = CurrentStep())
	{
		Text += FString::Printf(TEXT("%s · %d/%d\n%s\n규칙 %s | "), *Step->StepId.ToString(), StepIndex + 1, Scenario->Steps.Num(), *Step->Instruction.ToString(), RuleSatisfied() ? TEXT("만족") : TEXT("대기"));
		if (Step->bRequireBaseViewpoint && Alignment && !Alignment->IsBaseViewpoint()) { Text += TEXT("운전석으로 돌아가세요"); }
		else if (RuleSatisfied() && NeedsConfirmation()) { Text += Step->ConfirmBy == EVBoRConfirmBy::Operator ? TEXT("진행자 확인 대기") : TEXT("확인을 눌러 주세요"); }
		else { Text += TEXT("안내에 따라 행동해 주세요"); }
		if (CanParticipantConfirm()) { Text += TEXT("\n왼쪽 버튼: 확인"); }
		if (Step->Rule == EVBoRStepRule::Viewpoint && !RuleSatisfied())
		{
			const FString Label = Step->TargetTag == TEXT("Car.PassengerView") ? TEXT("동승석으로 이동") : Step->TargetTag == TEXT("Car.RearView") ? TEXT("2열로 이동") : Step->TargetTag.ToString() + TEXT(" 시점으로 이동");
			Text += TEXT("\n오른쪽 버튼: ") + Label;
		}
		else if (Alignment && !Alignment->IsBaseViewpoint()) { Text += TEXT("\n오른쪽 버튼: 운전석으로 돌아가기"); }
	}
	Text += FString::Printf(TEXT("\n이전 %d 재시도 %d 건너뛰기 %d | 버림 단계 %d 권한 %d 규칙 %d 전환 %d"), PreviousCount, RetryCount, SkipCount, StaleDiscards, PermissionDiscards, RuleDiscards, TransitionDiscards);
	Text += TEXT("\n") + LastViewRequest + TEXT("\n") + LastCompletion;
	if (Approach) { Text += FString::Printf(TEXT("\n접근 %d (%.1f s) 이탈 %d (%.1f s) near=%d"), Approach->Approaches, Approach->LastApproachSeconds, Approach->Departures, Approach->LastDepartureSeconds, Approach->IsParticipantNear()); }
	if (Seat) { Text += FString::Printf(TEXT(" | 좌석 %.2f s 보류 %d"), Seat->DwellSeconds, Seat->TrackingPauses); }
	return Text;
}

void AVBoRScenarioRunner::RefreshPanel()
{
	if (PanelText) { PanelText->SetText(FText::FromString(GetPanelText())); }
	SetP3ButtonVisible(ConfirmButton, CanParticipantConfirm());
	const auto* Step = CurrentStep();
	const bool bAway = Alignment && !Alignment->IsBaseViewpoint();
	const bool bNeedsView = Step && Step->Rule == EVBoRStepRule::Viewpoint && !RuleSatisfied();
	PanelAction = bNeedsView ? EVBoRFlowCommand::Viewpoint : EVBoRFlowCommand::ReturnBase;
	SetP3ButtonVisible(ActionButton, bRunning && Alignment && !Alignment->IsSwitching() && (bAway || bNeedsView));
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
