#include "P0ProbePawn.h"

#include "Camera/CameraComponent.h"
#include "Components/InputComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/WorldSettings.h"
#include "IXRTrackingSystem.h"
#include "MotionControllerComponent.h"
#include "OculusXRHandComponent.h"

DEFINE_LOG_CATEGORY(LogP0Probe);

namespace
{
	// Private FOculusKey 는 include 못 함. 정의된 문자열 그대로 사용 (P0-a §2)
	const TCHAR* PinchKeys[2][5] = {
		{ TEXT("OculusHand_Left_ThumbPinch"), TEXT("OculusHand_Left_IndexPinch"), TEXT("OculusHand_Left_MiddlePinch"), TEXT("OculusHand_Left_RingPinch"), TEXT("OculusHand_Left_PinkPinch") },
		{ TEXT("OculusHand_Right_ThumbPinch"), TEXT("OculusHand_Right_IndexPinch"), TEXT("OculusHand_Right_MiddlePinch"), TEXT("OculusHand_Right_RingPinch"), TEXT("OculusHand_Right_PinkPinch") }
	};
	const TCHAR* StrengthKeys[2][5] = {
		{ TEXT("OculusHand_Left_ThumbPinchStrength"), TEXT("OculusHand_Left_IndexPinchStrength"), TEXT("OculusHand_Left_MiddlePinchStrength"), TEXT("OculusHand_Left_RingPinchStrength"), TEXT("OculusHand_Left_PinkPinchStrength") },
		{ TEXT("OculusHand_Right_ThumbPinchStrength"), TEXT("OculusHand_Right_IndexPinchStrength"), TEXT("OculusHand_Right_MiddlePinchStrength"), TEXT("OculusHand_Right_RingPinchStrength"), TEXT("OculusHand_Right_PinkPinchStrength") }
	};
	const TCHAR* GestureKeys[2][5] = {
		{ TEXT("OculusHand_Left_MicrogestureSwipeLeft"), TEXT("OculusHand_Left_MicrogestureSwipeRight"), TEXT("OculusHand_Left_MicrogestureSwipeForward"), TEXT("OculusHand_Left_MicrogestureSwipeBack"), TEXT("OculusHand_Left_MicrogestureTapThumb") },
		{ TEXT("OculusHand_Right_MicrogestureSwipeLeft"), TEXT("OculusHand_Right_MicrogestureSwipeRight"), TEXT("OculusHand_Right_MicrogestureSwipeForward"), TEXT("OculusHand_Right_MicrogestureSwipeBack"), TEXT("OculusHand_Right_MicrogestureTapThumb") }
	};
	const TCHAR* HandNames[2] = { TEXT("Left"), TEXT("Right") };
	const TCHAR* FingerNames[5] = { TEXT("Thumb"), TEXT("Index"), TEXT("Middle"), TEXT("Ring"), TEXT("Pink") };
	const TCHAR* GestureNames[5] = { TEXT("SwipeLeft"), TEXT("SwipeRight"), TEXT("SwipeForward"), TEXT("SwipeBack"), TEXT("TapThumb") };

	FString EventTime(double Time)
	{
		return Time < 0.0 ? TEXT("n/a") : FString::Printf(TEXT("%.3f s"), Time);
	}
}

