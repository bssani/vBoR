#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "P5cTestFixture.h"
#include "P3VehicleMock.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "Interaction/IsdkGrabbableComponent.h"
#include "IsdkFunctionLibrary.h"
#include "Misc/OutputDevice.h"
#include "Misc/OutputDeviceRedirector.h"

struct FP5cTestAccess
{
	static double Value(const AVBoRAxisPart* Part) { return Part->Value; }
	static FTransform Rest(const AVBoRAxisPart* Part) { return Part->RestPivot * Part->GetActorTransform(); }
	static void SetValue(AVBoRAxisPart* Part, double Angle) { Part->Value = Angle; Part->AxisPivot->SetRelativeTransform(Part->ValueTransform() * Part->RestPivot); }
	static void Begin(AVBoRAxisPart* Part, const FIsdkGrabPose& Pose) { Part->HoldingHand = 0; Part->Transformer->BeginTransform({Pose},FIsdkTargetTransform(Part->AxisPivot)); }
	static void Move(AVBoRAxisPart* Part, const FIsdkGrabPose& Pose, bool bTracked) { Part->AxisPivot->SetRelativeTransform(Part->Transformer->UpdateTracked({Pose},FIsdkTargetTransform(Part->AxisPivot),bTracked)); }
	static void Release(AVBoRAxisPart* Part) { Part->ReleasePart(); }
	static UP5bAudioObserver* Audio(AVBoRAxisPart* Part) { auto* Audio = NewObject<UP5bAudioObserver>(Part); Part->StateAudio = Audio; for (auto& Slot : Part->SoundAudio) { Slot = Audio; } return Audio; }
	static const TArray<TObjectPtr<UStaticMeshComponent>>& Plates(AP3VehicleMock* Car) { return Car->Plates; }
};

