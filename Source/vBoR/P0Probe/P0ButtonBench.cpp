#include "P0ButtonBench.h"

#include "P0ProbePawn.h"
#include "../Interaction/VBoRHandRigPawn.h"
#include "../Interaction/VBoRPokeButton.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "Interaction/IsdkPokeInteractable.h"
#include "Interaction/IsdkPokeInteractor.h"
#include "Kismet/GameplayStatics.h"
#include "Subsystem/IsdkWorldSubsystem.h"

AP0ButtonBench::AP0ButtonBench()
{
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("BenchOrigin")));
	Panel = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Panel"));
	Panel->SetupAttachment(RootComponent);
	Panel->SetHorizontalAlignment(EHTA_Left);
	Panel->SetVerticalAlignment(EVRTA_TextTop);
	Panel->SetWorldSize(0.8f);
	Panel->SetTextRenderColor(FColor::White);
	ButtonClass = AVBoRPokeButton::StaticClass();
}

void AP0ButtonBench::BeginPlay()
{
	Super::BeginPlay();
	HandPawn = Cast<AVBoRHandRigPawn>(UGameplayStatics::GetPlayerPawn(this, 0));
	Panel->SetRelativeLocation(PanelOffsetCm);
	Panel->SetRelativeRotation(FRotator(0, 180, 0));
	for (int32 Index = 0; Index < SingleSizesMm.Num(); ++Index)
	{
		SpawnButton(FString::Printf(TEXT("Single %.0f mm"), SingleSizesMm[Index]),
			FVector(0, (Index - (SingleSizesMm.Num() - 1) / 2.0) * SingleSpacingCm, 0), SingleSizesMm[Index]);
	}
	ClusterStart = Buttons.Num();
	const int32 Rows = FMath::Clamp(ClusterRows, 1, 12);
	const int32 Columns = FMath::Clamp(ClusterColumns, 1, 12);
	for (int32 Row = 0; Row < Rows; ++Row)
	{
		for (int32 Column = 0; Column < Columns; ++Column)
		{
			// 표시 mm → UE cm. 보이는 크기는 판정 배율과 별개임 (P0-b D2, U-04)
			SpawnButton(FString::Printf(TEXT("Cluster %d,%d"), Row + 1, Column + 1), ClusterOffsetCm +
				FVector(0, (Column - (Columns - 1) / 2.0) * ClusterSpacingMm / 10.0,
					((Rows - 1) / 2.0 - Row) * ClusterSpacingMm / 10.0), ClusterSizeMm);
		}
	}
	FarIndex = Buttons.Num();
	SpawnButton(TEXT("Far"), FarOffsetCm, FarSizeMm);
	Presses[0].SetNum(Buttons.Num());
	Presses[1].SetNum(Buttons.Num());
	RigStatus = HandPawn ? HandPawn->DescribeRig() : TEXT("Hand rig pawn unavailable");
	RecordEvent(RigStatus);
	for (int32 Index = 0; Index < Buttons.Num(); ++Index) { RecordEvent(Names[Index] + TEXT(" normal")); }
	// 이벤트 전달이 끝난 frame 에 센 값만 사용함 (순서에 따른 순간 중복 제외, P0-b §4)
	UIsdkWorldSubsystem::Get(GetWorld()).GetFrameFinishedEventDelegate().AddDynamic(this, &AP0ButtonBench::HandleFrameFinished);
	HandleFrameFinished();
}

void AP0ButtonBench::SpawnButton(const FString& Name, const FVector& Offset, float SizeMm)
{
	// Actor +Z 윗면이 참가자(-X)를 보게 함 (P0-b D4)
	const FTransform Transform(FRotator(90, GetActorRotation().Yaw, 0), GetActorTransform().TransformPosition(Offset));
	AVBoRPokeButton* Button = GetWorld()->SpawnActorDeferred<AVBoRPokeButton>(ButtonClass, Transform, this, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Button) { return; }
	Button->ButtonMesh->SetRelativeScale3D(FVector(SizeMm / 1000.0, SizeMm / 1000.0, 0.006));
	Button->PressDepthMm = PressDepthMm;
	Button->HitAreaScale = HitAreaScale;
	Button->FinishSpawning(Transform);
	Buttons.Add(Button);
	Names.Add(Name);
	Button->Poke->GetInteractionPointerEventDelegate().AddUniqueDynamic(this, &AP0ButtonBench::HandlePointer);
	Button->Poke->GetInteractableStateChangedDelegate()->AddUniqueDynamic(this, &AP0ButtonBench::HandleState);
}

