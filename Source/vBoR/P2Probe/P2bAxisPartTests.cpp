#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "../Interaction/VBoRAxisPart.h"
#include "../Interaction/VBoRHingedDoor.h"
#include "../Interaction/VBoRScenarioRunner.h"
#include "../P3Probe/P3VehicleMock.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/WorldSettings.h"
#include "Interaction/IsdkGrabberComponent.h"
#include "Interaction/IsdkGrabbableComponent.h"
#include "Interaction/GrabDetectors/IsdkHandGrabDetector.h"
#include "IsdkFunctionLibrary.h"
#include "IsdkRuntimeSettings.h"
#include "P2bTestFixture.h"
#include "P2DoorBench.h"
#include "Misc/OutputDevice.h"
#include "Misc/OutputDeviceRedirector.h"

struct FP2bTestAccess
{
	static void Value(AVBoRAxisPart* Part, double Value)
	{
		Part->Value = Value; Part->AxisPivot->SetRelativeTransform(Part->ValueTransform() * Part->RestPivot); Part->EvaluateState();
	}
	static int32 Opens(AVBoRAxisPart* Part) { return Part->Opens; }
	static int32 Closes(AVBoRAxisPart* Part) { return Part->Closes; }
	static int32 Pauses(AVBoRAxisPart* Part) { return Part->Pauses; }
	static double Discard(AVBoRAxisPart* Part) { return Part->RecoveryDiscard; }
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
	static bool Transfer(AVBoRHingedDoor* Door, AVBoRAxisPart* Part, const FVBoRDoorHand& Hand)
	{
		const bool bResult = Door->BeginAxisGrab(Part, Hand);
		if (bResult) { Part->bDoorHold = true; Part->GrabComponent->ForceCancel(); }
		return bResult;
	}
	static FVBoRDoorMotion& Motion(AVBoRHingedDoor* Door) { return Door->Motion; }
	static void Apply(AVBoRHingedDoor* Door, double Angle, int32 Unlocks, int32 Latches) { Door->ApplyState(Angle, Unlocks, Latches); }
	static bool NeedsOpen(AVBoRAxisPart* Part) { return Part->bNeedsPalmOpen[0]; }
	static void PalmOpen(AVBoRHingedDoor* Door, AVBoRAxisPart* Part) { Door->bNeedsPalmOpen[0] = Part->bNeedsPalmOpen[0] = false; }
	static void Pointer(AVBoRHingedDoor* Door, const FIsdkInteractionPointerEvent& Event) { Door->HandleOutsidePointer(Event); }
};

