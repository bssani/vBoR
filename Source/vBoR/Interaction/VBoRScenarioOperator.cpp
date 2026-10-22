#include "VBoRScenarioRunner.h"
#include "VBoRFoldingSeat.h"
#include "VBoRVehiclePowerComponent.h"
#include "VBoRVehicleAlignmentComponent.h"
#include "VBoRViewpointComponent.h"
#include "../Spectator/VBoRSpectatorModel.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "Misc/App.h"
#include "Widgets/SWindow.h"

namespace
{
	EVBoRFlowCommand FlowAction(EVBoROperatorButton Button)
	{
		switch (Button)
		{
		case EVBoROperatorButton::CapturePoint: return EVBoRFlowCommand::CapturePoint;
		case EVBoROperatorButton::SavePoints: return EVBoRFlowCommand::SavePoints;
		case EVBoROperatorButton::CancelPoints: return EVBoRFlowCommand::CancelPoints;
		case EVBoROperatorButton::Confirm: return EVBoRFlowCommand::Confirm;
		case EVBoROperatorButton::Next: return EVBoRFlowCommand::Next;
		case EVBoROperatorButton::Previous: return EVBoRFlowCommand::Previous;
		case EVBoROperatorButton::Restart: return EVBoRFlowCommand::Restart;
		case EVBoROperatorButton::Ignore: return EVBoRFlowCommand::IgnoreAlignment;
		case EVBoROperatorButton::Driver: return EVBoRFlowCommand::ReturnBase;
		case EVBoROperatorButton::Passenger: return EVBoRFlowCommand::Passenger;
		case EVBoROperatorButton::Rear: return EVBoRFlowCommand::Rear;
		case EVBoROperatorButton::Reapply: return EVBoRFlowCommand::ReapplyAlignment;
		case EVBoROperatorButton::Free: return EVBoRFlowCommand::EnterFreeMode;
		case EVBoROperatorButton::Reload: return EVBoRFlowCommand::ReloadLevel;
		case EVBoROperatorButton::FoldSeat: return EVBoRFlowCommand::FoldSeat;
		case EVBoROperatorButton::UnfoldSeat: return EVBoRFlowCommand::UnfoldSeat;
		case EVBoROperatorButton::Power: return EVBoRFlowCommand::Power;
		default: return EVBoRFlowCommand::Confirm;
		}
	}
	FString CommandName(EVBoRFlowCommand Action)
	{
		for (const auto& Shortcut : VBoROperator::Shortcuts())
		{ if (Shortcut.Button != EVBoROperatorButton::Info && FlowAction(Shortcut.Button) == Action) { return Shortcut.Label; } }
		return TEXT("Change viewpoint");
	}
	bool SeatAction(EVBoRFlowCommand Action)
	{ return Action == EVBoRFlowCommand::ReturnBase || Action == EVBoRFlowCommand::Viewpoint || Action == EVBoRFlowCommand::Passenger || Action == EVBoRFlowCommand::Rear; }
	EVBoROperatorStyle ButtonStyle(EVBoROperatorButton Button, const FVBoRCommandDecision& Decision, bool bReapplyPrimary, const FString& Badge)
	{
		if (!Decision.bAccepted) { return EVBoROperatorStyle::Disabled; }
		if ((Button == EVBoROperatorButton::Confirm && !bReapplyPrimary) || (Button == EVBoROperatorButton::Reapply && bReapplyPrimary)) { return EVBoROperatorStyle::Primary; }
		if (Badge == TEXT("Current")) { return EVBoROperatorStyle::Current; }
		return Button == EVBoROperatorButton::Reload ? EVBoROperatorStyle::Danger : EVBoROperatorStyle::Secondary;
	}
}

UVBoRViewpointComponent* AVBoRScenarioRunner::CommandViewpoint(EVBoRFlowCommand Action) const
{
	if (!SeatAction(Action) || Action == EVBoRFlowCommand::ReturnBase) { return nullptr; }
	if (Action == EVBoRFlowCommand::Viewpoint)
	{ return CurrentStep() && CurrentStep()->Rule == EVBoRStepRule::Viewpoint && Targets.IsValidIndex(StepIndex) ? Cast<UVBoRViewpointComponent>(Targets[StepIndex]) : nullptr; }
	// Use startup validation cache. Null if missing/ambiguous/deleted (P5o)
	return SeatViews[Action == EVBoRFlowCommand::Passenger ? 0 : 1].Get();
}

