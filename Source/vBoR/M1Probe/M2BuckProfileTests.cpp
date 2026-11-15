#if WITH_DEV_AUTOMATION_TESTS

#include "M2TestAlignment.h"
#include "M1ProbeTestActor.h"
#include "../Interaction/VBoRAlignPointComponent.h"
#include "../Interaction/VBoREyePointComponent.h"
#include "../Interaction/VBoRViewpointComponent.h"
#include "../P3Probe/P3VehicleMock.h"
#include "../P3Probe/P5gTestFixture.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/WorldSettings.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformFileManager.h"
#include "HeadMountedDisplayTypes.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace
{
	constexpr auto M2Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;
	using Button = EVBoROperatorButton;
	const FTransform Initial(FVector(-400,50,0));
	const FTransform ExpectedB(FRotator(0,20,0), FVector(100,0,0));

	struct FM2Files
	{
		const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation"), FGuid::NewGuid().ToString() + TEXT("-M2"));
		FM2Files() { IFileManager::Get().MakeDirectory(*Directory,true); }
		~FM2Files() { IFileManager::Get().DeleteDirectory(*Directory,false,true); }
		FString ActivePath() const { return FPaths::Combine(Directory,TEXT("ActiveBuck.txt")); }
		FString LegacyPath() const { return FPaths::Combine(Directory,TEXT("BuckEye.json")); }
		FString Path(const FString& Buck) const { return FPaths::Combine(Directory,TEXT("Bucks"),Buck + TEXT(".json")); }
		FString Read(const FString& Path) const { FString Text; FFileHelper::LoadFileToString(Text,*Path); return Text; }
		FString Active() const { return Read(ActivePath()).TrimStartAndEnd(); }
		bool WriteActive(const FString& Buck) const { return FFileHelper::SaveStringToFile(Buck + TEXT("\n"),*ActivePath()); }
	};
}

