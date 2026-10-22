#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "P2cTestFixture.h"
#include "P2bTestFixture.h"
#include "../P3Probe/P3VehicleMock.h"
#include "Components/BoxComponent.h"
#include "Components/ChildActorComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "IsdkFunctionLibrary.h"
#include "Misc/OutputDevice.h"
#include "Misc/OutputDeviceRedirector.h"

struct FP2eTestAccess
{
	static TArray<FString> Problems(const AVBoRHingedPanel* Panel) { return Panel->EvaluateSetup().Problems; }
	static AVBoRHingedPanel* Linked(const AVBoRAxisPart* Part) { return Part->LinkedDoor; }
	static void Angle(AVBoRHingedPanel* Panel, double Angle)
	{
		Panel->Motion.AngleDeg = Angle; Panel->ApplyState(0, Panel->Motion.Metrics.Unlocks, Panel->Motion.Metrics.Latches);
	}
	static void Begin(AVBoRAxisPart* Part)
	{
		Part->HoldingHand = 0;
		Part->Transformer->BeginTransform({UIsdkFunctionLibrary::MakeGrabPoseStruct(11, FVector(0,10,0), FQuat::Identity)}, FIsdkTargetTransform(Part->AxisPivot));
	}
	static void Value(AVBoRAxisPart* Part, double Value)
	{
		Part->Value = Value; Part->AxisPivot->SetRelativeTransform(Part->ValueTransform() * Part->RestPivot);
	}
};