UVBoRViewpointComponent* AVBoRScenarioRunner::ScanSeatView(FName Tag) const
{
	UVBoRViewpointComponent* Found = nullptr;
	int32 Matches = 0;
#if WITH_DEV_AUTOMATION_TESTS
	++OperatorViewpointScans;
#endif
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		TArray<UVBoRViewpointComponent*> Views; It->GetComponents(Views);
		if (It->ActorHasTag(Tag)) { ++Matches; Found = Views.Num() == 1 ? Views[0] : nullptr; }
		TArray<UActorComponent*> Components; It->GetComponents(Components);
		for (UActorComponent* Component : Components)
		{ if (Component->ComponentHasTag(Tag)) { ++Matches; Found = Cast<UVBoRViewpointComponent>(Component); } }
	}
	return Matches == 1 ? Found : nullptr;
}

FVBoRCommandDecision AVBoRScenarioRunner::EvaluateCommand(const FVBoRFlowCommand& Command) const
{
	return EvaluateCommand(Command, CommandViewpoint(Command.Action));
}

FVBoRCommandDecision AVBoRScenarioRunner::EvaluateCommand(const FVBoRFlowCommand& Command, UVBoRViewpointComponent* Viewpoint) const
{
	auto Reject = [](EVBoRDiscard Kind, const TCHAR* English, const TCHAR* Korean)
	{ return FVBoRCommandDecision{false, Kind, English, Korean}; };
	if (bReloadPending) { return Reject(EVBoRDiscard::Transition, TEXT("Level reload in progress"), TEXT("Command discarded — level reopen blackout")); }
	if (Command.Token.StepIndex != StepIndex || Command.Token.Generation != Generation)
	{ return Reject(EVBoRDiscard::Stale, TEXT("Stale step or generation"), TEXT("Command discarded — stale step or guidance complete")); }
	const bool bOperator = Command.Source == EVBoRCommandSource::Operator;
	const auto Action = Command.Action;
	if (!bOperator && Action != EVBoRFlowCommand::Confirm && Action != EVBoRFlowCommand::Viewpoint && Action != EVBoRFlowCommand::ReturnBase &&
		!(bFreeMode && (Action == EVBoRFlowCommand::Passenger || Action == EVBoRFlowCommand::Rear)))
	{ return Reject(EVBoRDiscard::Permission, TEXT("Operator only"), TEXT("Command discarded — confirmation not permitted")); }
	if (Action == EVBoRFlowCommand::CapturePoint || Action == EVBoRFlowCommand::SavePoints || Action == EVBoRFlowCommand::CancelPoints)
	{
		FString Reason;
		if (Action == EVBoRFlowCommand::CapturePoint && (bRunning || bFreeMode)) { Reason = TEXT("Before the flow starts only"); }
		else if (!Alignment) { Reason = TEXT("Alignment component missing"); }
		else if (Action == EVBoRFlowCommand::CapturePoint) { Reason = Alignment->CapturePointRejection(); }
		else if (Action == EVBoRFlowCommand::SavePoints) { Reason = Alignment->SavePointsRejection(); }
		else if (!Alignment->IsPointAligning()) { Reason = TEXT("No point alignment in progress"); }
		return Reason.IsEmpty() ? FVBoRCommandDecision{} : FVBoRCommandDecision{false,EVBoRDiscard::Rule,Reason,TEXT("Point alignment — ") + Reason};
	}
	if (Action == EVBoRFlowCommand::ReapplyAlignment)
	{
		const FString Reason = Alignment ? Alignment->ReapplyRejection() : FString(TEXT("Alignment component missing"));
		if (!Reason.IsEmpty()) { return {false, EVBoRDiscard::Transition, Reason, TEXT("Command discarded — ") + Reason}; }
		return {};
	}
	if (Action == EVBoRFlowCommand::IgnoreAlignment)
	{
		if (bFreeMode) { return Reject(EVBoRDiscard::Rule, TEXT("No steps in free mode"), TEXT("Command discarded — no steps in free mode")); }
		if (!bRunning && !ValidationProblems.IsEmpty()) { return Reject(EVBoRDiscard::None, TEXT("Scenario tag validation failed"), TEXT("Command discarded — cannot ignore tag validation failure")); }
		// Keep state/counts if already started (P5i D3, dim no-op button)
		if (bRunning) { return Reject(EVBoRDiscard::None, TEXT("Scenario already started"), TEXT("")); }
		if (Alignment && Alignment->IsSwitching()) { return Reject(EVBoRDiscard::Transition, TEXT("View transition in progress"), TEXT("Command discarded — transitioning")); }
		return {};
	}
	if (!bRunning && !bFreeMode && Action != EVBoRFlowCommand::Restart)
	{ return Reject(EVBoRDiscard::Rule, TEXT("Scenario has not started"), TEXT("Command discarded — not started")); }
	if (bFreeMode && (Action == EVBoRFlowCommand::Confirm || Action == EVBoRFlowCommand::Next || Action == EVBoRFlowCommand::Previous || Action == EVBoRFlowCommand::EnterFreeMode || Action == EVBoRFlowCommand::Viewpoint))
	{ return Reject(EVBoRDiscard::Rule, TEXT("No steps in free mode"), TEXT("Command discarded — no steps in free mode")); }
	if (Action == EVBoRFlowCommand::ReloadLevel && !bFreeMode)
	{ return Reject(EVBoRDiscard::Rule, TEXT("Reload is available in free mode only"), TEXT("Command discarded — level reopen only in free mode")); }
	if (SeatAction(Action))
	{
		if (bFreeMode && !bOperator && (!Alignment || !Alignment->IsSwitching()) && (Command.bConfirmLocked || ConfirmLockRemaining > 0 || bPanelPlacementPending))
		{ return Reject(EVBoRDiscard::Lock, TEXT("Panel placement locked"), TEXT("Seat command discarded — panel placement lock")); }
		const FString Reason = Alignment ? Alignment->SwitchRejection(Viewpoint, Action == EVBoRFlowCommand::ReturnBase) : FString(TEXT("Alignment component missing"));
		if (!Reason.IsEmpty())
		{ return {false, EVBoRDiscard::Transition, Reason, FString::Printf(TEXT("Viewpoint command %s | discarded"), bOperator ? TEXT("Operator") : TEXT("Participant"))}; }
		return {};
	}
	if (Alignment && Alignment->IsSwitching()) { return Reject(EVBoRDiscard::Transition, TEXT("View transition in progress"), TEXT("Command discarded — transitioning")); }
	if (Action == EVBoRFlowCommand::Power)
	{
		return VehiclePower.IsValid() && VehiclePower->IsAvailable() ? FVBoRCommandDecision{} :
			Reject(EVBoRDiscard::Rule, TEXT("No vehicle power"), TEXT("Command discarded — missing or duplicate power component"));
	}
	if (Action == EVBoRFlowCommand::FoldSeat || Action == EVBoRFlowCommand::UnfoldSeat)
	{
		bool bFound = false;
		const auto Required = Action == EVBoRFlowCommand::FoldSeat ? EVBoRFoldingSeatState::Standing : EVBoRFoldingSeatState::Folded;
		for (TActorIterator<AVBoRFoldingSeat> It(GetWorld()); It; ++It)
		{
			bFound = true;
			if (It->GetState() == Required) { return {}; }
		}
		return bFound ? Reject(EVBoRDiscard::None, TEXT("No seat ready for this action"), TEXT("")) :
			Reject(EVBoRDiscard::Rule, TEXT("No folding seat"), TEXT("Command discarded — no folding seat"));
	}
	if (Action == EVBoRFlowCommand::Confirm)
	{
		const auto* Step = CurrentStep();
		if (!Step || !NeedsConfirmation() || (Step->ConfirmBy == EVBoRConfirmBy::Operator && !bOperator) || (Step->ConfirmBy == EVBoRConfirmBy::Participant && bOperator))
		{ return Reject(EVBoRDiscard::Permission, TEXT("Confirmation permission required"), TEXT("Command discarded — confirmation not permitted")); }
		if (!RuleSatisfied()) { return Reject(EVBoRDiscard::Rule, TEXT("Rule not satisfied"), TEXT("Command discarded — rule unsatisfied")); }
		if (Command.bConfirmLocked || ConfirmLockRemaining > 0 || bPanelPlacementPending)
		{ return Reject(EVBoRDiscard::Lock, TEXT("Confirm locked"), TEXT("Command discarded — immediately after step start (confirmation lock/panel placement)")); }
	}
	if (Action == EVBoRFlowCommand::Next && CurrentStep() && CurrentStep()->bRequireBaseViewpoint && (!Alignment || !Alignment->IsBaseViewpoint()))
	{ return Reject(EVBoRDiscard::Rule, TEXT("Return to the driver seat first"), TEXT("Command discarded — return to reference before exit")); }
	if (Action == EVBoRFlowCommand::Restart)
	{
		if (!ValidationProblems.IsEmpty()) { return Reject(EVBoRDiscard::None, TEXT("Scenario tag validation failed"), TEXT("Retry discarded — tag validation failed")); }
		if (!Alignment || Alignment->State != EVBoRAlignmentState::Applied) { return Reject(EVBoRDiscard::Rule, TEXT("Saved alignment required"), TEXT("Retry discarded — saved alignment required for reference return")); }
	}
	return {};
}

