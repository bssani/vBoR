#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "../Interaction/VBoRMotionGuideComponent.h"
#include "../Interaction/VBoRAxisPart.h"
#include "../Interaction/VBoRHingedPanel.h"
#include "../Interaction/VBoRPoweredPanel.h"
#include "../Interaction/VBoRObstructionBoxComponent.h"
#include "../Interaction/VBoRGrabCollider.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "PhysicsEngine/BodySetup.h"
#include "Interaction/IsdkGrabbableComponent.h"

struct FR2cTestAccess
{
	static bool Face(const AVBoRHingedPanel* P, const FVector& Point, int32 Side, double& Y) { return P->QueryMeshFace(Point, Side, Y); }
	static void Push(const AVBoRAxisPart* P, FBox& Bounds, FVector& Radial, FVector& Normal) { P->PushGeometry(Bounds, Radial, Normal); }
	static FVector Axis(const AVBoRAxisPart* P) { return P->LocalAxis(); }
	static auto Shape(const AVBoRPoweredPanel* P, float Angle, const UVBoRObstructionBoxComponent* Box) { return P->ObstructionShapeAt(Angle, Box); }
};

namespace
{
struct FR2cWorld
{
	UWorld* World;
	explicit FR2cWorld(EWorldType::Type Type = EWorldType::EditorPreview)
	{
		World = UWorld::CreateWorld(Type, false);
		GEngine->CreateNewWorldContext(Type).SetCurrentWorld(World);
	}
	void Play() { World->InitializeActorsForPlay(FURL()); World->GetWorldSettings()->NotifyBeginPlay(); }
	~FR2cWorld() { World->EndPlay(EEndPlayReason::Quit); World->DestroyWorld(false); GEngine->DestroyWorldContext(World); }
};

TArray<FVBoRGuideLine> R2cKind(UVBoRMotionGuideComponent* C, EVBoRGuideShape K)
{
	return C->BuildShapes().FilterByPredicate([K](const auto& L) { return L.Kind == K; });
}

FVector R2cCorner(const FBox& B, int32 I) { return FVector(I & 1 ? B.Max.X : B.Min.X, I & 2 ? B.Max.Y : B.Min.Y, I & 4 ? B.Max.Z : B.Min.Z); }

void R2cBoxMatches(FAutomationTestBase& T, const TArray<FVBoRGuideLine>& Lines, const FBox& Bounds, const FTransform& Frame)
{
	if (!T.TestEqual(TEXT("Twelve box edges"), Lines.Num(), 12)) { return; }
	TArray<FVector> Corners;
	for (int32 I = 0; I < 8; ++I) { Corners.Add(Frame.TransformPosition(R2cCorner(Bounds, I))); }
	for (const auto& L : Lines) for (const FVector& P : {L.Start, L.End})
	{
		T.TestTrue(TEXT("Endpoint equals expected corner within 0.1 cm"), Corners.ContainsByPredicate([&](const FVector& C) { return C.Equals(P, .1); }));
	}
	for (const FVector& C : Corners)
	{
		T.TestTrue(TEXT("Every corner represented"), Lines.ContainsByPredicate([&](const auto& L) { return L.Start.Equals(C, .1) || L.End.Equals(C, .1); }));
	}
}

AVBoRAxisPart* R2cGrabPart(UWorld* W, int32 Mode)
{
	auto* P = W->SpawnActor<AVBoRAxisPart>();
	P->MeshRestFromRoot = FTransform(FRotator(5, 17, 9), FVector(25, 8, 60), FVector(.2, .3, .4));
	P->bGrabWithMesh = Mode != 0;
	if (Mode >= 2)
	{
		auto* Mesh = DuplicateObject<UStaticMesh>(P->PartMesh->GetStaticMesh(), W);
		auto* Body = NewObject<UBodySetup>(Mesh);
		Body->CollisionTraceFlag = Mode == 3 ? CTF_UseComplexAsSimple : CTF_UseSimpleAndComplex;
		Mesh->SetBodySetup(Body);
		if (Mode == 3) { Body->CreatePhysicsMeshes(); }
		P->PartMesh->SetStaticMesh(Mesh);
	}
	P->OnConstruction(P->GetActorTransform());
	return P;
}

UVBoRObstructionBoxComponent* R2cObstructionBox(AVBoRPoweredPanel* P, FVector Location, FVector Extent)
{
	auto* B = NewObject<UVBoRObstructionBoxComponent>(P);
	P->AddInstanceComponent(B); B->SetupAttachment(P->HingePivot);
	B->SetRelativeLocation(Location); B->SetRelativeRotation(FRotator(11, 19, 7));
	B->SetRelativeScale3D(FVector(1.2, .8, 1.1)); B->SetBoxExtent(Extent); B->RegisterComponent();
	return B;
}
}