namespace
{
	struct FP5cWorld
	{
		UWorld* World = UWorld::CreateWorld(EWorldType::Game,false);
		void Play() { World->InitializeActorsForPlay(FURL()); World->GetWorldSettings()->NotifyBeginPlay(); }
		AP2bTrackedPart* Part(int32 Start = 1, float Nudge = 7)
		{
			auto* Part = World->SpawnActorDeferred<AP2bTrackedPart>(AP2bTrackedPart::StaticClass(),FTransform::Identity);
			Part->Release = EVBoRAxisRelease::Detent; Part->DetentStartIndex = Start; Part->DetentNudgeDeg = Nudge;
			Part->FinishSpawning(FTransform::Identity); return Part;
		}
		~FP5cWorld() { World->EndPlay(EEndPlayReason::Quit); World->DestroyWorld(false); }
	};
	FIsdkGrabPose P5cPose(AVBoRAxisPart* Part, double Angle)
	{
		const FQuat Rotation(FVector::XAxisVector,FMath::DegreesToRadians(Angle * (Part->bReverse ? -1 : 1)));
		return UIsdkFunctionLibrary::MakeGrabPoseStruct(11,FP5cTestAccess::Rest(Part).TransformPosition(Rotation.RotateVector(FVector(0,100,0))),FQuat::Identity);
	}
	void P5cBegin(AVBoRAxisPart* Part) { FP5cTestAccess::Begin(Part,P5cPose(Part,FP5cTestAccess::Value(Part))); }
	void P5cMove(AVBoRAxisPart* Part, double Angle, bool bTracked = true) { FP5cTestAccess::Move(Part,P5cPose(Part,Angle),bTracked); }
	void P5cSetUpper(AVBoRAxisPart* Part)
	{
		// Start detent at center (upper start changes lane-change reference, P5c D3/E6/E7)
		P5cBegin(Part); P5cMove(Part,20); Part->Tick(0.01f); FP5cTestAccess::Release(Part); Part->Tick(1);
	}
	UP5cEventObserver* P5cObserve(AVBoRAxisPart* Part)
	{
		auto* Events = NewObject<UP5cEventObserver>(Part);
		Part->OnDetentNudge.AddDynamic(Events,&UP5cEventObserver::Nudged); Part->OnDetentChanged.AddDynamic(Events,&UP5cEventObserver::Changed); return Events;
	}
	struct FP5cLogs : FOutputDevice
	{
		TArray<FString> Warnings;
		FP5cLogs() { GLog->FlushThreadedLogs(); GLog->AddOutputDevice(this); }
		~FP5cLogs() { GLog->RemoveOutputDevice(this); }
		virtual void Serialize(const TCHAR* Message, ELogVerbosity::Type Verbosity, const FName& Category) override
		{ if (Category == TEXT("LogAxisPart") && Verbosity == ELogVerbosity::Warning) { Warnings.Add(Message); } }
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5cBoundary,"vBoR.P5c.E1InclusiveBoundaryAndDeferredAudio",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5cBoundary::RunTest(const FString&)
{
	for (int32 Direction : {-1,1})
	{
		FP5cWorld W; auto* Part = W.Part(); auto* Events = P5cObserve(Part); W.Play(); auto* Audio = FP5cTestAccess::Audio(Part);
		P5cBegin(Part); P5cMove(Part,Direction * 7);
		TestEqual(TEXT("Center stays confirmed"),Part->GetDetentIndex(),1);
		TestEqual(TEXT("No Move event"),Events->Order.Num(),0); TestEqual(TEXT("No Move sound"),Audio->Plays,0);
		Part->Tick(0.01f); TestTrue(TEXT("One directional event"),Events->Directions == TArray<int32>{Direction}); TestEqual(TEXT("One click"),Audio->Plays,1);
		TestTrue(TEXT("Debug includes nudge angle and armed state"),Part->GetDebugText().Contains(TEXT("nudge=7.000 deg armed=0")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5cBelow,"vBoR.P5c.E2BelowNudge",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5cBelow::RunTest(const FString&)
{
	FP5cWorld W; auto* Part = W.Part(); auto* Events = P5cObserve(Part); W.Play(); P5cBegin(Part); P5cMove(Part,6.9);
	FP5cTestAccess::Release(Part); Part->Tick(1); TestEqual(TEXT("No signal"),Events->Order.Num(),0); TestEqual(TEXT("Returns to center"),FP5cTestAccess::Value(Part),0.0); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5cReturn,"vBoR.P5c.E3NudgeReturnTime",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5cReturn::RunTest(const FString&)
{
	FP5cWorld W; auto* Part = W.Part(); auto* Events = P5cObserve(Part); W.Play(); auto* Audio = FP5cTestAccess::Audio(Part); Part->ReturnDegPerSecond = 10;
	P5cBegin(Part); P5cMove(Part,7); Part->Tick(0.01f); FP5cTestAccess::Release(Part); Part->Tick(0.2f);
	TestTrue(TEXT("Returns 2 deg in 0.2 s"),FMath::IsNearlyEqual(FP5cTestAccess::Value(Part),5.0,0.001)); Part->Tick(0.5f); Part->Tick(1);
	TestEqual(TEXT("Stops at center"),FP5cTestAccess::Value(Part),0.0); TestEqual(TEXT("No release or return nudge"),Events->Directions.Num(),1); TestEqual(TEXT("No return click"),Audio->Plays,1); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5cRearm,"vBoR.P5c.E4HalfAngleRearm",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5cRearm::RunTest(const FString&)
{
	for (double Reversal : {4.0,3.0,3.5})
	{
		FP5cWorld W; auto* Part = W.Part(); auto* Events = P5cObserve(Part); W.Play(); P5cBegin(Part);
		P5cMove(Part,8); P5cMove(Part,Reversal); P5cMove(Part,8); Part->Tick(0.01f);
		TestEqual(TEXT("Only half angle rearms"),Events->Directions.Num(),Reversal > 3.5 ? 1 : 2);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5cOrder,"vBoR.P5c.E5NudgeBeforeDetent",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5cOrder::RunTest(const FString&)
{
	for (bool Slow : {false,true})
	{
		FP5cWorld W; auto* Part = W.Part(); auto* Events = P5cObserve(Part); W.Play(); auto* Audio = FP5cTestAccess::Audio(Part); P5cBegin(Part);
		if (Slow) { for (double Angle : {4.0,7.0,10.0,14.0}) { P5cMove(Part,Angle); Part->Tick(0.01f); } }
		P5cMove(Part,20); TestEqual(TEXT("Upper confirmed before Tick"),Part->GetDetentIndex(),2); Part->Tick(0.01f);
		TestTrue(TEXT("Nudge then detent"),Events->Order == TArray<FString>({TEXT("N1"),TEXT("D2")})); TestEqual(TEXT("Two clicks"),Audio->Plays,2);
		FP5cTestAccess::Release(Part); Part->Tick(1); TestEqual(TEXT("Stays upper"),FP5cTestAccess::Value(Part),20.0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5cUpper,"vBoR.P5c.E6UpperDoesNotNudge",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5cUpper::RunTest(const FString&)
{
	FP5cWorld W; auto* Part = W.Part(); W.Play(); P5cSetUpper(Part); auto* Events = P5cObserve(Part); P5cBegin(Part); P5cMove(Part,13);
	FP5cTestAccess::Release(Part); Part->Tick(1); TestEqual(TEXT("Upper retained"),FP5cTestAccess::Value(Part),20.0); TestEqual(TEXT("No nudge or detent"),Events->Order.Num(),0); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5cCancel,"vBoR.P5c.E7CenterEntryDisarmsOvershoot",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5cCancel::RunTest(const FString&)
{
	for (bool Overshoot : {false,true})
	{
		FP5cWorld W; auto* Part = W.Part(); W.Play(); P5cSetUpper(Part); auto* Events = P5cObserve(Part); P5cBegin(Part); P5cMove(Part,Overshoot ? -10 : 0); Part->Tick(0.01f);
		TestTrue(TEXT("Cancel only emits center"),Events->Order == TArray<FString>{TEXT("D1")});
		if (Overshoot) { P5cMove(Part,-10); Part->Tick(0.01f); TestEqual(TEXT("Remaining past nudge stays disarmed"),Events->Directions.Num(),0); P5cMove(Part,-3); }
		P5cMove(Part,-7); Part->Tick(0.01f); TestTrue(TEXT("Near center passage rearms opposite nudge"),Events->Order == TArray<FString>({TEXT("D1"),TEXT("N-1")}));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5cSameFrame,"vBoR.P5c.E8SameFrameRelease",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5cSameFrame::RunTest(const FString&)
{
	FP5cWorld W; auto* Part = W.Part(); auto* Events = P5cObserve(Part); W.Play(); auto* Audio = FP5cTestAccess::Audio(Part);
	P5cBegin(Part); P5cMove(Part,7); FP5cTestAccess::Release(Part); TestEqual(TEXT("Release does not dispatch"),Events->Order.Num(),0); TestEqual(TEXT("Release does not play click"),Audio->Plays,0);
	Part->Tick(1); Part->Tick(1); TestTrue(TEXT("Pending nudge survives release exactly once"),Events->Directions == TArray<int32>{1}); TestEqual(TEXT("One click"),Audio->Plays,1); TestEqual(TEXT("Returned center"),FP5cTestAccess::Value(Part),0.0); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5cTracking,"vBoR.P5c.E9TrackingRecoveryDiscardsNudge",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5cTracking::RunTest(const FString&)
{
	FP5cWorld W; auto* Part = W.Part(); auto* Events = P5cObserve(Part); W.Play(); P5cBegin(Part); Part->bTracked = false;
	P5cMove(Part,10,false); Part->Tick(1); Part->bTracked = true; P5cMove(Part,10); P5cMove(Part,10); Part->Tick(0.01f);
	TestEqual(TEXT("Discarded ten degrees never nudge"),Events->Order.Num(),0); TestTrue(TEXT("Recovery keeps center pose"),FMath::IsNearlyZero(FP5cTestAccess::Value(Part),0.001));
	P5cMove(Part,17); Part->Tick(0.01f); TestTrue(TEXT("Subsequent tracked seven degrees nudge"),Events->Directions == TArray<int32>{1}); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5cRegrab,"vBoR.P5c.E10RegrabDuringReturn",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5cRegrab::RunTest(const FString&)
{
	FP5cWorld W; auto* Part = W.Part(); auto* Events = P5cObserve(Part); W.Play(); Part->ReturnDegPerSecond = 10; P5cBegin(Part); P5cMove(Part,8);
	FP5cTestAccess::Release(Part); Part->Tick(0.3f); TestEqual(TEXT("Return adds no nudge"),Events->Directions.Num(),1);
	const FTransform Before = Part->AxisPivot->GetComponentTransform(); P5cBegin(Part); P5cMove(Part,5); Part->Tick(0.01f);
	TestTrue(TEXT("Regrab keeps current pose"),Part->AxisPivot->GetComponentTransform().Equals(Before,0.001)); TestEqual(TEXT("Regrab below threshold is quiet"),Events->Directions.Num(),1);
	P5cMove(Part,7); Part->Tick(0.01f); TestEqual(TEXT("Release rearmed next nudge"),Events->Directions.Num(),2); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5cInvalid,"vBoR.P5c.E11InvalidNudgeDisablesGrab",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5cInvalid::RunTest(const FString&)
{
	for (int32 Case = 0; Case < 5; ++Case)
	{
		FP5cWorld W; FP5cLogs Logs;
		auto* Part = W.World->SpawnActorDeferred<AP2bTrackedPart>(AP2bTrackedPart::StaticClass(),FTransform::Identity); Part->Release = EVBoRAxisRelease::Detent;
		Part->DetentUpThresholdDeg = Case == 1 ? 12 : 15; Part->DetentDownThresholdDeg = Case == 2 ? 12 : 15;
		Part->DetentNudgeDeg = Case == 0 ? -1 : Case < 3 ? 12 : Case == 3 ? 15 : 16;
		Part->FinishSpawning(FTransform::Identity); W.Play(); Part->Tick(1); GLog->FlushThreadedLogs();
		TestEqual(TEXT("One warning"),Logs.Warnings.Num(),1); TestTrue(TEXT("Warning explains nudge and disabled grab"),Logs.Warnings.Num() == 1 && Logs.Warnings[0].Contains(TEXT("nudge")) && Logs.Warnings[0].Contains(TEXT("grabbing disabled")));
		TestEqual(TEXT("Collider disabled"),Part->GrabBox->GetCollisionEnabled(),ECollisionEnabled::NoCollision); TestFalse(TEXT("Palm disabled"),Part->Grabbable->IsGrabInputMethodAllowed(EIsdkGrabInputMethod::Palm));
		TestFalse(TEXT("Pinch disabled"),Part->Grabbable->IsGrabInputMethodAllowed(EIsdkGrabInputMethod::Pinch));
		P5cBegin(Part); P5cMove(Part,20); Part->Tick(0.01f); TestEqual(TEXT("Invalid acceptance keeps initial angle"),FP5cTestAccess::Value(Part),0.0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5cDisabled,"vBoR.P5c.E12DefaultGearNudgeDisabled",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5cDisabled::RunTest(const FString&)
{
	FP5cWorld W; FP5cLogs Logs; auto* Part = W.Part(1,0); auto* Events = P5cObserve(Part); W.Play(); P5cBegin(Part);
	TestEqual(TEXT("Class default nudge is off"),GetDefault<AVBoRAxisPart>()->DetentNudgeDeg,0.f);
	for (double Angle : {7.0,-7.0,20.0,-20.0}) { P5cMove(Part,Angle); } Part->Tick(0.01f); GLog->FlushThreadedLogs();
	TestEqual(TEXT("No nudge"),Events->Directions.Num(),0); TestEqual(TEXT("No warning"),Logs.Warnings.Num(),0); TestTrue(TEXT("Gear transitions preserved"),Events->Indices == TArray<int32>({2,1,0})); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5cCar,"vBoR.P5c.E13ActualCarPlacementAndBounds",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5cCar::RunTest(const FString&)
{
	FP5cWorld W; const FTransform CarWorld(FRotator(0,37,0),FVector(123,-256,42)); auto* Car = W.World->SpawnActor<AP3VehicleMock>(AP3VehicleMock::StaticClass(),CarWorld); W.Play();
	AVBoRAxisPart* Part = Car->TurnSignalLever; if (!TestNotNull(TEXT("Actual approximate turn signal"),Part)) { return false; }
	TestTrue(TEXT("Actor at car origin"),Part->GetActorTransform().Equals(CarWorld,0.001)); TestTrue(TEXT("Pivot at exposed position"),Part->AxisPivot->GetRelativeLocation().Equals(Car->TurnSignalPivotCm));
	TestTrue(TEXT("Rest mesh keeps authored world pose"),Part->PartMesh->GetComponentTransform().Equals(Part->MeshRestFromRoot * CarWorld,0.001)); TestEqual(TEXT("Starts center"),Part->GetDetentIndex(),1); TestEqual(TEXT("Nudge enabled"),Part->DetentNudgeDeg,7.f);
	const FVector RestTip = CarWorld.InverseTransformPosition(Part->AxisPivot->GetComponentTransform().TransformPosition(FVector(0,-Car->TurnSignalSizeCm.Y,0)));
	TestTrue(TEXT("Lever extends car -Y"),(RestTip - Car->TurnSignalPivotCm).Equals(FVector(0,-Car->TurnSignalSizeCm.Y,0),0.001));
	for (double Angle : {-20.0,0.0,20.0})
	{
		FP5cTestAccess::SetValue(Part,Angle); const FVector Tip = CarWorld.InverseTransformPosition(Part->AxisPivot->GetComponentTransform().TransformPosition(FVector(0,-Car->TurnSignalSizeCm.Y,0)));
		if (Angle > 0) { TestTrue(TEXT("Positive angle raises tip"),Tip.Z > RestTip.Z); }
		const FBox Bounds = Part->PartMesh->GetStaticMesh()->GetBoundingBox().TransformBy(Part->PartMesh->GetComponentTransform().GetRelativeTransform(CarWorld));
		for (int32 Index : {7,11,12}) { UStaticMeshComponent* Plate = FP5cTestAccess::Plates(Car)[Index]; TestFalse(TEXT("Clears pillar dashboard and steering at all detents"),Bounds.Intersect(Plate->GetStaticMesh()->GetBoundingBox().TransformBy(Plate->GetRelativeTransform()))); }
		for (double GearAngle : {-20.0,0.0,20.0}) { FP5cTestAccess::SetValue(Car->GearLever,GearAngle); auto* Mesh = Car->GearLever->PartMesh.Get(); TestFalse(TEXT("Clears gear at all detent pairs"),Bounds.Intersect(Mesh->GetStaticMesh()->GetBoundingBox().TransformBy(Mesh->GetComponentTransform().GetRelativeTransform(CarWorld)))); }
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5cCleanup,"vBoR.P5c.E14CarEndPlayCleanup",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5cCleanup::RunTest(const FString&)
{
	FP5cWorld W; auto* Car = W.World->SpawnActor<AP3VehicleMock>(); W.Play(); AVBoRAxisPart* Part = Car->TurnSignalLever;
	if (!TestNotNull(TEXT("Turn signal exists"),Part)) { return false; } Car->Destroy(); TestTrue(TEXT("EndPlay destroys turn signal"),Part->IsActorBeingDestroyed()); return true;
}
#endif
