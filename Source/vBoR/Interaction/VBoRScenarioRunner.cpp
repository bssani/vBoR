#include "VBoRScenarioRunner.h"
#include "VBoRPoweredPanel.h"
#include "VBoRFoldingSeat.h"
#include "VBoRBoxStand.h"
#include "VBoRParticipantZones.h"
#include "VBoRVehicleAlignmentComponent.h"
#include "VBoRViewpointComponent.h"
#include "VBoRHingedDoor.h"
#include "VBoRPokeButton.h"
#include "VBoRFloorRoute.h"
#include "VBoRKeyFob.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/ArrowComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SplineComponent.h"
#include "Components/InputComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "HAL/IConsoleManager.h"
#include "HeadMountedDisplayFunctionLibrary.h"
#include "Interaction/IsdkPokeInteractable.h"
#include "Kismet/GameplayStatics.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBorder.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	TAutoConsoleVariable<int32> CVarHomeShift(TEXT("vBoR.HomeShift"), 0,
		TEXT("Home testing only: enable keypad 8/2/4/6 to shift by 1 m in car axes, 5 to reset."));
	FVector P3RouteEnd(const AVBoRFloorRoute* Route)
	{
		return Route->Spline->GetLocationAtSplinePoint(Route->Spline->GetNumberOfSplinePoints() - 1, ESplineCoordinateSpace::World);
	}
	FVector P3RouteDirection(const AVBoRFloorRoute* Route)
	{
		return Route->Spline->GetDirectionAtDistanceAlongSpline(Route->Spline->GetSplineLength(), ESplineCoordinateSpace::World).GetSafeNormal2D();
	}
	TArray<FVector> P3RouteSamples(const AVBoRFloorRoute* Route)
	{
		TArray<FVector> Points;
		const float Length = Route->Spline->GetSplineLength();
		const int32 Intervals = FMath::Max(1, FMath::CeilToInt(Length / 10));
		for (int32 I = 0; I <= Intervals; ++I) { Points.Add(Route->Spline->GetLocationAtDistanceAlongSpline(Length * I / Intervals, ESplineCoordinateSpace::World)); }
		const FVector End = P3RouteEnd(Route);
		for (int32 I = 0; I < 48; ++I)
		{
			const double Angle = 2 * PI * I / 48;
			Points.Add(End + Route->RingRadiusCm * FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0));
		}
		return Points;
	}
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
	FreeModeInstruction = FText::FromString(TEXT("자유모드 — 차를 자유롭게 살펴보고 조작해 보세요. 좌석은 아래 버튼으로 바꿉니다"));
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
		InputComponent->BindKey(EKeys::R, IE_Pressed, this, &AVBoRScenarioRunner::KeyRestart);
		InputComponent->BindKey(EKeys::End, IE_Pressed, this, &AVBoRScenarioRunner::KeyFreeMode);
		InputComponent->BindKey(EKeys::F6, IE_Pressed, this, &AVBoRScenarioRunner::KeyReloadLevel);
		InputComponent->BindKey(EKeys::O, IE_Pressed, this, &AVBoRScenarioRunner::KeyIgnore);
		InputComponent->BindKey(EKeys::I, IE_Pressed, this, &AVBoRScenarioRunner::KeyOperatorInfo);
		InputComponent->BindKey(EKeys::A, IE_Pressed, this, &AVBoRScenarioRunner::KeyReapply);
		InputComponent->BindKey(EKeys::F, IE_Pressed, this, &AVBoRScenarioRunner::KeyFoldSeat);
		InputComponent->BindKey(EKeys::U, IE_Pressed, this, &AVBoRScenarioRunner::KeyUnfoldSeat);
		for (const FKey Key : {EKeys::NumPadEight, EKeys::NumPadTwo, EKeys::NumPadFour, EKeys::NumPadSix, EKeys::NumPadFive})
		{
			InputComponent->BindKey(Key, IE_Pressed, this, &AVBoRScenarioRunner::KeyHomeShift).bConsumeInput = false;
		}
	}
	PlacePanel(); ShowOperatorWindow();
}

