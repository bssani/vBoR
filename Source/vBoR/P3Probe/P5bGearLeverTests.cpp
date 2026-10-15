#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "P5bTestFixture.h"
#include "P3VehicleMock.h"
#include "../Interaction/VBoRPokeButton.h"
#include "../Interaction/VBoRScenarioRunner.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "Interaction/IsdkGrabbableComponent.h"
#include "Interaction/IsdkGrabberComponent.h"
#include "Interaction/IsdkPokeInteractable.h"
#include "Interaction/Grabbable/IsdkOneGrabRotateTransformer.h"
#include "IsdkFunctionLibrary.h"
#include "IsdkRuntimeSettings.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/OutputDevice.h"
#include "Misc/OutputDeviceRedirector.h"

struct FP5bTestAccess
{
	static double Value(const AVBoRAxisPart* Part) { return Part->Value; }
	static FTransform Rest(const AVBoRAxisPart* Part) { return Part->RestPivot * Part->GetActorTransform(); }
	static void SetValue(AVBoRAxisPart* Part, double Angle)
	{
		Part->Value = Angle; Part->AxisPivot->SetRelativeTransform(Part->ValueTransform() * Part->RestPivot);
	}
	static void Begin(AVBoRAxisPart* Part, const FIsdkGrabPose& Pose)
	{
		Part->HoldingHand = 0; Part->Transformer->BeginTransform({Pose}, FIsdkTargetTransform(Part->AxisPivot));
	}
	static void Move(AVBoRAxisPart* Part, const FIsdkGrabPose& Pose, bool bTracked = true)
	{
		Part->AxisPivot->SetRelativeTransform(Part->Transformer->UpdateTracked({Pose}, FIsdkTargetTransform(Part->AxisPivot), bTracked));
		Part->EvaluateState();
	}
	static void Release(AVBoRAxisPart* Part) { Part->ReleasePart(); }
	static FTransform SDK(AVBoRAxisPart* Part, const FIsdkGrabPose& Pose) { return Part->Transformer->Rotate->UpdateTransform({Pose},Part->Transformer->AxisTarget()); }
	static double Discard(const AVBoRAxisPart* Part) { return Part->RecoveryDiscard; }
	static void Hover(AVBoRAxisPart* Part, UIsdkGrabberComponent* Grabber) { Part->Hovering.Add(Grabber); Part->EvaluateState(); }
	static FLinearColor Color(AVBoRAxisPart* Part) { return Part->StateOverlay->K2_GetVectorParameterValue(TEXT("ColorParam")); }
	static UP5bAudioObserver* Audio(AVBoRAxisPart* Part)
	{
		UP5bAudioObserver* Audio = NewObject<UP5bAudioObserver>(Part); Part->StateAudio = Audio; return Audio;
	}
	static const TArray<TObjectPtr<UStaticMeshComponent>>& Plates(AP3VehicleMock* Car) { return Car->Plates; }
	static bool Counted(AVBoRScenarioRunner* Runner, AVBoRPokeButton* Button) { return Runner->ButtonCounts.Contains(Button); }
};