void AP0ButtonBench::RecordEvent(const FString& Text)
{
	const FString Event = FString::Printf(TEXT("[%.3f s] %s"), GetWorld()->GetTimeSeconds(), *Text);
	UE_LOG(LogP0Probe, Log, TEXT("%s"), *Event);
	RecentEvents.Add(Event);
	if (RecentEvents.Num() > 10) { RecentEvents.RemoveAt(0); }
}

void AP0ButtonBench::SamplePress(int32 Hand, int32 Button)
{
	FVector Tip = FVector::ZeroVector;
	const bool bValid = HandPawn && HandPawn->ReadRawIndexTip(Hand, Tip);
	// 추적 손실로 생긴 놓임은 누름 완료로 세지 않음 (P0-b §2)
	if (!bValid) { Presses[Hand][Button].Cancel(); return; }
	const double MmPerUnit = 1000.0 / GetWorld()->GetWorldSettings()->WorldToMeters;
	Presses[Hand][Button].Sample(bValid, Tip, Buttons[Button]->GetRestFrontWorld(), Buttons[Button]->GetActorUpVector(), MmPerUnit);
}

void AP0ButtonBench::HandlePointer(const FIsdkInteractionPointerEvent& Event)
{
	const int32 ButtonIndex = Buttons.IndexOfByPredicate([&Event](const AVBoRPokeButton* Button) { return Button->Poke.Get() == Event.Interactable.GetObject(); });
	if (ButtonIndex == INDEX_NONE || Event.Type == EIsdkPointerEventType::Move) { return; }
	RecordEvent(Names[ButtonIndex] + TEXT(" ") + StaticEnum<EIsdkPointerEventType>()->GetNameStringByValue(static_cast<int64>(Event.Type)));
	if (ButtonIndex == FarIndex)
	{
		if (Event.Type == EIsdkPointerEventType::Hover) { ++FarHoverCount; }
		if (Event.Type == EIsdkPointerEventType::Select) { ++FarSelectCount; }
	}
	int32 Hand = INDEX_NONE;
	if (HandPawn)
	{
		for (int32 Index = 0; Index < 2; ++Index)
		{
			if (HandPawn->GetHandRig(Index)->GetPokeInteractor() == Event.Interactor) { Hand = Index; }
		}
	}
	// Cancel 은 interactor 가 지워진 뒤 올 수도 있음. 완료로 세지 않음 (P0-b §2)
	if (Hand == INDEX_NONE)
	{
		if (Event.Type == EIsdkPointerEventType::Cancel)
		{
			for (int32 Index = 0; Index < 2; ++Index) { Presses[Index][ButtonIndex].Cancel(); }
		}
		return;
	}
	FP0ButtonPressMeasurement& Press = Presses[Hand][ButtonIndex];
	const double Time = GetWorld()->GetTimeSeconds();
	const bool bCluster = ButtonIndex >= ClusterStart && ButtonIndex < FarIndex;
	switch (Event.Type)
	{
	case EIsdkPointerEventType::Hover:
		Press.Hover(Time);
		if (bCluster) { Targets[Hand].Hover(ButtonIndex); }
		SamplePress(Hand, ButtonIndex);
		break;
	case EIsdkPointerEventType::Select:
		Press.Select(Time);
		SamplePress(Hand, ButtonIndex);
		break;
	case EIsdkPointerEventType::Unselect:
		SamplePress(Hand, ButtonIndex);
		if (Press.Unselect())
		{
			LastPress = FString::Printf(TEXT("%s %s | hover->select %s | max raw depth %s | target switches %d"), *Names[ButtonIndex], Hand == 0 ? TEXT("Left") : TEXT("Right"),
				Press.HoverToSelectSeconds >= 0 ? *FString::Printf(TEXT("%.3f s"), Press.HoverToSelectSeconds) : TEXT("n/a"),
				Press.bHasDepth ? *FString::Printf(TEXT("%.2f mm"), Press.MaxDepthMm) : TEXT("n/a"), bCluster ? Targets[Hand].Switches : 0);
			RecordEvent(LastPress);
			if (bCluster) { Targets[Hand].Reset(); }
		}
		break;
	case EIsdkPointerEventType::Unhover: Press.Unhover(); break;
	case EIsdkPointerEventType::Cancel: Press.Cancel(); if (bCluster) { Targets[Hand].Reset(); } break;
	default: break;
	}
}

