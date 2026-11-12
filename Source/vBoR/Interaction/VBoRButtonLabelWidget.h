#pragma once

#include "Blueprint/UserWidget.h"
#include "VBoRPanelStyle.h"
#include "VBoRButtonLabelWidget.generated.h"

class UBorder;
class UTextBlock;

UCLASS(Abstract, Blueprintable)
class VBOR_API UVBoRButtonLabelWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category="Guidance")
	void SetLabel(const FText& Label, bool bEmphasis);

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
	TObjectPtr<UTextBlock> LabelText;

private:
	void RefreshLabelStyle();

	FVBoRPanelStyle AppliedStyle;
	bool bEmphasized = false;
};
