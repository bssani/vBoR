#include "VBoRHandRigPawn.h"
#include "VBoRDoorMotion.h"

#include "Camera/CameraComponent.h"
#include "HeadMountedDisplayFunctionLibrary.h"
#include "HeadMountedDisplayTypes.h"
#include "MotionControllerComponent.h"
#include "Rig/IsdkControllerRigComponent.h"
#include "Rig/IsdkHandVisualsRigComponent.h"
#include "Rig/IsdkPokeLimiterRigModifier.h"
#include "Rig/IsdkRayInteractionRigComponent.h"
#include "Rig/IsdkGrabInteractionRigComponent.h"
#include "Interaction/IsdkGrabberComponent.h"
#include "HandPoseDetection/IsdkHandPalmGrabRecognizer.h"
#include "HandPoseDetection/IsdkHandFingerPinchGrabRecognizer.h"
#include "StructTypesNonGenerated.h"

namespace
{
	void ConfigurePokeRig(UIsdkHandRigComponent* Rig)
	{
		// 기본 RayInteraction 은 내부에서 참조하므로 제거 대신 활성 조건을 끔 (U-03, P0-b §3)
		Rig->GetRayInteraction()->bAutoActivate = false;
		Rig->GetRayInteraction()->Deactivate();
		Rig->GetRayInteraction()->RayVisualClass = nullptr;
		// P0-b 는 poke 만 시험함 (D1)
		Rig->GetGrabInteraction()->bAutoActivate = false;
		Rig->GetGrabInteraction()->Deactivate();
		// Grabber 는 ISDK 생성자에서 이미 존재함. 잡기 재활성화 때도 원거리 차단 유지 (U-03, P0-b R2-1)
		Rig->GetGrabber()->SetGrabDetectionTypeAllowed(EIsdkGrabDetectorType::DistanceGrab, false);
		Rig->GetGrabber()->SetGrabDetectionTypeAllowed(EIsdkGrabDetectorType::RayGrab, false);
	}
}

UVBoRHandRigLeft::UVBoRHandRigLeft()
{
	RigModifiersToSpawn.Add(UIsdkPokeLimiterRigModifier::StaticClass());
	bAutoBindInputActions = false;
	ConfigurePokeRig(this);
}

bool UVBoRHandRigLeft::ReadPalmGrab(bool& bOutClosed) const
{
	if (!PalmGrabRecognizer) { return false; }
	bOutClosed = PalmGrabRecognizer->IsActive();
	return true;
}

bool UVBoRHandRigLeft::ReadPinchGrab(bool& bOutClosed) const
{
	// OpenXR 의 SDK 인식기와 같은 상태를 읽음 (추적 복구 뒤 실제 놓기 확인, P5e D6)
	if (!PinchGrabRecognizer) { return false; }
	bOutClosed = PinchGrabRecognizer->IsActive(); return true;
}

UVBoRHandRigRight::UVBoRHandRigRight()
{
	RigModifiersToSpawn.Add(UIsdkPokeLimiterRigModifier::StaticClass());
	bAutoBindInputActions = false;
	ConfigurePokeRig(this);
}

bool UVBoRHandRigRight::ReadPalmGrab(bool& bOutClosed) const
{
	if (!PalmGrabRecognizer) { return false; }
	bOutClosed = PalmGrabRecognizer->IsActive();
	return true;
}

bool UVBoRHandRigRight::ReadPinchGrab(bool& bOutClosed) const
{
	if (!PinchGrabRecognizer) { return false; }
	bOutClosed = PinchGrabRecognizer->IsActive(); return true;
}

AVBoRHandRigPawn::AVBoRHandRigPawn()
{
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("VROrigin")));
	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(RootComponent);
	Camera->bLockToHmd = true;
	LeftMotion = CreateDefaultSubobject<UMotionControllerComponent>(TEXT("LeftHandTracking"));
	RightMotion = CreateDefaultSubobject<UMotionControllerComponent>(TEXT("RightHandTracking"));
	LeftMotion->SetupAttachment(RootComponent);
	RightMotion->SetupAttachment(RootComponent);
	LeftMotion->SetTrackingMotionSource(TEXT("Left"));
	RightMotion->SetTrackingMotionSource(TEXT("Right"));
	LeftMotion->bDisableLowLatencyUpdate = true;
	RightMotion->bDisableLowLatencyUpdate = true;
	LeftRig = CreateDefaultSubobject<UVBoRHandRigLeft>(TEXT("LeftHandRig"));
	RightRig = CreateDefaultSubobject<UVBoRHandRigRight>(TEXT("RightHandRig"));
	LeftRig->SetupAttachment(LeftMotion);
	RightRig->SetupAttachment(RightMotion);
}

void AVBoRHandRigPawn::PreInitializeComponents()
{
	for (UIsdkHandRigComponent* Rig : {static_cast<UIsdkHandRigComponent*>(LeftRig.Get()), static_cast<UIsdkHandRigComponent*>(RightRig.Get())})
	{
		UIsdkGrabberComponent* Grabber = Rig->GetGrabber();
		// P1 에서만 손바닥 근접 잡기 활성. Pinch 와 원거리는 Grabber 초기화 전부터 차단 (P1 §2 D1, U-03)
		if (bAllowPalmGrab)
		{
			Grabber->SetGrabInputMethodAllowed(EIsdkGrabInputMethod::Palm, true);
			Grabber->SetGrabInputMethodAllowed(EIsdkGrabInputMethod::Pinch, false);
			Grabber->SetGrabDetectionTypeAllowed(EIsdkGrabDetectorType::HandGrab, true);
			Grabber->SetGrabDetectionTypeAllowed(EIsdkGrabDetectorType::DistanceGrab, false);
			Grabber->SetGrabDetectionTypeAllowed(EIsdkGrabDetectorType::RayGrab, false);
			Rig->GetGrabInteraction()->bAutoActivate = true;
		}
	}
	Super::PreInitializeComponents();
}

