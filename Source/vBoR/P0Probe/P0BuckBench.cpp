#include "P0BuckBench.h"
#include "../Interaction/VBoREyePointComponent.h"
#include "../Interaction/VBoRHandRigPawn.h"
#include "Components/InputComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/WorldSettings.h"
#include "HeadMountedDisplayFunctionLibrary.h"
#include "IXRTrackingSystem.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CoreDelegates.h"
#include "Misc/Paths.h"

DEFINE_LOG_CATEGORY(LogP0Buck);

namespace
{
	TArray<UVBoREyePointComponent*> FindEyes(UWorld* World)
	{
		TArray<UVBoREyePointComponent*> Eyes;
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			TArray<UVBoREyePointComponent*> Components;
			It->GetComponents(Components);
			for (UVBoREyePointComponent* Eye : Components) { if (IsValid(Eye) && Eye->IsRegistered()) { Eyes.Add(Eye); } }
		}
		return Eyes;
	}
	FString OriginName(int32 Origin)
	{
		switch (Origin)
		{
		case EHMDTrackingOrigin::View: return TEXT("View");
		case EHMDTrackingOrigin::LocalFloor: return TEXT("LocalFloor");
		case EHMDTrackingOrigin::Local: return TEXT("Local");
		case EHMDTrackingOrigin::Stage: return TEXT("Stage");
		case EHMDTrackingOrigin::CustomOpenXR: return TEXT("CustomOpenXR");
		default: return TEXT("unavailable");
		}
	}
}

AP0BuckBench::AP0BuckBench()
{
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("BuckOrigin")));
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostUpdateWork;
	EyePoint = CreateDefaultSubobject<UVBoREyePointComponent>(TEXT("EyePoint"));
	EyePoint->SetupAttachment(RootComponent);
	EyePoint->SetRelativeLocation(EyeOffsetCm);
	Panel = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Panel"));
	Panel->SetupAttachment(RootComponent);
	Panel->SetHorizontalAlignment(EHTA_Left);
	Panel->SetVerticalAlignment(EVRTA_TextTop);
	Panel->SetTextRenderColor(FColor::White);
}

void AP0BuckBench::BeginPlay()
{
	Super::BeginPlay();
	EyePoint->SetRelativeLocation(EyeOffsetCm);
	Panel->SetRelativeLocation(EyeOffsetCm + PanelOffsetCm);
	Panel->SetRelativeRotation(FRotator(0, 180, 0));
	Panel->SetWorldSize(PanelTextSizeCm);
	CreateGeometry();
	AlignmentPawn = UGameplayStatics::GetPlayerPawn(this, 0);
	InputController = UGameplayStatics::GetPlayerController(this, 0);
	if (InputController)
	{
		EnableInput(InputController);
		InputComponent->BindKey(SaveKey, IE_Pressed, this, &AP0BuckBench::StartSave);
		InputComponent->BindKey(SaveKey, IE_Released, this, &AP0BuckBench::ReleaseSave);
		InputComponent->BindKey(MeasureKey, IE_Pressed, this, &AP0BuckBench::StartMeasure);
		InputComponent->BindKey(TipKey, IE_Pressed, this, &AP0BuckBench::CheckTips);
	}
	RecenterHandle = FCoreDelegates::VRHeadsetRecenter.AddUObject(this, &AP0BuckBench::HandleRecenter);
	if (SavePath.IsEmpty()) { SavePath = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Calibration/BuckEye.json")); }
	CurrentFrame = ReadHead();
	LoadAndApply(CurrentFrame);
	UpdatePanel();
}

void AP0BuckBench::EndPlay(const EEndPlayReason::Type Reason)
{
	FCoreDelegates::VRHeadsetRecenter.Remove(RecenterHandle);
	RecenterHandle.Reset();
	if (InputComponent) { InputComponent->KeyBindings.Reset(); }
	if (InputController) { DisableInput(InputController); }
	Window.Cancel(TEXT("EndPlay")); bSaveHeld = false;
	Super::EndPlay(Reason);
}

void AP0BuckBench::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	ProcessFrame(DeltaSeconds, ReadHead());
}

