#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/OutputDevice.h"
#include "Misc/OutputDeviceRedirector.h"
#include "P5fTestFixture.h"
#include "P3VehicleMock.h"
#include "../Interaction/VBoRAxisPart.h"
#include "../Interaction/VBoRHingedPanel.h"
#include "../Interaction/VBoRSteeringWheel.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/AudioComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "Interaction/IsdkGrabbableComponent.h"
#include "Interaction/IsdkGrabberComponent.h"
#include "Interaction/Pointable/IsdkInteractionPointerEvent.h"
#include "IsdkRuntimeSettings.h"
#include "Materials/MaterialInstanceDynamic.h"

struct FP5fTestAccess
{
	static FVBoRKnobMotion& Motion(AVBoRKnob* Knob) { return Knob->Motion; }
	static bool Select(AVBoRKnob* Knob, int32 Hand=0) { return Knob->BeginHold(Hand,nullptr); }
	static void Pointer(AVBoRKnob* Knob, UIsdkGrabberComponent* Grabber, EIsdkPointerEventType Type)
	{ FIsdkInteractionPointerEvent Event; Event.Interactor=Grabber; Event.Type=Type; Knob->HandlePointer(Event); }
	static UMaterialInstanceDynamic* Overlay(AVBoRKnob* Knob) { return Knob->StateOverlay; }
	static const TArray<TObjectPtr<UStaticMeshComponent>>& Plates(AP3VehicleMock* Car) { return Car->Plates; }
};

