#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "R5TestActors.h"
#include "../Interaction/VBoRScenarioRunner.h"
#include "../Interaction/VBoRKeyTable.h"
#include "../Interaction/VBoRPoweredPanel.h"
#include "../Interaction/VBoRHingedPanel.h"
#include "../Interaction/VBoRAxisPart.h"
#include "../Interaction/VBoRFoldingMirror.h"
#include "../Interaction/VBoRFloorRoute.h"
#include "../Interaction/VBoRVehicleAlignmentComponent.h"
#include "../Interaction/VBoRVehiclePowerComponent.h"
#include "Components/BoxComponent.h"
#include "Components/InputComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/WorldSettings.h"
#include "Misc/OutputDevice.h"
#include "Misc/OutputDeviceRedirector.h"

struct FR5TestAccess
{
	static int32 Targets(const AVBoRScenarioRunner* R) { return R->Targets.Num(); }
	static auto Setup(const AVBoRHingedPanel* P) { return P->EvaluateSetup(); }
};

namespace
{
struct FR5Log : FOutputDevice
{
	struct FLine { FString Text; ELogVerbosity::Type Verbosity; };
	TArray<FLine> Lines;
	FR5Log() { GLog->FlushThreadedLogs(); GLog->AddOutputDevice(this); }
	~FR5Log() { GLog->RemoveOutputDevice(this); }
	virtual void Serialize(const TCHAR* Text, ELogVerbosity::Type V, const FName&) override { Lines.Add({Text, V}); }
	int32 Count(const FString& Text, ELogVerbosity::Type V) const
	{
		return Lines.FilterByPredicate([&](const FLine& L) { return L.Verbosity == V && L.Text.Contains(Text); }).Num();
	}
};

struct FR5World
{
	UWorld* World;
	AVBoRScenarioRunner* Runner;
	AActor* Car;
	UVBoRVehicleAlignmentComponent* Alignment;
	UVBoRVehiclePowerComponent* Power;
	AVBoRPokeButton* Start;
	explicit FR5World(EWorldType::Type Type = EWorldType::EditorPreview)
	{
		World = UWorld::CreateWorld(Type, false); GEngine->CreateNewWorldContext(Type).SetCurrentWorld(World);
		Car = World->SpawnActor<AActor>();
		Alignment = Add<UVBoRVehicleAlignmentComponent>(); Power = Add<UVBoRVehiclePowerComponent>();
		Start = World->SpawnActor<AVBoRPokeButton>(); Start->Tags.Add(TEXT("Car.StartButton"));
		Runner = World->SpawnActor<AVBoRScenarioRunner>(); Runner->Scenario = NewObject<UVBoRScenarioAsset>(Runner); Runner->Scenario->Steps.Reset();
		Step();
	}
	template<class T> T* Add()
	{
		auto* C = NewObject<T>(Car); Car->AddInstanceComponent(C); C->RegisterComponent(); return C;
	}
	void Step(EVBoRStepRule Rule = EVBoRStepRule::Confirm, FName Tag = NAME_None)
	{
		FVBoRScenarioStep S; S.StepId = TEXT("01"); S.Rule = Rule; S.TargetTag = Tag;
		Runner->Scenario->Steps = {S};
	}
	AVBoRPoweredPanel* Gate()
	{
		auto* P = World->SpawnActor<AVBoRPoweredPanel>();
		P->OpenButtonClass = AR5ReachButton::StaticClass(); P->CloseButtonClass = AR5ReachButton::StaticClass(); return P;
	}
	void Play() { World->InitializeActorsForPlay(FURL()); World->GetWorldSettings()->NotifyBeginPlay(); }
	~FR5World() { World->EndPlay(EEndPlayReason::Quit); World->DestroyWorld(false); GEngine->DestroyWorldContext(World); }
};

int32 R5Actors(UWorld* World) { int32 N = 0; for (TActorIterator<AActor> It(World); It; ++It) { ++N; } return N; }
TArray<FString> R5Lines(const FString& Report) { TArray<FString> Lines; Report.ParseIntoArrayLines(Lines, true); return Lines; }
}

