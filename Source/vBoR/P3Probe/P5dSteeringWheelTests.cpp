#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/OutputDevice.h"
#include "Misc/OutputDeviceRedirector.h"
#include "P5dTestFixture.h"
#include "P3VehicleMock.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "Interaction/IsdkGrabbableComponent.h"
#include "Interaction/IsdkGrabberComponent.h"
#include "Interaction/Pointable/IsdkInteractionPointerEvent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "IsdkRuntimeSettings.h"
#include "IsdkFunctionLibrary.h"
#include "Interaction/GrabDetectors/IsdkHandGrabDetector.h"

struct FP5dTestAccess
{
	static bool Select(AVBoRSteeringWheel* Wheel, int32 Hand) { return Wheel->BeginHold(Hand,nullptr); }
	static void Release(AVBoRSteeringWheel* Wheel, int32 Hand) { Wheel->EndHold(Hand); }
	static void Sample(AVBoRSteeringWheel* Wheel) { Wheel->AdvanceMotion(); }
	static int32 Count(AVBoRSteeringWheel* Wheel) { return Wheel->HandOrder.Num(); }
	static bool Paused(AVBoRSteeringWheel* Wheel) { return Wheel->bPaused; }
	static double Discard(AVBoRSteeringWheel* Wheel) { return Wheel->RecoveryDiscardDeg; }
	static FTransform Rest(AVBoRSteeringWheel* Wheel) { return Wheel->RestPivot * Wheel->GetActorTransform(); }
	static UP5bAudioObserver* Audio(AVBoRSteeringWheel* Wheel) { auto* Audio = NewObject<UP5bAudioObserver>(Wheel); Wheel->StateAudio = Audio; for (auto& Slot : Wheel->SoundAudio) { Slot = Audio; } return Audio; }
	static void Pointer(AVBoRSteeringWheel* Wheel, UIsdkGrabberComponent* Grabber, EIsdkPointerEventType Type)
	{ FIsdkInteractionPointerEvent Event; Event.Interactor = Grabber; Event.Type = Type; Wheel->HandlePointer(Event); }
	static UMaterialInstanceDynamic* Overlay(AVBoRSteeringWheel* Wheel) { return Wheel->StateOverlay; }
	static const TArray<TObjectPtr<UStaticMeshComponent>>& Plates(AP3VehicleMock* Car) { return Car->Plates; }
};

