#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include <limits>
#include "../Interaction/VBoRBuckAlignment.h"
#include "../Interaction/VBoREyePointComponent.h"
#include "P0BuckBench.h"
#include "Components/InputComponent.h"
#include "Components/ArrowComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/WorldSettings.h"
#include "Misc/CoreDelegates.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#endif

struct FP0BuckTestAccess
{
	static void Path(AP0BuckBench* Bench, const FString& Path) { Bench->SavePath = Path; }
	static void Load(AP0BuckBench* Bench, const FVBoRBuckHeadFrame& Frame) { Bench->LoadAndApply(Frame); }
	static void Frame(AP0BuckBench* Bench, double Dt, const FVBoRBuckHeadFrame& Head) { Bench->ProcessFrame(Dt, Head); }
	static void Save(AP0BuckBench* Bench) { Bench->StartSave(); }
	static void Release(AP0BuckBench* Bench) { Bench->ReleaseSave(); }
	static void Measure(AP0BuckBench* Bench) { Bench->StartMeasure(); }
	static void Tips(AP0BuckBench* Bench) { Bench->CheckTips(); }
	static const FVBoRBuckSavedAlignment& Saved(AP0BuckBench* Bench) { return Bench->Saved; }
	static bool HasSaved(AP0BuckBench* Bench) { return Bench->bHasSaved; }
	static bool NeedsCheck(AP0BuckBench* Bench) { return Bench->bNeedsCheck; }
	static const FString& State(AP0BuckBench* Bench) { return Bench->State; }
	static const FString& TipsText(AP0BuckBench* Bench) { return Bench->LastTips; }
	static const FString& Event(AP0BuckBench* Bench) { return Bench->LastEvent; }
	static const FVBoRBuckCaptureWindow& Window(AP0BuckBench* Bench) { return Bench->Window; }
	static int32 RecenterCount(AP0BuckBench* Bench) { return Bench->RecenterCount; }
};

