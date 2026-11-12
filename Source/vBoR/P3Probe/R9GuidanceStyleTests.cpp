#include "R9TestWidgets.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"

bool UR9TestGuidanceWidget::Initialize()
{
	if (!Super::Initialize()) { return false; }
	Plate = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Plate"));
	MainText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("MainText"));
	Header = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Header"));
	AccentBar = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("AccentBar"));
	auto* Column = WidgetTree->ConstructWidget<UVerticalBox>();
	Column->AddChildToVerticalBox(AccentBar);
	Column->AddChildToVerticalBox(Header);
	Column->AddChildToVerticalBox(MainText);
	Plate->SetContent(Column);
	WidgetTree->RootWidget = Plate;
	return true;
}

bool UR9TestButtonLabelWidget::Initialize()
{
	if (!Super::Initialize()) { return false; }
	Plate = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Plate"));
	LabelText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("LabelText"));
	Plate->SetContent(LabelText);
	WidgetTree->RootWidget = Plate;
	return true;
}

bool UR9TestMissingPlateWidget::Initialize()
{
	if (!Super::Initialize()) { return false; }
	MainText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("MainText"));
	WidgetTree->RootWidget = MainText;
	return true;
}

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "P3VehicleMock.h"
#include "../Interaction/VBoRPokeButton.h"
#include "../Interaction/VBoRScenarioRunner.h"
#include "../Interaction/VBoRVehicleAlignmentComponent.h"
#include "Components/WidgetComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/WorldSettings.h"
#include "Styling/CoreStyle.h"
#include "UObject/UnrealType.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"

struct FR9TestAccess
{
	static const auto& SlateText(const AVBoRScenarioRunner& Runner) { return Runner.PanelText; }
	static const auto& SlateBorder(const AVBoRScenarioRunner& Runner) { return Runner.PanelBorder; }
	static UWidgetComponent* Label(const AVBoRScenarioRunner& Runner, int32 Index) { return Runner.ButtonLabels[Index]; }
	static const AVBoRPokeButton* Button(const AVBoRScenarioRunner& Runner, int32 Index)
	{
		return Index == 0 ? Runner.ConfirmButton.Get() : Runner.ActionButton.Get();
	}
};

namespace
{
	constexpr auto R9Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;

	FVBoRScenarioStep R9Step(FName Id, const TCHAR* Instruction)
	{
		FVBoRScenarioStep Step;
		Step.StepId = Id;
		Step.Instruction = FText::FromString(Instruction);
		Step.Rule = EVBoRStepRule::Confirm;
		Step.ConfirmBy = EVBoRConfirmBy::Either;
		Step.PanelTag = TEXT("Guide.Outside");
		return Step;
	}

	struct FR9World
	{
		UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
		AVBoRScenarioRunner* Runner;
		AP3VehicleMock* Car;
		APawn* Pawn;

		FR9World()
		{
			GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
			auto* Controller = World->SpawnActor<APlayerController>();
			Pawn = World->SpawnActor<APawn>();
			Pawn->SetRootComponent(NewObject<USceneComponent>(Pawn));
			Pawn->GetRootComponent()->RegisterComponent();
			Controller->Possess(Pawn);
			Car = World->SpawnActor<AP3VehicleMock>();
			Runner = World->SpawnActor<AVBoRScenarioRunner>();
			Runner->Scenario = NewObject<UVBoRScenarioAsset>(Runner);
		}

		~FR9World()
		{
			World->EndPlay(EEndPlayReason::Quit);
			GEngine->DestroyWorldContext(World);
			World->DestroyWorld(false);
		}

		bool Start(std::initializer_list<FVBoRScenarioStep> Steps)
		{
			Runner->Scenario->Steps = Steps;
			World->InitializeActorsForPlay(FURL());
			World->GetWorldSettings()->NotifyBeginPlay();
			FVBoRBuckSavedAlignment Saved;
			Saved.SavedAt = TEXT("test");
			Saved.TrackingOrigin = 3;
			Saved.PlayAreaCm = FVector2D(300, 400);
			Saved.Capture.PositionCm = FVector(10, 20, 115);
			Saved.Capture.YawDeg = 30;
			Car->Alignment->InitializeAlignment(Pawn, &Saved, 3, Saved.PlayAreaCm);
			return Runner->InitializeFlow();
		}