FVBoRBuckHeadFrame AP0BuckBench::ReadHead() const
{
	FVBoRBuckHeadFrame Frame;
	Frame.TrackingOrigin = INDEX_NONE;
	if (GEngine && GEngine->XRSystem.IsValid())
	{
		const auto& XR = GEngine->XRSystem;
		Frame.TrackingOrigin = XR->GetTrackingOrigin();
		// VALID 만으로 남은 자세를 쓰지 않음. 위치·방향 TRACKED 를 별도로 확인함 (P0-c §3-3)
		const bool bPoseValid = XR->GetCurrentPose(IXRTrackingSystem::HMDDeviceId, Frame.Orientation, Frame.PositionCm);
		Frame.bTracked = bPoseValid && XR->IsTracking(IXRTrackingSystem::HMDDeviceId);
		Frame.PlayAreaCm = UHeadMountedDisplayFunctionLibrary::GetPlayAreaBounds(EHMDTrackingOrigin::Stage);
	}
	return Frame;
}

void AP0BuckBench::LoadAndApply(const FVBoRBuckHeadFrame& Frame)
{
	FString Reason;
	FVBoRBuckSavedAlignment Loaded;
	if (!FVBoRBuckAlignmentStore::Load(SavePath, Loaded, Reason))
	{
		bHasSaved = false;
		State = bNeedsCheck ? State : TEXT("No saved alignment: ") + Reason;
		Record(State);
		return;
	}
	Saved = Loaded; bHasSaved = true;
	ApplySaved(Frame, false);
}

void AP0BuckBench::ApplySaved(const FVBoRBuckHeadFrame& Frame, bool bFreshSave)
{
	const auto Eyes = FindEyes(GetWorld());
	if (Eyes.Num() != 1)
	{
		MarkNeedsCheck(FString::Printf(TEXT("eye point count %d; exactly one required"), Eyes.Num()));
		return;
	}
	if (!AlignmentPawn) { AlignmentPawn = UGameplayStatics::GetPlayerPawn(this, 0); }
	if (!AlignmentPawn || AlignmentPawn->bUseControllerRotationYaw)
	{
		MarkNeedsCheck(TEXT("Pawn unavailable or controller yaw enabled; apply blocked"));
		return;
	}
	const auto Decision = FVBoRBuckAlignment::CheckEnvironment(Saved, Frame.TrackingOrigin, Frame.PlayAreaCm, PlayAreaToleranceCm);
	if (!Decision.bApply) { MarkNeedsCheck(Decision.Reason); return; }
	const FTransform Transform = FVBoRBuckAlignment::CalculatePawn(Saved.Capture, Eyes[0]->GetComponentTransform());
	AlignmentPawn->SetActorTransform(Transform, false, nullptr, ETeleportType::TeleportPhysics);
	LastPawn = FString::Printf(TEXT("Pawn cm %s | Yaw %.3f deg | floor Z %.3f cm"), *Transform.GetLocation().ToString(), Transform.Rotator().Yaw, Transform.GetLocation().Z);
	Record(LastPawn);
	if (bFreshSave) { bNeedsCheck = false; State = TEXT("Applied: ") + Saved.SavedAt; }
	else if (!bNeedsCheck) { State = TEXT("Applied: ") + Saved.SavedAt; }
	if (Decision.bNeedsCheck) { MarkNeedsCheck(Decision.Reason); }
	Record(State);
}

void AP0BuckBench::StartSave()
{
	if (Window.bActive || bSaveHeld) { Record(TEXT("C ignored: capture or hold already active")); return; }
	bSaveHeld = true;
	CaptureOrigin = CurrentFrame.TrackingOrigin;
	if (!Window.Begin(SaveHoldSeconds, true)) { Record(TEXT("C rejected: invalid hold duration")); return; }
	Record(FString::Printf(TEXT("C capture started: hold %.3f s"), SaveHoldSeconds));
}