AP0ProbePawn::AP0ProbePawn()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostUpdateWork;
	VROrigin = CreateDefaultSubobject<USceneComponent>(TEXT("VROrigin"));
	SetRootComponent(VROrigin);
	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(VROrigin);
	Camera->bLockToHmd = true;

	// ── 1) 손 표시 구성 (root 보정 회전은 plugin 에 맡김, P0-a §3) ──
	LeftMotion = CreateDefaultSubobject<UMotionControllerComponent>(TEXT("LeftMotion"));
	RightMotion = CreateDefaultSubobject<UMotionControllerComponent>(TEXT("RightMotion"));
	LeftHand = CreateDefaultSubobject<UOculusXRHandComponent>(TEXT("LeftHand"));
	RightHand = CreateDefaultSubobject<UOculusXRHandComponent>(TEXT("RightHand"));
	UMotionControllerComponent* Motions[2] = { LeftMotion, RightMotion };
	UOculusXRHandComponent* Hands[2] = { LeftHand, RightHand };
	for (int32 HandIndex = 0; HandIndex < 2; ++HandIndex)
	{
		Motions[HandIndex]->SetupAttachment(VROrigin);
		Motions[HandIndex]->SetTrackingMotionSource(FName(HandNames[HandIndex]));
		// 측정값과 표시 위치를 game thread 에 맞춤 (render thread 의 late update 제외)
		Motions[HandIndex]->bDisableLowLatencyUpdate = true;
		Hands[HandIndex]->SetupAttachment(Motions[HandIndex]);
		Hands[HandIndex]->SkeletonType = HandIndex == 0 ? EOculusXRHandType::HandLeft : EOculusXRHandType::HandRight;
		Hands[HandIndex]->MeshType = Hands[HandIndex]->SkeletonType;
		Hands[HandIndex]->ConfidenceBehavior = EOculusXRConfidenceBehavior::None;
		Hands[HandIndex]->SystemGestureBehavior = EOculusXRSystemGestureBehavior::None;
		Hands[HandIndex]->bInitializePhysics = false;
		Hands[HandIndex]->bUpdateHandScale = true;
		Hands[HandIndex]->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Hands[HandIndex]->AddTickPrerequisiteComponent(Motions[HandIndex]);
		AddTickPrerequisiteComponent(Hands[HandIndex]);
	}

	// ── 2) 레벨 고정 패널 구성 (머리에 붙이지 않음, P0-a §2) ──
	Panel = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Panel"));
	Panel->SetupAttachment(VROrigin);
	// Player camera 나 Pawn 이동에 따라가지 않게 world transform 고정 (P0-a §2)
	Panel->SetAbsolute(true, true, true);
	Panel->SetWorldSize(3.0f);
	Panel->SetTextRenderColor(FColor::White);
	Panel->SetHorizontalAlignment(EHTA_Left);
	Panel->SetVerticalAlignment(EVRTA_TextTop);
	Panel->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AP0ProbePawn::BeginPlay()
{
	Super::BeginPlay();
	ProbeStartedAt = GetWorld()->GetRealTimeSeconds();
	Panel->SetWorldTransform(PanelWorldTransform);
	IndexBone = FName(*UOculusXRInputFunctionLibrary::GetBoneName(EOculusXRBone::Index_Tip));
	ThumbBone = FName(*UOculusXRInputFunctionLibrary::GetBoneName(EOculusXRBone::Thumb_Tip));
}

double AP0ProbePawn::ProbeTime() const
{
	return GetWorld()->GetRealTimeSeconds() - ProbeStartedAt;
}

void AP0ProbePawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	for (int32 HandIndex = 0; HandIndex < 2; ++HandIndex)
	{
		for (int32 Finger = 0; Finger < 5; ++Finger)
		{
			const FKey Key{ FName(PinchKeys[HandIndex][Finger]) };
			PlayerInputComponent->BindKey(Key, IE_Pressed, this, &AP0ProbePawn::OnKeyPressed).bConsumeInput = false;
			PlayerInputComponent->BindKey(Key, IE_Released, this, &AP0ProbePawn::OnKeyReleased).bConsumeInput = false;
			PlayerInputComponent->BindKey(FKey(FName(GestureKeys[HandIndex][Finger])), IE_Pressed, this, &AP0ProbePawn::OnKeyPressed).bConsumeInput = false;
		}
	}
	PlayerInputComponent->BindKey(EKeys::AnyKey, IE_Pressed, this, &AP0ProbePawn::OnKeyboardPressed).bConsumeInput = false;
}

void AP0ProbePawn::RecordEvent(const FString& Event)
{
	const FString Line = FString::Printf(TEXT("[%.3f s] %s"), ProbeTime(), *Event);
	RecentEvents.Add(Line);
	if (RecentEvents.Num() > 10)
	{
		RecentEvents.RemoveAt(0);
	}
	UE_LOG(LogP0Probe, Log, TEXT("%s"), *Line);
}