#define R5_TEST(N, Name) IMPLEMENT_SIMPLE_AUTOMATION_TEST(FR5E##N, "vBoR.R5.E" #N Name, EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter) bool FR5E##N::RunTest(const FString&)

R5_TEST(1, "ForecastLiftgateButtons")
{
	FR5World W; W.Step(EVBoRStepRule::ButtonPressed, TEXT("Car.ReachButton"));
	W.World->SpawnActor<AR5ReachButton>(); auto* Gate = W.Gate(); FR5Log Log;
	W.Runner->CheckSetup(); GLog->FlushThreadedLogs();
	const auto Lines = R5Lines(W.Runner->SetupReport);
	TestEqual(TEXT("Four problems"), Lines.Num(), 4);
	TestTrue(TEXT("Placed plus two generated buttons"), Lines.Contains(TEXT("Tag Car.ReachButton: found 3 (need 1)")));
	TestTrue(TEXT("Play mismatch wording"), Lines.Contains(TEXT("01: tag Car.ReachButton type mismatch or missing")));
	for (const TCHAR* Slot : {TEXT("OpenButtonClass"), TEXT("CloseButtonClass")})
	{
		TestTrue(TEXT("Slot source explanation"), Lines.ContainsByPredicate([&](const FString& L) { return L.Contains(Gate->GetName()) && L.Contains(Slot) && L.Contains(TEXT("R5ReachButton")) && L.Contains(TEXT("Car.ReachButton")); }));
	}
	TestEqual(TEXT("Header count"), Log.Count(TEXT("setup check: 4 problems, 0 notices"), ELogVerbosity::Log), 1);
	return true;
}

R5_TEST(2, "EmptyButtonSlots")
{
	FR5World W; W.Step(EVBoRStepRule::ButtonPressed, TEXT("Car.ReachButton")); W.World->SpawnActor<AR5ReachButton>();
	auto* Gate = W.Gate(); Gate->OpenButtonClass = nullptr; Gate->CloseButtonClass = nullptr;
	W.Runner->CheckSetup(); TestEqual(TEXT("No forecast tags for empty slots"), W.Runner->SetupReport, FString(TEXT("No issues")));
	Gate->OpenButtonClass = AR5ReachButton::StaticClass(); Gate->CloseButtonClass = AR5ReachButton::StaticClass();
	auto* Class = AR5ReachButton::StaticClass(); const auto SavedFlags = Class->ClassFlags;
	for (EClassFlags Flag : {CLASS_Abstract, CLASS_Deprecated, CLASS_NewerVersionExists})
	{
		Class->ClassFlags = SavedFlags | Flag; W.Runner->CheckSetup(); Class->ClassFlags = SavedFlags;
		TestEqual(TEXT("Unusable classes fall back to untagged default"), W.Runner->SetupReport, FString(TEXT("No issues")));
	}
	return true;
}

