#if WITH_DEV_AUTOMATION_TESTS
#include "../P3Probe/P4hTestFixture.h"
#include "../P3Probe/P3VehicleMock.h"
#include "../Interaction/VBoRAlignPointComponent.h"
#include "../Interaction/VBoREyePointComponent.h"
#include "../Interaction/VBoRScenarioRunner.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/WorldSettings.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "HeadMountedDisplayTypes.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"

struct FM3bTestAccess
{
	static void Hook(UVBoRVehicleAlignmentComponent* A, int32& Requests)
	{ A->AnchorRequestForTest = [&Requests] { ++Requests; }; }
	static void Load(UVBoRVehicleAlignmentComponent* A) { A->LoadSetupAlignment(true); }
	static void Found(UVBoRVehicleAlignmentComponent* A, const FTransform& Stage) { A->FinishAnchorDiscovery(&Stage, 0); }
	static void Timeout(UVBoRVehicleAlignmentComponent* A) { A->AnchorStarted = -11; A->AdvanceAnchorDiscovery(); }
	static void Points(UVBoRVehicleAlignmentComponent* A, const FVBoRBuckSavedAlignment& S)
	{ A->CapturedPoints = S.Points; A->PointOrigin = S.TrackingOrigin; A->PointPlayArea = S.PlayAreaCm; A->bPointSession = true; }
};

