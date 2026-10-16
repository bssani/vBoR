#include "VBoROperatorToolbar.h"

#include "Interaction/VBoRScenarioRunner.h"
#include "Brushes/SlateImageBrush.h"
#include "Editor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Framework/Commands/UIAction.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "Styling/SlateStyle.h"
#include "Styling/SlateStyleRegistry.h"
#include "ToolMenu.h"
#include "ToolMenus.h"

VBoROperatorToolbar::FState VBoROperatorToolbar::Evaluate(UWorld* PlayWorld)
{
	if (!PlayWorld || !PlayWorld->IsGameWorld())
	{
		return {nullptr, FText::FromString(TEXT("Start Play first."))};
	}
	for (TActorIterator<AVBoRScenarioRunner> It(PlayWorld); It; ++It)
	{
		return {*It, FText::FromString(TEXT("Show the operator window. Restores it if minimized, reopens it if closed."))};
	}
	return {nullptr, FText::FromString(TEXT("This level has no scenario runner."))};
}

void VBoROperatorToolbar::Execute(UWorld* PlayWorld, TFunctionRef<void(AVBoRScenarioRunner&)> ShowWindow)
{
	// 클릭 때 다시 찾음 (Play 종료·레벨 전환 전의 runner 를 붙잡지 않음, P5j D3·D4)
	if (AVBoRScenarioRunner* Runner = Evaluate(PlayWorld).Runner.Get())
	{
		ShowWindow(*Runner);
	}
}

void FVBoREditorToolsModule::StartupModule()
{
	if (bStarted) { return; }
	bStarted = true;
	Style = MakeShared<FSlateStyleSet>(VBoROperatorToolbar::StyleName);
	Style->SetContentRoot(FPaths::Combine(FPaths::ProjectDir(), TEXT("Resources/Editor")));
	Style->Set(VBoROperatorToolbar::BrushName, new FSlateVectorImageBrush(
		Style->RootToContentDir(TEXT("OperatorPanel.svg")), FVector2D(20, 20),
		FLinearColor::FromSRGBColor(FColor(0x5b, 0x8c, 0xff))));
	FSlateStyleRegistry::RegisterSlateStyle(*Style);
	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FVBoREditorToolsModule::RegisterMenus));
}

void FVBoREditorToolsModule::ShutdownModule()
{
	if (!bStarted) { return; }
	UToolMenus::UnRegisterStartupCallback(this);
	UToolMenus::UnregisterOwner(this);
	FSlateStyleRegistry::UnRegisterSlateStyle(*Style);
	Style.Reset();
	bStarted = false;
}

void FVBoREditorToolsModule::RegisterMenus()
{
	if (!bStarted) { return; }
	// 같은 owner 의 항목을 먼저 치움 (재등록·모듈 재시작 때 중복 방지, P5j E5)
	UToolMenus::UnregisterOwner(this);
	FToolMenuOwnerScoped Owner(this);
	UToolMenu* Menu = UToolMenus::Get()->ExtendMenu(VBoROperatorToolbar::MenuName);
	FToolMenuSection& Section = Menu->FindOrAddSection(VBoROperatorToolbar::SectionName);
	const auto PlayWorld = []() -> UWorld* { return GEditor ? GEditor->PlayWorld.Get() : nullptr; };
	Section.AddEntry(FToolMenuEntry::InitToolBarButton(
		VBoROperatorToolbar::EntryName,
		FUIAction(
			FExecuteAction::CreateLambda([PlayWorld]()
			{
				VBoROperatorToolbar::Execute(PlayWorld(), [](AVBoRScenarioRunner& Runner) { Runner.ShowOperatorWindow(); });
			}),
			FCanExecuteAction::CreateLambda([PlayWorld]() { return VBoROperatorToolbar::Evaluate(PlayWorld()).CanExecute(); })),
		FText::FromString(TEXT("Operator")),
		TAttribute<FText>::CreateLambda([PlayWorld]() { return VBoROperatorToolbar::Evaluate(PlayWorld()).ToolTip; }),
		FSlateIcon(VBoROperatorToolbar::StyleName, VBoROperatorToolbar::BrushName)));
}

IMPLEMENT_MODULE(FVBoREditorToolsModule, vBoREditorTools)
