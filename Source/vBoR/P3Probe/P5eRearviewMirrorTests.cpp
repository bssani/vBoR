#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/OutputDevice.h"
#include "Misc/OutputDeviceRedirector.h"
#include "P5eTestFixture.h"
#include "P3VehicleMock.h"
#include "P3FlowGameMode.h"
#include "../Interaction/VBoRSteeringWheel.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "Interaction/IsdkGrabbableComponent.h"
#include "Interaction/IsdkGrabberComponent.h"
#include "Interaction/Pointable/IsdkInteractionPointerEvent.h"
#include "IsdkRuntimeSettings.h"
#include "IsdkFunctionLibrary.h"
#include "Materials/MaterialInstanceDynamic.h"
#include <limits>

struct FP5eTestAccess
{
	static FVBoRTwoAxisMotion& Motion(AVBoRTwoAxisPart* Part) { return Part->Motion; }
	static bool Select(AVBoRTwoAxisPart* Part, int32 Hand=0) { return Part->BeginHold(Hand,nullptr); }
	static void Release(AVBoRTwoAxisPart* Part) { Part->Motion.ReleaseGrab(); Part->HoldingGrabber = nullptr; }
	static void Apply(AVBoRTwoAxisPart* Part) { Part->ApplyPose(); }
	static UMaterialInstanceDynamic* Overlay(AVBoRTwoAxisPart* Part) { return Part->StateOverlay; }
	static bool Pinch(AVBoRTwoAxisPart* Part) { return Part->bPinch; }
	static void Pointer(AVBoRTwoAxisPart* Part, UIsdkGrabberComponent* Grabber, EIsdkPointerEventType Type)
	{ FIsdkInteractionPointerEvent Event; Event.Interactor = Grabber; Event.Type = Type; Part->HandlePointer(Event); }
	static const TArray<TObjectPtr<UStaticMeshComponent>>& Plates(AP3VehicleMock* Car) { return Car->Plates; }
};

