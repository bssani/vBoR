#if WITH_DEV_AUTOMATION_TESTS && PLATFORM_WINDOWS
#include "Misc/AutomationTest.h"
#include "../Interaction/VBoRBuckAlignment.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "Misc/Paths.h"
#include "Windows/WindowsHWrapper.h"

struct FR0BuckSaveTestAccess
{
	using FReplace = uint32 (*)(const FString&, const FString&);
	static FReplace Exchange(FReplace Replacement)
	{
		FReplace Previous = FVBoRBuckAlignmentStore::ReplaceForTest;
		FVBoRBuckAlignmentStore::ReplaceForTest = Replacement;
		return Previous;
	}
	static bool IsNative() { return FVBoRBuckAlignmentStore::ReplaceForTest == nullptr; }
};

namespace
{
	int32 R0Calls = 0;
	int32 R0Failures = 0;
	uint32 R0Error = 0;

	uint32 R0Replace(const FString& CandidatePath, const FString& Path)
	{
		if (++R0Calls <= R0Failures) { return R0Error; }
		return ::MoveFileExW(*CandidatePath, *Path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) ? 0 : ::GetLastError();
	}

	struct FR0ReplacementGuard
	{
		FR0BuckSaveTestAccess::FReplace Previous;
		FR0ReplacementGuard(int32 Failures, uint32 Error)
			: Previous(FR0BuckSaveTestAccess::Exchange(&R0Replace))
		{
			R0Calls = 0;
			R0Failures = Failures;
			R0Error = Error;
		}
		~FR0ReplacementGuard() { FR0BuckSaveTestAccess::Exchange(Previous); }
		FR0ReplacementGuard(const FR0ReplacementGuard&) = delete;
		FR0ReplacementGuard& operator=(const FR0ReplacementGuard&) = delete;
	};

	FVBoRBuckSavedAlignment R0Fixture(double X)
	{
		FVBoRBuckSavedAlignment Saved;
		Saved.SavedAt = TEXT("2026-11-01T00:00:00Z");
		Saved.TrackingOrigin = 3;
		Saved.PlayAreaCm = FVector2D(300, 400);
		Saved.Capture.PositionCm = FVector(X, 20, 130);
		Saved.Capture.UsedFrames = 30;
		return Saved;
	}