R5_TEST(3, "ForecastKeyAndNestedButton")
{
	FR5World W; W.Step(EVBoRStepRule::KeyHeld, TEXT("Car.FrontKey"));
	auto* Table = W.World->SpawnActor<AVBoRKeyTable>(); Table->KeyClass = AR5FrontKey::StaticClass();
	W.Runner->CheckSetup(); TestEqual(TEXT("Generated key is valid target"), W.Runner->SetupReport, FString(TEXT("No issues")));
	auto* Defaults = GetMutableDefault<AR5FrontKey>(); const auto Saved = Defaults->UnlockButtonClass;
	Defaults->UnlockButtonClass = AR5FrontKeyButton::StaticClass();
	W.Runner->CheckSetup(); Defaults->UnlockButtonClass = Saved;
	TestTrue(TEXT("Key and nested button counted"), W.Runner->SetupReport.Contains(TEXT("Tag Car.FrontKey: found 2 (need 1)")));
	TestTrue(TEXT("Nested slot described"), W.Runner->SetupReport.Contains(TEXT("UnlockButtonClass")) && W.Runner->SetupReport.Contains(TEXT("R5FrontKey")));
	TestEqual(TEXT("Three problems"), R5Lines(W.Runner->SetupReport).Num(), 3);
	Table->Destroy(); auto* Placed = W.World->SpawnActor<AR5FrontKey>(); Placed->LockButtonClass = AR5FrontKeyButton::StaticClass();
	W.Runner->CheckSetup(); TestTrue(TEXT("Placed key button forecast"), W.Runner->SetupReport.Contains(TEXT("Tag Car.FrontKey: found 2 (need 1)")));
	TestTrue(TEXT("Placed key and lock slot identified"), W.Runner->SetupReport.Contains(Placed->GetName() + TEXT(" LockButtonClass")));
	Placed->LockButtonClass = nullptr; W.Runner->CheckSetup(); TestEqual(TEXT("Placed key alone"), W.Runner->SetupReport, FString(TEXT("No issues")));
	Table = W.World->SpawnActor<AVBoRKeyTable>(); Table->KeyClass = nullptr; W.Runner->CheckSetup();
	TestEqual(TEXT("Empty KeyClass has no forecast key"), W.Runner->SetupReport, FString(TEXT("No issues")));
	Table->KeyClass = AR5FrontKey::StaticClass(); W.Runner->CheckSetup();
	TestTrue(TEXT("Placed plus table key"), W.Runner->SetupReport.Contains(TEXT("Tag Car.FrontKey: found 2 (need 1)")));
	return true;
}

R5_TEST(4, "MissingTarget")
{
	FR5World W; W.Step(EVBoRStepRule::ButtonPressed, TEXT("Car.Missing")); W.Runner->CheckSetup();
	TestEqual(TEXT("Exact Play messages"), W.Runner->SetupReport, FString(TEXT("Tag Car.Missing: found 0 (need 1)\n01: tag Car.Missing type mismatch or missing"))); return true;
}

R5_TEST(5, "PlayValidationParity")
{
	FR5World W(EWorldType::Game); W.Alignment->DestroyComponent();
	W.Step(EVBoRStepRule::ButtonPressed, TEXT("Car.Duplicate"));
	W.Start->Tags.Add(TEXT("Car.Duplicate")); W.World->SpawnActor<AVBoRPokeButton>()->Tags.Add(TEXT("Car.Duplicate"));
	auto* BadMarker = W.Add<UInputComponent>(); BadMarker->ComponentTags.Add(TEXT("Car.BadMarker"));
	auto& S = W.Runner->Scenario->Steps[0]; S.StepId = TEXT("17"); S.MarkerTag = TEXT("Car.BadMarker"); S.RouteTag = TEXT("Car.MissingRoute");
	FVBoRScenarioStep Second; Second.StepId = TEXT("02"); Second.PanelTag = TEXT("Car.MissingPanel"); W.Runner->Scenario->Steps.Add(Second);
	W.Runner->CheckSetup(); const FString Before = W.Runner->SetupReport;
	W.Play(); TestFalse(TEXT("Invalid setup blocks Play"), W.Runner->InitializeFlow());
	TestEqual(TEXT("All validation lines in same order"), Before, FString::Join(R5Lines(W.Runner->ValidationProblems), TEXT("\n")));
	TestFalse(TEXT("No alignment-state check"), Before.Contains(TEXT("Alignment required")));
	// Exercise valid routes with missing footprint and footprint crossing too (D2)
	W.Runner->Scenario->Steps.SetNum(1); auto& RouteStep = W.Runner->Scenario->Steps[0]; RouteStep.RouteTag = TEXT("Car.Route");
	auto* Route = W.World->SpawnActor<AVBoRFloorRoute>(); Route->Tags.Add(TEXT("Car.Route"));
	for (bool Footprint : {false, true})
	{
		if (Footprint) { auto* Box = W.Add<UBoxComponent>(); Box->SetBoxExtent(FVector(10000)); Box->ComponentTags.Add(TEXT("Car.Footprint")); }
		W.Runner->CheckSetup(); W.Runner->InitializeFlow();
		TestEqual(TEXT("Route validation parity"), W.Runner->SetupReport, FString::Join(R5Lines(W.Runner->ValidationProblems), TEXT("\n")));
	}
	return true;
}