void AP0ProbePawn::OnKeyPressed(FKey Key)
{
	for (int32 HandIndex = 0; HandIndex < 2; ++HandIndex)
	{
		for (int32 Finger = 0; Finger < 5; ++Finger)
		{
			if (Key == FKey(FName(PinchKeys[HandIndex][Finger])))
			{
				bPinched[HandIndex][Finger] = true;
				RecordEvent(FString::Printf(TEXT("%s pinch %s pressed"), HandNames[HandIndex], FingerNames[Finger]));
				return;
			}
			if (Key == FKey(FName(GestureKeys[HandIndex][Finger])))
			{
				RecordEvent(FString::Printf(TEXT("%s microgesture %s"), HandNames[HandIndex], GestureNames[Finger]));
				return;
			}
		}
	}
}

void AP0ProbePawn::OnKeyboardPressed(FKey Key)
{
	if (!Key.IsGamepadKey() && !Key.IsMouseButton() && !Key.IsAnalog() && !Key.IsGesture() && !Key.IsTouch())
	{
		LastKeyboardKey = Key.GetFName().ToString();
		RecordEvent(FString::Printf(TEXT("Keyboard %s pressed"), *LastKeyboardKey));
	}
}

void AP0ProbePawn::OnKeyReleased(FKey Key)
{
	for (int32 HandIndex = 0; HandIndex < 2; ++HandIndex)
	{
		for (int32 Finger = 0; Finger < 5; ++Finger)
		{
			if (Key == FKey(FName(PinchKeys[HandIndex][Finger])))
			{
				bPinched[HandIndex][Finger] = false;
				RecordEvent(FString::Printf(TEXT("%s pinch %s released"), HandNames[HandIndex], FingerNames[Finger]));
				return;
			}
		}
	}
}

FString AP0ProbePawn::FormatHand(int32 HandIndex, double Time) const
{
	const FP0ProbeMeasurement& State = Measurements[HandIndex];
	FString Text = FString::Printf(TEXT("%s: position %s | confidence %s | bones %s\nIndex world (cm): %s | Thumb world (cm): %s\nWindow diameter: %.2f mm | samples: %d\nConfidence lost: %s | recovered: %s | duration: %.3f s | jump: %s | moved in Low: %s\nPosition lost: %s | recovered: %s | duration: %.3f s | jump: %s\n"),
		HandNames[HandIndex], State.bPositionValid ? TEXT("valid") : TEXT("invalid"), State.bHighConfidence ? TEXT("High") : TEXT("Low"),
		bBonesAvailable[HandIndex] ? TEXT("ready") : TEXT("unavailable"),
		bBonesAvailable[HandIndex] ? *IndexPositions[HandIndex].ToString() : TEXT("n/a"),
		bBonesAvailable[HandIndex] ? *ThumbPositions[HandIndex].ToString() : TEXT("n/a"), State.DiameterMm, State.Samples.Num(),
		*EventTime(State.ConfidenceLostAt), *EventTime(State.ConfidenceRecoveredAt),
		!State.bHighConfidence && State.ConfidenceLostAt >= 0.0 ? Time - State.ConfidenceLostAt : State.ConfidenceLossDuration,
		State.bHasConfidenceJump ? *FString::Printf(TEXT("%.2f mm"), State.ConfidenceJumpMm) : TEXT("n/a"),
		State.bHasLowStartPosition ? *FString::Printf(TEXT("%.2f mm"), State.LowMaxMoveMm) : TEXT("n/a"),
		*EventTime(State.PositionLostAt), *EventTime(State.PositionRecoveredAt),
		!State.bPositionValid && State.PositionLostAt >= 0.0 ? Time - State.PositionLostAt : State.PositionLossDuration,
		State.bHasRecoveryDistance ? *FString::Printf(TEXT("%.2f mm"), State.RecoveryDistanceMm) : TEXT("n/a"));
	Text += TEXT("Pinch / strength: ");
	for (int32 Finger = 0; Finger < 5; ++Finger)
	{
		Text += FString::Printf(TEXT("%s=%s/%.3f "), FingerNames[Finger], bPinched[HandIndex][Finger] ? TEXT("down") : TEXT("up"), PinchStrengths[HandIndex][Finger]);
	}
	return Text;
}

