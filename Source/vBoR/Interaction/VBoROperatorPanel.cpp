#include "VBoROperatorPanel.h"
#include "VBoRScenarioRunner.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

const TArray<FVBoROperatorShortcut>& VBoROperator::Shortcuts()
{
	static const TArray<FVBoROperatorShortcut> Values = {
		{EVBoROperatorButton::Confirm, EKeys::SpaceBar, TEXT("Confirm"), TEXT("Space")},
		{EVBoROperatorButton::Next, EKeys::Right, TEXT("Next step"), TEXT("→")},
		{EVBoROperatorButton::Previous, EKeys::Left, TEXT("Previous step"), TEXT("←")},
		{EVBoROperatorButton::Restart, EKeys::R, TEXT("Restart from step 1"), TEXT("R")},
		{EVBoROperatorButton::Ignore, EKeys::O, TEXT("Start without alignment"), TEXT("O")},
		{EVBoROperatorButton::Driver, EKeys::Home, TEXT("Driver seat"), TEXT("Home")},
		{EVBoROperatorButton::Passenger, EKeys::One, TEXT("Passenger seat"), TEXT("1")},
		{EVBoROperatorButton::Rear, EKeys::Two, TEXT("Rear seat"), TEXT("2")},
		{EVBoROperatorButton::Reapply, EKeys::A, TEXT("Re-apply saved alignment"), TEXT("A")},
		{EVBoROperatorButton::Info, EKeys::I, TEXT("In-VR operator info"), TEXT("I")},
		{EVBoROperatorButton::Free, EKeys::End, TEXT("Enter free mode"), TEXT("End")},
		{EVBoROperatorButton::Reload, EKeys::F6, TEXT("Reload level"), TEXT("F6")}};
	return Values;
}

const FVBoROperatorShortcut& VBoROperator::Shortcut(EVBoROperatorButton Button) { return Shortcuts()[static_cast<int32>(Button)]; }
FLinearColor VBoROperator::Color(const TCHAR* Hex) { return FLinearColor::FromSRGBColor(FColor::FromHex(Hex)); }

FLinearColor VBoROperator::TextColor(EVBoROperatorStyle Style)
{
	return Color(Style == EVBoROperatorStyle::Primary ? TEXT("0b0f17") : Style == EVBoROperatorStyle::Disabled ? TEXT("66748a") : Style == EVBoROperatorStyle::Danger ? TEXT("ff7a72") : TEXT("eef2f7"));
}

FButtonStyle VBoROperator::ButtonStyle(EVBoROperatorStyle Style)
{
	const FLinearColor Fill = Color(Style == EVBoROperatorStyle::Primary ? TEXT("5b8cff") : Style == EVBoROperatorStyle::Disabled ? TEXT("0b0f17") : Style == EVBoROperatorStyle::Current ? TEXT("1c2534") : TEXT("131a26"));
	const FLinearColor Outline = Color(Style == EVBoROperatorStyle::Primary || Style == EVBoROperatorStyle::Current ? TEXT("5b8cff") : Style == EVBoROperatorStyle::Disabled ? TEXT("2e3a4e") : Style == EVBoROperatorStyle::Danger ? TEXT("ff7a72") : TEXT("66748a"));
	const FSlateRoundedBoxBrush Brush(Fill, 6.f, Outline, 1.f);
	return FButtonStyle().SetNormal(Brush).SetHovered(Brush).SetPressed(Brush).SetDisabled(Brush)
		.SetNormalPadding(FMargin(16, 0, 12, 0)).SetPressedPadding(FMargin(16, 0, 12, 0));
}

FVBoROperatorPlacement& FVBoROperatorPlacement::Session() { static FVBoROperatorPlacement Value; return Value; }

namespace
{
	FSlateFontInfo Font(float Px, bool bBold = false)
	{ return FCoreStyle::GetDefaultFontStyle(bBold ? "Bold" : "Regular", Px * 0.75f); }
	TSharedRef<STextBlock> Text(const TCHAR* Value, float Px = 14, const TCHAR* Hex = TEXT("eef2f7"), bool bBold = false)
	{ return SNew(STextBlock).Text(FText::FromString(Value)).Font(Font(Px,bBold)).ColorAndOpacity(VBoROperator::Color(Hex)); }
	TSharedRef<STextBlock> WrappedText(const TCHAR* Value, float Px, const TCHAR* Hex)
	{ const auto Widget = Text(Value,Px,Hex); Widget->SetAutoWrapText(true); return Widget; }
}

void SVBoROperatorPanel::Tick(const FGeometry& Geometry, double CurrentTime, float DeltaTime)
{
	SCompoundWidget::Tick(Geometry,CurrentTime,DeltaTime);
	if (!Runner.IsValid()) { return; }
	for (auto& Entry : Buttons) { Entry.Value->SetButtonStyle(&Styles[static_cast<int32>(Runner->GetOperatorStyle(Entry.Key))]); }
}