namespace
{
	struct FP2eWorld
	{
		UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
		void Play() { World->InitializeActorsForPlay(FURL()); World->GetWorldSettings()->NotifyBeginPlay(); }
		~FP2eWorld() { World->EndPlay(EEndPlayReason::Quit); World->DestroyWorld(false); }
	};
	struct FP2eLogs : FOutputDevice
	{
		TArray<FString> Warnings, Attachments;
		FP2eLogs() { GLog->FlushThreadedLogs(); GLog->AddOutputDevice(this); }
		~FP2eLogs() { GLog->RemoveOutputDevice(this); }
		virtual void Serialize(const TCHAR* Message, ELogVerbosity::Type Verbosity, const FName& Category) override
		{
			if (Category != TEXT("LogAxisDoor")) { return; }
			// Engine downgrades expected Warning to Verbose (P2e review note)
			if (Verbosity == ELogVerbosity::Warning || (Verbosity == ELogVerbosity::Verbose && FString(Message).Contains(TEXT("P2e setup:")))) { Warnings.Add(Message); }
			if (FString(Message).Contains(TEXT("P2e attached"))) { Attachments.Add(Message); }
		}
		void Flush() { GLog->FlushThreadedLogs(); }
	};
	AVBoRAxisPart* P2eHandle(UWorld* World, EVBoRDoorHandleSide Side, USceneComponent* Parent = nullptr)
	{
		AVBoRAxisPart* Part = World->SpawnActor<AVBoRAxisPart>(); Part->DoorHandleSide = Side;
		Part->SetActorTransform(FTransform(FRotator(12,31,-8), FVector(23,-40,17)));
		if (Parent) { Part->AttachToComponent(Parent, FAttachmentTransformRules::KeepWorldTransform); }
		return Part;
	}
	UChildActorComponent* P2eChild(AVBoRHingedPanel* Panel, USceneComponent* Parent, EVBoRDoorHandleSide Side)
	{
		UChildActorComponent* Child = NewObject<UChildActorComponent>(Panel);
		Child->SetupAttachment(Parent); Child->SetRelativeLocation(FVector(31,4,22));
		Child->SetChildActorClass(AVBoRAxisPart::StaticClass()); Child->RegisterComponent();
		CastChecked<AVBoRAxisPart>(Child->GetChildActor())->DoorHandleSide = Side;
		return Child;
	}
	void P2eCheckButton(FAutomationTestBase& Test, AVBoRHingedPanel* Panel, bool bExpectWarnings = true)
	{
		const FTransform PanelPose = Panel->GetActorTransform(), HingePose = Panel->HingePivot->GetComponentTransform();
		AVBoRAxisPart* Outside = Panel->OutsideAxisHandle; AVBoRAxisPart* Inside = Panel->InsideAxisHandle;
		TArray<AActor*> Children; Panel->GetAttachedActors(Children);
		TArray<FTransform> Poses; TArray<USceneComponent*> Parents;
		for (AActor* Child : Children) { Poses.Add(Child->GetActorTransform()); Parents.Add(Child->GetRootComponent()->GetAttachParent()); }
		Panel->CheckSetup();
		const TArray<FString> Problems = FP2eTestAccess::Problems(Panel);
		Test.TestEqual(TEXT("E19 report uses the shared judgement"), Panel->SetupReport, Problems.IsEmpty() ? FString(TEXT("No issues")) : FString::Join(Problems, TEXT("\n")));
		Test.TestTrue(TEXT("E19 button does not change slots"), Panel->OutsideAxisHandle == Outside && Panel->InsideAxisHandle == Inside);
		Test.TestTrue(TEXT("E19 button does not move panel or hinge"), Panel->GetActorTransform().Equals(PanelPose) && Panel->HingePivot->GetComponentTransform().Equals(HingePose));
		for (int32 I = 0; I < Children.Num(); ++I)
		{
			Test.TestTrue(TEXT("E19 button does not move or reparent children"), Children[I]->GetActorTransform().Equals(Poses[I]) && Children[I]->GetRootComponent()->GetAttachParent() == Parents[I]);
		}
		if (bExpectWarnings)
		{
			for (const FString& Problem : Problems) { Test.AddExpectedMessagePlain(Problem, ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 1); }
		}
	}
	FVBoRDoorHand P2eHand(FVector Point)
	{
		FVBoRDoorHand Hand; Hand.bTracked = true; Hand.PalmNormal = FVector::YAxisVector;
		for (FVector& P : Hand.Points) { P = Point; } return Hand;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP2eAttachmentPaths, "vBoR.P2e.Assembly.E1E2E4E5AttachmentPaths", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP2eAttachmentPaths::RunTest(const FString& Parameters)
{
	for (int32 Mode = 0; Mode < 3; ++Mode)
	{
		FP2eWorld W; FP2eLogs Logs; AVBoRHingedPanel* Panel = W.World->SpawnActor<AVBoRHingedPanel>();
		Panel->SetActorTransform(FTransform(FRotator(4,35,9), FVector(200,80,10)));
		Panel->HingePivot->SetRelativeTransform(FTransform(FRotator(73,0,0), FVector(8,-11,42)));
		Panel->OnConstruction(Panel->GetActorTransform());
		AVBoRAxisPart* Parts[2]; USceneComponent* Parents[2]; FTransform Before[2];
		for (int32 I = 0; I < 2; ++I)
		{
			const EVBoRDoorHandleSide Side = I == 0 ? EVBoRDoorHandleSide::Outside : EVBoRDoorHandleSide::Inside;
			if (Mode == 1) { Parts[I] = CastChecked<AVBoRAxisPart>(P2eChild(Panel, Panel->HingePivot, Side)->GetChildActor()); }
			else { Parts[I] = P2eHandle(W.World, Side, Mode == 0 ? Panel->GetRootComponent() : nullptr); }
			Parents[I] = Parts[I]->GetRootComponent()->GetAttachParent(); Before[I] = Parts[I]->GetActorTransform();
		}
		if (Mode == 2) { Panel->OutsideAxisHandle = Parts[0]; Panel->InsideAxisHandle = Parts[1]; }
		P2eCheckButton(*this, Panel); TestEqual(TEXT("Valid setup"), FP2eTestAccess::Problems(Panel).Num(), 0);
		W.Play(); Logs.Flush();
		TestTrue(TEXT("Both handles resolve"), Panel->OutsideAxisHandle == Parts[0] && Panel->InsideAxisHandle == Parts[1]);
		TestEqual(TEXT("One log for each actual reattachment"), Logs.Attachments.Num(), Mode == 1 ? 0 : 2);
		const FTransform HingeBefore = Panel->HingePivot->GetComponentTransform();
		for (int32 I = 0; I < 2; ++I)
		{
			TestTrue(TEXT("World location within 0.01 cm"), Parts[I]->GetActorLocation().Equals(Before[I].GetLocation(), 0.01));
			TestTrue(TEXT("World rotation within 0.01 deg"), FMath::RadiansToDegrees(Parts[I]->GetActorQuat().AngularDistance(Before[I].GetRotation())) < 0.01);
			TestTrue(TEXT("Scale preserved"), Parts[I]->GetActorScale3D().Equals(Before[I].GetScale3D(), 0.0001));
			TestTrue(TEXT("Under the hinge"), Parts[I]->GetRootComponent()->IsAttachedTo(Panel->HingePivot));
			if (Mode == 1) { TestTrue(TEXT("Child Actor keeps its component parent"), Parts[I]->GetRootComponent()->GetAttachParent() == Parents[I]); }
			TestTrue(TEXT("Linked to panel"), FP2eTestAccess::Linked(Parts[I]) == Panel);
		}
		FP2eTestAccess::Angle(Panel, 30);
		const FQuat Delta(HingeBefore.GetUnitAxis(EAxis::Z), FMath::DegreesToRadians(30.0));
		for (int32 I = 0; I < 2; ++I)
		{
			const FVector Expected = HingeBefore.GetLocation() + Delta.RotateVector(Before[I].GetLocation() - HingeBefore.GetLocation());
			TestTrue(TEXT("E5 follows actual tilted world hinge axis by 30 deg"), Parts[I]->GetActorLocation().Equals(Expected, 0.01));
			TestTrue(TEXT("E5 world orientation follows hinge"), Parts[I]->GetActorQuat().Equals(Delta * Before[I].GetRotation(), 0.0001));
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP2eBadChild, "vBoR.P2e.Assembly.E3ChildOutsideHinge", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP2eBadChild::RunTest(const FString& Parameters)
{
	FP2eWorld W; FP2eLogs Logs; AVBoRHingedPanel* Panel = W.World->SpawnActor<AVBoRHingedPanel>();
	UChildActorComponent* Child = P2eChild(Panel, Panel->GetRootComponent(), EVBoRDoorHandleSide::Outside);
	AVBoRAxisPart* Part = CastChecked<AVBoRAxisPart>(Child->GetChildActor()); const FTransform Before = Part->GetActorTransform();
	TestEqual(TEXT("One child placement problem"), FP2eTestAccess::Problems(Panel).Num(), 1); P2eCheckButton(*this, Panel); W.Play(); Logs.Flush();
	TestNull(TEXT("Invalid child is not linked"), Panel->OutsideAxisHandle.Get()); TestNull(TEXT("Part unlinked"), FP2eTestAccess::Linked(Part));
	TestTrue(TEXT("Child stays with original component"), Part->GetRootComponent()->GetAttachParent() == Child && Part->GetActorTransform().Equals(Before));
	TestNull(TEXT("No outside handle connected"), Panel->OutsideAxisHandle.Get());
	TestEqual(TEXT("E20 one warning"), Logs.Warnings.Num(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP2eOpenTransfer, "vBoR.P2e.Assembly.E6OpenedHandleTransfer", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP2eOpenTransfer::RunTest(const FString& Parameters)
{
	FP2eWorld W; AP2cTrackedPanel* Panel = W.World->SpawnActor<AP2cTrackedPanel>(); AP2bTrackedPart* Part = W.World->SpawnActor<AP2bTrackedPart>();
	Part->DoorHandleSide = EVBoRDoorHandleSide::Outside; Part->AttachToActor(Panel, FAttachmentTransformRules::KeepWorldTransform); W.Play();
	FP2eTestAccess::Angle(Panel, 30); Panel->Hands[0] = P2eHand(FQuat(FVector::UpVector, PI/6).RotateVector(FVector(100,9,60)));
	FP2eTestAccess::Begin(Part); FP2eTestAccess::Value(Part, Part->MaxAngleDeg * 0.79); Part->Tick(0.001f);
	TestTrue(TEXT("Handle angle increased"), Part->GetOpenRatio() > 0.78); TestEqual(TEXT("Below 80 percent does not transfer"), Panel->GetMotion().GrabHand, INDEX_NONE);
	FP2eTestAccess::Value(Part, Part->MaxAngleDeg * 0.81); Part->Tick(0.001f);
	TestEqual(TEXT("At 80 percent same hand transfers"), Panel->GetMotion().GrabHand, 0);
	Panel->Hands[0] = P2eHand(FQuat(FVector::UpVector, PI/4).RotateVector(FVector(100,9,60))); Panel->Tick(1);
	TestTrue(TEXT("Same hand pulls panel from 30 to 45 deg"), FMath::IsNearlyEqual(Panel->GetMotion().AngleDeg, 45.0, 0.001));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP2ePriority, "vBoR.P2e.Assembly.E7E9ExplicitPriority", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP2ePriority::RunTest(const FString& Parameters)
{
	for (bool bOpposite : {false, true})
	{
		FP2eWorld W; FP2eLogs Logs; AVBoRHingedPanel* Panel = W.World->SpawnActor<AVBoRHingedPanel>();
		AVBoRAxisPart* A = P2eHandle(W.World, bOpposite ? EVBoRDoorHandleSide::Inside : EVBoRDoorHandleSide::Outside, bOpposite ? Panel->GetRootComponent() : nullptr);
		AVBoRAxisPart* B = bOpposite ? nullptr : P2eHandle(W.World, EVBoRDoorHandleSide::Outside, Panel->GetRootComponent());
		Panel->OutsideAxisHandle = A; const FTransform Before = A->GetActorTransform();
		P2eCheckButton(*this, Panel); TestEqual(TEXT("One priority/side problem"), FP2eTestAccess::Problems(Panel).Num(), 1); W.Play(); Logs.Flush();
		TestTrue(TEXT("Explicit slot wins"), Panel->OutsideAxisHandle == A && FP2eTestAccess::Linked(A) == Panel);
		TestNull(TEXT("Opposite slot cannot reuse an explicitly assigned handle"), Panel->InsideAxisHandle.Get());
		TestTrue(TEXT("Explicit handle world pose preserved"), A->GetActorTransform().Equals(Before, 0.0001));
		if (B) { TestNull(TEXT("Ignored candidate is unlinked"), FP2eTestAccess::Linked(B)); TestTrue(TEXT("Ignored candidate not reparented"), B->GetRootComponent()->GetAttachParent() == Panel->GetRootComponent()); }
		TestEqual(TEXT("E20 warning once"), Logs.Warnings.Num(), 1);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP2eAmbiguous, "vBoR.P2e.Assembly.E8AmbiguousSide", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP2eAmbiguous::RunTest(const FString& Parameters)
{
	FP2eWorld W; FP2eLogs Logs; AVBoRHingedPanel* Panel = W.World->SpawnActor<AVBoRHingedPanel>();
	P2eHandle(W.World, EVBoRDoorHandleSide::Outside, Panel->GetRootComponent()); P2eHandle(W.World, EVBoRDoorHandleSide::Outside, Panel->HingePivot);
	AVBoRAxisPart* Inside = P2eHandle(W.World, EVBoRDoorHandleSide::Inside, Panel->DoorMesh);
	P2eCheckButton(*this, Panel); TestEqual(TEXT("One ambiguity problem"), FP2eTestAccess::Problems(Panel).Num(), 1); W.Play(); Logs.Flush();
	TestNull(TEXT("Ambiguous side stays empty"), Panel->OutsideAxisHandle.Get()); TestTrue(TEXT("Other side still links"), Panel->InsideAxisHandle == Inside);
	TestNull(TEXT("Ambiguous side has no handle"), Panel->OutsideAxisHandle.Get());
	TestEqual(TEXT("E20 one ambiguity warning"), Logs.Warnings.Num(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP2eShared, "vBoR.P2e.Assembly.E10FirstConnectedKeepsHandle", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP2eShared::RunTest(const FString& Parameters)
{
	for (int32 Order = 0; Order < 2; ++Order)
	{
		FP2eWorld W; FP2eLogs Logs; AVBoRHingedPanel* A = W.World->SpawnActor<AVBoRHingedPanel>(); AVBoRHingedPanel* B = W.World->SpawnActor<AVBoRHingedPanel>();
		AVBoRAxisPart* Part = P2eHandle(W.World, EVBoRDoorHandleSide::Outside, B->GetRootComponent()); A->OutsideAxisHandle = Part;
		const FTransform Before = Part->GetActorTransform(); P2eCheckButton(*this, A, false); P2eCheckButton(*this, B, false);
		TestEqual(TEXT("A has shared claim problem"), FP2eTestAccess::Problems(A).Num(), 1); TestEqual(TEXT("B has shared claim problem"), FP2eTestAccess::Problems(B).Num(), 1);
		const FString SharedProblem = FP2eTestAccess::Problems(A)[0];
		AddExpectedMessagePlain(SharedProblem, ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, Order == 0 ? 2 : 3);
		AVBoRHingedPanel* First = Order == 0 ? A : B; AVBoRHingedPanel* Second = Order == 0 ? B : A;
		W.World->InitializeActorsForPlay(FURL()); First->DispatchBeginPlay(); Logs.Flush();
		TestEqual(TEXT("First BeginPlay warns once"), Logs.Warnings.Num(), 1);
		TestTrue(TEXT("First panel links and keeps world pose"), First->OutsideAxisHandle == Part && FP2eTestAccess::Linked(Part) == First && Part->GetActorTransform().Equals(Before, 0.0001));
		TestTrue(TEXT("First panel owns hinge parent"), Part->GetRootComponent()->GetAttachParent() == First->HingePivot);
		TestEqual(TEXT("Current claims after first attachment"), FP2eTestAccess::Problems(Second).Num(), Order == 0 ? 0 : 1);
		Second->DispatchBeginPlay(); W.World->GetWorldSettings()->NotifyBeginPlay(); Logs.Flush();
		TestNull(TEXT("Second panel does not connect"), Second->OutsideAxisHandle.Get());
		TestNull(TEXT("Second panel has no handle"), Second->OutsideAxisHandle.Get());
		TestEqual(TEXT("Only still-visible claims cause a second warning"), Logs.Warnings.Num(), Order == 0 ? 1 : 2);
		TestTrue(TEXT("No historical claims on either panel"), FP2eTestAccess::Problems(A).IsEmpty() && FP2eTestAccess::Problems(B).IsEmpty());
		Second->SetAxisHandle(0, Part); Logs.Flush();
		TestNull(TEXT("Runtime shared assignment is rejected"), Second->OutsideAxisHandle.Get());
		TestEqual(TEXT("Runtime rejection warns once"), Logs.Warnings.Num(), Order == 0 ? 2 : 3);
		TestTrue(TEXT("Rejected assignment preserves first link, hinge and pose"), First->OutsideAxisHandle == Part && FP2eTestAccess::Linked(Part) == First && Part->GetRootComponent()->GetAttachParent() == First->HingePivot && Part->GetActorTransform().Equals(Before, 0.0001));
		TestEqual(TEXT("Only first panel reattaches"), Logs.Attachments.Num(), 1);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP2eNone, "vBoR.P2e.Assembly.E11NonDoorPart", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP2eNone::RunTest(const FString& Parameters)
{
	FP2eWorld W; FP2eLogs Logs; AVBoRHingedPanel* Panel = W.World->SpawnActor<AVBoRHingedPanel>();
	AVBoRAxisPart* Part = P2eHandle(W.World, EVBoRDoorHandleSide::None, Panel->GetRootComponent());
	TestEqual(TEXT("Door side defaults to none"), GetDefault<AVBoRAxisPart>()->DoorHandleSide, EVBoRDoorHandleSide::None);
	Part->Release = EVBoRAxisRelease::Stay; P2eCheckButton(*this, Panel); W.Play(); Logs.Flush();
	TestNull(TEXT("No auto connection"), Panel->OutsideAxisHandle.Get()); TestNull(TEXT("Part not linked"), FP2eTestAccess::Linked(Part));
	TestEqual(TEXT("Independent release behavior unchanged"), Part->Release, EVBoRAxisRelease::Stay); TestEqual(TEXT("No setup warning"), Logs.Warnings.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP2eMock, "vBoR.P2e.Assembly.E12MockUnchanged", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP2eMock::RunTest(const FString& Parameters)
{
	FP2eWorld W; FP2eLogs Logs; AP3VehicleMock* Car = W.World->SpawnActor<AP3VehicleMock>(); W.Play(); Logs.Flush();
	TestEqual(TEXT("No new mock warning"), Logs.Warnings.Num(), 0); TestEqual(TEXT("One glovebox attachment"), Logs.Attachments.Num(), 1);
	TestTrue(TEXT("Only glovebox handle attachment is logged"), Logs.Attachments.Num() == 1 && Logs.Attachments[0].Contains(FString::Printf(TEXT("handle=%s to HingePivot"), *Car->GloveboxHandle->GetName())));
	TestTrue(TEXT("Door handles retain actor origin"), Car->OutsideAxisHandle->GetActorTransform().Equals(Car->GetActorTransform(), 0.001) && Car->InsideAxisHandle->GetActorTransform().Equals(Car->GetActorTransform(), 0.001));
	TestTrue(TEXT("Door handles keep hinge parent"), Car->OutsideAxisHandle->GetRootComponent()->GetAttachParent() == Car->Door->HingePivot && Car->InsideAxisHandle->GetRootComponent()->GetAttachParent() == Car->Door->HingePivot);
	TestTrue(TEXT("Door explicit links remain"), Car->Door->OutsideAxisHandle == Car->OutsideAxisHandle && Car->Door->InsideAxisHandle == Car->InsideAxisHandle);
	TestTrue(TEXT("Glovebox handle link remains"), Car->Glovebox->OutsideAxisHandle == Car->GloveboxHandle);
	TestTrue(TEXT("Glovebox handle keeps hinge parent and actor origin"), Car->GloveboxHandle->GetRootComponent()->GetAttachParent() == Car->Glovebox->HingePivot && Car->GloveboxHandle->GetActorTransform().Equals(Car->GetActorTransform(), 0.001));
	TestNull(TEXT("Console keeps grab area"), Car->ConsoleLid->OutsideAxisHandle.Get());
	for (AVBoRHingedPanel* Panel : {static_cast<AVBoRHingedPanel*>(Car->Door.Get()), Car->Glovebox.Get(), Car->ConsoleLid.Get()})
	{
		TestFalse(TEXT("Box option remains off"), Panel->bUsePushFaceBox); TestTrue(TEXT("Mock setup clean"), FP2eTestAccess::Problems(Panel).IsEmpty());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP2eOpenSet, "vBoR.P2e.Assembly.E13OpenSetAxisHandle", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP2eOpenSet::RunTest(const FString& Parameters)
{
	FP2eWorld W; FP2eLogs Logs; AVBoRHingedPanel* Panel = W.World->SpawnActor<AVBoRHingedPanel>(); W.Play(); FP2eTestAccess::Angle(Panel, 30);
	AVBoRAxisPart* Part = P2eHandle(W.World, EVBoRDoorHandleSide::Outside); const FTransform Before = Part->GetActorTransform();
	Panel->OutsideAxisHandle = Part; P2eCheckButton(*this, Panel); Panel->SetAxisHandle(0, Part); Logs.Flush();
	TestTrue(TEXT("Open setter links and keeps world pose"), Panel->OutsideAxisHandle == Part && FP2eTestAccess::Linked(Part) == Panel && Part->GetActorTransform().Equals(Before, 0.0001));
	Panel->SetAxisHandle(0, Part); Logs.Flush();
	TestEqual(TEXT("One open-attachment warning"), Logs.Warnings.Num(), 1); TestEqual(TEXT("Attached once"), Logs.Attachments.Num(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP2eDefaultFace, "vBoR.P2e.PushFace.E14DefaultBounds", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP2eDefaultFace::RunTest(const FString& Parameters)
{
	FP2eWorld W; AVBoRHingedPanel* Panel = W.World->SpawnActor<AVBoRHingedPanel>();
	Panel->DoorMesh->SetRelativeTransform(FTransform(FRotator(0,0,90), FVector(30,2,50), FVector(0.4,0.6,0.03))); Panel->MeshRestFromRoot = Panel->DoorMesh->GetRelativeTransform();
	Panel->OnConstruction(Panel->GetActorTransform()); const FBox Bounds = Panel->DoorMesh->GetStaticMesh()->GetBoundingBox().TransformBy(Panel->DoorMesh->GetRelativeTransform());
	TestTrue(TEXT("Construction box fits existing mesh formula"), Panel->PushFaceBox->GetRelativeLocation().Equals(Bounds.GetCenter()) && Panel->PushFaceBox->GetUnscaledBoxExtent().Equals(Bounds.GetExtent()));
	TestTrue(TEXT("Construction box has identity rotation/scale"), Panel->PushFaceBox->GetRelativeRotation().IsNearlyZero() && Panel->PushFaceBox->GetRelativeScale3D().Equals(FVector::OneVector));
	TestTrue(TEXT("Existing face min/max unchanged"), Panel->GetMotion().FaceMinCm.Equals(Bounds.Min) && Panel->GetMotion().FaceMaxCm.Equals(Bounds.Max));
	TestEqual(TEXT("Existing thickness unchanged"), Panel->GetMotion().ThicknessCm, Bounds.GetSize().Y);
	TestTrue(TEXT("Box hidden in game and visible in editor"), Panel->PushFaceBox->bHiddenInGame && Panel->PushFaceBox->GetVisibleFlag());
	TestEqual(TEXT("Box has no collision"), Panel->PushFaceBox->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
	const FTransform BoxBefore = Panel->PushFaceBox->GetRelativeTransform(); const FVector ExtentBefore = Panel->PushFaceBox->GetUnscaledBoxExtent();
	Panel->bUsePushFaceBox = true; Panel->OnConstruction(Panel->GetActorTransform());
	TestTrue(TEXT("Enabling box starts from fitted bounds"), Panel->PushFaceBox->GetRelativeTransform().Equals(BoxBefore) && Panel->PushFaceBox->GetUnscaledBoxExtent().Equals(ExtentBefore));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP2eBoxPush, "vBoR.P2e.PushFace.E15ThinBoxPush", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP2eBoxPush::RunTest(const FString& Parameters)
{
	FP2eWorld W; AP2cTrackedPanel* Panel = W.World->SpawnActor<AP2cTrackedPanel>();
	Panel->bUsePushFaceBox = true; Panel->PushFaceBox->SetRelativeLocation(FVector(70,0,50)); Panel->PushFaceBox->SetBoxExtent(FVector(10,1,10)); Panel->PushFaceBox->SetRelativeScale3D(FVector(2,0.5,1.5));
	Panel->OnConstruction(Panel->GetActorTransform()); P2eCheckButton(*this, Panel); W.Play();
	TestTrue(TEXT("Box range includes relative scale"), Panel->GetMotion().FaceMinCm.Equals(FVector(50,-0.5,35)) && Panel->GetMotion().FaceMaxCm.Equals(FVector(90,0.5,65))); TestEqual(TEXT("Thin box thickness"), Panel->GetMotion().ThicknessCm, 1.0);
	Panel->Hands[0] = P2eHand(FVector(30,-6,50)); Panel->Tick(0.1f); Panel->Hands[0] = P2eHand(FVector(30,1,50)); Panel->Tick(0.1f);
	TestEqual(TEXT("Hand inside mesh but outside box cannot push"), Panel->GetMotion().AngleDeg, 0.0);
	Panel->Hands[0] = P2eHand(FVector(70,-1,50)); Panel->Tick(0.1f); Panel->Hands[0] = P2eHand(FVector(70,1,50)); Panel->Tick(0.1f);
	TestTrue(TEXT("Crossing thin box face opens panel"), Panel->GetMotion().AngleDeg > 0);
	TestEqual(TEXT("One push episode"), Panel->GetMotion().Metrics.PushStarts[0][1], 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP2eRotatedBox, "vBoR.P2e.PushFace.E16RotatedBoxFallback", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP2eRotatedBox::RunTest(const FString& Parameters)
{
	FP2eWorld W; FP2eLogs Logs; AVBoRHingedPanel* Panel = W.World->SpawnActor<AVBoRHingedPanel>();
	Panel->bUsePushFaceBox = true; Panel->PushFaceBox->SetRelativeRotation(FRotator(0,15,0)); Panel->PushFaceBox->SetBoxExtent(FVector(10,1,10)); Panel->OnConstruction(Panel->GetActorTransform());
	const FBox Bounds = Panel->DoorMesh->GetStaticMesh()->GetBoundingBox().TransformBy(Panel->DoorMesh->GetRelativeTransform());
	TestEqual(TEXT("Construction does not warn"), Logs.Warnings.Num(), 0); P2eCheckButton(*this, Panel); TestEqual(TEXT("One rotation problem"), FP2eTestAccess::Problems(Panel).Num(), 1); W.Play(); Panel->Tick(0); Panel->Tick(0); Logs.Flush();
	TestTrue(TEXT("Rotated box falls back to mesh"), Panel->GetMotion().FaceMinCm.Equals(Bounds.Min) && Panel->GetMotion().FaceMaxCm.Equals(Bounds.Max)); TestEqual(TEXT("Fallback thickness"), Panel->GetMotion().ThicknessCm, Bounds.GetSize().Y); TestEqual(TEXT("E20 no per-tick duplicate warning"), Logs.Warnings.Num(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP2eMeshOffset, "vBoR.P2e.Setup.E17MeshRestWarning", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP2eMeshOffset::RunTest(const FString& Parameters)
{
	for (int32 Mode = 0; Mode < 3; ++Mode)
	{
		FP2eWorld W; FP2eLogs Logs; AVBoRHingedPanel* Panel = W.World->SpawnActor<AVBoRHingedPanel>();
		if (Mode > 0) { Panel->DoorMesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"))); }
		if (Mode == 2) { Panel->MeshRestFromRoot = FTransform::Identity; Panel->bUsePushFaceBox = true; }
		Panel->OnConstruction(Panel->GetActorTransform()); P2eCheckButton(*this, Panel);
		TestEqual(TEXT("Only non-cube with mock default warns"), FP2eTestAccess::Problems(Panel).Num(), Mode == 1 ? 1 : 0); W.Play(); Logs.Flush(); TestEqual(TEXT("E20 warning matches preflight"), Logs.Warnings.Num(), Mode == 1 ? 1 : 0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP2eThickFace, "vBoR.P2e.Setup.E18ThicknessWarning", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP2eThickFace::RunTest(const FString& Parameters)
{
	for (bool bBox : {false, true})
	{
		FP2eWorld W; FP2eLogs Logs; AVBoRHingedPanel* Panel = W.World->SpawnActor<AVBoRHingedPanel>();
		Panel->MeshRestFromRoot.SetScale3D(FVector(1.1,0.25,1.05)); Panel->bUsePushFaceBox = bBox; Panel->OnConstruction(Panel->GetActorTransform());
		P2eCheckButton(*this, Panel); TestEqual(TEXT("Thick bounds warning only when box disabled"), FP2eTestAccess::Problems(Panel).Num(), bBox ? 0 : 1);
		W.Play(); Logs.Flush(); TestEqual(TEXT("E20 thickness warning once"), Logs.Warnings.Num(), bBox ? 0 : 1);
		// Setter never repeats thickness issue unrelated to new connection (P2e review note/E20)
		AVBoRAxisPart* Part = P2eHandle(W.World, EVBoRDoorHandleSide::Outside);
		Panel->SetAxisHandle(0, Part); Panel->SetAxisHandle(0, Part); Panel->SetAxisHandle(1, nullptr); Logs.Flush();
		TestEqual(TEXT("Setters do not repeat unrelated setup warnings"), Logs.Warnings.Num(), bBox ? 0 : 1);
	}
	return true;
}
#endif
