#pragma once

#include "CoreMinimal.h"
#include "VBoRPanelStyle.generated.h"

UENUM(BlueprintType)
enum class EVBoRPanelStylePreset : uint8
{
	Graphite,
	NavyInstrument,
	SoftLight,
	Custom
};

USTRUCT(BlueprintType)
struct VBOR_API FVBoRPanelStyle
{
	GENERATED_BODY()

	FVBoRPanelStyle();
	static FVBoRPanelStyle FromPreset(EVBoRPanelStylePreset Preset);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Plate", meta=(sRGB="true"))
	FLinearColor PlateColor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Plate", meta=(sRGB="true"))
	FLinearColor OutlineColor = FLinearColor::Transparent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Plate", meta=(ClampMin="0", DisplayName="Outline Width (px)"))
	float OutlineWidth = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Plate", meta=(ClampMin="0", DisplayName="Corner Radius (px)"))
	float CornerRadius = 28;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Instruction", meta=(sRGB="true"))
	FLinearColor TextColor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Instruction", meta=(ClampMin="1"))
	float FontSize = 40;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Accent", meta=(sRGB="true"))
	FLinearColor AccentColor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Accent")
	bool bShowAccentBar = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Accent", meta=(ClampMin="0", DisplayName="Accent Bar Width (px)", ToolTip="Zero fills the full plate width."))
	float AccentBarWidth = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Accent", meta=(ClampMin="0", DisplayName="Accent Bar Height (px)"))
	float AccentBarHeight = 8;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Header")
	bool bShowHeader = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Header")
	FText HeaderText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Header", meta=(sRGB="true"))
	FLinearColor HeaderColor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Button Label")
	bool bFillConfirmLabel = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Button Label", meta=(sRGB="true"))
	FLinearColor ConfirmTextColor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Button Label", meta=(sRGB="true"))
	FLinearColor LabelPlateColor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Button Label", meta=(sRGB="true"))
	FLinearColor LabelOutlineColor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Button Label", meta=(sRGB="true"))
	FLinearColor LabelTextColor;
};