#define R2C_TEST(N, Name) IMPLEMENT_SIMPLE_AUTOMATION_TEST(FR2cE##N, "vBoR.R2c.E" #N Name, EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter) bool FR2cE##N::RunTest(const FString&)

R2C_TEST(1, "GrabBoxAndGrip")
{
	FR2cWorld W;
	for (bool Grip : {false, true})
	{
		auto* P = R2cGrabPart(W.World, 0); P->DoorHandleSide = Grip ? EVBoRDoorHandleSide::Grip : EVBoRDoorHandleSide::None;
		P->SetActorTransform(FTransform(FRotator(12, 31, 6), FVector(70, 80, 90)));
		P->GrabBox->SetRelativeRotation(FRotator(8, 15, 29));
		const auto Lines = R2cKind(P->MotionDisplay, EVBoRGuideShape::GrabBox);
		R2cBoxMatches(*this, Lines, FBox(-P->GrabBox->GetUnscaledBoxExtent(), P->GrabBox->GetUnscaledBoxExtent()), P->GrabBox->GetComponentTransform());
		for (const auto& L : Lines) { TestEqual(TEXT("Grab color"), L.Color, FColor::Cyan); }
		if (Grip)
		{
			TestEqual(TEXT("Grip has only grab shape"), P->MotionDisplay->BuildShapes().Num(), Lines.Num());
		}
	}
	return true;
}

R2C_TEST(2, "MeshSimpleElements")
{
	FR2cWorld W; auto* P = R2cGrabPart(W.World, 1);
	auto* Mesh = DuplicateObject<UStaticMesh>(P->PartMesh->GetStaticMesh(), W.World);
	auto* Body = NewObject<UBodySetup>(Mesh); Mesh->SetBodySetup(Body);
	FKBoxElem B(12, 18, 24); B.Center = FVector(3, 5, 7); B.Rotation = FRotator(5, 12, 9); Body->AggGeom.BoxElems.Add(B);
	P->PartMesh->SetStaticMesh(Mesh); P->OnConstruction(P->GetActorTransform());
	const auto Lines = R2cKind(P->MotionDisplay, EVBoRGuideShape::GrabMesh);
	const FKBoxElem Scaled = B.GetFinalScaled(P->PartMesh->GetComponentScale(), FTransform::Identity);
	const FTransform World(P->PartMesh->GetComponentQuat(), P->PartMesh->GetComponentLocation());
	R2cBoxMatches(*this, Lines, FBox(-FVector(Scaled.X, Scaled.Y, Scaled.Z) / 2, FVector(Scaled.X, Scaled.Y, Scaled.Z) / 2), Scaled.GetTransform() * World);
	TestEqual(TEXT("No GrabBox when mesh chosen"), R2cKind(P->MotionDisplay, EVBoRGuideShape::GrabBox).Num(), 0);
	// Also exercise curved primitives and hull triangles without source-asset edits
	Body->AggGeom.SphereElems.Add(FKSphereElem(8)); Body->AggGeom.SphylElems.Add(FKSphylElem(4, 16));
	FKConvexElem Hull; Hull.ConvexFromBoxElem(B); Body->AggGeom.ConvexElems.Add(Hull);
	const auto Extra = R2cKind(P->MotionDisplay, EVBoRGuideShape::GrabMesh);
	if (!TestEqual(TEXT("Sphere capsule and every convex triangle add wires"), Extra.Num(), Lines.Num() + 72 + 100 + Hull.IndexData.Num())) { return false; }
	for (int32 I = 0; I < Hull.IndexData.Num(); ++I)
	{
		const auto& L = Extra[Lines.Num() + 72 + 100 + I];
		const FVector Expected = (Hull.GetTransform() * P->PartMesh->GetComponentTransform()).TransformPosition(Hull.VertexData[Hull.IndexData[I]]);
		TestTrue(TEXT("Convex vertex transformed within 0.1 cm"), L.Start.Equals(Expected, .1));
	}
	for (const auto& L : Extra) { TestFalse(TEXT("Finite geometry"), L.Start.ContainsNaN() || L.End.ContainsNaN()); }
	return true;
}

