#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "P4hTestFixture.h"
#include "P3VehicleMock.h"
#include "../P0Probe/P0BuckBench.h"
#include "../Interaction/VBoRAlignPointComponent.h"
#include "../Interaction/VBoREyePointComponent.h"
#include "../Interaction/VBoRViewpointComponent.h"
#include "../Interaction/VBoRScenarioRunner.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/WorldSettings.h"
#include "HeadMountedDisplayTypes.h"
#include "IXRTrackingSystem.h"
#include "HAL/IConsoleManager.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

struct FP5uTestAccess
{
	static void BenchSave(AP0BuckBench* Bench, const FString& Path, APawn* Pawn)
	{
		Bench->SavePath = Path; Bench->AlignmentPawn = Pawn;
		double Now = 100;
		Bench->CaptureClock = [&Now] { return Now; };
		FVBoRBuckHeadFrame Frame; Frame.bTracked = true; Frame.TrackingOrigin = EHMDTrackingOrigin::Stage;
		Frame.PlayAreaCm = FVector2D(300,400); Frame.PositionCm = FVector(0,0,115);
		Bench->CurrentFrame = Frame; Bench->StartSave(); Now += Bench->SaveHoldSeconds;
		Bench->ProcessFrame(Bench->SaveHoldSeconds,Frame); Bench->ReleaseSave();
		Bench->CaptureClock = [] { return 0.; };
	}
	static FString BenchResult(AP0BuckBench* Bench) { return Bench->LastCapture; }
};

namespace
{
	constexpr auto P5uFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;
	struct FP5uCVar
	{
		IConsoleVariable* Var = IConsoleManager::Get().FindConsoleVariable(TEXT("vBoR.EyeAlignment"));
		int32 Before = Var ? Var->GetInt() : 0;
		explicit FP5uCVar(int32 Value) { Set(Value); }
		void Set(int32 Value) { if (Var) { Var->Set(Value, ECVF_SetByCode); } }
		~FP5uCVar() { Set(Before); }
	};
	struct FP5uWorld
	{
		UWorld* World = UWorld::CreateWorld(EWorldType::Game,false);
		APawn* Pawn;
		AP3VehicleMock* Car;
		UP4hTestAlignment* A;
		TSharedPtr<IXRTrackingSystem,ESPMode::ThreadSafe> OriginalXR = GEngine->XRSystem;
		FString Path = FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("Automation"),FGuid::NewGuid().ToString()+TEXT("-P5u.json"));
		FTransform Before = FTransform(FRotator(0,23,0),FVector(33,-72,19));
		FP5uWorld()
		{
			GEngine->XRSystem.Reset();
			GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
			auto* PC = World->SpawnActor<APlayerController>(); Pawn = World->SpawnActor<APawn>();
			Pawn->SetRootComponent(NewObject<USceneComponent>(Pawn)); Pawn->GetRootComponent()->RegisterComponent(); PC->Possess(Pawn);
			Pawn->SetActorTransform(Before);
			Car = World->SpawnActor<AP3VehicleMock>(); Car->Alignment->DestroyComponent();
			A = NewObject<UP4hTestAlignment>(Car); Car->AddInstanceComponent(A); Car->Alignment = A;
			A->BaseEye = Car->DriverEye; A->TestPath = Path; A->RegisterComponent();
		}
		~FP5uWorld()
		{
			World->EndPlay(EEndPlayReason::Quit); GEngine->DestroyWorldContext(World); World->DestroyWorld(false);
			GEngine->XRSystem = OriginalXR;
			IFileManager::Get().Delete(*Path); IFileManager::Get().Delete(*(Path+TEXT(".candidate")));
			const FString Backup = FPaths::ChangeExtension(Path,TEXT("points-backup.json"));
			IFileManager::Get().Delete(*Backup); IFileManager::Get().DeleteDirectory(*Backup);
		}
		FVBoRBuckSavedAlignment SaveValue(int32 Version = 1)
		{
			FVBoRBuckSavedAlignment S; S.Version = Version; S.SavedAt = TEXT("2026-10-22T00:00:00Z");
			S.TrackingOrigin = A->Origin; S.PlayAreaCm = A->Area;
			S.Capture.PositionCm = FVector(12,-34,100); S.Capture.YawDeg = 17; S.Capture.UsedFrames = 15;
			if (Version == 2)
			{ for (UVBoRAlignPointComponent* P : Car->AlignPoints) { S.Points.Add({P->Order,P->GetComponentLocation(),0,15}); } }
			return S;
		}
		bool Save(const FVBoRBuckSavedAlignment& S) { FString Why; return FVBoRBuckAlignmentStore::Save(Path,S,Why); }
		void Start() { World->InitializeActorsForPlay(FURL()); World->GetWorldSettings()->NotifyBeginPlay(); }
		void Tick() { A->TickComponent(0,LEVELTICK_All,nullptr); }
		void Fade() { A->AdvanceSwitch(A->FadeSeconds); A->AdvanceSwitch(A->FadeSeconds); }
	};
	bool SamePose(const FTransform& A, const FTransform& B)
	{ return A.GetLocation().Equals(B.GetLocation(),0.01) && A.Rotator().Equals(B.Rotator(),0.01); }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5uDefault,"vBoR.P5u.E1DefaultOff",P5uFlags)