namespace
{
	struct FP5fWorld
	{
		UWorld* World = UWorld::CreateWorld(EWorldType::Game,false);
		void Play() { World->InitializeActorsForPlay(FURL()); World->GetWorldSettings()->NotifyBeginPlay(); }
		AP5fTrackedKnob* Knob()
		{
			auto* K=World->SpawnActor<AP5fTrackedKnob>(); K->OnKnobStep.AddDynamic(K,&AP5fTrackedKnob::RecordStep); return K;
		}
		~FP5fWorld() { World->EndPlay(EEndPlayReason::Quit); World->DestroyWorld(false); }
	};
	FQuat P5fTwist(double ClockwiseDeg) { return FQuat(FVector::ZAxisVector,FMath::DegreesToRadians(ClockwiseDeg)); }
	void P5fMove(AP5fTrackedKnob* K, double Angle) { K->Palm.SetRotation(P5fTwist(Angle)); K->Tick(0.016f); }
	UIsdkGrabberComponent* P5fPointer(AVBoRKnob* K)
	{ auto* G=NewObject<UIsdkGrabberComponent>(K); G->InteractorTags.AddTag(IsdkGameplayTags::TAG_Isdk_Type_Device_Left); return G; }
	struct FP5fWarnings : FOutputDevice
	{
		int32 Count=0;
		FP5fWarnings() { GLog->FlushThreadedLogs(); GLog->AddOutputDevice(this); }
		~FP5fWarnings() { GLog->RemoveOutputDevice(this); }
		virtual void Serialize(const TCHAR*, ELogVerbosity::Type Verbosity, const FName& Category) override
		{ if (Category==TEXT("LogKnob") && Verbosity==ELogVerbosity::Warning) { ++Count; } }
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5fRest,"vBoR.P5f.E1Rest",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5fRest::RunTest(const FString&)
{
	FP5fWorld W; auto* K=W.Knob(); W.Play(); TestEqual(TEXT("Rest angle"),K->GetAngleDeg(),0.0); TestEqual(TEXT("Rest step"),K->GetStepIndex(),int64(0));
	TestTrue(TEXT("Authored mesh world pose"),K->KnobMesh->GetComponentTransform().Equals(K->MeshRestFromRoot*K->GetActorTransform(),0.001));
	TestTrue(TEXT("Input and validation boundary visible"),K->GetDebugText().Contains(TEXT("Palm wrap only (no thumb/index rolling)")) && K->GetDebugText().Contains(TEXT("not-tested"))); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5fSigns,"vBoR.P5f.E2ClockwiseSigns",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5fSigns::RunTest(const FString&)
{
	for (int32 Sign : {-1,1}) { FP5fWorld W; auto* K=W.Knob(); W.Play(); FP5fTestAccess::Select(K); P5fMove(K,Sign*10);
		TestTrue(TEXT("Clockwise positive"),FMath::IsNearlyEqual(K->GetAngleDeg(),Sign*10.0,0.001)); TestEqual(TEXT("Below threshold"),K->GetStepIndex(),int64(0)); } return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5fSwing,"vBoR.P5f.E3TranslationSwingAndTwist",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5fSwing::RunTest(const FString&)
{
	FP5fWorld W; auto* K=W.Knob(); W.Play(); FP5fTestAccess::Select(K); K->Palm.SetLocation(FVector(100,200,300)); K->Tick(0.016f); TestEqual(TEXT("Translation discarded"),K->GetAngleDeg(),0.0);
	K->Palm.SetRotation(FQuat(FVector::XAxisVector,FMath::DegreesToRadians(20.0))); K->Tick(0.016f); TestEqual(TEXT("Swing discarded"),K->GetAngleDeg(),0.0);
	K->Palm.SetRotation(P5fTwist(12)*K->Palm.GetRotation()); K->Tick(0.016f); TestTrue(TEXT("Twist at tilted pose"),FMath::IsNearlyEqual(K->GetAngleDeg(),12.0,0.001)); TestEqual(TEXT("One step"),K->GetStepIndex(),int64(1));
	FVBoRKnobMotion M; M.BeginGrab(0,FQuat::Identity); M.AdvanceGrab(true,FQuat(FVector::YAxisVector,FMath::DegreesToRadians(20.0))*P5fTwist(12));
	TestTrue(TEXT("Combined swing and twist projected"),FMath::IsNearlyEqual(M.AngleDeg,12.0,0.001)); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5fTurns,"vBoR.P5f.E4MultipleTurns",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5fTurns::RunTest(const FString&)
{
	for (int32 Sign : {-1,1}) { FP5fWorld W; auto* K=W.Knob(); W.Play(); FP5fTestAccess::Select(K);
		for (int32 A=10; A<=400; A+=10) { P5fMove(K,Sign*A); } TestTrue(TEXT("No wrap"),FMath::IsNearlyEqual(K->GetAngleDeg(),Sign*400.0,0.001));
		TestEqual(TEXT("Unbounded steps"),K->GetStepIndex(),int64(Sign*26)); TestEqual(TEXT("Each crossing delivered"),K->Steps.Num(),26);
		for (int32 D : K->Steps) { TestEqual(TEXT("Direction"),D,Sign); } } return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5fRegrab,"vBoR.P5f.E5ReleaseRegrab",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5fRegrab::RunTest(const FString&)
{
	FP5fWorld W; auto* K=W.Knob(); auto* G=P5fPointer(K); W.Play();
	for (int32 I=1; I<=5; ++I) { K->Palm.SetRotation(P5fTwist(-73)); FP5fTestAccess::Pointer(K,G,EIsdkPointerEventType::Select); K->Tick(0.016f);
		TestTrue(TEXT("Regrab no jump"),FMath::IsNearlyEqual(K->GetAngleDeg(),(I-1)*15.0,0.001)); P5fMove(K,-53); const int32 Count=K->Steps.Num();
		FP5fTestAccess::Pointer(K,G,EIsdkPointerEventType::Unselect); TestTrue(TEXT("Snaps to retained step"),FMath::IsNearlyEqual(K->GetAngleDeg(),I*15.0,0.001));
		K->Tick(0.016f); TestEqual(TEXT("No release notification"),K->Steps.Num(),Count); TestEqual(TEXT("Step retained"),K->GetStepIndex(),int64(I)); } return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5fHysteresis,"vBoR.P5f.E6Hysteresis",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5fHysteresis::RunTest(const FString&)
{
	FP5fWorld W; auto* K=W.Knob(); W.Play(); FP5fTestAccess::Select(K);
	P5fMove(K,10.4); TestEqual(TEXT("Before upper edge"),K->GetStepIndex(),int64(0)); P5fMove(K,10.6); TestEqual(TEXT("After upper edge"),K->GetStepIndex(),int64(1));
	P5fMove(K,4.6); TestEqual(TEXT("Above lower edge"),K->GetStepIndex(),int64(1)); P5fMove(K,4.4); TestEqual(TEXT("Below lower edge"),K->GetStepIndex(),int64(0));
	const int32 Before=K->Steps.Num(); for (int32 I=0; I<10; ++I) { P5fMove(K,I%2?10.1:10.9); } TestEqual(TEXT("Jitter crosses only once"),K->Steps.Num()-Before,1);
	FVBoRKnobMotion M; M.StepHysteresisDeg=0; M.BeginGrab(0,FQuat::Identity); M.AdvanceGrab(true,P5fTwist(7.6)); M.AdvanceGrab(true,P5fTwist(7.6));
	TestEqual(TEXT("Zero hysteresis keeps resting boundary state"),M.StepIndex,int64(1)); TestEqual(TEXT("Zero hysteresis does not duplicate"),M.PendingSteps.Num(),1); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5fJump,"vBoR.P5f.E7CrossingsAndJump",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5fJump::RunTest(const FString&)
{
	for (double A : {29.0,31.0}) { FP5fWorld W; auto* K=W.Knob(); W.Play(); FP5fTestAccess::Select(K); P5fMove(K,A);
		TestEqual(TEXT("Crossing count"),K->Steps.Num(),A==29?2:0); TestTrue(TEXT("Jump ignored"),FMath::IsNearlyEqual(K->GetAngleDeg(),A==29?29.0:0.0,0.001));
		for (int32 D : K->Steps) { TestEqual(TEXT("Ordered positive directions"),D,1); } if (A==31) { P5fMove(K,36); TestTrue(TEXT("Jump rebases"),FMath::IsNearlyEqual(K->GetAngleDeg(),5.0,0.001)); } } return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5fDeferred,"vBoR.P5f.E8UnselectDefersFeedback",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5fDeferred::RunTest(const FString&)
{
	FP5fWorld W; auto* K=W.Knob(); auto* G=P5fPointer(K); W.Play(); FP5fTestAccess::Pointer(K,G,EIsdkPointerEventType::Select); K->Palm.SetRotation(P5fTwist(12));
	FP5fTestAccess::Pointer(K,G,EIsdkPointerEventType::Unselect); TestEqual(TEXT("Step committed in callback"),K->GetStepIndex(),int64(1));
	TestEqual(TEXT("No event inside callback"),K->Steps.Num(),0); TestEqual(TEXT("No sound inside callback"),K->SoundCount,0); K->Tick(0.016f);
	TestEqual(TEXT("Tick emits one event"),K->Steps.Num(),1); TestEqual(TEXT("Tick emits one sound"),K->SoundCount,1); K->Tick(0.016f); TestEqual(TEXT("No duplicate"),K->Steps.Num(),1); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5fContinuous,"vBoR.P5f.E9NoReturnAndContinuous",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5fContinuous::RunTest(const FString&)
{
	for (float S : {0.f,15.f}) { FP5fWorld W; auto* K=W.Knob(); K->StepDeg=S; auto* G=P5fPointer(K); W.Play(); FP5fTestAccess::Pointer(K,G,EIsdkPointerEventType::Select); P5fMove(K,S==0?7:20);
		FP5fTestAccess::Pointer(K,G,EIsdkPointerEventType::Unselect); const double Expected=S==0?7:15; const int32 Count=K->Steps.Num();
		for (int32 I=0; I<8; ++I) { K->Tick(1); } TestTrue(TEXT("Stays released"),FMath::IsNearlyEqual(K->GetAngleDeg(),Expected,0.001)); TestEqual(TEXT("No delayed return steps"),K->Steps.Num(),Count); } return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5fInputs,"vBoR.P5f.E10PalmOnlySingleHand",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5fInputs::RunTest(const FString&)
{
	FP5fWorld W; auto* K=W.Knob(); W.Play(); TestTrue(TEXT("Palm enabled"),K->Grabbable->IsGrabInputMethodAllowed(EIsdkGrabInputMethod::Palm)); TestFalse(TEXT("Pinch disabled"),K->Grabbable->IsGrabInputMethodAllowed(EIsdkGrabInputMethod::Pinch));
	TestFalse(TEXT("No throw"),K->GrabComponent->IsThrowingEnabled()); TestFalse(TEXT("No physics"),K->KnobMesh->IsSimulatingPhysics());
	TestEqual(TEXT("First retained"),K->GrabComponent->GetMultiGrabBehavior(),EIsdkMultiGrabBehavior::SingleGrabFirstRetained); TestEqual(TEXT("One grab point"),K->GrabComponent->GetSingleGrabTransformerObject()->MaxGrabPoints(),1);
	TestTrue(TEXT("First hand accepted"),FP5fTestAccess::Select(K)); TestFalse(TEXT("Other hand ignored"),FP5fTestAccess::Select(K,1)); TestEqual(TEXT("First still owns"),FP5fTestAccess::Motion(K).GrabHand,0); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5fSDK,"vBoR.P5f.E10ActualSDKSelection",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5fSDK::RunTest(const FString&)
{
	FP5fWorld W; auto* K=W.Knob(); auto* Owner=W.World->SpawnActor<AActor>(); auto* G=NewObject<UIsdkGrabberComponent>(Owner); Owner->SetRootComponent(G);
	G->InteractorTags.AddTag(IsdkGameplayTags::TAG_Isdk_Type_Device_Left); G->SetWorldLocation(FVector(1000)); G->SetGrabDetectionTypeAllowed(EIsdkGrabDetectorType::DistanceGrab,false); G->SetGrabDetectionTypeAllowed(EIsdkGrabDetectorType::RayGrab,false);
	G->SetGrabInputMethodAllowed(EIsdkGrabInputMethod::Palm,true); G->SetGrabInputMethodAllowed(EIsdkGrabInputMethod::Pinch,true); G->RegisterComponent(); W.Play(); G->Activate(true); G->SetWorldLocation(K->GrabBox->GetComponentLocation()); G->TickComponent(0.016f,LEVELTICK_All,nullptr);
	G->PinchGrab(); TestTrue(TEXT("Pinch cannot select knob"),G->GetGrabbedComponent()!=K->Grabbable); G->PinchRelease(); G->PalmGrab(); TestTrue(TEXT("SDK selects knob"),G->GetGrabbedComponent()==K->Grabbable);
	P5fMove(K,12); G->TickComponent(0.016f,LEVELTICK_All,nullptr); TestTrue(TEXT("Transformer retains constrained angle"),K->RotationAxis->GetRelativeRotation().Quaternion().Equals(P5fTwist(12),0.001));
	G->PalmRelease(); TestEqual(TEXT("SDK clears ownership"),FP5fTestAccess::Motion(K).GrabHand,INDEX_NONE); TestTrue(TEXT("Release snaps"),FMath::IsNearlyEqual(K->GetAngleDeg(),15.0,0.001)); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5fTracking,"vBoR.P5f.E11TrackingLoss",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5fTracking::RunTest(const FString&)
{
	for (auto Type : {EIsdkPointerEventType::Unselect,EIsdkPointerEventType::Cancel}) { FP5fWorld W; auto* K=W.Knob(); auto* G=P5fPointer(K); W.Play(); FP5fTestAccess::Pointer(K,G,EIsdkPointerEventType::Select);
		K->bTracked=false; P5fMove(K,25); FP5fTestAccess::Pointer(K,G,Type); TestTrue(TEXT("Paused"),FP5fTestAccess::Motion(K).bPaused); TestEqual(TEXT("Ownership retained"),FP5fTestAccess::Motion(K).GrabHand,0); TestEqual(TEXT("No loss alerts"),K->Steps.Num(),0);
		K->bTracked=true; K->Tick(0.016f); TestEqual(TEXT("Recovery discards movement"),K->GetAngleDeg(),0.0); TestTrue(TEXT("Discard recorded"),FMath::IsNearlyEqual(FP5fTestAccess::Motion(K).RecoveryDiscardDeg,25.0,0.001));
		P5fMove(K,37); TestTrue(TEXT("Only new twist"),FMath::IsNearlyEqual(K->GetAngleDeg(),12.0,0.001)); FP5fTestAccess::Pointer(K,G,Type); TestEqual(TEXT("Still retained"),FP5fTestAccess::Motion(K).GrabHand,0);
		K->bClosed=false; K->Tick(0.016f); TestEqual(TEXT("Open ends ownership"),FP5fTestAccess::Motion(K).GrabHand,INDEX_NONE); } return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5fSettings,"vBoR.P5f.E12RuntimeValidation",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5fSettings::RunTest(const FString&)
{
	for (int32 Invalid=0; Invalid<5; ++Invalid) { FP5fWorld W; auto* K=W.Knob();
		if (Invalid==0) { K->KnobMesh->SetStaticMesh(nullptr); } else if (Invalid==1) { K->StepDeg=-1; } else if (Invalid==2) { K->StepHysteresisDeg=-1; } else { K->StepHysteresisDeg=Invalid==3?7.5f:8.f; }
		{ FP5fWarnings Counter; K->OnConstruction(K->GetActorTransform()); K->OnConstruction(K->GetActorTransform()); GLog->FlushThreadedLogs(); TestEqual(TEXT("Construction does not warn"),Counter.Count,0); }
		AddExpectedMessagePlain(TEXT("Missing mesh or invalid knob settings; grabbing disabled"),ELogVerbosity::Warning); W.Play();
		TestFalse(TEXT("Palm disabled"),K->Grabbable->IsGrabInputMethodAllowed(EIsdkGrabInputMethod::Palm)); TestFalse(TEXT("Pinch disabled"),K->Grabbable->IsGrabInputMethodAllowed(EIsdkGrabInputMethod::Pinch));
		TestEqual(TEXT("No collision"),K->GrabBox->GetCollisionEnabled(),ECollisionEnabled::NoCollision); TestFalse(TEXT("No overlaps"),K->GrabBox->GetGenerateOverlapEvents()); TestFalse(TEXT("Injected select rejected"),FP5fTestAccess::Select(K)); } return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5fVisual,"vBoR.P5f.E13ColorsAndMarker",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5fVisual::RunTest(const FString&)
{
	FP5fWorld W; auto* K=W.Knob(); auto* G=P5fPointer(K); W.Play(); auto* Overlay=FP5fTestAccess::Overlay(K); if (!TestNotNull(TEXT("Overlay exists"),Overlay)) { return false; }
	auto Check=[this,Overlay](FLinearColor Expected) { FLinearColor Color; Overlay->GetVectorParameterValue(FMaterialParameterInfo(TEXT("ColorParam")),Color); TestTrue(TEXT("Color"),Color.Equals(Expected)); };
	FP5fTestAccess::Pointer(K,G,EIsdkPointerEventType::Hover); K->Tick(0.016f); Check(K->HoverColor); FP5fTestAccess::Pointer(K,G,EIsdkPointerEventType::Select); P5fMove(K,20); Check(K->GrabColor);
	TestTrue(TEXT("Marker follows knob"),K->RotationMarker->GetComponentTransform().Equals(K->MarkerRestFromPivot*K->RotationAxis->GetComponentTransform(),0.001));
	TestTrue(TEXT("Marker turns toward pivot right"),FVector::DotProduct(K->RotationMarker->GetComponentLocation()-K->Pivot->GetComponentLocation(),K->Pivot->GetRightVector())>0); FP5fTestAccess::Pointer(K,G,EIsdkPointerEventType::Unselect); FP5fTestAccess::Pointer(K,G,EIsdkPointerEventType::Unhover); K->Tick(0.016f); Check(FLinearColor::Transparent);
	TestNotNull(TEXT("Click sound assigned"),K->StepCue.Sound.Get()); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5fCar,"vBoR.P5f.E14CarPlacementSignAndClearance",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5fCar::RunTest(const FString&)
{
	FP5fWorld W; const FTransform CarWorld(FRotator(12,37,5),FVector(123,-256,42)); auto* Car=W.World->SpawnActor<AP3VehicleMock>(AP3VehicleMock::StaticClass(),CarWorld); W.Play(); auto* K=Car->ConsoleKnob.Get(); if (!TestNotNull(TEXT("Knob spawned"),K)) { return false; }
	TestTrue(TEXT("Origin equals car"),K->GetActorTransform().Equals(CarWorld,0.001)); TestTrue(TEXT("Pivot world placement"),K->Pivot->GetComponentLocation().Equals(CarWorld.TransformPosition(Car->KnobPivotCm),0.001));
	TestTrue(TEXT("Mesh authored world pose"),K->KnobMesh->GetComponentTransform().Equals(FTransform(FQuat::Identity,Car->KnobPivotCm+FVector(0,0,Car->KnobHeightCm/2),FVector(Car->KnobDiameterCm,Car->KnobDiameterCm,Car->KnobHeightCm)/100)*CarWorld,0.001));
	TestTrue(TEXT("Support authored world pose"),Car->KnobSupport->GetComponentTransform().Equals(FTransform(FQuat::Identity,Car->KnobSupportCenterCm,Car->KnobSupportSizeCm/100)*CarWorld,0.001));
	const FTransform Support=Car->KnobSupport->GetComponentTransform(); auto* Input=W.Knob(); Input->SetActorTransform(CarWorld); Input->Pivot->SetRelativeLocation(Car->KnobPivotCm); FP5fTestAccess::Select(Input);
	const FQuat WorldDelta(CarWorld.TransformVectorNoScale(FVector::ZAxisVector),FMath::DegreesToRadians(12.0)); Input->Palm.SetRotation(CarWorld.GetRotation().Inverse()*WorldDelta*CarWorld.GetRotation()); Input->Tick(0.016f);
	TestTrue(TEXT("Clockwise about car Z is positive"),FMath::IsNearlyEqual(Input->GetAngleDeg(),12.0,0.001));
	FP5fTestAccess::Motion(K).BeginGrab(0,FQuat::Identity); FP5fTestAccess::Motion(K).AdvanceGrab(true,Input->Palm.GetRotation()); K->Tick(0.016f);
	TestTrue(TEXT("Same twist moves marker toward car right"),FVector::DotProduct(K->RotationMarker->GetComponentLocation()-K->Pivot->GetComponentLocation(),Car->GetActorRightVector())>0);
	TestTrue(TEXT("Support fixed"),Car->KnobSupport->GetComponentTransform().Equals(Support,0.001));
	auto Bounds=[CarWorld](UStaticMeshComponent* Mesh) { return Mesh->GetStaticMesh()->GetBoundingBox().TransformBy(Mesh->GetComponentTransform().GetRelativeTransform(CarWorld)); };
	const FBox B=Bounds(K->KnobMesh); for (int32 I : {9,11}) { TestFalse(TEXT("Clears seat and dashboard"),B.Intersect(Bounds(FP5fTestAccess::Plates(Car)[I]))); }
	for (auto* Mesh : {Car->ConsoleLid->DoorMesh.Get(),Car->GearLever->PartMesh.Get(),Car->TurnSignalLever->PartMesh.Get(),Car->SteeringWheel->WheelMesh.Get()}) { TestFalse(TEXT("Clears other controls"),B.Intersect(Bounds(Mesh))); } return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5fCleanup,"vBoR.P5f.E15Cleanup",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5fCleanup::RunTest(const FString&)
{
	FP5fWorld W; auto* Car=W.World->SpawnActor<AP3VehicleMock>(); W.Play(); auto* K=Car->ConsoleKnob.Get(); auto* Support=Car->KnobSupport.Get(); if (!TestNotNull(TEXT("Knob spawned"),K)) { return false; }
	Car->Destroy(); TestTrue(TEXT("Knob destroyed"),K->IsActorBeingDestroyed()); TestFalse(TEXT("Support unregistered by owner"),Support->IsRegistered()); return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5nKnobGain,"vBoR.P5n.E3E6KnobGain",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5nKnobGain::RunTest(const FString&)
{
	for (double HandAngle : {10.0,25.0,31.0})
	{
		FP5fWorld W; auto* K=W.Knob(); K->TwistGain=2; W.Play(); FP5fTestAccess::Select(K); P5fMove(K,HandAngle);
		const double Expected=HandAngle>30?0:HandAngle*2;
		const int32 Count=HandAngle>30?0:(HandAngle==10?1:3);
		TestTrue(TEXT("E3/E4/E5 actual motion uses gain after raw jump check"),FMath::IsNearlyEqual(K->GetAngleDeg(),Expected,0.001));
		TestEqual(TEXT("E3/E4 each crossed step delivered in Tick"),K->Steps.Num(),Count);
		TestEqual(TEXT("E3/E4 sound per crossing"),K->SoundCount,Count);
		for (int32 Direction : K->Steps) { TestEqual(TEXT("E4 ordered clockwise notifications"),Direction,1); }
		if (HandAngle>30)
		{
			P5fMove(K,32); TestTrue(TEXT("E5 rejected jump rebases"),FMath::IsNearlyEqual(K->GetAngleDeg(),2.0,0.001));
			K->bTracked=false; K->Tick(0.016f); K->bTracked=true; P5fMove(K,42);
			TestTrue(TEXT("Recovery discards raw ten deg"),FMath::IsNearlyEqual(FP5fTestAccess::Motion(K).RecoveryDiscardDeg,10.0,0.001));
			TestTrue(TEXT("Recovery adds no gain-scaled motion"),FMath::IsNearlyEqual(K->GetAngleDeg(),2.0,0.001));
		}
	}
	FP5fWorld W; auto* Parent=W.Knob(); auto* Car=W.World->SpawnActor<AP3VehicleMock>(); Car->KnobTwistGain=2.5f; W.Play();
	TestEqual(TEXT("E6 parent gain remains one"),Parent->TwistGain,1.f);
	TestEqual(TEXT("Mock default gain is two"),GetDefault<AP3VehicleMock>()->KnobTwistGain,2.f);
	TestEqual(TEXT("Mock forwards configurable gain"),Car->ConsoleKnob->TwistGain,2.5f);
	TestEqual(TEXT("Mock gain reaches motion"),FP5fTestAccess::Motion(Car->ConsoleKnob).TwistGain,2.5);
	return true;
}
#endif