R2C_TEST(3, "FallbackWarningCount")
{
	FR2cWorld W; auto* P = R2cGrabPart(W.World, 2);
	AddExpectedMessagePlain(TEXT("grab mesh has no simple collision"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 1);
	for (int32 I = 0; I < 3; ++I)
	{
		P->MotionDisplay->Refresh(); const auto Lines = R2cKind(P->MotionDisplay, EVBoRGuideShape::GrabBox);
		TestEqual(TEXT("Fallback box"), Lines.Num(), 12);
		for (const auto& L : Lines) { TestEqual(TEXT("Fallback warning color"), L.Color, FColor::Magenta); }
	}
	ConfigureVBoRGrabCollider(P->PartMesh, P->GrabBox, true, true, P->MeshRestFromRoot);
	return true;
}

R2C_TEST(4, "ComplexMeshApproximation")
{
	FR2cWorld W; auto* P = R2cGrabPart(W.World, 3);
	const auto Lines = R2cKind(P->MotionDisplay, EVBoRGuideShape::GrabMesh);
	R2cBoxMatches(*this, Lines, P->PartMesh->GetStaticMesh()->GetBoundingBox(), P->PartMesh->GetComponentTransform());
	for (const auto& L : Lines) { TestEqual(TEXT("Complex approximation color"), L.Color, FColor::Magenta); }
	TestEqual(TEXT("No GrabBox for complex mesh"), R2cKind(P->MotionDisplay, EVBoRGuideShape::GrabBox).Num(), 0);
	return true;
}

R2C_TEST(5, "RuntimeColliderMatchesDisplay")
{
	FR2cWorld Editor; FR2cWorld Game(EWorldType::Game);
	TArray<AVBoRAxisPart*> Runtime;
	for (int32 Mode = 0; Mode < 4; ++Mode)
	{
		auto* P = R2cGrabPart(Editor.World, Mode);
		TestTrue(TEXT("Display chooses same source"), R2cKind(P->MotionDisplay, Mode == 0 || Mode == 2 ? EVBoRGuideShape::GrabBox : EVBoRGuideShape::GrabMesh).Num() > 0);
		Runtime.Add(R2cGrabPart(Game.World, Mode));
	}
	AddExpectedMessagePlain(TEXT("grab mesh has no simple collision"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 1);
	Game.Play();
	for (int32 I = 0; I < Runtime.Num(); ++I)
	{
		auto* P = Runtime[I]; UPrimitiveComponent* Expected = I == 0 || I == 2 ? static_cast<UPrimitiveComponent*>(P->GrabBox) : P->PartMesh.Get();
		TestTrue(TEXT("ISDK collider matches chosen guide"), P->Grabbable->GetGrabCollider() == Expected);
	}
	return true;
}

R2C_TEST(6, "PanelBoxFaces")
{
	FR2cWorld W; auto* P = W.World->SpawnActor<AVBoRHingedPanel>(); P->bAllowPalmOpening = true;
	for (bool Custom : {false, true})
	{
		P->bUsePushFaceBox = Custom; P->PushFaceBox->SetRelativeLocation(FVector(30, 4, 50));
		P->PushFaceBox->SetRelativeScale3D(FVector(1.1, .8, 1.2)); P->PushFaceBox->SetBoxExtent(FVector(20, 3, 40));
		P->OnConstruction(P->GetActorTransform());
		const auto Lines = R2cKind(P->MotionDisplay, EVBoRGuideShape::PushFace); TestEqual(TEXT("Two face rectangles"), Lines.Num(), 8);
		const auto& M = P->GetMotion();
		for (const auto& L : Lines) for (const FVector& WorldPoint : {L.Start, L.End})
		{
			const FVector V = P->HingePivot->GetComponentTransform().InverseTransformPosition(WorldPoint);
			TestTrue(TEXT("Runtime face corners"), (FMath::IsNearlyEqual(V.X, M.FaceMinCm.X, .1) || FMath::IsNearlyEqual(V.X, M.FaceMaxCm.X, .1)) &&
				(FMath::IsNearlyEqual(V.Y, M.FaceMinCm.Y, .1) || FMath::IsNearlyEqual(V.Y, M.FaceMaxCm.Y, .1)) &&
				(FMath::IsNearlyEqual(V.Z, M.FaceMinCm.Z, .1) || FMath::IsNearlyEqual(V.Z, M.FaceMaxCm.Z, .1)));
		}
	}
	P->bAllowPalmOpening = false; TestEqual(TEXT("Opening face omitted"), R2cKind(P->MotionDisplay, EVBoRGuideShape::PushFace).Num(), 4);
	return true;
}

R2C_TEST(7, "MeshFaceGrid")
{
	FR2cWorld W; auto* P = W.World->SpawnActor<AVBoRHingedPanel>(); P->bAllowPalmOpening = true; P->bPushOnMeshSurface = true;
	P->DoorMesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere")));
	P->MeshRestFromRoot = FTransform(FRotator(12, 25, 8), FVector(55, 0, 53)); P->OnConstruction(P->GetActorTransform());
	const auto Lines = R2cKind(P->MotionDisplay, EVBoRGuideShape::PushFace); TestTrue(TEXT("Curved surface grid exists"), Lines.Num() > 8);
	int32 FaceCounts[2] = {};
	for (const auto& L : Lines) for (const FVector& WorldPoint : {L.Start, L.End})
	{
		TestEqual(TEXT("Successful trace color"), L.Color, FColor::Blue);
		const FVector V = P->HingePivot->GetComponentTransform().InverseTransformPosition(WorldPoint);
		bool Matches = false;
		for (int32 Face = 0; Face < 2; ++Face)
		{
			double Y; if (FR2cTestAccess::Face(P, V, Face, Y) && FMath::IsNearlyEqual(Y, V.Y, .1)) { Matches = true; ++FaceCounts[Face]; }
		}
		TestTrue(TEXT("Every grid endpoint is a real query hit within 0.1 cm"), Matches);
	}
	TestTrue(TEXT("Both surfaces represented"), FaceCounts[0] > 0 && FaceCounts[1] > 0);
	return true;
}

R2C_TEST(8, "UntraceableMeshFallback")
{
	FR2cWorld W; auto* P = W.World->SpawnActor<AVBoRHingedPanel>(); P->bAllowPalmOpening = true; P->bPushOnMeshSurface = true;
	auto* Mesh = DuplicateObject<UStaticMesh>(P->DoorMesh->GetStaticMesh(), W.World);
	auto* Body = NewObject<UBodySetup>(Mesh); Body->CollisionTraceFlag = CTF_UseSimpleAsComplex; Mesh->SetBodySetup(Body);
	P->DoorMesh->SetStaticMesh(Mesh); P->OnConstruction(P->GetActorTransform());
	const auto Lines = R2cKind(P->MotionDisplay, EVBoRGuideShape::PushFace); TestEqual(TEXT("Fallback rectangles"), Lines.Num(), 8);
	for (const auto& L : Lines) { TestEqual(TEXT("Trace failure warning color"), L.Color, FColor::Magenta); }
	return true;
}

R2C_TEST(9, "AxisPushPlate")
{
	FR2cWorld W;
	for (EAxis::Type Axis : {EAxis::X, EAxis::Y, EAxis::Z})
	{
		auto* P = R2cGrabPart(W.World, 0); P->Axis = Axis; P->bAllowPush = true;
		FBox Bounds; FVector Radial, Normal; FR2cTestAccess::Push(P, Bounds, Radial, Normal);
		FMatrix Basis = FMatrix::Identity; Basis.SetAxes(&Radial, &Normal); const FVector A = FR2cTestAccess::Axis(P); Basis.SetAxis(2, A);
		R2cBoxMatches(*this, R2cKind(P->MotionDisplay, EVBoRGuideShape::PushPlate), Bounds, FTransform(Basis) * P->AxisPivot->GetComponentTransform());
		P->bAllowPush = false; TestEqual(TEXT("Disabled push has no plate"), R2cKind(P->MotionDisplay, EVBoRGuideShape::PushPlate).Num(), 0);
		P->bAllowPush = true; P->Movement = EVBoRAxisMovement::Translate;
		TestEqual(TEXT("Translate has no plate"), R2cKind(P->MotionDisplay, EVBoRGuideShape::PushPlate).Num(), 0);
	}
	return true;
}

R2C_TEST(10, "ObstructionBoxSweep")
{
	FR2cWorld W; auto* P = W.World->SpawnActor<AVBoRPoweredPanel>(); P->OpenAngleDeg = 85;
	P->SetActorTransform(FTransform(FRotator(9, 21, 7), FVector(120, 30, 50), FVector(1.2, .9, 1.1)));
	TArray<UVBoRObstructionBoxComponent*> Boxes = {R2cObstructionBox(P, FVector(5, 10, -25), FVector(5, 9, 12)), R2cObstructionBox(P, FVector(-8, -15, -50), FVector(7, 11, 13))};
	const auto Lines = R2cKind(P->MotionDisplay, EVBoRGuideShape::Obstruction);
	const int32 Steps = FMath::CeilToInt(P->OpenAngleDeg / 10.f);
	if (!TestEqual(TEXT("Both boxes at every angle including endpoints"), Lines.Num(), (Steps + 1) * 2 * 12)) { return false; }
	for (int32 Step = 0; Step <= Steps; ++Step) for (int32 B = 0; B < 2; ++B)
	{
		const float Angle = P->OpenAngleDeg * Step / Steps;
		const auto Shape = FR2cTestAccess::Shape(P, Angle, Boxes[B]);
		TArray<FVBoRGuideLine> Slice; Slice.Append(Lines.GetData() + (Step * 2 + B) * 12, 12);
		R2cBoxMatches(*this, Slice, FBox(-Shape.Extent, Shape.Extent), FTransform(Shape.Rotation, Shape.Center));
		for (const auto& L : Slice) { TestEqual(TEXT("Configured sweep color"), L.Color, FColor::Orange); }
	}
	return true;
}

R2C_TEST(11, "MeshBoundsSweep")
{
	FR2cWorld W; auto* P = W.World->SpawnActor<AVBoRPoweredPanel>();
	const auto Lines = R2cKind(P->MotionDisplay, EVBoRGuideShape::Obstruction); const int32 Steps = FMath::CeilToInt(P->OpenAngleDeg / 10.f);
	if (!TestEqual(TEXT("One approximate box per sample"), Lines.Num(), (Steps + 1) * 12)) { return false; }
	for (int32 Step = 0; Step <= Steps; ++Step)
	{
		const float Angle = P->OpenAngleDeg * Step / Steps;
		const auto Shape = FR2cTestAccess::Shape(P, Angle, nullptr);
		TArray<FVBoRGuideLine> Slice; Slice.Append(Lines.GetData() + Step * 12, 12);
		R2cBoxMatches(*this, Slice, FBox(-Shape.Extent, Shape.Extent), FTransform(Shape.Rotation, Shape.Center));
		for (const auto& L : Slice) { TestEqual(TEXT("Approximate sweep color"), L.Color, FColor::Magenta); }
	}
	return true;
}

R2C_TEST(12, "IndependentDisplayModes")
{
	FR2cWorld W;
	auto Check = [&](auto* P)
	{
		TestTrue(TEXT("Judgment defaults Always"), P->JudgmentGuide == EVBoRMotionGuide::Always);
		P->MotionGuide = EVBoRMotionGuide::Off; P->MotionDisplay->Refresh();
		TestTrue(TEXT("Judgment remains when motion off"), P->MotionDisplay->BuildShapes().Num() > 0);
		P->MotionGuide = EVBoRMotionGuide::SelectedOnly;
		TestTrue(TEXT("Judgment remains when motion unselected"), P->MotionDisplay->BuildShapes().Num() > 0);
		P->JudgmentGuide = EVBoRMotionGuide::Off;
		TestEqual(TEXT("Both hidden"), P->MotionDisplay->BuildShapes().Num(), 0);
		P->JudgmentGuide = EVBoRMotionGuide::SelectedOnly;
		TestEqual(TEXT("Unselected judgment hidden"), P->MotionDisplay->BuildShapes().Num(), 0);
		P->MotionGuide = EVBoRMotionGuide::Always;
		TestTrue(TEXT("Motion independent of hidden judgment"), P->MotionDisplay->BuildShapes().Num() > 0);
	};
	Check(R2cGrabPart(W.World, 0)); auto* H = W.World->SpawnActor<AVBoRHingedPanel>(); Check(H); Check(W.World->SpawnActor<AVBoRPoweredPanel>());
	FR2cWorld Game(EWorldType::Game); auto* A = R2cGrabPart(Game.World, 0); auto* G = Game.World->SpawnActor<AVBoRPoweredPanel>(); auto* D = Game.World->SpawnActor<AVBoRHingedPanel>();
	for (UVBoRMotionGuideComponent* C : {A->MotionDisplay.Get(), G->MotionDisplay.Get(), D->MotionDisplay.Get()})
	{
		C->Refresh(); TestEqual(TEXT("Game shapes empty"), C->BuildShapes().Num(), 0); TestEqual(TEXT("Game rendered lines empty"), C->BatchedLines.Num(), 0);
	}
	return true;
}

R2C_TEST(13, "PreviewGeometry")
{
	FR2cWorld W; auto* P = R2cGrabPart(W.World, 0); P->bAllowPush = true;
	const FTransform Authored = P->AxisPivot->GetRelativeTransform();
	const auto Before = R2cKind(P->MotionDisplay, EVBoRGuideShape::GrabBox);
	const auto PlateBefore = R2cKind(P->MotionDisplay, EVBoRGuideShape::PushPlate);
	const FTransform OldFrame = P->AxisPivot->GetComponentTransform(); P->Preview = 1; P->MotionDisplay->Refresh();
	auto Check = [&](const auto& Old, EVBoRGuideShape K, const FTransform& From, const FTransform& To, UVBoRMotionGuideComponent* C)
	{
		const auto After = R2cKind(C, K); if (!TestEqual(TEXT("Same topology during preview"), After.Num(), Old.Num())) { return; }
		for (int32 I = 0; I < Old.Num(); ++I)
		{
			TestTrue(TEXT("Preview start follows visible frame"), After[I].Start.Equals(To.TransformPosition(From.InverseTransformPosition(Old[I].Start)), .1));
			TestTrue(TEXT("Preview end follows visible frame"), After[I].End.Equals(To.TransformPosition(From.InverseTransformPosition(Old[I].End)), .1));
		}
	};
	Check(Before, EVBoRGuideShape::GrabBox, OldFrame, P->AxisPivot->GetComponentTransform(), P->MotionDisplay);
	Check(PlateBefore, EVBoRGuideShape::PushPlate, OldFrame, P->AxisPivot->GetComponentTransform(), P->MotionDisplay);
	TestTrue(TEXT("Authored relative preserved"), P->AxisPivot->GetRelativeTransform().Equals(Authored));
	auto* H = W.World->SpawnActor<AVBoRHingedPanel>(); H->bAllowPalmOpening = true; H->OnConstruction(H->GetActorTransform());
	const auto Faces = R2cKind(H->MotionDisplay, EVBoRGuideShape::PushFace); const FTransform OldHinge = H->HingePivot->GetComponentTransform();
	H->Preview = 1; H->MotionDisplay->Refresh(); Check(Faces, EVBoRGuideShape::PushFace, OldHinge, H->HingePivot->GetComponentTransform(), H->MotionDisplay);
	H->Preview = 0; H->bPushOnMeshSurface = true;
	H->DoorMesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere")));
	H->MeshRestFromRoot = FTransform(FRotator(12, 23, 7), FVector(55, 0, 53)); H->OnConstruction(H->GetActorTransform());
	const auto MeshFaces = R2cKind(H->MotionDisplay, EVBoRGuideShape::PushFace); const FTransform OldMeshHinge = H->HingePivot->GetComponentTransform();
	TestTrue(TEXT("Preview fixture has a traced mesh grid"), MeshFaces.Num() > 8);
	H->Preview = 1; H->MotionDisplay->Refresh(); Check(MeshFaces, EVBoRGuideShape::PushFace, OldMeshHinge, H->HingePivot->GetComponentTransform(), H->MotionDisplay);
	auto* G = W.World->SpawnActor<AVBoRPoweredPanel>(); R2cObstructionBox(G, FVector(0, 5, -40), FVector(4, 8, 15));
	const auto Sweeps = R2cKind(G->MotionDisplay, EVBoRGuideShape::Obstruction); G->Preview = 1; G->MotionDisplay->Refresh();
	Check(Sweeps, EVBoRGuideShape::Obstruction, FTransform::Identity, FTransform::Identity, G->MotionDisplay);
	return true;
}
#undef R2C_TEST
#endif