struct FM2Fixture
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	const FM2Files& Files;
	APawn* Pawn;
	AP3VehicleMock* Car;
	UM2TestAlignment* Alignment;
	AP5gReloadObserver* Runner;
	AM1ProbeTestActor* Probe;
	TArray<UVBoRAlignPointComponent*> PointsA, PointsB;
	IConsoleVariable* Eye = IConsoleManager::Get().FindConsoleVariable(TEXT("vBoR.EyeAlignment"));
	int32 OriginalEye = Eye->GetInt();

	explicit FM2Fixture(const FM2Files& InFiles, bool bProfiles = true) : Files(InFiles)
	{
		Eye->Set(0, ECVF_SetByCode);
		GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
		auto* PC = World->SpawnActor<APlayerController>();
		Pawn = World->SpawnActor<APawn>();
		Pawn->SetRootComponent(NewObject<USceneComponent>(Pawn));
		Pawn->GetRootComponent()->RegisterComponent();
		PC->Possess(Pawn);
		Pawn->SetActorTransform(Initial);
		Car = World->SpawnActor<AP3VehicleMock>();
		Car->Alignment->DestroyComponent();
		Alignment = NewObject<UM2TestAlignment>(Car);
		Car->AddInstanceComponent(Alignment);
		Car->Alignment = Alignment;
		Alignment->BaseEye = Car->DriverEye;
		Alignment->Directory = Files.Directory;
		Alignment->Origin = EHMDTrackingOrigin::Stage;
		Alignment->RegisterComponent();
		for (UVBoRAlignPointComponent* Point : Car->AlignPoints)
		{
			PointsA.Add(Point);
			if (!bProfiles) { continue; }
			Point->Buck = TEXT("A");
			auto* Extra = NewObject<UVBoRAlignPointComponent>(Car);
			Car->AddInstanceComponent(Extra);
			Extra->Buck = TEXT("B");
			Extra->Order = Point->Order;
			Extra->Label = FString::Printf(TEXT("Buck B bolt %d"), Point->Order);
			Extra->SetupAttachment(Car->GetRootComponent());
			Extra->RegisterComponent();
			Extra->SetWorldLocation(Point->GetComponentLocation() + FVector(-30,20,0));
			PointsB.Add(Extra);
		}
		if (bProfiles)
		{
			Alignment->Bucks = {{TEXT("A"), 1}, {TEXT("B"), 5}};
		}
		Runner = World->SpawnActor<AP5gReloadObserver>();
		Runner->Scenario = NewObject<UVBoRScenarioAsset>(Runner);
		FVBoRScenarioStep Step;
		Step.StepId = TEXT("01");
		Step.ConfirmBy = EVBoRConfirmBy::Either;
		Runner->Scenario->Steps = {Step};
		Probe = World->SpawnActor<AM1ProbeTestActor>();
		Probe->Pawn = Pawn;
		Probe->MarkerMinFrames = 3;
		Probe->bCollectTestMessages = true;
	}

	~FM2Fixture()
	{
		World->EndPlay(EEndPlayReason::Quit);
		GEngine->DestroyWorldContext(World);
		World->DestroyWorld(false);
		Eye->Set(OriginalEye, ECVF_SetByCode);
	}

	void Start()
	{
		World->InitializeActorsForPlay(FURL());
		World->GetWorldSettings()->NotifyBeginPlay();
		Runner->Tick(0);
	}
	void Press(Button Id)
	{
		Runner->LiveFrame = MAX_uint64;
		Runner->PressOperatorButton(Id);
		Runner->ProcessFlow(0);
	}
	bool PressKey(FKey Key)
	{
		Runner->LiveFrame = MAX_uint64;
		const bool bHandled = Runner->HandleOperatorKey(Key);
		Runner->ProcessFlow(0);
		return bHandled;
	}
	void FinishSwitch()
	{
		Alignment->AdvanceSwitch(Alignment->FadeSeconds);
		Alignment->AdvanceSwitch(Alignment->FadeSeconds);
	}
	FVBoRBuckSavedAlignment Candidate(const TArray<UVBoRAlignPointComponent*>& Points, const FTransform& Pose) const
	{
		FVBoRBuckSavedAlignment Saved;
		Saved.Version = 2;
		Saved.SavedAt = TEXT("2026-11-16T00:00:00Z");
		Saved.TrackingOrigin = Alignment->Origin;
		Saved.PlayAreaCm = Alignment->Area;
		for (const auto* Point : Points)
		{
			FVBoRAlignPointSample& Sample = Saved.Points.AddDefaulted_GetRef();
			Sample.Order = Point->Order;
			Sample.StageCm = Pose.InverseTransformPosition(Point->GetComponentLocation());
			Sample.Frames = 3;
		}
		return Saved;
	}
	void BeginMarkerCapture() { Probe->StartCapture(10); }
	void RecordMarker(int32 Id, const FVector& Stage, double Yaw = -20, double ErrorPx = 0)
	{
		const FM1MarkerObservation Observation{Stage,Yaw,0,ErrorPx,32};
		Probe->RecordObservation(Id,&Observation,false);
	}
	void CaptureMarkers(double Yaw = -20, double OffsetCm = 0)
	{
		BeginMarkerCapture();
		const bool bLegacy = PointsB.IsEmpty();
		const auto Samples = Candidate(bLegacy ? PointsA : PointsB, ExpectedB).Points;
		for (int32 I = 0; I < Samples.Num(); ++I)
		{
			const int32 Id = I + (bLegacy ? 1 : 5);
			const FVector Stage = Samples[I].StageCm + FVector(I == 0 ? OffsetCm : 0,0,0);
			const FM1MarkerObservation Observation{Stage,Yaw,0,0,32};
			for (int32 Frame = 0; Frame < Probe->MarkerMinFrames; ++Frame)
			{
				Probe->RecordObservation(Id,&Observation,false);
			}
		}
		EndMarkerCapture();
	}
	void EndMarkerCapture() { Probe->FinishCapture(11); }
	bool IsCapturingMarkers() const { return Probe->bCapturing; }
	FString MarkerBuck() const { return Probe->SessionBuck; }
	FTransform MarkerPose() const { return Probe->Fit.IsSet() ? Probe->Fit->Pawn : Initial; }
	void PreviewMarkers() { Probe->ApplyFit(); }
	void SaveMarkers() { Probe->SaveFit(); }
	bool MarkerMessage(const FString& Text) const
	{
		return Probe->TestMessages.ContainsByPredicate([&Text](const FString& Message) { return Message.Contains(Text); });
	}
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FM2E1, "vBoR.M2.E1Legacy", M2Flags)
bool FM2E1::RunTest(const FString&)
{
	FM2Files Files;
	FM2Fixture W(Files,false);
	W.Start();
	TestEqual(TEXT("Legacy path"), W.Alignment->SavedAlignmentPath(), Files.LegacyPath());
	TestEqual(TEXT("Next buck disabled"), W.Runner->EvaluateOperatorButton(Button::NextBuck).Reason, FString(TEXT("One buck or none")));
	TestTrue(TEXT("No buck status"), W.Runner->GetOperatorStatus().Buck.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FM2E2, "vBoR.M2.E2Fallback", M2Flags)
bool FM2E2::RunTest(const FString&)
{
	for (bool bUnknown : {false, true})
	{
		FM2Files Files;
		if (bUnknown) { TestTrue(TEXT("Unknown selection fixture"), Files.WriteActive(TEXT("Unknown"))); }
		FM2Fixture W(Files);
		W.Start();
		TestEqual(TEXT("First buck selected"), W.Alignment->ActiveBuckName(), FString(TEXT("A")));
		TestEqual(TEXT("No saved value"), W.Alignment->State, EVBoRAlignmentState::NoSavedValue);
		TestTrue(TEXT("Buck-specific reason"), W.Alignment->Reason.StartsWith(TEXT("No saved value for buck A")));
		TestTrue(TEXT("Fallback reason"), W.Alignment->Reason.Contains(TEXT("Active buck file missing — using A")));
		TestEqual(TEXT("Fallback does not write selection"), Files.Active(), bUnknown ? FString(TEXT("Unknown")) : FString());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FM2E3, "vBoR.M2.E3Validation", M2Flags)
bool FM2E3::RunTest(const FString&)
{
	const TCHAR* Reasons[] = {TEXT("Duplicate buck name"), TEXT("Buck name is empty"), TEXT("Invalid buck name"),
		TEXT("overlaps"), TEXT("outside 0"), TEXT("order duplicated"), TEXT("unknown buck C"), TEXT("unknown buck (empty)")};
	for (int32 Case = 0; Case < UE_ARRAY_COUNT(Reasons); ++Case)
	{
		FM2Files Files;
		FM2Fixture W(Files);
		switch (Case)
		{
		case 0: W.Alignment->Bucks[1].Name = TEXT("a"); break;
		case 1: W.Alignment->Bucks[1].Name.Reset(); break;
		case 2: W.Alignment->Bucks[1].Name = TEXT("A/.."); break;
		case 3: W.Alignment->Bucks[1].FirstMarkerId = 3; break;
		case 4: W.Alignment->Bucks[1].FirstMarkerId = 48; break;
		case 5: W.PointsA[1]->Order = W.PointsA[0]->Order; break;
		case 6: W.PointsA[1]->Buck = TEXT("C"); break;
		case 7: W.PointsA[1]->Buck.Reset(); break;
		}
		W.Start();
		TestEqual(Reasons[Case], W.Alignment->State, EVBoRAlignmentState::NeedsCheck);
		TestTrue(TEXT("Setup reason"), W.Alignment->Reason.Contains(Reasons[Case]));
		TestTrue(TEXT("No pawn move"), W.Pawn->GetActorTransform().Equals(Initial));
		W.BeginMarkerCapture();
		TestFalse(TEXT("Invalid configuration rejected by M"), W.IsCapturingMarkers());
		TestTrue(TEXT("Same marker rejection"), W.MarkerMessage(W.Alignment->Reason));
	}
	FM2Files Files;
	FM2Fixture W(Files);
	W.PointsA.Pop()->DestroyComponent();
	W.Start();
	TestTrue(TEXT("Three points allowed for hand capture"), W.Alignment->CapturePointRejection().IsEmpty());
	W.BeginMarkerCapture();
	TestFalse(TEXT("Three points rejected for marker capture"), W.IsCapturingMarkers());
	TestTrue(TEXT("Marker count reason"), W.MarkerMessage(TEXT("Expected four vehicle alignment points for buck A")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FM2E4, "vBoR.M2.E4Reload", M2Flags)
bool FM2E4::RunTest(const FString&)
{
	FM2Files Files;
	{
		FM2Fixture W(Files);
		FString Failure;
		const auto SaveA = W.Candidate(W.PointsA,FTransform::Identity);
		const auto SaveB = W.Candidate(W.PointsB,ExpectedB);
		TestTrue(TEXT("Saved A"), FVBoRBuckAlignmentStore::Save(Files.Path(TEXT("A")),SaveA,Failure));
		TestTrue(TEXT("Saved B"), FVBoRBuckAlignmentStore::Save(Files.Path(TEXT("B")),SaveB,Failure));
		TArray<FVector> Stage, World;
		for (int32 I = 0; I < W.PointsA.Num(); ++I)
		{
			Stage.Add(SaveB.Points[I].StageCm);
			World.Add(W.PointsA[I]->GetComponentLocation());
		}
		FVBoRPointFit Wrong;
		TestTrue(TEXT("Other set can fit with a different pose"), FVBoRBuckAlignment::FitPoints(Stage,World,0,Wrong) &&
			!Wrong.Pawn.Equals(ExpectedB,0.01));
		W.Start();
		TestTrue(TEXT("A pawn"), W.Pawn->GetActorTransform().Equals(FTransform::Identity,0.01));
		TestTrue(TEXT("A auto started"), W.Runner->IsRunning());
		TestTrue(TEXT("B key handled"), W.PressKey(EKeys::B));
		TestEqual(TEXT("B selected"), Files.Active(), FString(TEXT("B")));
		TestEqual(TEXT("Fade precedes reload"), W.Runner->ReloadRequests, 0);
		W.Runner->ProcessFlow(1);
		W.Runner->ProcessFlow(1);
		TestEqual(TEXT("Exactly one level reload"), W.Runner->ReloadRequests, 1);
	}
	{
		FM2Fixture W(Files);
		W.Start();
		TestTrue(TEXT("B pawn"), W.Pawn->GetActorTransform().Equals(ExpectedB,0.01));
		TestTrue(TEXT("B auto started at 01"), W.Runner->IsRunning() && W.Runner->GetStepIndex() == 0);
		for (Button Seat : {Button::Passenger, Button::Rear})
		{
			W.Press(Seat);
			W.FinishSwitch();
			const auto* View = Seat == Button::Passenger ? W.Car->PassengerView.Get() : W.Car->RearView.Get();
			const FVector Offset = View->GetComponentLocation() - W.Car->DriverEye->GetComponentLocation();
			TestTrue(TEXT("Seat follows B alignment"), W.Pawn->GetActorLocation().Equals(ExpectedB.GetLocation() + Offset,0.01));
		}
		W.Press(Button::NextBuck);
		W.Runner->ProcessFlow(1);
		TestEqual(TEXT("A selected from rear seat"), Files.Active(), FString(TEXT("A")));
		TestEqual(TEXT("Exactly one level reload"), W.Runner->ReloadRequests, 1);
	}
	FM2Fixture W(Files);
	W.Start();
	TestTrue(TEXT("A pawn after wraparound"), W.Pawn->GetActorTransform().Equals(FTransform::Identity,0.01));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FM2E5, "vBoR.M2.E5Guards", M2Flags)
bool FM2E5::RunTest(const FString&)
{
	FM2Files Files;
	TestTrue(TEXT("Initial selection"), Files.WriteActive(TEXT("A")));
	FM2Fixture W(Files);
	W.Start();
	TestTrue(TEXT("Next buck accepted before flow"), W.Runner->EvaluateOperatorButton(Button::NextBuck).bAccepted);
	auto Reject = [&](const TCHAR* Reason)
	{
		W.Press(Button::NextBuck);
		W.Runner->ProcessFlow(1);
		TestTrue(Reason, W.Runner->GetOperatorStatus().LastCommand.Contains(Reason));
		TestEqual(TEXT("Selection retained"), Files.Active(), FString(TEXT("A")));
		TestEqual(TEXT("No reload"), W.Runner->ReloadRequests, 0);
		TestTrue(TEXT("Pawn retained"), W.Pawn->GetActorTransform().Equals(Initial));
	};
	const auto B = W.Alignment->Bucks.Pop();
	Reject(TEXT("One buck or none"));
	W.Alignment->Bucks.Add(B);
	W.Press(Button::CapturePoint);
	Reject(TEXT("Point alignment in progress"));
	W.Press(Button::CancelPoints);
	TestTrue(TEXT("Start reapply fade"), W.Alignment->RequestReapply());
	Reject(TEXT("View transition in progress"));
	W.FinishSwitch();
	IPlatformFile& Platform = FPlatformFileManager::Get().GetPlatformFile();
	TestTrue(TEXT("Make selection read-only"), Platform.SetReadOnly(*Files.ActivePath(),true));
	Reject(TEXT("ActiveBuck.txt write failed"));
	TestTrue(TEXT("Restore selection write access"), Platform.SetReadOnly(*Files.ActivePath(),false));
	W.Press(Button::Ignore);
	W.Press(Button::Free);
	TestTrue(TEXT("Free mode entered"), W.Runner->IsFreeMode());
	W.Press(Button::NextBuck);
	W.Runner->ProcessFlow(1);
	TestEqual(TEXT("Free mode reload accepted"), W.Runner->ReloadRequests, 1);
	TestEqual(TEXT("B selected"), Files.Active(), FString(TEXT("B")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FM2E6, "vBoR.M2.E6HandSave", M2Flags)
bool FM2E6::RunTest(const FString&)
{
	FM2Files Files;
	TestTrue(TEXT("Select B"), Files.WriteActive(TEXT("B")));
	FM2Fixture W(Files);
	FString Failure;
	const FString PathA = Files.Path(TEXT("A"));
	TestTrue(TEXT("Create A save"), FVBoRBuckAlignmentStore::Save(PathA,W.Candidate(W.PointsA,FTransform::Identity),Failure));
	const FString Before = Files.Read(PathA);
	W.Start();
	for (int32 I = 0; I < W.PointsB.Num(); ++I)
	{
		const auto* Point = W.PointsB[I];
		W.Press(Button::CapturePoint);
		TestTrue(TEXT("Label follows B order"), W.Alignment->PointStatus().Contains(Point->Label));
		TestTrue(TEXT("Only B markers shown"), W.PointsA[I]->bHiddenInGame && !Point->bHiddenInGame);
		W.Alignment->StageTip = ExpectedB.InverseTransformPosition(Point->GetComponentLocation());
		W.Alignment->Now += W.Alignment->PointCaptureSeconds;
		W.Alignment->AdvancePointCapture();
	}
	W.Press(Button::SavePoints);
	FVBoRBuckSavedAlignment Saved;
	TestTrue(TEXT("B save readable"), FVBoRBuckAlignmentStore::Load(Files.Path(TEXT("B")),Saved,Failure));
	TestEqual(TEXT("A save retained"), Files.Read(PathA), Before);
	W.FinishSwitch();
	TestTrue(TEXT("B save applied"), W.Pawn->GetActorTransform().Equals(ExpectedB,0.01));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FM2E7, "vBoR.M2.E7HomeEye", M2Flags)
bool FM2E7::RunTest(const FString&)
{
	FM2Files Files;
	FM2Fixture W(Files);
	FVBoRBuckSavedAlignment Eye;
	Eye.SavedAt = TEXT("2026-11-16T00:00:00Z");
	Eye.TrackingOrigin = W.Alignment->Origin;
	Eye.PlayAreaCm = W.Alignment->Area;
	Eye.Capture.PositionCm = FVector(15,35,111);
	Eye.Capture.UsedFrames = 30;
	FString Failure;
	TestTrue(TEXT("Save legacy eye"), FVBoRBuckAlignmentStore::Save(Files.LegacyPath(),Eye,Failure));
	TestTrue(TEXT("Save conflicting B pose"), FVBoRBuckAlignmentStore::Save(Files.Path(TEXT("B")),W.Candidate(W.PointsB,ExpectedB),Failure));
	TestTrue(TEXT("Select B"), Files.WriteActive(TEXT("B")));
	W.Eye->Set(1,ECVF_SetByCode);
	W.Start();
	const FTransform Expected = FVBoRBuckAlignment::CalculatePawn(Eye.Capture,W.Car->DriverEye->GetComponentTransform());
	TestTrue(TEXT("Legacy eye save applied"), W.Pawn->GetActorTransform().Equals(Expected,0.01));
	TestEqual(TEXT("Active buck retained"), W.Alignment->ActiveBuckName(), FString(TEXT("B")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FM2E8, "vBoR.M2.E8MarkerSelection", M2Flags)
bool FM2E8::RunTest(const FString&)
{
	FM2Files Files;
	FM2Fixture W(Files);
	W.Start();
	W.BeginMarkerCapture();
	W.RecordMarker(2,FVector::ZeroVector,0,2);
	TestTrue(TEXT("Rejected observation cannot select A"), W.MarkerBuck().IsEmpty());
	const auto Samples = W.Candidate(W.PointsB,ExpectedB).Points;
	for (int32 I = 0; I < Samples.Num(); ++I)
	{
		for (int32 Frame = 0; Frame < W.Probe->MarkerMinFrames; ++Frame)
		{
			W.RecordMarker(I + 5, Samples[I].StageCm);
		}
	}
	W.RecordMarker(2,FVector::ZeroVector);
	W.RecordMarker(2,FVector::ZeroVector);
	W.EndMarkerCapture();
	TestEqual(TEXT("B selected from marker ID"), W.MarkerBuck(), FString(TEXT("B")));
	TestTrue(TEXT("Other buck counted"), W.MarkerMessage(TEXT("Ignored markers from buck A: ID 2 ×2")));
	TestTrue(TEXT("Fit uses B coordinates"), W.MarkerPose().Equals(ExpectedB,0.01));
	W.PreviewMarkers();
	for (int32 I = 0; I < W.PointsB.Num(); ++I)
	{
		TestTrue(TEXT("G shows only session buck"), W.PointsA[I]->bHiddenInGame && !W.PointsB[I]->bHiddenInGame);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FM2E9, "vBoR.M2.E9MarkerSave", M2Flags)
bool FM2E9::RunTest(const FString&)
{
	FM2Files Files;
	FM2Fixture W(Files);
	W.Start();
	const FString Path = Files.Path(TEXT("B"));
	W.SaveMarkers();
	TestTrue(TEXT("No result refused"), W.MarkerMessage(TEXT("No marker fit to save")));
	TestFalse(TEXT("No save before fit"), IFileManager::Get().FileExists(*Path));
	W.CaptureMarkers();
	W.SaveMarkers();
	FString Failure;
	FVBoRBuckSavedAlignment Saved;
	TestTrue(TEXT("Valid markers save B file"), FVBoRBuckAlignmentStore::Load(Path,Saved,Failure));
	TestEqual(TEXT("B active"), Files.Active(), FString(TEXT("B")));
	TestTrue(TEXT("Success message"), W.MarkerMessage(TEXT("Saved for buck B — open L_Main")));
	const auto Expected = W.Candidate(W.PointsB,ExpectedB).Points;
	TestEqual(TEXT("Four saved Stage centers"), Saved.Points.Num(), Expected.Num());
	for (int32 I = 0; I < Saved.Points.Num() && I < Expected.Num(); ++I)
	{
		TestEqual(TEXT("Mapped point order"), Saved.Points[I].Order, Expected[I].Order);
		TestTrue(TEXT("Saved median Stage center"), Saved.Points[I].StageCm.Equals(Expected[I].StageCm,0.0001));
		TestEqual(TEXT("Frame count"), Saved.Points[I].Frames, W.Probe->MarkerMinFrames);
	}
	const FString Before = Files.Read(Path);
	W.CaptureMarkers(160);
	W.SaveMarkers();
	TestTrue(TEXT("Arrow rejection logged"), W.MarkerMessage(TEXT("off car front")));
	TestEqual(TEXT("Arrow failure keeps save"), Files.Read(Path), Before);
	W.CaptureMarkers(-20,14);
	W.SaveMarkers();
	TestTrue(TEXT("Fit-limit rejection logged"), W.MarkerMessage(TEXT("Marker save rejected — Fit")));
	TestEqual(TEXT("Fit failure keeps save"), Files.Read(Path), Before);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FM2E10, "vBoR.M2.E10MarkerRoundTrip", M2Flags)
bool FM2E10::RunTest(const FString&)
{
	FM2Files Files;
	FM2Fixture W(Files);
	W.Start();
	W.CaptureMarkers();
	const FTransform Fit = W.MarkerPose();
	W.SaveMarkers();
	TestTrue(TEXT("Load through normal reapply"), W.Alignment->RequestReapply());
	W.FinishSwitch();
	TestEqual(TEXT("Marker file applied"), W.Alignment->State, EVBoRAlignmentState::Applied);
	TestTrue(TEXT("Applied position matches M1 fit"), W.Pawn->GetActorLocation().Equals(Fit.GetLocation(),0.01));
	TestTrue(TEXT("Applied yaw matches M1 fit"), FMath::Abs(FMath::FindDeltaAngleDegrees(W.Pawn->GetActorRotation().Yaw,Fit.Rotator().Yaw)) <= 0.01);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FM2E11, "vBoR.M2.E11OperatorStatus", M2Flags)
bool FM2E11::RunTest(const FString&)
{
	FM2Files Files;
	FM2Fixture W(Files);
	FString Failure;
	TestTrue(TEXT("B fixture"), FVBoRBuckAlignmentStore::Save(Files.Path(TEXT("B")),W.Candidate(W.PointsB,ExpectedB),Failure));
	TestTrue(TEXT("Select B"), Files.WriteActive(TEXT("B")));
	W.Start();
	const auto Snapshot = W.Runner->GetOperatorSnapshot();
	TestEqual(TEXT("B profile line"), Snapshot.Status.Buck, FString(TEXT("Buck: B (2/2)")));
	TestEqual(TEXT("Alignment wording"), Snapshot.Status.Alignment, FString(TEXT("Applied")));
	TestEqual(TEXT("Next buck warns about reset"), Snapshot.Buttons[static_cast<int32>(Button::NextBuck)].Style, EVBoROperatorStyle::Danger);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FM2E12, "vBoR.M2.E12LegacyPointTags", M2Flags)
bool FM2E12::RunTest(const FString&)
{
	FM2Files Files;
	FM2Fixture W(Files,false);
	W.PointsA[0]->Buck = TEXT("Unused");
	W.PointsA[1]->Buck = TEXT("Also unused");
	FString Failure;
	TestTrue(TEXT("Legacy point save"), FVBoRBuckAlignmentStore::Save(Files.LegacyPath(),W.Candidate(W.PointsA,ExpectedB),Failure));
	W.Start();
	TestTrue(TEXT("Legacy fit ignores point buck names"), W.Pawn->GetActorTransform().Equals(ExpectedB,0.01));
	W.BeginMarkerCapture();
	TestTrue(TEXT("Legacy M ignores point buck names"), W.IsCapturingMarkers());
	TestTrue(TEXT("Legacy M has no session buck"), W.MarkerBuck().IsEmpty());
	W.CaptureMarkers();
	W.SaveMarkers();
	FVBoRBuckSavedAlignment Saved;
	TestTrue(TEXT("Legacy marker fit saved to BuckEye.json"), FVBoRBuckAlignmentStore::Load(Files.LegacyPath(),Saved,Failure));
	TestTrue(TEXT("Legacy marker save does not select a buck"), Files.Active().IsEmpty());
	return true;
}

#endif