FReply SVBoROperatorPanel::OnPreviewKeyDown(const FGeometry&, const FKeyEvent& Event)
{
	// 자식 버튼의 Space 처리 전에 소비함 (키 한 번을 클릭과 명령으로 두 번 보내지 않게, P5i D5)
	return Runner.IsValid() && Runner->HandleOperatorKey(Event.GetKey(), Event.IsRepeat()) ? FReply::Handled() : FReply::Unhandled();
}

TSharedRef<SWidget> SVBoROperatorPanel::Button(EVBoROperatorButton Id)
{
	const auto& Shortcut = VBoROperator::Shortcut(Id);
	auto Style = [this,Id]() { return Runner.IsValid() ? Runner->GetOperatorStyle(Id) : EVBoROperatorStyle::Disabled; };
	const auto Badge = SNew(STextBlock).Font(Font(12)).ColorAndOpacity(VBoROperator::Color(TEXT("9aa6b8")))
		.Text_Lambda([this,Id]() { return FText::FromString(Runner.IsValid() ? Runner->OperatorButtonBadge(Id) : FString()); });
	TSharedPtr<SButton> Widget;
	const auto Box = SNew(SBox).HeightOverride(40)
	[
		SAssignNew(Widget,SButton).ButtonStyle(&Styles[static_cast<int32>(Style())]).ContentPadding(FMargin(0))
		.IsEnabled_Lambda([this,Id]() { return Runner.IsValid() && Runner->EvaluateOperatorButton(Id).bAccepted; })
		.ToolTipText_Lambda([this,Id]() { return FText::FromString(Runner.IsValid() ? Runner->EvaluateOperatorButton(Id).Reason : FString()); })
		.OnClicked_Lambda([this,Id]() { if (Runner.IsValid()) { Runner->PressOperatorButton(Id); } return FReply::Handled(); })
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)
			[ SNew(STextBlock).Text(FText::FromString(Shortcut.Label)).Font(Font(14))
				.ColorAndOpacity_Lambda([Style]() { return FSlateColor(VBoROperator::TextColor(Style())); }) ]
			+ SHorizontalBox::Slot().AutoWidth().Padding(4,0).VAlign(VAlign_Center)[Badge]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(SBox).MinDesiredWidth(Id == EVBoROperatorButton::Confirm ? 44 : 28)
				[SNew(STextBlock).Text(FText::FromString(Shortcut.KeyLabel)).Font(Font(12)).Justification(ETextJustify::Center)
					.ColorAndOpacity_Lambda([Style]() { return FSlateColor(VBoROperator::TextColor(Style())); })]
			]
		]
	];
	Buttons.Emplace(Id, Widget);
	return Box;
}

TSharedRef<SWidget> SVBoROperatorPanel::Section(const TCHAR* Title, TSharedRef<SWidget> Content)
{
	return SNew(SBorder).BorderImage(&PanelBrush).Padding(16)
	[SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(0,0,0,10)[Text(Title,13,TEXT("9aa6b8"),true)]
		+ SVerticalBox::Slot().AutoHeight()[Content]];
}