void AVBoRScenarioRunner::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	CloseOperatorWindow();
	UnbindKeys();
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
	UnbindKeys(); bStartHeadPending = false;
	bInitialized = true; bRunning = false; bFinished = false; ValidationProblems.Reset(); Targets.Reset(); Markers.Reset();
	Routes.Reset(); FloorRouteNotice.Reset(); StartMarker = nullptr;
	for (TActorIterator<AVBoRFloorRoute> It(GetWorld()); It; ++It) { It->SetRouteVisible(false); }
	bFreeMode = false; bReloadPending = false; bReloadRequested = false; ReloadRemaining = 0;
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
		case EVBoRStepRule::KeyHeld: case EVBoRStepRule::KeyUnlockAndLock: Target = Cast<AVBoRKeyFob>(P3Actor(Raw)); break;
		case EVBoRStepRule::PoweredOpen: case EVBoRStepRule::PoweredClosed: Target = Cast<AVBoRPoweredPanel>(P3Actor(Raw)); break;
		}
		if (Step.Rule != EVBoRStepRule::Confirm && !Target) { ValidationProblems += FString::Printf(TEXT("%s: 태그 %s 종류 불일치 또는 없음\n"), *Step.StepId.ToString(), *Step.TargetTag.ToString()); }
		const FString Id = Step.StepId.ToString(); const int32 Number = FCString::Atoi(*Id);
		if (Number < 1 || Number > 20 || Number == 17 || Id != FString::Printf(TEXT("%02d"), Number)) { ValidationProblems += TEXT("원본 StepId 필요 (01–16, 18–20)\n"); }
		Targets.Add(Target);
		UObject* Marker = FindP3Tag(GetWorld(), Step.MarkerTag, ValidationProblems);
		USceneComponent* Scene = P3Scene(Marker); Markers.Add(Scene);
		if (!Step.MarkerTag.IsNone() && !Scene) { ValidationProblems += FString::Printf(TEXT("%s: 표시 태그 종류 불일치\n"), *Step.StepId.ToString()); }
		FString RouteProblems;
		AVBoRFloorRoute* Route = Cast<AVBoRFloorRoute>(FindP3Tag(GetWorld(), Step.RouteTag, RouteProblems)); Routes.Add(Route);
		if (!Step.RouteTag.IsNone())
		{
			if (!Route || Route->Spline->GetNumberOfSplinePoints() < 2)
			{ ValidationProblems += FString::Printf(TEXT("%s: 바닥 경로 %s — 경로 Actor 하나와 점 둘 이상 필요 | %s\n"), *Step.StepId.ToString(), *Step.RouteTag.ToString(), *RouteProblems); }
			else
			{
				UBoxComponent* Footprint = Cast<UBoxComponent>(FindP3Tag(GetWorld(), TEXT("Car.Footprint"), RouteProblems));
				if (!Footprint) { ValidationProblems += FString::Printf(TEXT("%s: 바닥 경로 %s — Car.Footprint 상자 하나 필요 | %s\n"), *Step.StepId.ToString(), *Step.RouteTag.ToString(), *RouteProblems); }
				else
				{
					TArray<FVector> Points = P3RouteSamples(Route);
					Points.Add(P3RouteEnd(Route) + P3RouteDirection(Route) * PanelDistanceCm);
					for (const FVector& Point : Points)
					{
						const FVector Local = Footprint->GetComponentTransform().InverseTransformPosition(Point), Extent = Footprint->GetUnscaledBoxExtent();
						if (FMath::Abs(Local.X) <= Extent.X && FMath::Abs(Local.Y) <= Extent.Y)
						{ ValidationProblems += FString::Printf(TEXT("%s: 바닥 경로 %s가 차체 영역 Car.Footprint를 지남\n"), *Step.StepId.ToString(), *Step.RouteTag.ToString()); break; }
					}
				}
			}
		}
		if (AVBoRPokeButton* Button = Cast<AVBoRPokeButton>(Target))
		{
			ButtonCounts.FindOrAdd(Button) = 0; Button->OnButtonPressed.AddUniqueDynamic(this, &AVBoRScenarioRunner::HandleTargetPoke);
		}
		if (AVBoRKeyFob* Key = Cast<AVBoRKeyFob>(Target))
		{
			KeyCounts.FindOrAdd(Key) = FIntPoint::ZeroValue;
			Key->OnUnlockPressed.AddUniqueDynamic(this, &AVBoRScenarioRunner::HandleKeyUnlock);
			Key->OnLockPressed.AddUniqueDynamic(this, &AVBoRScenarioRunner::HandleKeyLock);
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
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		TArray<USceneComponent*> Components; It->GetComponents(Components);
		for (USceneComponent* Component : Components) { if (Component->ComponentHasTag(TEXT("Car.Start"))) { StartMarker = Component; } }
	}
	if (!ValidationProblems.IsEmpty()) { Record(TEXT("시작 차단:\n") + ValidationProblems); RefreshPanel(); return false; }
	if (Alignment->State != EVBoRAlignmentState::Applied && !bAlignmentIgnored)
	{ Record(TEXT("정렬 필요 — P0-c 레벨에서 C로 저장; O로 무시 가능 | ") + Alignment->Describe()); RefreshPanel(); return false; }
	bRunning = true; EnterStep(0, true); RefreshPanel(); return true;
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
	case EVBoRStepRule::KeyHeld: { const auto* Key = Cast<AVBoRKeyFob>(Target); return Key && Key->GetHoldingHand() != INDEX_NONE; }
	case EVBoRStepRule::PoweredOpen: { const auto* Gate = Cast<AVBoRPoweredPanel>(Target); return Gate && Gate->GetState() == EVBoRPoweredState::Open; }
	case EVBoRStepRule::PoweredClosed: { const auto* Gate = Cast<AVBoRPoweredPanel>(Target); return Gate && Gate->GetState() == EVBoRPoweredState::Closed; }
	case EVBoRStepRule::KeyUnlockAndLock:
	{
		const FIntPoint Counts = KeyCounts.FindRef(Cast<AVBoRKeyFob>(Target));
		return Counts.X > KeyBaseline.X && Counts.Y > KeyBaseline.Y;
	}
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

