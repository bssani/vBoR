#if WITH_DEV_AUTOMATION_TESTS
#include "VBoROperatorToolbar.h"
#include "Interaction/VBoRScenarioRunner.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "Styling/ISlateStyle.h"
#include "Styling/SlateBrush.h"
#include "Styling/SlateStyleRegistry.h"
#include "ToolMenu.h"
#include "ToolMenus.h"

namespace
{
	struct FP5jWorld
	{
		UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
		FP5jWorld() { GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World); }
		~FP5jWorld() { GEngine->DestroyWorldContext(World); World->DestroyWorld(false); }
	};

	int32 P5jEntryCount()
	{
		int32 Count = 0;
		if (const UToolMenu* Menu = UToolMenus::Get()->FindMenu(VBoROperatorToolbar::MenuName))
		{
			for (const FToolMenuSection& Section : Menu->Sections)
			{
				for (const FToolMenuEntry& Entry : Section.Blocks)
				{
					if (Entry.Name == VBoROperatorToolbar::EntryName) { ++Count; }
				}
			}
		}
		return Count;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5jNoPlay, "vBoR.P5j.E1NoPlay", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5jNoPlay::RunTest(const FString&)
{
	const auto State = VBoROperatorToolbar::Evaluate(nullptr);
	TestFalse(TEXT("Play 전 비활성"), State.CanExecute());
	TestEqual(TEXT("Play 전 툴팁"), State.ToolTip.ToString(), FString(TEXT("Start Play first.")));
	int32 Calls = 0;
	VBoROperatorToolbar::Execute(nullptr, [&Calls](AVBoRScenarioRunner&) { ++Calls; });
	TestEqual(TEXT("Play 전 호출 없음"), Calls, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5jRunner, "vBoR.P5j.E2RunnerDispatch", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5jRunner::RunTest(const FString&)
{
	FP5jWorld Play;
	FP5jWorld Other;
	AVBoRScenarioRunner* Runner = Play.World->SpawnActor<AVBoRScenarioRunner>();
	Other.World->SpawnActor<AVBoRScenarioRunner>();
	const auto State = VBoROperatorToolbar::Evaluate(Play.World);
	TestTrue(TEXT("Play runner 가 있으면 활성"), State.CanExecute());
	TestEqual(TEXT("현재 Play 월드의 runner 선택"), State.Runner.Get(), Runner);
	TestEqual(TEXT("활성 툴팁"), State.ToolTip.ToString(), FString(TEXT("Show the operator window. Restores it if minimized, reopens it if closed.")));
	int32 Calls = 0;
	VBoROperatorToolbar::Execute(Play.World, [&](AVBoRScenarioRunner& Selected)
	{
		++Calls;
		TestEqual(TEXT("호출 대상도 같은 runner"), &Selected, Runner);
		Selected.ShowOperatorWindow();
	});
	TestEqual(TEXT("한 번 누르면 한 번 호출"), Calls, 1);
	TestFalse(TEXT("자동화에서 실제 창 없음"), Runner->HasOperatorWindow());
	Play.World->DestroyActor(Runner);
	TestFalse(TEXT("runner 삭제 뒤 비활성"), VBoROperatorToolbar::Evaluate(Play.World).CanExecute());
	VBoROperatorToolbar::Execute(Play.World, [&Calls](AVBoRScenarioRunner&) { ++Calls; });
	TestEqual(TEXT("이전 runner 호출 안 함"), Calls, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5jNoRunner, "vBoR.P5j.E3NoRunner", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5jNoRunner::RunTest(const FString&)
{
	FP5jWorld Play;
	const auto State = VBoROperatorToolbar::Evaluate(Play.World);
	TestFalse(TEXT("빈 Play 월드는 비활성"), State.CanExecute());
	TestEqual(TEXT("runner 없음 툴팁"), State.ToolTip.ToString(), FString(TEXT("This level has no scenario runner.")));
	int32 Calls = 0;
	VBoROperatorToolbar::Execute(Play.World, [&Calls](AVBoRScenarioRunner&) { ++Calls; });
	TestEqual(TEXT("빈 월드 호출 없음"), Calls, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5jIcon, "vBoR.P5j.E4Icon", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5jIcon::RunTest(const FString&)
{
	const ISlateStyle* Style = FSlateStyleRegistry::FindSlateStyle(VBoROperatorToolbar::StyleName);
	if (!TestNotNull(TEXT("에디터 스타일 등록"), Style)) { return false; }
	const FSlateBrush* Brush = Style->GetOptionalBrush(VBoROperatorToolbar::BrushName, nullptr, nullptr);
	if (!TestNotNull(TEXT("Operator 브러시 조회"), Brush)) { return false; }
	const FString Path = FPaths::Combine(FPaths::ProjectDir(), TEXT("Resources/Editor/OperatorPanel.svg"));
	TestTrue(TEXT("SVG 파일 있음"), IFileManager::Get().FileExists(*Path));
	TestEqual(TEXT("브러시가 지정 SVG 사용"), Brush->GetResourceName(), FName(*Path));
	TestTrue(TEXT("20 x 20 크기"), Brush->ImageSize == FVector2f(20, 20));
	TestTrue(TEXT("벡터 이미지"), Brush->ImageType == ESlateBrushImageType::Vector);
	TestTrue(TEXT("sRGB 파랑 tint"), Brush->TintColor.GetSpecifiedColor().Equals(FLinearColor::FromSRGBColor(FColor(0x5b, 0x8c, 0xff))));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5jRegistration, "vBoR.P5j.E5Registration", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5jRegistration::RunTest(const FString&)
{
	auto& Module = FModuleManager::GetModuleChecked<FVBoREditorToolsModule>(TEXT("vBoREditorTools"));
	// headless 에서는 시작 callback 을 실행하지 않을 수 있어 같은 등록 함수를 직접 호출함 (P5j E5)
	Module.RegisterMenus();
	TestEqual(TEXT("Play 툴바 버튼 하나"), P5jEntryCount(), 1);
	Module.RegisterMenus();
	Module.StartupModule();
	TestEqual(TEXT("재등록·중복 시작 뒤에도 하나"), P5jEntryCount(), 1);
	Module.ShutdownModule();
	TestEqual(TEXT("종료 때 항목 해제"), P5jEntryCount(), 0);
	TestNull(TEXT("종료 때 스타일 해제"), FSlateStyleRegistry::FindSlateStyle(VBoROperatorToolbar::StyleName));
	Module.ShutdownModule();
	Module.StartupModule();
	Module.RegisterMenus();
	TestEqual(TEXT("재시작 뒤 버튼 하나"), P5jEntryCount(), 1);
	TestNotNull(TEXT("재시작 뒤 스타일 복구"), FSlateStyleRegistry::FindSlateStyle(VBoROperatorToolbar::StyleName));
	return true;
}
#endif