namespace
{
	struct FP5eWorld
	{
		UWorld* World = UWorld::CreateWorld(EWorldType::Game,false);
		void Play() { World->InitializeActorsForPlay(FURL()); World->GetWorldSettings()->NotifyBeginPlay(); }
		AP5eTrackedPart* Part() { return World->SpawnActor<AP5eTrackedPart>(); }
		~FP5eWorld() { World->EndPlay(EEndPlayReason::Quit); World->DestroyWorld(false); }
	};
	FQuat P5eRotation(double Yaw, double Pitch)
	{ return FQuat(FVector::ZAxisVector,FMath::DegreesToRadians(Yaw))*FQuat(FVector::YAxisVector,FMath::DegreesToRadians(-Pitch)); }
	void P5eMove(AP5eTrackedPart* Part, double Yaw, double Pitch)
	{ Part->Wrist.SetLocation(P5eRotation(Yaw,Pitch).RotateVector(FVector(-20,0,0))); Part->Tick(0.016f); }
	void P5ePoint(FVBoRDoorHand& Hand, FVector Point)
	{
		Hand = FVBoRDoorHand(); Hand.bTracked = true;
		for (FVector& P : Hand.Points) { P = FVector(1000); }
		Hand.Points[1] = Point;
	}
	void P5ePush(AP5eTrackedPart* Part, double Y=10, double Z=0)
	{
		P5ePoint(Part->Hands[0],FVector(-5,Y,Z)); Part->Tick(0.016f);
		P5ePoint(Part->Hands[0],FVector(-2,Y,Z)); Part->Tick(0.016f);
	}
	UIsdkGrabberComponent* P5ePointer(AVBoRTwoAxisPart* Part)
	{ auto* Grabber=NewObject<UIsdkGrabberComponent>(Part); Grabber->InteractorTags.AddTag(IsdkGameplayTags::TAG_Isdk_Type_Device_Left); return Grabber; }
	struct FP5eWarnings : FOutputDevice
	{
		int32 Count = 0;
		FP5eWarnings() { GLog->FlushThreadedLogs(); GLog->AddOutputDevice(this); }
		~FP5eWarnings() { GLog->RemoveOutputDevice(this); }
		virtual void Serialize(const TCHAR* Message, ELogVerbosity::Type Verbosity, const FName& Category) override
		{
			// 기대 경고는 UE 자동화가 Verbose 로 낮춘 뒤 전달함 (P5l E13)
			if (Category == TEXT("LogTwoAxisPart") && FString(Message).Contains(TEXT("Missing mesh or invalid two-axis settings")) &&
				(Verbosity == ELogVerbosity::Warning || Verbosity == ELogVerbosity::Verbose)) { ++Count; }
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5eRest,"vBoR.P5e.E1RestPose",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5eRest::RunTest(const FString&)
{
	FP5eWorld W; auto* Part=W.Part(); W.Play();
	TestEqual(TEXT("Yaw rest"),Part->GetYawDeg(),0.f); TestEqual(TEXT("Pitch rest"),Part->GetPitchDeg(),0.f);
	TestTrue(TEXT("Authored world pose"),Part->PartMesh->GetComponentTransform().Equals(Part->MeshRestFromRoot*Part->GetActorTransform(),0.001));
	TestTrue(TEXT("No reflection boundary"),Part->GetDebugText().Contains(TEXT("반사 없음")) && Part->GetDebugText().Contains(TEXT("not-tested"))); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5eYaw,"vBoR.P5e.E2GrabPointYaw",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5eYaw::RunTest(const FString&)
{
	for (double Y : {0.0,12.0})
	{
		for (int32 Sign : {-1,1})
		{
			FP5eWorld W; auto* Part=W.Part(); W.Play(); Part->Wrist.SetLocation(FVector(-4,Y,0)); FP5eTestAccess::Select(Part);
			Part->Wrist.AddToTranslation(Y==0 ? FVector(0,Sign,0) : FVector(-Sign,0,0)); Part->Tick(0.016f);
			const double Expected=Y==0 ? -Sign*5.614497 : (Sign>0 ? 4.184916 : -4.398705);
			TestTrue(TEXT("Center damped, edge full lever"),FMath::IsNearlyEqual(static_cast<double>(Part->GetYawDeg()),Expected,0.05));
			const FVector Normal=-Part->PitchAxis->GetForwardVector();
			TestTrue(TEXT("Normal direction against actual right vector"),FVector::DotProduct(Normal,Part->GetActorRightVector())*(Y==0 ? Sign : -Sign)>0);
			TestTrue(TEXT("No pitch"),FMath::IsNearlyZero(Part->GetPitchDeg(),0.001f));
		}
	} return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5ePitch,"vBoR.P5e.E3PitchAndDiagonal",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5ePitch::RunTest(const FString&)
{
	for (double Yaw : {0.0,10.0})
	{
		for (int32 Sign : {-1,1})
		{
			FP5eWorld W; auto* Part=W.Part(); W.Play(); FP5eTestAccess::Select(Part); P5eMove(Part,Yaw,Sign*8);
			TestTrue(TEXT("Yaw"),FMath::IsNearlyEqual(Part->GetYawDeg(),static_cast<float>(Yaw),0.001f));
			TestTrue(TEXT("Pitch"),FMath::IsNearlyEqual(Part->GetPitchDeg(),Sign*8.f,0.001f));
		}
	} return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5eLimits,"vBoR.P5e.E4LimitsImmediateReverse",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5eLimits::RunTest(const FString&)
{
	for (int32 Sign : {-1,1})
	{
		for (bool Pitch : {false,true})
		{
			FP5eWorld W; auto* Part=W.Part(); W.Play(); FP5eTestAccess::Select(Part);
			for (double Angle : {10.0,20.0,30.0,40.0}) { P5eMove(Part,Pitch?0:Sign*Angle,Pitch?Sign*Angle:0); }
			TestTrue(TEXT("Clamped"),FMath::IsNearlyEqual(Pitch?Part->GetPitchDeg():Part->GetYawDeg(),Sign*(Pitch?15.f:25.f),0.001f));
			P5eMove(Part,Pitch?0:Sign*35,Pitch?Sign*35:0);
			TestTrue(TEXT("Immediate reverse"),FMath::IsNearlyEqual(Pitch?Part->GetPitchDeg():Part->GetYawDeg(),Sign*(Pitch?10.f:20.f),0.001f));
		}
	} return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5eRelease,"vBoR.P5e.E5ReleaseStays",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5eRelease::RunTest(const FString&)
{
	FP5eWorld W; auto* Part=W.Part(); W.Play(); FP5eTestAccess::Select(Part); P5eMove(Part,10,8); FP5eTestAccess::Release(Part);
	for (int32 I=0; I<5; ++I) { Part->Tick(1); }
	TestTrue(TEXT("Yaw stays"),FMath::IsNearlyEqual(Part->GetYawDeg(),10.f,0.001f)); TestTrue(TEXT("Pitch stays"),FMath::IsNearlyEqual(Part->GetPitchDeg(),8.f,0.001f)); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5eIgnored,"vBoR.P5e.E6WristIgnoredAndDistanceJump",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5eIgnored::RunTest(const FString&)
{
	FP5eWorld W; auto* Part=W.Part(); W.Play(); FP5eTestAccess::Select(Part);
	for (const FVector Axis : {FVector::XAxisVector,FVector::YAxisVector,FVector::ZAxisVector})
	{
		Part->Wrist.SetRotation(FQuat(Axis,FMath::DegreesToRadians(20.0))); Part->Tick(0.016f);
		TestEqual(TEXT("Wrist ignored yaw"),Part->GetYawDeg(),0.f); TestEqual(TEXT("Wrist ignored pitch"),Part->GetPitchDeg(),0.f);
	}
	Part->Wrist.AddToTranslation(FVector(0,6,0)); Part->Tick(0.016f); TestEqual(TEXT("6 cm jump ignored"),Part->GetYawDeg(),0.f);
	Part->Wrist.AddToTranslation(FVector(0,4,0)); Part->Tick(0.016f);
	TestTrue(TEXT("4 cm accepted from new baseline"),FMath::IsNearlyEqual(static_cast<double>(Part->GetYawDeg()),-9.865807,0.001)); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5eInputs,"vBoR.P5e.E7PalmPinchSingleHand",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5eInputs::RunTest(const FString&)
{
	FP5eWorld W; auto* Part=W.Part(); W.Play();
	TestTrue(TEXT("Palm allowed"),Part->Grabbable->IsGrabInputMethodAllowed(EIsdkGrabInputMethod::Palm));
	TestTrue(TEXT("Pinch allowed"),Part->Grabbable->IsGrabInputMethodAllowed(EIsdkGrabInputMethod::Pinch));
	TestFalse(TEXT("No throw"),Part->GrabComponent->IsThrowingEnabled()); TestFalse(TEXT("No physics"),Part->PartMesh->IsSimulatingPhysics());
	TestEqual(TEXT("First retained"),Part->GrabComponent->GetMultiGrabBehavior(),EIsdkMultiGrabBehavior::SingleGrabFirstRetained);
	TestEqual(TEXT("One grab point"),Part->GrabComponent->GetSingleGrabTransformerObject()->MaxGrabPoints(),1);
	TestTrue(TEXT("First accepted"),FP5eTestAccess::Select(Part)); TestFalse(TEXT("Other hand rejected"),FP5eTestAccess::Select(Part,1));
	TestEqual(TEXT("First remains owner"),FP5eTestAccess::Motion(Part).GrabHand,0); P5eMove(Part,10,8);
	TestTrue(TEXT("Single grab point drives both axes"),FMath::IsNearlyEqual(Part->GetYawDeg(),10.f,0.001f) && FMath::IsNearlyEqual(Part->GetPitchDeg(),8.f,0.001f)); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5eSDKInputs,"vBoR.P5e.E7RealSDKPalmAndPinch",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5eSDKInputs::RunTest(const FString&)
{
	for (bool Pinch : {false,true})
	{
		FP5eWorld W; auto* Part=W.Part(); auto* Pawn=W.World->SpawnActor<AP3PalmGrabPawn>();
		auto* Owner=W.World->SpawnActor<AActor>(); auto* Grabber=NewObject<UIsdkGrabberComponent>(Owner); Owner->SetRootComponent(Grabber);
		Grabber->InteractorTags.AddTag(IsdkGameplayTags::TAG_Isdk_Type_Device_Left); Grabber->SetWorldLocation(FVector(1000));
		Grabber->SetGrabDetectionTypeAllowed(EIsdkGrabDetectorType::DistanceGrab,false); Grabber->SetGrabDetectionTypeAllowed(EIsdkGrabDetectorType::RayGrab,false);
		Grabber->SetGrabInputMethodAllowed(EIsdkGrabInputMethod::Palm,true); Grabber->SetGrabInputMethodAllowed(EIsdkGrabInputMethod::Pinch,true); Grabber->RegisterComponent(); W.Play();
		for (int32 Hand=0; Hand<2; ++Hand)
		{
			bool Closed=true; TestTrue(TEXT("SDK pinch recognizer readable"),Hand==0?Pawn->LeftRig->ReadPinchGrab(Closed):Pawn->RightRig->ReadPinchGrab(Closed));
			TestTrue(TEXT("P3 rig permits pinch"),Pawn->GetHandRig(Hand)->GetGrabber()->IsGrabInputMethodAllowed(EIsdkGrabInputMethod::Pinch));
		}
		Grabber->Activate(true); Grabber->SetWorldLocation(Part->GrabBox->GetComponentLocation()); Grabber->TickComponent(0.016f,LEVELTICK_All,nullptr);
		if (Pinch) { Grabber->PinchGrab(); } else { Grabber->PalmGrab(); }
		TestTrue(TEXT("SDK selects actual part"),Grabber->GetGrabbedComponent()==Part->Grabbable);
		TestEqual(TEXT("Input method captured"),FP5eTestAccess::Pinch(Part),Pinch); TestEqual(TEXT("SDK owner retained"),FP5eTestAccess::Motion(Part).GrabHand,0);
		P5eMove(Part,10,8); TestTrue(TEXT("Both inputs drive same yaw and pitch"),FMath::IsNearlyEqual(Part->GetYawDeg(),10.f,0.001f) && FMath::IsNearlyEqual(Part->GetPitchDeg(),8.f,0.001f));
		Grabber->TickComponent(0.016f,LEVELTICK_All,nullptr);
		TestTrue(TEXT("SDK transformer preserves constrained pose"),Part->PitchAxis->GetRelativeRotation().Quaternion().Equals(FQuat(FVector::YAxisVector,FMath::DegreesToRadians(-8.0)),0.001));
		if (Pinch) { Grabber->PinchRelease(); } else { Grabber->PalmRelease(); }
		TestEqual(TEXT("SDK release clears ownership"),FP5eTestAccess::Motion(Part).GrabHand,INDEX_NONE);
	} return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5eTracking,"vBoR.P5e.E8TrackingAndSDKRelease",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5eTracking::RunTest(const FString&)
{
	for (auto Type : {EIsdkPointerEventType::Unselect,EIsdkPointerEventType::Cancel})
	{
		FP5eWorld W; auto* Part=W.Part(); auto* Grabber=P5ePointer(Part); W.Play(); FP5eTestAccess::Pointer(Part,Grabber,EIsdkPointerEventType::Select);
		Part->bTracked=false; Part->Tick(0.016f); FP5eTestAccess::Pointer(Part,Grabber,Type); P5eMove(Part,20,0);
		TestTrue(TEXT("Paused"),FP5eTestAccess::Motion(Part).bPaused); TestEqual(TEXT("Ownership retained"),FP5eTestAccess::Motion(Part).GrabHand,0);
		Part->bTracked=true; Part->Tick(0.016f); TestEqual(TEXT("Recovery no motion"),Part->GetYawDeg(),0.f);
		TestTrue(TEXT("Discard recorded in cm"),FMath::IsNearlyEqual(FP5eTestAccess::Motion(Part).RecoveryDiscardCm,2*20*FMath::Sin(FMath::DegreesToRadians(10.0)),0.001)); P5eMove(Part,25,0);
		TestTrue(TEXT("New motion only"),FMath::IsNearlyEqual(Part->GetYawDeg(),5.f,0.001f)); FP5eTestAccess::Pointer(Part,Grabber,Type);
		TestEqual(TEXT("Retains until actual open"),FP5eTestAccess::Motion(Part).GrabHand,0);
		Part->bClosed=false; Part->Tick(0.016f); TestEqual(TEXT("Open releases"),FP5eTestAccess::Motion(Part).GrabHand,INDEX_NONE);
	} return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5ePush,"vBoR.P5e.E9EdgePushStopAndWithdraw",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5ePush::RunTest(const FString&)
{
	FP5eWorld W; auto* Part=W.Part(); W.Play(); P5ePush(Part);
	TestTrue(TEXT("Positive Y edge push turns normal toward positive Y"),Part->GetYawDeg()<0); const float Before=Part->GetYawDeg();
	TestTrue(TEXT("Pushed edge retreats behind its rest plane"),FP5eTestAccess::Motion(Part).Rotation().RotateVector(FVector(-4,10,0)).X > -4);
	const FVector Local=FP5eTestAccess::Motion(Part).Rotation().UnrotateVector(Part->Hands[0].Points[1]);
	TestTrue(TEXT("Finger lies on face"),FMath::IsNearlyEqual(Local.X,-4.0,0.001));
	for (int32 I=0; I<5; ++I) { Part->Tick(0.1f); } TestTrue(TEXT("Stationary finger stops mirror"),FMath::IsNearlyEqual(Part->GetYawDeg(),Before,0.001f));
	P5ePoint(Part->Hands[0],FP5eTestAccess::Motion(Part).Rotation().RotateVector(FVector(-8,10,0))); Part->Tick(0.016f);
	TestTrue(TEXT("Withdrawal never pulls"),FMath::IsNearlyEqual(Part->GetYawDeg(),Before,0.001f)); TestEqual(TEXT("Push ended"),FP5eTestAccess::Motion(Part).PushHand,INDEX_NONE); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5eEdges,"vBoR.P5e.E10VerticalEdgesAndCorner",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5eEdges::RunTest(const FString&)
{
	for (double Y : {0.0,10.0})
	{
		for (int32 Sign : {-1,1})
		{
			FP5eWorld W; auto* Part=W.Part(); W.Play(); P5ePush(Part,Y,Sign*3.5);
			TestEqual(TEXT("Top or bottom push preserves pitch"),Part->GetPitchDeg(),0.f);
			TestTrue(TEXT("Only corner push changes yaw"),Y==0 ? FMath::IsNearlyZero(Part->GetYawDeg(),0.001f) : Part->GetYawDeg()<0);
			const double EdgeX = FP5eTestAccess::Motion(Part).Rotation().RotateVector(FVector(-4,Y,Sign*3.5)).X;
			TestTrue(TEXT("Center stays still; corner retreats"),Y==0 ? FMath::IsNearlyEqual(EdgeX,-4.0,0.001) : EdgeX > -4);
		}
	} return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5eRejectPush,"vBoR.P5e.E11RejectInvalidCrossings",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5eRejectPush::RunTest(const FString&)
{
	const FVector Before[] = {FVector(-5,0,0),FVector(-5,20,0),FVector(3,10,0),FVector(-2,20,0),FVector(-5,10,0)};
	const FVector After[] = {FVector(-2,0,0),FVector(-2,20,0),FVector(-2,10,0),FVector(-2,10,0),FVector(3,10,0)};
	for (int32 I=0; I<UE_ARRAY_COUNT(Before); ++I)
	{
		FP5eWorld W; auto* Part=W.Part(); W.Play(); P5ePoint(Part->Hands[0],Before[I]); Part->Tick(0.016f); P5ePoint(Part->Hands[0],After[I]); Part->Tick(0.016f);
		TestEqual(TEXT("Invalid crossing yaw"),Part->GetYawDeg(),0.f); TestEqual(TEXT("Invalid crossing pitch"),Part->GetPitchDeg(),0.f);
		TestEqual(TEXT("Not pushing"),FP5eTestAccess::Motion(Part).PushHand,INDEX_NONE);
	} return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5eModes,"vBoR.P5e.E12GrabPushExclusionAndHandoff",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5eModes::RunTest(const FString&)
{
	FP5eWorld W; auto* Part=W.Part(); W.Play(); FP5eTestAccess::Select(Part); P5ePoint(Part->Hands[1],FVector(-5,10,0)); Part->Tick(0.016f);
	P5ePoint(Part->Hands[1],FVector(-2,10,0)); Part->Tick(0.016f); TestEqual(TEXT("Grab excludes other push"),Part->GetYawDeg(),0.f);
	FP5eTestAccess::Release(Part); Part->Hands[1].bTracked=false; P5ePush(Part); const float Before=Part->GetYawDeg();
	Part->Wrist.SetLocation(P5eRotation(20,8).RotateVector(FVector(-20,0,0))); FP5eTestAccess::Select(Part); Part->Tick(0.016f);
	TestTrue(TEXT("Push to grab no jump"),FMath::IsNearlyEqual(Part->GetYawDeg(),Before,0.001f)); TestEqual(TEXT("Push cleared"),FP5eTestAccess::Motion(Part).PushHand,INDEX_NONE);
	P5eMove(Part,25,8); TestTrue(TEXT("Subsequent grab follows"),FMath::IsNearlyEqual(Part->GetYawDeg(),Before+5,0.001f)); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5ePushTracking,"vBoR.P5e.E13PushTrackingLoss",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5ePushTracking::RunTest(const FString&)
{
	FP5eWorld W; auto* Part=W.Part(); W.Play(); P5ePush(Part); const float Before=Part->GetYawDeg(); const FQuat Rotation=FP5eTestAccess::Motion(Part).Rotation();
	Part->Hands[0].bTracked=false; Part->Tick(0.016f); TestEqual(TEXT("Loss ends push"),FP5eTestAccess::Motion(Part).PushHand,INDEX_NONE);
	P5ePoint(Part->Hands[0],Rotation.RotateVector(FVector(-2,10,0))); Part->Tick(0.016f); Part->Tick(0.016f);
	TestTrue(TEXT("Recovery from behind ignored"),FMath::IsNearlyEqual(Part->GetYawDeg(),Before,0.001f));
	P5ePoint(Part->Hands[0],Rotation.RotateVector(FVector(-5,10,0))); Part->Tick(0.016f);
	P5ePoint(Part->Hands[0],Rotation.RotateVector(FVector(-3,10,0))); Part->Tick(0.016f);
	TestTrue(TEXT("Fresh front crossing resumes"),Part->GetYawDeg()<Before); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5ePushOwnership,"vBoR.P5e.E14FirstPushingHand",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5ePushOwnership::RunTest(const FString&)
{
	for (int32 First : {0,1})
	{
		FP5eWorld W; auto* Part=W.Part(); W.Play(); P5ePoint(Part->Hands[First],FVector(-5,10,0)); Part->Tick(0.016f);
		P5ePoint(Part->Hands[First],FVector(-3,10,0)); Part->Tick(0.016f); TestEqual(TEXT("First contact owns push"),FP5eTestAccess::Motion(Part).PushHand,First);
		const float Before=Part->GetYawDeg(); P5ePoint(Part->Hands[1-First],FP5eTestAccess::Motion(Part).Rotation().RotateVector(FVector(-5,-10,0))); Part->Tick(0.016f);
		P5ePoint(Part->Hands[1-First],FP5eTestAccess::Motion(Part).Rotation().RotateVector(FVector(-2,-10,0))); Part->Tick(0.016f);
		TestEqual(TEXT("Second contact cannot steal"),FP5eTestAccess::Motion(Part).PushHand,First); TestTrue(TEXT("Other hand cannot turn"),FMath::IsNearlyEqual(Part->GetYawDeg(),Before,0.001f));
	}
	FP5eWorld W; auto* Part=W.Part(); W.Play();
	for (int32 H=0; H<2; ++H) { P5ePoint(Part->Hands[H],FVector(-5,H==0?10:-10,0)); } Part->Tick(0.016f);
	for (int32 H=0; H<2; ++H) { P5ePoint(Part->Hands[H],FVector(-3,H==0?10:-10,0)); } Part->Tick(0.016f);
	TestEqual(TEXT("Same-frame contact has one owner"),FP5eTestAccess::Motion(Part).PushHand,0); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5eSettings,"vBoR.P5e.E15RuntimeValidation",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5eSettings::RunTest(const FString&)
{
	for (int32 Invalid=0; Invalid<4; ++Invalid)
	{
		FP5eWorld W; auto* Part=W.Part();
		if (Invalid==0) { Part->PartMesh->SetStaticMesh(nullptr); }
		else if (Invalid==1) { Part->YawLimitDeg=-1; }
		else if (Invalid==2) { Part->PitchLimitDeg=-1; }
		else { Part->CenterExclusionCm=-1; }
		{ FP5eWarnings Warnings; Part->OnConstruction(Part->GetActorTransform()); Part->OnConstruction(Part->GetActorTransform()); GLog->FlushThreadedLogs(); TestEqual(TEXT("Construction never warns"),Warnings.Count,0); }
		AddExpectedMessagePlain(TEXT("Missing mesh or invalid two-axis settings; grabbing and pushing disabled"),ELogVerbosity::Warning);
		W.Play(); GLog->FlushThreadedLogs();
		TestFalse(TEXT("Invalid Palm disabled"),Part->Grabbable->IsGrabInputMethodAllowed(EIsdkGrabInputMethod::Palm)); TestFalse(TEXT("Invalid Pinch disabled"),Part->Grabbable->IsGrabInputMethodAllowed(EIsdkGrabInputMethod::Pinch));
		TestEqual(TEXT("Collision disabled"),Part->GrabBox->GetCollisionEnabled(),ECollisionEnabled::NoCollision); TestFalse(TEXT("Overlap disabled"),Part->GrabBox->GetGenerateOverlapEvents());
		TestFalse(TEXT("Injected grab rejected"),FP5eTestAccess::Select(Part)); P5ePush(Part); TestEqual(TEXT("Invalid push disabled"),Part->GetYawDeg(),0.f);
	} return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5eVisual,"vBoR.P5e.E16HoverGrabPushColors",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5eVisual::RunTest(const FString&)
{
	FP5eWorld W; auto* Part=W.Part(); auto* Grabber=P5ePointer(Part); W.Play(); auto* Overlay=FP5eTestAccess::Overlay(Part);
	if (!TestNotNull(TEXT("ISDK overlay"),Overlay)) { return false; }
	auto Check=[this,Overlay](FLinearColor Expected) { FLinearColor Color; Overlay->GetVectorParameterValue(FMaterialParameterInfo(TEXT("ColorParam")),Color); TestTrue(TEXT("State color"),Color.Equals(Expected)); };
	FP5eTestAccess::Pointer(Part,Grabber,EIsdkPointerEventType::Hover); Part->Tick(0.016f); Check(Part->HoverColor);
	FP5eTestAccess::Select(Part); Part->Tick(0.016f); Check(Part->GrabColor); FP5eTestAccess::Release(Part); P5ePush(Part); Check(Part->PushingColor);
	Part->Hands[0].bTracked=false; FP5eTestAccess::Pointer(Part,Grabber,EIsdkPointerEventType::Unhover); Part->Tick(0.016f); Check(FLinearColor::Transparent); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5eCar,"vBoR.P5e.E17CarPlacementSignAndClearance",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5eCar::RunTest(const FString&)
{
	FP5eWorld W; const FTransform CarWorld(FRotator(12,37,5),FVector(123,-256,42)); auto* Car=W.World->SpawnActor<AP3VehicleMock>(AP3VehicleMock::StaticClass(),CarWorld); W.Play(); auto* Part=Car->RearviewMirror.Get();
	if (!TestNotNull(TEXT("Mirror exists"),Part)) { return false; }
	TestTrue(TEXT("Car origin"),Part->GetActorTransform().Equals(CarWorld,0.001)); TestTrue(TEXT("Pivot authored"),Part->Pivot->GetRelativeLocation().Equals(Car->MirrorPivotCm));
	TestTrue(TEXT("Authored world mesh"),Part->PartMesh->GetComponentTransform().Equals(FTransform(FQuat::Identity,Car->MirrorCenterCm,Car->MirrorSizeCm/100)*CarWorld,0.001));
	const FTransform Support=Car->MirrorSupport->GetComponentTransform();
	auto Normal=[Part,CarWorld]() { return CarWorld.InverseTransformVectorNoScale(Part->PitchAxis->GetForwardVector()*-1); };
	FP5eTestAccess::Motion(Part).YawDeg=10; FP5eTestAccess::Apply(Part); TestTrue(TEXT("Yaw plus toward driver -Y"),Normal().Y<0 && FMath::IsNearlyZero(Normal().Z,0.001));
	FP5eTestAccess::Motion(Part).YawDeg=0; FP5eTestAccess::Motion(Part).PitchDeg=8; FP5eTestAccess::Apply(Part); TestTrue(TEXT("Pitch plus down -Z"),Normal().Z<0 && FMath::IsNearlyZero(Normal().Y,0.001));
	for (int32 Yaw : {-1,1})
	{
		for (int32 Pitch : {-1,1})
		{
			FP5eTestAccess::Motion(Part).YawDeg=Yaw*Part->YawLimitDeg; FP5eTestAccess::Motion(Part).PitchDeg=Pitch*Part->PitchLimitDeg; FP5eTestAccess::Apply(Part);
			const FBox Bounds=Part->PartMesh->GetStaticMesh()->GetBoundingBox().TransformBy(Part->PartMesh->GetComponentTransform().GetRelativeTransform(CarWorld));
			for (int32 I : {1,11,12})
			{ auto* Plate=FP5eTestAccess::Plates(Car)[I].Get(); TestFalse(TEXT("Clears roof dash and steering plate"),Bounds.Intersect(Plate->GetStaticMesh()->GetBoundingBox().TransformBy(Plate->GetRelativeTransform()))); }
			auto* Wheel=Car->SteeringWheel->WheelMesh.Get(); TestFalse(TEXT("Clears wheel"),Bounds.Intersect(Wheel->GetStaticMesh()->GetBoundingBox().TransformBy(Wheel->GetComponentTransform().GetRelativeTransform(CarWorld))));
			TestTrue(TEXT("Support stays fixed"),Car->MirrorSupport->GetComponentTransform().Equals(Support,0.001));
		}
	} return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5eCleanup,"vBoR.P5e.E18CarCleanup",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5eCleanup::RunTest(const FString&)
{
	FP5eWorld W; auto* Car=W.World->SpawnActor<AP3VehicleMock>(); W.Play(); auto* Part=Car->RearviewMirror.Get(); auto* Support=Car->MirrorSupport.Get();
	if (!TestNotNull(TEXT("Mirror exists"),Part)) { return false; } Car->Destroy(); TestTrue(TEXT("Mirror destroyed"),Part->IsActorBeingDestroyed()); TestFalse(TEXT("Support unregistered"),Support->IsRegistered()); return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5eSplitPoints,"vBoR.P5e.E20SplitPointsStayStill",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5eSplitPoints::RunTest(const FString&)
{
	for (int32 HandIndex : {0,1})
	{
		FVBoRTwoAxisMotion Motion; FVBoRDoorHand Hands[2];
		P5ePoint(Hands[HandIndex],FVector(-5,10,2.5)); Hands[HandIndex].Points[2]=FVector(-5,10,-2.5); Motion.AdvancePush(Hands);
		Hands[HandIndex].Points[1].X=-3.5; Hands[HandIndex].Points[2].X=-3.5; Motion.AdvancePush(Hands);
		TestEqual(TEXT("Split points start one push"),Motion.PushHand,HandIndex);
		TestTrue(TEXT("First push changes yaw only"),Motion.YawDeg < -1 && Motion.PitchDeg == 0);
		const double Yaw=Motion.YawDeg, Pitch=Motion.PitchDeg;
		for (int32 Frame=0; Frame<8; ++Frame)
		{
			Motion.AdvancePush(Hands);
			TestTrue(TEXT("Split points preserve yaw and pitch"),FMath::IsNearlyEqual(Motion.YawDeg,Yaw,0.001) && FMath::IsNearlyEqual(Motion.PitchDeg,Pitch,0.001));
		}
	} return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5eOpenHandPoints,"vBoR.P5e.E20OpenHandStaysStill",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5eOpenHandPoints::RunTest(const FString&)
{
	FVBoRTwoAxisMotion Motion; FVBoRDoorHand Hands[2]; Hands[0].bTracked=true;
	const FVector Points[] = {FVector(-5,7,0),FVector(-5,-1.2,2.6),FVector(-5,-2.2,1.5),FVector(-5,-2.2,-1.5),FVector(-5,-1.2,-2.6)};
	for (int32 PointIndex=0; PointIndex<5; ++PointIndex) { Hands[0].Points[PointIndex]=Points[PointIndex]; } Motion.AdvancePush(Hands);
	for (FVector& Point : Hands[0].Points) { Point.X=-3.5; } Motion.AdvancePush(Hands);
	TestEqual(TEXT("Open hand starts push"),Motion.PushHand,0); TestTrue(TEXT("Open hand turns mirror"),FMath::Abs(Motion.YawDeg)>1);
	const double Yaw=Motion.YawDeg, Pitch=Motion.PitchDeg;
	for (int32 Frame=0; Frame<8; ++Frame)
	{
		Motion.AdvancePush(Hands);
		TestTrue(TEXT("Stationary open hand preserves both angles"),FMath::IsNearlyEqual(Motion.YawDeg,Yaw,0.001) && FMath::IsNearlyEqual(Motion.PitchDeg,Pitch,0.001));
	} return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5ePushPointRetention,"vBoR.P5e.E20OtherPointCannotSteal",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5ePushPointRetention::RunTest(const FString&)
{
	FVBoRTwoAxisMotion Motion; FVBoRDoorHand Hands[2];
	P5ePoint(Hands[0],FVector(-5,10,0)); Hands[0].Points[2]=FVector(-5,-10,0); Motion.AdvancePush(Hands);
	Hands[0].Points[1].X=-3.5; Motion.AdvancePush(Hands);
	const double Yaw=Motion.YawDeg, Pitch=Motion.PitchDeg;
	Hands[0].Points[2]=Motion.Rotation().RotateVector(FVector(-2,-10,0));
	for (int32 Frame=0; Frame<8; ++Frame)
	{
		Motion.AdvancePush(Hands);
		TestTrue(TEXT("Deeper opposite point cannot change angles"),FMath::IsNearlyEqual(Motion.YawDeg,Yaw,0.001) && FMath::IsNearlyEqual(Motion.PitchDeg,Pitch,0.001));
	}
	Hands[0].Points[1]=Motion.Rotation().RotateVector(FVector(-8,10,0));
	for (int32 Frame=0; Frame<8; ++Frame)
	{
		Motion.AdvancePush(Hands); TestEqual(TEXT("Remaining point cannot restart push"),Motion.PushHand,INDEX_NONE);
		TestTrue(TEXT("Withdrawal preserves angles"),FMath::IsNearlyEqual(Motion.YawDeg,Yaw,0.001) && FMath::IsNearlyEqual(Motion.PitchDeg,Pitch,0.001));
	} return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5ePushPointLifecycle,"vBoR.P5e.E20PushPointLifecycle",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5ePushPointLifecycle::RunTest(const FString&)
{
	for (int32 EndMode : {0,1,2,3,4,5})
	{
		FVBoRTwoAxisMotion Motion; FVBoRDoorHand Hands[2];
		P5ePoint(Hands[0],FVector(-5,10,0)); Hands[0].Points[2]=FVector(-5,-10,0); Motion.AdvancePush(Hands);
		Hands[0].Points[1].X=-3.5; Hands[0].Points[2].X=-3.8; Motion.AdvancePush(Hands);
		TestEqual(TEXT("Deepest initial point selected"),Motion.PushPoint,1);
		const FQuat Before=Motion.Rotation();
		if (EndMode==0) { Motion.ClearPush(); }
		else if (EndMode==1) { Hands[0].bTracked=false; Motion.AdvancePush(Hands); }
		else if (EndMode==2) { Motion.BeginGrab(0,FVector(-4,0,0)); }
		else
		{
			const FVector InvalidPoint=EndMode==3?FVector(-3.5,20,0):(EndMode==4?FVector(0,10,0):FVector(-3.5,0,0));
			Hands[0].Points[1]=Before.RotateVector(InvalidPoint); Motion.AdvancePush(Hands);
		}
		TestEqual(TEXT("End clears hand"),Motion.PushHand,INDEX_NONE); TestEqual(TEXT("End clears point"),Motion.PushPoint,INDEX_NONE);
		TestTrue(TEXT("End never switches to remaining point"),Motion.Rotation().Equals(Before,0.001));
		if (EndMode==2) { Motion.ReleaseGrab(); }
		P5ePoint(Hands[0],Before.RotateVector(FVector(-5,10,0))); Hands[0].Points[2]=Before.RotateVector(FVector(-5,-10,0)); Motion.AdvancePush(Hands);
		Hands[0].Points[2]=Before.RotateVector(FVector(-3.5,-10,0)); Motion.AdvancePush(Hands);
		TestEqual(TEXT("Fresh crossing can select a different point"),Motion.PushPoint,2);
	} return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5lGrab,"vBoR.P5l.E1E2CenterFollowsHand",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5lGrab::RunTest(const FString&)
{
	for (bool Vertical : {false,true})
	{
		for (int32 Sign : {-1,1})
		{
			FP5eWorld W; auto* Part=W.Part(); Part->SetActorRotation(FRotator(0,37,0)); W.Play();
			Part->Wrist.SetLocation(FVector(-4,0,0)); FP5eTestAccess::Select(Part);
			Part->Wrist.AddToTranslation(Vertical ? FVector(0,0,Sign) : FVector(0,Sign,0)); Part->Tick(0.016f);
			TestTrue(TEXT("1 cm gives damped 5.61 deg"),FMath::IsNearlyEqual(static_cast<double>(Vertical?Part->GetPitchDeg():Part->GetYawDeg()),-Sign*5.614497,0.05));
			TestTrue(TEXT("Other axis stays zero"),FMath::IsNearlyZero(Vertical?Part->GetYawDeg():Part->GetPitchDeg(),0.001f));
			const FVector Direction=Vertical ? Part->GetActorUpVector() : Part->GetActorRightVector();
			TestTrue(TEXT("Actual normal follows world hand direction"),FVector::DotProduct(-Part->PitchAxis->GetForwardVector(),Direction)*Sign>0);
		}
	} return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5lSDK,"vBoR.P5l.E7SDKPalmPinchPosition",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5lSDK::RunTest(const FString&)
{
	for (bool Pinch : {false,true})
	{
		FP5eWorld W; auto* Part=W.Part(); Part->Wrist.SetLocation(FVector(-4,0,0));
		auto* Owner=W.World->SpawnActor<AActor>(); auto* Grabber=NewObject<UIsdkGrabberComponent>(Owner); Owner->SetRootComponent(Grabber);
		Grabber->InteractorTags.AddTag(IsdkGameplayTags::TAG_Isdk_Type_Device_Left); Grabber->SetWorldLocation(FVector(1000));
		Grabber->SetGrabDetectionTypeAllowed(EIsdkGrabDetectorType::DistanceGrab,false); Grabber->SetGrabDetectionTypeAllowed(EIsdkGrabDetectorType::RayGrab,false);
		Grabber->SetGrabInputMethodAllowed(EIsdkGrabInputMethod::Palm,true); Grabber->SetGrabInputMethodAllowed(EIsdkGrabInputMethod::Pinch,true);
		Grabber->RegisterComponent(); W.Play(); Grabber->Activate(true);
		Grabber->SetWorldLocation(Part->GrabBox->GetComponentLocation()); Grabber->TickComponent(0.016f,LEVELTICK_All,nullptr);
		if (Pinch) { Grabber->PinchGrab(); } else { Grabber->PalmGrab(); }
		TestTrue(TEXT("Actual SDK selection"),Grabber->GetGrabbedComponent()==Part->Grabbable);
		TestEqual(TEXT("Actual SDK method captured"),FP5eTestAccess::Pinch(Part),Pinch);
		Part->Wrist.AddToTranslation(FVector(0,0,1)); Part->Tick(0.016f);
		TestTrue(TEXT("Both SDK methods follow position"),FMath::IsNearlyEqual(Part->GetPitchDeg(),-5.614497f,0.05f));
		TestTrue(TEXT("SDK normal looks up"),FVector::DotProduct(-Part->PitchAxis->GetForwardVector(),Part->GetActorUpVector())>0);
		if (Pinch) { Grabber->PinchRelease(); } else { Grabber->PalmRelease(); }
	} return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5lTracking,"vBoR.P5l.E8RecoveryCentimeters",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5lTracking::RunTest(const FString&)
{
	FP5eWorld W; auto* Part=W.Part(); W.Play(); Part->Wrist.SetLocation(FVector(-4,0,0)); FP5eTestAccess::Select(Part);
	Part->bTracked=false; Part->Tick(0.016f); Part->Wrist.AddToTranslation(FVector(0,0,3)); Part->Tick(0.016f);
	TestEqual(TEXT("Lost tracking stays still"),Part->GetPitchDeg(),0.f); Part->bTracked=true; Part->Tick(0.016f);
	TestEqual(TEXT("Recovery stays still"),Part->GetPitchDeg(),0.f); TestEqual(TEXT("3 cm discarded"),FP5eTestAccess::Motion(Part).RecoveryDiscardCm,3.0);
	TestTrue(TEXT("Debug uses cm"),Part->GetDebugText().Contains(TEXT("discarded=3.000 cm")));
	Part->Wrist.AddToTranslation(FVector(0,0,1)); Part->Tick(0.016f);
	TestTrue(TEXT("Only new 1 cm used, previous lever 5 cm"),FMath::IsNearlyEqual(Part->GetPitchDeg(),-4.065051f,0.001f)); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5lRearPush,"vBoR.P5l.E9RearEdgeStopWithdraw",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5lRearPush::RunTest(const FString&)
{
	for (bool Vertical : {false,true})
	{
		FP5eWorld W; auto* Part=W.Part(); W.Play(); const FVector Edge(0,Vertical?0:10,Vertical?3.5:0);
		P5ePoint(Part->Hands[0],Edge+FVector(-1,0,0)); Part->Tick(0.016f);
		P5ePoint(Part->Hands[0],Edge+FVector(-2.5,0,0)); Part->Tick(0.016f);
		const FQuat Before=FP5eTestAccess::Motion(Part).Rotation();
		TestTrue(TEXT("Rear center stays still, side edge moves toward driver"),Vertical ? Before.Equals(FQuat::Identity,0.001) && FMath::IsNearlyEqual(Before.RotateVector(Edge+FVector(-2,0,0)).X,-2.0,0.001) : Before.RotateVector(Edge+FVector(-2,0,0)).X<-2);
		TestTrue(TEXT("Rear center finger passes through, side finger stays on plane"),FMath::IsNearlyEqual(Before.UnrotateVector(Part->Hands[0].Points[1]).X,Vertical?-2.5:-2.0,0.001));
		for (int32 I=0; I<5; ++I) { Part->Tick(0.016f); }
		TestTrue(TEXT("Rear stopped finger stays still"),FP5eTestAccess::Motion(Part).Rotation().Equals(Before,0.001));
		P5ePoint(Part->Hands[0],Before.RotateVector(Edge+FVector(1,0,0))); Part->Tick(0.016f);
		TestTrue(TEXT("Rear withdrawal never pulls"),FP5eTestAccess::Motion(Part).Rotation().Equals(Before,0.001));
		TestEqual(TEXT("Rear push ended"),FP5eTestAccess::Motion(Part).PushHand,INDEX_NONE);
	} return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5lRejectRear,"vBoR.P5l.E10RejectRearCrossings",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5lRejectRear::RunTest(const FString&)
{
	const FVector Before[] = {FVector(-1,0,0),FVector(-1,20,0),FVector(-1,10,0),FVector(-2.5,20,0),FVector(-2,10,0)};
	const FVector After[] = {FVector(-2.5,0,0),FVector(-2.5,20,0),FVector(-6,10,0),FVector(-2.5,10,0),FVector(-5,10,0)};
	for (int32 I=0; I<UE_ARRAY_COUNT(Before); ++I)
	{
		FVBoRTwoAxisMotion Motion; FVBoRDoorHand Hands[2]; P5ePoint(Hands[0],Before[I]); Motion.AdvancePush(Hands);
		P5ePoint(Hands[0],After[I]); Motion.AdvancePush(Hands);
		TestEqual(TEXT("Invalid rear yaw"),Motion.YawDeg,0.0); TestEqual(TEXT("Invalid rear pitch"),Motion.PitchDeg,0.0);
		TestEqual(TEXT("Invalid rear no push"),Motion.PushHand,INDEX_NONE);
	} return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5lFaceOwner,"vBoR.P5l.E11FirstHandPointAndFace",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5lFaceOwner::RunTest(const FString&)
{
	for (bool RearFirst : {false,true})
	{
		FVBoRTwoAxisMotion Motion; FVBoRDoorHand Hands[2];
		const double Outside=RearFirst?-1:-5, Inside=RearFirst?-2.5:-3.5;
		P5ePoint(Hands[0],FVector(Outside,10,0)); Motion.AdvancePush(Hands);
		Hands[0].Points[1].X=Inside; Motion.AdvancePush(Hands); const FQuat Rotation=Motion.Rotation();
		TestEqual(TEXT("First hand starts"),Motion.PushHand,0); TestEqual(TEXT("First face retained"),Motion.PushFace,RearFirst?1:0);
		Hands[0].Points[2]=Rotation.RotateVector(FVector(RearFirst?-5:-1,-10,0));
		P5ePoint(Hands[1],Hands[0].Points[2]); Motion.AdvancePush(Hands);
		Hands[0].Points[2]=Rotation.RotateVector(FVector(RearFirst?-3.5:-2.5,-10,0)); Hands[1].Points[1]=Hands[0].Points[2];
		Motion.AdvancePush(Hands);
		TestEqual(TEXT("Other hand cannot steal"),Motion.PushHand,0); TestEqual(TEXT("Other point cannot steal"),Motion.PushPoint,1);
		TestEqual(TEXT("Other face cannot steal"),Motion.PushFace,RearFirst?1:0);
		TestTrue(TEXT("Other face leaves rotation unchanged"),Motion.Rotation().Equals(Rotation,0.001));
	}
	FVBoRTwoAxisMotion Simultaneous; FVBoRDoorHand Hands[2];
	P5ePoint(Hands[0],FVector(-5,10,0)); P5ePoint(Hands[1],FVector(-1,-10,0)); Simultaneous.AdvancePush(Hands);
	Hands[0].Points[1].X=-3.5; Hands[1].Points[1].X=-2.5; Simultaneous.AdvancePush(Hands);
	TestEqual(TEXT("Simultaneous keeps one hand"),Simultaneous.PushHand,0); TestEqual(TEXT("Simultaneous keeps one face"),Simultaneous.PushFace,0); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5lRearTracking,"vBoR.P5l.E12RearTrackingAndGrabExclusion",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5lRearTracking::RunTest(const FString&)
{
	FVBoRTwoAxisMotion Motion; FVBoRDoorHand Hands[2]; P5ePoint(Hands[0],FVector(-1,10,0)); Motion.AdvancePush(Hands);
	Hands[0].Points[1].X=-2.5; Motion.AdvancePush(Hands); const FQuat Before=Motion.Rotation();
	Hands[0].bTracked=false; Motion.AdvancePush(Hands); TestEqual(TEXT("Loss clears face"),Motion.PushFace,INDEX_NONE);
	P5ePoint(Hands[0],Before.RotateVector(FVector(-2.5,10,0))); Motion.AdvancePush(Hands);
	TestEqual(TEXT("Recovery no push"),Motion.PushHand,INDEX_NONE); TestTrue(TEXT("Recovery no jump"),Motion.Rotation().Equals(Before,0.001));
	Motion.BeginGrab(0,FVector(-4,0,0)); P5ePoint(Hands[1],Before.RotateVector(FVector(-1,10,0))); Motion.AdvancePush(Hands);
	Hands[1].Points[1]=Before.RotateVector(FVector(-2.5,10,0)); Motion.AdvancePush(Hands);
	TestEqual(TEXT("Grab excludes rear push"),Motion.PushHand,INDEX_NONE); TestTrue(TEXT("Grab excludes rear rotation"),Motion.Rotation().Equals(Before,0.001)); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5lSettings,"vBoR.P5l.E13PositiveFiniteSettings",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5lSettings::RunTest(const FString&)
{
	for (bool Jump : {false,true})
	{
		for (float Value : {0.f,-1.f,std::numeric_limits<float>::quiet_NaN()})
		{
			FP5eWorld W; auto* Part=W.Part(); if (Jump) { Part->MaxGrabJumpCm=Value; } else { Part->MinLeverCm=Value; }
			FP5eWarnings Warnings;
			AddExpectedMessagePlain(TEXT("Missing mesh or invalid two-axis settings; grabbing and pushing disabled"),ELogVerbosity::Warning);
			W.Play(); Part->Tick(0.016f); Part->Tick(0.016f); GLog->FlushThreadedLogs(); TestEqual(TEXT("One runtime warning"),Warnings.Count,1);
			TestFalse(TEXT("Invalid Palm disabled"),Part->Grabbable->IsGrabInputMethodAllowed(EIsdkGrabInputMethod::Palm));
			TestFalse(TEXT("Invalid Pinch disabled"),Part->Grabbable->IsGrabInputMethodAllowed(EIsdkGrabInputMethod::Pinch));
			TestFalse(TEXT("Injected grab rejected"),FP5eTestAccess::Select(Part)); P5ePush(Part);
			TestEqual(TEXT("Invalid push disabled"),FP5eTestAccess::Motion(Part).PushHand,INDEX_NONE);
		}
	} return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5lMaterial,"vBoR.P5l.E14MaterialSurvivesBeginPlay",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5lMaterial::RunTest(const FString&)
{
	FP5eWorld W; auto* Part=W.Part(); auto* Grabber=P5ePointer(Part);
	auto* Base=LoadObject<UMaterialInterface>(nullptr,TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	auto* Material=UMaterialInstanceDynamic::Create(Base,Part); Part->PartMesh->SetMaterial(0,Material); W.Play();
	TestTrue(TEXT("Slot zero survives BeginPlay"),Part->PartMesh->GetMaterial(0)==Material);
	FP5eTestAccess::Pointer(Part,Grabber,EIsdkPointerEventType::Hover); Part->Tick(0.016f);
	TestTrue(TEXT("Hover keeps slot zero"),Part->PartMesh->GetMaterial(0)==Material);
	TestTrue(TEXT("Hover only uses overlay"),Part->PartMesh->GetOverlayMaterial()==FP5eTestAccess::Overlay(Part)); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5lMockMaterial,"vBoR.P5l.E15MockMirrorColor",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5lMockMaterial::RunTest(const FString&)
{
	FP5eWorld W; auto* Car=W.World->SpawnActor<AP3VehicleMock>(); W.Play(); auto* Part=Car->RearviewMirror.Get();
	if (!TestNotNull(TEXT("Mock mirror"),Part)) { return false; }
	auto* Material=Cast<UMaterialInstanceDynamic>(Part->PartMesh->GetMaterial(0));
	if (!TestNotNull(TEXT("Mock creates approximate material"),Material)) { return false; }
	FLinearColor Color; Material->GetVectorParameterValue(FMaterialParameterInfo(TEXT("ColorParam")),Color);
	TestTrue(TEXT("Approximate color retained"),Color.Equals(FLinearColor(0.15,0.3,0.4,1)));
	TestTrue(TEXT("Approximate no reflection label retained"),Part->GetDebugText().Contains(TEXT("Approximate geometry")) && Part->GetDebugText().Contains(TEXT("no reflection"))); return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5nMirrorPush,"vBoR.P5n.E7E8MirrorFaces",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5nMirrorPush::RunTest(const FString&)
{
	for (bool Rear : {false,true})
	{
		for (double Y : {-10.0,0.0,10.0})
		{
			for (double Z : {-3.5,0.0,3.5})
			{
				if (Y==0 && Z==0) { continue; }
				FP5eWorld W; auto* Part=W.Part(); W.Play();
				const double Face=Rear?-2:-4, Direction=Rear?-1:1;
				P5ePoint(Part->Hands[0],FVector(Face-Direction,Y,Z)); Part->Tick(0.016f);
				const FVector Hand(Face+Direction*0.5,Y,Z);
				P5ePoint(Part->Hands[0],Hand); Part->Tick(0.016f);
				const auto& M=FP5eTestAccess::Motion(Part);
				TestEqual(TEXT("E8 both faces retain pushing hand even in center band"),M.PushHand,0);
				TestEqual(TEXT("E7/E8 push never changes pitch"),Part->GetPitchDeg(),0.f);
				if (Y==0) { TestEqual(TEXT("E8 center has no yaw"),Part->GetYawDeg(),0.f); }
				else
				{
					TestTrue(TEXT("E7/E8 yaw follows pushed side"),Part->GetYawDeg()*Y*Direction<0);
					TestTrue(TEXT("E7/E8 hand lies on retained face"),FMath::IsNearlyEqual(M.Rotation().UnrotateVector(Hand).X,Face,0.001));
					TestTrue(TEXT("E7/E8 touched edge retreats in push direction"),Direction*(M.Rotation().RotateVector(FVector(Face,Y,Z)).X-Face)>0);
				}
			}
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5nMirrorBand,"vBoR.P5n.E8bMirrorBandBoundary",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5nMirrorBand::RunTest(const FString&)
{
	for (bool Rear : {false,true})
	{
		for (double Y : {-4.1,-3.9,3.9,4.1})
		{
			FP5eWorld W; auto* Part=W.Part(); W.Play();
			TestEqual(TEXT("Default band copied to motion"),FP5eTestAccess::Motion(Part).PushMinLeverCm,4.0);
			const double Face=Rear?-2:-4, Direction=Rear?-1:1;
			P5ePoint(Part->Hands[0],FVector(Face-Direction,Y,0)); Part->Tick(0.016f);
			const FVector Hand(Face+Direction*0.5,Y,0);
			P5ePoint(Part->Hands[0],Hand); Part->Tick(0.016f);
			auto& M=FP5eTestAccess::Motion(Part);
			TestEqual(TEXT("E8b band retains ownership"),M.PushHand,0);
			TestEqual(TEXT("E8b no pitch"),M.PitchDeg,0.0);
			if (FMath::Abs(Y)<4) { TestEqual(TEXT("E8b inside band stays still"),M.YawDeg,0.0); }
			else
			{
				TestTrue(TEXT("E8b outside band turns about seven deg without hitting limit"),FMath::Abs(M.YawDeg)>6 && FMath::Abs(M.YawDeg)<9);
				TestTrue(TEXT("E8b contact lies on face"),FMath::IsNearlyEqual(M.Rotation().UnrotateVector(Hand).X,Face,0.001));
			}
		}
	}
	// 띠 안에서 시작해도 같은 손·점·면으로 띠 밖에서 계속 밂 (P5n D3)
	FP5eWorld W; auto* Part=W.Part(); W.Play();
	P5ePoint(Part->Hands[0],FVector(-1,3.9,0)); Part->Tick(0.016f);
	P5ePoint(Part->Hands[0],FVector(-2.5,3.9,0)); Part->Tick(0.016f);
	P5ePoint(Part->Hands[0],FVector(-2.5,4.1,0)); Part->Tick(0.016f);
	const auto& M=FP5eTestAccess::Motion(Part);
	TestTrue(TEXT("E8b same push resumes outside band"),M.PushHand==0 && M.PushPoint==1 && M.PushFace==1 && M.YawDeg>6);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5nMirrorPitched,"vBoR.P5n.E8cE9PitchedMirror",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5nMirrorPitched::RunTest(const FString&)
{
	for (bool Pinch : {false,true})
	{
		for (bool Rear : {false,true})
		{
			for (double Y : {-10.0,10.0})
			{
				FP5eWorld W; auto* Part=W.Part();
				auto* Owner=W.World->SpawnActor<AActor>(); auto* G=NewObject<UIsdkGrabberComponent>(Owner); Owner->SetRootComponent(G);
				G->InteractorTags.AddTag(IsdkGameplayTags::TAG_Isdk_Type_Device_Left); G->SetWorldLocation(FVector(1000));
				G->SetGrabDetectionTypeAllowed(EIsdkGrabDetectorType::DistanceGrab,false); G->SetGrabDetectionTypeAllowed(EIsdkGrabDetectorType::RayGrab,false);
				G->SetGrabInputMethodAllowed(EIsdkGrabInputMethod::Palm,true); G->SetGrabInputMethodAllowed(EIsdkGrabInputMethod::Pinch,true);
				G->RegisterComponent(); W.Play(); G->Activate(true);
				G->SetWorldLocation(Part->GrabBox->GetComponentLocation()); G->TickComponent(0.016f,LEVELTICK_All,nullptr);
				if (Pinch) { G->PinchGrab(); } else { G->PalmGrab(); }
				TestTrue(TEXT("E9 actual SDK selection"),G->GetGrabbedComponent()==Part->Grabbable);
				TestEqual(TEXT("E9 SDK method retained"),FP5eTestAccess::Pinch(Part),Pinch);
				P5eMove(Part,0,10);
				TestTrue(TEXT("E9 palm and pinch still follow grab point pitch"),FMath::IsNearlyEqual(Part->GetPitchDeg(),10.f,0.001f));
				if (Pinch) { G->PinchRelease(); } else { G->PalmRelease(); }
				const FQuat Before=FP5eTestAccess::Motion(Part).Rotation();
				const double Face=Rear?-2:-4, Direction=Rear?-1:1;
				P5ePoint(Part->Hands[0],Before.RotateVector(FVector(Face-Direction,Y,0))); Part->Tick(0.016f);
				const FVector Hand=Before.RotateVector(FVector(Face+Direction*0.5,Y,0));
				P5ePoint(Part->Hands[0],Hand); Part->Tick(0.016f);
				const auto& M=FP5eTestAccess::Motion(Part);
				TestTrue(TEXT("E8c pitch stays at grabbed ten deg"),FMath::IsNearlyEqual(M.PitchDeg,10.0,0.001));
				TestTrue(TEXT("E8c only yaw changes with correct sign"),M.YawDeg*Y*Direction<0);
				TestTrue(TEXT("E8c hand remains on tilted face"),FMath::IsNearlyEqual(M.Rotation().UnrotateVector(Hand).X,Face,0.001));
			}
		}
	}
	return true;
}
#endif