void AVBoRScenarioRunner::EnterStep(int32 Index, bool bNewAttempt)
{
	if (StepIndex == 0 && Index != 0) { RecordStartFallback(); }
	OperatorCompletedSteps = FMath::Max(0, Index);
	StepIndex = Index; ++Generation; StepElapsed = 0; ConfirmLockRemaining = FMath::Max(0.0f, ConfirmLockSeconds); bSeatHeadRecorded = false;
	// 이전 단계로 돌아가도 같은 시도의 시작 기록을 유지함 (P4c D3)
	if (bNewAttempt) { bStartHeadPending = true; }
	if (!CurrentStep())
	{
		EnterFreeMode(true); return;
	}
	else
	{
		ButtonBaseline = ButtonCounts.FindRef(Cast<AVBoRPokeButton>(Targets[Index]));
		KeyBaseline = KeyCounts.FindRef(Cast<AVBoRKeyFob>(Targets[Index]));
		Record(FString::Printf(TEXT("단계 시작 %s %d/%d generation=%llu | 이전 %d 재시도 %d 건너뛰기 %d"), *CurrentStep()->StepId.ToString(), Index + 1, Scenario->Steps.Num(), Generation, PreviousCount, RetryCount, SkipCount));
	}
	BeginRoute(); PlacePanel(); UpdateMarker();
}

void AVBoRScenarioRunner::EnterFreeMode(bool bCompleted)
{
	if (StepIndex == 0) { RecordStartFallback(); }
	const FString Reason = bCompleted ? FString(TEXT("안내 완료"))
		: FString::Printf(TEXT("진행자 End, 단계 %s 에서"), *CurrentStep()->StepId.ToString());
	bRunning = false; bFreeMode = true; bFinished = bCompleted; StepIndex = INDEX_NONE; ++Generation;
	if (!bCompleted) { ++EndCount; }
	BeginRoute(); PlacePanel(); UpdateMarker();
	Record(FString::Printf(TEXT("자유모드 시작 — %s | 요약 이전 %d 재시도 %d 건너뛰기 %d End %d | 버림 지난단계 %d 권한 %d 규칙 %d 전환 %d 잠금 %d"),
		*Reason, PreviousCount, RetryCount, SkipCount, EndCount, StaleDiscards, PermissionDiscards, RuleDiscards, TransitionDiscards, ConfirmLockDiscards));
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
	const bool bSeat = bFreeMode && Command.Source == EVBoRCommandSource::Participant &&
		(Command.Action == EVBoRFlowCommand::Passenger || Command.Action == EVBoRFlowCommand::Rear || Command.Action == EVBoRFlowCommand::ReturnBase);
	Queued.bConfirmLocked = (Command.Action == EVBoRFlowCommand::Confirm || bSeat) && (ConfirmLockRemaining > 0 || bPanelPlacementPending);
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
	// 요청 frame 이전의 dt 를 새 암전에 넣지 않음 (P5g D8, s)
	if (bReloadPending && !bReloadRequested)
	{
		ReloadRemaining = FMath::Max(0.0, ReloadRemaining - FMath::Max(0.0, DeltaSeconds));
		if (ReloadRemaining <= 0)
		{
			bReloadRequested = true;
			Record(TEXT("진행자 레벨 다시 열기 요청 — 암전 완료"));
			RequestLevelReload(FName(*UGameplayStatics::GetCurrentLevelName(this)));
		}
	}
	const uint64 Before = Generation;
	StepElapsed += FMath::Max(0.0, DeltaSeconds);
	// 같은 Tick 앞에서 Pawn 을 옮겼으면 이동 전 dt 로 새 잠금을 줄이지 않음 (R1-7)
	if (!bPawnMovedBeforeFlow) { ConfirmLockRemaining = FMath::Max(0.0, ConfirmLockRemaining - FMath::Max(0.0, DeltaSeconds)); }
	bPawnMovedBeforeFlow = false;
	FTransform Head; const bool bTracked = Alignment && Alignment->ReadWorldHead(Head);
	SampleHead(bTracked, Head, DeltaSeconds);
	SampleRoute(bTracked, Head, DeltaSeconds);
	TArray<FVBoRFlowCommand> Commands = MoveTemp(PendingCommands); PendingCommands.Reset();
	for (const auto& Command : Commands) { Dispatch(Command); }
	if (bReapplyPending && Alignment && !Alignment->IsSwitching()) { FinishReapply(); }
	if (bRestartPending && Alignment && !Alignment->IsSwitching())
	{ bRestartPending = false; bRunning = true; bFinished = false; EnterStep(0, true); }
	// 한 frame 에 규칙으로 여러 단계를 넘기지 않음 (P3 E1·E2)
	if (bRunning && Generation == Before && RuleSatisfied() && !NeedsConfirmation()) { CompleteStep(TEXT("참가자 규칙")); }
	RefreshPanel();
}

