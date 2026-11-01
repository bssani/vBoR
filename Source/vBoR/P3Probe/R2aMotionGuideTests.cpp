#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include <type_traits>
#include "R2aTestObserver.h"
#include "../Interaction/VBoRMotionGuideComponent.h"
#include "../Interaction/VBoRHingedDoor.h"
#include "../Interaction/VBoRPoweredPanel.h"
#include "../Interaction/VBoRFoldingMirror.h"
#include "../Interaction/VBoRFoldingSeat.h"
#include "../Interaction/VBoRAxisPart.h"
#include "../Interaction/VBoRObstructionBoxComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "UObject/UnrealType.h"

struct FR2aTestAccess
{
	static void Apply(AVBoRHingedPanel* P, double V) { P->ClosedHingeRotation = P->HingePivot->GetRelativeRotation().Quaternion(); P->Motion.AngleDeg = V; P->ApplyState(V, P->Motion.Metrics.Unlocks, P->Motion.Metrics.Latches); }
	static void Apply(AVBoRPoweredPanel* P, double V) { P->Motion.AngleDeg = V; P->ApplyAngle(); }
	static void Apply(AVBoRFoldingMirror* P, double V) { P->AngleDeg = V; P->ApplyAngle(); }
	static void Apply(AVBoRFoldingSeat* P, double V) { P->AngleDeg = V; P->ApplyAngle(); }
	static void Apply(AVBoRAxisPart* P, double V) { P->RestPivot = P->AxisPivot->GetRelativeTransform(); P->Value = V; P->AxisPivot->SetRelativeTransform(P->ValueTransform() * P->RestPivot); }
	static double Value(AVBoRAxisPart* P) { return P->Value; }
	static void SetupPush(AVBoRAxisPart* P) { P->ConfigurePush(); }
	static void Push(AVBoRAxisPart* P, const FVBoRDoorHand (&Hands)[2]) { P->AdvancePush(Hands); }
};

