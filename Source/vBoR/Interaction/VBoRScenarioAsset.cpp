#include "VBoRScenarioAsset.h"
#include "VBoRFloorRoute.h"
#include "VBoRPanelAnchor.h"
#include "Components/ActorComponent.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"

namespace
{
	UWorld* ScenarioEditorWorld()
	{
#if WITH_EDITOR
		if (GEngine)
		{
			for (const FWorldContext& Context : GEngine->GetWorldContexts())
			{
				if (Context.WorldType == EWorldType::Editor) { return Context.World(); }
			}
		}
#endif
		return nullptr;
	}
}

UVBoRScenarioAsset::UVBoRScenarioAsset()
{
	auto Add = [this](const TCHAR* Id, const TCHAR* Text, EVBoRStepRule Rule, const TCHAR* Tag, EVBoRConfirmBy By)
	{
		FVBoRScenarioStep Step; Step.StepId = Id; Step.Instruction = FText::FromString(Text);
		Step.Rule = Rule; Step.TargetTag = Tag; Step.ConfirmBy = By; Steps.Add(Step);
	};
	Add(TEXT("01"), TEXT("Approach the vehicle with the key."), EVBoRStepRule::Approach, TEXT("Car.Approach"), EVBoRConfirmBy::None);
	Steps.Last().MarkerTag = TEXT("Car.DriverDoor");
	Add(TEXT("02"), TEXT("Check the exterior lights as you approach."), EVBoRStepRule::Confirm, TEXT(""), EVBoRConfirmBy::Either);
	Add(TEXT("07"), TEXT("Operate the door handle."), EVBoRStepRule::DoorUnlocked, TEXT("Car.DriverDoor"), EVBoRConfirmBy::None);
	Add(TEXT("08"), TEXT("Open the door."), EVBoRStepRule::DoorOpened, TEXT("Car.DriverDoor"), EVBoRConfirmBy::None);
	Add(TEXT("09"), TEXT("Get in the vehicle."), EVBoRStepRule::Confirm, TEXT(""), EVBoRConfirmBy::Operator);
	Add(TEXT("10"), TEXT("Sit in the seat."), EVBoRStepRule::Seated, TEXT("Car.DriverSeat"), EVBoRConfirmBy::Operator);
	Add(TEXT("11"), TEXT("Close the door."), EVBoRStepRule::DoorLatched, TEXT("Car.DriverDoor"), EVBoRConfirmBy::None);
	Add(TEXT("12"), TEXT("Look around the interior."), EVBoRStepRule::Confirm, TEXT(""), EVBoRConfirmBy::Either);
	Add(TEXT("13"), TEXT("Reach the farthest button with your finger."), EVBoRStepRule::ButtonPressed, TEXT("Car.ReachButton"), EVBoRConfirmBy::None);
	Add(TEXT("14"), TEXT("Check from the passenger seat."), EVBoRStepRule::Viewpoint, TEXT("Car.PassengerView"), EVBoRConfirmBy::Either);
	Add(TEXT("15"), TEXT("Check from the rear seat."), EVBoRStepRule::Viewpoint, TEXT("Car.RearView"), EVBoRConfirmBy::Either);
	Add(TEXT("18"), TEXT("Open the door to get out."), EVBoRStepRule::DoorOpened, TEXT("Car.DriverDoor"), EVBoRConfirmBy::None);
	Steps.Last().bRequireBaseViewpoint = true;
	Add(TEXT("19"), TEXT("Get out of the vehicle."), EVBoRStepRule::Confirm, TEXT(""), EVBoRConfirmBy::Operator);
	Add(TEXT("20"), TEXT("Close the door."), EVBoRStepRule::DoorLatched, TEXT("Car.DriverDoor"), EVBoRConfirmBy::None);
}

TArray<FString> UVBoRScenarioAsset::GetPanelTagOptions() const
{
	return TagOptions(&FVBoRScenarioStep::PanelTag, ScenarioEditorWorld(), AVBoRPanelAnchor::StaticClass());
}

TArray<FString> UVBoRScenarioAsset::GetRouteTagOptions() const
{
	return TagOptions(&FVBoRScenarioStep::RouteTag, ScenarioEditorWorld(), AVBoRFloorRoute::StaticClass());
}

TArray<FString> UVBoRScenarioAsset::GetMarkerTagOptions() const
{
	return TagOptions(&FVBoRScenarioStep::MarkerTag, ScenarioEditorWorld());
}

TArray<FString> UVBoRScenarioAsset::GetTargetTagOptions() const
{
	return TagOptions(&FVBoRScenarioStep::TargetTag, ScenarioEditorWorld());
}

TArray<FString> UVBoRScenarioAsset::TagOptions(FName FVBoRScenarioStep::* Field, UWorld* World, const UClass* ActorClass) const
{
	TSet<FName> Tags;
	if (World)
	{
		auto AddLevelTags = [&](const TArray<FName>& LevelTags)
		{
			for (const FName Tag : LevelTags)
			{
				if (ActorClass || Tag.ToString().StartsWith(TEXT("Car."))) { Tags.Add(Tag); }
			}
		};
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (ActorClass && !It->IsA(ActorClass)) { continue; }
			AddLevelTags(It->Tags);
			TInlineComponentArray<UActorComponent*> Components(*It);
			for (const UActorComponent* Component : Components) { AddLevelTags(Component->ComponentTags); }
		}
	}
	else
	{
		// GetOptions receives the asset (no row context, R12 D3)
		for (const auto& Step : Steps) { Tags.Add(Step.*Field); }
	}
	Tags.Remove(NAME_None);

	TArray<FString> Options;
	Options.Reserve(Tags.Num() + 1);
	for (const FName Tag : Tags) { Options.Add(Tag.ToString()); }
	Options.Sort();
	Options.Insert(FName(NAME_None).ToString(), 0);
	return Options;
}
