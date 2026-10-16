#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "P5hTestFixture.h"
#include "P5dTestFixture.h"
#include "P5eTestFixture.h"
#include "P5fTestFixture.h"
#include "P4aTestFixture.h"
#include "../P2Probe/P2dTestFixture.h"
#include "P3VehicleMock.h"
#include "../Spectator/VBoRSpectatorSubsystem.h"
#include "../Interaction/VBoRScenarioRunner.h"
#include "../Interaction/VBoRVehicleAlignmentComponent.h"
#include "../Interaction/VBoRKeyTable.h"
#include "../Interaction/VBoRPokeButton.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/WorldSettings.h"
#include "Interaction/IsdkGrabberComponent.h"
#include "IsdkRuntimeSettings.h"
#include "Misc/App.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Layout/ArrangedChildren.h"
#include "Rendering/SlateRenderer.h"
#include "UObject/UnrealType.h"
#include "Widgets/Text/STextBlock.h"

struct FP5hTestAccess
{
	static FString Line(UVBoRSpectatorSubsystem* S) { return S->PartLine; }
	static bool HasResources(UVBoRSpectatorSubsystem* S) { return S->WidgetRenderer != nullptr || S->SlateWidget.IsValid() || S->RenderTarget != nullptr; }
	static FVector2D Size() { return UVBoRSpectatorSubsystem::DrawSize; }
	static TSharedRef<SWidget> Widget(UVBoRSpectatorSubsystem* S, const FText& Mission, const FString& Parts, const FText& Label = FText::GetEmpty())
	{
		S->CurrentBanner = {Label.IsEmpty() ? FText::Format(S->MissionLabel,14,14) : Label,Mission,14,14,true};
		S->PartLine = Parts;
		return S->MakeWidget();
	}
};

