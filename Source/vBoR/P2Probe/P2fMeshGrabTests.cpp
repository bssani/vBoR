#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "P2bTestFixture.h"
#include "../Interaction/VBoRSteeringWheel.h"
#include "../Interaction/VBoRTwoAxisPart.h"
#include "../Interaction/VBoRKnob.h"
#include "../Interaction/VBoRCarryObject.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "GameFramework/WorldSettings.h"
#include "PhysicsEngine/BodySetup.h"
#include "Interaction/IsdkGrabbableComponent.h"
#include "Interaction/IsdkGrabberComponent.h"
#include "Interaction/GrabDetectors/IsdkHandGrabDetector.h"
#include "IsdkRuntimeSettings.h"
#include "Misc/OutputDevice.h"
#include "Misc/OutputDeviceRedirector.h"

namespace
{
	struct FP2fWorld
	{
		UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
		void Play() { World->InitializeActorsForPlay(FURL()); World->GetWorldSettings()->NotifyBeginPlay(); }
		~FP2fWorld() { World->EndPlay(EEndPlayReason::Quit); World->DestroyWorld(false); }
	};
	struct FP2fPart
	{
		AActor* Actor;
		UStaticMeshComponent* Mesh;
		UBoxComponent* Box;
		UIsdkGrabbableComponent* Grabbable;
		bool* Option;
		FTransform* Rest;
	};
	TArray<FP2fPart> P2fParts(UWorld* World)
	{
		auto* Axis = World->SpawnActor<AP2bTrackedPart>();
		auto* Wheel = World->SpawnActor<AVBoRSteeringWheel>();
		auto* Mirror = World->SpawnActor<AVBoRTwoAxisPart>();
		auto* Knob = World->SpawnActor<AVBoRKnob>();
		return {
			{Axis, Axis->PartMesh, Axis->GrabBox, Axis->Grabbable, &Axis->bGrabWithMesh, &Axis->MeshRestFromRoot},
			{Wheel, Wheel->WheelMesh, Wheel->GrabBox, Wheel->Grabbable, &Wheel->bGrabWithMesh, &Wheel->MeshRestFromRoot},
			{Mirror, Mirror->PartMesh, Mirror->GrabBox, Mirror->Grabbable, &Mirror->bGrabWithMesh, &Mirror->MeshRestFromRoot},
			{Knob, Knob->KnobMesh, Knob->GrabBox, Knob->Grabbable, &Knob->bGrabWithMesh, &Knob->MeshRestFromRoot}
		};
	}
	void P2fLayout(TArray<FP2fPart>& Parts, bool bMesh)
	{
		for (int32 I = 0; I < Parts.Num(); ++I)
		{
			FP2fPart& Part = Parts[I];
			*Part.Option = bMesh;
			Part.Actor->SetActorLocation(FVector(I * 400, 0, 0));
			*Part.Rest = FTransform(FQuat::Identity, FVector(20,0,80), FVector(0.1));
			Part.Mesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
			Part.Actor->OnConstruction(Part.Actor->GetActorTransform());
		}
	}
	UIsdkGrabberComponent* P2fGrabber(UWorld* World)
	{
		AActor* Actor = World->SpawnActor<AActor>();
		auto* Grabber = NewObject<UIsdkGrabberComponent>(Actor);
		Actor->SetRootComponent(Grabber);
		Grabber->InteractorTags.AddTag(IsdkGameplayTags::TAG_Isdk_Type_Device_Left);
		Grabber->SetWorldLocation(FVector(3000));
		Grabber->SetGrabDetectionTypeAllowed(EIsdkGrabDetectorType::DistanceGrab, false);
		Grabber->SetGrabDetectionTypeAllowed(EIsdkGrabDetectorType::RayGrab, false);
		Grabber->SetGrabInputMethodAllowed(EIsdkGrabInputMethod::Palm, true);
		Grabber->SetGrabInputMethodAllowed(EIsdkGrabInputMethod::Pinch, false);
		Grabber->RegisterComponent();
		return Grabber;
	}
	void P2fMove(UIsdkGrabberComponent* Grabber, const FVector& Position)
	{
		Grabber->SetWorldLocation(Position);
		Grabber->TickComponent(0.016f, LEVELTICK_All, nullptr);
	}
	bool P2fHover(UIsdkGrabberComponent* Grabber, UIsdkGrabbableComponent* Part)
	{
		return Grabber->GetHandGrabDetector()->GetHoveredGrabbables().Contains(Part);
	}
	struct FP2fLogs : FOutputDevice
	{
		TArray<FString> Lines;
		FP2fLogs() { GLog->FlushThreadedLogs(); GLog->AddOutputDevice(this); }
		~FP2fLogs() { GLog->RemoveOutputDevice(this); }
		void Serialize(const TCHAR* Message, ELogVerbosity::Type Verbosity, const FName& Category) override
		{
			if (Category == TEXT("LogVBoRGrabCollider")) { Lines.Add(Message); }
		}
		int32 Count(const FString& Text)
		{
			GLog->FlushThreadedLogs();
			return Lines.FilterByPredicate([&](const FString& Line) { return Line.Contains(Text); }).Num();
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP2fDefaults, "vBoR.P2f.E1Defaults", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP2fDefaults::RunTest(const FString& Parameters)
{
	FP2fWorld W; const auto Parts = P2fParts(W.World); W.Play();
	for (const FP2fPart& Part : Parts)
	{
		TestFalse(TEXT("Mesh grabbing defaults off"), *Part.Option);
		TestTrue(TEXT("Default collider remains GrabBox"), Part.Grabbable->GetGrabCollider() == Part.Box);
		TestEqual(TEXT("Default visible mesh has no collision"), Part.Mesh->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP2fEnabled, "vBoR.P2f.E2MeshCollision", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP2fEnabled::RunTest(const FString& Parameters)
{
	FP2fWorld W; auto Parts = P2fParts(W.World); P2fLayout(Parts, true);
	TArray<FTransform> Poses; TArray<UMaterialInterface*> Materials;
	for (const auto& Part : Parts) { Poses.Add(Part.Mesh->GetComponentTransform()); Materials.Add(Part.Mesh->GetMaterial(0)); }
	W.Play();
	for (int32 I = 0; I < Parts.Num(); ++I)
	{
		const auto& Part = Parts[I];
		TestTrue(TEXT("ISDK collider is mesh"), Part.Grabbable->GetGrabCollider() == Part.Mesh);
		TestEqual(TEXT("Mesh is query only"), Part.Mesh->GetCollisionEnabled(), ECollisionEnabled::QueryOnly);
		TestEqual(TEXT("Mesh object is WorldDynamic"), Part.Mesh->GetCollisionObjectType(), ECC_WorldDynamic);
		for (int32 Channel = 0; Channel < ECC_MAX; ++Channel)
		{
			TestEqual(TEXT("Only WorldDynamic overlaps; all other channels ignored"), Part.Mesh->GetCollisionResponseToChannel(static_cast<ECollisionChannel>(Channel)), Channel == ECC_WorldDynamic ? ECR_Overlap : ECR_Ignore);
		}
		TestTrue(TEXT("Mesh generates overlaps"), Part.Mesh->GetGenerateOverlapEvents());
		TestEqual(TEXT("GrabBox collision disabled"), Part.Box->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
		TestFalse(TEXT("GrabBox overlaps disabled"), Part.Box->GetGenerateOverlapEvents());
		TestTrue(TEXT("Visible pose and material retained"), Part.Mesh->GetComponentTransform().Equals(Poses[I]) && Part.Mesh->GetMaterial(0) == Materials[I]);
		TestFalse(TEXT("No physics simulation"), Part.Mesh->IsSimulatingPhysics());
		*Part.Option = false;
		Part.Actor->Tick(0);
		TestTrue(TEXT("Runtime option change does not reconfigure collider"), Part.Grabbable->GetGrabCollider() == Part.Mesh);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP2fSurface, "vBoR.P2f.E3RealPalmSurface", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP2fSurface::RunTest(const FString& Parameters)
{
	for (bool bMesh : {false, true})
	{
		FP2fWorld W; auto* Part = W.World->SpawnActor<AP2bTrackedPart>();
		Part->bGrabWithMesh = bMesh;
		Part->PartMesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere")));
		Part->MeshRestFromRoot = FTransform(FQuat::Identity, FVector(0,0,80), FVector(0.4));
		Part->OnConstruction(Part->GetActorTransform());
		auto* Grabber = P2fGrabber(W.World); W.Play(); Grabber->Activate(true);
		// 20 cm radius sphere bounds corner. 4 cm palm sphere also misses surface (P2f E3)
		P2fMove(Grabber, FVector(18,18,98));
		TestEqual(TEXT("Box corner hover only with default box"), P2fHover(Grabber, Part->Grabbable), !bMesh);
		P2fMove(Grabber, FVector(20,0,80));
		TestTrue(TEXT("Surface contact hovers"), P2fHover(Grabber, Part->Grabbable));
		Grabber->PalmGrab();
		TestTrue(TEXT("Real PalmGrab selects sphere"), Grabber->GetGrabbedComponent() == Part->Grabbable);
		Grabber->PalmRelease();
	}
	FP2fWorld W; auto Parts = P2fParts(W.World); P2fLayout(Parts, true);
	auto* Grabber = P2fGrabber(W.World); W.Play(); Grabber->Activate(true);
	for (const auto& Part : Parts)
	{
		P2fMove(Grabber, Part.Mesh->GetComponentLocation() + FVector(5,0,0));
		TestTrue(TEXT("Every parent hovers at mesh surface"), P2fHover(Grabber, Part.Grabbable));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP2fNearest, "vBoR.P2f.E4NearestPivot", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP2fNearest::RunTest(const FString& Parameters)
{
	FP2fWorld W;
	AP2bTrackedPart* Parts[2];
	for (int32 I = 0; I < 2; ++I)
	{
		Parts[I] = W.World->SpawnActor<AP2bTrackedPart>();
		Parts[I]->bGrabWithMesh = true;
		Parts[I]->MeshRestFromRoot = FTransform(FQuat::Identity, FVector(I * 10,0,80), FVector(0.1));
		Parts[I]->OnConstruction(Parts[I]->GetActorTransform());
	}
	auto* Grabber = P2fGrabber(W.World); W.Play(); Grabber->Activate(true);
	TestTrue(TEXT("Both actors share origin"), Parts[0]->GetActorLocation().Equals(Parts[1]->GetActorLocation()));
	for (int32 I = 0; I < 2; ++I)
	{
		P2fMove(Grabber, FVector(I == 0 ? 4 : 6,0,80));
		TestTrue(TEXT("Palm overlaps both candidates"), P2fHover(Grabber, Parts[0]->Grabbable) && P2fHover(Grabber, Parts[1]->Grabbable));
		Grabber->PalmGrab();
		TestTrue(TEXT("Nearest mesh origin wins"), Grabber->GetGrabbedComponent() == Parts[I]->Grabbable);
		Grabber->PalmRelease();
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP2fFallback, "vBoR.P2f.E5MissingCollision", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP2fFallback::RunTest(const FString& Parameters)
{
	FP2fWorld W; auto Parts = P2fParts(W.World); P2fLayout(Parts, true);
	for (auto& Part : Parts)
	{
		auto* Mesh = DuplicateObject<UStaticMesh>(Part.Mesh->GetStaticMesh(), W.World);
		Mesh->SetBodySetup(NewObject<UBodySetup>(Mesh)); Part.Mesh->SetStaticMesh(Mesh);
	}
	AddExpectedMessagePlain(TEXT("grab mesh has no simple collision"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 4);
	FP2fLogs Logs; auto* Grabber = P2fGrabber(W.World); W.Play(); Grabber->Activate(true);
	for (const auto& Part : Parts)
	{
		TestTrue(TEXT("No collision falls back to box"), Part.Grabbable->GetGrabCollider() == Part.Box);
		TestEqual(TEXT("Fallback mesh stays collision free"), Part.Mesh->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
		P2fMove(Grabber, Part.Box->GetComponentLocation());
		TestTrue(TEXT("Fallback still hovers through box"), P2fHover(Grabber, Part.Grabbable));
	}
	P2fMove(Grabber, Parts[0].Box->GetComponentLocation()); Grabber->PalmGrab();
	TestTrue(TEXT("Fallback box can be selected"), Grabber->GetGrabbedComponent() == Parts[0].Grabbable); Grabber->PalmRelease();
	for (const auto& Part : Parts) { Part.Actor->Tick(0); }
	TestEqual(TEXT("One fallback warning per parent"), Logs.Count(TEXT("grab mesh has no simple collision")), 4);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP2fComplex, "vBoR.P2f.E6ComplexAsSimple", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP2fComplex::RunTest(const FString& Parameters)
{
	FP2fWorld W; auto* Part = W.World->SpawnActor<AP2bTrackedPart>(); Part->bGrabWithMesh = true;
	Part->MeshRestFromRoot = FTransform(FQuat::Identity, FVector(0,0,80), FVector(0.2));
	// Triangle-only collision without changing source asset (P2f E6)
	auto* Mesh = DuplicateObject<UStaticMesh>(Part->PartMesh->GetStaticMesh(), W.World);
	auto* Body = NewObject<UBodySetup>(Mesh); Body->CollisionTraceFlag = CTF_UseComplexAsSimple; Mesh->SetBodySetup(Body);
	Body->CreatePhysicsMeshes(); Part->PartMesh->SetStaticMesh(Mesh); Part->OnConstruction(Part->GetActorTransform());
	FP2fLogs Logs; auto* Grabber = P2fGrabber(W.World); W.Play(); Grabber->Activate(true);
	TestEqual(TEXT("Complex fixture has no simple elements"), Body->AggGeom.GetElementCount(), 0);
	TestTrue(TEXT("Complex-as-simple uses mesh collider"), Part->Grabbable->GetGrabCollider() == Part->PartMesh);
	P2fMove(Grabber, FVector(10,0,80));
	TestTrue(TEXT("Real palm sphere hovers on triangle collision"), P2fHover(Grabber, Part->Grabbable));
	Grabber->PalmGrab(); TestTrue(TEXT("Complex surface can be selected"), Grabber->GetGrabbedComponent() == Part->Grabbable); Grabber->PalmRelease();
	TestEqual(TEXT("Complex collision needs no fallback warning"), Logs.Count(TEXT("grab mesh has no simple collision")), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP2fPivotWarning, "vBoR.P2f.E7PivotWarning", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP2fPivotWarning::RunTest(const FString& Parameters)
{
	for (double Distance : {0.0, 1.0, 1.01})
	{
		FP2fWorld W; auto Parts = P2fParts(W.World); P2fLayout(Parts, true);
		for (auto& Part : Parts) { Part.Rest->SetLocation(FVector(Distance,0,0)); Part.Actor->OnConstruction(Part.Actor->GetActorTransform()); }
		if (Distance <= 1) { AddExpectedMessagePlain(TEXT("grab mesh pivot is at the actor origin"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 4); }
		FP2fLogs Logs; W.Play();
		for (const auto& Part : Parts)
		{
			TestTrue(TEXT("Pivot warning does not block grabbing"), Part.Grabbable->GetGrabCollider() == Part.Mesh && Part.Grabbable->IsGrabInputMethodAllowed(EIsdkGrabInputMethod::Palm));
			Part.Actor->Tick(0); Part.Actor->Tick(0);
		}
		TestEqual(TEXT("One warning per enabled parent within 1 cm"), Logs.Count(TEXT("grab mesh pivot is at the actor origin")), Distance <= 1 ? 4 : 0);
	}
	FP2fWorld W; P2fParts(W.World); FP2fLogs Logs; W.Play();
	TestEqual(TEXT("Option off never warns about pivot"), Logs.Count(TEXT("grab mesh pivot is at the actor origin")), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP2fDisabled, "vBoR.P2f.E8InvalidSettings", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP2fDisabled::RunTest(const FString& Parameters)
{
	for (bool bMissingMesh : {true, false})
	{
		FP2fWorld W; auto Parts = P2fParts(W.World); P2fLayout(Parts, true);
		if (bMissingMesh)
		{
			AddExpectedMessagePlain(TEXT("No part mesh; grabbing disabled"), ELogVerbosity::Warning);
			Parts[0].Mesh->SetStaticMesh(nullptr);
			Parts[0].Actor->OnConstruction(Parts[0].Actor->GetActorTransform());
			AddExpectedMessagePlain(TEXT("grab mesh has no simple collision"), ELogVerbosity::Warning);
			Parts.SetNum(1);
		}
		else
		{
			auto* Axis = CastChecked<AVBoRAxisPart>(Parts[0].Actor); Axis->Release = EVBoRAxisRelease::Detent; Axis->DetentAnglesDeg.Reset();
			CastChecked<AVBoRSteeringWheel>(Parts[1].Actor)->LimitAngleDeg = 0;
			CastChecked<AVBoRTwoAxisPart>(Parts[2].Actor)->YawLimitDeg = -1;
			CastChecked<AVBoRKnob>(Parts[3].Actor)->TwistGain = 0;
			AddExpectedMessagePlain(TEXT("Invalid detent:"), ELogVerbosity::Warning);
			AddExpectedMessagePlain(TEXT("Missing mesh or invalid wheel"), ELogVerbosity::Warning);
			AddExpectedMessagePlain(TEXT("Missing mesh or invalid two-axis"), ELogVerbosity::Warning);
			AddExpectedMessagePlain(TEXT("Missing mesh or invalid knob"), ELogVerbosity::Warning);
		}
		auto* Grabber = P2fGrabber(W.World); W.Play(); Grabber->Activate(true);
		for (const auto& Part : Parts)
		{
			TestEqual(TEXT("Invalid mesh collider is disabled"), Part.Mesh->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
			TestFalse(TEXT("Invalid mesh overlap events disabled"), Part.Mesh->GetGenerateOverlapEvents());
			TestEqual(TEXT("Invalid box collision also disabled"), Part.Box->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
			TestFalse(TEXT("Palm input disabled"), Part.Grabbable->IsGrabInputMethodAllowed(EIsdkGrabInputMethod::Palm));
			P2fMove(Grabber, Part.Mesh->GetComponentLocation()); Grabber->PalmGrab(); TestFalse(TEXT("Palm cannot select invalid part"), Grabber->IsGrabbing());
			TestTrue(TEXT("Visible part remains visible"), Part.Mesh->IsVisible());
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP2fMoving, "vBoR.P2f.E9MovingMesh", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP2fMoving::RunTest(const FString& Parameters)
{
	FP2fWorld W; auto* Part = W.World->SpawnActor<AP2bTrackedPart>();
	Part->bGrabWithMesh = true; Part->MeshRestFromRoot = FTransform(FQuat::Identity, FVector(80,0,80), FVector(0.1));
	Part->OnConstruction(Part->GetActorTransform());
	auto* Door = W.World->SpawnActor<AP2bTrackedDoor>(); Door->SetActorLocation(FVector(400,0,0));
	Door->bUseOutsideGrabArea = Door->bUseInsideGrabArea = false;
	auto* Handle = W.World->SpawnActor<AP2bTrackedPart>();
	Handle->bGrabWithMesh = true; Handle->DoorHandleSide = EVBoRDoorHandleSide::Outside;
	Handle->SetActorLocation(Door->GetActorLocation());
	Handle->MeshRestFromRoot = FTransform(FQuat::Identity, FVector(80,10,80), FVector(0.1));
	Handle->OnConstruction(Handle->GetActorTransform());
	Handle->AttachToComponent(Door->GetRootComponent(), FAttachmentTransformRules::KeepWorldTransform);
	auto* Grabber = P2fGrabber(W.World); W.Play(); Grabber->Activate(true);
	TestTrue(TEXT("P2e attaches child handle under hinge"), Handle->GetRootComponent()->GetAttachParent() == Door->HingePivot);
	const FVector OldPart = Part->PartMesh->GetComponentLocation(), OldHandle = Handle->PartMesh->GetComponentLocation();
	Part->AxisPivot->SetRelativeRotation(FRotator(0,30,0)); Door->HingePivot->SetRelativeRotation(FRotator(0,40,0));
	P2fMove(Grabber, OldPart); TestFalse(TEXT("No hover at old axis mesh position"), P2fHover(Grabber, Part->Grabbable));
	P2fMove(Grabber, Part->PartMesh->GetComponentLocation()); TestTrue(TEXT("Hover follows axis rotated 30 deg"), P2fHover(Grabber, Part->Grabbable));
	P2fMove(Grabber, OldHandle); TestFalse(TEXT("No hover at closed handle position"), P2fHover(Grabber, Handle->Grabbable));
	P2fMove(Grabber, Handle->PartMesh->GetComponentLocation()); TestTrue(TEXT("Hover follows door opened 40 deg"), P2fHover(Grabber, Handle->Grabbable));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP2fCarryOverlap, "vBoR.P2f.E10NoCarryBlocking", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP2fCarryOverlap::RunTest(const FString& Parameters)
{
	FP2fWorld W; auto Parts = P2fParts(W.World); P2fLayout(Parts, true);
	auto* Carry = W.World->SpawnActor<AVBoRCarryObject>(); Carry->SetActorLocation(FVector(3000)); W.Play();
	for (const auto& Part : Parts)
	{
		TArray<FOverlapResult> Hits;
		FComponentQueryParams Params(SCENE_QUERY_STAT(P2fCarry), Carry);
		W.World->ComponentOverlapMulti(Hits, Carry->Mesh, Part.Mesh->GetComponentLocation(), FQuat::Identity, Params);
		bool bFoundMesh = false, bBlocked = false;
		for (const auto& Hit : Hits) { bFoundMesh |= Hit.GetComponent() == Part.Mesh; bBlocked |= Hit.bBlockingHit; }
		TestTrue(TEXT("Carry query reaches enabled mesh"), bFoundMesh);
		TestFalse(TEXT("Enabled mesh does not block carry query"), bBlocked);
	}
	return true;
}
#endif
