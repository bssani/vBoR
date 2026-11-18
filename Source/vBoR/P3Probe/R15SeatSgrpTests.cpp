#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "P3VehicleMock.h"
#include "../Interaction/VBoRAlignPointComponent.h"
#include "../Interaction/VBoREyePointComponent.h"
#include "../Interaction/VBoRScenarioRunner.h"
#include "../Interaction/VBoRVehicleAlignmentComponent.h"
#include "../Interaction/VBoRViewpointComponent.h"
#include "Components/InputComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/WorldSettings.h"
#include "XRTrackingSystemBase.h"

struct FR15TestAccess
{
	static void NewFrame(AVBoRScenarioRunner* R) { R->LiveFrame = MAX_uint64; }
	static FString Label(AVBoRScenarioRunner* R) { return R->ActionLabel(); }
	static bool Key(AVBoRScenarioRunner* R, FKey Key)
	{
		NewFrame(R);
		for (auto& Binding : R->InputComponent->KeyBindings)
		{
			if (Binding.Chord.Key == Key && Binding.KeyEvent == IE_Pressed)
			{ Binding.KeyDelegate.Execute(Key); return true; }
		}
		return false;
	}
};

namespace
{
	constexpr auto R15Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;
	using R15Button = EVBoROperatorButton;

	class FR15Head : public FXRTrackingSystemBase
	{
	public:
		FR15Head() : FXRTrackingSystemBase(nullptr) {}
		FTransform Pose;
		virtual FName GetSystemName() const override { return TEXT("R15SyntheticHead"); }
		virtual int32 GetXRSystemFlags() const override { return 0; }
		virtual bool EnumerateTrackedDevices(TArray<int32>& Out, EXRTrackedDeviceType) override { Out.Add(0); return true; }
		virtual bool IsTracking(int32 Device) override { return Device == 0; }
		virtual bool GetCurrentPose(int32 Device, FQuat& Rotation, FVector& Position) override
		{ Rotation = Pose.GetRotation(); Position = Pose.GetLocation(); return IsTracking(Device); }
		virtual float GetWorldToMetersScale() const override { return 100; }
		virtual void ResetOrientationAndPosition(float) override {}
		virtual bool IsHeadTrackingAllowed() const override { return true; }
		virtual EHMDTrackingOrigin::Type GetTrackingOrigin() const override { return EHMDTrackingOrigin::Stage; }
		virtual FVector2D GetPlayAreaBounds(EHMDTrackingOrigin::Type) const override { return FVector2D(10000,10000); }
	};

