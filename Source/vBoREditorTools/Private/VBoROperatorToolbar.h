#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"

class AVBoRScenarioRunner;
class FSlateStyleSet;
class UWorld;

namespace VBoROperatorToolbar
{
	inline const FName MenuName(TEXT("LevelEditor.LevelEditorToolBar.PlayToolBar"));
	inline const FName SectionName(TEXT("VBoROperator"));
	inline const FName EntryName(TEXT("VBoR.ShowOperator"));
	inline const FName StyleName(TEXT("VBoREditorTools"));
	inline const FName BrushName(TEXT("OperatorPanel"));

	struct FState
	{
		TWeakObjectPtr<AVBoRScenarioRunner> Runner;
		FText ToolTip;
		bool CanExecute() const { return Runner.IsValid(); }
	};

	FState Evaluate(UWorld* PlayWorld);
	void Execute(UWorld* PlayWorld, TFunctionRef<void(AVBoRScenarioRunner&)> ShowWindow);
}

class FVBoREditorToolsModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	void RegisterMenus();

private:
	TSharedPtr<FSlateStyleSet> Style;
	bool bStarted = false;
};