R5_TEST(6, "PlayUsesActualActors")
{
	FR5World W(EWorldType::Game); W.Step(EVBoRStepRule::ButtonPressed, TEXT("Car.ReachButton")); W.World->SpawnActor<AR5ReachButton>(); W.Gate(); W.Play();
	W.Runner->CheckSetup(); TestTrue(TEXT("Three actual actors only"), W.Runner->SetupReport.Contains(TEXT("found 3 (need 1)")));
	TestFalse(TEXT("No duplicate forecasts"), W.Runner->SetupReport.Contains(TEXT("found 5")));
	TestTrue(TEXT("Slot explanation retained in Play"), W.Runner->SetupReport.Contains(TEXT("OpenButtonClass"))); return true;
}

R5_TEST(7, "StartButtonAndPower")
{
	FR5World W; W.Start->Tags.Reset(); W.Runner->CheckSetup(); TestTrue(TEXT("Operator only notice"), W.Runner->SetupReport.Contains(TEXT("operator power only")));
	W.Start->Tags.Add(TEXT("Car.StartButton")); auto* Second = W.World->SpawnActor<AVBoRPokeButton>(); Second->Tags.Add(TEXT("Car.StartButton"));
	W.Runner->CheckSetup(); TestEqual(TEXT("Multiple start buttons allowed"), W.Runner->SetupReport, FString(TEXT("No issues")));
	auto* Bad = W.World->SpawnActor<AActor>(); Bad->Tags.Add(TEXT("Car.StartButton")); W.Runner->CheckSetup();
	TestEqual(TEXT("One invalid actor"), R5Lines(W.Runner->SetupReport).Num(), 1); TestTrue(TEXT("Invalid actor identified"), W.Runner->SetupReport.Contains(Bad->GetName()));
	Bad->Destroy(); auto* Component = W.Add<UInputComponent>(); Component->ComponentTags.Add(TEXT("Car.StartButton")); W.Runner->CheckSetup();
	TestTrue(TEXT("Component tag ignored by power"), W.Runner->SetupReport.Contains(Component->GetName())); Component->DestroyComponent();
	W.Power->DestroyComponent(); W.Runner->CheckSetup(); TestTrue(TEXT("Missing power notice"), W.Runner->SetupReport.Contains(TEXT("no power component")));
	W.Add<UVBoRVehiclePowerComponent>(); W.Add<UVBoRVehiclePowerComponent>(); W.Runner->CheckSetup();
	TestTrue(TEXT("Duplicate power disabled"), W.Runner->SetupReport.Contains(TEXT("found 2; power disabled"))); return true;
}

R5_TEST(8, "MirrorAttachment")
{
	FR5World W; auto* Door = W.World->SpawnActor<AVBoRHingedPanel>();
	for (int32 I = 0; I < 3; ++I)
	{
		auto* Mirror = W.World->SpawnActor<AVBoRFoldingMirror>();
		if (I < 2) { Mirror->AttachToActor(I == 0 ? static_cast<AActor*>(Door) : W.Runner, FAttachmentTransformRules::KeepWorldTransform); }
		W.Runner->CheckSetup(); TestEqual(TEXT("Only off-door mirrors reported"), W.Runner->SetupReport.Contains(Mirror->GetName()), I != 0);
	}
	return true;
}