UIsdkHandRigComponent* AVBoRHandRigPawn::GetHandRig(int32 HandIndex) const
{
	return HandIndex == 0 ? static_cast<UIsdkHandRigComponent*>(LeftRig.Get()) : HandIndex == 1 ? RightRig.Get() : nullptr;
}

bool AVBoRHandRigPawn::ReadRawIndexTip(int32 HandIndex, FVector& OutWorldTip) const
{
	if (HandIndex < 0 || HandIndex > 1) { return false; }
	FXRHandTrackingState State;
	// XR 원본을 직접 읽음 (limiter 의 SyntheticHandVisual root 보정 제외, U-06)
	UHeadMountedDisplayFunctionLibrary::GetHandTrackingState(const_cast<AVBoRHandRigPawn*>(this),
		EXRSpaceType::UnrealWorldSpace, HandIndex == 0 ? EControllerHand::Left : EControllerHand::Right, State);
	const int32 TipIndex = static_cast<int32>(EHandKeypoint::IndexTip);
	if (!State.bValid || State.TrackingStatus != ETrackingStatus::Tracked || !State.HandKeyLocations.IsValidIndex(TipIndex)) { return false; }
	OutWorldTip = State.HandKeyLocations[TipIndex];
	return !OutWorldTip.ContainsNaN();
}

bool AVBoRHandRigPawn::ReadRawPalmAndTips(int32 HandIndex, FVBoRDoorHand& OutHand) const
{
	OutHand = FVBoRDoorHand();
	if (HandIndex < 0 || HandIndex > 1) { return false; }
	FXRHandTrackingState State;
	UHeadMountedDisplayFunctionLibrary::GetHandTrackingState(const_cast<AVBoRHandRigPawn*>(this),
		EXRSpaceType::UnrealWorldSpace, HandIndex == 0 ? EControllerHand::Left : EControllerHand::Right, State);
	const EHandKeypoint Keys[] = {EHandKeypoint::Palm, EHandKeypoint::IndexTip, EHandKeypoint::MiddleTip, EHandKeypoint::RingTip, EHandKeypoint::LittleTip};
	if (!State.bValid || State.TrackingStatus != ETrackingStatus::Tracked || !State.HandKeyRotations.IsValidIndex(0)) { return false; }
	for (int32 I = 0; I < 5; ++I)
	{
		const int32 Key = static_cast<int32>(Keys[I]);
		if (!State.HandKeyLocations.IsValidIndex(Key) || State.HandKeyLocations[Key].ContainsNaN()) { return false; }
		OutHand.Points[I] = State.HandKeyLocations[Key];
	}
	const FQuat Palm = State.HandKeyRotations[0];
	if (Palm.ContainsNaN() || !Palm.IsNormalized()) { return false; }
	// OpenXR +Y 손등 → UE +Z, 손바닥은 −Z 로 추정함 (P2 §2-1, 기기 미확인)
	OutHand.PalmNormal = -Palm.GetUpVector();
	OutHand.bTracked = true;
	return true;
}

FString AVBoRHandRigPawn::DescribeRig() const
{
	TArray<UIsdkControllerRigComponent*> Controllers;
	GetComponents(Controllers);
	FString Text = FString::Printf(TEXT("Controller rigs: %d | Raw tip: XR IndexTip (world)"), Controllers.Num());
	for (int32 HandIndex = 0; HandIndex < 2; ++HandIndex)
	{
		UIsdkHandRigComponent* Rig = GetHandRig(HandIndex);
		FString Modifiers;
		for (UIsdkRigModifier* Modifier : Rig->GetActiveRigModifiers())
		{
			Modifiers += Modifier->GetClass()->GetName() + TEXT(" ");
		}
		const auto& Sources = Rig->GetHandVisuals()->GetDataSources();
		const UIsdkGrabberComponent* Grabber = Rig->GetGrabber();
		Text += FString::Printf(TEXT("\n%s ray active=%d, enabled=%d | grabber active=%d, distance allowed=%d, ray grab allowed=%d | modifiers: %s | source: %s"),
			HandIndex == 0 ? TEXT("Left") : TEXT("Right"), Rig->GetRayInteraction()->IsActive(),
			Rig->GetRayInteraction()->GetEnabledConditional()->GetResolvedValue(), Grabber->IsActive(),
			Grabber->IsGrabDetectionTypeAllowed(EIsdkGrabDetectorType::DistanceGrab),
			Grabber->IsGrabDetectionTypeAllowed(EIsdkGrabDetectorType::RayGrab), *Modifiers,
			Sources.DataSourceComponent ? *Sources.DataSourceComponent->GetClass()->GetName() : TEXT("unavailable"));
		Text += FString::Printf(TEXT(" | allow palm grab=%d, grab rig active=%d, hand grab allowed=%d, palm allowed=%d, pinch allowed=%d"),
			bAllowPalmGrab, Rig->GetGrabInteraction()->IsActive(),
			Grabber->IsGrabDetectionTypeAllowed(EIsdkGrabDetectorType::HandGrab),
			Grabber->IsGrabInputMethodAllowed(EIsdkGrabInputMethod::Palm),
			Grabber->IsGrabInputMethodAllowed(EIsdkGrabInputMethod::Pinch));
	}
	return Text;
}
