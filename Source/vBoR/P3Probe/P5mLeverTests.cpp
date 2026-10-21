#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "P5mTestFixture.h"
#include "P3VehicleMock.h"
#include "../Interaction/VBoRHandRigPawn.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "Interaction/IsdkGrabbableComponent.h"
#include "Interaction/IsdkGrabberComponent.h"
#include "IsdkFunctionLibrary.h"
#include "IsdkRuntimeSettings.h"
#include "Materials/MaterialInstanceDynamic.h"

struct FP5mTestAccess
{
	static double Value(AVBoRAxisPart* P) { return P->Value; }
	static int32 Hand(AVBoRAxisPart* P) { return P->PushHand; }
	static int32 Point(AVBoRAxisPart* P) { return P->PushPoint; }
	static int32 Face(AVBoRAxisPart* P) { return P->PushFace; }
	static int32 Starts(AVBoRAxisPart* P) { return P->PushStarts; }
	static bool Pinch(AVBoRAxisPart* P) { return P->bPinch; }
	static bool Enabled(AVBoRAxisPart* P) { return P->bPushEnabled; }
	static FLinearColor Color(AVBoRAxisPart* P) { return P->StateOverlay->K2_GetVectorParameterValue(TEXT("ColorParam")); }
	static UP5bAudioObserver* Audio(AVBoRAxisPart* P) { auto* A=NewObject<UP5bAudioObserver>(P); P->StateAudio=A; for (auto& Slot : P->SoundAudio) { Slot = A; } return A; }
	static void RequireOpen(AVBoRAxisPart* P) { P->bNeedsPalmOpen[0]=true; P->bNeedsPinchOpen[0]=true; }
	static bool NeedsOpen(AVBoRAxisPart* P) { return P->bNeedsPalmOpen[0]; }
};
namespace
{
	struct FP5mWorld
	{
		UWorld* World=UWorld::CreateWorld(EWorldType::Game,false);
		void Play() { World->InitializeActorsForPlay(FURL()); World->GetWorldSettings()->NotifyBeginPlay(); }
		AP5mTrackedPart* Part(bool Reverse=false)
		{
			auto* P=World->SpawnActorDeferred<AP5mTrackedPart>(AP5mTrackedPart::StaticClass(),FTransform::Identity);
			P->Release=EVBoRAxisRelease::Detent; P->bReverse=Reverse;
			P->bAllowPalmGrab=false; P->bAllowPinchGrab=true; P->bAllowPush=true;
			P->MeshRestFromRoot=FTransform(FQuat::Identity,FVector(0,Reverse?-10:10,0),FVector(.02,.2,.02));
			P->FinishSpawning(FTransform::Identity); return P;
		}
		~FP5mWorld() { World->EndPlay(EEndPlayReason::Quit); World->DestroyWorld(false); }
	};
	void P5mPoint(FVBoRDoorHand& H, FVector P, int32 I=1)
	{
		if (!H.bTracked) { for (FVector& Q:H.Points) { Q=FVector(1000); } }
		H.bTracked=true; H.Points[I]=P;
	}
	// 기대 방향은 실제 +Z, 면 접촉은 두께 2 cm 의 아래·윗면으로 정함 (P5m E5·E6)
	FVector P5mContact(double Angle, bool Below=true, bool Reverse=false)
	{
		return FQuat(FVector::XAxisVector,FMath::DegreesToRadians(Reverse?-Angle:Angle)).RotateVector(FVector(0,Reverse?-15:15,Below?-1:1));
	}
	void P5mStart(AP5mTrackedPart* P, bool Below=true, bool Reverse=false, int32 Hand=0)
	{
		P5mPoint(P->Hands[Hand],FVector(0,Reverse?-15:15,Below?-2:2)); P->Tick(.001f);
	}
	UP5cEventObserver* P5mObserve(AVBoRAxisPart* P)
	{
		auto* E=NewObject<UP5cEventObserver>(P); P->OnDetentChanged.AddDynamic(E,&UP5cEventObserver::Changed); P->OnDetentNudge.AddDynamic(E,&UP5cEventObserver::Nudged); return E;
	}
	UIsdkGrabberComponent* P5mGrabber(FP5mWorld& W)
	{
		auto* O=W.World->SpawnActor<AActor>(); auto* G=NewObject<UIsdkGrabberComponent>(O); O->SetRootComponent(G);
		G->InteractorTags.AddTag(IsdkGameplayTags::TAG_Isdk_Type_Device_Left); G->SetWorldLocation(FVector(1000));
		G->SetGrabDetectionTypeAllowed(EIsdkGrabDetectorType::DistanceGrab,false); G->SetGrabDetectionTypeAllowed(EIsdkGrabDetectorType::RayGrab,false);
		G->SetGrabInputMethodAllowed(EIsdkGrabInputMethod::Palm,true); G->SetGrabInputMethodAllowed(EIsdkGrabInputMethod::Pinch,true); G->RegisterComponent(); return G;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5mDefaults,"vBoR.P5m.E1E2DefaultsAndActualCar",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FP5mDefaults::RunTest(const FString&)
{
	FP5mWorld W; auto* Default=W.World->SpawnActor<AVBoRAxisPart>(); auto* Car=W.World->SpawnActor<AP3VehicleMock>(); W.Play();
	TestTrue(TEXT("Default palm allowed"),Default->Grabbable->IsGrabInputMethodAllowed(EIsdkGrabInputMethod::Palm));
	TestFalse(TEXT("Default pinch disabled"),Default->Grabbable->IsGrabInputMethodAllowed(EIsdkGrabInputMethod::Pinch)); TestFalse(TEXT("Default push disabled"),FP5mTestAccess::Enabled(Default));
	for (AVBoRAxisPart* P:{Car->GearLever.Get(),Car->TurnSignalLever.Get()})
	{
		TestFalse(TEXT("Car palm disabled"),P->Grabbable->IsGrabInputMethodAllowed(EIsdkGrabInputMethod::Palm));
		TestTrue(TEXT("Car pinch allowed"),P->Grabbable->IsGrabInputMethodAllowed(EIsdkGrabInputMethod::Pinch)); TestTrue(TEXT("Car push enabled"),FP5mTestAccess::Enabled(P));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5mSDK,"vBoR.P5m.E2E3SDKPinchAndPalmRejection",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FP5mSDK::RunTest(const FString&)
{
	for (double Angle:{14.,16.})
	{
		FP5mWorld W; auto* P=W.Part(); auto* G=P5mGrabber(W); W.Play(); G->Activate(true);
		G->SetWorldLocation(P->GrabBox->GetComponentLocation()); G->TickComponent(.016f,LEVELTICK_All,nullptr); G->PalmGrab();
		TestFalse(TEXT("SDK palm cannot select"),G->GetGrabbedComponent()==P->Grabbable); G->PalmRelease(); G->PinchGrab();
		TestTrue(TEXT("SDK pinch selects"),G->GetGrabbedComponent()==P->Grabbable); TestTrue(TEXT("Captured pinch"),FP5mTestAccess::Pinch(P));
		G->SetWorldLocation(FQuat(FVector::XAxisVector,FMath::DegreesToRadians(Angle)).RotateVector(FVector(0,10,0))); G->TickComponent(.016f,LEVELTICK_All,nullptr);
		TestTrue(TEXT("SDK follows pinch position"),FMath::IsNearlyEqual(FP5mTestAccess::Value(P),Angle,.01)); G->PinchRelease(); P->Tick(1);
		TestEqual(TEXT("Release returns to committed detent"),FP5mTestAccess::Value(P),Angle<15?0.:20.);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5mPinchLoss,"vBoR.P5m.E4RecognizerAndTrackingRetention",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FP5mPinchLoss::RunTest(const FString&)
{
	FP5mWorld W; auto* P=W.Part(); auto* G=P5mGrabber(W); W.Play(); G->Activate(true); G->SetWorldLocation(P->GrabBox->GetComponentLocation()); G->TickComponent(.016f,LEVELTICK_All,nullptr); G->PinchGrab();
	P->bTracked=false; P->Tick(.01f); G->PinchRelease(); TestEqual(TEXT("Loss unselect retains owner"),P->GetHoldingHand(),0);
	P->bTracked=true; G->SetWorldLocation(FVector(0,0,50)); P->Tick(.01f); TestEqual(TEXT("Recovery discards displacement"),FP5mTestAccess::Value(P),0.);
	P->Closed[0]=false; P->Tick(.01f); TestTrue(TEXT("Reads pinch recognizer"),P->LastReadPinch); TestEqual(TEXT("Open pinch releases"),P->GetHoldingHand(),INDEX_NONE);
	FP5mTestAccess::RequireOpen(P); P->Closed[0]=true; P->Tick(.01f); TestTrue(TEXT("Closed pinch blocks rearm"),FP5mTestAccess::NeedsOpen(P));
	P->Closed[0]=false; P->Tick(.01f); TestFalse(TEXT("Pinch opening rearms"),FP5mTestAccess::NeedsOpen(P)); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5mPush,"vBoR.P5m.E5E6FaceDirectionContactAndCommit",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FP5mPush::RunTest(const FString&)
{
	for (bool Reverse:{false,true}) for (bool Below:{false,true}) for (double Magnitude:{5.,16.})
	{
		FP5mWorld W; auto* P=W.Part(Reverse); auto* E=P5mObserve(P); W.Play(); auto* A=FP5mTestAccess::Audio(P); P5mStart(P,Below,Reverse);
		const double Angle=Below?Magnitude:-Magnitude;
		for (double Step=4;Step<Magnitude;Step+=4) { P5mPoint(P->Hands[0],P5mContact(Below?Step:-Step,Below,Reverse)); P->Tick(.001f); }
		P5mPoint(P->Hands[0],P5mContact(Angle,Below,Reverse)); P->Tick(.001f);
		TestTrue(TEXT("Face follows point angle"),FMath::IsNearlyEqual(FP5mTestAccess::Value(P),Angle,.001));
		const FVector Tip=P->AxisPivot->GetComponentTransform().TransformPosition(FVector(0,Reverse?-15:15,0));
		TestTrue(TEXT("Below raises actual tip; above lowers it"),Below?Tip.Z>0:Tip.Z<0);
		const FVector Local=P->AxisPivot->GetComponentTransform().InverseTransformPosition(P->Hands[0].Points[1]);
		TestTrue(TEXT("Selected point remains on actual face"),FMath::IsNearlyEqual(Local.Z,Below?-1.:1.,.001));
		TestEqual(TEXT("Notifications deferred until next Tick"),E->Indices.Num(),0); TestEqual(TEXT("Sound deferred"),A->Plays,0);
		P->Hands[0].bTracked=false; P->Tick(1); const double Expected=Magnitude<15?0.:Below?20.:-20.;
		TestEqual(TEXT("Return to confirmed detent"),FP5mTestAccess::Value(P),Expected); TestEqual(TEXT("One event only after crossing"),E->Indices.Num(),Magnitude<15?0:1); TestEqual(TEXT("One click only after crossing"),A->Plays,Magnitude<15?0:1);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5mNudge,"vBoR.P5m.E7PushNudgeAndRearm",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FP5mNudge::RunTest(const FString&)
{
	FP5mWorld W; auto* P=W.Part(); P->DetentNudgeDeg=5; auto* E=P5mObserve(P); W.Play();
	for (int32 I=0;I<2;++I) { P5mStart(P); P5mPoint(P->Hands[0],P5mContact(6)); P->Tick(.001f); P->Hands[0].bTracked=false; P->Tick(1); }
	TestTrue(TEXT("Each fresh center excursion nudges once"),E->Directions==TArray<int32>{1,1}); TestEqual(TEXT("No detent crossed"),E->Indices.Num(),0); TestEqual(TEXT("Nudge returns to center"),FP5mTestAccess::Value(P),0.); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5mMulti,"vBoR.P5m.E8TwoDetentsOneFrame",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FP5mMulti::RunTest(const FString&)
{
	FP5mWorld W; auto* P=W.Part(); P->DetentStartIndex=0; W.Play(); auto* E=P5mObserve(P);
	P5mPoint(P->Hands[0],FQuat(FVector::XAxisVector,FMath::DegreesToRadians(-20.)).RotateVector(FVector(0,3,-2))); P->Tick(.001f);
	P5mPoint(P->Hands[0],FQuat(FVector::XAxisVector,FMath::DegreesToRadians(16.)).RotateVector(FVector(0,3,-1))); P->Tick(.001f); P->Tick(.001f);
	TestTrue(TEXT("Two adjacent confirmations in order"),E->Indices==TArray<int32>{1,2}); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5mReject,"vBoR.P5m.E9InvalidCrossings",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FP5mReject::RunTest(const FString&)
{
	const FVector Before[]={FVector(0,1,-2),FVector(5,15,-2),FVector(0,15,-2),FVector(0,30,0),FVector(0,15,0)};
	const FVector After[]={FVector(0,1,0),FVector(5,15,0),FVector(0,15,5),FVector(0,15,0),FVector(0,15,.1)};
	for (int32 I=0;I<5;++I)
	{
		FP5mWorld W; auto* P=W.Part(); W.Play();
		if (I!=4) { P5mPoint(P->Hands[0],Before[I]); P->Tick(.001f); }
		P5mPoint(P->Hands[0],After[I]); P->Tick(.001f);
		TestEqual(TEXT("Rejected point never starts push"),FP5mTestAccess::Hand(P),INDEX_NONE); TestEqual(TEXT("Rejected point does not rotate"),FP5mTestAccess::Value(P),0.);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5mRetain,"vBoR.P5m.E10PointFaceAndHandRetention",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FP5mRetain::RunTest(const FString&)
{
	FP5mWorld W; auto* P=W.Part(); W.Play(); P5mStart(P); P5mPoint(P->Hands[1],FVector(0,15,2)); P->Tick(.001f);
	P5mPoint(P->Hands[0],P5mContact(5)); P5mPoint(P->Hands[1],P5mContact(-5,false)); P->Tick(.001f);
	const int32 Face=FP5mTestAccess::Face(P); P5mPoint(P->Hands[0],P5mContact(-10,false),2); P->Tick(.001f);
	TestEqual(TEXT("First hand retained"),FP5mTestAccess::Hand(P),0); TestEqual(TEXT("First point retained"),FP5mTestAccess::Point(P),1); TestEqual(TEXT("First face retained"),FP5mTestAccess::Face(P),Face);
	TestEqual(TEXT("One episode"),FP5mTestAccess::Starts(P),1); TestTrue(TEXT("Other contacts cannot reverse push"),FP5mTestAccess::Value(P)>0); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5mGrabPriority,"vBoR.P5m.E11PushToPinchAndGrabPriority",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FP5mGrabPriority::RunTest(const FString&)
{
	FP5mWorld W; auto* P=W.Part(); auto* G=P5mGrabber(W); W.Play(); P5mStart(P); P5mPoint(P->Hands[0],P5mContact(5)); P->Tick(.001f);
	const double Before=FP5mTestAccess::Value(P); G->Activate(true); G->SetWorldLocation(P->GrabBox->GetComponentLocation()); G->TickComponent(.016f,LEVELTICK_All,nullptr); G->PinchGrab();
	TestEqual(TEXT("Same hand pinch owns lever"),P->GetHoldingHand(),0); TestEqual(TEXT("Push ends on grab"),FP5mTestAccess::Hand(P),INDEX_NONE);
	TestTrue(TEXT("Switch preserves angle"),FMath::IsNearlyEqual(FP5mTestAccess::Value(P),Before,.001));
	P5mPoint(P->Hands[1],FVector(0,15,-2)); P->Tick(.001f); P5mPoint(P->Hands[1],P5mContact(10)); P->Tick(.001f);
	TestEqual(TEXT("Other hand cannot push held lever"),FP5mTestAccess::Hand(P),INDEX_NONE); TestTrue(TEXT("Held angle unchanged"),FMath::IsNearlyEqual(FP5mTestAccess::Value(P),Before,.001)); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5mOccupied,"vBoR.P5m.E12OtherSDKSelection",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FP5mOccupied::RunTest(const FString&)
{
	FP5mWorld W; auto* Other=W.Part(); Other->SetActorLocation(FVector(100,0,0)); auto* P=W.Part(); auto* G=P5mGrabber(W); P->PushGrabbers[0]=G;
	W.Play(); G->Activate(true); G->SetWorldLocation(Other->GrabBox->GetComponentLocation()); G->TickComponent(.016f,LEVELTICK_All,nullptr); G->PinchGrab();
	TestTrue(TEXT("SDK selected another grabbable"),G->GetGrabbedComponent()==Other->Grabbable);
	P5mStart(P); P5mPoint(P->Hands[0],P5mContact(5)); P->Tick(.001f); TestEqual(TEXT("Occupied hand cannot push"),FP5mTestAccess::Starts(P),0); TestEqual(TEXT("Occupied hand cannot rotate"),FP5mTestAccess::Value(P),0.);
	G->PinchRelease(); P->Tick(.001f); TestEqual(TEXT("Release while inside cannot start"),FP5mTestAccess::Starts(P),0);
	P5mStart(P); P5mPoint(P->Hands[0],P5mContact(5)); P->Tick(.001f); TestEqual(TEXT("New crossing after SDK release pushes"),FP5mTestAccess::Starts(P),1); TestTrue(TEXT("Released hand raises lever"),FP5mTestAccess::Value(P)>0); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5mLoss,"vBoR.P5m.E13PushTrackingLossRecovery",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FP5mLoss::RunTest(const FString&)
{
	FP5mWorld W; auto* P=W.Part(); W.Play(); P5mStart(P); P5mPoint(P->Hands[0],P5mContact(5)); P->Tick(.001f);
	P->Hands[0].bTracked=false; P->Tick(1); TestEqual(TEXT("Loss ends push"),FP5mTestAccess::Hand(P),INDEX_NONE); TestEqual(TEXT("Loss returns to center"),FP5mTestAccess::Value(P),0.);
	P5mPoint(P->Hands[0],FVector(0,15,0)); P->Tick(.001f); P5mPoint(P->Hands[0],FVector(0,15,.1)); P->Tick(.001f);
	TestEqual(TEXT("Recovery inside cannot start"),FP5mTestAccess::Starts(P),1); P5mStart(P); P5mPoint(P->Hands[0],P5mContact(5)); P->Tick(.001f); TestEqual(TEXT("New outside crossing starts"),FP5mTestAccess::Starts(P),2); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5mTranslate,"vBoR.P5m.E14TranslationPushWarning",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FP5mTranslate::RunTest(const FString&)
{
	FP5mWorld W; auto* P=W.Part(); P->Release=EVBoRAxisRelease::Stay; P->Movement=EVBoRAxisMovement::Translate;
	AddExpectedMessagePlain(TEXT("Push requires Rotate; push disabled"),ELogVerbosity::Warning); W.Play(); P5mStart(P); P5mPoint(P->Hands[0],FVector(0,15,0)); P->Tick(.001f);
	TestFalse(TEXT("Translation push ignored"),FP5mTestAccess::Enabled(P)); TestEqual(TEXT("Translation unchanged"),FP5mTestAccess::Value(P),0.); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5mInvalid,"vBoR.P5m.E1InvalidSettingsDisableEveryInput",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FP5mInvalid::RunTest(const FString&)
{
	FP5mWorld W; auto* P=W.Part(); P->bAllowPalmGrab=true; P->DetentUpThresholdDeg=0;
	AddExpectedMessagePlain(TEXT("Invalid detent"),ELogVerbosity::Warning); W.Play();
	TestFalse(TEXT("Invalid palm disabled"),P->Grabbable->IsGrabInputMethodAllowed(EIsdkGrabInputMethod::Palm));
	TestFalse(TEXT("Invalid pinch disabled"),P->Grabbable->IsGrabInputMethodAllowed(EIsdkGrabInputMethod::Pinch)); TestFalse(TEXT("Invalid push disabled"),FP5mTestAccess::Enabled(P));
	P5mStart(P); P5mPoint(P->Hands[0],P5mContact(5)); P->Tick(.001f); TestEqual(TEXT("Invalid push does not move"),FP5mTestAccess::Value(P),0.); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5mColors,"vBoR.P5m.E15PushAndGrabOverlay",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FP5mColors::RunTest(const FString&)
{
	FP5mWorld W; auto* P=W.Part(); auto* G=P5mGrabber(W); W.Play(); P5mStart(P); P5mPoint(P->Hands[0],P5mContact(5)); P->Tick(.001f);
	TestTrue(TEXT("Push overlay"),FP5mTestAccess::Color(P).Equals(P->PushingColor,.001f));
	G->Activate(true); G->SetWorldLocation(P->GrabBox->GetComponentLocation()); G->TickComponent(.016f,LEVELTICK_All,nullptr); G->PinchGrab(); P->Tick(.001f);
	TestTrue(TEXT("Grab overlay"),FP5mTestAccess::Color(P).Equals(P->GrabColor,.001f)); return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5nLeverMargin,"vBoR.P5n.E2LeverPushMargin",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5nLeverMargin::RunTest(const FString&)
{
	for (bool Reverse : {false,true})
	{
		for (double OutsideCm : {1.2,2.0})
		{
			FP5mWorld W; auto* P=W.Part(Reverse); P->PushMarginCm=1.5f; W.Play();
			const double Y=Reverse?-15:15;
			P5mPoint(P->Hands[0],FVector(1+OutsideCm,Y,-2)); P->Tick(0.001f);
			P5mPoint(P->Hands[0],FVector(1+OutsideCm,Y,-0.5)); P->Tick(0.001f);
			TestEqual(TEXT("E2 axial margin accepts 1.2 cm but rejects 2 cm"),FP5mTestAccess::Hand(P),OutsideCm<1.5?0:INDEX_NONE);
		}
	}
	return true;
}
#endif