namespace
{
	struct FP5bWorld
	{
		UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
		void Play() { World->InitializeActorsForPlay(FURL()); World->GetWorldSettings()->NotifyBeginPlay(); }
		AP2bTrackedPart* Part(int32 Start = 1)
		{
			AP2bTrackedPart* Part = World->SpawnActorDeferred<AP2bTrackedPart>(AP2bTrackedPart::StaticClass(), FTransform::Identity);
			Part->Release = EVBoRAxisRelease::Detent; Part->DetentStartIndex = Start;
			Part->FinishSpawning(FTransform::Identity); return Part;
		}
		~FP5bWorld() { World->EndPlay(EEndPlayReason::Quit); World->DestroyWorld(false); }
	};
	FIsdkGrabPose P5bPose(AVBoRAxisPart* Part, double Angle)
	{
		const FQuat Rotation(FVector::XAxisVector, FMath::DegreesToRadians(Angle * (Part->bReverse ? -1 : 1)));
		return UIsdkFunctionLibrary::MakeGrabPoseStruct(11, FP5bTestAccess::Rest(Part).TransformPosition(Rotation.RotateVector(FVector(0,100,0))), FQuat::Identity);
	}
	void P5bBegin(AVBoRAxisPart* Part) { FP5bTestAccess::Begin(Part, P5bPose(Part, FP5bTestAccess::Value(Part))); }
	void P5bMove(AVBoRAxisPart* Part, double Angle, bool bTracked = true) { FP5bTestAccess::Move(Part, P5bPose(Part, Angle), bTracked); }
	UP5bEventObserver* P5bObserve(AVBoRAxisPart* Part)
	{
		UP5bEventObserver* Events = NewObject<UP5bEventObserver>(Part);
		Part->OnDetentChanged.AddDynamic(Events, &UP5bEventObserver::Changed);
		Part->OnPartOpened.AddDynamic(Events, &UP5bEventObserver::Opened);
		Part->OnPartClosed.AddDynamic(Events, &UP5bEventObserver::Closed);
		Part->OnPartReleased.AddDynamic(Events, &UP5bEventObserver::Released); return Events;
	}
	struct FP5bLogs : FOutputDevice
	{
		TArray<FString> Warnings;
		FP5bLogs() { GLog->FlushThreadedLogs(); GLog->AddOutputDevice(this); }
		~FP5bLogs() { GLog->RemoveOutputDevice(this); }
		virtual void Serialize(const TCHAR* Message, ELogVerbosity::Type Verbosity, const FName& Category) override
		{
			if (Category == TEXT("LogAxisPart") && Verbosity == ELogVerbosity::Warning) { Warnings.Add(Message); }
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5bStart, "vBoR.P5b.E1StartAndRestPivot", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5bStart::RunTest(const FString&)
{
	for (int32 Start : {1,2})
	{
		FP5bWorld W;
		AP2bTrackedPart* Part = W.Part(Start);
		Part->AxisPivot->SetRelativeTransform(FTransform(FRotator(10,25,5),FVector(20,30,40)));
		Part->MeshRestFromRoot = FTransform(FQuat::Identity,FVector(25,45,60),FVector(0.1));
		Part->OnConstruction(Part->GetActorTransform()); Part->OnConstruction(Part->GetActorTransform());
		const FTransform Rest = Part->AxisPivot->GetComponentTransform();
		UP5bEventObserver* Events = P5bObserve(Part);
		W.Play();
		TestEqual(TEXT("Start index"),Part->GetDetentIndex(),Start);
		TestEqual(TEXT("Start angle"),FP5bTestAccess::Value(Part),static_cast<double>(Part->DetentAnglesDeg[Start]));
		TestTrue(TEXT("Construction never accumulates start angle"),FP5bTestAccess::Rest(Part).Equals(Rest,0.001));
		TestEqual(TEXT("No initial transition"),Events->Indices.Num(),0);
		FP5bTestAccess::SetValue(Part,0);
		TestTrue(TEXT("Zero preserves authored mesh world pose"),Part->PartMesh->GetComponentTransform().Equals(Part->MeshRestFromRoot * Part->GetActorTransform(),0.001));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5bBelow, "vBoR.P5b.E2BelowThresholdReturnTime", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5bBelow::RunTest(const FString&)
{
	FP5bWorld W; W.Play();
	for (double Angle : {-14.0,14.0})
	{
		AP2bTrackedPart* Part = W.Part(); UP5bEventObserver* Events = P5bObserve(Part);
		Part->ReturnDegPerSecond = 10; P5bBegin(Part); P5bMove(Part,Angle); FP5bTestAccess::Release(Part);
		TestEqual(TEXT("Below threshold retains center"),Part->GetDetentIndex(),1);
		Part->Tick(0.5f); TestTrue(TEXT("Moves exactly 5 deg toward center"),FMath::IsNearlyEqual(FP5bTestAccess::Value(Part),Angle > 0 ? 9.0 : -9.0,0.001));
		Part->Tick(0.9f); TestTrue(TEXT("Arrives without overshoot at 1.4 s"),FMath::IsNearlyZero(FP5bTestAccess::Value(Part),0.001));
		Part->Tick(1); TestEqual(TEXT("Stops at center"),FP5bTestAccess::Value(Part),0.0);
		TestEqual(TEXT("No transition on release or return"),Events->Indices.Num(),0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5bBoundary, "vBoR.P5b.E3InclusiveBoundaryAndAudio", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5bBoundary::RunTest(const FString&)
{
	FP5bWorld W; AP2bTrackedPart* Part = W.Part(); UP5bEventObserver* Events = P5bObserve(Part); W.Play();
	UP5bAudioObserver* Audio = FP5bTestAccess::Audio(Part);
	P5bBegin(Part); P5bMove(Part,15);
	TestEqual(TEXT("Exact threshold selects upper"),Part->GetDetentIndex(),2);
	Part->Tick(0.01f);
	TestTrue(TEXT("One event with upper index"),Events->Indices == TArray<int32>{2});
	TestEqual(TEXT("One click request"),Audio->Plays,1);
	FP5bTestAccess::Release(Part); Part->ReturnDegPerSecond = 10; Part->Tick(0.2f);
	TestTrue(TEXT("Return can increase angle"),FMath::IsNearlyEqual(FP5bTestAccess::Value(Part),17.0,0.001));
	Part->Tick(1); TestEqual(TEXT("Stops at upper angle"),FP5bTestAccess::Value(Part),20.0);
	TestEqual(TEXT("No return click"),Audio->Plays,1); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5bContinuous, "vBoR.P5b.E4ContinuousBothDirections", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5bContinuous::RunTest(const FString&)
{
	FP5bWorld W; AP2bTrackedPart* Part = W.Part(2); UP5bEventObserver* Events = P5bObserve(Part); W.Play(); P5bBegin(Part);
	P5bMove(Part,5); TestEqual(TEXT("Upper to center at +5"),Part->GetDetentIndex(),1);
	P5bMove(Part,-15); TestEqual(TEXT("Center to lower at -15"),Part->GetDetentIndex(),0);
	P5bMove(Part,-20); P5bMove(Part,-5); TestEqual(TEXT("Lower to center at -5"),Part->GetDetentIndex(),1);
	P5bMove(Part,15); TestEqual(TEXT("Center to upper at +15"),Part->GetDetentIndex(),2);
	Part->Tick(0.01f);
	TestTrue(TEXT("Ordered transitions while holding"),Events->Indices == TArray<int32>({1,0,1,2})); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5bJump, "vBoR.P5b.E5SingleFrameOrderedTransitions", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5bJump::RunTest(const FString&)
{
	FP5bWorld W; AP2bTrackedPart* Part = W.Part(2); UP5bEventObserver* Events = P5bObserve(Part); W.Play(); P5bBegin(Part);
	UP5bAudioObserver* Audio = FP5bTestAccess::Audio(Part); P5bMove(Part,-20);
	TestEqual(TEXT("Single update confirms lower before Tick"),Part->GetDetentIndex(),0);
	TestEqual(TEXT("Move emits no event before Tick"),Events->Indices.Num(),0);
	TestEqual(TEXT("Move emits no click before Tick"),Audio->Plays,0);
	Part->Tick(0.01f);
	TestTrue(TEXT("Tick emits center then lower"),Events->Indices == TArray<int32>({1,0}));
	TestEqual(TEXT("Two click requests"),Audio->Plays,2);
	P5bMove(Part,20); Part->Tick(0.01f);
	TestTrue(TEXT("Reverse update emits center then upper"),Events->Indices == TArray<int32>({1,0,1,2}));
	Part->Tick(0.01f);
	TestEqual(TEXT("Drained notifications do not repeat"),Events->Indices.Num(),4);
	TestEqual(TEXT("Four clicks across both directions"),Audio->Plays,4); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5bSameFrameRelease, "vBoR.P5b.R1SameFrameThresholdAndRelease", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5bSameFrameRelease::RunTest(const FString&)
{
	FP5bWorld W; AP2bTrackedPart* Part = W.Part(); UP5bEventObserver* Events = P5bObserve(Part); W.Play();
	UP5bAudioObserver* Audio = FP5bTestAccess::Audio(Part);
	P5bBegin(Part); P5bMove(Part,15); FP5bTestAccess::Release(Part);
	TestEqual(TEXT("Same frame release retains confirmed upper"),Part->GetDetentIndex(),2);
	TestEqual(TEXT("Release does not flush pending transition"),Events->Indices.Num(),0);
	TestEqual(TEXT("Release does not flush pending click"),Audio->Plays,0);
	Part->Tick(1);
	TestEqual(TEXT("Release returns to upper endpoint"),FP5bTestAccess::Value(Part),20.0);
	TestTrue(TEXT("Tick delivers upper once after release"),Events->Indices == TArray<int32>{2});
	TestEqual(TEXT("One click after release"),Audio->Plays,1);
	Part->Tick(1);
	TestEqual(TEXT("No duplicate transition on later Tick"),Events->Indices.Num(),1);
	TestEqual(TEXT("No duplicate click on later Tick"),Audio->Plays,1); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5bHysteresis, "vBoR.P5b.E6LastConfirmedAndHysteresis", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5bHysteresis::RunTest(const FString&)
{
	FP5bWorld W; W.Play();
	for (bool bRelease : {false,true})
	{
		AP2bTrackedPart* Part = W.Part(); UP5bEventObserver* Events = P5bObserve(Part); P5bBegin(Part); P5bMove(Part,15); P5bMove(Part,12);
		TestEqual(TEXT("Small reversal retains upper"),Part->GetDetentIndex(),2);
		Part->Tick(0.01f);
		TestEqual(TEXT("No oscillation"),Events->Indices.Num(),1);
		if (bRelease) { FP5bTestAccess::Release(Part); Part->Tick(1); TestEqual(TEXT("Returns to last confirmed upper"),FP5bTestAccess::Value(Part),20.0); }
		else { P5bMove(Part,4); Part->Tick(0.01f); TestTrue(TEXT("Crossing reverse threshold selects center once"),Events->Indices == TArray<int32>({2,1})); }
	}
	AP2bTrackedPart* Tight = W.World->SpawnActorDeferred<AP2bTrackedPart>(AP2bTrackedPart::StaticClass(),FTransform::Identity);
	Tight->Release = EVBoRAxisRelease::Detent; Tight->DetentUpThresholdDeg = Tight->DetentDownThresholdDeg = 10.00001f;
	Tight->FinishSpawning(FTransform::Identity); UP5bEventObserver* TightEvents = P5bObserve(Tight); P5bBegin(Tight);
	P5bMove(Tight,10.00005); P5bMove(Tight,10.00005);
	Tight->Tick(0.01f);
	TestTrue(TEXT("Numerical boundary allowance preserves a narrow valid hysteresis band"),TightEvents->Indices == TArray<int32>{2});
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5bAsymmetric, "vBoR.P5b.E7IndependentThresholds", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5bAsymmetric::RunTest(const FString&)
{
	FP5bWorld W; AP2bTrackedPart* Part = W.Part(); UP5bEventObserver* Events = P5bObserve(Part); Part->DetentUpThresholdDeg = 10; Part->DetentDownThresholdDeg = 18; W.Play(); P5bBegin(Part);
	P5bMove(Part,9); TestEqual(TEXT("Below up threshold"),Part->GetDetentIndex(),1);
	P5bMove(Part,10); TestEqual(TEXT("Up threshold is 10"),Part->GetDetentIndex(),2);
	P5bMove(Part,3); TestEqual(TEXT("Above reverse threshold"),Part->GetDetentIndex(),2);
	P5bMove(Part,2); TestEqual(TEXT("Down threshold from upper is 2"),Part->GetDetentIndex(),1);
	P5bMove(Part,-18); TestEqual(TEXT("Down threshold from center is -18"),Part->GetDetentIndex(),0);
	Part->Tick(0.01f);
	TestTrue(TEXT("Independent thresholds retain notification order"),Events->Indices == TArray<int32>({2,1,0})); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5bRanges, "vBoR.P5b.E8RotatedSDKRangesBothSigns", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5bRanges::RunTest(const FString&)
{
	for (bool Asymmetric : {false,true})
	for (bool Reverse : {false,true})
	{
		FP5bWorld W;
		AP2bTrackedPart* Part = W.Part(); Part->bReverse = Reverse;
		if (Asymmetric) { Part->DetentAnglesDeg = {-15,0,25}; Part->DetentUpThresholdDeg = Part->DetentDownThresholdDeg = 14; }
		Part->AxisPivot->SetRelativeTransform(FTransform(FRotator(15,55,20),FVector(100,23,160))); Part->OnConstruction(Part->GetActorTransform());
		W.Play();
		Part->MaxAngleDeg = 1; P5bBegin(Part);
		for (double Angle : {50.0,-50.0})
		{
			const double Expected = Angle > 0 ? Part->DetentAnglesDeg.Last() : Part->DetentAnglesDeg[0];
			const FQuat ExpectedRotation(FVector::XAxisVector,FMath::DegreesToRadians(Expected * (Reverse ? -1 : 1)));
			TestTrue(TEXT("SDK itself clamps signed endpoints before acceptance"),FP5bTestAccess::SDK(Part,P5bPose(Part,Angle)).GetRotation().Equals(ExpectedRotation,0.001));
			P5bMove(Part,Angle);
			TestTrue(TEXT("SDK and accepted value clamp at detent endpoints"),FMath::IsNearlyEqual(FP5bTestAccess::Value(Part),Expected,0.001));
			TestTrue(TEXT("World pose stays on rotated axis"),Part->AxisPivot->GetComponentQuat().Equals(FP5bTestAccess::Rest(Part).GetRotation() * ExpectedRotation,0.001));
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5bTracking, "vBoR.P5b.E9TrackingPauseDiscardsRecovery", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5bTracking::RunTest(const FString&)
{
	FP5bWorld W; AP2bTrackedPart* Part = W.Part(); UP5bEventObserver* Events = P5bObserve(Part); W.Play(); P5bBegin(Part);
	const FTransform Before = Part->AxisPivot->GetComponentTransform(); Part->bTracked = false;
	P5bMove(Part,30,false); Part->Tick(1);
	TestTrue(TEXT("Loss holds pose"),Part->AxisPivot->GetComponentTransform().Equals(Before));
	Part->bTracked = true; P5bMove(Part,30);
	TestTrue(TEXT("Recovery holds pose"),Part->AxisPivot->GetComponentTransform().Equals(Before));
	TestEqual(TEXT("No recovery transition"),Events->Indices.Num(),0);
	TestEqual(TEXT("Confirmed center retained"),Part->GetDetentIndex(),1);
	TestTrue(TEXT("Discarded recovery is recorded in deg"),FMath::IsNearlyEqual(FP5bTestAccess::Discard(Part),30.0,0.001));
	P5bMove(Part,40); TestTrue(TEXT("Only subsequent ten degrees applied"),FMath::IsNearlyEqual(FP5bTestAccess::Value(Part),10.0,0.001)); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5bRegrab, "vBoR.P5b.E10RegrabDuringReturn", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5bRegrab::RunTest(const FString&)
{
	FP5bWorld W; AP2bTrackedPart* Part = W.Part(); UP5bEventObserver* Events = P5bObserve(Part); W.Play();
	P5bBegin(Part); P5bMove(Part,15); FP5bTestAccess::Release(Part); Part->ReturnDegPerSecond = 10; Part->Tick(0.2f);
	const FTransform Before = Part->AxisPivot->GetComponentTransform(); P5bBegin(Part); P5bMove(Part,17);
	TestTrue(TEXT("Regrab starts at current return pose"),Before.Equals(Part->AxisPivot->GetComponentTransform(),0.001));
	TestEqual(TEXT("Return and regrab emit no transition"),Events->Indices.Num(),1);
	P5bMove(Part,7); TestEqual(TEXT("Regrab movement follows hand"),Part->GetDetentIndex(),2);
	P5bMove(Part,4); TestEqual(TEXT("Later crossing changes center"),Part->GetDetentIndex(),1);
	Part->Tick(0.01f);
	TestTrue(TEXT("Regrab crossing emits center after Tick"),Events->Indices == TArray<int32>({2,1})); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5bEvents, "vBoR.P5b.E11NoOpenCloseEventsOrColor", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5bEvents::RunTest(const FString&)
{
	FP5bWorld W; AP2bTrackedPart* Part = W.Part(); UP5bEventObserver* Events = P5bObserve(Part); W.Play();
	P5bBegin(Part); P5bMove(Part,20); Part->Tick(0.01f);
	TestTrue(TEXT("Held upper uses grab color"),FP5bTestAccess::Color(Part).Equals(Part->GrabColor));
	FP5bTestAccess::Release(Part); Part->Tick(1);
	TestTrue(TEXT("Released upper is transparent"),FP5bTestAccess::Color(Part).Equals(FLinearColor::Transparent));
	TestEqual(TEXT("No opened event"),Events->Opens,0); TestEqual(TEXT("No closed event"),Events->Closes,0);
	TestEqual(TEXT("Release event preserved"),Events->Releases,1);
	TestTrue(TEXT("Only upper detent notification"),Events->Indices == TArray<int32>{2});
	FP5bTestAccess::Hover(Part,NewObject<UIsdkGrabberComponent>(Part));
	TestTrue(TEXT("Upper hover uses hover color"),FP5bTestAccess::Color(Part).Equals(Part->HoverColor)); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5bInvalid, "vBoR.P5b.E12InvalidSettingsDisableAllGrabPaths", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5bInvalid::RunTest(const FString&)
{
	FP5bWorld W;
	for (int32 Case = 0; Case < 14; ++Case)
	{
		FP5bLogs Logs;
		AP2bTrackedPart* Part = W.World->SpawnActorDeferred<AP2bTrackedPart>(AP2bTrackedPart::StaticClass(),FTransform::Identity);
		Part->Release = EVBoRAxisRelease::Detent;
		switch (Case)
		{
		case 0: Part->DetentAnglesDeg = {0}; break;
		case 1: Part->DetentAnglesDeg = {0,0}; break;
		case 2: Part->DetentAnglesDeg = {20,0}; break;
		case 3: Part->DetentAnglesDeg = {-171,0,20}; break;
		case 4: Part->DetentAnglesDeg = {-20,0,171}; break;
		case 5: Part->DetentStartIndex = 3; break;
		case 6: Part->DetentUpThresholdDeg = 0; break;
		case 7: Part->DetentDownThresholdDeg = 21; break;
		case 8: Part->DetentUpThresholdDeg = Part->DetentDownThresholdDeg = 10; break;
		case 9: Part->Movement = EVBoRAxisMovement::Translate; break;
		case 10: Part->DetentDownThresholdDeg = -1; break;
		case 11: Part->DetentUpThresholdDeg = 21; break;
		case 12: Part->DetentUpThresholdDeg = Part->DetentDownThresholdDeg = 5; break;
		case 13: Part->DetentStartIndex = -1; break;
		}
		const double Expected = Part->DetentAnglesDeg.IsValidIndex(Part->DetentStartIndex) ? Part->DetentAnglesDeg[Part->DetentStartIndex] : 0;
		Part->FinishSpawning(FTransform::Identity);
		if (!W.World->HasBegunPlay()) { W.Play(); }
		GLog->FlushThreadedLogs();
		TestEqual(TEXT("One diagnostic per spawn"),Logs.Warnings.Num(),1);
		TestTrue(TEXT("Diagnostic includes reason"),Logs.Warnings.Num() == 1 && Logs.Warnings[0].Contains(TEXT("Invalid detent")) && Logs.Warnings[0].Contains(TEXT("disabled")));
		const TCHAR* Reasons[] = {TEXT("at least two"),TEXT("strictly increasing"),TEXT("strictly increasing"),TEXT("+/-170"),TEXT("+/-170"),TEXT("start index"),TEXT("thresholds"),TEXT("exceeds"),TEXT("sum"),TEXT("Rotate"),TEXT("thresholds"),TEXT("exceeds"),TEXT("sum"),TEXT("start index")};
		TestTrue(TEXT("Diagnostic identifies failed rule"),Logs.Warnings.Num() == 1 && Logs.Warnings[0].Contains(Reasons[Case]));
		TestEqual(TEXT("Invalid config holds initial angle"),FP5bTestAccess::Value(Part),Expected);
		TestEqual(TEXT("Grab collision disabled"),Part->GrabBox->GetCollisionEnabled(),ECollisionEnabled::NoCollision);
		TestFalse(TEXT("Palm disabled"),Part->Grabbable->IsGrabInputMethodAllowed(EIsdkGrabInputMethod::Palm));
		AActor* Owner = W.World->SpawnActor<AActor>(); UIsdkGrabberComponent* Grabber = NewObject<UIsdkGrabberComponent>(Owner);
		Owner->SetRootComponent(Grabber); Grabber->InteractorTags.AddTag(IsdkGameplayTags::TAG_Isdk_Type_Device_Left); Grabber->RegisterComponent();
		FIsdkInteractionPointerEvent Event; Event.Interactor = Grabber; Event.Type = EIsdkPointerEventType::Select;
		Part->Grabbable->GetInteractionPointerEventDelegate().Broadcast(Event);
		TestEqual(TEXT("Injected select rejected"),Part->GetHoldingHand(),INDEX_NONE);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5bDefaults, "vBoR.P5b.E13ExistingDefaults", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5bDefaults::RunTest(const FString&)
{
	TestEqual(TEXT("Release default remains Return"),GetDefault<AVBoRAxisPart>()->Release,EVBoRAxisRelease::Return);
	TestEqual(TEXT("Return enum value unchanged"),static_cast<uint8>(EVBoRAxisRelease::Return),static_cast<uint8>(0));
	TestEqual(TEXT("Stay enum value unchanged"),static_cast<uint8>(EVBoRAxisRelease::Stay),static_cast<uint8>(1)); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5bCar, "vBoR.P5b.E14E16ActualCarBoundsAndCleanup", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5bCar::RunTest(const FString&)
{
	FP5bWorld W; const FTransform CarWorld(FRotator(0,37,0),FVector(123,-256,42));
	AP3VehicleMock* Car = W.World->SpawnActor<AP3VehicleMock>(AP3VehicleMock::StaticClass(),CarWorld); W.Play();
	AVBoRAxisPart* Part = Car->GearLever; AVBoRPokeButton* Button = Car->GearButton;
	if (!TestNotNull(TEXT("Actual gear lever"),Part) || !TestNotNull(TEXT("Actual end button"),Button)) { return false; }
	TestTrue(TEXT("Lever actor uses car origin"),Part->GetActorTransform().Equals(CarWorld,0.001));
	TestTrue(TEXT("Pivot uses exposed position"),Part->AxisPivot->GetRelativeLocation().Equals(Car->GearPivotCm));
	TestTrue(TEXT("Rest mesh preserves authored world pose"),Part->PartMesh->GetComponentTransform().Equals(Part->MeshRestFromRoot * CarWorld,0.001));
	TestEqual(TEXT("Actual gear starts in center"),Part->GetDetentIndex(),1);
	const FVector RestTip = CarWorld.InverseTransformPosition(Button->GetActorLocation());
	for (double Angle : {-20.0,0.0,20.0})
	{
		FP5bTestAccess::SetValue(Part,Angle);
		if (Angle > 0) { TestTrue(TEXT("Upper tip moves toward car +Z"),CarWorld.InverseTransformPosition(Button->GetActorLocation()).Z > RestTip.Z); }
		for (UStaticMeshComponent* Mesh : {Part->PartMesh.Get(),Button->ButtonMesh.Get()})
		{
			const FBox Bounds = Mesh->GetStaticMesh()->GetBoundingBox().TransformBy(Mesh->GetComponentTransform().GetRelativeTransform(CarWorld));
			for (int32 Index : {11,12})
			{
				UStaticMeshComponent* Plate = FP5bTestAccess::Plates(Car)[Index];
				const FBox Obstacle = Plate->GetStaticMesh()->GetBoundingBox().TransformBy(Plate->GetRelativeTransform());
				TestFalse(TEXT("No dashboard or steering overlap at any detent"),Bounds.Intersect(Obstacle));
			}
		}
	}
	Car->Destroy(); TestTrue(TEXT("Car EndPlay destroys lever and button"),Part->IsActorBeingDestroyed() && Button->IsActorBeingDestroyed()); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5bButton, "vBoR.P5b.E15ButtonIndependentOfLeverAndScenario", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5bButton::RunTest(const FString&)
{
	FP5bWorld W; AP3VehicleMock* Car = W.World->SpawnActor<AP3VehicleMock>(); W.Play();
	AVBoRAxisPart* Part = Car->GearLever; AVBoRPokeButton* Button = Car->GearButton;
	TestTrue(TEXT("Button attaches to moving pivot"),Button->GetRootComponent()->GetAttachParent() == Part->AxisPivot);
	TestTrue(TEXT("Rest button face points car +Y"),Button->GetActorUpVector().Equals(Car->GetActorTransform().TransformVectorNoScale(FVector::YAxisVector),0.001));
	TestEqual(TEXT("Hit area not enlarged"),Button->HitAreaScale,1.f);
	TestTrue(TEXT("Button has no scenario tags"),Button->Tags.IsEmpty());
	const FTransform ButtonRelative = Button->GetActorTransform().GetRelativeTransform(Part->AxisPivot->GetComponentTransform());
	FP5bTestAccess::SetValue(Part,20);
	TestTrue(TEXT("Button follows pivot rotation"),Button->GetActorTransform().Equals(ButtonRelative * Part->AxisPivot->GetComponentTransform(),0.001));
	AVBoRScenarioRunner* Runner = W.World->SpawnActor<AVBoRScenarioRunner>(); Runner->Scenario = NewObject<UVBoRScenarioAsset>(Runner);
	Runner->bAlignmentIgnored = true;
	TestTrue(TEXT("Scenario initializes before button count check"),Runner->InitializeFlow());
	TestTrue(TEXT("Scenario counts actual reach button"),FP5bTestAccess::Counted(Runner,Car->ReachButton));
	TestFalse(TEXT("Scenario does not count gear button"),FP5bTestAccess::Counted(Runner,Button));
	UP5bEventObserver* Events = NewObject<UP5bEventObserver>(Button); Button->OnButtonPressed.AddDynamic(Events,&UP5bEventObserver::Pressed);
	for (bool bHeld : {false,true})
	{
		if (bHeld) { P5bBegin(Part); }
		const double Before = FP5bTestAccess::Value(Part); const int32 Index = Part->GetDetentIndex(), Hand = Part->GetHoldingHand();
		FIsdkInteractionPointerEvent Event; Event.Type = EIsdkPointerEventType::Select; Event.Interactable = Button->Poke.Get();
		Button->Poke->GetInteractionPointerEventDelegate().Broadcast(Event);
		TestEqual(TEXT("Press preserves angle"),FP5bTestAccess::Value(Part),Before);
		TestEqual(TEXT("Press preserves confirmed index"),Part->GetDetentIndex(),Index);
		TestEqual(TEXT("Press preserves grab"),Part->GetHoldingHand(),Hand);
	}
	TestEqual(TEXT("One event per select held or released"),Events->Presses,2); return true;
}
#endif