void AP0BuckBench::ReleaseSave()
{
	bSaveHeld = false;
	if (Window.bActive && Window.bSaving)
	{
		Window.Cancel(TEXT("C released early; previous alignment retained"));
		LastCapture = Window.Reason; Record(LastCapture);
	}
}

void AP0BuckBench::StartMeasure()
{
	if (Window.bActive || bSaveHeld) { Record(TEXT("M ignored: capture or hold already active")); return; }
	CaptureOrigin = CurrentFrame.TrackingOrigin;
	if (!Window.Begin(MeasureSeconds, false)) { Record(TEXT("M rejected: invalid window duration")); return; }
	Record(FString::Printf(TEXT("M measurement started: %.3f s; no save"), MeasureSeconds));
}

void AP0BuckBench::ProcessFrame(double DeltaSeconds, const FVBoRBuckHeadFrame& Frame)
{
	CurrentFrame = Frame;
	if (bHasSaved && !bNeedsCheck)
	{
		const auto Check = FVBoRBuckAlignment::CheckEnvironment(Saved, Frame.TrackingOrigin, Frame.PlayAreaCm, PlayAreaToleranceCm);
		if (Check.bNeedsCheck) { MarkNeedsCheck(Check.Reason); }
	}
	if (Window.bActive)
	{
		if (Frame.TrackingOrigin != EHMDTrackingOrigin::Stage || Frame.TrackingOrigin != CaptureOrigin ||
			!FMath::IsNearlyEqual(GetWorld()->GetWorldSettings()->WorldToMeters, 100.0f))
		{
			Window.Cancel(TEXT("capture requires unchanged Stage origin and WorldToMeters=100"));
			LastCapture = Window.Reason; Record(LastCapture);
		}
		else
		{
			const bool bComplete = Window.Sample(DeltaSeconds, Frame, MinHorizontalLength);
			if (!Window.bActive && !bComplete) { LastCapture = Window.Reason; Record(LastCapture); }
			if (bComplete)
			{
				FVBoRBuckCaptureResult Result;
				if (!Window.Finish(Result)) { LastCapture = TEXT("capture rejected: circular direction ambiguous"); Record(LastCapture); }
				else if (!Window.bSaving)
				{
					LastMeasure = TEXT("M averaged: ") + DescribeDelta(Result);
					Record(LastMeasure);
				}
				else
				{
					LastCapture = FString::Printf(TEXT("C mean cm %s | Yaw %.3f deg | spread %.3f mm / %.3f deg | used %d discarded %d | previous: %s"),
						*Result.PositionCm.ToString(), Result.YawDeg, Result.PositionSpreadMm, Result.YawSpreadDeg, Result.UsedFrames, Result.DiscardedFrames, *DescribeDelta(Result));
					Record(LastCapture);
					const auto Eyes = FindEyes(GetWorld());
					if (Eyes.Num() != 1) { MarkNeedsCheck(FString::Printf(TEXT("eye point count %d; save not committed"), Eyes.Num())); }
					else
					{
						FVBoRBuckSavedAlignment Candidate;
						Candidate.SavedAt = FDateTime::UtcNow().ToIso8601();
						Candidate.TrackingOrigin = Frame.TrackingOrigin; Candidate.PlayAreaCm = Frame.PlayAreaCm; Candidate.Capture = Result;
						FString Reason;
						if (!FVBoRBuckAlignmentStore::Save(SavePath, Candidate, Reason)) { LastCapture += TEXT(" | ") + Reason; Record(LastCapture); }
						else { Saved = Candidate; bHasSaved = true; ApplySaved(Frame, true); }
					}
				}
			}
		}
	}
	UpdatePanel();
}