void AP0ButtonBench::HandleState(const FIsdkInteractableStateEvent& Event)
{
	const int32 Index = Buttons.IndexOfByPredicate([&Event](const AVBoRPokeButton* Button) { return Button->Poke.Get() == Event.Interactable.GetObject(); });
	if (Index != INDEX_NONE)
	{
		RecordEvent(Names[Index] + TEXT(" state ") + StaticEnum<EIsdkInteractableState>()->GetNameStringByValue(static_cast<int64>(Event.Args.NewState)));
	}
}

void AP0ButtonBench::HandleFrameFinished()
{
	int32 HoverCount = 0;
	for (int32 Index = ClusterStart; Index < FarIndex; ++Index)
	{
		if (Buttons[Index]->Poke->GetCurrentState() == EIsdkInteractableState::Hover) { ++HoverCount; }
	}
	Hovers.Update(HoverCount);
	FString LivePress;
	for (int32 Hand = 0; Hand < 2; ++Hand)
	{
		bool bClusterActive = false;
		for (int32 Index = 0; Index < Buttons.Num(); ++Index)
		{
			const FP0ButtonPressMeasurement& Press = Presses[Hand][Index];
			if (Press.bHover || Press.bSelected)
			{
				SamplePress(Hand, Index);
				LivePress += FString::Printf(TEXT("\n%s %s depth %s"), *Names[Index], Hand == 0 ? TEXT("Left") : TEXT("Right"),
					Press.bHasDepth ? *FString::Printf(TEXT("%.2f mm"), Press.MaxDepthMm) : TEXT("n/a"));
				bClusterActive |= Index >= ClusterStart && Index < FarIndex;
			}
		}
		if (!bClusterActive) { Targets[Hand].Reset(); }
	}
	const FString Status = FString::Printf(TEXT("P0-b | not-tested | temporary sizes\n%s\nCluster hover: %d | max: %d | >=2 episodes: %d\nTarget switches: L=%d R=%d | Far hover=%d select=%d\nLast press: %s%s"),
		*RigStatus, Hovers.Current, Hovers.Maximum, Hovers.MultipleEpisodes, Targets[0].Switches, Targets[1].Switches, FarHoverCount, FarSelectCount, *LastPress, *LivePress);
	FString Text = Status + TEXT("\nRecent events (10):");
	for (const FString& Event : RecentEvents) { Text += TEXT("\n") + Event; }
	Panel->SetText(FText::FromString(Text));
	const double Time = GetWorld()->GetTimeSeconds();
	if (Time - LastLogAt >= 1.0)
	{
		LastLogAt = Time;
		FString Line = Status;
		Line.ReplaceInline(TEXT("\n"), TEXT(" | "));
		UE_LOG(LogP0Probe, Log, TEXT("%s"), *Line);
	}
}

void AP0ButtonBench::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UIsdkWorldSubsystem::Get(GetWorld()).GetFrameFinishedEventDelegate().RemoveDynamic(this, &AP0ButtonBench::HandleFrameFinished);
	for (AVBoRPokeButton* Button : Buttons)
	{
		if (!IsValid(Button)) { continue; }
		Button->Poke->GetInteractionPointerEventDelegate().RemoveDynamic(this, &AP0ButtonBench::HandlePointer);
		Button->Poke->GetInteractableStateChangedDelegate()->RemoveDynamic(this, &AP0ButtonBench::HandleState);
		Button->Destroy();
	}
	Super::EndPlay(EndPlayReason);
}
