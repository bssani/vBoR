#include "VBoRGuidanceWidget.h"
#include "VBoRButtonLabelWidget.h"
#include "Brushes/SlateColorBrush.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBoxSlot.h"

const TCHAR* UVBoRGuidanceWidget::FindMissingBinding() const
{
	if (!Plate) { return TEXT("Plate"); }
	if (!MainText) { return TEXT("MainText"); }
	return nullptr;
}

void UVBoRGuidanceWidget::NativePreConstruct()
{
	if (IsDesignTime()) { ApplyStyle(PreviewStyle); }
	Super::NativePreConstruct();
}

void UVBoRGuidanceWidget::SetInstruction_Implementation(const FText& Instruction)
{
	if (MainText) { MainText->SetText(Instruction); }
}

void UVBoRGuidanceWidget::ApplyStyle_Implementation(const FVBoRPanelStyle& Style)
{
	if (Plate)
	{
		Plate->SetBrush(FSlateRoundedBoxBrush(Style.PlateColor, Style.CornerRadius, Style.OutlineColor, Style.OutlineWidth));
	}
	if (MainText)
	{
		MainText->SetColorAndOpacity(Style.TextColor);
		FSlateFontInfo Font = MainText->GetFont();
		Font.Size = Style.FontSize;
		MainText->SetFont(Font);
	}
	if (AccentBar)
	{
		AccentBar->SetVisibility(Style.bShowAccentBar ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		AccentBar->SetBrush(FSlateColorBrush(Style.AccentColor));
		AccentBar->SetDesiredSizeOverride(FVector2D(Style.AccentBarWidth, Style.AccentBarHeight));
		if (auto* AccentSlot = Cast<UVerticalBoxSlot>(AccentBar->Slot))
		{
			AccentSlot->SetHorizontalAlignment(Style.AccentBarWidth > 0 ? HAlign_Left : HAlign_Fill);
		}
	}
	if (Header)
	{
		Header->SetVisibility(Style.bShowHeader ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		Header->SetText(Style.HeaderText);
		Header->SetColorAndOpacity(Style.HeaderColor);
	}
}

const TCHAR* UVBoRButtonLabelWidget::FindMissingBinding() const
{
	if (!Plate) { return TEXT("Plate"); }
	if (!LabelText) { return TEXT("LabelText"); }
	return nullptr;
}

void UVBoRButtonLabelWidget::NativePreConstruct()
{
	if (IsDesignTime()) { ApplyStyle(PreviewStyle); }
	Super::NativePreConstruct();
}

void UVBoRButtonLabelWidget::SetLabel_Implementation(const FText& Label, bool bEmphasis)
{
	if (LabelText) { LabelText->SetText(Label); }
	if (bEmphasized != bEmphasis)
	{
		bEmphasized = bEmphasis;
		RefreshLabelStyle();
	}
}

void UVBoRButtonLabelWidget::ApplyStyle_Implementation(const FVBoRPanelStyle& Style)
{
	AppliedStyle = Style;
	RefreshLabelStyle();
}

void UVBoRButtonLabelWidget::RefreshLabelStyle()
{
	const bool bFilled = bEmphasized && AppliedStyle.bFillConfirmLabel;
	const FLinearColor Fill = bEmphasized
		? (bFilled ? AppliedStyle.AccentColor : AppliedStyle.PlateColor)
		: AppliedStyle.LabelPlateColor;
	const FLinearColor Outline = bEmphasized
		? (bFilled ? FLinearColor::Transparent : AppliedStyle.AccentColor)
		: AppliedStyle.LabelOutlineColor;
	const float OutlineWidth = Outline.A > 0 ? (bEmphasized ? 3.0f : 2.0f) : 0.0f;
	if (Plate) { Plate->SetBrush(FSlateRoundedBoxBrush(Fill, AppliedStyle.CornerRadius, Outline, OutlineWidth)); }
	if (LabelText) { LabelText->SetColorAndOpacity(bEmphasized ? AppliedStyle.ConfirmTextColor : AppliedStyle.LabelTextColor); }
}