	struct FR15World
	{
		UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
		APawn* Pawn;
		AP3VehicleMock* Car;
		AVBoRScenarioRunner* Runner;
		UVBoRViewpointComponent* Views[5];
		TSharedPtr<IXRTrackingSystem, ESPMode::ThreadSafe> OriginalXR = GEngine->XRSystem;
		TSharedPtr<FR15Head, ESPMode::ThreadSafe> Head = MakeShared<FR15Head, ESPMode::ThreadSafe>();
		FR15World(bool bLegacy = false)
		{
			GEngine->XRSystem = Head;
			GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
			auto* PC = World->SpawnActor<APlayerController>(); Pawn = World->SpawnActor<APawn>();
			Pawn->SetRootComponent(NewObject<USceneComponent>(Pawn)); Pawn->GetRootComponent()->RegisterComponent(); PC->Possess(Pawn);
			Car = World->SpawnActor<AP3VehicleMock>();
			Car->SetActorTransform(FTransform(FRotator(0,25,0), FVector(137,-74,17)));
			Views[0] = AddView(bLegacy ? TEXT("Car.DriverView") : TEXT("Car.DriverSgRP"), FVector(-30,-37,40));
			Views[1] = Car->PassengerView; Views[2] = Car->RearView;
			if (!bLegacy)
			{
				Views[1]->ComponentTags = {TEXT("Car.PassengerSgRP")}; Views[2]->ComponentTags = {TEXT("Car.RearLeftSgRP")};
			}
			Views[3] = bLegacy ? nullptr : AddView(TEXT("Car.RearCenterSgRP"), FVector(-110,0,43));
			Views[4] = bLegacy ? nullptr : AddView(TEXT("Car.RearRightSgRP"), FVector(-110,37,43));
			Runner = World->SpawnActor<AVBoRScenarioRunner>(); Runner->Scenario = NewObject<UVBoRScenarioAsset>(Runner);
			FVBoRScenarioStep Step; Step.StepId = TEXT("Observe"); Step.Rule = EVBoRStepRule::Confirm; Runner->Scenario->Steps = {Step};
			World->InitializeActorsForPlay(FURL()); World->GetWorldSettings()->NotifyBeginPlay();
			FVBoRBuckSavedAlignment Saved; Saved.Version = 2; Saved.SavedAt = TEXT("test");
			Saved.TrackingOrigin = EHMDTrackingOrigin::Stage; Saved.PlayAreaCm = Head->GetPlayAreaBounds(EHMDTrackingOrigin::Stage);
			for (const UVBoRAlignPointComponent* Point : Car->AlignPoints)
			{
				FVBoRAlignPointSample Sample; Sample.Order = Point->Order; Sample.StageCm = Point->GetComponentLocation(); Sample.Frames = 15;
				Saved.Points.Add(Sample);
			}
			Car->Alignment->InitializeAlignment(Pawn, &Saved, Saved.TrackingOrigin, Saved.PlayAreaCm);
			Head->Pose = Car->DriverEye->GetComponentTransform().GetRelativeTransform(Pawn->GetActorTransform());
			Runner->InitializeFlow(); Runner->ProcessFlow(1);
		}
		~FR15World()
		{
			World->EndPlay(EEndPlayReason::Quit); GEngine->DestroyWorldContext(World); World->DestroyWorld(false); GEngine->XRSystem = OriginalXR;
		}
		UVBoRViewpointComponent* AddView(FName Tag, FVector Position)
		{
			auto* View = NewObject<UVBoRViewpointComponent>(Car); Car->AddInstanceComponent(View);
			View->SetupAttachment(Car->GetRootComponent()); View->ComponentTags.Add(Tag); View->RegisterComponent(); View->SetRelativeLocation(Position);
			return View;
		}
		void Finish()
		{
			Car->Alignment->AdvanceSwitch(Car->Alignment->FadeSeconds); Car->Alignment->AdvanceSwitch(Car->Alignment->FadeSeconds);
			Runner->ProcessFlow(1);
		}
		void Button(R15Button Button)
		{ FR15TestAccess::NewFrame(Runner); Runner->PressOperatorButton(Button); Runner->ProcessFlow(0); Finish(); }
		bool Key(FKey Key, bool bPanel = false)
		{
			FR15TestAccess::NewFrame(Runner);
			const bool bHandled = bPanel ? Runner->HandleOperatorKey(Key) : FR15TestAccess::Key(Runner, Key);
			Runner->ProcessFlow(0); Finish(); return bHandled;
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FR15NewSeats, "vBoR.R15.E1FiveSeatKeys", R15Flags)
bool FR15NewSeats::RunTest(const FString&)
{
	FR15World W;
	if (!TestTrue(TEXT("Point alignment starts flow"), W.Runner->IsRunning())) { return false; }
	const FTransform Base = W.Pawn->GetActorTransform();
	const FKey Keys[] = {EKeys::Home, EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four};
	const R15Button Buttons[] = {R15Button::Driver, R15Button::Passenger, R15Button::Rear, R15Button::RearCenter, R15Button::RearRight};
	const TCHAR* Names[] = {TEXT("Driver"), TEXT("Passenger"), TEXT("Rear left"), TEXT("Rear center"), TEXT("Rear right")};
	for (bool bPanel : {false, true})
	{
		for (int32 I : {1,2,3,4,0})
		{
			TestTrue(TEXT("Seat key handled"), W.Key(Keys[I], bPanel));
			const FVector Expected = Base.GetLocation() + W.Views[I]->GetComponentLocation() - W.Views[0]->GetComponentLocation();
			TestTrue(TEXT("World SgRP difference only, no accumulated offset"), W.Pawn->GetActorLocation().Equals(Expected, 0.001));
			TestTrue(TEXT("Pawn rotation preserved"), W.Pawn->GetActorQuat().Equals(Base.GetRotation(), 0.0001));
			TestEqual(TEXT("Seat name"), W.Car->Alignment->SeatName(), FString(Names[I]));
			const auto Snapshot = W.Runner->GetOperatorSnapshot();
			TestEqual(TEXT("Current seat badge"), Snapshot.Buttons[static_cast<int32>(Buttons[I])].Badge, FString(TEXT("Current")));
			W.Button(Buttons[I]);
			TestTrue(TEXT("Operator button uses same destination"), W.Pawn->GetActorLocation().Equals(Expected, 0.001));
		}
	}
	W.Runner->CheckSetup(); TestFalse(TEXT("New tags need no rename notices"), W.Runner->SetupReport.Contains(TEXT(": rename to ")));
	W.Views[3]->DestroyComponent(); W.Views[4]->DestroyComponent();
	for (R15Button Button : {R15Button::RearCenter, R15Button::RearRight})
	{
		TestEqual(TEXT("Missing seats rejected"), W.Runner->EvaluateOperatorButton(Button).Reason, FString(TEXT("Seat not found")));
		W.Button(Button); TestTrue(TEXT("Rejected seat keeps Pawn"), W.Pawn->GetActorTransform().Equals(Base, 0.001));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FR15LegacySeats, "vBoR.R15.E2LegacyTags", R15Flags)
bool FR15LegacySeats::RunTest(const FString&)
{
	FR15World W(true); const FTransform Base = W.Pawn->GetActorTransform();
	TestTrue(TEXT("Legacy points start flow"), W.Runner->IsRunning());
	W.Runner->CheckSetup();
	TArray<FString> Lines; W.Runner->SetupReport.ParseIntoArrayLines(Lines, true);
	int32 Renames = 0; for (const auto& Line : Lines) { Renames += Line.Contains(TEXT(": rename to ")); }
	TestEqual(TEXT("Exactly three rename notices"), Renames, 3);
	for (const TCHAR* Line : {TEXT("Car.DriverView: rename to Car.DriverSgRP"), TEXT("Car.PassengerView: rename to Car.PassengerSgRP"), TEXT("Car.RearView: rename to Car.RearLeftSgRP")})
	{ TestTrue(TEXT("Legacy rename destination"), Lines.Contains(FString(Line))); }
	for (int32 I : {1,2,0})
	{
		W.Key(I == 1 ? EKeys::One : I == 2 ? EKeys::Two : EKeys::Home);
		TestTrue(TEXT("Legacy seat uses original difference"), W.Pawn->GetActorLocation().Equals(Base.GetLocation() + W.Views[I]->GetComponentLocation() - W.Views[0]->GetComponentLocation(), 0.001));
	}
	for (FName Tag : {FName(TEXT("Car.PassengerSgRP")), FName(TEXT("Car.PassengerView"))})
	{
		auto& Step = W.Runner->Scenario->Steps[0]; Step.Rule = EVBoRStepRule::Viewpoint; Step.TargetTag = Tag;
		TestTrue(TEXT("New and saved scenario tags resolve legacy point"), W.Runner->InitializeFlow());
	}
	for (FKey Key : {EKeys::Three, EKeys::Four})
	{
		TestTrue(TEXT("Missing legacy seat key handled"), W.Key(Key));
		TestTrue(TEXT("Missing legacy seat never moves Pawn"), W.Pawn->GetActorTransform().Equals(Base, 0.001));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FR15Labels, "vBoR.R15.E3SeatLabels", R15Flags)
bool FR15Labels::RunTest(const FString&)
{
	FR15World W;
	const FName Tags[] = {TEXT("Car.PassengerSgRP"), TEXT("Car.RearLeftSgRP"), TEXT("Car.RearCenterSgRP"), TEXT("Car.RearRightSgRP")};
	const TCHAR* Labels[] = {TEXT("Move to passenger seat"), TEXT("Move to rear left seat"), TEXT("Move to rear center seat"), TEXT("Move to rear right seat")};
	for (int32 I = 0; I < 4; ++I)
	{
		W.Key(EKeys::Home);
		auto& Step = W.Runner->Scenario->Steps[0]; Step.StepId = TEXT("Seat"); Step.Rule = EVBoRStepRule::Viewpoint; Step.TargetTag = Tags[I];
		TestTrue(TEXT("Seat scenario starts"), W.Runner->InitializeFlow()); W.Runner->ProcessFlow(1);
		TestEqual(TEXT("Participant destination label"), FR15TestAccess::Label(W.Runner), FString(Labels[I]));
		W.Runner->QueueCommand({EVBoRFlowCommand::Viewpoint, EVBoRCommandSource::Participant, W.Runner->GetToken()}); W.Runner->ProcessFlow(0); W.Finish();
		TestTrue(TEXT("Seat stage reaches its point"), W.Car->Alignment->IsAtViewpoint(W.Views[I+1]));
		TestEqual(TEXT("Arrival alone preserves stage"), W.Runner->GetStepIndex(), 0);
		TestEqual(TEXT("Arrival offers return"), FR15TestAccess::Label(W.Runner), FString(TEXT("Return to driver seat")));
	}
	const auto* Defaults = NewObject<UVBoRScenarioAsset>();
	for (const auto& Step : Defaults->Steps)
	{
		if (Step.StepId == TEXT("14")) { TestEqual(TEXT("Default 14"), Step.TargetTag, FName(TEXT("Car.PassengerSgRP"))); }
		if (Step.StepId == TEXT("15")) { TestEqual(TEXT("Default 15"), Step.TargetTag, FName(TEXT("Car.RearLeftSgRP"))); }
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FR15DuplicateSeats, "vBoR.R15.MixedTagDuplicates", R15Flags)
bool FR15DuplicateSeats::RunTest(const FString&)
{
	FR15World W;
	auto* Extra = W.AddView(TEXT("Car.PassengerView"), FVector(20,40,45));
	auto& Step = W.Runner->Scenario->Steps[0]; Step.Rule = EVBoRStepRule::Viewpoint; Step.TargetTag = TEXT("Car.PassengerSgRP");
	TestFalse(TEXT("Mixed passenger tags block ambiguous scenario"), W.Runner->InitializeFlow());
	TestTrue(TEXT("Both aliases counted"), W.Runner->ValidationProblems.Contains(TEXT("found 2 (need 1)")));
	TestEqual(TEXT("Mixed tags disable operator seat"), W.Runner->EvaluateOperatorButton(R15Button::Passenger).Reason, FString(TEXT("Scenario has not started")));
	Step.Rule = EVBoRStepRule::Confirm; Step.TargetTag = NAME_None;
	TestTrue(TEXT("Unrelated scenario can start"), W.Runner->InitializeFlow());
	TestEqual(TEXT("Ambiguous optional seat rejected"), W.Runner->EvaluateOperatorButton(R15Button::Passenger).Reason, FString(TEXT("Seat not found")));
	Extra->DestroyComponent();
	Step.Rule = EVBoRStepRule::Viewpoint; Step.TargetTag = TEXT("Car.PassengerView");
	TestTrue(TEXT("Saved scenario tag resolves new point"), W.Runner->InitializeFlow());
	Extra = W.AddView(TEXT("Car.DriverView"), FVector(-30,-37,40));
	TestTrue(TEXT("New plus legacy driver reference rejected"), W.Car->Alignment->SwitchRejection(W.Views[1]).Contains(TEXT("Multiple")));
	Extra->DestroyComponent();
	return true;
}
#endif
