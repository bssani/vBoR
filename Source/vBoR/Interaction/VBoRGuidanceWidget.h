#pragma once

#include "Blueprint/UserWidget.h"
#include "VBoRPanelStyle.h"
#include "VBoRGuidanceWidget.generated.h"

class UBorder;
class UImage;
class UTextBlock;

UCLASS(Abstract, Blueprintable)
class VBOR_API UVBoRGuidanceWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category="Guidance")
	void SetInstruction(const FText& Instruction);

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category="Guidance")
	void ApplyStyle(const FVBoRPanelStyle& Style);

	const TCHAR* FindMissingBinding() const;

protected:
	void NativePreConstruct() override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Guidance")
	FVBoRPanelStyle PreviewStyle;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidget))
	TObjectPtr<UBorder> Plate;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidget))
	TObjectPtr<UTextBlock> MainText;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UImage> AccentBar;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> Header;
};
