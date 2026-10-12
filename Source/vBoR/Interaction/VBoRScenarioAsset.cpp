#include "VBoRScenarioAsset.h"

UVBoRScenarioAsset::UVBoRScenarioAsset()
{
	auto Add = [this](const TCHAR* Id, const TCHAR* Text, EVBoRStepRule Rule, const TCHAR* Tag, EVBoRConfirmBy By)
	{
		FVBoRScenarioStep Step; Step.StepId = Id; Step.Instruction = FText::FromString(Text);
		Step.Rule = Rule; Step.TargetTag = Tag; Step.ConfirmBy = By; Steps.Add(Step);
	};
	Add(TEXT("01"), TEXT("키를 가진 상태로 차량에 접근하세요."), EVBoRStepRule::Approach, TEXT("Car.Approach"), EVBoRConfirmBy::None);
	Steps.Last().MarkerTag = TEXT("Car.DriverDoor");
	Add(TEXT("02"), TEXT("접근할 때 외장 조명을 확인하세요."), EVBoRStepRule::Confirm, TEXT(""), EVBoRConfirmBy::Either);
	Add(TEXT("07"), TEXT("도어 핸들을 조작해 보세요."), EVBoRStepRule::DoorUnlocked, TEXT("Car.DriverDoor"), EVBoRConfirmBy::None);
	Add(TEXT("08"), TEXT("문을 열어보세요."), EVBoRStepRule::DoorOpened, TEXT("Car.DriverDoor"), EVBoRConfirmBy::None);
	Add(TEXT("09"), TEXT("차량에 탑승해 보세요."), EVBoRStepRule::Confirm, TEXT(""), EVBoRConfirmBy::Operator);
	Add(TEXT("10"), TEXT("좌석에 앉아보세요."), EVBoRStepRule::Seated, TEXT("Car.DriverSeat"), EVBoRConfirmBy::Operator);
	Add(TEXT("11"), TEXT("문을 닫아보세요."), EVBoRStepRule::DoorLatched, TEXT("Car.DriverDoor"), EVBoRConfirmBy::None);
	Add(TEXT("12"), TEXT("실내를 둘러보세요."), EVBoRStepRule::Confirm, TEXT(""), EVBoRConfirmBy::Either);
	Add(TEXT("13"), TEXT("가장 먼 버튼까지 손가락을 뻗어보세요."), EVBoRStepRule::ButtonPressed, TEXT("Car.ReachButton"), EVBoRConfirmBy::None);
	Add(TEXT("14"), TEXT("동승석 위치에서 확인하세요."), EVBoRStepRule::Viewpoint, TEXT("Car.PassengerView"), EVBoRConfirmBy::Either);
	Add(TEXT("15"), TEXT("2열 위치에서 확인하세요."), EVBoRStepRule::Viewpoint, TEXT("Car.RearView"), EVBoRConfirmBy::Either);
	Add(TEXT("18"), TEXT("하차를 위해 문을 열어보세요."), EVBoRStepRule::DoorOpened, TEXT("Car.DriverDoor"), EVBoRConfirmBy::None);
	Steps.Last().bRequireBaseViewpoint = true;
	Add(TEXT("19"), TEXT("차량에서 내려보세요."), EVBoRStepRule::Confirm, TEXT(""), EVBoRConfirmBy::Operator);
	Add(TEXT("20"), TEXT("문을 닫아보세요."), EVBoRStepRule::DoorLatched, TEXT("Car.DriverDoor"), EVBoRConfirmBy::None);
}