	struct FR0SaveFiles
	{
		FString Path = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation"),
			TEXT("R0BuckSave-") + FGuid::NewGuid().ToString() + TEXT(".json"));
		~FR0SaveFiles()
		{
			IFileManager::Get().Delete(*(Path + TEXT(".candidate")));
			IFileManager::Get().Delete(*Path);
		}
	};

	bool R0Scenario(FAutomationTestBase& Test, int32 Failures, uint32 Error, bool bExpectSuccess)
	{
		FR0SaveFiles Files;
		FString Reason;
		if (!Test.TestTrue(TEXT("Native baseline saved"), FVBoRBuckAlignmentStore::Save(Files.Path, R0Fixture(10), Reason))) { return false; }
		if (bExpectSuccess)
		{
			Test.AddExpectedMessagePlain(FString::Printf(TEXT("Buck alignment save: replacement succeeded on attempt %d after OS error %u"), Failures + 1, Error),
				ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Exact, 1);
		}
		const double Start = FPlatformTime::Seconds();
		{
			FR0ReplacementGuard Guard(Failures, Error);
			Reason = TEXT("stale reason");
			Test.TestEqual(TEXT("Save result"), FVBoRBuckAlignmentStore::Save(Files.Path, R0Fixture(99), Reason), bExpectSuccess);
		}
		const double Elapsed = FPlatformTime::Seconds() - Start;
		const int32 ExpectedCalls = bExpectSuccess ? Failures + 1 : (Error == ERROR_ACCESS_DENIED || Error == ERROR_SHARING_VIOLATION ? 5 : 1);
		Test.TestEqual(TEXT("Replacement call count"), R0Calls, ExpectedCalls);
		Test.TestTrue(TEXT("Hook restored after scope"), FR0BuckSaveTestAccess::IsNative());
		if (bExpectSuccess) { Test.TestTrue(TEXT("Success clears reason"), Reason.IsEmpty()); }
		else
		{
			Test.TestEqual(TEXT("Final error and retained-save reason"), Reason,
				FString::Printf(TEXT("candidate replacement failed (OS error %u); previous save retained"), Error));
		}
		Test.TestFalse(TEXT("Candidate removed"), IFileManager::Get().FileExists(*(Files.Path + TEXT(".candidate"))));
		FVBoRBuckSavedAlignment Loaded;
		if (Test.TestTrue(TEXT("Saved alignment loads"), FVBoRBuckAlignmentStore::Load(Files.Path, Loaded, Reason)))
		{
			Test.TestEqual(TEXT("New or previous value retained"), Loaded.Capture.PositionCm.X, bExpectSuccess ? 99.0 : 10.0);
		}
		if (Error == ERROR_ACCESS_DENIED && !bExpectSuccess)
		{
			Test.TestTrue(FString::Printf(TEXT("Retry elapsed %.3f s <= 0.7 s (0.2 s test allowance)"), Elapsed), Elapsed <= 0.7);
		}
		return true;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FR0AccessRetryTest, "vBoR.R0.E1AccessDeniedRetry", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FR0AccessRetryTest::RunTest(const FString&) { return R0Scenario(*this, 2, ERROR_ACCESS_DENIED, true); }

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FR0SharingRetryTest, "vBoR.R0.E2SharingViolationRetry", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FR0SharingRetryTest::RunTest(const FString&) { return R0Scenario(*this, 1, ERROR_SHARING_VIOLATION, true); }

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FR0ExhaustionTest, "vBoR.R0.E3RetryExhausted", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FR0ExhaustionTest::RunTest(const FString&) { return R0Scenario(*this, MAX_int32, ERROR_ACCESS_DENIED, false); }

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FR0OtherErrorTest, "vBoR.R0.E4OtherError", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FR0OtherErrorTest::RunTest(const FString&) { return R0Scenario(*this, 1, ERROR_PATH_NOT_FOUND, false); }

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FR0WaitLimitTest, "vBoR.R0.E5WaitLimit", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FR0WaitLimitTest::RunTest(const FString&) { return R0Scenario(*this, MAX_int32, ERROR_ACCESS_DENIED, false); }

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FR0RestoreTest, "vBoR.R0.E6NativeReplacementRestored", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FR0RestoreTest::RunTest(const FString&)
{
	TestTrue(TEXT("Native replacement initially active"), FR0BuckSaveTestAccess::IsNative());
	{
		FR0ReplacementGuard Guard(MAX_int32, ERROR_PATH_NOT_FOUND);
		TestFalse(TEXT("Hook installed"), FR0BuckSaveTestAccess::IsNative());
	}
	TestTrue(TEXT("Native replacement restored"), FR0BuckSaveTestAccess::IsNative());
	FR0SaveFiles Files;
	FString Reason;
	if (!TestTrue(TEXT("Native save succeeds after guard"), FVBoRBuckAlignmentStore::Save(Files.Path, R0Fixture(10), Reason))) { return false; }
	TestTrue(TEXT("Native replacement succeeds after guard"), FVBoRBuckAlignmentStore::Save(Files.Path, R0Fixture(99), Reason));
	TestEqual(TEXT("Restored saves bypass injected function"), R0Calls, 0);
	FVBoRBuckSavedAlignment Loaded;
	if (TestTrue(TEXT("Native replacement loads"), FVBoRBuckAlignmentStore::Load(Files.Path, Loaded, Reason)))
	{
		TestEqual(TEXT("Native replacement persisted new value"), Loaded.Capture.PositionCm.X, 99.0);
	}
	TestFalse(TEXT("Native candidate consumed"), IFileManager::Get().FileExists(*(Files.Path + TEXT(".candidate"))));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FR0WarningTest, "vBoR.R0.E7RecoveryWarning", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FR0WarningTest::RunTest(const FString&) { return R0Scenario(*this, 2, ERROR_ACCESS_DENIED, true); }
#endif