void AVBoRScenarioRunner::RecordDecision(const FVBoRFlowCommand& Command, const FVBoRCommandDecision& Decision)
{
	LastOperatorCommand = CommandName(Command.Action) + (Decision.bAccepted ? TEXT(" — accepted") : FString(TEXT(" — discarded (")) + Decision.Reason + TEXT(")"));
	if (Decision.bAccepted) { return; }
	switch (Decision.Discard)
	{
	case EVBoRDiscard::Stale: ++StaleDiscards; break;
	case EVBoRDiscard::Permission: ++PermissionDiscards; break;
	case EVBoRDiscard::Rule: ++RuleDiscards; break;
	case EVBoRDiscard::Transition: ++TransitionDiscards; break;
	case EVBoRDiscard::Lock: ++ConfirmLockDiscards; break;
	default: break;
	}
	if (Decision.Record.StartsWith(TEXT("Viewpoint command")))
	{
		// Keep component request/discard counts too (P5i D3)
		if (Alignment)
		{ if (Command.Action == EVBoRFlowCommand::ReturnBase) { Alignment->ReturnToBase(); } else { Alignment->SwitchViewpoint(CommandViewpoint(Command.Action)); } }
		LastViewRequest = Decision.Record;
	}
	if (!Decision.Record.IsEmpty()) { Record(Decision.Record); }
}