R5_TEST(9, "HandleClaims")
{
	FR5World W; auto* Door = W.World->SpawnActor<AVBoRHingedPanel>();
	for (int32 I = 0; I < 5; ++I)
	{
		auto* Part = W.World->SpawnActor<AVBoRAxisPart>(); Part->DoorHandleSide = I == 4 ? EVBoRDoorHandleSide::None : I == 3 ? EVBoRDoorHandleSide::Inside : EVBoRDoorHandleSide::Grip;
		if (I == 0) { Part->AttachToActor(Door, FAttachmentTransformRules::KeepWorldTransform); }
		if (I == 1) { Door->GripAxisHandle = Part; }
		W.Runner->CheckSetup();
		const FString Expected = Part->GetName() + (I == 2 ? TEXT(": unclaimed Grip; grabbing disabled") : TEXT(": unclaimed handle; cannot unlock door"));
		TestEqual(TEXT("Only unclaimed non-None parts"), W.Runner->SetupReport.Contains(Expected), I == 2 || I == 3);
	}
	return true;
}

R5_TEST(10, "GripSlotMismatch")
{
	for (int32 I = 0; I < 6; ++I)
	{
		FR5World W(EWorldType::Game); auto* Door = W.World->SpawnActor<AVBoRHingedPanel>(); auto* Part = W.World->SpawnActor<AVBoRAxisPart>();
		const int32 Slot = I < 2 ? I : I < 4 ? 2 : I - 4;
		Part->DoorHandleSide = I < 2 ? EVBoRDoorHandleSide::Grip : I == 2 || I == 4 ? EVBoRDoorHandleSide::Inside : EVBoRDoorHandleSide::Outside;
		if (Slot == 0) { Door->OutsideAxisHandle = Part; } else if (Slot == 1) { Door->InsideAxisHandle = Part; } else { Door->GripAxisHandle = Part; }
		const auto Setup = FR5TestAccess::Setup(Door); TestEqual(TEXT("One slot mismatch"), Setup.Problems.Num(), 1);
		TestEqual(TEXT("Slot wins in assessment"), Setup.Handles[Slot], Part);
		const FString Side = Slot == 0 ? TEXT("outside") : Slot == 1 ? TEXT("inside") : TEXT("grip");
		if (I >= 4) { TestEqual(TEXT("Old opposite wording"), Setup.Problems[0], FString::Printf(TEXT("%s slot %s has the opposite DoorHandleSide; slot wins."), *Side, *Part->GetName())); }
		else
		{
			const FString PartSide = I < 2 ? TEXT("Grip") : I == 2 ? TEXT("Inside") : TEXT("Outside");
			TestTrue(TEXT("New slot side and part"), Setup.Problems[0].Contains(Side) && Setup.Problems[0].Contains(Part->GetName()) && Setup.Problems[0].Contains(TEXT("DoorHandleSide ") + PartSide) && Setup.Problems[0].Contains(TEXT("slot wins")));
		}
		FR5Log Log; W.Play(); GLog->FlushThreadedLogs();
		TestEqual(TEXT("One BeginPlay mismatch warning"), Log.Count(TEXT("P2e setup: ") + Setup.Problems[0], ELogVerbosity::Warning), 1);
		TestEqual(TEXT("Slot remains connected after Play"), Slot == 0 ? Door->OutsideAxisHandle.Get() : Slot == 1 ? Door->InsideAxisHandle.Get() : Door->GripAxisHandle.Get(), Part);
	}
	return true;
}

R5_TEST(11, "PanelAssessmentIncluded")
{
	FR5World W; auto* Door = W.World->SpawnActor<AVBoRHingedPanel>(); auto* Part = W.World->SpawnActor<AVBoRAxisPart>();
	Part->DoorHandleSide = EVBoRDoorHandleSide::Grip; Door->OutsideAxisHandle = Part;
	W.Runner->CheckSetup(); const auto Setup = FR5TestAccess::Setup(Door);
	TestTrue(TEXT("Panel name and exact assessment"), W.Runner->SetupReport.Contains(Door->GetName() + TEXT(": ") + Setup.Problems[0]));
	Door->bUsePushFaceBox = true; Door->bPushOnMeshSurface = true; W.Runner->CheckSetup();
	for (const auto& Notice : FR5TestAccess::Setup(Door).Notices) { TestTrue(TEXT("Panel notice included"), W.Runner->SetupReport.Contains(Door->GetName() + TEXT(": ") + Notice)); }
	return true;
}

