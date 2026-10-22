#include "VBoRScenarioAsset.h"

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