void SVBoROperatorPanel::Construct(const FArguments& Args)
{
	Runner = Args._Runner;
	for (int32 I = 0; I < 5; ++I) { Styles[I] = VBoROperator::ButtonStyle(static_cast<EVBoROperatorStyle>(I)); }
	PanelBrush = FSlateRoundedBoxBrush(VBoROperator::Color(TEXT("131a26")),12.f,VBoROperator::Color(TEXT("2e3a4e")),1.f);
	WarningBrush = FSlateRoundedBoxBrush(VBoROperator::Color(TEXT("131a26")),12.f,VBoROperator::Color(TEXT("f2b544")),1.f);
	auto StatusText = [this](FString FVBoROperatorStatus::*Field, float Px, const TCHAR* Hex, bool bBold = false)
	{
		return SNew(STextBlock).Font(Font(Px,bBold)).ColorAndOpacity(VBoROperator::Color(Hex)).OverflowPolicy(ETextOverflowPolicy::Ellipsis)
			.Text_Lambda([this,Field]() { return FText::FromString(Runner.IsValid() ? Runner->GetOperatorStatus().*Field : FString()); });
	};
	auto StatusRow = [&](const TCHAR* Label, FString FVBoROperatorStatus::*Field)
	{
		return SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth()[SNew(SBox).WidthOverride(148)[Text(Label,15,TEXT("9aa6b8"))]]
			+ SHorizontalBox::Slot().FillWidth(1)[StatusText(Field,15,TEXT("eef2f7"))];
	};
	auto Pair = [&](EVBoROperatorButton Left, EVBoROperatorButton Right)
	{
		return SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1).Padding(0,0,4,0)[Button(Left)]
			+ SHorizontalBox::Slot().FillWidth(1).Padding(4,0,0,0)[Button(Right)];
	};
	const auto AlignmentText = StatusText(&FVBoROperatorStatus::Alignment,15,TEXT("eef2f7"));
	AlignmentText->SetColorAndOpacity(TAttribute<FSlateColor>::CreateLambda([this]() { return FSlateColor(VBoROperator::Color(Runner.IsValid() && Runner->GetOperatorStatus().bApplied ? TEXT("3cc285") : TEXT("f2b544"))); }));
	ChildSlot
	[
		SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(VBoROperator::Color(TEXT("0b0f17"))).Padding(0)
		[
			SNew(SScrollBox)
			+ SScrollBox::Slot()
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(VBoROperator::Color(TEXT("0d2a66"))).Padding(FMargin(24,20))
					[SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().FillWidth(1)
						[SNew(SVerticalBox)
							+ SVerticalBox::Slot().AutoHeight()[StatusText(&FVBoROperatorStatus::Mode,12,TEXT("ffffff"))]
							+ SVerticalBox::Slot().AutoHeight().Padding(0,4)[StatusText(&FVBoROperatorStatus::Title,24,TEXT("ffffff"),true)]
							+ SVerticalBox::Slot().AutoHeight()[StatusText(&FVBoROperatorStatus::Mission,15,TEXT("ffffff"))]]
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Bottom).Padding(24,0,0,0)[StatusText(&FVBoROperatorStatus::Progress,13,TEXT("ffffff"))]]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(24,20,24,0)
				[
					SNew(SBorder).BorderImage(&PanelBrush).Padding(16)
					[SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight().Padding(0,0,0,8)[StatusRow(TEXT("Waiting for"),&FVBoROperatorStatus::Waiting)]
						+ SVerticalBox::Slot().AutoHeight().Padding(0,0,0,8)
						[SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().AutoWidth()[SNew(SBox).WidthOverride(148)[Text(TEXT("Alignment"),15,TEXT("9aa6b8"))]]
							+ SHorizontalBox::Slot().FillWidth(1)[AlignmentText]]
						+ SVerticalBox::Slot().AutoHeight().Padding(0,0,0,8)[StatusRow(TEXT("Seat"),&FVBoROperatorStatus::Seat)]
						+ SVerticalBox::Slot().AutoHeight()[StatusRow(TEXT("Last command"),&FVBoROperatorStatus::LastCommand)]
						+ SVerticalBox::Slot().AutoHeight().Padding(0,12,0,4)[StatusText(&FVBoROperatorStatus::Counts,12,TEXT("9aa6b8"))]
						+ SVerticalBox::Slot().AutoHeight()[StatusText(&FVBoROperatorStatus::Discards,12,TEXT("9aa6b8"))]]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(24,16,24,0)
				[
					SNew(SBorder).BorderImage(&WarningBrush).Padding(16)
					.Visibility_Lambda([this]() { return Runner.IsValid() && Runner->GetOperatorStatus().bNeedsCheck ? EVisibility::Visible : EVisibility::Collapsed; })
					[WrappedText(TEXT("Alignment needs check. Re-apply the setup alignment from the driver seat."),14,TEXT("f2b544"))]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(24,16,24,0)
				[Section(TEXT("Flow"),SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()[Button(EVBoROperatorButton::Confirm)]
					+ SVerticalBox::Slot().AutoHeight().Padding(0,8)[Pair(EVBoROperatorButton::Next,EVBoROperatorButton::Previous)]
					+ SVerticalBox::Slot().AutoHeight()[Pair(EVBoROperatorButton::Restart,EVBoROperatorButton::Ignore)])]
				+ SVerticalBox::Slot().AutoHeight().Padding(24,16,24,0)
				[SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(1).Padding(0,0,8,0)
					[Section(TEXT("Seats"),SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight()[Button(EVBoROperatorButton::Driver)]
						+ SVerticalBox::Slot().AutoHeight().Padding(0,8)[Button(EVBoROperatorButton::Passenger)]
						+ SVerticalBox::Slot().AutoHeight()[Button(EVBoROperatorButton::Rear)])]
					+ SHorizontalBox::Slot().FillWidth(1).Padding(8,0,0,0)
					[Section(TEXT("Alignment and view"),SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight()[Button(EVBoROperatorButton::Reapply)]
						+ SVerticalBox::Slot().AutoHeight().Padding(0,8)[SNew(STextBlock).Font(Font(12)).ColorAndOpacity(VBoROperator::Color(TEXT("9aa6b8"))).AutoWrapText(true)
							.Text_Lambda([this]() { return FText::FromString(Runner.IsValid() ? Runner->ReapplyHelp() : FString()); })]
						+ SVerticalBox::Slot().AutoHeight()[Button(EVBoROperatorButton::Info)]
						+ SVerticalBox::Slot().AutoHeight().Padding(0,8,0,0)[WrappedText(TEXT("If the Quest tracking space moved, save again in the P0-c Buck setup level."),12,TEXT("9aa6b8"))])]]
				+ SVerticalBox::Slot().AutoHeight().Padding(24,16,24,0)
				[Section(TEXT("Free mode"),SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()[Pair(EVBoROperatorButton::Free,EVBoROperatorButton::Reload)]
					+ SVerticalBox::Slot().AutoHeight().Padding(0,10,0,0)[WrappedText(TEXT("Reload level works in free mode only. The view fades out, then the level reopens."),12,TEXT("9aa6b8"))])]
				+ SVerticalBox::Slot().AutoHeight().Padding(24,12,24,16)[Text(TEXT("Shortcut keys also work while this window is focused."),12,TEXT("9aa6b8"))]
			]
		]
	];
}