namespace
{
	constexpr auto M3bFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;
	struct FM3bWorld
	{
		UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
		APawn* Pawn;
		AP3VehicleMock* Car;
		UP4hTestAlignment* A;
		AVBoRScenarioRunner* Runner;
		int32 Requests = 0;
		IConsoleVariable* Switch = IConsoleManager::Get().FindConsoleVariable(TEXT("vBoR.AnchorAlignment"));
		int32 Previous = Switch->GetInt();
		FString Path = FPaths::ProjectSavedDir() / TEXT("Automation") / (FGuid::NewGuid().ToString() + TEXT("-M3b.json"));
		FM3bWorld()
		{
			Switch->Set(0, ECVF_SetByCode);
			GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
			auto* PC = World->SpawnActor<APlayerController>();
			Pawn = World->SpawnActor<APawn>();
			Pawn->SetRootComponent(NewObject<USceneComponent>(Pawn)); Pawn->GetRootComponent()->RegisterComponent(); PC->Possess(Pawn);
			Car = World->SpawnActor<AP3VehicleMock>(); Car->Alignment->DestroyComponent();
			A = NewObject<UP4hTestAlignment>(Car); Car->AddInstanceComponent(A); Car->Alignment = A; A->BaseEye = Car->DriverEye;
			A->TestPath = Path; A->Origin = EHMDTrackingOrigin::Stage; A->RegisterComponent();
			FM3bTestAccess::Hook(A, Requests);
			Runner = World->SpawnActor<AVBoRScenarioRunner>(); Runner->Scenario = NewObject<UVBoRScenarioAsset>(Runner);
			FVBoRScenarioStep Step; Step.StepId = TEXT("01"); Step.ConfirmBy = EVBoRConfirmBy::Either; Runner->Scenario->Steps = {Step};
		}
		~FM3bWorld()
		{
			World->EndPlay(EEndPlayReason::Quit); GEngine->DestroyWorldContext(World); World->DestroyWorld(false);
			IFileManager::Get().Delete(*Path); Switch->Set(Previous, ECVF_SetByCode);
		}
		FVBoRBuckSavedAlignment Value(bool bAnchor = true) const
		{
			FVBoRBuckSavedAlignment S; S.Version = 2; S.SavedAt = TEXT("2026-11-18T00:00:00Z");
			S.TrackingOrigin = EHMDTrackingOrigin::Stage; S.PlayAreaCm = A->Area;
			for (UVBoRAlignPointComponent* P : Car->AlignPoints)
			{ FVBoRAlignPointSample Point; Point.Order = P->Order; Point.StageCm = P->GetComponentLocation(); Point.Frames = 15; S.Points.Add(Point); }
			if (bAnchor) { S.AnchorUuid = TEXT("00112233445566778899aabbccddeeff"); S.AnchorStage = FTransform(FVector(10,20,50)); }
			return S;
		}
		void Start(const FVBoRBuckSavedAlignment& S, bool bEnabled)
		{
			FString Why; FVBoRBuckAlignmentStore::Save(Path, S, Why); Switch->Set(bEnabled ? 1 : 0, ECVF_SetByCode);
			World->InitializeActorsForPlay(FURL()); World->GetWorldSettings()->NotifyBeginPlay(); Runner->InitializeFlow();
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FM3bOff, "vBoR.M3b.E1Disabled", M3bFlags)
bool FM3bOff::RunTest(const FString&)
{
	FM3bWorld W; W.Start(W.Value(), false);
	TestEqual(TEXT("No discovery request"), W.Requests, 0);
	TestTrue(TEXT("Legacy point alignment applied"), W.A->State == EVBoRAlignmentState::Applied);
	TestTrue(TEXT("Legacy Pawn unchanged"), W.Pawn->GetActorTransform().Equals(FTransform::Identity, 0.001));
	FString Why; TestTrue(TEXT("Marker save succeeds"), W.A->SaveMarkerPoints(FString(), W.Value(false).Points, Why));
	TestEqual(TEXT("No create request"), W.Requests, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FM3bStore, "vBoR.M3b.E2OptionalAnchor", M3bFlags)
bool FM3bStore::RunTest(const FString&)
{
	FM3bWorld W; FString Why; FVBoRBuckSavedAlignment Out;
	for (bool bAnchor : {true, false})
	{
		auto S = W.Value(bAnchor);
		TestTrue(TEXT("Save"), FVBoRBuckAlignmentStore::Save(W.Path, S, Why));
		TestTrue(TEXT("Load"), FVBoRBuckAlignmentStore::Load(W.Path, Out, Why));
		TestEqual(TEXT("Version unchanged"), Out.Version, 2);
		TestEqual(TEXT("Optional UUID roundtrip"), Out.AnchorUuid, S.AnchorUuid);
		if (bAnchor) { TestTrue(TEXT("Stage pose cm and deg roundtrip"), Out.AnchorStage.Equals(S.AnchorStage)); }
	}
	auto Invalid = W.Value(); Invalid.AnchorUuid = TEXT("bad UUID");
	TestFalse(TEXT("Invalid anchor UUID rejected on save"), FVBoRBuckAlignmentStore::Save(W.Path, Invalid, Why));
	W.Start(W.Value(false), true);
	TestEqual(TEXT("Old file needs no discovery even when enabled"), W.Requests, 0);
	TestTrue(TEXT("Old file still applied"), W.A->State == EVBoRAlignmentState::Applied);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FM3bSave, "vBoR.M3b.SaveRequestsAfterPointSave", M3bFlags)
bool FM3bSave::RunTest(const FString&)
{
	FM3bWorld W; auto S = W.Value(false); W.Start(S, true);
	FString Why; FVBoRBuckSavedAlignment Loaded;
	TestTrue(TEXT("Marker save succeeds before anchor request"), W.A->SaveMarkerPoints(FString(), S.Points, Why));
	TestEqual(TEXT("Marker save requests one anchor"), W.Requests, 1);
	TestTrue(TEXT("Successful point file remains readable while anchor pending"), FVBoRBuckAlignmentStore::Load(W.Path, Loaded, Why));
	TestTrue(TEXT("Point file has no anchor until device save succeeds"), Loaded.AnchorUuid.IsEmpty());
	FM3bTestAccess::Points(W.A, S);
	TestTrue(TEXT("Hand point save succeeds"), W.A->SavePoints(Why));
	TestEqual(TEXT("Hand save also requests one anchor"), W.Requests, 2);
	W.A->AdvanceSwitch(W.A->FadeSeconds); W.A->AdvanceSwitch(W.A->FadeSeconds);
	TestTrue(TEXT("Anchor operation does not block alignment save"), W.A->State == EVBoRAlignmentState::Applied);
	S.Points[0].StageCm.X += 100;
	TestFalse(TEXT("Rejected fit not saved"), W.A->SaveMarkerPoints(FString(), S.Points, Why));
	TestEqual(TEXT("Rejected save never requests anchor"), W.Requests, 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FM3bFound, "vBoR.M3b.E3Found", M3bFlags)
bool FM3bFound::RunTest(const FString&)
{
	FM3bWorld W; auto S = W.Value(); W.A->Area += FVector2D(100,100); W.Start(S, true);
	TestEqual(TEXT("One UUID discovery request"), W.Requests, 1);
	TestTrue(TEXT("NeedsCheck while finding"), W.A->State == EVBoRAlignmentState::NeedsCheck);
	const FTransform Delta(FRotator(0,15,0), FVector(30,-20,0));
	FM3bTestAccess::Found(W.A, S.AnchorStage * Delta);
	TestTrue(TEXT("Bounds difference does not block anchor"), W.A->State == EVBoRAlignmentState::Applied);
	TestTrue(TEXT("Anchor visible in status"), W.A->Reason.Contains(TEXT("anchor")));
	// Pawn maps moved Stage points back to fixed CAD (inverse Stage delta, M3b D3)
	TestTrue(TEXT("Pawn compensates Stage translation and Yaw"), W.Pawn->GetActorTransform().Equals(Delta.Inverse(), 0.001));
	W.Runner->ProcessFlow(0);
	TestEqual(TEXT("Scenario resumes after anchor"), W.Runner->GetOperatorStatus().Mode, FString(TEXT("GUIDED SCENARIO")));
	TestTrue(TEXT("A accepted"), W.A->RequestReapply()); W.A->AdvanceSwitch(W.A->FadeSeconds); W.A->AdvanceSwitch(W.A->FadeSeconds);
	TestEqual(TEXT("A reuses found pose"), W.Requests, 1);
	TestTrue(TEXT("A keeps compensated Pawn"), W.Pawn->GetActorTransform().Equals(Delta.Inverse(), 0.001));
	W.A->Origin = EHMDTrackingOrigin::Local; TestTrue(TEXT("A accepted for environment check"), W.A->RequestReapply()); W.A->AdvanceSwitch(W.A->FadeSeconds);
	TestTrue(TEXT("Non-Stage still blocked"), W.A->State == EVBoRAlignmentState::NeedsCheck);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FM3bTimeout, "vBoR.M3b.E4Timeout", M3bFlags)
bool FM3bTimeout::RunTest(const FString&)
{
	for (bool bChangedBounds : {false, true})
	{
		FM3bWorld W; auto S = W.Value(); if (bChangedBounds) { W.A->Area += FVector2D(100,100); }
		W.Pawn->SetActorLocation(FVector(500,600,0)); W.Start(S, true);
		TestTrue(TEXT("Finding anchor reason"), W.A->Reason == TEXT("Finding anchor"));
		TestTrue(TEXT("Pawn held during discovery"), W.Pawn->GetActorLocation().Equals(FVector(500,600,0)));
		W.Runner->ProcessFlow(0); TestEqual(TEXT("No automatic start while finding"), W.Runner->GetOperatorStatus().Mode, FString(TEXT("GETTING READY")));
		FM3bTestAccess::Timeout(W.A);
		TestTrue(TEXT("Legacy Pawn after timeout"), W.Pawn->GetActorTransform().Equals(FTransform::Identity, 0.001));
		TestTrue(TEXT("Legacy environment decision"), W.A->State == (bChangedBounds ? EVBoRAlignmentState::NeedsCheck : EVBoRAlignmentState::Applied));
		FM3bTestAccess::Found(W.A, FTransform(FVector(900,900,0)));
		TestTrue(TEXT("Late result ignored"), W.Pawn->GetActorTransform().Equals(FTransform::Identity, 0.001));
	}
	return true;
}
#endif