bool FP5uDefault::RunTest(const FString&)
{
	auto* Var = IConsoleManager::Get().FindConsoleVariable(TEXT("vBoR.EyeAlignment"));
	TestNotNull(TEXT("Variable registered"),Var);
	if (Var) { TestEqual(TEXT("Registered default is off"),Var->GetDefaultValue(),FString(TEXT("0"))); }
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5uOff,"vBoR.P5u.E2HeadFileBlocked",P5uFlags)
bool FP5uOff::RunTest(const FString&)
{
	FP5uCVar C(0); FP5uWorld W; TestTrue(TEXT("Head file saved"),W.Save(W.SaveValue())); W.Start();
	TestTrue(TEXT("No saved alignment"),W.A->State == EVBoRAlignmentState::NoSavedValue);
	TestTrue(TEXT("Pawn unchanged"),SamePose(W.Before,W.Pawn->GetActorTransform()));
	TestTrue(TEXT("Reason prefix retained"),W.A->Reason.StartsWith(TEXT("No saved value")));
	TestTrue(TEXT("Switch in reason"),W.A->Reason.Contains(TEXT("vBoR.EyeAlignment")));
	TestTrue(TEXT("English explains switch"),W.A->EnglishReason().Contains(TEXT("vBoR.EyeAlignment 1")));
	auto* R = W.World->SpawnActor<AVBoRScenarioRunner>(); R->Scenario = NewObject<UVBoRScenarioAsset>(R);
	R->Scenario->Steps.Reset(); FVBoRScenarioStep Step; Step.StepId = TEXT("01"); Step.ConfirmBy = EVBoRConfirmBy::Either; R->Scenario->Steps.Add(Step); R->InitializeFlow();
	TestFalse(TEXT("Start blocked"),R->IsRunning());
	TestTrue(TEXT("Start reason retained"),R->GetOperatorText().Contains(TEXT("Alignment required")));
	R->PressOperatorButton(EVBoROperatorButton::Ignore); R->ProcessFlow(0);
	TestTrue(TEXT("O starts without alignment"),R->IsRunning());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5uPointsOff,"vBoR.P5u.E3PointsWithSwitchOff",P5uFlags)
bool FP5uPointsOff::RunTest(const FString&)
{
	FP5uCVar C(0); FP5uWorld W; TestTrue(TEXT("Point file saved"),W.Save(W.SaveValue(2))); W.Start();
	TestTrue(TEXT("Points applied"),W.A->State == EVBoRAlignmentState::Applied);
	TestTrue(TEXT("Points fit with floor height"),SamePose(W.Pawn->GetActorTransform(),FTransform::Identity));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5uOn,"vBoR.P5u.E4SavedEyeHeight",P5uFlags)
bool FP5uOn::RunTest(const FString&)
{
	FP5uCVar C(1); FP5uWorld W; const auto S = W.SaveValue(); TestTrue(TEXT("Head file saved"),W.Save(S)); W.Start();
	const FTransform Expected = FVBoRBuckAlignment::CalculatePawn(S.Capture,W.Car->DriverEye->GetComponentTransform());
	TestTrue(TEXT("Eye alignment applied"),W.A->State == EVBoRAlignmentState::Applied);
	TestTrue(TEXT("Saved eye height and yaw"),SamePose(W.Pawn->GetActorTransform(),Expected));
	TestTrue(TEXT("Different from floor height"),FMath::Abs(Expected.GetLocation().Z-W.A->FloorWorldZCm)>1);
	TestTrue(TEXT("Home test described"),W.A->Describe().Contains(TEXT("eye alignment (home test)")));
	TestTrue(TEXT("Passenger accepted"),W.A->SwitchViewpoint(W.Car->PassengerView)); W.Fade();
	FTransform Passenger = Expected; Passenger.AddToTranslation(W.Car->PassengerView->GetComponentLocation()-W.Car->DriverEye->GetComponentLocation());
	TestTrue(TEXT("Passenger retains eye height"),SamePose(W.Pawn->GetActorTransform(),Passenger));
	TestTrue(TEXT("Return accepted"),W.A->ReturnToBase()); W.Fade(); TestTrue(TEXT("Return eye height"),SamePose(W.Pawn->GetActorTransform(),Expected));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5uPointsOn,"vBoR.P5u.E5PointsWithSwitchOn",P5uFlags)
