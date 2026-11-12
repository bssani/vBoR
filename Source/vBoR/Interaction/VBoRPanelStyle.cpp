#include "VBoRPanelStyle.h"

namespace
{
	FLinearColor PanelSRGB(uint8 Red, uint8 Green, uint8 Blue)
	{
		return FLinearColor::FromSRGBColor(FColor(Red, Green, Blue));
	}
}

FVBoRPanelStyle::FVBoRPanelStyle()
	: PlateColor(PanelSRGB(0x1E, 0x22, 0x26))
	, TextColor(PanelSRGB(0xE3, 0xE6, 0xE9))
	, AccentColor(PanelSRGB(0x6F, 0x8F, 0xAF))
	, HeaderText(NSLOCTEXT("VBoRGuidance", "Header", "GUIDANCE"))
	, HeaderColor(TextColor)
	, ConfirmTextColor(TextColor)
	, LabelPlateColor(PlateColor)
	, LabelOutlineColor(PanelSRGB(0x3E, 0x45, 0x4C))
	, LabelTextColor(PanelSRGB(0xC3, 0xC9, 0xCF))
{
}

FVBoRPanelStyle FVBoRPanelStyle::FromPreset(EVBoRPanelStylePreset Preset)
{
	FVBoRPanelStyle Style;
	switch (Preset)
	{
	case EVBoRPanelStylePreset::NavyInstrument:
		Style.PlateColor = PanelSRGB(0x18, 0x21, 0x2B);
		Style.OutlineColor = PanelSRGB(0x2A, 0x36, 0x44);
		Style.OutlineWidth = 2;
		Style.CornerRadius = 20;
		Style.TextColor = PanelSRGB(0xDC, 0xE3, 0xEA);
		Style.AccentColor = PanelSRGB(0xC9, 0x9A, 0x4B);
		Style.bShowAccentBar = false;
		Style.bShowHeader = true;
		Style.HeaderColor = PanelSRGB(0x8E, 0x9B, 0xAA);
		Style.bFillConfirmLabel = true;
		Style.ConfirmTextColor = PanelSRGB(0x1A, 0x1D, 0x21);
		Style.LabelPlateColor = Style.PlateColor;
		Style.LabelOutlineColor = PanelSRGB(0x3A, 0x47, 0x57);
		Style.LabelTextColor = PanelSRGB(0xC4, 0xCD, 0xD6);
		break;
	case EVBoRPanelStylePreset::SoftLight:
		Style.PlateColor = PanelSRGB(0xD3, 0xD1, 0xCC);
		Style.CornerRadius = 24;
		Style.TextColor = PanelSRGB(0x22, 0x26, 0x2A);
		Style.AccentColor = PanelSRGB(0x3B, 0x4A, 0x5A);
		Style.AccentBarWidth = 96;
		Style.AccentBarHeight = 6;
		Style.HeaderColor = Style.TextColor;
		Style.bFillConfirmLabel = true;
		Style.ConfirmTextColor = PanelSRGB(0xE4, 0xE6, 0xE8);
		Style.LabelPlateColor = Style.PlateColor;
		Style.LabelOutlineColor = FLinearColor::Transparent;
		Style.LabelTextColor = PanelSRGB(0x2C, 0x31, 0x36);
		break;
	default:
		break;
	}
	return Style;
}