R5_TEST(12, "NoSideEffects")
{
	for (bool Play : {false, true})
	{
		FR5World W(Play ? EWorldType::Game : EWorldType::EditorPreview); auto* Gate = W.Gate();
		if (Play)
		{
			W.World->SpawnActor<APlayerController>(); W.Play(); W.Alignment->State = EVBoRAlignmentState::Applied; W.Runner->InitializeFlow();
		}
		const int32 Actors = R5Actors(W.World), Targets = FR5TestAccess::Targets(W.Runner), Index = W.Runner->GetStepIndex();
		const auto Token = W.Runner->GetToken(); const bool Running = W.Runner->IsRunning();
		const FString Record = W.Runner->LastRecord, Validation = W.Runner->ValidationProblems;
		const int32 Keys = W.Runner->InputComponent ? W.Runner->InputComponent->KeyBindings.Num() : 0;
		TArray<UObject*> Bound, CloseBound;
		if (Gate->OpenButton) { Bound = Gate->OpenButton->OnButtonPressed.GetAllObjects(); CloseBound = Gate->CloseButton->OnButtonPressed.GetAllObjects(); }
		W.Runner->CheckSetup();
		TArray<UObject*> After, CloseAfter;
		if (Gate->OpenButton) { After = Gate->OpenButton->OnButtonPressed.GetAllObjects(); CloseAfter = Gate->CloseButton->OnButtonPressed.GetAllObjects(); }
		TestEqual(TEXT("Actor count"), R5Actors(W.World), Actors); TestEqual(TEXT("Targets"), FR5TestAccess::Targets(W.Runner), Targets);
		TestEqual(TEXT("Record"), W.Runner->LastRecord, Record); TestEqual(TEXT("Validation"), W.Runner->ValidationProblems, Validation);
		TestEqual(TEXT("Running"), W.Runner->IsRunning(), Running); TestEqual(TEXT("Step"), W.Runner->GetStepIndex(), Index);
		TestEqual(TEXT("Generation"), W.Runner->GetToken().Generation, Token.Generation);
		TestEqual(TEXT("Input bindings"), W.Runner->InputComponent ? W.Runner->InputComponent->KeyBindings.Num() : 0, Keys);
		TestTrue(TEXT("Gate button bindings"), Bound == After && CloseBound == CloseAfter);
	}
	return true;
}

R5_TEST(13, "ReportAndLogLines")
{
	FR5World W; FR5Log Log; W.Runner->CheckSetup(); GLog->FlushThreadedLogs();
	TestEqual(TEXT("Clean report"), W.Runner->SetupReport, FString(TEXT("No issues")));
	TestEqual(TEXT("Clean header"), Log.Count(TEXT("setup check: 0 problems, 0 notices"), ELogVerbosity::Log), 1);
	TestEqual(TEXT("No warning"), Log.Count(TEXT(""), ELogVerbosity::Warning), 0);
	W.Start->Tags.Reset(); W.Alignment->DestroyComponent(); W.Runner->Scenario = nullptr; Log.Lines.Reset();
	W.Runner->CheckSetup(); GLog->FlushThreadedLogs(); const auto Lines = R5Lines(W.Runner->SetupReport);
	TestEqual(TEXT("Two problems plus notice"), Lines.Num(), 3);
	TestEqual(TEXT("Header once"), Log.Count(TEXT("setup check: 2 problems, 1 notices"), ELogVerbosity::Log), 1);
	TestEqual(TEXT("Warnings separately"), Log.Count(TEXT("setup check:"), ELogVerbosity::Warning), 2);
	for (const FString& Line : Lines)
	{
		TestEqual(TEXT("Report line logged individually"), Log.Count(Line, Line.Contains(TEXT("operator power only")) ? ELogVerbosity::Log : ELogVerbosity::Warning), 1);
	}
	return true;
}
#undef R5_TEST
#endif