namespace
{
	struct FP5dWarnings : FOutputDevice
	{
		int32 Count = 0;
		FP5dWarnings() { GLog->FlushThreadedLogs(); GLog->AddOutputDevice(this); }
		~FP5dWarnings() { GLog->RemoveOutputDevice(this); }
		virtual void Serialize(const TCHAR* Message, ELogVerbosity::Type Verbosity, const FName& Category) override
		{ if (Category == TEXT("LogSteeringWheel") && Verbosity == ELogVerbosity::Warning) { ++Count; } }
	};
	struct FP5dWorld
	{
		UWorld* World = UWorld::CreateWorld(EWorldType::Game,false);
		void Play() { World->InitializeActorsForPlay(FURL()); World->GetWorldSettings()->NotifyBeginPlay(); }
		AP5dTrackedWheel* Wheel() { return World->SpawnActor<AP5dTrackedWheel>(); }
		~FP5dWorld() { World->EndPlay(EEndPlayReason::Quit); World->DestroyWorld(false); }
	};
	void P5dPose(AP5dTrackedWheel* Wheel, int32 Hand, double Angle, double Radius = 18)
	{
		const double Radians = FMath::DegreesToRadians(Angle);
		Wheel->Hands[Hand] = FTransform(FP5dTestAccess::Rest(Wheel).TransformPosition(FVector(0,Radius*FMath::Sin(Radians),Radius*FMath::Cos(Radians))));
	}
	void P5dMove(AP5dTrackedWheel* Wheel, int32 Hand, double Angle, double Radius = 18)
	{ P5dPose(Wheel,Hand,Angle,Radius); FP5dTestAccess::Sample(Wheel); }
	void P5dGrab(AP5dTrackedWheel* Wheel, int32 Hand, double Angle)
	{ P5dPose(Wheel,Hand,Angle); FP5dTestAccess::Select(Wheel,Hand); }
	UP5dLimitObserver* P5dObserve(AVBoRSteeringWheel* Wheel)
	{ auto* Observer = NewObject<UP5dLimitObserver>(Wheel); Wheel->OnLimitReached.AddDynamic(Observer,&UP5dLimitObserver::Reached); return Observer; }
	UIsdkGrabberComponent* P5dPointer(AVBoRSteeringWheel* Wheel)
	{ auto* Grabber=NewObject<UIsdkGrabberComponent>(Wheel); Grabber->InteractorTags.AddTag(IsdkGameplayTags::TAG_Isdk_Type_Device_Left); return Grabber; }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5dInitial,"vBoR.P5d.E1RestPose",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5dInitial::RunTest(const FString&)
{
	FP5dWorld W; auto* Wheel = W.Wheel(); W.Play();
	TestEqual(TEXT("Starts straight"),Wheel->GetAngleDeg(),0.f);
	TestTrue(TEXT("Authored mesh world pose"),Wheel->WheelMesh->GetComponentTransform().Equals(Wheel->MeshRestFromRoot * Wheel->GetActorTransform(),0.001));
	TestTrue(TEXT("Twelve o'clock above hub"),Wheel->TopMarker->GetComponentLocation().Z > Wheel->AxisPivot->GetComponentLocation().Z);
	TestTrue(TEXT("Debug exposes validation boundary"),Wheel->GetDebugText().Contains(TEXT("hands=0 paused=0 discarded=0.000 deg | not-tested"))); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5dSingle,"vBoR.P5d.E2SingleHandSignAndPose",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5dSingle::RunTest(const FString&)
{
	for (int32 Sign : {-1,1})
	{
		FP5dWorld W; auto* Wheel = W.Wheel(); W.Play(); P5dGrab(Wheel,0,0); P5dMove(Wheel,0,Sign*90);
		TestTrue(TEXT("Clockwise positive"),FMath::IsNearlyEqual(Wheel->GetAngleDeg(),Sign*90.f,0.001f));
		const FVector Marker = FP5dTestAccess::Rest(Wheel).InverseTransformPosition(Wheel->TopMarker->GetComponentLocation());
		TestTrue(TEXT("Marker moves driver right for positive"),Sign*Marker.Y > 15 && FMath::Abs(Marker.Z)<0.001);
	} return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5dTurns,"vBoR.P5d.E3MultipleTurns",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5dTurns::RunTest(const FString&)
{
	FP5dWorld W; auto* Wheel = W.Wheel(); W.Play(); P5dGrab(Wheel,0,0);
	for (int32 Angle=10; Angle<=400; Angle+=10) { P5dMove(Wheel,0,Angle); }
	TestTrue(TEXT("Unwrapped 400 deg"),FMath::IsNearlyEqual(Wheel->GetAngleDeg(),400.f,0.001f)); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5dLimits,"vBoR.P5d.E4LimitsImmediateReverseAndRearm",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5dLimits::RunTest(const FString&)
{
	for (int32 Sign : {-1,1})
	{
		FP5dWorld W; auto* Wheel = W.Wheel(); W.Play(); auto* Observer=P5dObserve(Wheel); auto* Audio=FP5dTestAccess::Audio(Wheel); P5dGrab(Wheel,0,0);
		for (int32 Angle=10; Angle<=470; Angle+=10) { P5dMove(Wheel,0,Sign*Angle); }
		TestEqual(TEXT("Clamped"),Wheel->GetAngleDeg(),Sign*450.f); Wheel->Tick(0.01f); TestEqual(TEXT("One click"),Audio->Plays,1);
		P5dMove(Wheel,0,Sign*465); TestTrue(TEXT("Reverse immediately"),FMath::IsNearlyEqual(Wheel->GetAngleDeg(),Sign*445.f,0.001f));
		P5dMove(Wheel,0,Sign*470); Wheel->Tick(0.01f); TestEqual(TEXT("5 deg does not rearm"),Audio->Plays,1);
		P5dMove(Wheel,0,Sign*460); P5dMove(Wheel,0,Sign*470); Wheel->Tick(0.01f);
		TestTrue(TEXT("Directional limits twice"),Observer->Directions==TArray<int32>({Sign,Sign})); TestEqual(TEXT("10 deg rearmed"),Audio->Plays,2);
	} return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5dStay,"vBoR.P5d.E5ReleaseStays",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5dStay::RunTest(const FString&)
{
	FP5dWorld W; auto* Wheel=W.Wheel(); W.Play(); P5dGrab(Wheel,0,0); P5dMove(Wheel,0,70); FP5dTestAccess::Release(Wheel,0);
	for (int32 I=0; I<5; ++I) { Wheel->Tick(1); } TestTrue(TEXT("No spring"),FMath::IsNearlyEqual(Wheel->GetAngleDeg(),70.f,0.001f)); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5dJumps,"vBoR.P5d.E6NearAxisAndFrameJump",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5dJumps::RunTest(const FString&)
{
	FP5dWorld W; auto* Wheel=W.Wheel(); W.Play(); P5dGrab(Wheel,0,0);
	P5dMove(Wheel,0,60,5); P5dMove(Wheel,0,120); TestEqual(TEXT("Near axis exit rebases"),Wheel->GetAngleDeg(),0.f);
	P5dMove(Wheel,0,130); TestTrue(TEXT("Normal motion resumes"),FMath::IsNearlyEqual(Wheel->GetAngleDeg(),10.f,0.001f));
	P5dMove(Wheel,0,230); TestTrue(TEXT("100 deg discarded"),FMath::IsNearlyEqual(Wheel->GetAngleDeg(),10.f,0.001f));
	P5dMove(Wheel,0,240); TestTrue(TEXT("Continues from jumped position"),FMath::IsNearlyEqual(Wheel->GetAngleDeg(),20.f,0.001f)); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5dPair,"vBoR.P5d.E7PairSegmentRotation",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5dPair::RunTest(const FString&)
{
	for (bool Fixed : {false,true})
	{
		FP5dWorld W; auto* Wheel=W.Wheel(); W.Play(); P5dGrab(Wheel,0,-90); P5dGrab(Wheel,1,90);
		if (!Fixed) { P5dPose(Wheel,0,-60); } P5dPose(Wheel,1,Fixed?150:120); FP5dTestAccess::Sample(Wheel);
		TestTrue(TEXT("Segment turns 30 deg"),FMath::IsNearlyEqual(Wheel->GetAngleDeg(),30.f,0.001f));
	} return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5dShortPair,"vBoR.P5d.E8ShortSegmentFallback",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5dShortPair::RunTest(const FString&)
{
	FP5dWorld W; auto* Wheel=W.Wheel(); W.Play(); P5dGrab(Wheel,1,0); P5dGrab(Wheel,0,10);
	P5dPose(Wheel,0,30); P5dPose(Wheel,1,20); FP5dTestAccess::Sample(Wheel);
	TestTrue(TEXT("First selected right hand drives short pair"),FMath::IsNearlyEqual(Wheel->GetAngleDeg(),20.f,0.001f));
	P5dMove(Wheel,0,150); TestTrue(TEXT("Long segment rebases"),FMath::IsNearlyEqual(Wheel->GetAngleDeg(),20.f,0.001f));
	P5dMove(Wheel,0,25); TestTrue(TEXT("Short segment rebases"),FMath::IsNearlyEqual(Wheel->GetAngleDeg(),20.f,0.001f));
	P5dPose(Wheel,0,35); P5dPose(Wheel,1,30); FP5dTestAccess::Sample(Wheel);
	TestTrue(TEXT("Fallback resumes"),FMath::IsNearlyEqual(Wheel->GetAngleDeg(),30.f,0.001f)); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5dMembership,"vBoR.P5d.E9HandTransitions",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5dMembership::RunTest(const FString&)
{
	FP5dWorld W; auto* Wheel=W.Wheel(); W.Play(); P5dGrab(Wheel,0,0); P5dMove(Wheel,0,40); P5dGrab(Wheel,1,180);
	TestTrue(TEXT("Adding hand preserves angle"),FMath::IsNearlyEqual(Wheel->GetAngleDeg(),40.f,0.001f));
	FP5dTestAccess::Release(Wheel,0); P5dMove(Wheel,1,190); TestTrue(TEXT("Remaining hand continues"),FMath::IsNearlyEqual(Wheel->GetAngleDeg(),50.f,0.001f));
	FP5dTestAccess::Release(Wheel,1); P5dGrab(Wheel,0,-90); P5dMove(Wheel,0,-80);
	TestTrue(TEXT("New grip rebased"),FMath::IsNearlyEqual(Wheel->GetAngleDeg(),60.f,0.001f)); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5dTracking,"vBoR.P5d.E10WholeWheelPauseAndDiscard",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5dTracking::RunTest(const FString&)
{
	for (bool Pair : {false,true})
	{
		FP5dWorld W; auto* Wheel=W.Wheel(); W.Play(); P5dGrab(Wheel,0,0); if (Pair) { P5dGrab(Wheel,1,180); }
		Wheel->bTracked[Pair?1:0]=false; Wheel->Tick(0.01f); P5dPose(Wheel,0,60); if (Pair) { P5dPose(Wheel,1,240); } Wheel->Tick(0.01f);
		TestEqual(TEXT("Whole wheel paused"),Wheel->GetAngleDeg(),0.f); TestTrue(TEXT("Paused flag"),FP5dTestAccess::Paused(Wheel));
		Wheel->bTracked[0]=Wheel->bTracked[1]=true; Wheel->Tick(0.01f);
		TestEqual(TEXT("Recovery has no motion"),Wheel->GetAngleDeg(),0.f); TestTrue(TEXT("Discard recorded"),FMath::IsNearlyEqual(FP5dTestAccess::Discard(Wheel),60.0,0.001));
		P5dPose(Wheel,0,70); if (Pair) { P5dPose(Wheel,1,250); } Wheel->Tick(0.01f);
		TestTrue(TEXT("Subsequent motion only"),FMath::IsNearlyEqual(Wheel->GetAngleDeg(),10.f,0.001f));
	} return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5dSDKRelease,"vBoR.P5d.E11SDKUnselectWhileUntracked",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5dSDKRelease::RunTest(const FString&)
{
	FP5dWorld W; auto* Wheel=W.Wheel(); auto* Grabber=P5dPointer(Wheel); W.Play();
	P5dPose(Wheel,0,0); FP5dTestAccess::Pointer(Wheel,Grabber,EIsdkPointerEventType::Select);
	TestEqual(TEXT("Pointer accepted"),FP5dTestAccess::Count(Wheel),1); Wheel->bTracked[0]=false;
	FP5dTestAccess::Pointer(Wheel,Grabber,EIsdkPointerEventType::Unselect); TestEqual(TEXT("SDK release retains ownership"),FP5dTestAccess::Count(Wheel),1);
	P5dPose(Wheel,0,60); Wheel->bTracked[0]=true; Wheel->Tick(0.01f); P5dMove(Wheel,0,70); TestTrue(TEXT("Retained hand resumes"),FMath::IsNearlyEqual(Wheel->GetAngleDeg(),10.f,0.001f));
	P5dGrab(Wheel,1,180); Wheel->bClosed[0]=false; Wheel->Tick(0.01f); TestEqual(TEXT("Open releases only that hand"),FP5dTestAccess::Count(Wheel),1);
	P5dMove(Wheel,1,190); TestTrue(TEXT("Other hand continues"),FMath::IsNearlyEqual(Wheel->GetAngleDeg(),20.f,0.001f)); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5dRim,"vBoR.P5d.E12RimAcceptanceDeferredCancel",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5dRim::RunTest(const FString&)
{
	for (double Radius : {5.0,12.0,24.0,30.0})
	{
		FP5dWorld W; auto* Wheel=W.Wheel(); W.Play(); P5dPose(Wheel,0,0,Radius); const bool Accepted=FP5dTestAccess::Select(Wheel,0);
		TestEqual(TEXT("Inclusive rim bounds"),Accepted,Radius>=12 && Radius<=24); TestEqual(TEXT("No callback cancellation"),Wheel->GrabComponent->CancelCalls,0);
		Wheel->Tick(0.01f); TestEqual(TEXT("Rejected selection cancelled in Tick"),Wheel->GrabComponent->CancelCalls,Accepted?0:1);
		if (Accepted) { P5dMove(Wheel,0,30,30); TestEqual(TEXT("Held radius may leave rim"),FP5dTestAccess::Count(Wheel),1); }
	} return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5dSettings,"vBoR.P5d.E13PalmOnlyNoThrow",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5dSettings::RunTest(const FString&)
{
	FP5dWorld W; auto* Wheel=W.Wheel(); W.Play(); TestTrue(TEXT("Palm allowed"),Wheel->Grabbable->IsGrabInputMethodAllowed(EIsdkGrabInputMethod::Palm)); TestFalse(TEXT("Pinch forbidden"),Wheel->Grabbable->IsGrabInputMethodAllowed(EIsdkGrabInputMethod::Pinch));
	TestFalse(TEXT("No throw"),Wheel->GrabComponent->IsThrowingEnabled()); TestFalse(TEXT("No physics"),Wheel->WheelMesh->IsSimulatingPhysics());
	TestEqual(TEXT("Multi grab"),Wheel->GrabComponent->GetMultiGrabBehavior(),EIsdkMultiGrabBehavior::MultiGrab); TestEqual(TEXT("Two points"),Wheel->GrabComponent->GetMultiGrabTransformerObject()->MaxGrabPoints(),2); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5dRuntimeSettings,"vBoR.P5d.R1RuntimeSettingsWithoutConstruction",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5dRuntimeSettings::RunTest(const FString&)
{
	FP5dWorld W; auto* Wheel = W.Wheel(); auto* Grabber = P5dPointer(Wheel);
	TestEqual(TEXT("Valid construction leaves grab collision enabled"),Wheel->GrabBox->GetCollisionEnabled(),ECollisionEnabled::QueryOnly);
	// Runtime init without construction (reproduce invalid saved-level values, P5d R1-1)
	Wheel->MaxGrabRadiusCm = Wheel->MinGrabRadiusCm - 1;
	// UE downgrades expected warning to Verbose. Verify once via official count (P5d R1-1)
	AddExpectedMessagePlain(TEXT("Missing mesh or invalid wheel limits/radii; grabbing disabled"),ELogVerbosity::Warning);
	Wheel->PreInitializeComponents(); GLog->FlushThreadedLogs();
	TestFalse(TEXT("Invalid runtime settings disable Palm"),Wheel->Grabbable->IsGrabInputMethodAllowed(EIsdkGrabInputMethod::Palm));
	TestEqual(TEXT("Invalid runtime settings disable grab collision"),Wheel->GrabBox->GetCollisionEnabled(),ECollisionEnabled::NoCollision);
	P5dPose(Wheel,0,0); FP5dTestAccess::Pointer(Wheel,Grabber,EIsdkPointerEventType::Select);
	TestEqual(TEXT("Invalid runtime settings reject selection"),FP5dTestAccess::Count(Wheel),0); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5dConstructionSettings,"vBoR.P5d.R1ConstructionDoesNotWarn",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5dConstructionSettings::RunTest(const FString&)
{
	FP5dWorld W; auto* Wheel = W.Wheel(); Wheel->MaxGrabRadiusCm = Wheel->MinGrabRadiusCm - 1;
	FP5dWarnings Warnings;
	Wheel->OnConstruction(Wheel->GetActorTransform()); Wheel->OnConstruction(Wheel->GetActorTransform()); GLog->FlushThreadedLogs();
	TestEqual(TEXT("Repeated construction does not warn for invalid settings"),Warnings.Count,0); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5dSameFrame,"vBoR.P5d.E14LimitAndReleaseSameFrame",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5dSameFrame::RunTest(const FString&)
{
	FP5dWorld W; auto* Wheel=W.Wheel(); auto* Grabber=P5dPointer(Wheel); W.Play(); auto* Observer=P5dObserve(Wheel); auto* Audio=FP5dTestAccess::Audio(Wheel);
	P5dPose(Wheel,0,0); FP5dTestAccess::Pointer(Wheel,Grabber,EIsdkPointerEventType::Select);
	for (int32 Angle=10; Angle<=440; Angle+=10) { P5dMove(Wheel,0,Angle); }
	P5dPose(Wheel,0,450); FP5dTestAccess::Pointer(Wheel,Grabber,EIsdkPointerEventType::Unselect);
	TestEqual(TEXT("Final pointer pose sampled"),Wheel->GetAngleDeg(),450.f); TestEqual(TEXT("No pointer event"),Observer->Directions.Num(),0); TestEqual(TEXT("No pointer sound"),Audio->Plays,0);
	Wheel->Tick(0.01f); Wheel->Tick(0.01f); TestEqual(TEXT("Exactly one event"),Observer->Directions.Num(),1); TestEqual(TEXT("Exactly one click"),Audio->Plays,1); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5dSDKPair,"vBoR.P5d.E13RealSDKTwoPalmSelection",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5dSDKPair::RunTest(const FString&)
{
	FP5dWorld W; auto* Wheel=W.Wheel(); UIsdkGrabberComponent* Grabbers[2];
	for (int32 Hand=0; Hand<2; ++Hand)
	{
		auto* Owner=W.World->SpawnActor<AActor>(); auto* Grabber=NewObject<UIsdkGrabberComponent>(Owner); Grabbers[Hand]=Grabber; Owner->SetRootComponent(Grabber);
		Grabber->InteractorTags.AddTag(Hand==0?IsdkGameplayTags::TAG_Isdk_Type_Device_Left:IsdkGameplayTags::TAG_Isdk_Type_Device_Right);
		Grabber->SetWorldLocation(FVector(300)); Grabber->SetGrabDetectionTypeAllowed(EIsdkGrabDetectorType::DistanceGrab,false); Grabber->SetGrabDetectionTypeAllowed(EIsdkGrabDetectorType::RayGrab,false);
		Grabber->SetGrabInputMethodAllowed(EIsdkGrabInputMethod::Palm,true); Grabber->SetGrabInputMethodAllowed(EIsdkGrabInputMethod::Pinch,false); Grabber->RegisterComponent();
	}
	W.Play(); P5dPose(Wheel,0,-90); P5dPose(Wheel,1,90);
	for (int32 Hand=0; Hand<2; ++Hand)
	{
		auto* Grabber=Grabbers[Hand]; Grabber->Activate(true); Grabber->SetWorldLocation(Wheel->Hands[Hand].GetLocation()); Grabber->TickComponent(0.016f,LEVELTICK_All,nullptr); Grabber->PalmGrab();
		TestTrue(TEXT("Real palm selects wheel"),Grabber->GetGrabbedComponent()==Wheel->Grabbable);
	}
	TestEqual(TEXT("Two SDK poses"),Wheel->GrabComponent->GetNumGrabbers(),2); TestEqual(TEXT("Two separately owned hands"),FP5dTestAccess::Count(Wheel),2);
	P5dPose(Wheel,0,-60); P5dPose(Wheel,1,120); Wheel->Tick(0.01f); TestTrue(TEXT("Actual selections drive angle"),FMath::IsNearlyEqual(Wheel->GetAngleDeg(),30.f,0.001f));
	Grabbers[0]->PalmRelease(); TestEqual(TEXT("SDK first release leaves second"),FP5dTestAccess::Count(Wheel),1);
	P5dMove(Wheel,1,130); TestTrue(TEXT("Remaining selection continues"),FMath::IsNearlyEqual(Wheel->GetAngleDeg(),40.f,0.001f));
	Grabbers[1]->PalmRelease(); TestEqual(TEXT("All SDK poses released"),Wheel->GrabComponent->GetNumGrabbers(),0); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5dVisual,"vBoR.P5d.E15HoverAndGrabOverlay",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5dVisual::RunTest(const FString&)
{
	FP5dWorld W; auto* Wheel=W.Wheel(); auto* Grabber=P5dPointer(Wheel); W.Play(); FLinearColor Color;
	FP5dTestAccess::Pointer(Wheel,Grabber,EIsdkPointerEventType::Hover); Wheel->Tick(0.01f);
	if (!TestNotNull(TEXT("ISDK overlay"),FP5dTestAccess::Overlay(Wheel))) { return false; }
	FP5dTestAccess::Overlay(Wheel)->GetVectorParameterValue(FMaterialParameterInfo(TEXT("ColorParam")),Color); TestTrue(TEXT("Hover color"),Color.Equals(Wheel->HoverColor));
	P5dGrab(Wheel,0,0); Wheel->Tick(0.01f); FP5dTestAccess::Overlay(Wheel)->GetVectorParameterValue(FMaterialParameterInfo(TEXT("ColorParam")),Color); TestTrue(TEXT("Grab color"),Color.Equals(Wheel->GrabColor)); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5dCar,"vBoR.P5d.E16ActualCarPlacementAndClearance",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5dCar::RunTest(const FString&)
{
	FP5dWorld W; const FTransform CarWorld(FRotator(12,37,5),FVector(123,-256,42)); auto* Car=W.World->SpawnActor<AP3VehicleMock>(AP3VehicleMock::StaticClass(),CarWorld);
	TestTrue(TEXT("Editor plate visible"),FP5dTestAccess::Plates(Car)[12]->IsVisible()); W.Play(); auto* Wheel=Car->SteeringWheel.Get();
	if (!TestNotNull(TEXT("Approximate wheel spawned"),Wheel)) { return false; }
	TestTrue(TEXT("Car origin pivot"),Wheel->GetActorTransform().Equals(CarWorld,0.001)); TestTrue(TEXT("Hub position"),Wheel->AxisPivot->GetRelativeLocation().Equals(Car->SteeringHubCm));
	TestTrue(TEXT("Rest world pose"),Wheel->WheelMesh->GetComponentTransform().Equals(Wheel->MeshRestFromRoot*CarWorld,0.001)); TestEqual(TEXT("Starts straight"),Wheel->GetAngleDeg(),0.f);
	TestTrue(TEXT("Plate hidden in play"),FP5dTestAccess::Plates(Car)[12]->bHiddenInGame);
	// Synthesize wheel pose instead of optical input (sign/overlap on moved/tilted car, P5d E16)
	Wheel->AxisPivot->SetRelativeTransform(FTransform(FQuat(FVector::XAxisVector,-PI/2))*FTransform(Car->SteeringAxisRotation,Car->SteeringHubCm));
	const FVector Marker=CarWorld.InverseTransformPosition(Wheel->TopMarker->GetComponentLocation())-Car->SteeringHubCm;
	TestTrue(TEXT("Positive clockwise points car right"),Marker.Y>15 && FMath::Abs(Marker.Z)<0.001);
	for (double Angle : {-450.0,-90.0,0.0,90.0,450.0})
	{
		Wheel->AxisPivot->SetRelativeTransform(FTransform(FQuat(FVector::XAxisVector,FMath::DegreesToRadians(-Angle)))*FTransform(Car->SteeringAxisRotation,Car->SteeringHubCm));
		for (auto* Mesh : {Wheel->WheelMesh.Get(),Wheel->TopMarker.Get()})
		{
			const FBox Bounds=Mesh->GetStaticMesh()->GetBoundingBox().TransformBy(Mesh->GetComponentTransform().GetRelativeTransform(CarWorld));
			auto* Dash=FP5dTestAccess::Plates(Car)[11].Get(); TestFalse(TEXT("Clears dashboard"),Bounds.Intersect(Dash->GetStaticMesh()->GetBoundingBox().TransformBy(Dash->GetRelativeTransform())));
			for (auto* Lever : {Car->GearLever.Get(),Car->TurnSignalLever.Get()})
			{
				const FTransform Rest=Lever->AxisPivot->GetRelativeTransform();
				for (double Detent : {-20.0,0.0,20.0})
				{
					Lever->AxisPivot->SetRelativeTransform(FTransform(FQuat(FVector::XAxisVector,FMath::DegreesToRadians(Detent)))*Rest);
					TestFalse(TEXT("Clears each lever detent"),Bounds.Intersect(Lever->PartMesh->GetStaticMesh()->GetBoundingBox().TransformBy(Lever->PartMesh->GetComponentTransform().GetRelativeTransform(CarWorld))));
				} Lever->AxisPivot->SetRelativeTransform(Rest);
			}
		}
	} return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5dCleanup,"vBoR.P5d.E17CarCleanup",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5dCleanup::RunTest(const FString&)
{
	FP5dWorld W; auto* Car=W.World->SpawnActor<AP3VehicleMock>(); W.Play(); auto* Wheel=Car->SteeringWheel.Get(); if (!TestNotNull(TEXT("Wheel exists"),Wheel)) { return false; } Car->Destroy(); TestTrue(TEXT("Wheel destroyed"),Wheel->IsActorBeingDestroyed()); return true;
}
#endif
