#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "Widgets/SCompoundWidget.h"
#include "Styling/SlateTypes.h"

class AVBoRScenarioRunner;
class SWindow;
class SButton;

enum class EVBoROperatorButton : uint8
{ Confirm, Next, Previous, Restart, Ignore, Driver, Passenger, Rear, Reapply, Info, Free, Reload, Count };
enum class EVBoROperatorStyle : uint8 { Primary, Secondary, Disabled, Current, Danger };
enum class EVBoRDiscard : uint8 { None, Stale, Permission, Rule, Transition, Lock };

struct FVBoRCommandDecision
{
	bool bAccepted = true;
	EVBoRDiscard Discard = EVBoRDiscard::None;
	FString Reason, Record;
};

struct FVBoROperatorStatus
{
	FString Mode, Title, Mission, Progress, Waiting, Alignment, Seat, Counts, Discards, LastCommand, FloorRouteNotice;
	bool bNeedsCheck = false, bApplied = false;
};

struct FVBoROperatorButtonState
{
	FVBoRCommandDecision Decision;
	EVBoROperatorStyle Style = EVBoROperatorStyle::Disabled;
	FString Badge;
};

struct FVBoROperatorSnapshot
{
	FVBoROperatorStatus Status;
	FVBoROperatorButtonState Buttons[static_cast<int32>(EVBoROperatorButton::Count)];
	FString ReapplyHelp;
};

struct FVBoROperatorShortcut
{
	EVBoROperatorButton Button;
	FKey Key;
	const TCHAR* Label;
	const TCHAR* KeyLabel;
};

namespace VBoROperator
{
	const TArray<FVBoROperatorShortcut>& Shortcuts();
	const FVBoROperatorShortcut& Shortcut(EVBoROperatorButton Button);
	FLinearColor Color(const TCHAR* Hex);
	FButtonStyle ButtonStyle(EVBoROperatorStyle Style);
	FLinearColor TextColor(EVBoROperatorStyle Style);
}

// 같은 프로세스 안 travel 에서만 유지함 (P5i D1)
struct FVBoROperatorPlacement
{
	bool bRemembered = false;
	FVector2D Position = FVector2D::ZeroVector, ClientSize = FVector2D(720, 1000);
	void Remember(FVector2D InPosition, FVector2D InClientSize) { Position = InPosition; ClientSize = InClientSize; bRemembered = true; }
	static FVBoROperatorPlacement& Session();
};

class SVBoROperatorPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SVBoROperatorPanel) {} SLATE_ARGUMENT(TWeakObjectPtr<AVBoRScenarioRunner>, Runner) SLATE_END_ARGS()
	void Construct(const FArguments& Args);
	virtual bool SupportsKeyboardFocus() const override { return true; }
	virtual FReply OnPreviewKeyDown(const FGeometry&, const FKeyEvent& Event) override;
	virtual void Tick(const FGeometry&, double CurrentTime, float DeltaTime) override;
private:
	friend struct FP5iTestAccess;
	TWeakObjectPtr<AVBoRScenarioRunner> Runner;
	FVBoROperatorSnapshot Snapshot;
	FButtonStyle Styles[5];
	FSlateBrush PanelBrush, WarningBrush;
	TArray<TPair<EVBoROperatorButton, TSharedPtr<SButton>>> Buttons;
	TSharedRef<SWidget> Button(EVBoROperatorButton Id);
	TSharedRef<SWidget> Section(const TCHAR* Title, TSharedRef<SWidget> Content);
};