void AP0BuckBench::CheckTips()
{
	if (Window.bActive || bSaveHeld) { Record(TEXT("T ignored: capture or hold already active")); return; }
	const auto Eyes = FindEyes(GetWorld());
	if (Eyes.Num() != 1) { Record(FString::Printf(TEXT("T rejected: eye point count %d"), Eyes.Num())); return; }
	const AVBoRHandRigPawn* HandPawn = Cast<AVBoRHandRigPawn>(AlignmentPawn);
	LastTips.Reset();
	for (int32 Hand = 0; Hand < 2; ++Hand)
	{
		FVector Tip = FVector::ZeroVector;
		const bool bTracked = HandPawn && HandPawn->ReadRawIndexTip(Hand, Tip);
		if (Hand > 0) { LastTips += TEXT(" | "); }
		LastTips += Hand == 0 ? TEXT("Left: ") : TEXT("Right: ");
		LastTips += FVBoRBuckAlignment::DescribeTip(bTracked, Tip, Eyes[0]->GetComponentTransform());
	}
	Record(TEXT("T raw index tip: ") + LastTips);
	UpdatePanel();
}

void AP0BuckBench::HandleRecenter()
{
	++RecenterCount;
	LastRecenter = FDateTime::UtcNow().ToIso8601();
	if (Window.bActive) { Window.Cancel(TEXT("tracking space changed during capture")); LastCapture = Window.Reason; Record(LastCapture); }
	MarkNeedsCheck(TEXT("tracking space changed (VRHeadsetRecenter)") + FString::Printf(TEXT(" | count %d | %s"), RecenterCount, *LastRecenter));
	UpdatePanel();
}

void AP0BuckBench::MarkNeedsCheck(const FString& Reason)
{
	bNeedsCheck = true;
	// M·추적 복귀로 지우지 않고 새 C 저장만 해제함 (P0-c §3-8)
	State = TEXT("Needs check: ") + Reason;
	Record(State);
}

void AP0BuckBench::Record(const FString& Text)
{
	LastEvent = Text;
	UE_LOG(LogP0Buck, Log, TEXT("[%.3f s] %s"), GetWorld() ? GetWorld()->GetTimeSeconds() : 0, *Text);
}

FString AP0BuckBench::DescribeDelta(const FVBoRBuckCaptureResult& Sample) const
{
	if (!bHasSaved) { return TEXT("no saved alignment"); }
	const FVector DeltaMm = (Sample.PositionCm - Saved.Capture.PositionCm) * 10;
	return FString::Printf(TEXT("Stage delta X=%.2f Y=%.2f Z=%.2f | distance %.2f mm | Yaw %.3f deg"),
		DeltaMm.X, DeltaMm.Y, DeltaMm.Z, DeltaMm.Length(), FMath::FindDeltaAngleDegrees(Saved.Capture.YawDeg, Sample.YawDeg));
}

void AP0BuckBench::UpdatePanel()
{
	double Yaw;
	FString Live = TEXT("HMD not TRACKED; live delta unavailable");
	if (CurrentFrame.bTracked && FVBoRBuckAlignment::ProjectYaw(CurrentFrame.Orientation, MinHorizontalLength, Yaw))
	{
		FVBoRBuckCaptureResult Sample; Sample.PositionCm = CurrentFrame.PositionCm; Sample.YawDeg = Yaw;
		Live = DescribeDelta(Sample);
	}
	const FString Text = FString::Printf(TEXT("P0-c | not-tested | approximate geometry / placeholder values\n%s\nOrigin: %s | HMD TRACKED: %s\nStage play area cm: now %.1f x %.1f | saved %.1f x %.1f\n%s\n%s hold %.2f s = save | %s %.2f s = measure | %s = raw fingertips\nCapture: %s %.2f / %.2f s | discarded %d\n%s\n%s\n%s\n%s\nRecenter: %d | last %s\nLast event: %s"),
		*State, *OriginName(CurrentFrame.TrackingOrigin), CurrentFrame.bTracked ? TEXT("yes") : TEXT("no"),
		CurrentFrame.PlayAreaCm.X, CurrentFrame.PlayAreaCm.Y, Saved.PlayAreaCm.X, Saved.PlayAreaCm.Y,
		*Live, *SaveKey.ToString(), SaveHoldSeconds, *MeasureKey.ToString(), MeasureSeconds, *TipKey.ToString(),
		Window.bActive ? (Window.bSaving ? TEXT("saving") : TEXT("measuring")) : TEXT("idle"), Window.ElapsedSeconds, Window.TargetSeconds, Window.DiscardedFrames,
		*LastCapture, *LastMeasure, *LastTips, *LastPawn, RecenterCount, *LastRecenter, *LastEvent);
	Panel->SetText(FText::FromString(Text));
	const double Now = GetWorld()->GetTimeSeconds();
	if (Now - LastLogSeconds >= 1)
	{
		LastLogSeconds = Now;
		FString Line = Text; Line.ReplaceInline(TEXT("\n"), TEXT(" | "));
		UE_LOG(LogP0Buck, Log, TEXT("%s"), *Line);
	}
}