FVBoRCommandDecision AVBoRScenarioRunner::EvaluateOperatorButton(EVBoROperatorButton Button) const
{
	if (Button == EVBoROperatorButton::Info)
	{ return bReloadPending ? FVBoRCommandDecision{false, EVBoRDiscard::Transition, TEXT("Level reload in progress"), TEXT("Command discarded — level reopen blackout")} : FVBoRCommandDecision{}; }
	return EvaluateCommand({FlowAction(Button), EVBoRCommandSource::Operator, GetToken()});
}

void AVBoRScenarioRunner::PressOperatorButton(EVBoROperatorButton Button)
{
	if (Button == EVBoROperatorButton::Info) { KeyOperatorInfo(); }
	else { QueueLive(FlowAction(Button), EVBoRCommandSource::Operator); }
}

bool AVBoRScenarioRunner::HandleOperatorKey(FKey Key, bool bRepeat)
{
	if (HandleHomeShiftKey(Key, bRepeat)) { return true; }
	for (const auto& Shortcut : VBoROperator::Shortcuts())
	{
		if (Shortcut.Key == Key) { if (!bRepeat) { PressOperatorButton(Shortcut.Button); } return true; }
	}
	return false;
}

void AVBoRScenarioRunner::FinishReapply()
{
	bReapplyPending = false;
	if (bPointSavePending)
	{
		bPointSavePending = false;
		LastOperatorCommand = TEXT("Save points — accepted (") + (Alignment->State == EVBoRAlignmentState::Applied ? FString(TEXT("Applied")) : Alignment->EnglishReason()) + TEXT(")");
		Record(LastOperatorCommand); return;
	}
	const FString Result = Alignment->State == EVBoRAlignmentState::Applied ? FString(TEXT("Applied")) : Alignment->State == EVBoRAlignmentState::NoSavedValue ? FString(TEXT("no saved alignment")) : TEXT("still needs check: ") + Alignment->EnglishReason();
	LastOperatorCommand = TEXT("Re-apply saved alignment — accepted (") + Result + TEXT(")");
	Record(TEXT("Operator reapply alignment — ") + Alignment->Describe());
	if (!bRunning && !bFreeMode && ValidationProblems.IsEmpty() && Alignment->State == EVBoRAlignmentState::Applied)
	{ Alignment->FlowStarted(); bRunning = true; EnterStep(0, true); }
}