		void Command(EVBoRFlowCommand Action)
		{
			Runner->QueueCommand({Action, EVBoRCommandSource::Operator, Runner->GetToken()});
			Runner->ProcessFlow(0);
		}
	};

	float R9Luminance(const FLinearColor& Color)
	{
		return 0.2126f * Color.R + 0.7152f * Color.G + 0.0722f * Color.B;
	}

	float R9Contrast(const FLinearColor& A, const FLinearColor& B)
	{
		const float First = R9Luminance(A);
		const float Second = R9Luminance(B);
		return (FMath::Max(First, Second) + 0.05f) / (FMath::Min(First, Second) + 0.05f);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FR9SlateFallbackTest, "vBoR.R9.E1", R9Flags)
bool FR9SlateFallbackTest::RunTest(const FString&)
{
	FR9World E;
	TestTrue(TEXT("Flow starts with empty widget class fields"), E.Start({R9Step(TEXT("01"), TEXT("Observe."))}));
	const auto& Text = FR9TestAccess::SlateText(*E.Runner);
	const auto& Border = FR9TestAccess::SlateBorder(*E.Runner);
	if (!TestTrue(TEXT("Slate text and plate exist"), Text.IsValid() && Border.IsValid())) { return false; }
	TestEqual(TEXT("Black text"), Text->GetColorAndOpacity().GetSpecifiedColor(), FLinearColor::Black);
	TestEqual(TEXT("Font size"), Text->GetFont().Size, 40.0f);
	TestTrue(TEXT("White brush"), Border->GetBorderImage() == FCoreStyle::Get().GetBrush("WhiteBrush"));
	TestEqual(TEXT("Light opaque background"), Border->GetBorderBackgroundColor().GetSpecifiedColor(), FLinearColor(0.95, 0.95, 0.95, 1));
	TestNull(TEXT("No UMG guidance"), E.Runner->Panel->GetUserWidgetObject());
	TestNull(TEXT("No UMG button label"), FR9TestAccess::Label(*E.Runner, 0)->GetUserWidgetObject());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FR9InstructionTest, "vBoR.R9.E2", R9Flags)
bool FR9InstructionTest::RunTest(const FString&)
{
	FR9World E;
	E.Runner->GuidanceWidgetClass = UR9TestGuidanceWidget::StaticClass();
	TestTrue(TEXT("Flow starts with guidance widget"), E.Start({R9Step(TEXT("01"), TEXT("Observe.")), R9Step(TEXT("02"), TEXT("Look inside."))}));
	const auto* Widget = Cast<UR9TestGuidanceWidget>(E.Runner->Panel->GetUserWidgetObject());
	if (!TestNotNull(TEXT("Guidance widget attached"), Widget)) { return false; }
	const auto* Main = CastChecked<UTextBlock>(Widget->GetWidgetFromName(TEXT("MainText")));
	TestNull(TEXT("Independent label fallback"), FR9TestAccess::Label(*E.Runner, 0)->GetUserWidgetObject());
	TestFalse(TEXT("First instruction visible"), E.Runner->Panel->bHiddenInGame);
	TestEqual(TEXT("Initial instruction"), Main->GetText().ToString(), E.Runner->GetPanelText());
	E.Command(EVBoRFlowCommand::Next);
	TestEqual(TEXT("Next instruction"), Main->GetText().ToString(), FString(TEXT("Look inside.")));
	TestEqual(TEXT("Next instruction matches runner"), Main->GetText().ToString(), E.Runner->GetPanelText());
	E.Command(EVBoRFlowCommand::Next);
	TestTrue(TEXT("Finish enters free mode"), E.Runner->IsFreeMode());
	TestTrue(TEXT("Finished panel hidden"), E.Runner->Panel->bHiddenInGame);
	TestTrue(TEXT("Finished instruction is empty"), Main->GetText().IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FR9ButtonLabelTest, "vBoR.R9.E3", R9Flags)
bool FR9ButtonLabelTest::RunTest(const FString&)
{
	FR9World E;
	E.Runner->ButtonLabelWidgetClass = UR9TestButtonLabelWidget::StaticClass();
	auto ViewStep = R9Step(TEXT("14"), TEXT("Move to the passenger seat."));
	ViewStep.Rule = EVBoRStepRule::Viewpoint;
	ViewStep.TargetTag = TEXT("Car.PassengerView");
	TestTrue(TEXT("Flow starts with labels only"), E.Start({R9Step(TEXT("12"), TEXT("Observe.")), ViewStep}));
	E.Runner->ProcessFlow(E.Runner->ConfirmLockSeconds + 0.01);
	TestTrue(TEXT("Independent guidance fallback"), FR9TestAccess::SlateText(*E.Runner).IsValid());
	auto* Confirm = FR9TestAccess::Label(*E.Runner, 0);
	auto* Action = FR9TestAccess::Label(*E.Runner, 1);
	const auto* ConfirmWidget = Cast<UR9TestButtonLabelWidget>(Confirm->GetUserWidgetObject());
	const auto* ActionWidget = Cast<UR9TestButtonLabelWidget>(Action->GetUserWidgetObject());
	if (!TestNotNull(TEXT("Confirm widget"), ConfirmWidget) || !TestNotNull(TEXT("Action widget"), ActionWidget)) { return false; }
	const auto* ConfirmText = CastChecked<UTextBlock>(ConfirmWidget->GetWidgetFromName(TEXT("LabelText")));
	const auto* ActionText = CastChecked<UTextBlock>(ActionWidget->GetWidgetFromName(TEXT("LabelText")));
	TestEqual(TEXT("Confirm text"), ConfirmText->GetText().ToString(), FString(TEXT("Confirm")));
	TestEqual(TEXT("Confirm emphasis color"), ConfirmText->GetColorAndOpacity().GetSpecifiedColor(), E.Runner->PanelStyle.ConfirmTextColor);
	TestEqual(TEXT("Action normal color"), ActionText->GetColorAndOpacity().GetSpecifiedColor(), E.Runner->PanelStyle.LabelTextColor);
	const auto* ConfirmPlate = CastChecked<UBorder>(ConfirmWidget->GetWidgetFromName(TEXT("Plate")));
	TestEqual(TEXT("Graphite emphasis outline"), ConfirmPlate->Background.OutlineSettings.Width, 3.0f);
	TestFalse(TEXT("Confirm label visible"), Confirm->bHiddenInGame);
	TestTrue(TEXT("Action label hidden"), Action->bHiddenInGame);
	E.Command(EVBoRFlowCommand::Next);
	TestEqual(TEXT("Passenger action text"), ActionText->GetText().ToString(), FString(TEXT("Move to passenger seat")));
	TestFalse(TEXT("Passenger action label visible"), Action->bHiddenInGame);
	for (int32 Index = 0; Index < 2; ++Index)
	{
		TestEqual(TEXT("Label visibility follows button"), FR9TestAccess::Label(*E.Runner, Index)->bHiddenInGame,
			FR9TestAccess::Button(*E.Runner, Index)->IsHidden());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FR9NavyStyleTest, "vBoR.R9.E4", R9Flags)
bool FR9NavyStyleTest::RunTest(const FString&)
{
	FR9World E;
	E.Runner->GuidanceWidgetClass = UR9TestGuidanceWidget::StaticClass();
	E.Runner->ButtonLabelWidgetClass = UR9TestButtonLabelWidget::StaticClass();
	E.Runner->PanelStyle = FVBoRPanelStyle::FromPreset(EVBoRPanelStylePreset::NavyInstrument);
	TestTrue(TEXT("Flow starts with both widgets"), E.Start({R9Step(TEXT("01"), TEXT("Observe."))}));
	const auto* Widget = Cast<UR9TestGuidanceWidget>(E.Runner->Panel->GetUserWidgetObject());
	const auto* Confirm = Cast<UR9TestButtonLabelWidget>(FR9TestAccess::Label(*E.Runner, 0)->GetUserWidgetObject());
	if (!TestNotNull(TEXT("Guidance widget"), Widget) || !TestNotNull(TEXT("Confirm widget"), Confirm)) { return false; }
	const auto& Brush = CastChecked<UBorder>(Widget->GetWidgetFromName(TEXT("Plate")))->Background;
	const auto* Text = CastChecked<UTextBlock>(Widget->GetWidgetFromName(TEXT("MainText")));
	const auto* Header = CastChecked<UTextBlock>(Widget->GetWidgetFromName(TEXT("Header")));
	TestEqual(TEXT("Rounded box"), Brush.DrawAs.GetValue(), ESlateBrushDrawType::RoundedBox);
	TestEqual(TEXT("Navy fill"), Brush.GetTint(FWidgetStyle()).ToFColorSRGB(), FColor(0x18, 0x21, 0x2B));
	TestEqual(TEXT("Corner radius"), Brush.OutlineSettings.CornerRadii, FVector4(20, 20, 20, 20));
	TestEqual(TEXT("Outline width"), Brush.OutlineSettings.Width, 2.0f);
	TestEqual(TEXT("Outline color"), Brush.OutlineSettings.Color.GetSpecifiedColor().ToFColorSRGB(), FColor(0x2A, 0x36, 0x44));
	TestEqual(TEXT("Navy text"), Text->GetColorAndOpacity().GetSpecifiedColor().ToFColorSRGB(), FColor(0xDC, 0xE3, 0xEA));
	TestTrue(TEXT("Header visible"), Header->IsVisible());
	TestEqual(TEXT("Header text"), Header->GetText().ToString(), FString(TEXT("GUIDANCE")));
	TestFalse(TEXT("Accent hidden"), Widget->GetWidgetFromName(TEXT("AccentBar"))->IsVisible());
	const auto& ConfirmBrush = CastChecked<UBorder>(Confirm->GetWidgetFromName(TEXT("Plate")))->Background;
	TestEqual(TEXT("Navy confirm fill"), ConfirmBrush.GetTint(FWidgetStyle()).ToFColorSRGB(), FColor(0xC9, 0x9A, 0x4B));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FR9PresetEditTest, "vBoR.R9.E5", R9Flags)
bool FR9PresetEditTest::RunTest(const FString&)
{
	FR9World E;
	auto* Preset = FindFProperty<FProperty>(AVBoRScenarioRunner::StaticClass(), GET_MEMBER_NAME_CHECKED(AVBoRScenarioRunner, PanelStylePreset));
	auto* Style = FindFProperty<FProperty>(AVBoRScenarioRunner::StaticClass(), GET_MEMBER_NAME_CHECKED(AVBoRScenarioRunner, PanelStyle));
	auto* TextColor = FindFProperty<FProperty>(FVBoRPanelStyle::StaticStruct(), GET_MEMBER_NAME_CHECKED(FVBoRPanelStyle, TextColor));
	TestEqual(TEXT("Default preset"), E.Runner->PanelStylePreset, EVBoRPanelStylePreset::Graphite);
	TestEqual(TEXT("Default Graphite fill"), E.Runner->PanelStyle.PlateColor.ToFColorSRGB(), FColor(0x1E, 0x22, 0x26));
	E.Runner->PanelStylePreset = EVBoRPanelStylePreset::SoftLight;
	FPropertyChangedEvent PresetEvent(Preset, EPropertyChangeType::ValueSet);
	E.Runner->PostEditChangeProperty(PresetEvent);
	TestEqual(TEXT("Soft Light fill"), E.Runner->PanelStyle.PlateColor.ToFColorSRGB(), FColor(0xD3, 0xD1, 0xCC));
	TestEqual(TEXT("Soft Light corner radius"), E.Runner->PanelStyle.CornerRadius, 24.0f);
	TestEqual(TEXT("Soft Light accent width"), E.Runner->PanelStyle.AccentBarWidth, 96.0f);
	TestEqual(TEXT("Soft Light accent height"), E.Runner->PanelStyle.AccentBarHeight, 6.0f);
	const FLinearColor EditedColor(0.2, 0.3, 0.4);
	E.Runner->PanelStyle.TextColor = EditedColor;
	FPropertyChangedEvent StyleEvent(TextColor, EPropertyChangeType::ValueSet);
	StyleEvent.SetActiveMemberProperty(Style);
	E.Runner->PostEditChangeProperty(StyleEvent);
	TestEqual(TEXT("Manual edit selects Custom"), E.Runner->PanelStylePreset, EVBoRPanelStylePreset::Custom);
	TestEqual(TEXT("Manual edit retained"), E.Runner->PanelStyle.TextColor, EditedColor);
	E.Runner->PanelStylePreset = EVBoRPanelStylePreset::Custom;
	E.Runner->PostEditChangeProperty(PresetEvent);
	TestEqual(TEXT("Selecting Custom preserves edit"), E.Runner->PanelStyle.TextColor, EditedColor);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FR9ContrastTest, "vBoR.R9.E6", R9Flags)
bool FR9ContrastTest::RunTest(const FString&)
{
	const float MinDark = R9Luminance(FLinearColor::FromSRGBColor(FColor(0x1A, 0x1A, 0x1A)));
	const float MaxLight = R9Luminance(FLinearColor::FromSRGBColor(FColor(0xDA, 0xDA, 0xDA)));
	for (auto Preset : {EVBoRPanelStylePreset::Graphite, EVBoRPanelStylePreset::NavyInstrument, EVBoRPanelStylePreset::SoftLight})
	{
		const auto Style = FVBoRPanelStyle::FromPreset(Preset);
		const auto ConfirmFill = Style.bFillConfirmLabel ? Style.AccentColor : Style.PlateColor;
		TestTrue(TEXT("Instruction contrast >= 4.5"), R9Contrast(Style.TextColor, Style.PlateColor) >= 4.5f);
		TestTrue(TEXT("Header contrast >= 3"), R9Contrast(Style.HeaderColor, Style.PlateColor) >= 3.0f);
		TestTrue(TEXT("Confirm contrast >= 4.5"), R9Contrast(Style.ConfirmTextColor, ConfirmFill) >= 4.5f);
		TestTrue(TEXT("Action contrast >= 4.5"), R9Contrast(Style.LabelTextColor, Style.LabelPlateColor) >= 4.5f);
		const float Plate = R9Luminance(Style.PlateColor);
		TestTrue(TEXT("Plate luminance bound"), Preset == EVBoRPanelStylePreset::SoftLight ? Plate <= MaxLight : Plate >= MinDark);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FR9MissingBindingTest, "vBoR.R9.E7", R9Flags)
bool FR9MissingBindingTest::RunTest(const FString&)
{
	FR9World E;
	E.Runner->GuidanceWidgetClass = UR9TestMissingPlateWidget::StaticClass();
	AddExpectedError(TEXT("Guidance widget R9TestMissingPlateWidget is missing Plate; using the Slate plate"), EAutomationExpectedErrorFlags::Contains, 1);
	TestTrue(TEXT("Flow starts after failed binding"), E.Start({R9Step(TEXT("01"), TEXT("Observe."))}));
	TestNull(TEXT("Incomplete widget is not attached"), E.Runner->Panel->GetUserWidgetObject());
	TestTrue(TEXT("Slate plate is restored"), FR9TestAccess::SlateBorder(*E.Runner).IsValid());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FR9WidgetLifetimeTest, "vBoR.R9.E8", R9Flags)
bool FR9WidgetLifetimeTest::RunTest(const FString&)
{
	FR9World E;
	E.Runner->GuidanceWidgetClass = UR9TestGuidanceWidget::StaticClass();
	E.Runner->ButtonLabelWidgetClass = UR9TestButtonLabelWidget::StaticClass();
	TestTrue(TEXT("Flow starts"), E.Start({R9Step(TEXT("01"), TEXT("Observe."))}));
	auto* Panel = E.Runner->Panel.Get();
	auto* Confirm = FR9TestAccess::Label(*E.Runner, 0);
	auto* Action = FR9TestAccess::Label(*E.Runner, 1);
	const auto* InitialWidget = Panel->GetUserWidgetObject();
	if (!TestNotNull(TEXT("Initial guidance widget"), InitialWidget)) { return false; }
	TestTrue(TEXT("Restart key accepted"), E.Runner->HandleOperatorKey(EKeys::R));
	E.Runner->ProcessFlow(0);
	TestEqual(TEXT("Restart applied"), E.Runner->RetryCount, 1);
	TestTrue(TEXT("Restart reuses the attached widget"), Panel->GetUserWidgetObject() == InitialWidget);
	E.Runner->Destroy();
	TestNull(TEXT("EndPlay releases guidance"), Panel->GetUserWidgetObject());
	TestNull(TEXT("EndPlay releases Confirm"), Confirm->GetUserWidgetObject());
	TestNull(TEXT("EndPlay releases action label"), Action->GetUserWidgetObject());
	return true;
}
#endif
