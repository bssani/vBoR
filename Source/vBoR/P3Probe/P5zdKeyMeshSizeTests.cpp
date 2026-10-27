#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "../Interaction/VBoRKeyFob.h"
#include "../Interaction/VBoRPokeButton.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "IsdkContentAssetPaths.h"

namespace
{
	struct FP5zdWorld
	{
		UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
		void Play() { World->InitializeActorsForPlay(FURL()); World->GetWorldSettings()->NotifyBeginPlay(); }
		~FP5zdWorld() { World->EndPlay(EEndPlayReason::Quit); World->DestroyWorld(false); }
	};
	AVBoRKeyFob* P5zdKey(FP5zdWorld& W, const TCHAR* MeshPath)
	{
		const FTransform Pose(FRotator(0, 30, 0), FVector(50, -20, 80));
		AVBoRKeyFob* Key = W.World->SpawnActorDeferred<AVBoRKeyFob>(AVBoRKeyFob::StaticClass(), Pose);
		if (MeshPath) { Key->KeyMesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, MeshPath)); }
		Key->FinishSpawning(Pose);
		return Key;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5zdCubeKeepsApproxSize, "vBoR.P5zd.E1CubeKeepsApproxSize", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5zdCubeKeepsApproxSize::RunTest(const FString&)
{
	FP5zdWorld W; AVBoRKeyFob* Key = P5zdKey(W, nullptr);
	TestTrue(TEXT("Placeholder cube scaled to KeySizeCm"), Key->GetActorScale3D().Equals(Key->KeySizeCm / 100.0, 0.0001));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5zdRealMeshKeepsOwnSize, "vBoR.P5zd.E2RealMeshKeepsOwnSize", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5zdRealMeshKeepsOwnSize::RunTest(const FString&)
{
	FP5zdWorld W; AVBoRKeyFob* Key = P5zdKey(W, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	TestTrue(TEXT("Real mesh scale untouched"), Key->GetActorScale3D().Equals(FVector::OneVector, 0.0001));
	W.Play();
	// Buttons at cm offsets from key root, same as before (P4a D3)
	for (const bool bUnlock : {true, false})
	{
		const AVBoRPokeButton* Button = bUnlock ? Key->UnlockButton.Get() : Key->LockButton.Get();
		if (!TestNotNull(TEXT("Button spawned"), Button)) { return false; }
		const FVector Expected = Key->GetActorLocation() + Key->GetActorQuat().RotateVector(bUnlock ? Key->UnlockButtonCm : Key->LockButtonCm);
		TestTrue(TEXT("Button at cm offset"), Button->GetActorLocation().Equals(Expected, 0.001));
	}
	return true;
}
#endif