void AP0BuckBench::AddBox(UStaticMesh* Cube, const FString& Name, const FVector& EyeCenter, const FVector& Size)
{
	UStaticMeshComponent* Mesh = NewObject<UStaticMeshComponent>(this, *Name);
	AddInstanceComponent(Mesh);
	Mesh->SetupAttachment(RootComponent);
	Mesh->SetStaticMesh(Cube);
	Mesh->SetRelativeLocation(EyeOffsetCm + EyeCenter);
	Mesh->SetRelativeScale3D(Size / 100);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->RegisterComponent();
}

void AP0BuckBench::CreateGeometry()
{
	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (!Cube) { Record(TEXT("Approximate geometry cube unavailable")); return; }
	// 형상은 레벨의 Buck 자리 고정, 머리 기준 배치 없음 (P0-c D4, U-07)
	AddBox(Cube, TEXT("Cushion"), CushionTopCm - FVector(0, 0, CushionSizeCm.Z / 2), CushionSizeCm);
	AddBox(Cube, TEXT("Back"), FVector(BackFrontXCm - BackSizeCm.X / 2, 0, CushionTopCm.Z + BackSizeCm.Z / 2), BackSizeCm);
	AddBox(Cube, TEXT("Dashboard"), FVector(DashboardXCm, 0, DashboardCenterZCm), DashboardSizeCm);
	AddBox(Cube, TEXT("EyeCrossY"), FVector::ZeroVector, FVector(0.3, MarkerSizeCm, 0.3));
	AddBox(Cube, TEXT("EyeCrossZ"), FVector::ZeroVector, FVector(0.3, 0.3, MarkerSizeCm));
	const int32 Segments = FMath::Clamp(WheelSegments, 12, 64);
	for (int32 Index = 0; Index < Segments; ++Index)
	{
		const double Angle = 2 * UE_DOUBLE_PI * Index / Segments;
		const FVector Center = WheelCenterCm + FVector(0, FMath::Cos(Angle), FMath::Sin(Angle)) * WheelDiameterCm / 2;
		AddBox(Cube, FString::Printf(TEXT("Wheel%d"), Index), Center, FVector(WheelTubeCm, WheelTubeCm, WheelDiameterCm * UE_DOUBLE_PI / Segments + WheelTubeCm));
		UStaticMeshComponent* Segment = Cast<UStaticMeshComponent>(GetInstanceComponents().Last());
		Segment->SetRelativeRotation(FRotator(0, 0, -FMath::RadiansToDegrees(Angle)));
	}
	const double Spacing = FMath::Max(1.0, GridSpacingCm);
	const int32 Lines = FMath::Clamp(FMath::FloorToInt(GroundHalfSizeCm / Spacing), 0, 100);
	for (int32 Index = -Lines; Index <= Lines; ++Index)
	{
		// EyeOffset 의 높이를 빼서 격자 윗면은 Actor Z=0 에 둠 (P0-c §2-5)
		AddBox(Cube, FString::Printf(TEXT("GridX%d"), Index), FVector(0, Index * Spacing, -EyeOffsetCm.Z - 0.05), FVector(GroundHalfSizeCm * 2, 0.2, 0.1));
		AddBox(Cube, FString::Printf(TEXT("GridY%d"), Index), FVector(Index * Spacing, 0, -EyeOffsetCm.Z - 0.05), FVector(0.2, GroundHalfSizeCm * 2, 0.1));
	}
}