FVBoROperatorStatus AVBoRScenarioRunner::GetOperatorStatus() const
{
	FVBoROperatorStatus S;
	const int32 Total = Scenario ? Scenario->Steps.Num() : 0;
	const int32 Completed = bFreeMode ? (bFinished ? Total : OperatorCompletedSteps) : bRunning ? FMath::Max(0, StepIndex) : 0;
	S.Mode = bFreeMode ? TEXT("FREE MODE") : bRunning ? TEXT("GUIDED SCENARIO") : TEXT("GETTING READY");
	S.Title = bFreeMode ? TEXT("Free mode") : bRunning ? FString::Printf(TEXT("Mission %d of %d"), StepIndex + 1, Total) : FString(TEXT("Getting ready"));
	S.Mission = bFreeMode ? (bFinished ? TEXT("All missions complete") : TEXT("Entered by operator (End)")) : CurrentStep() && bRunning ? CurrentStep()->StepId.ToString() + TEXT(" ,  ") + VBoRSpectator::MissionText(*CurrentStep(), FText::FromString(TEXT("Mission in progress"))).ToString() : FString(TEXT("Check the setup alignment before starting."));
	S.Progress = FString::Printf(TEXT("%d of %d complete"), Completed, Total);
	S.Waiting = bFreeMode ? TEXT("Nothing — participant explores") : !bRunning ? TEXT("Alignment check") : !RuleSatisfied() ? TEXT("Waiting for the rule") : (ConfirmLockRemaining > 0 || bPanelPlacementPending) ? TEXT("Confirm locked") : NeedsConfirmation() ? (CurrentStep()->ConfirmBy == EVBoRConfirmBy::Participant ? TEXT("Waiting for participant confirmation") : TEXT("Operator confirmation needed")) : TEXT("Waiting for the rule");
	S.bApplied = Alignment && Alignment->State == EVBoRAlignmentState::Applied;
	S.bNeedsCheck = Alignment && Alignment->State == EVBoRAlignmentState::NeedsCheck;
	S.Alignment = S.bApplied ? TEXT("Applied") : S.bNeedsCheck ? TEXT("Needs check — ") + Alignment->EnglishReason() : FString(TEXT("No saved alignment"));
	if (bAlignmentIgnored) { S.Alignment += TEXT(" ,  started without alignment"); }
	S.Seat = Alignment ? Alignment->SeatName() : FString(TEXT("Driver"));
	if (IsHomeShiftEnabled())
	{
		const FVector2D Offset = Alignment ? Alignment->GetHomeShiftCm() : FVector2D::ZeroVector;
		// Display cm → m only (P3c D6)
		S.Seat += FString::Printf(TEXT(" ,  Home shift ON ,  %+.1f m front ,  %+.1f m side"), Offset.X / 100, Offset.Y / 100);
	}
	S.Counts = FString::Printf(TEXT("Previous %d ,  Retry %d ,  Skipped %d ,  End %d ,  Re-apply %d"), PreviousCount, RetryCount, SkipCount, EndCount, ReapplyCount);
	S.Discards = FString::Printf(TEXT("Discarded — stale %d ,  permission %d ,  rule %d ,  transition %d ,  lock %d"), StaleDiscards, PermissionDiscards, RuleDiscards, TransitionDiscards, ConfirmLockDiscards);
	S.LastCommand = LastOperatorCommand.IsEmpty() ? FString(TEXT("None")) : LastOperatorCommand;
	S.PointAlignment = Alignment ? Alignment->PointStatus() : FString();
	S.FloorRouteNotice = FloorRouteNotice;
	return S;
}

FString AVBoRScenarioRunner::OperatorButtonBadge(EVBoROperatorButton Button) const
{
	return OperatorButtonBadge(Button, CommandViewpoint(FlowAction(Button)));
}

FString AVBoRScenarioRunner::OperatorButtonBadge(EVBoROperatorButton Button, UVBoRViewpointComponent* Viewpoint) const
{
	if (Button == EVBoROperatorButton::Info) { return bOperatorInfo ? TEXT("On") : TEXT("Off"); }
	if (!Alignment || Alignment->IsSwitching()) { return FString(); }
	if ((Button == EVBoROperatorButton::Driver && Alignment->IsBaseViewpoint()) ||
		((Button == EVBoROperatorButton::Passenger || Button == EVBoROperatorButton::Rear) && Alignment->IsAtViewpoint(Viewpoint))) { return TEXT("Current"); }
	return FString();
}