void AP0ProbePawn::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const double Time = ProbeTime();
	// ── 1) 활성 XR 확인 (백엔드별 신호 비교용, P0-a §2) ──
	const FString CurrentXR = GEngine && GEngine->XRSystem.IsValid() ? GEngine->XRSystem->GetSystemName().ToString() : TEXT("None");
	if (CurrentXR != XRSystemName)
	{
		XRSystemName = CurrentXR;
		RecordEvent(FString::Printf(TEXT("XR system: %s"), *XRSystemName));
	}
	// ── 2) 손별 날것의 신호 측정 (필터 없이 비교, P0-a §2) ──
	// WorldToMeters 는 UE units/m. 기본 100 이면 cm -> mm 는 10 배 (P0-a §2)
	const double MmPerUnit = 1000.0 / GetWorld()->GetWorldSettings()->WorldToMeters;
	UOculusXRHandComponent* Hands[2] = { LeftHand, RightHand };
	APlayerController* PC = GetController<APlayerController>();
	for (int32 HandIndex = 0; HandIndex < 2; ++HandIndex)
	{
		UOculusXRHandComponent* Hand = Hands[HandIndex];
		const bool bValid = UOculusXRInputFunctionLibrary::IsHandPositionValid(Hand->SkeletonType);
		const bool bHigh = UOculusXRInputFunctionLibrary::GetTrackingConfidence(Hand->SkeletonType) == EOculusXRTrackingConfidence::High;
		bBonesAvailable[HandIndex] = Hand->bSkeletalMeshInitialized && Hand->GetBoneIndex(IndexBone) != INDEX_NONE && Hand->GetBoneIndex(ThumbBone) != INDEX_NONE;
		if (bBonesAvailable[HandIndex])
		{
			// UpdateBonePose 가 dirty 만 표시하므로 현재 frame 의 본 위치로 refresh (P0-a §4)
			Hand->RefreshBoneTransforms();
			IndexPositions[HandIndex] = Hand->GetBoneLocation(IndexBone, EBoneSpaces::WorldSpace);
			ThumbPositions[HandIndex] = Hand->GetBoneLocation(ThumbBone, EBoneSpaces::WorldSpace);
		}
		for (const FString& Event : Measurements[HandIndex].Update(Time, bValid, bHigh, IndexPositions[HandIndex], bBonesAvailable[HandIndex], MmPerUnit))
		{
			RecordEvent(FString::Printf(TEXT("%s %s"), HandNames[HandIndex], *Event));
		}
		for (int32 Finger = 0; Finger < 5; ++Finger)
		{
			PinchStrengths[HandIndex][Finger] = PC ? PC->GetInputAnalogKeyState(FKey(FName(StrengthKeys[HandIndex][Finger]))) : 0.0f;
		}
	}
	// ── 3) 패널·로그 표시 (연속 로그는 초당 한 줄, P0-a §2) ──
	const FString Status = FString::Printf(TEXT("P0-a | t=%.3f s | XR: %s | Keyboard: %s\n%s\n%s"), Time, *XRSystemName, *LastKeyboardKey, *FormatHand(0, Time), *FormatHand(1, Time));
	FString PanelText = Status + TEXT("\nRecent events (10):");
	for (const FString& Event : RecentEvents)
	{
		PanelText += TEXT("\n") + Event;
	}
	Panel->SetText(FText::FromString(PanelText));
	if (Time - LastContinuousLogAt >= 1.0)
	{
		LastContinuousLogAt = Time;
		FString LogLine = Status;
		LogLine.ReplaceInline(TEXT("\n"), TEXT(" | "));
		UE_LOG(LogP0Probe, Log, TEXT("%s"), *LogLine);
	}
}

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "EnhancedInputComponent.h"
#include "EnhancedPlayerInput.h"
#include "InputKeyEventArgs.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP0ProbeInputTest, "vBoR.P0Probe.NativeKeyBindings", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP0ProbeInputTest::RunTest(const FString& Parameters)
{
	// InputKey 는 World 가 필요함. 실제 HMD 없는 합성 입력 시험용 (P0-a §4)
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	APlayerController* Controller = World->SpawnActor<APlayerController>();
	UEnhancedPlayerInput* Input = NewObject<UEnhancedPlayerInput>(Controller);
	Controller->PlayerInput = Input;
	UEnhancedInputComponent* Component = NewObject<UEnhancedInputComponent>(Controller);
	const TArray<UInputComponent*> Stack{ Component };
	for (int32 HandIndex = 0; HandIndex < 2; ++HandIndex)
	{
		for (int32 Finger = 0; Finger < 5; ++Finger)
		{
			for (const TCHAR* KeyName : { PinchKeys[HandIndex][Finger], GestureKeys[HandIndex][Finger] })
			{
				const FKey Key{ FName(KeyName) };
				TestTrue(KeyName, Key.IsValid());
				int32 Pressed = 0;
				int32 Released = 0;
				FInputKeyBinding PressBinding(FInputChord(Key), IE_Pressed);
				PressBinding.bConsumeInput = false;
				PressBinding.KeyDelegate.GetDelegateForManualSet().BindLambda([&Pressed]() { ++Pressed; });
				FInputKeyBinding ReleaseBinding(FInputChord(Key), IE_Released);
				ReleaseBinding.bConsumeInput = false;
				ReleaseBinding.KeyDelegate.GetDelegateForManualSet().BindLambda([&Released]() { ++Released; });
				Component->KeyBindings = { PressBinding, ReleaseBinding };
				Input->InputKey(FInputKeyEventArgs::CreateSimulated(Key, IE_Pressed, 1.0f));
				Input->ProcessInputStack(Stack, 1.0f / 90.0f, false);
				Input->InputKey(FInputKeyEventArgs::CreateSimulated(Key, IE_Released, 0.0f));
				Input->ProcessInputStack(Stack, 1.0f / 90.0f, false);
				TestEqual(FString::Printf(TEXT("%s press"), KeyName), Pressed, 1);
				TestEqual(FString::Printf(TEXT("%s release"), KeyName), Released, 1);
				Component->KeyBindings.Reset();
			}
			const FKey Strength{ FName(StrengthKeys[HandIndex][Finger]) };
			TestTrue(StrengthKeys[HandIndex][Finger], Strength.IsValid());
			Input->InputKey(FInputKeyEventArgs::CreateSimulated(Strength, IE_Axis, 0.375f, 1));
			Input->ProcessInputStack(Stack, 1.0f / 90.0f, false);
			TestEqual(TEXT("Analog key state"), Input->GetKeyValue(Strength), 0.375f);
		}
	}
	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP0ProbeRecoveryTest, "vBoR.P0Probe.Recovery", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP0ProbeRecoveryTest::RunTest(const FString& Parameters)
{
	FP0ProbeMeasurement State;
	State.Update(0.0, true, true, FVector(2, 0, 0), true, 10.0);
	State.Update(0.1, true, true, FVector(3, 0, 0), true, 10.0);
	TestEqual(TEXT("Confidence loss is independent"), State.Update(0.2, true, false, FVector(4, 0, 0), true, 10.0).Num(), 1);
	TestEqual(TEXT("Position loss is independent"), State.Update(0.3, false, false, FVector::ZeroVector, true, 10.0).Num(), 1);
	State.Update(0.4, false, true, FVector(1000, 0, 0), true, 10.0);
	State.Update(0.5, true, true, FVector(7, 0, 0), true, 10.0);
	TestEqual(TEXT("Recovery uses last valid frame, including Low"), State.RecoveryDistanceMm, 30.0);
	TestTrue(TEXT("Confidence duration"), FMath::IsNearlyEqual(State.ConfidenceLossDuration, 0.2));
	TestTrue(TEXT("Position duration"), FMath::IsNearlyEqual(State.PositionLossDuration, 0.2));
	TestEqual(TEXT("Invalid samples never enter window"), State.Samples.Num(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP0ProbeConfidenceRecoveryTest, "vBoR.P0Probe.ConfidenceRecovery", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP0ProbeConfidenceRecoveryTest::RunTest(const FString& Parameters)
{
	// Low 구간 중 무효 frame 이 껴도 기준점·이동값이 오염되지 않는지 확인 (P0-a §4)
	FP0ProbeMeasurement State;
	State.Update(0.0, true, true, FVector(0, 0, 0), true, 10.0);
	State.Update(0.1, true, true, FVector(1, 0, 0), true, 10.0);
	State.Update(0.2, true, false, FVector(2, 0, 0), true, 10.0);
	State.Update(0.3, true, false, FVector(5, 0, 0), true, 10.0);
	State.Update(0.4, false, false, FVector(100, 0, 0), true, 10.0);
	State.Update(0.5, true, false, FVector(3, 0, 0), true, 10.0);
	State.Update(0.6, true, true, FVector(7, 0, 0), true, 10.0);
	TestTrue(TEXT("Confidence jump measured"), State.bHasConfidenceJump);
	TestEqual(TEXT("Jump uses last High frame, not Low frames"), State.ConfidenceJumpMm, 60.0);
	TestEqual(TEXT("Max move in Low excludes invalid frame"), State.LowMaxMoveMm, 30.0);
	State.Update(0.7, true, false, FVector(7, 0, 0), true, 10.0);
	TestEqual(TEXT("New Low segment resets move"), State.LowMaxMoveMm, 0.0);

	FP0ProbeMeasurement Frozen;
	Frozen.Update(0.0, true, true, FVector(0, 0, 0), true, 10.0);
	Frozen.Update(0.1, true, false, FVector(4, 0, 0), true, 10.0);
	Frozen.Update(0.2, true, false, FVector(4, 0, 0), true, 10.0);
	Frozen.Update(0.3, true, true, FVector(4, 0, 0), true, 10.0);
	TestEqual(TEXT("Frozen Low position reads as zero move"), Frozen.LowMaxMoveMm, 0.0);
	TestEqual(TEXT("Frozen case jump"), Frozen.ConfidenceJumpMm, 40.0);

	FP0ProbeMeasurement InitialLow;
	InitialLow.Update(0.0, true, false, FVector(0, 0, 0), true, 10.0);
	InitialLow.Update(0.1, true, true, FVector(5, 0, 0), true, 10.0);
	TestFalse(TEXT("No High reference before initial Low"), InitialLow.bHasConfidenceJump);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP0ProbeWindowTest, "vBoR.P0Probe.WindowDiameter", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP0ProbeWindowTest::RunTest(const FString& Parameters)
{
	FP0ProbeMeasurement State;
	State.Update(0.0, true, true, FVector(0, 0, 0), true, 10.0);
	State.Update(0.5, true, true, FVector(3, 4, 0), true, 10.0);
	TestEqual(TEXT("Diameter in mm"), State.DiameterMm, 50.0);
	State.Update(1.1, true, true, FVector(3, 4, 0), true, 10.0);
	TestEqual(TEXT("Expired sample excluded"), State.DiameterMm, 0.0);
	TestEqual(TEXT("Sliding sample count"), State.Samples.Num(), 2);
	State.Update(1.2, true, false, FVector(8, 0, 0), true, 10.0);
	TestEqual(TEXT("Low clears window"), State.Samples.Num(), 0);
	State.Update(1.3, true, true, FVector(8, 0, 0), true, 10.0);
	State.Update(1.4, false, true, FVector(9, 0, 0), true, 10.0);
	TestEqual(TEXT("Invalid clears window"), State.Samples.Num(), 0);
	State.Update(1.5, true, true, FVector::ZeroVector, false, 10.0);
	TestEqual(TEXT("Missing skeleton is not a zero-position sample"), State.Samples.Num(), 0);
	return true;
}
#endif

TArray<FString> FP0ProbeMeasurement::Update(double Time, bool bValid, bool bHigh, const FVector& IndexPosition, bool bBonesAvailable, double MmPerUnit)
{
	TArray<FString> Events;
	// ── 1) 신뢰도·위치 상태 별도 비교 (같은 frame 에 바뀐다는 보장 없음, P0-a §4) ──
	if (bInitialized && bHigh != bHighConfidence)
	{
		if (!bHigh)
		{
			ConfidenceLostAt = Time;
			// 새 Low 구간 시작. 이전 구간 이동값은 버림
			bHasLowStartPosition = false;
			LowMaxMoveMm = 0.0;
			Events.Add(TEXT("confidence High -> Low"));
		}
		else
		{
			ConfidenceRecoveredAt = Time;
			ConfidenceLossDuration = ConfidenceLostAt >= 0.0 ? Time - ConfidenceLostAt : 0.0;
			// 가려짐은 대개 Low + 위치 유효로 옴. 무효 복구만 재면 튐을 놓침 (P0-a §2)
			bHasConfidenceJump = bHasLastHighPosition && bValid && bBonesAvailable;
			ConfidenceJumpMm = bHasConfidenceJump ? FVector::Distance(LastHighPosition, IndexPosition) * MmPerUnit : 0.0;
			Events.Add(FString::Printf(TEXT("confidence Low -> High, duration %s, jump %s, moved in Low %s"),
				ConfidenceLostAt >= 0.0 ? *FString::Printf(TEXT("%.3f s"), ConfidenceLossDuration) : TEXT("n/a (initial Low)"),
				bHasConfidenceJump ? *FString::Printf(TEXT("%.2f mm"), ConfidenceJumpMm) : TEXT("n/a"),
				bHasLowStartPosition ? *FString::Printf(TEXT("%.2f mm"), LowMaxMoveMm) : TEXT("n/a")));
		}
	}
	if (bInitialized && bValid != bPositionValid)
	{
		if (!bValid)
		{
			PositionLostAt = Time;
			Events.Add(TEXT("position valid -> invalid"));
		}
		else
		{
			PositionRecoveredAt = Time;
			PositionLossDuration = PositionLostAt >= 0.0 ? Time - PositionLostAt : 0.0;
			bHasRecoveryDistance = bHasLastValidPosition && bBonesAvailable;
			RecoveryDistanceMm = bHasRecoveryDistance ? FVector::Distance(LastValidPosition, IndexPosition) * MmPerUnit : 0.0;
			Events.Add(FString::Printf(TEXT("position invalid -> valid, duration %s, jump %s"),
				PositionLostAt >= 0.0 ? *FString::Printf(TEXT("%.3f s"), PositionLossDuration) : TEXT("n/a (initial invalid)"),
				bHasRecoveryDistance ? *FString::Printf(TEXT("%.2f mm"), RecoveryDistanceMm) : TEXT("n/a")));
		}
	}
	// 무효 frame 의 위치는 못 믿음. Low 여도 직전 유효 위치 기준 (P0-a §4)
	if (bValid)
	{
		bHasLastValidPosition = bBonesAvailable;
		if (bBonesAvailable)
		{
			LastValidPosition = IndexPosition;
		}
	}
	// Low 동안 위치가 멈춰 있는지 따라오는지 보려는 값. 0 근처면 멈춤 (P0-a §2)
	if (!bHigh && bValid && bBonesAvailable)
	{
		if (!bHasLowStartPosition)
		{
			bHasLowStartPosition = true;
			LowStartPosition = IndexPosition;
		}
		LowMaxMoveMm = FMath::Max(LowMaxMoveMm, FVector::Distance(LowStartPosition, IndexPosition) * MmPerUnit);
	}
	// 신뢰도 튐 기준점은 High frame 만. Low frame 위치는 추정값일 수 있음 (P0-a §2)
	if (bHigh && bValid && bBonesAvailable)
	{
		bHasLastHighPosition = true;
		LastHighPosition = IndexPosition;
	}
	bPositionValid = bValid;
	bHighConfidence = bHigh;
	bInitialized = true;

	// ── 2) 흔들림 창 갱신 (추적 오류를 흔들림으로 재지 않게, P0-a §4) ──
	DiameterMm = 0.0;
	if (!bValid || !bHigh || !bBonesAvailable)
	{
		Samples.Reset();
		return Events;
	}
	Samples.RemoveAll([Time](const FP0ProbeSample& Sample) { return Time - Sample.Time > 1.0; });
	Samples.Add({ Time, IndexPosition });
	double MaxDistanceSquared = 0.0;
	for (int32 A = 0; A < Samples.Num(); ++A)
	{
		for (int32 B = A + 1; B < Samples.Num(); ++B)
		{
			MaxDistanceSquared = FMath::Max(MaxDistanceSquared, FVector::DistSquared(Samples[A].Position, Samples[B].Position));
		}
	}
	DiameterMm = FMath::Sqrt(MaxDistanceSquared) * MmPerUnit;
	return Events;
}