void AVBoRScenarioRunner::Dispatch(const FVBoRFlowCommand& Command)
{
	const auto Decision = EvaluateCommand(Command);
	RecordDecision(Command, Decision);
	if (!Decision.bAccepted) { return; }
	const bool bOperator = Command.Source == EVBoRCommandSource::Operator;
	if (Command.Action == EVBoRFlowCommand::IgnoreAlignment)
	{
		bAlignmentIgnored = true; Record(TEXT("진행자 정렬 무시하고 시작")); bRunning = true; EnterStep(0, true); return;
	}
	if (Command.Action == EVBoRFlowCommand::Viewpoint || Command.Action == EVBoRFlowCommand::Passenger || Command.Action == EVBoRFlowCommand::Rear || Command.Action == EVBoRFlowCommand::ReturnBase)
	{
		const bool bAccepted = Command.Action == EVBoRFlowCommand::ReturnBase ? Alignment->ReturnToBase() : Alignment->SwitchViewpoint(CommandViewpoint(Command.Action));
		if (!bAccepted) { ++TransitionDiscards; }
		LastViewRequest = FString::Printf(TEXT("시점 명령 %s | %s"), bOperator ? TEXT("진행자") : TEXT("참가자"), bAccepted ? TEXT("요청됨") : TEXT("버림"));
		Record(LastViewRequest); return;
	}

	switch (Command.Action)
	{
	case EVBoRFlowCommand::FoldSeat:
	case EVBoRFlowCommand::UnfoldSeat:
		for (TActorIterator<AVBoRFoldingSeat> It(GetWorld()); It; ++It)
		{ if (Command.Action == EVBoRFlowCommand::FoldSeat) { It->Fold(); } else { It->Unfold(); } }
		Record(Command.Action == EVBoRFlowCommand::FoldSeat ? TEXT("진행자 시트 접기") : TEXT("진행자 시트 펴기")); break;
	case EVBoRFlowCommand::ReapplyAlignment:
		bReapplyPending = Alignment->RequestReapply(); ++ReapplyCount;
		Record(TEXT("진행자 정렬 다시 적용 — 암전 시작")); break;
	case EVBoRFlowCommand::EnterFreeMode:
		EnterFreeMode(false); break;
	case EVBoRFlowCommand::ReloadLevel:
	{
		bReloadPending = true; bReloadRequested = false;
		ReloadRemaining = Alignment ? FMath::Max(0.01f, Alignment->FadeSeconds) : 0.3;
		if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
		{ if (PC->PlayerCameraManager) { PC->PlayerCameraManager->StartCameraFade(0, 1, ReloadRemaining, FLinearColor::Black, false, true); } }
		Record(TEXT("진행자 레벨 다시 열기 — 암전 시작")); break;
	}
	case EVBoRFlowCommand::Confirm:
	{
		CompleteStep(bOperator ? TEXT("진행자 확인") : TEXT("참가자 확인")); break;
	}
	case EVBoRFlowCommand::Next:
		++SkipCount; CompleteStep(TEXT("진행자 건너뛰기")); break;
	case EVBoRFlowCommand::Previous: ++PreviousCount; Record(TEXT("진행자 이전")); EnterStep(FMath::Max(0, StepIndex - 1)); break;
	case EVBoRFlowCommand::Restart:
		++RetryCount;
		// 기준 시점의 R 도 집 시험 이동을 되돌림 (P3c D5)
		Alignment->ResetHomeShift();
		{
			// 박스를 먼저 치워 닫힌 게이트 안에 남지 않게 함 (P4d D6)
			for (TActorIterator<AVBoRBoxStand> It(GetWorld()); It; ++It) { It->ResetStand(); }
			TSet<AVBoRPoweredPanel*> Gates;
			for (UObject* Target : Targets) { if (auto* Gate = Cast<AVBoRPoweredPanel>(Target)) { Gates.Add(Gate); } }
			for (TActorIterator<AVBoRPoweredPanel> It(GetWorld()); It; ++It) { if (It->ActorHasTag(TEXT("Car.Liftgate"))) { Gates.Add(*It); } }
			for (auto* Gate : Gates) { Gate->ResetPanel(); }
			for (TActorIterator<AVBoRFoldingSeat> It(GetWorld()); It; ++It) { It->ResetSeat(); }
			TSet<AVBoRHingedDoor*> Doors;
			for (UObject* Target : Targets) { if (auto* Door = Cast<AVBoRHingedDoor>(Target)) { Doors.Add(Door); } }
			FString Problems;
			if (auto* Door = Cast<AVBoRHingedDoor>(P3Actor(FindP3Tag(GetWorld(), TEXT("Car.DriverDoor"), Problems)))) { Doors.Add(Door); }
			for (auto* Door : Doors) { Door->ResetDoor(); }
		}
		Record(TEXT("진행자 처음부터 — 문 닫힘·잠김, 리프트게이트 초기화·박스 받침 1개·시트 세움, 기준 복귀"));
		bFreeMode = false;
		bStartHeadPending = false;
		if (!Alignment->IsBaseViewpoint()) { bRestartPending = Alignment->ReturnToBase(); bRunning = false; ++Generation; BeginRoute(); UpdateMarker(); }
		else { bRunning = true; bFinished = false; EnterStep(0, true); }
		break;
	default: break;
	}
}