namespace
{
struct FR2aWorld
{
	UWorld* World;
	explicit FR2aWorld(EWorldType::Type Type = EWorldType::EditorPreview)
	{
		World = UWorld::CreateWorld(Type, false);
		GEngine->CreateNewWorldContext(Type).SetCurrentWorld(World);
	}
	void Play() { World->InitializeActorsForPlay(FURL()); World->GetWorldSettings()->NotifyBeginPlay(); }
	~FR2aWorld() { World->EndPlay(EEndPlayReason::Quit); World->DestroyWorld(false); GEngine->DestroyWorldContext(World); }
};

TArray<FVBoRGuideLine> Kind(UVBoRMotionGuideComponent* C, EVBoRGuideShape K)
{
	return C->BuildShapes().FilterByPredicate([K](const FVBoRGuideLine& L) { return L.Kind == K; });
}

void Pose(FAutomationTestBase& T, const FTransform& A, const FTransform& B)
{
	T.TestTrue(TEXT("Position within 0.1 cm"), A.GetLocation().Equals(B.GetLocation(), .1));
	T.TestTrue(TEXT("Rotation within 0.1 deg"), FMath::RadiansToDegrees(A.GetRotation().AngularDistance(B.GetRotation())) < .1);
	T.TestTrue(TEXT("Scale preserved"), A.GetScale3D().Equals(B.GetScale3D(), 1.e-5));
}

template<class TPart>
void ArcMatches(FAutomationTestBase& T, TPart* Part, USceneComponent* Pivot, UStaticMeshComponent* Mesh, double EndValue, FVector LocalAxis)
{
	const auto Arcs = Kind(Part->MotionDisplay, EVBoRGuideShape::Arc);
	const auto Axes = Kind(Part->MotionDisplay, EVBoRGuideShape::Axis);
	if (!T.TestEqual(TEXT("32 arc segments"), Arcs.Num(), 32) || !T.TestEqual(TEXT("One axis"), Axes.Num(), 1)) { return; }
	T.TestTrue(TEXT("Axis matches actual frame"), FMath::Abs(FVector::DotProduct((Axes[0].End - Axes[0].Start).GetSafeNormal(), Pivot->GetComponentQuat().RotateVector(LocalAxis))) > .9999);
	const FVector Reference = Mesh->GetComponentTransform().InverseTransformPosition(Arcs[0].Start);
	const FTransform Rest = Pivot->GetRelativeTransform();
	FR2aTestAccess::Apply(Part, EndValue);
	T.TestTrue(TEXT("Arc end follows runtime pose"), Arcs.Last().End.Equals(Mesh->GetComponentTransform().TransformPosition(Reference), .1));
	T.TestTrue(TEXT("Arrow at increasing path end"), Arcs.Last().bArrow && !Arcs[0].bArrow);
	Pivot->SetRelativeTransform(Rest);
	FR2aTestAccess::Apply(Part, EndValue * (31. / 32));
	const FVector Before = Mesh->GetComponentTransform().TransformPosition(Reference);
	T.TestTrue(TEXT("Arrow tangent follows runtime motion"), (Arcs.Last().End - Arcs.Last().Start).Equals(Arcs.Last().End - Before, .1));
	Pivot->SetRelativeTransform(Rest);
}

template<class TPart>
void PreviewMatches(FAutomationTestBase& T, TPart* Part, USceneComponent* Pivot, UStaticMeshComponent* Mesh, double First, double Last)
{
	Part->Preview = 0; Part->OnConstruction(Part->GetActorTransform());
	const FTransform Saved = Pivot->GetRelativeTransform(), Base = Mesh->GetComponentTransform();
	for (float Alpha : {.5f, 1.f})
	{
		Part->Preview = Alpha; Part->MotionDisplay->Refresh();
		const FTransform PreviewPose = Mesh->GetComponentTransform();
		Part->MotionDisplay->ClearPreview();
		// Runtime pose from the same authored zero frame (bStartOpen handled separately)
		if constexpr (std::is_same_v<TPart, AVBoRPoweredPanel>) { Pivot->SetRelativeTransform(Part->HingePoseAt(0)); }
		FR2aTestAccess::Apply(Part, FMath::Lerp(First, Last, double(Alpha)));
		Pose(T, PreviewPose, Mesh->GetComponentTransform());
		Pivot->SetRelativeTransform(Saved);
	}
	Part->Preview = 0; Part->MotionDisplay->Refresh(); Pose(T, Mesh->GetComponentTransform(), Base);
	T.TestTrue(TEXT("Authored pivot unchanged"), Pivot->GetRelativeTransform().Equals(Saved));
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FR2aE1, "vBoR.R2a.E1HingedPanel", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FR2aE1::RunTest(const FString&)
{
	FR2aWorld W;
	auto* P = W.World->SpawnActor<AVBoRHingedPanel>();
	P->SetActorTransform(FTransform(FRotator(13, 22, 7), FVector(120, -45, 60)));
	P->HingePivot->SetRelativeRotation(FRotator(0, 30, 166)); P->OnConstruction(P->GetActorTransform());
	ArcMatches(*this, P, P->HingePivot, P->DoorMesh, P->MaxAngleDeg, FVector::ZAxisVector); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FR2aE2, "vBoR.R2a.E2OtherHinges", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FR2aE2::RunTest(const FString&)
{
	FR2aWorld W;
	auto* G = W.World->SpawnActor<AVBoRPoweredPanel>(); ArcMatches(*this, G, G->HingePivot, G->PanelMesh, G->OpenAngleDeg, FVector::YAxisVector);
	auto* S = W.World->SpawnActor<AVBoRFoldingSeat>(); ArcMatches(*this, S, S->HingePivot, S->SeatMesh, S->FoldAngleDeg, FVector::YAxisVector);
	auto* M = W.World->SpawnActor<AVBoRFoldingMirror>(); M->HingeRotation = FRotator(24, 37, 166); M->FoldAngleDeg = -65; M->OnConstruction(M->GetActorTransform());
	ArcMatches(*this, M, M->HingePivot, M->MirrorMesh, M->FoldAngleDeg, FVector::ZAxisVector); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FR2aE3, "vBoR.R2a.E3AxisRotation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FR2aE3::RunTest(const FString&)
{
	FR2aWorld W;
	for (EAxis::Type Axis : {EAxis::X, EAxis::Y, EAxis::Z}) for (bool Reverse : {false, true})
	{
		auto* P = W.World->SpawnActor<AVBoRAxisPart>(); P->Axis = Axis; P->bReverse = Reverse;
		P->AxisPivot->SetRelativeRotation(FRotator(23, 47, 19)); P->OnConstruction(P->GetActorTransform());
		ArcMatches(*this, P, P->AxisPivot, P->PartMesh, P->MaxAngleDeg, Axis == EAxis::X ? FVector::XAxisVector : Axis == EAxis::Y ? FVector::YAxisVector : FVector::ZAxisVector);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FR2aE4, "vBoR.R2a.E4AxisTranslation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FR2aE4::RunTest(const FString&)
{
	FR2aWorld W;
	for (EAxis::Type Axis : {EAxis::X, EAxis::Y, EAxis::Z}) for (bool Reverse : {false, true})
	{
		auto* P = W.World->SpawnActor<AVBoRAxisPart>(); P->Axis = Axis; P->bReverse = Reverse; P->Movement = EVBoRAxisMovement::Translate; P->MaxTravelCm = 17;
		P->AxisPivot->SetRelativeRotation(FRotator(23, 47, 19)); P->OnConstruction(P->GetActorTransform());
		const auto Arrows = Kind(P->MotionDisplay, EVBoRGuideShape::Travel);
		if (!TestEqual(TEXT("One travel arrow"), Arrows.Num(), 1)) { continue; }
		const FVector Before = P->AxisPivot->GetComponentLocation(); FR2aTestAccess::Apply(P, P->MaxTravelCm);
		TestTrue(TEXT("Direction equals actual travel"), (Arrows[0].End - Arrows[0].Start).Equals(P->AxisPivot->GetComponentLocation() - Before, .1));
		TestTrue(TEXT("Length equals MaxTravelCm"), FMath::IsNearlyEqual(FVector::Distance(Arrows[0].Start, Arrows[0].End), double(P->MaxTravelCm), .1));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FR2aE5, "vBoR.R2a.E5Detents", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FR2aE5::RunTest(const FString&)
{
	FR2aWorld W;
	for (bool Reverse : {false, true})
	{
		auto* P = W.World->SpawnActor<AVBoRAxisPart>(); P->Release = EVBoRAxisRelease::Detent; P->bReverse = Reverse; P->OnConstruction(P->GetActorTransform());
		const auto Ticks = Kind(P->MotionDisplay, EVBoRGuideShape::Detent), Arc = Kind(P->MotionDisplay, EVBoRGuideShape::Arc);
		if (!TestEqual(TEXT("Three detent ticks"), Ticks.Num(), 3) || !TestEqual(TEXT("Arc segments"), Arc.Num(), 32)) { continue; }
		TestTrue(TEXT("Start tick thicker"), Ticks[1].Thickness > Ticks[0].Thickness && Ticks[1].Thickness > Ticks[2].Thickness);
		const FVector Reference = P->PartMesh->GetComponentTransform().InverseTransformPosition((Ticks[1].Start + Ticks[1].End) / 2);
		const FTransform Rest = P->AxisPivot->GetRelativeTransform();
		for (int32 I = 0; I < 3; ++I)
		{
			P->AxisPivot->SetRelativeTransform(Rest); FR2aTestAccess::Apply(P, P->DetentAnglesDeg[I]);
			TestTrue(TEXT("Tick follows actual detent pose"), ((Ticks[I].Start + Ticks[I].End) / 2).Equals(P->PartMesh->GetComponentTransform().TransformPosition(Reference), .1));
		}
		TestTrue(TEXT("Arc starts at first detent"), Arc[0].Start.Equals((Ticks[0].Start + Ticks[0].End) / 2, .1));
		TestTrue(TEXT("Arc ends at last detent"), Arc.Last().End.Equals((Ticks[2].Start + Ticks[2].End) / 2, .1));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FR2aE6, "vBoR.R2a.E6PushDirections", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FR2aE6::RunTest(const FString&)
{
	FR2aWorld W;
	auto* Door = W.World->SpawnActor<AVBoRHingedPanel>();
	for (bool AllowOpening : {false, true})
	{
		Door->bAllowPalmOpening = AllowOpening; Door->OnConstruction(Door->GetActorTransform());
		const auto Arrows = Kind(Door->MotionDisplay, EVBoRGuideShape::Push);
		TestEqual(TEXT("Opening face can be disabled"), Arrows.Num(), AllowOpening ? 2 : 1);
		for (const auto& Arrow : Arrows)
		{
			FVBoRDoorMotion Motion; Motion.bUseLatch = false; Motion.bLocked = false; Motion.bAllowPalmOpening = AllowOpening; Motion.AngleDeg = 30;
			const FVector Center = Door->HingePivot->GetComponentTransform().InverseTransformPosition(Arrow.End);
			const FVector Inward = Door->HingePivot->GetComponentTransform().InverseTransformVectorNoScale((Arrow.End - Arrow.Start).GetSafeNormal());
			FVBoRDoorHand Hands[2]; Hands[0].bTracked = true;
			const FQuat Rotation(FVector::ZAxisVector, FMath::DegreesToRadians(30.));
			Hands[0].PalmNormal = Rotation.RotateVector(Inward);
			for (auto& Point : Hands[0].Points) { Point = Rotation.RotateVector(Center - Inward); }
			Motion.Advance(Hands, .1);
			for (auto& Point : Hands[0].Points) { Point = Rotation.RotateVector(Center + Inward); }
			Motion.Advance(Hands, .1);
			TestTrue(TEXT("Door arrow color equals actual push sign"), Arrow.Color == FColor::Green ? Motion.AngleDeg > 30 : Motion.AngleDeg < 30);
		}
	}
	for (bool Reverse : {false, true})
	{
		auto* P = W.World->SpawnActor<AVBoRAxisPart>(); P->Release = EVBoRAxisRelease::Detent; P->bAllowPush = true; P->bReverse = Reverse;
		P->MeshRestFromRoot = FTransform(FQuat::Identity, FVector(0, 10, 0), FVector(.02, .2, .02)); P->OnConstruction(P->GetActorTransform());
		const auto Arrows = Kind(P->MotionDisplay, EVBoRGuideShape::Push); TestEqual(TEXT("Two axis push faces"), Arrows.Num(), 2);
		for (const auto& Arrow : Arrows)
		{
			P->AxisPivot->SetRelativeTransform(FTransform::Identity); FR2aTestAccess::Apply(P, 0); FR2aTestAccess::SetupPush(P);
			const FVector Inward = (Arrow.End - Arrow.Start).GetSafeNormal(); FVBoRDoorHand Hands[2]; Hands[0].bTracked = true;
			for (auto& Point : Hands[0].Points) { Point = Arrow.End - Inward; } FR2aTestAccess::Push(P, Hands);
			for (auto& Point : Hands[0].Points) { Point = Arrow.End + Inward; } FR2aTestAccess::Push(P, Hands);
			TestTrue(TEXT("Axis arrow color equals actual push sign"), Arrow.Color == FColor::Green ? FR2aTestAccess::Value(P) > 0 : FR2aTestAccess::Value(P) < 0);
		}
		P->bAllowPush = false; TestEqual(TEXT("Push disabled hides arrows"), Kind(P->MotionDisplay, EVBoRGuideShape::Push).Num(), 0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FR2aE7, "vBoR.R2a.E7GripAndMissingMesh", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FR2aE7::RunTest(const FString&)
{
	FR2aWorld W; auto* P = W.World->SpawnActor<AVBoRAxisPart>(); P->DoorHandleSide = EVBoRDoorHandleSide::Grip;
	P->JudgmentGuide = EVBoRMotionGuide::Off;
	TestEqual(TEXT("Grip has no guide"), P->MotionDisplay->BuildShapes().Num(), 0);
	auto Check = [&](UVBoRMotionGuideComponent* Guide, USceneComponent* Pivot)
	{
		const auto Axis = Kind(Guide, EVBoRGuideShape::Axis), Arc = Kind(Guide, EVBoRGuideShape::Arc);
		if (!TestEqual(TEXT("Fallback axis"), Axis.Num(), 1) || !TestEqual(TEXT("Fallback arc"), Arc.Num(), 32)) { return; }
		TestTrue(TEXT("Fallback axis 20 cm"), FMath::IsNearlyEqual(FVector::Distance(Axis[0].Start, Axis[0].End), 20., .01));
		TestTrue(TEXT("Fallback radius 5 cm"), FMath::IsNearlyEqual(FVector::Distance(Arc[0].Start, Pivot->GetComponentLocation()), 5., .01));
	};
	auto* H = W.World->SpawnActor<AVBoRHingedPanel>(); H->DoorMesh->SetStaticMesh(nullptr); Check(H->MotionDisplay, H->HingePivot);
	auto* G = W.World->SpawnActor<AVBoRPoweredPanel>(); G->PanelMesh->SetStaticMesh(nullptr); Check(G->MotionDisplay, G->HingePivot);
	auto* M = W.World->SpawnActor<AVBoRFoldingMirror>(); M->MirrorMesh->SetStaticMesh(nullptr); Check(M->MotionDisplay, M->HingePivot);
	auto* S = W.World->SpawnActor<AVBoRFoldingSeat>(); S->SeatMesh->SetStaticMesh(nullptr); Check(S->MotionDisplay, S->HingePivot);
	P->DoorHandleSide = EVBoRDoorHandleSide::None; P->PartMesh->SetStaticMesh(nullptr); Check(P->MotionDisplay, P->AxisPivot); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FR2aE8, "vBoR.R2a.E8PreviewPoses", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FR2aE8::RunTest(const FString&)
{
	FR2aWorld W;
	auto* H = W.World->SpawnActor<AVBoRHingedPanel>(); H->HingePivot->SetRelativeRotation(FRotator(0, 30, 166)); PreviewMatches(*this, H, H->HingePivot, H->DoorMesh, 0, H->MaxAngleDeg);
	auto* G = W.World->SpawnActor<AVBoRPoweredPanel>(); G->bStartOpen = true; PreviewMatches(*this, G, G->HingePivot, G->PanelMesh, 0, G->OpenAngleDeg);
	auto* M = W.World->SpawnActor<AVBoRFoldingMirror>(); M->HingeRotation = FRotator(24, 37, 166); PreviewMatches(*this, M, M->HingePivot, M->MirrorMesh, 0, M->FoldAngleDeg);
	auto* S = W.World->SpawnActor<AVBoRFoldingSeat>(); PreviewMatches(*this, S, S->HingePivot, S->SeatMesh, 0, S->FoldAngleDeg);
	for (bool Reverse : {false, true}) for (bool Translate : {false, true})
	{
		auto* A = W.World->SpawnActor<AVBoRAxisPart>(); A->bReverse = Reverse; A->Movement = Translate ? EVBoRAxisMovement::Translate : EVBoRAxisMovement::Rotate;
		A->AxisPivot->SetRelativeRotation(FRotator(15, 25, 35)); PreviewMatches(*this, A, A->AxisPivot, A->PartMesh, 0, Translate ? A->MaxTravelCm : A->MaxAngleDeg);
		if (!Translate) { A->Release = EVBoRAxisRelease::Detent; PreviewMatches(*this, A, A->AxisPivot, A->PartMesh, -20, 20); }
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FR2aE9, "vBoR.R2a.E9AuthoredValues", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FR2aE9::RunTest(const FString&)
{
	FR2aWorld W; auto* H = W.World->SpawnActor<AVBoRHingedPanel>(); H->bUsePushFaceBox = true;
	H->PushFaceBox->SetRelativeLocation(FVector(30, 1, 50)); H->PushFaceBox->SetBoxExtent(FVector(25, 2, 45));
	const FTransform Pivot = H->HingePivot->GetRelativeTransform(), Rest = H->MeshRestFromRoot, Face = H->PushFaceBox->GetRelativeTransform();
	const FVector Extent = H->PushFaceBox->GetUnscaledBoxExtent();
	auto* Child = NewObject<UStaticMeshComponent>(H); Child->SetupAttachment(H->HingePivot); Child->SetStaticMesh(H->DoorMesh->GetStaticMesh()); Child->SetRelativeLocation(FVector(40, 0, 20)); Child->RegisterComponent();
	const FTransform ChildRelative = Child->GetRelativeTransform();
	for (float Alpha : {1.f, 0.f})
	{
		H->Preview = Alpha; H->MotionDisplay->Refresh();
		TestTrue(TEXT("Hinge relative unchanged"), H->HingePivot->GetRelativeTransform().Equals(Pivot));
		TestTrue(TEXT("MeshRest unchanged"), H->MeshRestFromRoot.Equals(Rest));
		TestTrue(TEXT("Face transform unchanged"), H->PushFaceBox->GetRelativeTransform().Equals(Face));
		TestTrue(TEXT("Face extent unchanged"), H->PushFaceBox->GetUnscaledBoxExtent().Equals(Extent));
		TestTrue(TEXT("Child follows without saved changes"), Child->GetRelativeTransform().Equals(ChildRelative) && Child->GetComponentTransform().Equals(ChildRelative * H->HingePivot->GetComponentTransform()));
	}
	H->Preview = 1; H->MaxAngleDeg = 45; H->HingePivot->SetRelativeLocation(FVector(12, 6, 3)); H->OnConstruction(H->GetActorTransform());
	TestTrue(TEXT("Only authored hinge location changed"), H->HingePivot->GetRelativeLocation().Equals(FVector(12, 6, 3)) && H->HingePivot->GetRelativeRotation().IsNearlyZero());
	TestTrue(TEXT("New limit used"), H->HingePivot->GetComponentQuat().Equals(FQuat(FVector::ZAxisVector, FMath::DegreesToRadians(45.)), 1.e-6));
	TestTrue(TEXT("Rest and push box still authored"), H->MeshRestFromRoot.Equals(Rest) && H->PushFaceBox->GetRelativeTransform().Equals(Face));
	auto* A = W.World->SpawnActor<AVBoRAxisPart>(); const FTransform Axis = A->AxisPivot->GetRelativeTransform();
	A->Preview = 1; A->MotionDisplay->Refresh(); TestTrue(TEXT("Axis relative unchanged"), A->AxisPivot->GetRelativeTransform().Equals(Axis));
	auto* G = W.World->SpawnActor<AVBoRPoweredPanel>(); auto* Box = NewObject<UVBoRObstructionBoxComponent>(G); Box->SetupAttachment(G->HingePivot); Box->RegisterComponent();
	const FTransform BoxRest = Box->GetRelativeTransform(); const FVector BoxExtent = Box->GetUnscaledBoxExtent(); G->Preview = 1; G->MotionDisplay->Refresh();
	TestTrue(TEXT("Obstruction box authored data unchanged"), Box->GetRelativeTransform().Equals(BoxRest) && Box->GetUnscaledBoxExtent().Equals(BoxExtent));
	TestTrue(TEXT("Preview property transient"), FindFProperty<FProperty>(H->GetClass(), TEXT("Preview"))->HasAnyPropertyFlags(CPF_Transient));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FR2aE10, "vBoR.R2a.E10RuntimeIsolation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FR2aE10::RunTest(const FString&)
{
	for (float Preview : {0.f, 1.f})
	{
		FR2aWorld W(EWorldType::Game);
		auto* H = W.World->SpawnActor<AVBoRHingedDoor>(); H->Preview = Preview;
		auto* G = W.World->SpawnActor<AVBoRPoweredPanel>(); G->Preview = Preview; G->bStartOpen = true; G->OnConstruction(G->GetActorTransform());
		auto* M = W.World->SpawnActor<AVBoRFoldingMirror>(); M->Preview = Preview;
		auto* S = W.World->SpawnActor<AVBoRFoldingSeat>(); S->Preview = Preview;
		auto* A = W.World->SpawnActor<AVBoRAxisPart>(); A->Preview = Preview; A->Release = EVBoRAxisRelease::Detent; A->DetentStartIndex = 0;
		auto* E = NewObject<UR2aTestObserver>(H);
		H->OnAngleChanged.AddDynamic(E, &UR2aTestObserver::Angle); H->OnPanelUnlocked.AddDynamic(E, &UR2aTestObserver::Panel); H->OnPanelLocked.AddDynamic(E, &UR2aTestObserver::Panel);
		G->OnStateChanged.AddDynamic(E, &UR2aTestObserver::Powered);
		A->OnPartOpened.AddDynamic(E, &UR2aTestObserver::Axis); A->OnPartClosed.AddDynamic(E, &UR2aTestObserver::Axis); A->OnDetentChanged.AddDynamic(E, &UR2aTestObserver::Detent);
		W.Play();
		TestTrue(TEXT("Door closed and locked"), H->GetMotion().AngleDeg == 0 && H->GetMotion().bLocked);
		TestTrue(TEXT("Gate preserves bStartOpen"), G->GetState() == EVBoRPoweredState::Open && G->GetAngleDeg() == G->OpenAngleDeg);
		TestTrue(TEXT("Mirror starts folded -65 deg"), M->GetState() == EVBoRFoldingMirrorState::Folded && M->GetAngleDeg() == -65);
		TestTrue(TEXT("Seat starts standing"), S->GetState() == EVBoRFoldingSeatState::Standing && S->GetAngleDeg() == 0);
		TestTrue(TEXT("Detent starts -20 deg"), A->GetDetentIndex() == 0 && FR2aTestAccess::Value(A) == -20);
		const int32 Events = E->Events;
		auto Check = [&](auto* P, USceneComponent* Pivot)
		{
			const FTransform Before = Pivot->GetComponentTransform(); P->Preview = 1 - Preview; P->MotionDisplay->Refresh(); P->MotionDisplay->TickComponent(.016f, LEVELTICK_All, nullptr);
			Pose(*this, Pivot->GetComponentTransform(), Before); TestEqual(TEXT("Game has no guide geometry"), P->MotionDisplay->BuildShapes().Num(), 0);
		};
		Check(H, H->HingePivot); Check(G, G->HingePivot); Check(M, M->HingePivot); Check(S, S->HingePivot); Check(A, A->AxisPivot);
		TestEqual(TEXT("Preview emits no runtime events"), E->Events, Events);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FR2aE11, "vBoR.R2a.E11DisplayFlags", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FR2aE11::RunTest(const FString&)
{
	FR2aWorld W;
	auto Check = [&](auto* P)
	{
		auto* C = P->MotionDisplay.Get(); TestNotNull(TEXT("Inherited motion display"), C);
		if constexpr (std::is_same_v<std::remove_pointer_t<decltype(P)>, AVBoRAxisPart> || std::is_same_v<std::remove_pointer_t<decltype(P)>, AVBoRHingedPanel> || std::is_same_v<std::remove_pointer_t<decltype(P)>, AVBoRPoweredPanel>) { P->JudgmentGuide = EVBoRMotionGuide::Off; }
		TestTrue(TEXT("Editor-only and hidden in game"), C->IsEditorOnly() && C->bHiddenInGame);
		TestTrue(TEXT("No collision, overlap, shadow or navigation"), C->GetCollisionEnabled() == ECollisionEnabled::NoCollision && !C->GetGenerateOverlapEvents() && !C->CastShadow && !C->CanEverAffectNavigation());
		TestTrue(TEXT("Visible in editor"), C->IsVisibleInEditor());
		P->MotionGuide = EVBoRMotionGuide::Off; C->Refresh(); TestEqual(TEXT("Off clears geometry"), C->BuildShapes().Num(), 0); TestEqual(TEXT("Off clears rendered lines"), C->BatchedLines.Num(), 0);
		P->MotionGuide = EVBoRMotionGuide::SelectedOnly; C->Refresh(); TestEqual(TEXT("Unselected has no shapes"), C->BuildShapes().Num(), 0);
		P->MotionGuide = EVBoRMotionGuide::Always; C->Refresh(); TestTrue(TEXT("Always has rendered lines"), C->BatchedLines.Num() > 0);
	};
	Check(W.World->SpawnActor<AVBoRHingedPanel>()); Check(W.World->SpawnActor<AVBoRPoweredPanel>()); Check(W.World->SpawnActor<AVBoRFoldingMirror>()); Check(W.World->SpawnActor<AVBoRFoldingSeat>()); Check(W.World->SpawnActor<AVBoRAxisPart>());
	return true;
}
#endif