namespace
{
	FVBoRBuckHeadFrame Head(double Yaw = 30)
	{
		FVBoRBuckHeadFrame Frame;
		Frame.bTracked = true;
		Frame.PositionCm = FVector(10, 20, 130);
		Frame.Orientation = FRotator(20, Yaw, 10).Quaternion();
		Frame.TrackingOrigin = 3;
		Frame.PlayAreaCm = FVector2D(300, 400);
		return Frame;
	}
	FVBoRBuckSavedAlignment Fixture()
	{
		FVBoRBuckSavedAlignment Saved;
		Saved.SavedAt = TEXT("2026-10-11T00:00:00Z");
		Saved.TrackingOrigin = 3;
		Saved.PlayAreaCm = FVector2D(300, 400);
		Saved.Capture.PositionCm = FVector(10, 20, 130);
		Saved.Capture.YawDeg = 30;
		Saved.Capture.UsedFrames = 30;
		Saved.Capture.PositionSpreadMm = 2;
		Saved.Capture.YawSpreadDeg = 0.5;
		return Saved;
	}
	struct FBenchWorld
	{
		UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
		APawn* Pawn = nullptr;
		AP0BuckBench* Bench = nullptr;
		APlayerController* Controller = nullptr;
		FString Path = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation/P0Buck"), FGuid::NewGuid().ToString() + TEXT(".json"));
		FBenchWorld()
		{
			GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
			Controller = World->SpawnActor<APlayerController>();
			Pawn = World->SpawnActor<APawn>();
			Pawn->SetRootComponent(NewObject<USceneComponent>(Pawn));
			Pawn->GetRootComponent()->RegisterComponent();
			Pawn->SetActorLocation(FVector(-100, 75, 0));
			Controller->Possess(Pawn);
			Bench = World->SpawnActor<AP0BuckBench>();
			FP0BuckTestAccess::Path(Bench, Path);
			World->InitializeActorsForPlay(FURL());
			World->GetWorldSettings()->NotifyBeginPlay();
			FP0BuckTestAccess::Frame(Bench, 0, Head());
		}
		~FBenchWorld()
		{
			World->EndPlay(EEndPlayReason::Quit);
			GEngine->DestroyWorldContext(World);
			World->DestroyWorld(false);
			IFileManager::Get().Delete(*Path);
			IFileManager::Get().Delete(*(Path + TEXT(".candidate")));
		}
		void Capture()
		{
			FP0BuckTestAccess::Save(Bench);
			for (int32 Index = 0; Index < 4; ++Index) { FP0BuckTestAccess::Frame(Bench, 0.5, Head()); }
			FP0BuckTestAccess::Release(Bench);
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP0BuckVerticalTest, "vBoR.P0Buck.E3VerticalForward", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP0BuckVerticalTest::RunTest(const FString&)
{
	double Yaw = 123;
	TestFalse(TEXT("Vertical orientation rejected"), FVBoRBuckAlignment::ProjectYaw(FRotator(90, 40, 0).Quaternion(), 0.1, Yaw));
	TestTrue(TEXT("Tilted forward accepted"), FVBoRBuckAlignment::ProjectYaw(FRotator(20, 40, 10).Quaternion(), 0.1, Yaw));
	TestTrue(TEXT("Projected yaw ignores roll and pitch"), FMath::Abs(FMath::FindDeltaAngleDegrees(Yaw, 40.0)) < 0.01);
	FVBoRBuckCaptureWindow Window;
	Window.Begin(1, true);
	FVBoRBuckHeadFrame Frame = Head(); Frame.Orientation = FRotator(90, 0, 0).Quaternion();
	Window.Sample(1, Frame, 0.1);
	TestFalse(TEXT("Near vertical aborts entire window"), Window.bActive);
	TestTrue(TEXT("Yaw rejection reason present"), !Window.Reason.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP0BuckCircularTest, "vBoR.P0Buck.E4CircularMeanAndSpread", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP0BuckCircularTest::RunTest(const FString&)
{
	FVBoRBuckCaptureWindow Window;
	TestTrue(TEXT("Time window starts"), Window.Begin(1, true));
	Window.Sample(0.5, Head(179), 0.1);
	FVBoRBuckHeadFrame Second = Head(-179); Second.PositionCm.X += 0.4;
	TestTrue(TEXT("Completes after one second"), Window.Sample(0.5, Second, 0.1));
	FVBoRBuckCaptureResult Capture;
	TestTrue(TEXT("Completed window has result"), Window.Finish(Capture));
	TestTrue(TEXT("Mean around 180, never zero"), FMath::Abs(FMath::Abs(Capture.YawDeg) - 180) < 0.01);
	TestTrue(TEXT("Position mean is hand-derived midpoint"), Capture.PositionCm.Equals(FVector(10.2, 20, 130), 0.00001));
	TestTrue(TEXT("Position spread diameter in mm"), FMath::IsNearlyEqual(Capture.PositionSpreadMm, 4.0, 0.001));
	TestTrue(TEXT("Circular yaw spread in deg"), FMath::IsNearlyEqual(Capture.YawSpreadDeg, 2.0, 0.001));
	TestEqual(TEXT("Used count"), Capture.UsedFrames, 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP0BuckTransformTest, "vBoR.P0Buck.E5E6YawOnlyTransform", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP0BuckTransformTest::RunTest(const FString&)
{
	FVBoRBuckCaptureResult Capture = Fixture().Capture;
	for (const FTransform Eye : { FTransform(FRotator(0, 0, 0), FVector(0, 0, 120)), FTransform(FRotator(12, 90, 8), FVector(700, -200, 120)) })
	{
		const FTransform Pawn = FVBoRBuckAlignment::CalculatePawn(Capture, Eye);
		TestTrue(TEXT("Position matches eye <0.1mm"), FVector::Distance(Pawn.TransformPosition(Capture.PositionCm), Eye.GetLocation()) < 0.01);
		TestTrue(TEXT("Yaw matches eye <0.01deg"), FMath::Abs(FMath::FindDeltaAngleDegrees(Pawn.Rotator().Yaw + Capture.YawDeg, double(Eye.Rotator().Yaw))) < 0.01);
		TestTrue(TEXT("World floor never tilts"), FMath::Abs(Pawn.Rotator().Pitch) < 0.0001 && FMath::Abs(Pawn.Rotator().Roll) < 0.0001);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP0BuckEnvironmentTest, "vBoR.P0Buck.E9E10Environment", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP0BuckEnvironmentTest::RunTest(const FString&)
{
	const auto Saved = Fixture();
	const auto ChangedOrigin = FVBoRBuckAlignment::CheckEnvironment(Saved, 0, Saved.PlayAreaCm, 1);
	TestFalse(TEXT("Wrong origin blocks apply"), ChangedOrigin.bApply);
	TestTrue(TEXT("Wrong origin requires check"), ChangedOrigin.bNeedsCheck);
	const auto Matching = FVBoRBuckAlignment::CheckEnvironment(Saved, 3, FVector2D(301, 399), 1);
	TestTrue(TEXT("Within tolerance applies"), Matching.bApply && !Matching.bNeedsCheck);
	for (const FVector2D Bounds : { FVector2D(302, 400), FVector2D(0, 0) })
	{
		const auto ChangedBounds = FVBoRBuckAlignment::CheckEnvironment(Saved, 3, Bounds, 1);
		TestTrue(TEXT("Changed or unknown bounds apply and warn"), ChangedBounds.bApply && ChangedBounds.bNeedsCheck && !ChangedBounds.Reason.IsEmpty());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP0BuckStoreTest, "vBoR.P0Buck.E8Persistence", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP0BuckStoreTest::RunTest(const FString&)
{
	FBenchWorld Env;
	FVBoRBuckSavedAlignment Loaded;
	FString Reason;
	TestFalse(TEXT("Missing file rejected"), FVBoRBuckAlignmentStore::Load(Env.Path, Loaded, Reason));
	TestTrue(TEXT("Save succeeds"), FVBoRBuckAlignmentStore::Save(Env.Path, Fixture(), Reason));
	TestTrue(TEXT("Roundtrip succeeds"), FVBoRBuckAlignmentStore::Load(Env.Path, Loaded, Reason));
	TestTrue(TEXT("Stage cm roundtrip"), Loaded.Capture.PositionCm.Equals(FVector(10, 20, 130)));
	TestEqual(TEXT("Origin roundtrip"), Loaded.TrackingOrigin, 3);
	TestEqual(TEXT("Timestamp roundtrip"), Loaded.SavedAt, Fixture().SavedAt);
	FString Json; FFileHelper::LoadFileToString(Json, *Env.Path);
	Json.ReplaceInline(TEXT("\"version\": 1"), TEXT("\"version\": 99"));
	Json.ReplaceInline(TEXT("\"version\":1"), TEXT("\"version\":99"));
	FFileHelper::SaveStringToFile(Json, *Env.Path);
	TestFalse(TEXT("Unknown version rejected"), FVBoRBuckAlignmentStore::Load(Env.Path, Loaded, Reason));
	FP0BuckTestAccess::Load(Env.Bench, Head());
	TestFalse(TEXT("Invalid file yields no saved alignment"), FP0BuckTestAccess::HasSaved(Env.Bench));
	TestTrue(TEXT("Invalid file preserves PlayerStart"), Env.Pawn->GetActorLocation().Equals(FVector(-100, 75, 0)));
	FFileHelper::SaveStringToFile(TEXT("{broken"), *Env.Path);
	TestFalse(TEXT("Corrupt JSON rejected"), FVBoRBuckAlignmentStore::Load(Env.Path, Loaded, Reason));
	// 파일 교체의 간헐적 I/O 실패를 같은 실제 경로로 반복 확인함 (P0-c E8)
	for (int32 Attempt = 0; Attempt < 40; ++Attempt)
	{
		auto Replacement = Fixture(); Replacement.Capture.PositionCm.X = 10 + Attempt;
		const bool bReplaced = FVBoRBuckAlignmentStore::Save(Env.Path, Replacement, Reason);
		if (!TestTrue(FString::Printf(TEXT("Repeated replacement %d: %s"), Attempt, *Reason), bReplaced)) { break; }
		TestTrue(TEXT("Repeated candidate loads"), FVBoRBuckAlignmentStore::Load(Env.Path, Loaded, Reason));
		TestTrue(TEXT("Latest candidate persisted"), FMath::IsNearlyEqual(Loaded.Capture.PositionCm.X, 10.0 + Attempt));
	}
	const bool bFixtureSaved = FVBoRBuckAlignmentStore::Save(Env.Path, Fixture(), Reason);
	TestTrue(FString::Printf(TEXT("Replacement fixture saved: %s"), *Reason), bFixtureSaved);
	auto Invalid = Fixture(); Invalid.Capture.YawDeg = std::numeric_limits<double>::infinity();
	TestFalse(TEXT("Invalid candidate cannot overwrite previous"), FVBoRBuckAlignmentStore::Save(Env.Path, Invalid, Reason));
	TestTrue(TEXT("Previous survives rejected candidate"), FVBoRBuckAlignmentStore::Load(Env.Path, Loaded, Reason));
	TestTrue(TEXT("Previous yaw unchanged"), FMath::IsNearlyEqual(Loaded.Capture.YawDeg, 30.0));
#if PLATFORM_WINDOWS
	const FString FullPath = FPaths::ConvertRelativePathToFull(Env.Path);
	HANDLE Lock = ::CreateFileW(*FullPath, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (TestTrue(TEXT("Existing save can be held open without delete sharing"), Lock != INVALID_HANDLE_VALUE))
	{
		auto Replacement = Fixture(); Replacement.Capture.PositionCm.X = 99;
		TestFalse(TEXT("Locked destination rejects replacement"), FVBoRBuckAlignmentStore::Save(Env.Path, Replacement, Reason));
		::CloseHandle(Lock);
		TestTrue(TEXT("Previous save survives actual replacement failure"), FVBoRBuckAlignmentStore::Load(Env.Path, Loaded, Reason));
		TestTrue(TEXT("Previous position retained after I/O failure"), Loaded.Capture.PositionCm.Equals(FVector(10, 20, 130)));
	}
#endif
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP0BuckAbortTest, "vBoR.P0Buck.E1E2CaptureAbort", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP0BuckAbortTest::RunTest(const FString&)
{
	FBenchWorld Env; Env.Capture();
	TestTrue(TEXT("Baseline saved"), FP0BuckTestAccess::HasSaved(Env.Bench));
	const FTransform Original = Env.Pawn->GetActorTransform();
	const FString Timestamp = FP0BuckTestAccess::Saved(Env.Bench).SavedAt;
	FP0BuckTestAccess::Save(Env.Bench);
	FP0BuckTestAccess::Frame(Env.Bench, 0.5, Head());
	FVBoRBuckHeadFrame Lost = Head(); Lost.bTracked = false; Lost.PositionCm = FVector(99999);
	FP0BuckTestAccess::Frame(Env.Bench, 0.5, Lost);
	TestFalse(TEXT("TRACKED loss aborts even with valid stale pose"), FP0BuckTestAccess::Window(Env.Bench).bActive);
	TestEqual(TEXT("Discarded frame recorded"), FP0BuckTestAccess::Window(Env.Bench).DiscardedFrames, 1);
	TestTrue(TEXT("Abort reason recorded"), FP0BuckTestAccess::Event(Env.Bench).Contains(TEXT("TRACKED")));
	FP0BuckTestAccess::Frame(Env.Bench, 2, Head(120));
	TestEqual(TEXT("Tracking recovery never saves"), FP0BuckTestAccess::Saved(Env.Bench).SavedAt, Timestamp);
	FP0BuckTestAccess::Release(Env.Bench);
	FP0BuckTestAccess::Save(Env.Bench);
	FP0BuckTestAccess::Frame(Env.Bench, 1, Head(100));
	FP0BuckTestAccess::Release(Env.Bench);
	TestFalse(TEXT("Early release aborts"), FP0BuckTestAccess::Window(Env.Bench).bActive);
	TestEqual(TEXT("Early release preserves save"), FP0BuckTestAccess::Saved(Env.Bench).SavedAt, Timestamp);
	TestTrue(TEXT("All abort paths preserve pawn"), Env.Pawn->GetActorTransform().Equals(Original));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP0BuckRepeatTest, "vBoR.P0Buck.E7NoAccumulationAndTime", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP0BuckRepeatTest::RunTest(const FString&)
{
	FBenchWorld Env;
	const FTransform FixedBench = Env.Bench->GetActorTransform();
	const FTransform FixedEye = Env.Bench->EyePoint->GetComponentTransform();
	TestTrue(TEXT("Default eye 120 cm above ground"), FixedEye.GetLocation().Equals(FVector(0, 0, 120)));
	TArray<UArrowComponent*> Arrows; Env.Bench->GetComponents(Arrows);
	TestTrue(TEXT("Eye forward arrow registered and visible"), Arrows.Num() == 1 && Arrows[0]->IsRegistered() && !Arrows[0]->bHiddenInGame);
	Env.Capture(); TestTrue(TEXT("First capture saved before repeat"), FP0BuckTestAccess::HasSaved(Env.Bench)); const FTransform First = Env.Pawn->GetActorTransform(); Env.Capture();
	TestTrue(TEXT("Calibration keeps vehicle fixed"), Env.Bench->GetActorTransform().Equals(FixedBench));
	TestTrue(TEXT("Calibration keeps eye fixed"), Env.Bench->EyePoint->GetComponentTransform().Equals(FixedEye));
	TestTrue(TEXT("Same Stage pose saves without accumulation"), Env.Pawn->GetActorTransform().Equals(First, 0.000001));
	TestTrue(TEXT("Actually moved to eye"), FVector::Distance(First.TransformPosition(Head().PositionCm), FVector(0, 0, 120)) < 0.01);
	for (int32 Rate : {15, 90})
	{
		FVBoRBuckCaptureWindow Window; Window.Begin(2, true);
		for (int32 Frame = 0; Frame < Rate * 2 - 1; ++Frame) { TestFalse(TEXT("No early completion"), Window.Sample(1.0 / Rate, Head(), 0.1)); }
		TestTrue(TEXT("Completion at two seconds at both rates"), Window.Sample(1.0 / Rate, Head(), 0.1));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP0BuckRecenterTest, "vBoR.P0Buck.E11StickyRecenter", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP0BuckRecenterTest::RunTest(const FString&)
{
	FBenchWorld Env; Env.Capture(); TestTrue(TEXT("Saved baseline before recenter"), FP0BuckTestAccess::HasSaved(Env.Bench)); const FTransform First = Env.Pawn->GetActorTransform();
	FCoreDelegates::VRHeadsetRecenter.Broadcast();
	TestTrue(TEXT("Real recenter delegate requires check"), FP0BuckTestAccess::NeedsCheck(Env.Bench));
	TestEqual(TEXT("Recenter counted"), FP0BuckTestAccess::RecenterCount(Env.Bench), 1);
	FP0BuckTestAccess::Frame(Env.Bench, 1, Head());
	FP0BuckTestAccess::Measure(Env.Bench); FP0BuckTestAccess::Frame(Env.Bench, 0.5, Head());
	TestTrue(TEXT("M and tracking recovery cannot clear needs check"), FP0BuckTestAccess::NeedsCheck(Env.Bench));
	TestTrue(TEXT("Recenter never moves pawn"), Env.Pawn->GetActorTransform().Equals(First));
	Env.Capture();
	TestFalse(TEXT("New successful C save clears needs check"), FP0BuckTestAccess::NeedsCheck(Env.Bench));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP0BuckEyeCountTest, "vBoR.P0Buck.E12EyeCount", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP0BuckEyeCountTest::RunTest(const FString&)
{
	FBenchWorld Env; FString Reason; FVBoRBuckAlignmentStore::Save(Env.Path, Fixture(), Reason);
	UVBoREyePointComponent* Extra = NewObject<UVBoREyePointComponent>(Env.Bench);
	Env.Bench->AddInstanceComponent(Extra); Extra->RegisterComponent();
	FP0BuckTestAccess::Load(Env.Bench, Head());
	TestTrue(TEXT("Two eyes prevent moving pawn"), Env.Pawn->GetActorLocation().Equals(FVector(-100, 75, 0)));
	TestTrue(TEXT("Two-eye error visible"), FP0BuckTestAccess::State(Env.Bench).Contains(TEXT("2")));
	Extra->DestroyComponent(); Env.Bench->EyePoint->DestroyComponent();
	FP0BuckTestAccess::Load(Env.Bench, Head());
	TestTrue(TEXT("Zero eyes prevent moving pawn"), Env.Pawn->GetActorLocation().Equals(FVector(-100, 75, 0)));
	TestTrue(TEXT("Zero-eye error visible"), FP0BuckTestAccess::State(Env.Bench).Contains(TEXT("0")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP0BuckBusyTest, "vBoR.P0Buck.E13BusyCommands", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP0BuckBusyTest::RunTest(const FString&)
{
	FBenchWorld Env; FP0BuckTestAccess::Save(Env.Bench); FP0BuckTestAccess::Frame(Env.Bench, 0.5, Head());
	FP0BuckTestAccess::Measure(Env.Bench); TestTrue(TEXT("M ignored during C"), FP0BuckTestAccess::Event(Env.Bench).Contains(TEXT("ignored")));
	FP0BuckTestAccess::Tips(Env.Bench); TestTrue(TEXT("T ignored during C"), FP0BuckTestAccess::Event(Env.Bench).Contains(TEXT("ignored")));
	FP0BuckTestAccess::Save(Env.Bench); TestTrue(TEXT("Second C ignored"), FP0BuckTestAccess::Event(Env.Bench).Contains(TEXT("ignored")));
	TestTrue(TEXT("First window not reset"), FMath::IsNearlyEqual(FP0BuckTestAccess::Window(Env.Bench).ElapsedSeconds, 0.5));
	FP0BuckTestAccess::Frame(Env.Bench, 1.5, Head()); TestTrue(TEXT("Single original save completes"), FP0BuckTestAccess::HasSaved(Env.Bench));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP0BuckTipTest, "vBoR.P0Buck.E14RawTip", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP0BuckTipTest::RunTest(const FString&)
{
	const FTransform Eye(FRotator(0, 90, 0), FVector(100, 200, 120));
	const FString Valid = FVBoRBuckAlignment::DescribeTip(true, FVector(98, 201, 123), Eye);
	TestTrue(TEXT("Eye coordinates X front Y right Z up mm"), Valid.Contains(TEXT("X=10.00 Y=20.00 Z=30.00")));
	const FString Lost = FVBoRBuckAlignment::DescribeTip(false, FVector(98, 201, 123), Eye);
	TestTrue(TEXT("Untracked tip produces no old coordinates"), Lost.Contains(TEXT("not tracked")) && !Lost.Contains(TEXT("X=")));
	FBenchWorld Env; FP0BuckTestAccess::Tips(Env.Bench);
	TestTrue(TEXT("Actual missing hand pawn reports both untracked"), FP0BuckTestAccess::TipsText(Env.Bench).Contains(TEXT("Left: not tracked")) && FP0BuckTestAccess::TipsText(Env.Bench).Contains(TEXT("Right: not tracked")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP0BuckCleanupTest, "vBoR.P0Buck.E15EndPlayCleanup", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP0BuckCleanupTest::RunTest(const FString&)
{
	FBenchWorld Env;
	TestTrue(TEXT("Bench binds real recenter delegate"), FCoreDelegates::VRHeadsetRecenter.IsBoundToObject(Env.Bench));
	TestTrue(TEXT("Bench binds four input keys"), Env.Bench->InputComponent && Env.Bench->InputComponent->KeyBindings.Num() == 4);
	Env.Bench->Destroy();
	TestFalse(TEXT("EndPlay removes recenter delegate"), FCoreDelegates::VRHeadsetRecenter.IsBoundToObject(Env.Bench));
	TestTrue(TEXT("EndPlay clears actor-owned key bindings"), !Env.Bench->InputComponent || Env.Bench->InputComponent->KeyBindings.IsEmpty());
	const int32 Count = FP0BuckTestAccess::RecenterCount(Env.Bench);
	FCoreDelegates::VRHeadsetRecenter.Broadcast();
	TestEqual(TEXT("Destroyed bench cannot receive recenter"), FP0BuckTestAccess::RecenterCount(Env.Bench), Count);
	return true;
}
#endif