EVBoROperatorStyle AVBoRScenarioRunner::GetOperatorStyle(EVBoROperatorButton Button) const
{
	const bool bReapplyPrimary = Alignment && Alignment->State == EVBoRAlignmentState::NeedsCheck && EvaluateOperatorButton(EVBoROperatorButton::Reapply).bAccepted;
	return ButtonStyle(Button, EvaluateOperatorButton(Button), bReapplyPrimary, OperatorButtonBadge(Button));
}

FVBoROperatorSnapshot AVBoRScenarioRunner::GetOperatorSnapshot() const
{
	FVBoROperatorSnapshot Snapshot;
	Snapshot.Status = GetOperatorStatus();
	Snapshot.ReapplyHelp = ReapplyHelp();
	// Same seat viewpoint for decision/label (P5i R1-3). Use startup cache, no world scan (P5o)
	for (const auto& Shortcut : VBoROperator::Shortcuts())
	{
		const auto Button = Shortcut.Button;
		auto& State = Snapshot.Buttons[static_cast<int32>(Button)];
		UVBoRViewpointComponent* Viewpoint = CommandViewpoint(FlowAction(Button));
		State.Decision = Button == EVBoROperatorButton::Info ? EvaluateOperatorButton(Button) : EvaluateCommand({FlowAction(Button), EVBoRCommandSource::Operator, GetToken()}, Viewpoint);
		State.Badge = OperatorButtonBadge(Button, Viewpoint);
	}
	const bool bReapplyPrimary = Snapshot.Status.bNeedsCheck && Snapshot.Buttons[static_cast<int32>(EVBoROperatorButton::Reapply)].Decision.bAccepted;
	for (const auto& Shortcut : VBoROperator::Shortcuts())
	{
		auto& State = Snapshot.Buttons[static_cast<int32>(Shortcut.Button)];
		State.Style = ButtonStyle(Shortcut.Button, State.Decision, bReapplyPrimary, State.Badge);
	}
	return Snapshot;
}

FString AVBoRScenarioRunner::ReapplyHelp() const
{
	if (Alignment && !Alignment->IsBaseViewpoint() && !Alignment->IsSwitching()) { return TEXT("Return to the driver seat first."); }
	return TEXT("Uses the Buck alignment saved at setup. Never reads the participant's head.");
}

void AVBoRScenarioRunner::ShowOperatorWindow()
{
	if (!FApp::CanEverRender() || GIsAutomationTesting || !FSlateApplication::IsInitialized() || !GetWorld() || !GetWorld()->IsGameWorld()) { return; }
	if (!OperatorWindow)
	{
		const auto& Placement = FVBoROperatorPlacement::Session();
		const auto Content = SNew(SVBoROperatorPanel).Runner(this);
		OperatorWindow = SNew(SWindow).Title(FText::FromString(TEXT("vBoR Operator"))).HasCloseButton(false).SupportsMinimize(true)
			.AutoCenter(Placement.bRemembered ? EAutoCenter::None : EAutoCenter::PreferredWorkArea)
			.ScreenPosition(Placement.Position).ClientSize(Placement.ClientSize).AdjustInitialSizeAndPositionForDPIScale(!Placement.bRemembered)
			[Content];
		OperatorWindow->SetWidgetToFocusOnActivate(Content);
		OperatorWindow->SetOnWindowClosed(FOnWindowClosed::CreateLambda([WeakRunner = TWeakObjectPtr<AVBoRScenarioRunner>(this)](const TSharedRef<SWindow>& Window)
		{ if (WeakRunner.IsValid()) { WeakRunner->HandleOperatorWindowClosed(Window); } }));
		FSlateApplication::Get().AddWindow(OperatorWindow.ToSharedRef());
	}
	if (OperatorWindow->IsWindowMinimized()) { OperatorWindow->Restore(); }
	OperatorWindow->BringToFront(true);
}

void AVBoRScenarioRunner::CloseOperatorWindow()
{
	if (!OperatorWindow) { return; }
	const auto Window = OperatorWindow.ToSharedRef();
	HandleOperatorWindowClosed(Window);
	if (FSlateApplication::IsInitialized()) { FSlateApplication::Get().RequestDestroyWindow(Window); }
}

void AVBoRScenarioRunner::HandleOperatorWindowClosed(const TSharedRef<SWindow>& Window)
{
	// Late close after EndPlay must not alter new window/saved position (P5i R1-1)
	if (OperatorWindow != Window) { return; }
	FVBoROperatorPlacement::Session().Remember(Window->GetPositionInScreen(), Window->GetClientSizeInScreen());
	OperatorWindow.Reset();
}
