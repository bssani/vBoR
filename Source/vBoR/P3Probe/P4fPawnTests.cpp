#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "../Interaction/VBoRPawn.h"
#include "../Interaction/VBoRGameMode.h"
#include "P3FlowGameMode.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "Interaction/IsdkGrabberComponent.h"
#include "IsdkContentAssetPaths.h"
#include "IsdkHandMeshComponent.h"
#include "Materials/Material.h"
#include "Rig/IsdkGrabInteractionRigComponent.h"
#include "Rig/IsdkHandVisualsRigComponent.h"
#include "Rig/IsdkRayInteractionRigComponent.h"

struct FP4fPawnTestAccess
{
	static void Check(AVBoRPawn* Pawn) { Pawn->CheckHandMeshMappings(); }
};

namespace
{
	struct FP4fWorld
	{
		UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
		void Play() { World->InitializeActorsForPlay(FURL()); World->GetWorldSettings()->NotifyBeginPlay(); }
		~FP4fWorld() { World->EndPlay(EEndPlayReason::Quit); World->DestroyWorld(false); }
	};
	USkeletalMesh* P4fDefaultMesh(int32 Hand)
	{
		return LoadObject<USkeletalMesh>(nullptr, Hand == 0 ? IsdkContentAssetPaths::Models::Hand::OpenXRLeftHand : IsdkContentAssetPaths::Models::Hand::OpenXRRightHand);
	}
	void P4fCheckVisuals(FAutomationTestBase& Test, AVBoRPawn* Pawn, int32 Hand, USkeletalMesh* Mesh, UMaterialInterface* Material = nullptr)
	{
		auto* Visuals = Pawn->GetHandRig(Hand)->GetHandVisuals();
		for (auto* Visual : {Visuals->TrackedHandVisual.Get(), Visuals->SyntheticHandVisual.Get()})
		{
			Test.TestTrue(TEXT("Both visuals use the expected mesh"), Visual->GetSkinnedAsset() == Mesh);
			Test.TestTrue(TEXT("Mapping is not invalid"), Visual->GetMappingState() != EIsdkSkeletonMappingState::Invalid);
			Test.TestEqual(TEXT("Every mesh material slot is present"), Visual->GetNumMaterials(), Mesh->GetMaterials().Num());
			for (int32 Slot = 0; Slot < Visual->GetNumMaterials(); ++Slot)
			{
				Test.TestTrue(TEXT("Every slot uses the expected material"), Visual->GetMaterial(Slot) == (Material ? Material : Mesh->GetMaterials()[Slot].MaterialInterface.Get()));
			}
		}
		Test.TestFalse(TEXT("Tracked hand stays hidden"), Visuals->TrackedHandVisual->IsVisible());
		Test.TestTrue(TEXT("Synthetic hand stays visible"), Visuals->SyntheticHandVisual->IsVisible());
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP4fRig, "vBoR.P4f.E1PawnRig", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP4fRig::RunTest(const FString&)
{
	FP4fWorld W;
	AVBoRPawn* Main = W.World->SpawnActor<AVBoRPawn>();
	AP3PalmGrabPawn* P3 = W.World->SpawnActor<AP3PalmGrabPawn>(); W.Play();
	TestTrue(TEXT("P3 pawn inherits main pawn"), P3->IsA<AVBoRPawn>());
	for (AVBoRPawn* Pawn : {Main, static_cast<AVBoRPawn*>(P3)})
	{
		TestTrue(TEXT("Palm enabled"), Pawn->bAllowPalmGrab);
		for (int32 Hand = 0; Hand < 2; ++Hand)
		{
			auto* Rig = Pawn->GetHandRig(Hand); auto* Grabber = Rig->GetGrabber();
			TestTrue(TEXT("Grab rig active"), Rig->GetGrabInteraction()->IsActive());
			TestTrue(TEXT("Palm allowed"), Grabber->IsGrabInputMethodAllowed(EIsdkGrabInputMethod::Palm));
			TestTrue(TEXT("Pinch allowed"), Grabber->IsGrabInputMethodAllowed(EIsdkGrabInputMethod::Pinch));
			TestTrue(TEXT("Near grab allowed"), Grabber->IsGrabDetectionTypeAllowed(EIsdkGrabDetectorType::HandGrab));
			TestFalse(TEXT("Distance blocked"), Grabber->IsGrabDetectionTypeAllowed(EIsdkGrabDetectorType::DistanceGrab));
			TestFalse(TEXT("Ray grab blocked"), Grabber->IsGrabDetectionTypeAllowed(EIsdkGrabDetectorType::RayGrab));
			TestFalse(TEXT("Ray rig inactive"), Rig->GetRayInteraction()->IsActive());
		}
	}
	TestFalse(TEXT("Base pawn stays poke only"), GetDefault<AVBoRHandRigPawn>()->bAllowPalmGrab);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP4fDefault, "vBoR.P4f.E2DefaultVisuals", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP4fDefault::RunTest(const FString&)
{
	FP4fWorld W; AVBoRPawn* Pawn = W.World->SpawnActor<AVBoRPawn>(); W.Play();
	TestFalse(TEXT("No mesh overrides"), Pawn->LeftHandMesh || Pawn->RightHandMesh);
	TestFalse(TEXT("No material overrides"), Pawn->LeftHandMaterial || Pawn->RightHandMaterial);
	P4fCheckVisuals(*this, Pawn, 0, P4fDefaultMesh(0)); P4fCheckVisuals(*this, Pawn, 1, P4fDefaultMesh(1));
	// Parent default Pawn Tick (preserve BP child Event Tick, P4f review R1-1)
	const FActorTickFunction& BaseTick = GetDefault<AVBoRHandRigPawn>()->PrimaryActorTick;
	TestTrue(TEXT("Actor tick settings stay as the base pawn"), Pawn->PrimaryActorTick.bCanEverTick == BaseTick.bCanEverTick
		&& Pawn->PrimaryActorTick.bStartWithTickEnabled == BaseTick.bStartWithTickEnabled && Pawn->PrimaryActorTick.TickGroup == BaseTick.TickGroup);
	TestTrue(TEXT("Actor tick stays enabled after BeginPlay"), Pawn->IsActorTickEnabled());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP4fMesh, "vBoR.P4f.E3CompatibleMesh", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP4fMesh::RunTest(const FString&)
{
	FP4fWorld W; AVBoRPawn* Pawn = W.World->SpawnActor<AVBoRPawn>();
	Pawn->LeftHandMesh = P4fDefaultMesh(1); W.Play();
	P4fCheckVisuals(*this, Pawn, 0, P4fDefaultMesh(1)); P4fCheckVisuals(*this, Pawn, 1, P4fDefaultMesh(1));
	for (auto* Visual : {Pawn->LeftRig->GetHandVisuals()->TrackedHandVisual.Get(), Pawn->LeftRig->GetHandVisuals()->SyntheticHandVisual.Get()})
	{
		TestTrue(TEXT("Real SDK mapping succeeds"), Visual->GetMappingState() == EIsdkSkeletonMappingState::Valid);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP4fFallback, "vBoR.P4f.E4InvalidMeshFallback", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP4fFallback::RunTest(const FString&)
{
	// Expect SDK bone mapping errors from each visual too (P4f E4, no plugin changes)
	AddExpectedError(TEXT("Bone XRHand_Palm has no valid bone name mapped to the skeleton."), EAutomationExpectedErrorFlags::Contains, 2);
	AddExpectedMessagePlain(TEXT("Right hand mesh SkeletalCube bones do not match XRHand_* - using default hand"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 1);
	FP4fWorld W; AVBoRPawn* Pawn = W.World->SpawnActor<AVBoRPawn>();
	Pawn->RightHandMesh = LoadObject<USkeletalMesh>(nullptr, TEXT("/Engine/EngineMeshes/SkeletalCube.SkeletalCube"));
	if (!TestNotNull(TEXT("Incompatible fixture exists"), Pawn->RightHandMesh.Get())) { return false; }
	Pawn->RightHandMaterial = UMaterial::GetDefaultMaterial(MD_Surface);
	Pawn->LeftHandMaterial = Pawn->RightHandMaterial; W.Play();
	P4fCheckVisuals(*this, Pawn, 1, P4fDefaultMesh(1));
	P4fCheckVisuals(*this, Pawn, 0, P4fDefaultMesh(0), Pawn->LeftHandMaterial);
	FP4fPawnTestAccess::Check(Pawn); Pawn->Tick(0.016f); FP4fPawnTestAccess::Check(Pawn);
	P4fCheckVisuals(*this, Pawn, 1, P4fDefaultMesh(1));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP4fMaterial, "vBoR.P4f.E5MaterialOnly", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP4fMaterial::RunTest(const FString&)
{
	FP4fWorld W; AVBoRPawn* Pawn = W.World->SpawnActor<AVBoRPawn>();
	Pawn->LeftHandMaterial = UMaterial::GetDefaultMaterial(MD_Surface); W.Play();
	P4fCheckVisuals(*this, Pawn, 0, P4fDefaultMesh(0), Pawn->LeftHandMaterial);
	P4fCheckVisuals(*this, Pawn, 1, P4fDefaultMesh(1)); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP4fMeshMaterial, "vBoR.P4f.E5MeshAndAllMaterials", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP4fMeshMaterial::RunTest(const FString&)
{
	FP4fWorld W; AVBoRPawn* Pawn = W.World->SpawnActor<AVBoRPawn>();
	USkeletalMesh* Mesh = DuplicateObject<USkeletalMesh>(P4fDefaultMesh(0), GetTransientPackage());
	const FSkeletalMaterial ExtraSlot = Mesh->GetMaterials()[0];
	Mesh->GetMaterials().Add(ExtraSlot);
	Pawn->RightHandMesh = Mesh; Pawn->RightHandMaterial = UMaterial::GetDefaultMaterial(MD_Surface); W.Play();
	TestTrue(TEXT("Multiple material slots exercised"), Mesh->GetMaterials().Num() > 1);
	P4fCheckVisuals(*this, Pawn, 1, Mesh, Pawn->RightHandMaterial);
	P4fCheckVisuals(*this, Pawn, 0, P4fDefaultMesh(0)); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP4fMode, "vBoR.P4f.E6GameModes", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP4fMode::RunTest(const FString&)
{
	TestTrue(TEXT("Main GameMode uses main pawn"), GetDefault<AVBoRGameMode>()->DefaultPawnClass == AVBoRPawn::StaticClass());
	TestTrue(TEXT("P3 GameMode unchanged"), GetDefault<AP3FlowGameMode>()->DefaultPawnClass == AP3PalmGrabPawn::StaticClass()); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP4fBlueprints, "vBoR.P4f.E7Blueprints", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP4fBlueprints::RunTest(const FString&)
{
	UClass* PawnClass = LoadClass<AVBoRPawn>(nullptr, TEXT("/Game/Main/Core/BP_vBoRPawn.BP_vBoRPawn_C"));
	UClass* ModeClass = LoadClass<AVBoRGameMode>(nullptr, TEXT("/Game/Main/Core/BP_vBoRGameMode.BP_vBoRGameMode_C"));
	if (!TestNotNull(TEXT("Pawn Blueprint loads"), PawnClass) || !TestNotNull(TEXT("GameMode Blueprint loads"), ModeClass)) { return false; }
	TestTrue(TEXT("Pawn Blueprint parent"), PawnClass->GetSuperClass() == AVBoRPawn::StaticClass());
	TestTrue(TEXT("GameMode Blueprint parent"), ModeClass->GetSuperClass() == AVBoRGameMode::StaticClass());
	const auto* Pawn = PawnClass->GetDefaultObject<AVBoRPawn>();
	TestFalse(TEXT("Blueprint mesh properties empty"), Pawn->LeftHandMesh || Pawn->RightHandMesh);
	TestFalse(TEXT("Blueprint material properties empty"), Pawn->LeftHandMaterial || Pawn->RightHandMaterial);
	TestTrue(TEXT("GameMode Blueprint uses pawn Blueprint"), ModeClass->GetDefaultObject<AVBoRGameMode>()->DefaultPawnClass == PawnClass);
	FP4fWorld W; auto* Instance = W.World->SpawnActor<AVBoRPawn>(PawnClass);
	for (int32 Hand = 0; Hand < 2; ++Hand)
	{
		const auto* Parent = Instance->GetHandRig(Hand)->GetHandVisuals()->GetAttachParent();
		TestTrue(TEXT("Blueprint visual attaches to its own instance"), Parent && Parent->GetOwner() == Instance);
	}
	W.Play();
	TestTrue(TEXT("Blueprint pawn keeps actor tick for Event Tick"), Instance->IsActorTickEnabled());
	P4fCheckVisuals(*this, Instance, 0, P4fDefaultMesh(0)); P4fCheckVisuals(*this, Instance, 1, P4fDefaultMesh(1)); return true;
}
#endif