namespace
{
	struct FP2bWorld
	{
		UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
		void Play() { World->InitializeActorsForPlay(FURL()); World->GetWorldSettings()->NotifyBeginPlay(); }
		~FP2bWorld() { World->EndPlay(EEndPlayReason::Quit); World->DestroyWorld(false); }
	};
	FIsdkGrabPose P2bPose(FVector Position, int32 Id = 11) { return UIsdkFunctionLibrary::MakeGrabPoseStruct(Id, Position, FQuat::Identity); }
	FVBoRDoorHand P2bHand(double Angle = 0)
	{
		FVBoRDoorHand Hand; Hand.bTracked = true;
		for (FVector& Point : Hand.Points) { Point = FQuat(FVector::UpVector, FMath::DegreesToRadians(Angle)).RotateVector(FVector(100,9,60)); }
		return Hand;
	}
	struct FP2bDoorLogs : FOutputDevice
	{
		TArray<FString> Lines;
		FP2bDoorLogs() { GLog->FlushThreadedLogs(); GLog->AddOutputDevice(this); Lines.Reset(); }
		~FP2bDoorLogs() { GLog->RemoveOutputDevice(this); }
		virtual void Serialize(const TCHAR* Message, ELogVerbosity::Type Verbosity, const FName& Category) override
		{
			if (Category == TEXT("LogAxisDoor")) { Lines.Add(Message); }
		}
		int32 Count(const FString& Text)
		{
			GLog->FlushThreadedLogs();
			return Lines.FilterByPredicate([&](const FString& Line) { return Line.Contains(Text); }).Num();
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP2bSDKReleaseTest, "vBoR.P2b.Axis.SDKReleaseBeforeTransformer", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP2bSDKReleaseTest::RunTest(const FString& Parameters)
{
	FP2bWorld W;
	AP2bTrackedPart* Part = W.World->SpawnActor<AP2bTrackedPart>();
	Part->Release = EVBoRAxisRelease::Stay;
	AActor* Actor = W.World->SpawnActor<AActor>();
	UIsdkGrabberComponent* Grabber = NewObject<UIsdkGrabberComponent>(Actor);
	Actor->SetRootComponent(Grabber);
	Grabber->InteractorTags.AddTag(IsdkGameplayTags::TAG_Isdk_Type_Device_Left);
	Grabber->SetWorldLocation(FVector(300));
	Grabber->SetGrabDetectionTypeAllowed(EIsdkGrabDetectorType::DistanceGrab, false);
	Grabber->SetGrabDetectionTypeAllowed(EIsdkGrabDetectorType::RayGrab, false);
	Grabber->SetGrabInputMethodAllowed(EIsdkGrabInputMethod::Palm, true);
	Grabber->SetGrabInputMethodAllowed(EIsdkGrabInputMethod::Pinch, false);
	Grabber->RegisterComponent();
	W.Play(); Grabber->Activate(true);
	UP2bPointerObserver* Observer = NewObject<UP2bPointerObserver>(W.World);
	Observer->Grab = Part->GrabComponent;
	Part->Grabbable->GetInteractionPointerEventDelegate().AddDynamic(Observer, &UP2bPointerObserver::Pointer);
	Part->GrabComponent->GetCancelGrabEventDelegate()->AddDynamic(Observer, &UP2bPointerObserver::Cancel);
	Part->OnPartReleased.AddDynamic(Observer, &UP2bPointerObserver::Released);
	Grabber->SetWorldLocation(Part->GrabBox->GetComponentLocation());
	Grabber->TickComponent(0.016f, LEVELTICK_All, nullptr);
	Grabber->PalmGrab();
	TestTrue(TEXT("Real SDK grab owns part"), Grabber->GetGrabbedComponent() == Part->Grabbable && Part->GetHoldingHand() == 0);
	FP2bTestAccess::Value(Part, 18);
	Grabber->PalmRelease();
	TestEqual(TEXT("Our event executes before transformer removes pose"), Observer->UnselectPoses, 1);
	TestEqual(TEXT("Zero ForceCancel calls in pointer callbacks"), Part->GrabComponent->CancelCalls, 0);
	TestEqual(TEXT("No reentrant SDK cancel"), Observer->CancelEvents, 0);
	TestEqual(TEXT("One application release"), Observer->ReleaseEvents, 1);
	TestEqual(TEXT("Application holding hand cleared"), Part->GetHoldingHand(), INDEX_NONE);
	TestTrue(TEXT("SDK grabber released without crash"), !Grabber->IsGrabbing() && !Grabber->GetGrabbedComponent());
	TestEqual(TEXT("Transformer selection cleared by SDK release"), Part->GrabComponent->GetNumGrabbers(), 0);
	TestTrue(TEXT("Lid stays at release angle"), FMath::IsNearlyEqual(Part->GetOpenRatio(), 0.6f));

	Part->bTracked = false;
	Grabber->TickComponent(0.016f, LEVELTICK_All, nullptr); Grabber->PalmGrab();
	TestEqual(TEXT("Low Select rejected by application"), Part->GetHoldingHand(), INDEX_NONE);
	TestTrue(TEXT("Low Select remains in SDK until Tick"), Grabber->IsGrabbing());
	TestEqual(TEXT("Low Select also calls ForceCancel zero times in event"), Part->GrabComponent->CancelCalls, 0);
	Part->Tick(0.016f);
	TestEqual(TEXT("One deferred ForceCancel on Tick"), Part->GrabComponent->CancelCalls, 1);
	TestEqual(TEXT("One SDK cancellation"), Observer->CancelEvents, 1);
	TestTrue(TEXT("Low Select cleared from grabber and transformer"), !Grabber->IsGrabbing() && Part->GrabComponent->GetNumGrabbers() == 0);
	TestEqual(TEXT("Low rejection does not emit application release"), Observer->ReleaseEvents, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP2bDoorLogTest, "vBoR.P2b.Door.TransferEndReasons", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP2bDoorLogTest::RunTest(const FString& Parameters)
{
	FP2bWorld W; FP2bDoorLogs Logs;
	AP2bTrackedDoor* Door = W.World->SpawnActor<AP2bTrackedDoor>();
	AVBoRAxisPart* Part = W.World->SpawnActor<AVBoRAxisPart>();
	W.Play(); Door->SetAxisHandle(0, Part);
	Door->Hands[0] = P2bHand();
	FP2bTestAccess::Begin(Part, P2bPose(FVector(100,9,60))); FP2bTestAccess::Value(Part, 25);
	TestTrue(TEXT("Starts same-hand door hold"), FP2bTestAccess::Transfer(Door, Part, Door->Hands[0]));
	TestEqual(TEXT("Start logs handle, hand, prior lock, angle"), Logs.Count(TEXT("handle=outside hand=L locked=1 angle=0.00 deg")), 1);
	Door->Hands[0] = P2bHand(30); Door->Tick(0.1f);
	Door->Hands[0].bTracked = false; Door->Tick(0.2f); Door->Tick(0.2f);
	TestEqual(TEXT("Tracking loss logged once as pause"), Logs.Count(TEXT("axis hold pause")), 1);
	TestEqual(TEXT("Tracking loss does not end hold"), Logs.Count(TEXT("axis hold end")), 0);
	TestEqual(TEXT("Hand retained during pause"), Part->GetHoldingHand(), 0);
	Door->Hands[0] = P2bHand(30); Door->Tick(0.1f);
	TestEqual(TEXT("Recovery logged once"), Logs.Count(TEXT("axis hold resume")), 1);
	Door->bPalmClosed[0] = false; Door->Tick(0.1f);
	TestEqual(TEXT("Palm open logs reason and angle"), Logs.Count(TEXT("reason=palm open angle=18.00 deg held=")), 1);
	TestEqual(TEXT("Palm open releases transferred part"), Part->GetHoldingHand(), INDEX_NONE);
	Door->Tick(0.1f);
	TestEqual(TEXT("End logged only once"), Logs.Count(TEXT("axis hold end")), 1);
	FP2bTestAccess::PalmOpen(Door, Part); Door->bPalmClosed[0] = true;
	FP2bTestAccess::Begin(Part, P2bPose(FVector(100,9,60)));
	FP2bTestAccess::Value(Part, 25);
	TestTrue(TEXT("Unlocked door regrabs at threshold"), FP2bTestAccess::Transfer(Door, Part, Door->Hands[0]));
	Door->Hands[0] = P2bHand(12); Door->Tick(0.1f);
	TestTrue(TEXT("Closing latches door"), Door->GetMotion().bLocked);
	TestEqual(TEXT("Latch end reason, angle and duration logged"), Logs.Count(TEXT("reason=latch angle=0.00 deg held=")), 1);
	TestEqual(TEXT("Latch releases part"), Part->GetHoldingHand(), INDEX_NONE);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP2bBenchSizeTest, "vBoR.P2b.Door.BenchSizeBounds", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP2bBenchSizeTest::RunTest(const FString& Parameters)
{
	FP2bWorld W;
	for (const FVector& Size : {FVector(90,8,100), FVector(110,10,105)})
	{
		AP2DoorBench* Bench = W.World->SpawnActor<AP2DoorBench>(); Bench->DoorSizeCm = Size;
		Bench->PlaceFromHead(FTransform(FRotator(0,40,0), FVector(300,20,165)), 25);
		AVBoRHingedDoor* Door = Bench->Door;
		if (!TestNotNull(TEXT("Bench creates door"), Door)) { continue; }
		const FBox Bounds = Door->DoorMesh->GetStaticMesh()->GetBoundingBox().TransformBy(Door->DoorMesh->GetRelativeTransform());
		TestTrue(TEXT("Mesh bounds follow requested size"), Bounds.GetSize().Equals(Size, 0.001));
		TestTrue(TEXT("Push faces exactly match resized mesh"), Door->GetMotion().FaceMinCm.Equals(Bounds.Min) && Door->GetMotion().FaceMaxCm.Equals(Bounds.Max));
		TestTrue(TEXT("Outside handle follows resized trailing edge and face"), Door->OutsideHandle->GetRelativeLocation().Equals(FVector(Size.X - Bench->OutsideHandleInsetCm, Bounds.Max.Y + 4, Bench->OutsideHandleHeightCm - Bench->BottomHeightCm)));
		TestTrue(TEXT("Inside handle follows resized trailing edge and face"), Door->InsideHandle->GetRelativeLocation().Equals(FVector(Size.X - Bench->InsideHandleInsetCm, Bounds.Min.Y - 4, Bench->InsideHandleHeightCm - Bench->BottomHeightCm)));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP2bPivotTest, "vBoR.P2b.Axis.PivotEquivalence", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP2bPivotTest::RunTest(const FString& Parameters)
{
	FP2bWorld W;
	AVBoRAxisPart* Original = W.World->SpawnActor<AVBoRAxisPart>();
	Original->Axis = EAxis::Y;
	Original->MeshRestFromRoot = FTransform(FQuat::Identity,FVector(100,23,150),FVector(0.2));
	Original->AxisPivot->SetRelativeLocation(FVector(100,23,160)); Original->OnConstruction(Original->GetActorTransform());
	AVBoRAxisPart* Fixed = W.World->SpawnActor<AVBoRAxisPart>();
	Fixed->SetActorLocation(FVector(100,23,160)); Fixed->Axis = EAxis::Y;
	Fixed->MeshRestFromRoot = FTransform(FQuat::Identity,FVector(0,0,-10),FVector(0.2)); Fixed->OnConstruction(Fixed->GetActorTransform());
	TestTrue(TEXT("E1 E2 same mesh world bounds"), Original->PartMesh->GetComponentTransform().Equals(Fixed->PartMesh->GetComponentTransform(),0.001));
	const FVector Pivot = Original->AxisPivot->GetComponentLocation();
	FP2bTestAccess::Value(Original,30); FP2bTestAccess::Value(Fixed,30);
	TestTrue(TEXT("E1 E2 same 30 deg world result"), Original->PartMesh->GetComponentTransform().Equals(Fixed->PartMesh->GetComponentTransform(),0.001));
	TestTrue(TEXT("E1 axis point stays fixed"), Pivot.Equals(Original->AxisPivot->GetComponentLocation()));
	TestTrue(TEXT("E1 10 cm radius follows arc"), Original->PartMesh->GetComponentLocation().Equals(Pivot+FQuat(FVector::YAxisVector,FMath::DegreesToRadians(30.0)).RotateVector(FVector(0,0,-10)),0.001));
	Original->AxisPivot->SetRelativeTransform(FTransform(FRotator(20,35,10),FVector(10,20,30)));
	Original->OnConstruction(Original->GetActorTransform());
	TestTrue(TEXT("E3 construction keeps mesh root rest pose"), Original->PartMesh->GetComponentTransform().Equals(Original->MeshRestFromRoot,0.001));
	const FBox Bounds = Original->PartMesh->GetStaticMesh()->GetBoundingBox().TransformBy(Original->PartMesh->GetRelativeTransform());
	TestTrue(TEXT("Grab box contains transformed bounds plus 1 cm"), Original->GrabBox->GetRelativeLocation().Equals(Bounds.GetCenter()) && Original->GrabBox->GetUnscaledBoxExtent().Equals(Bounds.GetExtent()+FVector(1)));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP2bRangeTest, "vBoR.P2b.Axis.SDKRangesAndRotatedAxis", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP2bRangeTest::RunTest(const FString& Parameters)
{
	FP2bWorld W;
	for (EVBoRAxisMovement Kind : {EVBoRAxisMovement::Rotate, EVBoRAxisMovement::Translate})
	{
		for (bool Reverse : {false,true})
		{
			AVBoRAxisPart* Part = W.World->SpawnActor<AVBoRAxisPart>(); Part->Movement = Kind; Part->bReverse = Reverse; Part->Axis = EAxis::Z;
			Part->AxisPivot->SetRelativeTransform(FTransform(FRotator(15,55,20),FVector(100,23,160))); Part->OnConstruction(Part->GetActorTransform());
			const FTransform Rest = Part->AxisPivot->GetComponentTransform();
			FP2bTestAccess::Begin(Part,P2bPose(Rest.TransformPosition(FVector(10,0,0))));
			const double Sign = Reverse ? -1 : 1;
			const FVector Beyond = Kind == EVBoRAxisMovement::Rotate ? FQuat(FVector::ZAxisVector,FMath::DegreesToRadians(Sign*50)).RotateVector(FVector(10,0,0)) : FVector(10,0,Sign*8);
			FP2bTestAccess::Move(Part,P2bPose(Rest.TransformPosition(Beyond)));
			TestTrue(TEXT("E4 clamps SDK at 30 deg / 4 cm"), FMath::IsNearlyEqual(Part->GetOpenRatio(),1.f,0.001f));
			const FVector Behind = Kind == EVBoRAxisMovement::Rotate ? FQuat(FVector::ZAxisVector,FMath::DegreesToRadians(-Sign*20)).RotateVector(FVector(10,0,0)) : FVector(10,0,-Sign*8);
			FP2bTestAccess::Move(Part,P2bPose(Rest.TransformPosition(Behind)));
			TestTrue(TEXT("E4 stops at zero when pushed behind"), FMath::IsNearlyZero(Part->GetOpenRatio(),0.001f));
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP2bHysteresisTest, "vBoR.P2b.Axis.OpenCloseHysteresis", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP2bHysteresisTest::RunTest(const FString& Parameters)
{
	FP2bWorld W; AVBoRAxisPart* Part = W.World->SpawnActor<AVBoRAxisPart>();
	for (double Value : {24.5,22.5,25.5,23.0,25.0}) { FP2bTestAccess::Value(Part,Value); }
	TestEqual(TEXT("E5 one open around 80 percent"),FP2bTestAccess::Opens(Part),1);
	for (double Value : {1.0,2.0,0.5,2.0,0.0}) { FP2bTestAccess::Value(Part,Value); }
	TestEqual(TEXT("E5 one close around 5 percent"),FP2bTestAccess::Closes(Part),1);
	FP2bTestAccess::Value(Part,25); FP2bTestAccess::Value(Part,0);
	TestEqual(TEXT("Thresholds rearm only after opposite bands"),FP2bTestAccess::Opens(Part),2);
	TestEqual(TEXT("Close rearmed after 20 percent"),FP2bTestAccess::Closes(Part),2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP2bReleaseTest, "vBoR.P2b.Axis.ReturnStayRegrab", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP2bReleaseTest::RunTest(const FString& Parameters)
{
	FP2bWorld W;
	for (EVBoRAxisMovement Kind : {EVBoRAxisMovement::Rotate,EVBoRAxisMovement::Translate})
	{
		for (float Dt : {0.011f,0.033f,0.1f})
		{
			AVBoRAxisPart* Part = W.World->SpawnActor<AVBoRAxisPart>(); Part->Movement = Kind;
			FP2bTestAccess::Value(Part,Kind == EVBoRAxisMovement::Rotate ? 30 : 4);
			double Time = 0;
			while (Time < 0.1-1.e-7) { const float Step = static_cast<float>(FMath::Min(static_cast<double>(Dt),0.1-Time)); Part->Tick(Step); Time += Step; }
			TestTrue(TEXT("E6 same elapsed time returns same ratio"),FMath::IsNearlyEqual(Part->GetOpenRatio(),Kind == EVBoRAxisMovement::Rotate ? 0.4f : 0.5f,0.001f));
			const FIsdkGrabPose Pose = P2bPose(Part->AxisPivot->GetComponentLocation()+FVector(0,10,10));
			const FTransform Before = Part->AxisPivot->GetComponentTransform(); FP2bTestAccess::Begin(Part,Pose); FP2bTestAccess::Move(Part,Pose);
			TestTrue(TEXT("E6 regrab during return has no jump"),Before.Equals(Part->AxisPivot->GetComponentTransform(),0.001));
			FP2bTestAccess::Release(Part); Part->Tick(1); TestTrue(TEXT("E6 returns fully"),FMath::IsNearlyZero(Part->GetOpenRatio()));
		}
	}
	AVBoRAxisPart* Stay = W.World->SpawnActor<AVBoRAxisPart>(); Stay->Release = EVBoRAxisRelease::Stay;
	FP2bTestAccess::Value(Stay,18); Stay->Tick(1);
	TestTrue(TEXT("E7 released lid stays at 60 percent"),FMath::IsNearlyEqual(Stay->GetOpenRatio(),0.6f));
	const FIsdkGrabPose Pose = P2bPose(FVector(0,10,10)); FP2bTestAccess::Begin(Stay,Pose); FP2bTestAccess::Move(Stay,Pose);
	TestTrue(TEXT("E7 regrab starts at same ratio"),FMath::IsNearlyEqual(Stay->GetOpenRatio(),0.6f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP2bTrackingTest, "vBoR.P2b.Axis.TrackingPauseRebase", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP2bTrackingTest::RunTest(const FString& Parameters)
{
	FP2bWorld W;
	for (EVBoRAxisMovement Kind : {EVBoRAxisMovement::Rotate,EVBoRAxisMovement::Translate})
	{
		AVBoRAxisPart* Part = W.World->SpawnActor<AVBoRAxisPart>(); Part->Movement = Kind; Part->Axis = EAxis::Z;
		FP2bTestAccess::Value(Part,Kind == EVBoRAxisMovement::Rotate ? 15 : 2);
		FP2bTestAccess::Begin(Part,P2bPose(FVector(10,0,0)));
		const FIsdkGrabPose Lost = P2bPose(Kind == EVBoRAxisMovement::Rotate ? FVector(0,10,0) : FVector(10,0,8));
		const FTransform Before = Part->AxisPivot->GetComponentTransform();
		FP2bTestAccess::Move(Part,Lost,false); Part->Tick(1); FP2bTestAccess::Move(Part,Lost,true);
		TestTrue(TEXT("E8 loss and recovery frame preserve pose"),Before.Equals(Part->AxisPivot->GetComponentTransform(),0.001));
		TestEqual(TEXT("E8 one pause"),FP2bTestAccess::Pauses(Part),1);
		TestEqual(TEXT("E8 no open event"),FP2bTestAccess::Opens(Part),0); TestEqual(TEXT("E8 no close event"),FP2bTestAccess::Closes(Part),0);
		TestTrue(TEXT("E8 discarded movement measured in deg/cm"),FP2bTestAccess::Discard(Part)>0);
		const FVector Next = Kind == EVBoRAxisMovement::Rotate ? FQuat(FVector::ZAxisVector,FMath::DegreesToRadians(5.0)).RotateVector(Lost.Position()) : Lost.Position()+FVector(0,0,0.5);
		FP2bTestAccess::Move(Part,P2bPose(Next));
		TestTrue(TEXT("E8 tracked movement continues from recovered pose"),Part->GetOpenRatio()>0.5f && Part->GetOpenRatio()<0.8f);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP2bOverlapTest, "vBoR.P2b.Axis.RealPalmOverlapSingleHand", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP2bOverlapTest::RunTest(const FString& Parameters)
{
	FP2bWorld W; AVBoRAxisPart* Part = W.World->SpawnActor<AVBoRAxisPart>();
	Part->MeshRestFromRoot = FTransform(FQuat::Identity,FVector(100,23,150),FVector(0.18,0.04,0.04));
	Part->AxisPivot->SetRelativeLocation(FVector(100,23,152)); Part->OnConstruction(Part->GetActorTransform());
	UIsdkGrabberComponent* Grabbers[2];
	for (int32 H=0; H<2; ++H)
	{
		AActor* Actor = W.World->SpawnActor<AActor>(); Grabbers[H] = NewObject<UIsdkGrabberComponent>(Actor); Actor->SetRootComponent(Grabbers[H]);
		Grabbers[H]->SetWorldLocation(FVector(300)); Grabbers[H]->SetGrabDetectionTypeAllowed(EIsdkGrabDetectorType::DistanceGrab,false); Grabbers[H]->SetGrabDetectionTypeAllowed(EIsdkGrabDetectorType::RayGrab,false);
		Grabbers[H]->SetGrabInputMethodAllowed(EIsdkGrabInputMethod::Palm,true); Grabbers[H]->SetGrabInputMethodAllowed(EIsdkGrabInputMethod::Pinch,false); Grabbers[H]->RegisterComponent();
	}
	W.Play();
	for (UIsdkGrabberComponent* Grabber : Grabbers) { Grabber->Activate(true); }
	TestTrue(TEXT("Transform target is axis"),Part->GrabComponent->GetTransformTarget()==Part->AxisPivot);
	TestTrue(TEXT("Single first retained"),Part->GrabComponent->GetMultiGrabBehavior()==EIsdkMultiGrabBehavior::SingleGrabFirstRetained);
	for (int32 H=0; H<2; ++H)
	{
		Grabbers[H]->SetWorldLocation(Part->GrabBox->GetComponentLocation());
		Grabbers[H]->TickComponent(0.016f,LEVELTICK_All,nullptr);
		TestTrue(TEXT("E16 real palm overlap detects grabbable"),Grabbers[H]->GetHandGrabDetector()->GetHoveredGrabbables().Contains(Part->Grabbable));
		TestTrue(TEXT("E16 actual hover state"),Grabbers[H]->HasInteractable());
		Grabbers[H]->PalmGrab();
		TestEqual(TEXT("E9 first hand only in SDK pose collection"),Part->GrabComponent->GetNumGrabbers(),1);
	}
	TestTrue(TEXT("E16 first palm selects through detector"),Grabbers[0]->GetGrabbedComponent()==Part->Grabbable);
	TestFalse(TEXT("E9 second palm cannot select"),Grabbers[1]->IsGrabbing());
	Grabbers[0]->PalmRelease();
	TestEqual(TEXT("Real palm release clears SDK selection"),Part->GrabComponent->GetNumGrabbers(),0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP2bDoorTest, "vBoR.P2b.Door.TransferReleaseLatch", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP2bDoorTest::RunTest(const FString& Parameters)
{
	FP2bWorld W; AVBoRHingedDoor* Door = W.World->SpawnActor<AVBoRHingedDoor>(); AVBoRAxisPart* Part = W.World->SpawnActor<AVBoRAxisPart>();
	W.Play(); Door->SetAxisHandle(0,Part);
	FVBoRDoorMotion& Motion = FP2bTestAccess::Motion(Door);
	FP2bTestAccess::Begin(Part,P2bPose(FVector(100,9,60))); FP2bTestAccess::Value(Part,20);
	TestFalse(TEXT("Below threshold cannot unlock"),FP2bTestAccess::Transfer(Door,Part,P2bHand()));
	FP2bTestAccess::Value(Part,25);
	TestTrue(TEXT("E10 opens door with same holding hand"),FP2bTestAccess::Transfer(Door,Part,P2bHand()));
	TestTrue(TEXT("E10 unlocked once, correct handle and hand"),!Motion.bLocked && Motion.Metrics.Unlocks==1 && Motion.Metrics.LastUnlockHandle==0 && Motion.GrabHand==0);
	Part->Tick(0.1f); TestTrue(TEXT("E10 handle held open after SDK cancel"),Part->GetOpenRatio()>0.8f);
	FVBoRDoorHand Hands[2] = {P2bHand(45),{}}; Motion.Advance(Hands,1);
	TestTrue(TEXT("E10 same hand moves door without regrab"),FMath::IsNearlyEqual(Motion.AngleDeg,45.0,0.001));
	Motion.ReleaseGrab(true); Door->Tick(0.016f); Part->Tick(1);
	TestTrue(TEXT("Door release returns handle"),FMath::IsNearlyZero(Part->GetOpenRatio()));
	FP2bTestAccess::PalmOpen(Door,Part); FP2bTestAccess::Begin(Part,P2bPose(FVector(100,9,60)));
	TestFalse(TEXT("P2c D4 unlocked door waits for threshold"),FP2bTestAccess::Transfer(Door,Part,P2bHand(45)));
	FP2bTestAccess::Value(Part,25);
	TestTrue(TEXT("P2c D4 unlocked door accepts handle at threshold"),FP2bTestAccess::Transfer(Door,Part,P2bHand(45)));
	TestEqual(TEXT("E11 no second unlock"),Motion.Metrics.Unlocks,1); TestTrue(TEXT("P2c D4 handle stays pulled"),Part->GetOpenRatio()>0.8f);
	const int32 Latches = Motion.Metrics.Latches; const double Angle = Motion.AngleDeg;
	Hands[0] = P2bHand(); Motion.Advance(Hands,1); FP2bTestAccess::Apply(Door,Angle,Motion.Metrics.Unlocks,Latches);
	TestTrue(TEXT("E13 latch cancels hold and requires palm open"),Motion.bLocked && Part->GetHoldingHand()==INDEX_NONE && FP2bTestAccess::NeedsOpen(Part));
	FP2bTestAccess::Begin(Part,P2bPose(FVector(100,9,60))); FP2bTestAccess::Value(Part,25);
	TestFalse(TEXT("E13 closed palm cannot reopen"),FP2bTestAccess::Transfer(Door,Part,P2bHand()));
	FIsdkInteractionPointerEvent Event; Event.Type = EIsdkPointerEventType::Select; FP2bTestAccess::Pointer(Door,Event);
	TestTrue(TEXT("E12 connected door collider disabled and direct event ignored"),Motion.bLocked && Door->OutsideHandle->GetCollisionEnabled()==ECollisionEnabled::NoCollision && !Door->OutsideGrab->IsGrabInputMethodAllowed(EIsdkGrabInputMethod::Palm));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP2bDoorPivotTest, "vBoR.P2b.Door.MeshPivotAndPushBounds", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP2bDoorPivotTest::RunTest(const FString& Parameters)
{
	FP2bWorld W; AVBoRHingedDoor* Door = W.World->SpawnActor<AVBoRHingedDoor>();
	Door->HingePivot->SetRelativeTransform(FTransform(FRotator(0,180,0),FVector(35,-92.5,35)));
	Door->MeshRestFromRoot = FTransform(FQuat::Identity,FVector(-20,-92.5,87.5),FVector(1.1,0.1,1.05)); Door->OnConstruction(Door->GetActorTransform());
	TestTrue(TEXT("E14 root pivot mesh remains in intended world pose"),Door->DoorMesh->GetComponentTransform().Equals(Door->MeshRestFromRoot,0.001));
	const FBox Bounds = Door->DoorMesh->GetStaticMesh()->GetBoundingBox().TransformBy(Door->DoorMesh->GetRelativeTransform());
	TestTrue(TEXT("E14 push faces match mesh bounds"),Door->GetMotion().FaceMinCm.Equals(Bounds.Min) && Door->GetMotion().FaceMaxCm.Equals(Bounds.Max));
	W.Play(); const FVector Hinge = Door->HingePivot->GetComponentLocation(); FP2bTestAccess::Motion(Door).AngleDeg = 30;
	FP2bTestAccess::Apply(Door,0,0,0); TestTrue(TEXT("E14 hinge point stays fixed"),Hinge.Equals(Door->HingePivot->GetComponentLocation()));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP2bMissingMeshTest, "vBoR.P2b.Axis.MissingMesh", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP2bMissingMeshTest::RunTest(const FString& Parameters)
{
	FP2bWorld W; AVBoRAxisPart* Part = W.World->SpawnActor<AVBoRAxisPart>();
	AddExpectedError(TEXT("No part mesh; grabbing disabled"),EAutomationExpectedErrorFlags::Contains,1);
	Part->PartMesh->SetStaticMesh(nullptr); Part->OnConstruction(Part->GetActorTransform()); W.Play();
	TestTrue(TEXT("E18 empty mesh disables collider"),Part->GrabBox->GetCollisionEnabled()==ECollisionEnabled::NoCollision);
	TestFalse(TEXT("E18 palm input disabled"),Part->Grabbable->IsGrabInputMethodAllowed(EIsdkGrabInputMethod::Palm));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP2bP3Test, "vBoR.P2b.P3.HandleLidAnd07To08", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP2bP3Test::RunTest(const FString& Parameters)
{
	FP2bWorld W; AP3VehicleMock* Car = W.World->SpawnActor<AP3VehicleMock>();
	AVBoRScenarioRunner* Runner = W.World->SpawnActor<AVBoRScenarioRunner>(); Runner->Scenario = NewObject<UVBoRScenarioAsset>(Runner);
	Runner->Scenario->Steps.Empty();
	FVBoRScenarioStep Step; Step.StepId = TEXT("07"); Step.Instruction = FText::FromString(TEXT("Handle")); Step.Rule = EVBoRStepRule::DoorUnlocked; Step.TargetTag = TEXT("Car.DriverDoor"); Step.ConfirmBy = EVBoRConfirmBy::None;
	Runner->Scenario->Steps.Add(Step); Step.StepId = TEXT("08"); Step.Rule = EVBoRStepRule::DoorOpened; Runner->Scenario->Steps.Add(Step);
	W.Play(); Runner->bAlignmentIgnored = true; Runner->InitializeFlow(); Runner->ProcessFlow(0.1);
	TestEqual(TEXT("E17 begins at 07"),Runner->GetStepIndex(),0);
	TestNotNull(TEXT("P3 spawned outside axis handle"),Car->OutsideAxisHandle.Get()); TestNotNull(TEXT("P3 spawned stay lid"),Car->ConsoleLid.Get());
	TestTrue(TEXT("Handle actor starts at car origin, follows door hinge"),Car->OutsideAxisHandle->GetActorLocation().Equals(Car->GetActorLocation(),0.001) && Car->OutsideAxisHandle->GetRootComponent()->GetAttachParent()==Car->Door->HingePivot);
	TestTrue(TEXT("P2d console starts latched with 100 deg limit"),Car->ConsoleLid->bUseLatch && Car->ConsoleLid->GetMotion().bLocked && Car->ConsoleLid->MaxAngleDeg==100);
	FP2bTestAccess::Begin(Car->OutsideAxisHandle,P2bPose(FVector(100,9,60))); FP2bTestAccess::Value(Car->OutsideAxisHandle,25);
	TestTrue(TEXT("E17 threshold transfers same hand"),FP2bTestAccess::Transfer(Car->Door,Car->OutsideAxisHandle,P2bHand())); Runner->ProcessFlow(0.1);
	TestEqual(TEXT("E17 unlock completes 07"),Runner->GetStepIndex(),1);
	FVBoRDoorHand Hands[2] = {P2bHand(45),{}}; FP2bTestAccess::Motion(Car->Door).Advance(Hands,1); Runner->ProcessFlow(0.1);
	TestTrue(TEXT("E17 45 deg completes 08"),Runner->IsFinished());
	return true;
}
#endif