bool FP5uPointsOn::RunTest(const FString&)
{
	FP5uCVar C(1); FP5uWorld W; TestTrue(TEXT("Point file saved"),W.Save(W.SaveValue(2))); W.Start();
	TestTrue(TEXT("Points applied"),W.A->State == EVBoRAlignmentState::Applied);
	TestTrue(TEXT("Points keep floor height"),SamePose(W.Pawn->GetActorTransform(),FTransform::Identity));
	TestFalse(TEXT("No eye label"),W.A->Describe().Contains(TEXT("eye alignment")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5uEnable,"vBoR.P5u.E6EnableDuringPlay",P5uFlags)
bool FP5uEnable::RunTest(const FString&)
{
	FP5uCVar C(0); FP5uWorld W; const auto S = W.SaveValue(); TestTrue(TEXT("Head file saved"),W.Save(S)); W.Start();
	TestTrue(TEXT("Point capture begins"),W.A->CapturePoint()); C.Set(1); W.Tick();
	TestFalse(TEXT("Waits for point session"),W.A->IsSwitching());
	TestTrue(TEXT("Still at initial pose"),SamePose(W.Before,W.Pawn->GetActorTransform()));
	W.A->CancelPoints(); W.Tick(); TestTrue(TEXT("Reapply fade begins"),W.A->IsSwitching());
	TestTrue(TEXT("No movement before blackout"),SamePose(W.Before,W.Pawn->GetActorTransform())); W.Fade();
	TestTrue(TEXT("Eye applied after blackout"),SamePose(W.Pawn->GetActorTransform(),FVBoRBuckAlignment::CalculatePawn(S.Capture,W.Car->DriverEye->GetComponentTransform())));
	TestEqual(TEXT("One reapply"),W.A->ReapplyCompletions,1); C.Set(1); W.Tick();
	TestFalse(TEXT("Same value does not reapply"),W.A->IsSwitching()); TestEqual(TEXT("Still one reapply"),W.A->ReapplyCompletions,1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5uDisable,"vBoR.P5u.E7DisableRestoresInitialPose",P5uFlags)
bool FP5uDisable::RunTest(const FString&)
{
	FP5uCVar C(1);
	for (int32 Seat = 0; Seat < 4; ++Seat)
	{
		C.Set(1); FP5uWorld W; TestTrue(TEXT("Head file saved"),W.Save(W.SaveValue())); W.Start();
		if (Seat > 0) { TestTrue(TEXT("Passenger accepted"),W.A->SwitchViewpoint(W.Car->PassengerView)); if (Seat != 2) { W.Fade(); } }
		if (Seat == 3) { TestTrue(TEXT("Home shift applied"),W.A->MoveHomeShift(FVector2D(100,0))); }
		const auto BeforeDisable = W.Pawn->GetActorTransform(); C.Set(0); W.Tick();
		TestTrue(TEXT("Same frame no saved alignment"),W.A->State == EVBoRAlignmentState::NoSavedValue);
		TestTrue(TEXT("Switch reason"),W.A->Reason.Contains(TEXT("vBoR.EyeAlignment")));
		TestTrue(TEXT("Restoring behind fade"),W.A->IsSwitching());
		TestTrue(TEXT("Pawn retained until blackout"),SamePose(BeforeDisable,W.Pawn->GetActorTransform())); W.Fade();
		TestTrue(TEXT("Original Pawn pose"),SamePose(W.Before,W.Pawn->GetActorTransform()));
		TestTrue(TEXT("Driver view restored"),W.A->IsBaseViewpoint()); TestTrue(TEXT("Home shift reset"),W.A->GetHomeShiftCm().IsZero());
		TestEqual(TEXT("Seat rejected after restore"),W.A->SwitchRejection(W.Car->PassengerView),FString(TEXT("Saved alignment required")));
		if (Seat == 2) { TestEqual(TEXT("Pending transition discarded"),W.A->SwitchDiscards,1); TestEqual(TEXT("No seat completion"),W.A->SwitchCompletions,0); }
		C.Set(1); W.Tick(); W.Fade(); TestTrue(TEXT("Can enable again"),W.A->State == EVBoRAlignmentState::Applied);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5uUnchanged,"vBoR.P5u.E8PointsAndMissingFileUnchanged",P5uFlags)
bool FP5uUnchanged::RunTest(const FString&)
{
	FP5uCVar C(0);
	for (bool bPoints : {false,true})
	{
		C.Set(0); FP5uWorld W; if (bPoints) { TestTrue(TEXT("Point file saved"),W.Save(W.SaveValue(2))); } W.Start();
		const auto Pose = W.Pawn->GetActorTransform(); const auto State = W.A->State;
		for (int32 Value : {1,0})
		{
			C.Set(Value); W.Tick(); TestFalse(TEXT("No fade"),W.A->IsSwitching());
			TestTrue(TEXT("State unchanged"),W.A->State == State); TestTrue(TEXT("Pose unchanged"),SamePose(Pose,W.Pawn->GetActorTransform()));
			TestEqual(TEXT("No reapply"),W.A->ReapplyCompletions,0); TestEqual(TEXT("No seat requests"),W.A->SwitchRequests,0);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5uDirect,"vBoR.P5u.E9DirectInitializationUnchanged",P5uFlags)
bool FP5uDirect::RunTest(const FString&)
{
	FP5uCVar C(0); FP5uWorld W; const auto S = W.SaveValue(); TestTrue(TEXT("File exists too"),W.Save(S)); W.Start();
	W.A->InitializeAlignment(W.Pawn,&S,W.A->Origin,W.A->Area);
	const auto Pose = W.Pawn->GetActorTransform();
	TestTrue(TEXT("Direct path keeps floor height"),SamePose(Pose,FVBoRBuckAlignment::CalculatePawnOnFloor(S.Capture,W.Car->DriverEye->GetComponentTransform(),W.Car->DriverEye->GetComponentTransform(),W.A->FloorWorldZCm)));
	for (int32 Value : {1,0}) { C.Set(Value); W.Tick(); TestTrue(TEXT("Direct state applied"),W.A->State == EVBoRAlignmentState::Applied); TestTrue(TEXT("Direct pose unchanged"),SamePose(Pose,W.Pawn->GetActorTransform())); TestFalse(TEXT("No direct reapply"),W.A->IsSwitching()); }
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5uBackup,"vBoR.P5u.E10PointSaveBackup",P5uFlags)
bool FP5uBackup::RunTest(const FString&)
{
	FP5uCVar C(0);
	for (int32 Case = 0; Case < 5; ++Case)
	{
		FP5uWorld W; auto S = W.SaveValue(Case == 1 ? 1 : 2); FString Original;
		if (Case != 2) { TestTrue(TEXT("Original saved"),W.Save(S)); FFileHelper::LoadFileToString(Original,*W.Path); }
		const FString Backup = FPaths::ChangeExtension(W.Path,TEXT("points-backup.json"));
		if (Case == 3) { TestTrue(TEXT("Backup path blocked"),IFileManager::Get().MakeDirectory(*Backup)); }
		if (Case == 4) { FFileHelper::SaveStringToFile(TEXT("older point backup"),*Backup); }
		auto* Bench = W.World->SpawnActor<AP0BuckBench>(); W.Car->DriverEye->DestroyComponent();
		FP5uTestAccess::BenchSave(Bench,W.Path,W.Pawn);
		FVBoRBuckSavedAlignment Loaded; FString Why; TestTrue(TEXT("Main file readable"),FVBoRBuckAlignmentStore::Load(W.Path,Loaded,Why));
		if (Case == 3)
		{
			TestEqual(TEXT("Failed backup retains version 2"),Loaded.Version,2); FString After; FFileHelper::LoadFileToString(After,*W.Path);
			TestEqual(TEXT("Original byte content retained"),After,Original); TestTrue(TEXT("Failure explained"),FP5uTestAccess::BenchResult(Bench).Contains(TEXT("backup failed")));
		}
		else
		{
			TestEqual(TEXT("Bench writes version 1"),Loaded.Version,1);
			if (Case == 0 || Case == 4) { FString Copy; TestTrue(TEXT("Backup readable"),FFileHelper::LoadFileToString(Copy,*Backup)); TestEqual(TEXT("Exact latest point content"),Copy,Original); }
			else { TestFalse(TEXT("No backup for head or missing file"),IFileManager::Get().FileExists(*Backup)); }
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5uLegacyCleanup,"vBoR.P5u.E11LegacyBenchBackupCleanup",P5uFlags)
bool FP5uLegacyCleanup::RunTest(const FString&)
{
	// Clean temporary backup left by protected P4h E6 only (P5u §3 audit)
	const FString Folder = FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("Automation")); TArray<FString> Files;
	IFileManager::Get().FindFiles(Files,*(Folder/TEXT("*-P4hBench.points-backup.json")),true,false);
	for (const FString& File : Files) { TestTrue(TEXT("Legacy temporary backup removed"),IFileManager::Get().Delete(*(Folder/File))); }
	return true;
}
#endif