void AVBoRScenarioRunner::RequestLevelReload(FName LevelName)
{
	UGameplayStatics::OpenLevel(this, LevelName);
}

bool AVBoRScenarioRunner::IsHomeShiftEnabled()
{
	return CVarHomeShift.GetValueOnGameThread() == 1;
}

void AVBoRScenarioRunner::KeyHomeShift(FKey Key)
{
	HandleHomeShiftKey(Key);
}

bool AVBoRScenarioRunner::HandleHomeShiftKey(FKey Key, bool bRepeat)
{
	if (!IsHomeShiftEnabled()) { return false; }
	FVector2D Direction = FVector2D::ZeroVector;
	if (Key == EKeys::NumPadEight) { Direction.X = 1; }
	else if (Key == EKeys::NumPadTwo) { Direction.X = -1; }
	else if (Key == EKeys::NumPadFour) { Direction.Y = -1; }
	else if (Key == EKeys::NumPadSix) { Direction.Y = 1; }
	else if (Key != EKeys::NumPadFive) { return false; }
	if (bRepeat) { return true; }
	if (bReloadPending || (Alignment && Alignment->IsSwitching()))
	{
		Record(bReloadPending ? TEXT("집 시험 옮기기 버림 — 레벨 다시 열기 암전 중") : TEXT("집 시험 옮기기 버림 — 시점 전환·정렬 다시 적용 암전 중"));
		return true;
	}
	if (!Alignment) { Record(TEXT("집 시험 옮기기 버림 — 정렬 컴포넌트 없음")); return true; }
	const FVector2D DeltaCm = Key == EKeys::NumPadFive ? -Alignment->GetHomeShiftCm() : Direction * Alignment->HomeShiftStepCm;
	const bool bMoved = Key == EKeys::NumPadFive ? Alignment->ResetHomeShift() : Alignment->MoveHomeShift(DeltaCm);
	if (bMoved)
	{
		const TCHAR* Axis = Key == EKeys::NumPadEight ? TEXT("+X") : Key == EKeys::NumPadTwo ? TEXT("-X") :
			Key == EKeys::NumPadFour ? TEXT("-Y") : Key == EKeys::NumPadSix ? TEXT("+Y") : TEXT("원위치");
		const FVector2D Offset = Alignment->GetHomeShiftCm();
		Record(FString::Printf(TEXT("집 시험 옮기기 %s %.0f cm → (%.0f, %.0f)"), Axis,
			FMath::Abs(DeltaCm.X) + FMath::Abs(DeltaCm.Y), Offset.X, Offset.Y));
	}
	return true;
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

void AVBoRScenarioRunner::PlacePanelAtHead(const FTransform& Head, bool bAtRouteArrival)
{
	float Yaw = Head.Rotator().Yaw;
	if (Alignment && ((Seat && Seat->IsHeadInside()) || !Alignment->IsBaseViewpoint()))
	{ if (const USceneComponent* Eye = Alignment->GetCurrentEye()) { Yaw = Eye->GetComponentRotation().Yaw; } }
	if (bAtRouteArrival && Routes.IsValidIndex(StepIndex) && Routes[StepIndex]) { Yaw = P3RouteDirection(Routes[StepIndex]).Rotation().Yaw; }
	const FRotator Facing(0, Yaw, 0);
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
	const FVector StartOffset = StartMarker ? Head.GetLocation() - StartMarker->GetComponentLocation() : FVector::ZeroVector;
	if (bStartHeadPending && bRunning && (!StartMarker || (StepIndex == 0 && StartOffset.SizeSquared2D() <= 50 * 50 && StartOffset.Z >= 100)) && RecordHead(TEXT("시작 위치"), true, Head)) { bStartHeadPending = false; }
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
	if (bFreeMode) { return (bFinished ? FString(TEXT("안내 완료\n")) : FString()) + FreeModeInstruction.ToString(); }
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
	if (bFreeMode) { Text = TEXT("자유모드 | 1 동승석 · 2 2열 · Home 운전석 · R 처음부터 · F6 레벨 다시 열기"); }
	else if (!bRunning && !bFinished)
	{ Text = ValidationProblems.IsEmpty() ? TEXT("정렬 필요 — P0-c 레벨에서 C로 저장 (O: 무시)") : ValidationProblems; }
	else if (bRunning && CurrentStep())
	{
		const auto* Step = CurrentStep();
		if (NeedsConfirmation()) { Text = RuleSatisfied() ? TEXT("진행자 확인 대기 — Space") : TEXT("규칙 대기 — 만족 후 Space"); }
		else { Text = TEXT("규칙 대기 — → 건너뛰기"); }
		if (Alignment && !Alignment->IsBaseViewpoint()) { Text += TEXT(" | Home 운전석으로"); }
		if (Step->ConfirmBy == EVBoRConfirmBy::Participant) { Text = TEXT("참가자 확인 대기 — → 건너뛰기") + (Alignment && !Alignment->IsBaseViewpoint() ? FString(TEXT(" | Home 운전석으로")) : FString()); }
	}
	if (!FloorRouteNotice.IsEmpty()) { Text += FloorRouteNotice.Contains(TEXT("unknown")) ? TEXT("\n바닥 경로 숨김 — 영역 모름") : TEXT("\n바닥 경로 숨김 — 플레이 영역 밖"); }
	if (!bOperatorInfo) { return Text; }
	Text += TEXT("\n") + (Alignment ? Alignment->Describe() : FString(TEXT("정렬 컴포넌트 없음")));
	Text += FString::Printf(TEXT(" | 정렬 무시 %s\n이전 %d 재시도 %d 건너뛰기 %d | 버림 단계 %d 권한 %d 규칙 %d 전환 %d 잠금 %d"), bAlignmentIgnored ? TEXT("예") : TEXT("아니오"), PreviousCount, RetryCount, SkipCount, StaleDiscards, PermissionDiscards, RuleDiscards, TransitionDiscards, ConfirmLockDiscards);
	Text += TEXT("\n") + LastViewRequest + TEXT("\n") + LastCompletion;
	if (bFreeMode) { Text += FString::Printf(TEXT("\n진행자 End %d"), EndCount); }
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
	// 좌석에서는 차 가운데 쪽, 서 있을 때는 오른쪽. 판 사이 2 cm 확보 (P3b D8)
	double Side = 1;
	if (bOperatorInfo && Alignment && ((Seat && Seat->IsHeadInside()) || !Alignment->IsBaseViewpoint()))
	{
		const FVector TowardCenter = Alignment->GetOwner()->GetActorLocation() - Panel->GetComponentLocation();
		Side = FVector::DotProduct(-Panel->GetRightVector(), TowardCenter) >= 0 ? 1 : -1;
	}
	const FVector OperatorOffset = bOperatorInfo ? -Panel->GetRightVector() * Side * (PanelSizeCm.X * 0.5 + 25 + 2)
		: -Panel->GetForwardVector() * (ButtonDistanceCm - 5 - PanelDistanceCm) - FVector(0, 0, PanelSizeCm.Y * 0.5 - 1);
	OperatorPanel->SetWorldLocationAndRotation(Panel->GetComponentLocation() + OperatorOffset, Panel->GetComponentRotation());
	Panel->SetHiddenInGame(bPanelPlacementPending); OperatorPanel->SetHiddenInGame(bPanelPlacementPending || Operator.IsEmpty());
	SetP3ButtonVisible(ConfirmButton, CanParticipantConfirm());
	const auto* Step = CurrentStep();
	const bool bAway = Alignment && !Alignment->IsBaseViewpoint();
	const bool bNeedsView = Step && Step->Rule == EVBoRStepRule::Viewpoint && !RuleSatisfied();
	PanelAction = bNeedsView ? EVBoRFlowCommand::Viewpoint : EVBoRFlowCommand::ReturnBase;
	SetP3ButtonVisible(ActionButton, !bPanelPlacementPending && bRunning && Alignment && !Alignment->IsSwitching() && (bAway || bNeedsView));
	TArray<FString> SeatLabels;
	if (bFreeMode)
	{
		TArray<EVBoRFlowCommand> Seats;
		if (bAway) { Seats.Add(EVBoRFlowCommand::ReturnBase); SeatLabels.Add(TEXT("운전석")); }
		for (const auto Action : {EVBoRFlowCommand::Passenger, EVBoRFlowCommand::Rear})
		{
			FString Problems;
			auto* View = P3Component<UVBoRViewpointComponent>(FindP3Tag(GetWorld(), Action == EVBoRFlowCommand::Passenger ? FName(TEXT("Car.PassengerView")) : FName(TEXT("Car.RearView")), Problems));
			if (View && (!Alignment || !Alignment->IsAtViewpoint(View)))
			{ Seats.Add(Action); SeatLabels.Add(Action == EVBoRFlowCommand::Passenger ? TEXT("동승석") : TEXT("2열")); }
		}
		const bool bVisible = !bPanelPlacementPending && !bReloadPending && Alignment && !Alignment->IsSwitching() && Seats.Num() == 2;
		for (int32 I = 0; I < 2; ++I)
		{
			if (Seats.IsValidIndex(I)) { FreeSeatActions[I] = Seats[I]; }
			SetP3ButtonVisible(I == 0 ? ConfirmButton.Get() : ActionButton.Get(), bVisible);
		}
	}
	for (int32 I = 0; I < 2; ++I)
	{
		AVBoRPokeButton* Button = I == 0 ? ConfirmButton.Get() : ActionButton.Get();
		ButtonLabels[I]->SetHiddenInGame(!Button || Button->IsHidden());
		if (ButtonLabelTexts.IsValidIndex(I)) { ButtonLabelTexts[I]->SetText(FText::FromString(bFreeMode && SeatLabels.IsValidIndex(I) ? SeatLabels[I] : I == 0 ? FString(TEXT("확인")) : ActionLabel())); }
	}
}

void AVBoRScenarioRunner::UpdateMarker()
{
	AVBoRFloorRoute* Route = bRunning && bRouteAllowed && !bRouteArrived && Routes.IsValidIndex(StepIndex) ? Routes[StepIndex].Get() : nullptr;
	if (Route)
	{
		MarkerArrow->SetHiddenInGame(false);
		MarkerArrow->SetWorldLocationAndRotation(P3RouteEnd(Route) + FVector(0, 0, 160), FRotator(-90, 0, 0));
		for (auto& Part : MarkerRing) { Part->SetHiddenInGame(true); }
		return;
	}
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

void AVBoRScenarioRunner::BeginRoute()
{
	for (TActorIterator<AVBoRFloorRoute> It(GetWorld()); It; ++It) { It->SetRouteVisible(false); }
	bRouteArrived = false; bRouteAllowed = false; bRouteAreaUnknown = false; RouteDwellSeconds = 0; FloorRouteNotice.Reset();
	CheckRouteArea();
}

void AVBoRScenarioRunner::CheckRouteArea()
{
	AVBoRFloorRoute* Route = bRunning && !bFreeMode && Routes.IsValidIndex(StepIndex) ? Routes[StepIndex].Get() : nullptr;
	if (!Route) { return; }
	const FVector2D Bounds = UHeadMountedDisplayFunctionLibrary::GetPlayAreaBounds(EHMDTrackingOrigin::Stage);
	const APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0);
	const bool bKnown = Pawn && !Bounds.ContainsNaN() && Bounds.X > 0 && Bounds.Y > 0;
	bRouteAreaUnknown = !bKnown;
	bool bInside = bKnown;
	if (bKnown)
	{
		for (const FVector& Point : P3RouteSamples(Route))
		{
			const FVector Stage = Pawn->GetActorTransform().InverseTransformPosition(Point);
			if (FMath::Abs(Stage.X) > Bounds.X * 0.5 - 30 || FMath::Abs(Stage.Y) > Bounds.Y * 0.5 - 30) { bInside = false; break; }
		}
	}
	bRouteAllowed = bInside;
	if (bInside) { Route->SetRouteVisible(true); FloorRouteNotice.Reset(); UpdateMarker(); }
	else
	{
		const FString Notice = bKnown ? TEXT("Floor route hidden — outside the play area") : TEXT("Floor route hidden — play area unknown");
		if (FloorRouteNotice != Notice) { FloorRouteNotice = Notice; Record(CurrentStep()->StepId.ToString() + TEXT(": ") + FloorRouteNotice); }
	}
}

void AVBoRScenarioRunner::SampleRoute(bool bTracked, const FTransform& Head, double DeltaSeconds)
{
	// 모를 때만 재조회함. 알려진 영역 밖 판정은 단계 안에서 고정 (P4d D5)
	if (bRouteAreaUnknown) { CheckRouteArea(); }
	AVBoRFloorRoute* Route = bRunning && bRouteAllowed && !bRouteArrived && Routes.IsValidIndex(StepIndex) ? Routes[StepIndex].Get() : nullptr;
	if (!Route || !bTracked) { return; }
	if (FVector::DistSquared2D(Head.GetLocation(), P3RouteEnd(Route)) > FMath::Square(Route->RingRadiusCm)) { RouteDwellSeconds = 0; return; }
	RouteDwellSeconds += FMath::Max(0.0, DeltaSeconds);
	if (RouteDwellSeconds < 0.5) { return; }
	bRouteArrived = true; Route->SetRouteVisible(false);
	PlacePanelAtHead(Head, true); UpdateMarker();
}

void AVBoRScenarioRunner::RecordStartFallback()
{
	if (!bStartHeadPending || StepIndex != 0 || !StartMarker) { return; }
	FTransform Head; const bool bTracked = Alignment && Alignment->ReadWorldHead(Head);
	if (!RecordHead(TEXT("시작 위치 — 시작 표시를 거치지 않음"), bTracked, Head))
	{ Record(TEXT("시작 위치 — 시작 표시를 거치지 않음 | 추적 없음 (좌표 보류)")); }
	bStartHeadPending = false;
}

void AVBoRScenarioRunner::Record(const FString& Text) { LastRecord = Text; UE_LOG(LogP3Flow, Log, TEXT("%s"), *Text); }
void AVBoRScenarioRunner::UnbindKeys()
{
	for (const auto& Entry : KeyCounts)
	{
		if (!Entry.Key.IsValid()) { continue; }
		Entry.Key->OnUnlockPressed.RemoveDynamic(this, &AVBoRScenarioRunner::HandleKeyUnlock);
		Entry.Key->OnLockPressed.RemoveDynamic(this, &AVBoRScenarioRunner::HandleKeyLock);
	}
	KeyCounts.Reset();
}
void AVBoRScenarioRunner::HandleKeyUnlock(AVBoRKeyFob* Key) { if (FIntPoint* Counts = KeyCounts.Find(Key)) { ++Counts->X; } }
void AVBoRScenarioRunner::HandleKeyLock(AVBoRKeyFob* Key) { if (FIntPoint* Counts = KeyCounts.Find(Key)) { ++Counts->Y; } }
void AVBoRScenarioRunner::HandleConfirmPoke(const FIsdkInteractionPointerEvent&) { QueueLive(bFreeMode ? FreeSeatActions[0] : EVBoRFlowCommand::Confirm, EVBoRCommandSource::Participant); }
void AVBoRScenarioRunner::HandleActionPoke(const FIsdkInteractionPointerEvent&) { QueueLive(bFreeMode ? FreeSeatActions[1] : PanelAction, EVBoRCommandSource::Participant); }
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
void AVBoRScenarioRunner::KeyFreeMode() { QueueLive(EVBoRFlowCommand::EnterFreeMode, EVBoRCommandSource::Operator); }
void AVBoRScenarioRunner::KeyReloadLevel() { QueueLive(EVBoRFlowCommand::ReloadLevel, EVBoRCommandSource::Operator); }
void AVBoRScenarioRunner::KeyReapply() { QueueLive(EVBoRFlowCommand::ReapplyAlignment, EVBoRCommandSource::Operator); }
void AVBoRScenarioRunner::KeyFoldSeat() { QueueLive(EVBoRFlowCommand::FoldSeat, EVBoRCommandSource::Operator); }
void AVBoRScenarioRunner::KeyUnfoldSeat() { QueueLive(EVBoRFlowCommand::UnfoldSeat, EVBoRCommandSource::Operator); }
void AVBoRScenarioRunner::KeyIgnore() { QueueLive(EVBoRFlowCommand::IgnoreAlignment, EVBoRCommandSource::Operator); }
void AVBoRScenarioRunner::KeyOperatorInfo()
{
	const auto Decision = EvaluateOperatorButton(EVBoROperatorButton::Info);
	LastOperatorCommand = FString(TEXT("In-VR operator info — ")) + (Decision.bAccepted ? TEXT("accepted") : TEXT("discarded (") + Decision.Reason + TEXT(")"));
	if (!Decision.bAccepted) { ++TransitionDiscards; Record(Decision.Record); return; }
	bOperatorInfo = !bOperatorInfo; HeadLogElapsed = 0;
	Record(bOperatorInfo ? TEXT("진행자 정보 켬 (I)\n") + GetOperatorText() : TEXT("진행자 정보 끔 (I)")); RefreshPanel();
}