namespace
{
	struct FP5hWorld
	{
		UWorld* World = UWorld::CreateWorld(EWorldType::Game,false);
		FP5hWorld() { GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World); }
		void Play() { World->InitializeActorsForPlay(FURL()); World->GetWorldSettings()->NotifyBeginPlay(); }
		~FP5hWorld() { World->EndPlay(EEndPlayReason::Quit); GEngine->DestroyWorldContext(World); World->DestroyWorld(false); }
	};
	struct FP5hFlow : FP5hWorld
	{
		AP3VehicleMock* Car;
		AVBoRScenarioRunner* Runner;
		FP5hFlow()
		{
			auto* Pawn = World->SpawnActor<APawn>();
			Pawn->SetRootComponent(NewObject<USceneComponent>(Pawn)); Pawn->GetRootComponent()->RegisterComponent();
			Car = World->SpawnActor<AP3VehicleMock>(); Runner = World->SpawnActor<AVBoRScenarioRunner>();
			Runner->Scenario = NewObject<UVBoRScenarioAsset>(Runner);
			for (auto& Step : Runner->Scenario->Steps)
			{ Step.Rule = EVBoRStepRule::Confirm; Step.ConfirmBy = EVBoRConfirmBy::Either; Step.TargetTag = NAME_None; Step.MarkerTag = NAME_None; Step.bRequireBaseViewpoint = false; }
			// 3번째가 08인 14단계 시험 시나리오 (P5h E3)
			Swap(Runner->Scenario->Steps[2],Runner->Scenario->Steps[3]);
			Play();
			FVBoRBuckSavedAlignment Saved; Saved.SavedAt = TEXT("2026-10-16T00:00:00Z"); Saved.TrackingOrigin = 3;
			Saved.PlayAreaCm = FVector2D(300,400); Saved.Capture.PositionCm = FVector(10,20,115); Saved.Capture.YawDeg = 30;
			Car->Alignment->InitializeAlignment(Pawn,&Saved,3,Saved.PlayAreaCm);
		}
		void Start() { Runner->InitializeFlow(); }
		void Command(EVBoRFlowCommand Action)
		{ Runner->QueueCommand({Action,EVBoRCommandSource::Operator,Runner->GetToken()}); Runner->ProcessFlow(0); }
		FVBoRSpectatorBanner Banner() { return VBoRSpectator::Banner(*Runner,FVBoRSpectatorLabels()); }
	};
	FString P5hLine(FVBoRSpectatorNames& Names) { return Names.Line(TEXT("Interacting: ")); }
	void P5hArrange(const TSharedRef<SWidget>& Widget, const FGeometry& Geometry, TArray<FArrangedWidget>& Texts, TArray<FArrangedWidget>& Borders)
	{
		if (Widget->GetType() == FName(TEXT("STextBlock"))) { Texts.Emplace(Widget,Geometry); }
		if (Widget->GetType() == FName(TEXT("SBorder"))) { Borders.Emplace(Widget,Geometry); }
		FArrangedChildren Children(EVisibility::Visible);
		Widget->ArrangeChildren(Geometry,Children);
		for (int32 I = 0; I < Children.Num(); ++I)
		{ P5hArrange(Children[I].Widget,Children[I].Geometry,Texts,Borders); }
	}
	UIsdkGrabberComponent* P5hPointer(AActor* Owner, int32 Hand = 0)
	{
		auto* G = NewObject<UIsdkGrabberComponent>(Owner);
		G->InteractorTags.AddTag(Hand == 0 ? IsdkGameplayTags::TAG_Isdk_Type_Device_Left : IsdkGameplayTags::TAG_Isdk_Type_Device_Right);
		return G;
	}
	void P5hEvent(UIsdkGrabbableComponent* Part, UIsdkGrabberComponent* Pointer, EIsdkPointerEventType Type)
	{
		FIsdkInteractionPointerEvent Event; Event.Interactable = Part; Event.Interactor = Pointer; Event.Type = Type;
		Part->GetInteractionPointerEventDelegate().Broadcast(Event);
	}
	FVBoRDoorHand P5hHand(FVector Point)
	{
		FVBoRDoorHand Hand; Hand.bTracked = true; Hand.PalmNormal = FVector(0,-1,0);
		for (FVector& P : Hand.Points) { P = Point; } return Hand;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5hNoHMD,"vBoR.P5h.E1NoHMDOrRenderer",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5hNoHMD::RunTest(const FString&)
{
	FP5hWorld W;
	const auto OriginalXR = GEngine->XRSystem; GEngine->XRSystem.Reset();
	W.Play(); auto* S = W.World->GetSubsystem<UVBoRSpectatorSubsystem>();
	TestNotNull(TEXT("Game world subsystem"),S);
	if (S)
	{
		S->OnWorldBeginPlay(*W.World);
		if (!FApp::CanEverRender()) { TestFalse(TEXT("nullrhi has no pending work"),S->IsTickable()); }
		for (int32 I = 0; I < 61; ++I) { S->Tick(0.5f); }
		TestFalse(TEXT("No target, widget or renderer without HMD"),FP5hTestAccess::HasResources(S));
		TestFalse(TEXT("Retry stops after 30 s without warnings"),S->IsTickable());
	}
	GEngine->XRSystem = OriginalXR;
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5hReady,"vBoR.P5h.E2E3ReadyAndThirdMission",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5hReady::RunTest(const FString&)
{
	FP5hFlow W;
	TestEqual(TEXT("Ready label"),W.Banner().Label.ToString(),FString(TEXT("GETTING READY")));
	TestTrue(TEXT("Ready has no mission"),W.Banner().Mission.IsEmpty());
	TestEqual(TEXT("Ready completed"),W.Banner().Completed,0); TestEqual(TEXT("Ready total"),W.Banner().Total,14);
	W.Start(); W.Command(EVBoRFlowCommand::Next); W.Command(EVBoRFlowCommand::Next);
	const auto B = W.Banner();
	TestEqual(TEXT("Third mission label hides original ID"),B.Label.ToString(),FString(TEXT("MISSION 3 / 14")));
	TestEqual(TEXT("English mission"),B.Mission.ToString(),FString(TEXT("Open the door.")));
	TestEqual(TEXT("Two completed"),B.Completed,2); TestTrue(TEXT("Progress fraction"),FMath::IsNearlyEqual(B.Progress(),2.f/14));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5hEnglish,"vBoR.P5h.E4EnglishFallbacks",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5hEnglish::RunTest(const FString&)
{
	FVBoRScenarioStep Step; Step.StepId = TEXT("08"); Step.Instruction = FText::FromString(TEXT("한국어는 TV에 쓰지 않음"));
	const FText Unknown = FVBoRSpectatorLabels().Unknown;
	TestEqual(TEXT("Empty newly serialized field falls back"),VBoRSpectator::MissionText(Step,Unknown).ToString(),FString(TEXT("Open the door.")));
	Step.SpectatorText = FText::FromString(TEXT("Custom mission"));
	TestEqual(TEXT("Asset English override"),VBoRSpectator::MissionText(Step,Unknown).ToString(),FString(TEXT("Custom mission")));
	Step.SpectatorText = FText::GetEmpty(); Step.StepId = TEXT("other");
	TestEqual(TEXT("Unknown ID fallback"),VBoRSpectator::MissionText(Step,Unknown).ToString(),FString(TEXT("Mission in progress")));
	Step.StepId = TEXT("17"); TestTrue(TEXT("No invented 17"),VBoRSpectator::MissionText(Step,Unknown).EqualTo(Unknown));
	for (int32 I = 1; I <= 20; ++I)
	{
		if (I == 17) { continue; }
		Step.StepId = FName(*FString::Printf(TEXT("%02d"),I));
		TestFalse(TEXT("All 19 original IDs have English defaults"),VBoRSpectator::MissionText(Step,Unknown).EqualTo(Unknown));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5hNavigation,"vBoR.P5h.E5OperatorNavigation",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5hNavigation::RunTest(const FString&)
{
	FP5hFlow W; W.Start(); W.Command(EVBoRFlowCommand::Next); W.Command(EVBoRFlowCommand::Next);
	W.Command(EVBoRFlowCommand::Previous);
	TestEqual(TEXT("Previous label"),W.Banner().Label.ToString(),FString(TEXT("MISSION 2 / 14")));
	TestEqual(TEXT("Previous progress"),W.Banner().Completed,1);
	TestEqual(TEXT("Previous mission"),W.Banner().Mission.ToString(),FString(TEXT("Check the exterior lights as you approach.")));
	W.Command(EVBoRFlowCommand::Next); TestEqual(TEXT("Skip returns to third"),W.Banner().Completed,2);
	W.Command(EVBoRFlowCommand::Restart);
	TestEqual(TEXT("Restart label"),W.Banner().Label.ToString(),FString(TEXT("MISSION 1 / 14")));
	TestEqual(TEXT("Restart progress"),W.Banner().Completed,0);
	TestEqual(TEXT("Restart mission"),W.Banner().Mission.ToString(),FString(TEXT("Approach the vehicle with the key.")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5hFree,"vBoR.P5h.E6FreeModeEntry",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5hFree::RunTest(const FString&)
{
	FP5hFlow W; W.Start();
	for (int32 I = 0; I < 14; ++I) { W.Command(EVBoRFlowCommand::Next); }
	TestEqual(TEXT("Completion free label"),W.Banner().Label.ToString(),FString(TEXT("FREE MODE")));
	TestEqual(TEXT("Completion mission"),W.Banner().Mission.ToString(),FString(TEXT("All missions complete. Explore the vehicle freely.")));
	TestTrue(TEXT("Full progress visible"),W.Banner().bShowProgress && W.Banner().Completed == 14 && W.Banner().Progress() == 1);
	W.Command(EVBoRFlowCommand::Restart); W.Command(EVBoRFlowCommand::EnterFreeMode);
	TestEqual(TEXT("End free label"),W.Banner().Label.ToString(),FString(TEXT("FREE MODE")));
	TestEqual(TEXT("End exploration mission"),W.Banner().Mission.ToString(),FString(TEXT("Explore the vehicle freely.")));
	TestFalse(TEXT("End hides progress"),W.Banner().bShowProgress);
	FVBoRSpectatorLabels Labels; Labels.Free = FText::FromString(TEXT("CUSTOM FREE")); Labels.Explore = FText::FromString(TEXT("Custom exploration"));
	const auto Custom = VBoRSpectator::Banner(*W.Runner,Labels);
	TestEqual(TEXT("Editable state label"),Custom.Label.ToString(),FString(TEXT("CUSTOM FREE")));
	TestEqual(TEXT("Editable state mission"),Custom.Mission.ToString(),FString(TEXT("Custom exploration")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5hHold,"vBoR.P5h.E7ReleaseHold",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5hHold::RunTest(const FString&)
{
	FVBoRSpectatorNames N; N.Sample({TEXT("Console Knob")},0,1);
	TestEqual(TEXT("Knob active"),P5hLine(N),FString(TEXT("Interacting: Console Knob")));
	N.Sample({},1,1); N.Sample({},1.5,1); TestFalse(TEXT("Held 0.5 s after release"),P5hLine(N).IsEmpty());
	N.Sample({},2.1,1); TestTrue(TEXT("Hidden 1.1 s after release"),P5hLine(N).IsEmpty());
	N.Sample({TEXT("Console Knob")},3,2); N.Sample({},4,2); N.Sample({},5.1,2);
	TestFalse(TEXT("Hold duration configurable"),P5hLine(N).IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5hDeduplicate,"vBoR.P5h.E8DuplicateDoorNames",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5hDeduplicate::RunTest(const FString&)
{
	FVBoRSpectatorNames N; N.Sample({TEXT("Driver Door"),TEXT("Driver Door")},0,1);
	TestEqual(TEXT("Panel and handle shown once"),P5hLine(N),FString(TEXT("Interacting: Driver Door")));
	N.Sample({TEXT("Driver Door")},5,1); N.Sample({},6,1); N.Sample({},6.5,1);
	TestEqual(TEXT("Remaining handle keeps door active"),P5hLine(N),FString(TEXT("Interacting: Driver Door")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5hRecent,"vBoR.P5h.E9TwoMostRecent",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5hRecent::RunTest(const FString&)
{
	FVBoRSpectatorNames N; N.Sample({TEXT("Steering Wheel")},0,1);
	N.Sample({TEXT("Steering Wheel"),TEXT("Gear Lever")},1,1);
	TestEqual(TEXT("Newer hand first"),P5hLine(N),FString(TEXT("Interacting: Gear Lever · Steering Wheel")));
	N.Sample({TEXT("Steering Wheel"),TEXT("Gear Lever"),TEXT("Console Knob")},1.1,1);
	TestEqual(TEXT("Only newest two"),P5hLine(N),FString(TEXT("Interacting: Console Knob · Gear Lever")));
	N.Sample({TEXT("Steering Wheel")},1.2,1); N.Sample({TEXT("Steering Wheel"),TEXT("Gear Lever")},1.3,1);
	TestEqual(TEXT("Regrab becomes most recent"),P5hLine(N),FString(TEXT("Interacting: Gear Lever · Console Knob")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5hButton,"vBoR.P5h.E10E11FrameButtonAndUnnamed",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5hButton::RunTest(const FString&)
{
	FP5hWorld W; auto* B = W.World->SpawnActor<AVBoRPokeButton>();
	B->SpectatorName = FText::FromString(TEXT("Button"));
	auto* State = NewObject<UP5hPokeState>(B); B->Poke = State;
	auto* Unnamed = W.World->SpawnActor<AVBoRPokeButton>();
	auto* UnnamedState = NewObject<UP5hPokeState>(Unnamed); Unnamed->Poke = UnnamedState;
	auto* S = W.World->GetSubsystem<UVBoRSpectatorSubsystem>();
	State->SetState(EIsdkInteractableState::Hover); S->SampleParts(0);
	TestFalse(TEXT("Hover alone is not pressing"),B->IsSpectatorInteracting());
	TestTrue(TEXT("No hover name"),FP5hTestAccess::Line(S).IsEmpty());
	State->SetState(EIsdkInteractableState::Select); UnnamedState->SetState(EIsdkInteractableState::Select);
	TestTrue(TEXT("Selected state reaches parent reader"),B->IsSpectatorInteracting());
	S->SampleParts(0.016); TestEqual(TEXT("One frame captured without a redraw"),FP5hTestAccess::Line(S),FString(TEXT("Interacting: Button")));
	State->SetState(EIsdkInteractableState::Normal); S->SampleParts(0.032); S->SampleParts(0.532);
	TestEqual(TEXT("Frame press remains readable"),FP5hTestAccess::Line(S),FString(TEXT("Interacting: Button")));
	S->SampleParts(1.032); TestTrue(TEXT("Expires after 1 s; selected unnamed ignored"),FP5hTestAccess::Line(S).IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5hParts,"vBoR.P5h.E8E12ParentInteractionStates",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5hParts::RunTest(const FString&)
{
	FP5hWorld W;
	auto* Axis = W.World->SpawnActor<AP2bTrackedPart>(); Axis->SpectatorName = FText::FromString(TEXT("Glovebox"));
	auto* Panel = W.World->SpawnActor<AP2cTrackedPanel>(); Panel->SpectatorName = FText::FromString(TEXT("Console Box"));
	auto* Wheel = W.World->SpawnActor<AP5dTrackedWheel>(); Wheel->SpectatorName = FText::FromString(TEXT("Steering Wheel"));
	auto* Mirror = W.World->SpawnActor<AP5eTrackedPart>(); Mirror->SpectatorName = FText::FromString(TEXT("Rearview Mirror"));
	auto* Knob = W.World->SpawnActor<AP5fTrackedKnob>(); Knob->SpectatorName = FText::FromString(TEXT("Console Knob"));
	auto* Key = W.World->SpawnActor<AP4aTrackedKey>(); Key->SpectatorName = FText::FromString(TEXT("Key"));
	W.Play();
	const TArray<IVBoRSpectatorPart*> Parents = {Axis,Panel,Wheel,Mirror,Knob,Key};
	for (IVBoRSpectatorPart* Part : Parents)
	{ TestFalse(TEXT("All parents idle initially"),Part->IsSpectatorInteracting()); }
	auto* Pointer = P5hPointer(Axis);
	P5hEvent(Axis->Grabbable,Pointer,EIsdkPointerEventType::Select);
	TestTrue(TEXT("Glovebox axis handle held"),Axis->IsSpectatorInteracting());
	P5hEvent(Axis->Grabbable,Pointer,EIsdkPointerEventType::Unselect);
	TestFalse(TEXT("Axis handle released"),Axis->IsSpectatorInteracting());
	Panel->Hands[0] = P5hHand(FVector(30,0,50));
	P5hEvent(Panel->OutsideGrab,Pointer,EIsdkPointerEventType::Select);
	TestTrue(TEXT("Console box face handle held"),Panel->IsSpectatorInteracting());
	P5hEvent(Panel->OutsideGrab,Pointer,EIsdkPointerEventType::Unselect);
	Panel->Hands[0] = P5hHand(FVector(30,8,50)); Panel->Tick(0.1f);
	Panel->Hands[0] = P5hHand(FVector(30,4,50)); Panel->Tick(0.1f);
	TestTrue(TEXT("Door-style palm push"),Panel->IsSpectatorInteracting());
	Wheel->Hands[0] = FTransform(Wheel->AxisPivot->GetComponentTransform().TransformPosition(FVector(0,0,18)));
	P5hEvent(Wheel->Grabbable,Pointer,EIsdkPointerEventType::Select);
	TestTrue(TEXT("Wheel one hand held"),Wheel->IsSpectatorInteracting());
	Wheel->Hands[1] = FTransform(Wheel->AxisPivot->GetComponentTransform().TransformPosition(FVector(0,0,-18)));
	auto* Right = P5hPointer(Wheel,1); P5hEvent(Wheel->Grabbable,Right,EIsdkPointerEventType::Select);
	P5hEvent(Wheel->Grabbable,Pointer,EIsdkPointerEventType::Unselect);
	TestTrue(TEXT("Wheel second hand retains use"),Wheel->IsSpectatorInteracting());
	P5hEvent(Wheel->Grabbable,Right,EIsdkPointerEventType::Unselect);
	TestFalse(TEXT("Wheel both hands released"),Wheel->IsSpectatorInteracting());
	Mirror->Hands[0] = P5hHand(FVector(-5,10,0)); Mirror->Tick(0.016f);
	Mirror->Hands[0] = P5hHand(FVector(-2,10,0)); Mirror->Tick(0.016f);
	TestTrue(TEXT("Rearview mirror edge push"),Mirror->IsSpectatorInteracting());
	Mirror->Hands[0] = {}; Mirror->Tick(0.016f);
	P5hEvent(Mirror->Grabbable,Pointer,EIsdkPointerEventType::Select);
	TestTrue(TEXT("Rearview mirror held"),Mirror->IsSpectatorInteracting());
	P5hEvent(Mirror->Grabbable,Pointer,EIsdkPointerEventType::Unselect);
	P5hEvent(Knob->Grabbable,Pointer,EIsdkPointerEventType::Select);
	TestTrue(TEXT("Knob held"),Knob->IsSpectatorInteracting());
	P5hEvent(Knob->Grabbable,Pointer,EIsdkPointerEventType::Unselect);
	TestFalse(TEXT("Knob released"),Knob->IsSpectatorInteracting());
	FIsdkInteractionPointerEvent KeyEvent; KeyEvent.Interactable = Key->Grabbable.Get(); KeyEvent.Interactor = Pointer; KeyEvent.Type = EIsdkPointerEventType::Select;
	Key->Grabbable->PostEvent(KeyEvent); TestTrue(TEXT("Key held"),Key->IsSpectatorInteracting());
	KeyEvent.Type = EIsdkPointerEventType::Unselect; Key->Grabbable->PostEvent(KeyEvent);
	TestFalse(TEXT("Key released"),Key->IsSpectatorInteracting());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5hMockNames,"vBoR.P5h.E13MockNames",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5hMockNames::RunTest(const FString&)
{
	FP5hWorld W; auto* Car = W.World->SpawnActor<AP3VehicleMock>(); W.Play();
	const TPair<IVBoRSpectatorPart*,const TCHAR*> Expected[] = {
		{Car->Door,TEXT("Driver Door")},{Car->OutsideAxisHandle,TEXT("Driver Door")},{Car->InsideAxisHandle,TEXT("Driver Door")},
		{Car->ConsoleLid,TEXT("Console Box")},{Car->Glovebox,TEXT("Glovebox")},{Car->GloveboxHandle,TEXT("Glovebox")},
		{Car->GearLever,TEXT("Gear Lever")},{Car->GearButton,TEXT("Gear Button")},{Car->TurnSignalLever,TEXT("Turn Signal Lever")},
		{Car->SteeringWheel,TEXT("Steering Wheel")},{Car->RearviewMirror,TEXT("Rearview Mirror")},{Car->ConsoleKnob,TEXT("Console Knob")},
		{Car->ReachButton,TEXT("Button")},{Car->FrontKeyTable->Key,TEXT("Key")},{Car->RearKeyTable->Key,TEXT("Key")}};
	for (const auto& Entry : Expected)
	{ TestEqual(Entry.Value,Entry.Key->GetSpectatorName().ToString(),FString(Entry.Value)); }
	TestTrue(TEXT("Unnamed default button"),GetDefault<AVBoRPokeButton>()->GetSpectatorName().IsEmpty());
	TestTrue(TEXT("Unnamed default axis"),GetDefault<AVBoRAxisPart>()->GetSpectatorName().IsEmpty());
	TestTrue(TEXT("Key child buttons unnamed"),Car->FrontKeyTable->Key->UnlockButton->SpectatorName.IsEmpty() && Car->RearKeyTable->Key->LockButton->SpectatorName.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5hRects,"vBoR.P5h.E15FourWindowRatios",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5hRects::RunTest(const FString&)
{
	const FIntPoint Sizes[] = {FIntPoint(1920,1080),FIntPoint(1440,1080),FIntPoint(2520,1080),FIntPoint(1080,1080)};
	const double MinX[] = {0.2075,0.11,0.277142857,0};
	const double MaxY[] = {0.78,0.78,0.78,0.75};
	for (int32 I = 0; I < UE_ARRAY_COUNT(Sizes); ++I)
	{
		FVBoRSpectatorLayout L; TestTrue(TEXT("Initial layout"),L.Update(Sizes[I],4.f/3,0.22f));
		TestTrue(TEXT("Valid engine layout"),L.Layout.IsValid());
		TestTrue(TEXT("Expected min X"),FMath::IsNearlyEqual(L.Layout.EyeRectMin.X,MinX[I],0.0001));
		TestTrue(TEXT("Centered max X"),FMath::IsNearlyEqual(L.Layout.EyeRectMax.X,1-MinX[I],0.0001));
		TestEqual(TEXT("Eye starts at top"),L.Layout.EyeRectMin.Y,0.0);
		TestTrue(TEXT("Expected height"),FMath::IsNearlyEqual(L.Layout.EyeRectMax.Y,MaxY[I],0.0001));
		TestTrue(TEXT("4:3 in pixels"),FMath::IsNearlyEqual((L.Layout.EyeRectMax.X-L.Layout.EyeRectMin.X)*Sizes[I].X/(L.Layout.EyeRectMax.Y*Sizes[I].Y),4.0/3,0.0001));
		TestTrue(TEXT("Transparent UI over black surround"),L.Layout.bUseAlpha && L.Layout.bDrawEyeFirst && L.Layout.bClearBlack);
		TestTrue(TEXT("Texture spans whole screen"),L.Layout.TextureRectMin.IsZero() && L.Layout.TextureRectMax == FVector2D(1,1));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5hResize,"vBoR.P5h.E16ResizeUpdates",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5hResize::RunTest(const FString&)
{
	FVBoRSpectatorLayout L;
	TestTrue(TEXT("Initial placement submitted"),L.Update(FIntPoint(1920,1080),4.f/3,0.22f));
	TestFalse(TEXT("Unchanged window not resubmitted"),L.Update(FIntPoint(1920,1080),4.f/3,0.22f));
	TestTrue(TEXT("TV resize resubmitted"),L.Update(FIntPoint(2520,1080),4.f/3,0.22f));
	TestFalse(TEXT("New size retained"),L.Update(FIntPoint(2520,1080),4.f/3,0.22f));
	TestFalse(TEXT("Minimized window ignored"),L.Update(FIntPoint::ZeroValue,4.f/3,0.22f));
	TestTrue(TEXT("Editable eye ratio resubmitted"),L.Update(FIntPoint(2520,1080),1.5f,0.22f));
	TestTrue(TEXT("Editable band resubmitted"),L.Update(FIntPoint(2520,1080),1.5f,0.3f));
	TestTrue(TEXT("Changed layout valid"),L.Layout.IsValid());
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5hBandFit,"vBoR.P5h.E17MeasuredBandFit",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5hBandFit::RunTest(const FString&)
{
	if (!TestTrue(TEXT("Slate font services available under nullrhi"),FSlateApplication::IsInitialized())) { return false; }
	const auto Measure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
	FP5hWorld W; W.World->SpawnActor<AP3VehicleMock>(); W.Play();
	auto* S = W.World->GetSubsystem<UVBoRSpectatorSubsystem>();
	TArray<FText> Missions = {S->CompletedMission,S->ExploreMission,S->UnknownMission};
	for (int32 I = 1; I <= 20; ++I)
	{
		if (I == 17) { continue; }
		FVBoRScenarioStep Step; Step.StepId = FName(*FString::Printf(TEXT("%02d"),I));
		Missions.Add(VBoRSpectator::MissionText(Step,S->UnknownMission));
	}
	// 근사 차에서 이름을 모아 가장 넓은 두 개를 고름 (이름 변경도 시험에 반영, P5h E17)
	TArray<FString> PartNames;
	for (TActorIterator<AActor> It(W.World); It; ++It)
	{
		if (const auto* Part = Cast<IVBoRSpectatorPart>(*It))
		{ if (!Part->GetSpectatorName().IsEmpty()) { PartNames.AddUnique(Part->GetSpectatorName().ToString()); } }
	}
	const auto Probe = FP5hTestAccess::Widget(S,Missions[0],TEXT("Probe"));
	Probe->SlatePrepass(1);
	TArray<FArrangedWidget> ProbeTexts, ProbeBorders;
	P5hArrange(Probe,FGeometry::MakeRoot(FP5hTestAccess::Size(),FSlateLayoutTransform(1.f)),ProbeTexts,ProbeBorders);
	if (!TestEqual(TEXT("Label, parts, mission, progress text widgets"),ProbeTexts.Num(),4)) { return false; }
	const auto LabelFont = StaticCastSharedRef<STextBlock>(ProbeTexts[0].Widget)->GetFont();
	FText LongestLabel = FText::Format(S->MissionLabel,14,14);
	for (const FText& Label : {S->ReadyLabel,S->FreeLabel})
	{ if (Measure->Measure(Label,LabelFont).X > Measure->Measure(LongestLabel,LabelFont).X) { LongestLabel = Label; } }
	const auto PartFont = StaticCastSharedRef<STextBlock>(ProbeTexts[1].Widget)->GetFont();
	PartNames.Sort([&](const FString& A, const FString& B) { return Measure->Measure(A,PartFont).X > Measure->Measure(B,PartFont).X; });
	if (!TestTrue(TEXT("Mock provides at least two named parts"),PartNames.Num() >= 2)) { return false; }
	const FString LongestParts = S->InteractingPrefix.ToString()+PartNames[0]+TEXT(" · ")+PartNames[1];
	// 폭을 넘는 에셋 문구도 같은 위젯에 넣음 (말줄임 때 줄 높이·막대 위치 유지, P5h R1-2)
	const int32 DefaultCount = Missions.Num();
	Missions.Add(FText::FromString(FString::ChrN(400,TEXT('W'))));
	for (int32 I = 0; I < Missions.Num(); ++I)
	{
		const auto Widget = FP5hTestAccess::Widget(S,Missions[I],LongestParts,LongestLabel);
		Widget->SlatePrepass(1);
		TArray<FArrangedWidget> Texts, Borders;
		P5hArrange(Widget,FGeometry::MakeRoot(FP5hTestAccess::Size(),FSlateLayoutTransform(1.f)),Texts,Borders);
		if (!TestEqual(TEXT("Three rows with four text widgets"),Texts.Num(),4) || !TestEqual(TEXT("Band border"),Borders.Num(),1)) { return false; }
		for (int32 T = 0; T < Texts.Num(); ++T)
		{
			const auto Text = StaticCastSharedRef<STextBlock>(Texts[T].Widget);
			const FVector2D Measured = Measure->Measure(Text->GetText(),Text->GetFont());
			if (I < DefaultCount || T != 2)
			{ TestTrue(*FString::Printf(TEXT("Text fits allotted width: %s"),*Text->GetText().ToString()),Measured.X <= Texts[T].Geometry.GetLocalSize().X); }
			else { TestTrue(TEXT("Long asset text exceeds width and needs ellipsis"),Measured.X > Texts[T].Geometry.GetLocalSize().X); }
			TestTrue(TEXT("Measured font height fits row"),Measure->GetMaxCharacterHeight(Text->GetFont()) <= Texts[T].Geometry.GetLocalSize().Y);
		}
		const auto Top = [&](int32 T) { return Texts[T].Geometry.GetAbsolutePosition().Y; };
		const auto Bottom = [&](int32 T) { return Top(T)+Texts[T].Geometry.GetLocalSize().Y; };
		TestTrue(TEXT("Parts share label row"),Top(1) >= Top(0) && Bottom(1) <= Bottom(0));
		TestTrue(TEXT("Header ends before mission"),Bottom(0) <= Top(2));
		TestTrue(TEXT("Single mission row ends before progress"),Bottom(2) <= Top(3));
		TestTrue(TEXT("Three desired row heights plus widget padding fit band"),Borders[0].Widget->GetDesiredSize().Y <= Borders[0].Geometry.GetLocalSize().Y);
		TestTrue(TEXT("Progress stays inside band"),Bottom(3) <= Borders[0].Geometry.GetAbsolutePosition().Y+Borders[0].Geometry.GetLocalSize().Y);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5hConfig,"vBoR.P5h.E18ConfigDefaults",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5hConfig::RunTest(const FString&)
{
	UClass* Class = UVBoRSpectatorSubsystem::StaticClass();
	TestTrue(TEXT("Game config class"),Class->HasAnyClassFlags(CLASS_Config));
	TestEqual(TEXT("DefaultGame.ini config category"),Class->ClassConfigName,FName(TEXT("Game")));
	const FName Properties[] = {TEXT("RedrawsPerSecond"),TEXT("EyeAspect"),TEXT("BandHeight"),TEXT("NameHoldSeconds"),
		TEXT("ReadyLabel"),TEXT("MissionLabel"),TEXT("FreeLabel"),TEXT("CompletedMission"),TEXT("ExploreMission"),TEXT("UnknownMission"),TEXT("InteractingPrefix"),
		TEXT("BackgroundColor"),TEXT("TextColor"),TEXT("AccentColor"),TEXT("LabelFontSize"),TEXT("MissionFontSize"),TEXT("PartFontSize")};
	for (FName Name : Properties)
	{
		const FProperty* Property = Class->FindPropertyByName(Name);
		TestTrue(*FString::Printf(TEXT("Config property %s"),*Name.ToString()),Property && Property->HasAnyPropertyFlags(CPF_Config));
	}
	auto* Defaults = GetMutableDefault<UVBoRSpectatorSubsystem>();
	// ini 를 쓰지 않고 CDO 상속만 확인함. 월드 정리 뒤 원래 값 복구 (P5h R1-1)
	TGuardValue<float> Aspect(Defaults->EyeAspect,1.5f), Band(Defaults->BandHeight,0.25f);
	FP5hWorld W;
	const auto* S = W.World->GetSubsystem<UVBoRSpectatorSubsystem>();
	if (TestNotNull(TEXT("New world subsystem"),S))
	{
		TestEqual(TEXT("World inherits configured eye ratio"),S->EyeAspect,1.5f);
		TestEqual(TEXT("World inherits configured band height"),S->BandHeight,0.25f);
	}
	return true;
}
#endif
