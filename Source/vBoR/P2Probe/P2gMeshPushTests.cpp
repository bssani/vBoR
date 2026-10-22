#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "P2cTestFixture.h"
#include "Components/BoxComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "GameFramework/WorldSettings.h"
#include "PhysicsEngine/BodySetup.h"

struct FP2gTestAccess
{
	static FVBoRDoorMotion& Motion(AVBoRHingedPanel* Panel) { return Panel->Motion; }
	static bool Face(const AVBoRHingedPanel* Panel, const FVector& Point, int32 Side, double& Y) { return Panel->QueryMeshFace(Point, Side, Y); }
	static void Angle(AVBoRHingedPanel* Panel, double Value)
	{
		Panel->Motion.AngleDeg = Value;
		Panel->ApplyState(0, Panel->Motion.Metrics.Unlocks, Panel->Motion.Metrics.Latches);
	}
	static bool Traceable(const AVBoRHingedPanel* Panel) { return Panel->EvaluateSetup().bMeshTraceable; }
};

namespace
{
	struct FP2gWorld
	{
		UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
		void Play() { World->InitializeActorsForPlay(FURL()); World->GetWorldSettings()->NotifyBeginPlay(); }
		~FP2gWorld() { World->EndPlay(EEndPlayReason::Quit); World->DestroyWorld(false); }
	};
	FVector P2gRotate(double Angle, const FVector& Point)
	{
		return FQuat(FVector::UpVector, FMath::DegreesToRadians(Angle)).RotateVector(Point);
	}
	FVBoRDoorHand P2gHand(const FVector& Point, const FVector& Normal = FVector(0,-1,0))
	{
		FVBoRDoorHand Hand;
		Hand.bTracked = true;
		for (FVector& P : Hand.Points) { P = Point; }
		Hand.PalmNormal = Normal;
		return Hand;
	}
	AP2cTrackedPanel* P2gPanel(UWorld* World, bool bMesh = true, double Tilt = 0, bool bSphere = false)
	{
		auto* Panel = World->SpawnActor<AP2cTrackedPanel>();
		Panel->bPushOnMeshSurface = bMesh;
		// Sphere BodyInstance ties XYZ on scale update (isotropic scale in E4, P2g §5)
		Panel->MeshRestFromRoot = FTransform(FRotator(0,Tilt,0), FVector(55,0,52.5), bSphere ? FVector(1) : FVector(1.1,0.1,1.05));
		if (bSphere) { Panel->DoorMesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"))); }
		Panel->OnConstruction(Panel->GetActorTransform());
		return Panel;
	}
	void P2gInput(AP2cTrackedPanel* Panel, double ReferenceAngle, FVector Point, FVector Normal = FVector(0,-1,0), int32 Hand = 0)
	{
		Panel->Hands[Hand] = P2gHand(P2gRotate(ReferenceAngle, Point), P2gRotate(ReferenceAngle, Normal));
		Panel->Tick(0.016f);
	}
	void P2gCompare(FAutomationTestBase& Test, FVBoRDoorMotion Plain, const TArray<FVBoRDoorHand>& Frames)
	{
		FVBoRDoorMotion Queried = Plain;
		Queried.QueryFace = [&Plain](const FVector& P, int32 Face, double& Y)
		{
			Y = Face == 0 ? Plain.FaceMaxCm.Y : Plain.FaceMinCm.Y;
			return P.X >= Plain.FaceMinCm.X && P.X <= Plain.FaceMaxCm.X && P.Z >= Plain.FaceMinCm.Z && P.Z <= Plain.FaceMaxCm.Z;
		};
		for (const FVBoRDoorHand& Input : Frames)
		{
			FVBoRDoorHand Hands[2] = {Input, {}};
			Plain.Advance(Hands, 0.1); Queried.Advance(Hands, 0.1);
			Test.TestTrue(TEXT("E1 plane callback preserves each frame (1e-6 deg)"), FMath::IsNearlyEqual(Plain.AngleDeg, Queried.AngleDeg, 1.e-6));
			Test.TestEqual(TEXT("E1 selected face preserved"), Plain.PushFace, Queried.PushFace);
		}
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP2gPlanes, "vBoR.P2g.E1.PlaneCallback", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP2gPlanes::RunTest(const FString& Parameters)
{
	FVBoRDoorMotion Door;
	Door.bLocked = false; Door.AngleDeg = 30;
	Door.FaceMinCm.Y = -20; Door.ThicknessCm = 25;
	P2gCompare(*this, Door, {P2gHand(P2gRotate(30,FVector(80,8,50)),P2gRotate(30,FVector(0,-1,0))),
		P2gHand(P2gRotate(30,FVector(80,0,50)),P2gRotate(30,FVector(0,-1,0)))});
	Door.Reset(); Door.bLocked = false;
	P2gCompare(*this, Door, {P2gHand(FVector(80,-23,50),FVector(0,1,0)),P2gHand(FVector(80,-12,50),FVector(0,1,0))});
	for (const FVector& Normal : {FVector(0,1,0), FVector(0,-1,0)})
	{
		P2gCompare(*this, Door, {P2gHand(FVector(80,-8,50), Normal),P2gHand(FVector(80,-8,50), Normal)});
	}
	P2gCompare(*this, Door, {P2gHand(FVector(80,8,50),FVector(0,1,0)),P2gHand(FVector(80,4,50),FVector(0,1,0))});
	Door = FVBoRDoorMotion(); Door.bLocked = false;
	FVBoRDoorHand Deep = P2gHand(FVector(80,-8,50),FVector(0,1,0)); Deep.Points[0].Y = 15;
	P2gCompare(*this, Door, {P2gHand(FVector(80,-8,50),FVector(0,1,0)), Deep,
		P2gHand(FVector(80,-20,50),FVector(0,1,0)),P2gHand(FVector(80,-20,50),FVector(0,1,0)),
		P2gHand(FVector(80,8,50)),P2gHand(FVector(80,-5,50))});
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP2gCube, "vBoR.P2g.E2.CubeEquivalence", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP2gCube::RunTest(const FString& Parameters)
{
	FP2gWorld W;
	AP2cTrackedPanel* Plain = P2gPanel(W.World, false); AP2cTrackedPanel* Mesh = P2gPanel(W.World);
	Mesh->SetActorLocation(FVector(400,0,0));
	W.Play();
	TestTrue(TEXT("Cube has traceable surface"), FP2gTestAccess::Traceable(Mesh));
	for (int32 Case = 0; Case < 4; ++Case)
	{
		Plain->ResetDoor(); Mesh->ResetDoor();
		const bool bInside = Case != 0;
		const double A = bInside ? 0 : 40;
		FP2gTestAccess::Angle(Plain,A); FP2gTestAccess::Angle(Mesh,A);
		const FVector Normal = Case == 2 ? FVector(1,0,0) : FVector(0,bInside ? 1 : -1,0);
		const double X = Case == 3 ? 5 : 80;
		for (int32 Frame = 0; Frame < 8; ++Frame)
		{
			const FVector P(X, (bInside ? -1 : 1) * (Frame == 0 ? 8 : -2),50);
			P2gInput(Plain,A,P,Normal); P2gInput(Mesh,A,P,Normal);
			TestTrue(TEXT("E2 same angle each frame (1e-3 deg)"), FMath::IsNearlyEqual(Plain->GetMotion().AngleDeg,Mesh->GetMotion().AngleDeg,1.e-3));
		}
		if (Case < 2) { TestTrue(TEXT("Cube actually moves"), FMath::Abs(Mesh->GetMotion().AngleDeg-A) > 0.1); }
		else { TestEqual(TEXT("Cube rejection reason retained"), Mesh->GetMotion().Metrics.Ignored[static_cast<int32>(Case == 2 ? EVBoRDoorIgnore::PalmDirection : EVBoRDoorIgnore::NearAxis)], 1); }
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP2gTilt, "vBoR.P2g.E3.TiltedFace", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP2gTilt::RunTest(const FString& Parameters)
{
	FP2gWorld W;
	AP2cTrackedPanel* Plain = P2gPanel(W.World,false,10); AP2cTrackedPanel* Mesh = P2gPanel(W.World,true,10);
	AddExpectedMessagePlain(TEXT("Mesh face thickness"),ELogVerbosity::Warning,EAutomationExpectedMessageFlags::Contains,1);
	Mesh->SetActorLocation(FVector(400,0,0)); W.Play();
	FP2gTestAccess::Angle(Plain,40); FP2gTestAccess::Angle(Mesh,40);
	double Y; TestTrue(TEXT("Tilted face hit"), FP2gTestAccess::Face(Mesh,FVector(80,0,50),0,Y));
	const double BoundsY = Mesh->GetMotion().FaceMaxCm.Y;
	TestTrue(TEXT("Bounds has empty space before actual face"), BoundsY-Y > 1);
	for (AP2cTrackedPanel* P : {Plain,Mesh})
	{
		P2gInput(P,40,FVector(80,BoundsY+2,50)); P2gInput(P,40,FVector(80,(BoundsY+Y)/2,50));
	}
	TestTrue(TEXT("Off pushes in bounds gap"), Plain->GetMotion().AngleDeg < 40);
	TestEqual(TEXT("On waits for actual face"), Mesh->GetMotion().AngleDeg,40.0);
	P2gInput(Mesh,40,FVector(80,Y-3,50));
	TestTrue(TEXT("Crossing actual face closes"), Mesh->GetMotion().AngleDeg < 40);
	for (int32 I=0; I<20; ++I) { Mesh->Tick(0.016f); }
	const FVector Point = Mesh->GetMotion().ToDoor(Mesh->Hands[0].Points[0]);
	TestTrue(TEXT("Final tilted face hit"), FP2gTestAccess::Face(Mesh,Point,0,Y));
	TestTrue(TEXT("E3 settles within 0.1 cm"), FMath::Abs(Point.Y-Y) < 0.1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP2gCurve, "vBoR.P2g.E4.CurvedConvergence", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP2gCurve::RunTest(const FString& Parameters)
{
	FP2gWorld W; AP2cTrackedPanel* Panel = P2gPanel(W.World,true,0,true); W.Play();
	FP2gTestAccess::Angle(Panel,40);
	double Y; TestTrue(TEXT("Sphere face hit"),FP2gTestAccess::Face(Panel,FVector(80,0,50),0,Y));
	P2gInput(Panel,40,FVector(80,Y+2,50));
	Panel->Hands[0] = P2gHand(P2gRotate(40,FVector(80,Y-6,50)),P2gRotate(40,FVector(0,-1,0)));
	FP2gTestAccess::Motion(Panel).MaxSpeedDegPerSecond = Panel->MaxSpeedDegPerSecond = 30;
	for (int32 Frame=0; Frame<60; ++Frame)
	{
		const double Before = Panel->GetMotion().AngleDeg; Panel->Tick(0.016f);
		const double After = Panel->GetMotion().AngleDeg;
		TestTrue(TEXT("E4 angle only closes, no oscillation"),After <= Before+1.e-6);
		TestTrue(TEXT("E4 speed cap 30 deg/s"),Before-After <= 30*0.016+1.e-5);
	}
	TestTrue(TEXT("E4 curve actually pushed"),Panel->GetMotion().AngleDeg < 39);
	TestTrue(TEXT("E4 cap exercised"),Panel->GetMotion().Metrics.RateLimitedFrames > 0);
	const FVector Point = Panel->GetMotion().ToDoor(Panel->Hands[0].Points[0]);
	TestTrue(TEXT("E4 final sphere hit"),FP2gTestAccess::Face(Panel,Point,0,Y));
	TestTrue(TEXT("E4 fixed hand settles within 0.5 cm of triangles"),FMath::Abs(Point.Y-Y) < 0.5);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP2gMiss, "vBoR.P2g.E5.SilhouetteMiss", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP2gMiss::RunTest(const FString& Parameters)
{
	FP2gWorld W; AP2cTrackedPanel* Panel = P2gPanel(W.World,true,0,true); W.Play();
	FP2gTestAccess::Angle(Panel,40);
	double Y; TestTrue(TEXT("Sphere interior is traceable"),FP2gTestAccess::Face(Panel,FVector(55,0,50),0,Y));
	TestFalse(TEXT("Sphere corner has no surface"),FP2gTestAccess::Face(Panel,FVector(105,0,100),0,Y));
	P2gInput(Panel,40,FVector(105,Y+2,100)); P2gInput(Panel,40,FVector(105,Y-2,100));
	for (int32 Frame=0; Frame<4; ++Frame) { Panel->Tick(0.016f); }
	TestEqual(TEXT("Miss never moves"),Panel->GetMotion().AngleDeg,40.0);
	TestEqual(TEXT("Outside once per contact"),Panel->GetMotion().Metrics.Ignored[static_cast<int32>(EVBoRDoorIgnore::Outside)],1);
	P2gInput(Panel,40,FVector(105,Y+2,100)); P2gInput(Panel,40,FVector(105,Y-2,100));
	TestEqual(TEXT("New contact counts again"),Panel->GetMotion().Metrics.Ignored[static_cast<int32>(EVBoRDoorIgnore::Outside)],2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP2gInside, "vBoR.P2g.E6.InsideFace", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP2gInside::RunTest(const FString& Parameters)
{
	FP2gWorld W; AP2cTrackedPanel* Panel = P2gPanel(W.World,true,10); W.Play();
	double Y; TestTrue(TEXT("Inside tilted face hit"),FP2gTestAccess::Face(Panel,FVector(80,0,50),1,Y));
	P2gInput(Panel,0,FVector(80,Y-2,50),FVector(0,1,0)); P2gInput(Panel,0,FVector(80,Y+3,50),FVector(0,1,0));
	TestTrue(TEXT("E6 actual inside crossing opens"),Panel->GetMotion().AngleDeg > 0);
	TestEqual(TEXT("E6 inside selected"),Panel->GetMotion().PushFace,1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP2gSelect, "vBoR.P2g.E7.SurfaceFaceSelection", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP2gSelect::RunTest(const FString& Parameters)
{
	FP2gWorld W; AP2cTrackedPanel* Panel = P2gPanel(W.World,true,10); W.Play();
	FP2gTestAccess::Angle(Panel,40);
	double Y; FP2gTestAccess::Face(Panel,FVector(80,0,50),0,Y);
	const double GapY=(Y+Panel->GetMotion().FaceMaxCm.Y)/2;
	// Hand direction would choose inside (validate previous point's actual face first, P2g E7)
	P2gInput(Panel,40,FVector(80,GapY,50),FVector(0,1,0));
	P2gInput(Panel,40,FVector(80,Y-2,50),FVector(0,1,0));
	TestEqual(TEXT("Outside selected despite inside-facing palm"),Panel->GetMotion().Metrics.Ignored[static_cast<int32>(EVBoRDoorIgnore::PalmDirection)],1);
	TestEqual(TEXT("Wrong palm leaves angle"),Panel->GetMotion().AngleDeg,40.0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP2gFallback, "vBoR.P2g.E8.MissingCollisionFallback", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP2gFallback::RunTest(const FString& Parameters)
{
	FP2gWorld W; AP2cTrackedPanel* Panel = P2gPanel(W.World); AP2cTrackedPanel* Plain = P2gPanel(W.World,false);
	UStaticMesh* Empty = DuplicateObject<UStaticMesh>(Panel->DoorMesh->GetStaticMesh(),W.World);
	UBodySetup* EmptyBody = NewObject<UBodySetup>(Empty);
	// Empty BodySetup recooks source triangles too. Force empty simple only (P2g E8)
	EmptyBody->CollisionTraceFlag = CTF_UseSimpleAsComplex;
	Empty->SetBodySetup(EmptyBody); Panel->DoorMesh->SetStaticMesh(Empty);
	Panel->CheckSetup(); TestTrue(TEXT("E8 setup detects missing collision"),Panel->SetupReport.Contains(TEXT("DoorMesh has no traceable collision; push uses bounds")));
	AddExpectedMessagePlain(TEXT("DoorMesh has no traceable collision; push uses bounds"),ELogVerbosity::Warning,EAutomationExpectedMessageFlags::Contains,1);
	AddExpectedMessagePlain(TEXT("Non-cube DoorMesh still uses mock MeshRestFromRoot"),ELogVerbosity::Warning,EAutomationExpectedMessageFlags::Contains,1);
	W.Play(); TestFalse(TEXT("E8 no callback installed"),static_cast<bool>(Panel->GetMotion().QueryFace));
	for (int32 Frame=0; Frame<8; ++Frame)
	{
		const FVector P(80,Frame == 0 ? -8 : 2,50);
		P2gInput(Plain,0,P,FVector(0,1,0)); P2gInput(Panel,0,P,FVector(0,1,0));
		TestEqual(TEXT("E8 fallback same angle each frame"),Panel->GetMotion().AngleDeg,Plain->GetMotion().AngleDeg);
	}
	TestTrue(TEXT("Fallback actually opens"),Panel->GetMotion().AngleDeg > 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP2gCollision, "vBoR.P2g.E9.CollisionIsolation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP2gCollision::RunTest(const FString& Parameters)
{
	FP2gWorld W; AP2cTrackedPanel* Panel = P2gPanel(W.World); AP2cTrackedPanel* Plain = P2gPanel(W.World,false);
	Plain->SetActorLocation(FVector(400,0,0));
	TestEqual(TEXT("Construction enables query only"),Panel->DoorMesh->GetCollisionEnabled(),ECollisionEnabled::QueryOnly);
	TestEqual(TEXT("Off remains no collision"),Plain->DoorMesh->GetCollisionEnabled(),ECollisionEnabled::NoCollision);
	TestFalse(TEXT("Overlap events disabled"),Panel->DoorMesh->GetGenerateOverlapEvents());
	for (int32 Channel=0; Channel<ECC_MAX; ++Channel) { TestEqual(TEXT("Every channel ignores"),Panel->DoorMesh->GetCollisionResponseToChannel(static_cast<ECollisionChannel>(Channel)),ECR_Ignore); }
	AActor* Hand = W.World->SpawnActor<AActor>(); auto* Sphere = NewObject<USphereComponent>(Hand);
	Hand->SetRootComponent(Sphere); Sphere->SetSphereRadius(4);
	Sphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly); Sphere->SetCollisionObjectType(ECC_WorldDynamic);
	Sphere->SetCollisionResponseToAllChannels(ECR_Overlap); Sphere->SetGenerateOverlapEvents(true); Sphere->RegisterComponent();
	Sphere->SetWorldLocation(FVector(80,0,50)); W.Play(); Sphere->UpdateOverlaps();
	TestFalse(TEXT("Hand detector sphere cannot overlap DoorMesh"),Sphere->IsOverlappingComponent(Panel->DoorMesh));
	TArray<FOverlapResult> Hits; FComponentQueryParams Query; Query.AddIgnoredActor(Hand);
	W.World->ComponentOverlapMulti(Hits,Sphere,Sphere->GetComponentLocation(),FQuat::Identity,Query);
	TestFalse(TEXT("World overlap excludes DoorMesh"),Hits.ContainsByPredicate([Panel](const FOverlapResult& Hit) { return Hit.Component.Get() == Panel->DoorMesh; }));
	FHitResult Hit; FCollisionQueryParams Params; Params.bTraceComplex = true; Params.AddIgnoredActor(Hand);
	TestFalse(TEXT("Visibility world trace ignores panel"),W.World->LineTraceSingleByChannel(Hit,FVector(80,20,50),FVector(80,-20,50),ECC_Visibility,Params));
	double Y; TestTrue(TEXT("Direct complex trace still hits"),FP2gTestAccess::Face(Panel,FVector(80,0,50),0,Y));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP2gSetup, "vBoR.P2g.E10.SetupAssessment", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP2gSetup::RunTest(const FString& Parameters)
{
	FP2gWorld W; AP2cTrackedPanel* Panel = P2gPanel(W.World);
	Panel->bUsePushFaceBox = true; Panel->PushFaceWarnThicknessCm = 1;
	Panel->PushFaceBox->SetRelativeLocation(FVector(500)); Panel->PushFaceBox->SetRelativeRotation(FRotator(0,20,0));
	Panel->OnConstruction(Panel->GetActorTransform()); Panel->CheckSetup();
	const FString Before = Panel->SetupReport;
	TestTrue(TEXT("Box ignored notice"),Before.Contains(TEXT("PushFaceBox ignored while pushing on the mesh surface")));
	TestFalse(TEXT("No thickness warning"),Before.Contains(TEXT("thickness")));
	TestFalse(TEXT("Ignored box rotation not warned"),Before.Contains(TEXT("rotation")));
	TestTrue(TEXT("Mesh bounds used instead of box"),Panel->GetMotion().FaceMaxCm.Equals(FVector(110,5,105),1.e-3));
	TestTrue(TEXT("Setup trace works before Play"),FP2gTestAccess::Traceable(Panel));
	W.Play(); Panel->CheckSetup(); TestEqual(TEXT("Editor and BeginPlay share assessment"),Panel->SetupReport,Before);
	TestTrue(TEXT("BeginPlay installs mesh query"),static_cast<bool>(Panel->GetMotion().QueryFace));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP2gRules, "vBoR.P2g.E11.ExistingRules", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP2gRules::RunTest(const FString& Parameters)
{
	FP2gWorld W; AP2cTrackedPanel* Panel = P2gPanel(W.World); Panel->bUseLatch = true; W.Play();
	P2gInput(Panel,0,FVector(80,-8,50),FVector(0,1,0)); P2gInput(Panel,0,FVector(80,2,50),FVector(0,1,0));
	TestEqual(TEXT("Locked once"),Panel->GetMotion().Metrics.Ignored[static_cast<int32>(EVBoRDoorIgnore::Locked)],1);
	TestEqual(TEXT("Locked never moves"),Panel->GetMotion().AngleDeg,0.0);
	Panel->Unlock();
	P2gInput(Panel,0,FVector(80,-8,50),FVector(1,0,0)); P2gInput(Panel,0,FVector(80,0,50),FVector(1,0,0));
	TestEqual(TEXT("Palm direction retained"),Panel->GetMotion().Metrics.Ignored[static_cast<int32>(EVBoRDoorIgnore::PalmDirection)],1);
	P2gInput(Panel,0,FVector(5,-8,50),FVector(0,1,0)); P2gInput(Panel,0,FVector(5,0,50),FVector(0,1,0));
	TestEqual(TEXT("Near axis retained"),Panel->GetMotion().Metrics.Ignored[static_cast<int32>(EVBoRDoorIgnore::NearAxis)],1);
	Panel->bAllowPalmOpening = false;
	P2gInput(Panel,0,FVector(80,-8,50),FVector(0,1,0)); P2gInput(Panel,0,FVector(80,0,50),FVector(0,1,0));
	TestEqual(TEXT("Palm opening disabled retained"),Panel->GetMotion().Metrics.Ignored[static_cast<int32>(EVBoRDoorIgnore::OpeningDisabled)],1);
	Panel->bAllowPalmOpening = true; Panel->Tick(0.016f);
	P2gInput(Panel,0,FVector(80,-8,50),FVector(0,1,0)); P2gInput(Panel,0,FVector(80,0,50),FVector(0,1,0));
	TestEqual(TEXT("Left begins pushing"),Panel->GetMotion().PushHand,0);
	const double A=Panel->GetMotion().AngleDeg;
	P2gInput(Panel,A,FVector(80,-8,50),FVector(0,1,0),1);
	Panel->Hands[0].bTracked=false;
	P2gInput(Panel,A,FVector(80,0,50),FVector(0,1,0),1);
	TestEqual(TEXT("Loss counted once"),Panel->GetMotion().Metrics.PushTrackingLosses,1);
	TestEqual(TEXT("Other hand pushes immediately"),Panel->GetMotion().PushHand,1);
	FVBoRDoorMotion& Motion=FP2gTestAccess::Motion(Panel);
	TestTrue(TEXT("Grab accepted while other hand pushes"),Motion.BeginGrab(0,0,P2gHand(FVector(80,10,50))));
	Panel->Hands[0]=P2gHand(FVector(80,10,50)); const double Before=Motion.AngleDeg; Panel->Tick(0.016f);
	TestEqual(TEXT("Stationary grab takes precedence"),Motion.AngleDeg,Before);
	TestEqual(TEXT("Grab clears push"),Motion.PushHand,INDEX_NONE);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP2gOpen, "vBoR.P2g.E12.OpenPanelTransform", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP2gOpen::RunTest(const FString& Parameters)
{
	FP2gWorld W; AP2cTrackedPanel* Panel = P2gPanel(W.World,true,10);
	Panel->SetActorTransform(FTransform(FRotator(12,33,-7),FVector(200,-150,90)));
	Panel->HingePivot->SetRelativeRotation(FRotator(8,15,23)); Panel->OnConstruction(Panel->GetActorTransform()); W.Play();
	double ClosedY, OpenY; TestTrue(TEXT("Closed trace"),FP2gTestAccess::Face(Panel,FVector(80,0,50),0,ClosedY));
	FP2gTestAccess::Angle(Panel,40); TestTrue(TEXT("Open trace"),FP2gTestAccess::Face(Panel,FVector(80,0,50),0,OpenY));
	TestTrue(TEXT("Door-local surface independent of opening and world pose"),FMath::IsNearlyEqual(ClosedY,OpenY,1.e-4));
	P2gInput(Panel,40,FVector(80,OpenY+2,50)); P2gInput(Panel,40,FVector(80,OpenY-2,50));
	TestTrue(TEXT("Open panel closes at actual surface"),Panel->GetMotion().AngleDeg < 40);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP2gCount, "vBoR.P2g.E13.QueryBudget", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP2gCount::RunTest(const FString& Parameters)
{
	FVBoRDoorMotion Door; Door.bUseLatch=false;
	int32 Calls=0;
	Door.QueryFace=[&](const FVector& P,int32 Face,double& Y) { ++Calls; Y=Face==0?5:-5; return true; };
	// Both hands wrong direction through full check (worst path, P2g E13)
	FVBoRDoorHand Hands[2]={P2gHand(FVector(80,-8,50),FVector(1,0,0)),P2gHand(FVector(80,-8,50),FVector(1,0,0))};
	Door.Advance(Hands,0.016);
	Hands[0]=Hands[1]=P2gHand(FVector(80,0,50),FVector(1,0,0)); Calls=0; Door.Advance(Hands,0.016);
	TestEqual(TEXT("Two hands: 2 face selections + 5 current + 5 previous each"),Calls,24);
	TestTrue(TEXT("E13 at most 30 traces per frame"),Calls<=30);
	return true;
}
#endif
