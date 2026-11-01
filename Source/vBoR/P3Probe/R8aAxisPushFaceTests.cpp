#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "P5mTestFixture.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"

struct FR8aTestAccess
{
	static void Geometry(const AVBoRAxisPart* P, FBox& B, FVector& R, FVector& N) { P->PushGeometry(B, R, N); }
	static double Value(const AVBoRAxisPart* P) { return P->Value; }
	static int32 Hand(const AVBoRAxisPart* P) { return P->PushHand; }
	static int32 Face(const AVBoRAxisPart* P) { return P->PushFace; }
	static void Setup(AVBoRAxisPart* P) { P->ConfigurePush(); }
};

namespace
{
struct FR8aWorld
{
	UWorld* World;
	explicit FR8aWorld(EWorldType::Type Type = EWorldType::Game)
	{
		World = UWorld::CreateWorld(Type, false);
		GEngine->CreateNewWorldContext(Type).SetCurrentWorld(World);
	}
	AP5mTrackedPart* Part()
	{
		auto* P = World->SpawnActorDeferred<AP5mTrackedPart>(AP5mTrackedPart::StaticClass(), FTransform::Identity);
		P->bAllowPush = true; P->Release = EVBoRAxisRelease::Detent;
		// Combined plate/axis-rod bounds proxy; authored face excludes the rod (R8a E1)
		P->MeshRestFromRoot = FTransform(FQuat::Identity, FVector(0, 20, 0), FVector(.8, .4, .1));
		P->FinishSpawning(FTransform::Identity);
		P->bUsePushFaceBox = true;
		P->PushFaceBox->SetRelativeTransform(FTransform(FQuat::Identity, FVector(0, 15, 0), FVector(-2, 1, 1)));
		P->PushFaceBox->SetBoxExtent(FVector(2, 8, 1));
		return P;
	}
	void Play() { World->InitializeActorsForPlay(FURL()); World->GetWorldSettings()->NotifyBeginPlay(); }
	~FR8aWorld() { World->EndPlay(EEndPlayReason::Quit); World->DestroyWorld(false); GEngine->DestroyWorldContext(World); }
};

void R8aPoint(AP5mTrackedPart* P, const FVector& Point)
{
	P->Hands[0].bTracked = true;
	for (FVector& Q : P->Hands[0].Points) { Q = FVector(1000); }
	P->Hands[0].Points[1] = Point; P->Tick(.001f);
}

void R8aBounds(FAutomationTestBase& T, const FBox& Actual, const FBox& Expected)
{
	T.TestTrue(TEXT("Minimum matches cm"), Actual.Min.Equals(Expected.Min, .001));
	T.TestTrue(TEXT("Maximum matches cm"), Actual.Max.Equals(Expected.Max, .001));
}

FBox R8aMeshBounds(const AVBoRAxisPart* P)
{
	return P->PartMesh->GetStaticMesh()->GetBoundingBox().TransformBy(P->PartMesh->GetRelativeTransform());
}

FBox R8aFaceBounds(const AVBoRAxisPart* P, const FBox& Source, FVector& Radial, FVector& Normal)
{
	FBox B; FR8aTestAccess::Geometry(P, B, Radial, Normal);
	FBox Expected(ForceInit);
	const FVector Axis = P->Axis == EAxis::Y ? FVector::YAxisVector : P->Axis == EAxis::Z ? FVector::ZAxisVector : FVector::XAxisVector;
	for (int32 I = 0; I < 8; ++I)
	{
		const FVector C(I & 1 ? Source.Max.X : Source.Min.X, I & 2 ? Source.Max.Y : Source.Min.Y, I & 4 ? Source.Max.Z : Source.Min.Z);
		Expected += FVector(FVector::DotProduct(C, Radial), FVector::DotProduct(C, Normal), FVector::DotProduct(C, Axis));
	}
	return Expected;
}

TArray<FVBoRGuideLine> R8aLines(AVBoRAxisPart* P, EVBoRGuideShape Kind)
{
	return P->MotionDisplay->BuildShapes().FilterByPredicate([Kind](const auto& L) { return L.Kind == Kind; });
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FR8aE1, "vBoR.R8a.E1AuthoredFace", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FR8aE1::RunTest(const FString&)
{
	FR8aWorld W; auto* P = W.Part(); P->PushContactOffsetCm = 0; W.Play();
	FBox B; FVector R, N; FR8aTestAccess::Geometry(P, B, R, N);
	R8aBounds(*this, B, FBox(FVector(7, -1, -4), FVector(23, 1, 4)));
	TestTrue(TEXT("Radial follows plate"), R.Equals(FVector::YAxisVector));
	TestTrue(TEXT("Normal follows plate"), N.Equals(FVector::ZAxisVector));
	TestTrue(TEXT("Face attached to axis"), P->PushFaceBox->GetAttachParent() == P->AxisPivot);
	TestEqual(TEXT("Face has no collision"), P->PushFaceBox->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
	TestFalse(TEXT("Face has no overlaps"), P->PushFaceBox->GetGenerateOverlapEvents());
	TestTrue(TEXT("Face hidden in game"), P->PushFaceBox->bHiddenInGame);
	R8aPoint(P, FVector(0, 15, -6)); R8aPoint(P, FVector(0, 15, -4.5));
	TestEqual(TEXT("Merged mesh face cannot start push"), FR8aTestAccess::Hand(P), INDEX_NONE);
	R8aPoint(P, FVector(15, 15, -2)); R8aPoint(P, FVector(15, 15, -.5));
	TestEqual(TEXT("Rod outside authored width cannot push"), FR8aTestAccess::Hand(P), INDEX_NONE);
	R8aPoint(P, FVector(0, 15, -2)); R8aPoint(P, FVector(0, 15, -.5));
	TestEqual(TEXT("Authored face starts push"), FR8aTestAccess::Hand(P), 0);
	TestTrue(TEXT("Authored face rotates"), FR8aTestAccess::Value(P) > 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FR8aE2, "vBoR.R8a.E2MeshFallback", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FR8aE2::RunTest(const FString&)
{
	FR8aWorld W; auto* P = W.Part(); P->bUsePushFaceBox = false; P->PushContactOffsetCm = 0;
	FBox B; FVector R, N; FR8aTestAccess::Geometry(P, B, R, N);
	R8aBounds(*this, B, R8aFaceBounds(P, R8aMeshBounds(P), R, N));
	W.Play(); R8aPoint(P, FVector(0, 15, -6)); R8aPoint(P, FVector(0, 15, -4.5));
	TestEqual(TEXT("Disabled box uses original mesh contact"), FR8aTestAccess::Hand(P), 0);
	TestFalse(TEXT("Box defaults off"), GetDefault<AVBoRAxisPart>()->bUsePushFaceBox);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FR8aE3, "vBoR.R8a.E3RotatedBoxFallback", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FR8aE3::RunTest(const FString&)
{
	FR8aWorld W; auto* P = W.Part(); P->PushFaceBox->SetRelativeRotation(FRotator(0, 10, 0));
	AddExpectedMessagePlain(TEXT("Push face box rotation must be zero; using mesh bounds"), ELogVerbosity::Warning);
	W.Play(); FR8aTestAccess::Setup(P); P->Tick(.001f); P->Tick(.001f);
	FBox B; FVector R, N; FR8aTestAccess::Geometry(P, B, R, N);
	R8aBounds(*this, B, R8aFaceBounds(P, R8aMeshBounds(P), R, N));
	FR8aWorld Editor(EWorldType::EditorPreview); auto* Display = Editor.Part(); Display->PushFaceBox->SetRelativeRotation(FRotator(0, 10, 0));
	const auto Lines = R8aLines(Display, EVBoRGuideShape::PushPlate);
	TestEqual(TEXT("Fallback plate has twelve edges"), Lines.Num(), 12);
	for (const auto& L : Lines) { TestEqual(TEXT("Fallback warning color"), L.Color, FColor::Magenta); }
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FR8aE4, "vBoR.R8a.E4ConstructionFit", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FR8aE4::RunTest(const FString&)
{
	FR8aWorld W; auto* P = W.Part(); P->bUsePushFaceBox = false;
	P->MeshRestFromRoot = FTransform(FRotator(8, 17, -6), FVector(3, -15, 6), FVector(.08, .4, .02));
	P->OnConstruction(P->GetActorTransform()); const FBox Expected = R8aMeshBounds(P);
	TestTrue(TEXT("Disabled box center fits mesh"), P->PushFaceBox->GetRelativeLocation().Equals(Expected.GetCenter(), .001));
	TestTrue(TEXT("Disabled box extent fits mesh"), P->PushFaceBox->GetUnscaledBoxExtent().Equals(Expected.GetExtent(), .001));
	TestTrue(TEXT("Construction clears box scale"), P->PushFaceBox->GetRelativeScale3D().Equals(FVector::OneVector));
	TestTrue(TEXT("Construction clears box rotation"), P->PushFaceBox->GetRelativeRotation().IsNearlyZero());
	P->bUsePushFaceBox = true; const FTransform Before = P->PushFaceBox->GetRelativeTransform();
	P->OnConstruction(P->GetActorTransform());
	TestTrue(TEXT("Enabled box preserves authored transform"), Before.Equals(P->PushFaceBox->GetRelativeTransform()));
	FBox B; FVector R, N; FR8aTestAccess::Geometry(P, B, R, N);
	R8aBounds(*this, B, R8aFaceBounds(P, Expected, R, N));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FR8aE5, "vBoR.R8a.E5AxesAndReverse", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FR8aE5::RunTest(const FString&)
{
	for (EAxis::Type Axis : {EAxis::X, EAxis::Y, EAxis::Z}) for (bool Reverse : {false, true}) for (double Sign : {-1., 1.})
	{
		FR8aWorld W; auto* P = W.Part(); P->Axis = Axis; P->bReverse = Reverse; P->PushContactOffsetCm = 0;
		const int32 A = Axis == EAxis::X ? 0 : Axis == EAxis::Y ? 1 : 2;
		FVector Along = FVector::ZeroVector, Radial = FVector::ZeroVector; Along[A] = 1; Radial[(A + 1) % 3] = Sign;
		const FVector Normal = FVector::CrossProduct(Along, Radial);
		P->PushFaceBox->SetRelativeTransform(FTransform(FQuat::Identity, Radial * 15));
		P->PushFaceBox->SetBoxExtent(Along.GetAbs() * 2 + Radial.GetAbs() * 8 + Normal.GetAbs());
		FBox B; FVector R, N; FR8aTestAccess::Geometry(P, B, R, N);
		TestTrue(TEXT("Authored radial for each axis"), R.Equals(Radial)); TestTrue(TEXT("Normal equals axis cross radial"), N.Equals(Normal));
		R8aBounds(*this, B, FBox(FVector(7, -1, -2), FVector(23, 1, 2)));
		FR8aWorld Editor(EWorldType::EditorPreview); auto* Display = Editor.Part(); Display->Axis = Axis; Display->bReverse = Reverse;
		Display->PushFaceBox->SetRelativeTransform(P->PushFaceBox->GetRelativeTransform()); Display->PushFaceBox->SetBoxExtent(P->PushFaceBox->GetUnscaledBoxExtent());
		const auto Arrows = R8aLines(Display, EVBoRGuideShape::Push); TestEqual(TEXT("Two face arrows"), Arrows.Num(), 2);
		W.Play();
		for (const auto& Arrow : Arrows)
		{
			// Same input-sign check as R2a E6 (R8a E5)
			P->Hands[0] = FVBoRDoorHand(); P->Tick(.001f); P->Tick(1.f); FR8aTestAccess::Setup(P);
			const FVector Center = P->AxisPivot->GetComponentTransform().InverseTransformPosition(Arrow.End);
			const FVector Inward = P->AxisPivot->GetComponentTransform().InverseTransformVectorNoScale((Arrow.End - Arrow.Start).GetSafeNormal());
			R8aPoint(P, Center - Inward); R8aPoint(P, Center + Inward);
			TestTrue(TEXT("Arrow color matches actual angular direction"), Arrow.Color == FColor::Green ? FR8aTestAccess::Value(P) > 0 : FR8aTestAccess::Value(P) < 0);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FR8aE6, "vBoR.R8a.E6EarlyContact", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FR8aE6::RunTest(const FString&)
{
	FR8aWorld W; auto* P = W.Part(); TestEqual(TEXT("Default contact offset cm"), P->PushContactOffsetCm, .8f); W.Play();
	R8aPoint(P, FVector(0, 15, -3)); R8aPoint(P, FVector(0, 15, -2));
	TestEqual(TEXT("One cm ahead does not push"), FR8aTestAccess::Hand(P), INDEX_NONE);
	R8aPoint(P, FVector(0, 15, -1.5));
	TestEqual(TEXT("Half cm ahead starts this frame"), FR8aTestAccess::Hand(P), 0);
	TestTrue(TEXT("Early contact rotates this frame"), FR8aTestAccess::Value(P) > 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FR8aE7, "vBoR.R8a.E7OffsetAngleAndDistance", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FR8aE7::RunTest(const FString&)
{
	double Angles[2] = {};
	for (int32 I = 0; I < 2; ++I)
	{
		FR8aWorld W; auto* P = W.Part(); P->PushContactOffsetCm = I ? .8f : 0; W.Play();
		R8aPoint(P, FVector(0, 15, -3)); R8aPoint(P, FVector(0, 15, -.5));
		Angles[I] = FR8aTestAccess::Value(P);
		TestTrue(TEXT("Q1 example angle within 0.1 deg"), FMath::IsNearlyEqual(Angles[I], I ? 4.979106 : 1.911276, .1));
		const FVector Local = P->AxisPivot->GetRelativeTransform().InverseTransformPosition(FVector(0, 15, -.5));
		TestTrue(TEXT("Joint remains offset from original face within 0.01 cm"), FMath::IsNearlyEqual(-1. - Local.Z, static_cast<double>(P->PushContactOffsetCm), .01));
		R8aPoint(P, FVector(0, 15, 0));
		TestEqual(TEXT("Continuing push keeps original face"), FR8aTestAccess::Face(P), 0);
		TestTrue(TEXT("Continuing push advances angle"), FR8aTestAccess::Value(P) > Angles[I]);
	}
	TestTrue(TEXT("Offset increases absolute angle at same point"), FMath::Abs(Angles[1]) > FMath::Abs(Angles[0]));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FR8aE8, "vBoR.R8a.E8ZeroOffsetLegacy", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FR8aE8::RunTest(const FString&)
{
	FR8aWorld W; auto* P = W.Part(); P->PushContactOffsetCm = 0; W.Play();
	R8aPoint(P, FVector(0, 15, -3)); R8aPoint(P, FVector(0, 15, -1.5));
	TestEqual(TEXT("Zero offset does not start ahead of face"), FR8aTestAccess::Hand(P), INDEX_NONE);
	TestEqual(TEXT("Zero offset leaves angle unchanged ahead of face"), FR8aTestAccess::Value(P), 0.);
	R8aPoint(P, FVector(0, 15, -.5));
	TestEqual(TEXT("Original crossing starts push"), FR8aTestAccess::Hand(P), 0);
	TestTrue(TEXT("Original angle retained"), FMath::IsNearlyEqual(FR8aTestAccess::Value(P), 1.911276, .1));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FR8aE9, "vBoR.R8a.E9BothFaces", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FR8aE9::RunTest(const FString&)
{
	for (double Side : {-1., 1.})
	{
		double Angles[2] = {};
		for (int32 I = 0; I < 2; ++I)
		{
			FR8aWorld W; auto* P = W.Part(); P->PushContactOffsetCm = I ? .8f : 0; W.Play();
			R8aPoint(P, FVector(0, 15, Side * 3)); R8aPoint(P, FVector(0, 15, Side * .5));
			Angles[I] = FR8aTestAccess::Value(P);
			TestEqual(TEXT("Incoming face retained"), FR8aTestAccess::Face(P), Side < 0 ? 0 : 1);
			TestTrue(TEXT("Face determines rotation sign"), Angles[I] * Side < 0);
			const FVector Local = P->AxisPivot->GetRelativeTransform().InverseTransformPosition(FVector(0, 15, Side * .5));
			TestTrue(TEXT("Offset points outward on both faces"), FMath::IsNearlyEqual(Side * Local.Z - 1, static_cast<double>(P->PushContactOffsetCm), .01));
		}
		TestTrue(TEXT("Both faces rotate farther with offset"), FMath::Abs(Angles[1]) > FMath::Abs(Angles[0]));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FR8aE10, "vBoR.R8a.E10GuidesFollowFace", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FR8aE10::RunTest(const FString&)
{
	FR8aWorld W(EWorldType::EditorPreview); auto* P = W.Part(); P->SetActorTransform(FTransform(FRotator(5, 25, 10), FVector(30, -20, 50), FVector(1.2, .8, 1.4)));
	const auto Plate = R8aLines(P, EVBoRGuideShape::PushPlate); TestEqual(TEXT("Twelve authored plate edges"), Plate.Num(), 12);
	const FTransform Frame = P->AxisPivot->GetComponentTransform();
	TArray<FVector> Expected;
	for (double X : {-4., 4.}) for (double Y : {7., 23.}) for (double Z : {-1., 1.}) { Expected.Add(Frame.TransformPosition(FVector(X, Y, Z))); }
	for (const FVector& C : Expected) { TestTrue(TEXT("All authored corners displayed"), Plate.ContainsByPredicate([&](const auto& L) { return L.Start.Equals(C, .01) || L.End.Equals(C, .01); })); }
	for (const auto& L : Plate)
	{
		TestEqual(TEXT("Valid plate is blue"), L.Color, FColor::Blue);
		for (const FVector& C : {L.Start, L.End}) { TestTrue(TEXT("No contact offset in plate"), Expected.ContainsByPredicate([&](const FVector& E) { return E.Equals(C, .01); })); }
	}
	const auto Arrows = R8aLines(P, EVBoRGuideShape::Push); TestEqual(TEXT("Two authored face arrows"), Arrows.Num(), 2);
	for (const auto& L : Arrows)
	{
		const FVector Center = Frame.InverseTransformPosition(L.End);
		TestTrue(TEXT("Arrow ends at unexpanded face center"), FMath::IsNearlyZero(Center.X, .01) && FMath::IsNearlyEqual(Center.Y, 15., .01) && FMath::IsNearlyEqual(FMath::Abs(Center.Z), 1., .01));
	}
	P->PushContactOffsetCm = 5;
	const auto Expanded = R8aLines(P, EVBoRGuideShape::PushPlate);
	TestEqual(TEXT("Offset does not add display edges"), Expanded.Num(), Plate.Num());
	for (int32 I = 0; I < FMath::Min(Expanded.Num(), Plate.Num()); ++I) { TestTrue(TEXT("Offset never moves display"), Expanded[I].Start.Equals(Plate[I].Start) && Expanded[I].End.Equals(Plate[I].End)); }
	return true;
}
#endif
