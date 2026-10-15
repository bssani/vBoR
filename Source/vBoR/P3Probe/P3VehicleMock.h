#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "../Interaction/VBoRKeyFob.h"
#include "P3VehicleMock.generated.h"

class UStaticMeshComponent;
class UWidgetComponent;
class UVBoREyePointComponent;
class UVBoRViewpointComponent;
class UVBoRApproachZoneComponent;
class UVBoRSeatZoneComponent;
class UVBoRVehicleAlignmentComponent;
class AVBoRHingedDoor;
class AVBoRHingedPanel;
class AVBoRPokeButton;
class AVBoRAxisPart;
class AVBoRKeyTable;
class AVBoRSteeringWheel;

UCLASS(Blueprintable)
class VBOR_API AP3VehicleMock : public AActor
{
	GENERATED_BODY()
public:
	AP3VehicleMock();
	virtual void OnConstruction(const FTransform& Transform) override;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="자리값", meta=(Units="cm")) FVector BodySizeCm = FVector(470,185,145);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="자리값", meta=(Units="cm")) FVector DriverEyeCm = FVector(-20,-37,115);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="자리값", meta=(Units="cm")) FVector PassengerEyeCm = FVector(-20,37,115);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="자리값", meta=(Units="cm")) FVector RearEyeCm = FVector(-100,-37,118);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="자리값", meta=(Units="cm")) FVector DoorHingeCm = FVector(35,-92.5,35);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="자리값", meta=(Units="cm")) FVector ReachButtonCm = FVector(40,25,90);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="자리값", meta=(Units="cm")) FVector DashboardCenterCm;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="자리값", meta=(Units="cm", ClampMin="4")) FVector DashboardSizeCm;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="자리값", meta=(Units="cm")) float ApproachSurroundCm = 150;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="자리값", meta=(Units="cm")) float StartOutsideCm = 250;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="근사 차량") TObjectPtr<UVBoREyePointComponent> DriverEye;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="근사 차량") TObjectPtr<UVBoRViewpointComponent> PassengerView;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="근사 차량") TObjectPtr<UVBoRViewpointComponent> RearView;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="근사 차량") TObjectPtr<UVBoRApproachZoneComponent> Approach;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="근사 차량") TObjectPtr<UVBoRSeatZoneComponent> Seat;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="근사 차량") TObjectPtr<UVBoRVehicleAlignmentComponent> Alignment;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="근사 차량") TObjectPtr<AVBoRHingedDoor> Door;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="자리값", meta=(Units="cm")) FVector OutsideHandlePivotCm = FVector(-65,-101.5,97);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="자리값", meta=(Units="cm")) FVector InsideHandlePivotCm = FVector(-50,-83.5,85);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="자리값", meta=(Units="cm")) FVector ConsoleLidPivotCm = FVector(-45,0,70);
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="근사 차량") TObjectPtr<AVBoRAxisPart> OutsideAxisHandle;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="근사 차량") TObjectPtr<AVBoRAxisPart> InsideAxisHandle;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="근사 차량") TObjectPtr<AVBoRHingedPanel> ConsoleLid;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="자리값|근사 글러브박스", meta=(Units="cm")) FVector GloveboxHingeCm = FVector(38,37,50);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="자리값|근사 글러브박스", meta=(Units="cm", ClampMin="1")) FVector GloveboxSizeCm = FVector(3,40,28);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="자리값|근사 글러브박스", meta=(Units="cm")) FVector GloveboxHandlePivotCm = FVector(37,25,76);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="자리값|근사 글러브박스", meta=(Units="cm", ClampMin="0.1")) FVector GloveboxHandleSizeCm = FVector(1.5,10,3);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="자리값|근사 글러브박스", meta=(Units="deg", ClampMin="1", ClampMax="170")) float GloveboxMaxAngleDeg = 80;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="자리값|근사 글러브박스", meta=(Units="deg", ClampMin="1", ClampMax="170")) float GloveboxHandleMaxAngleDeg = 25;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="자리값|근사 글러브박스", meta=(Units="cm", ClampMin="0")) float GloveboxMinAxisDistanceCm = 8;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="근사 차량") TObjectPtr<AVBoRHingedPanel> Glovebox;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="근사 차량") TObjectPtr<AVBoRAxisPart> GloveboxHandle;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="자리값|근사 기어", meta=(Units="cm")) FVector GearPivotCm = FVector(39,-30,70);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="자리값|근사 기어", meta=(Units="cm")) FVector GearSizeCm = FVector(2,16,2);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="자리값|근사 기어") FRotator GearAxisRotation = FRotator::ZeroRotator;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="자리값|근사 기어", meta=(ToolTip="Approximate detent angles in deg")) TArray<float> GearDetentAnglesDeg = {-20,0,20};
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="자리값|근사 기어", meta=(Units="deg")) float GearUpThresholdDeg = 15;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="자리값|근사 기어", meta=(Units="deg")) float GearDownThresholdDeg = 15;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="자리값|근사 기어", meta=(Units="cm")) FVector GearButtonSizeCm = FVector(1.6,1.6,0.6);
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="근사 차량") TObjectPtr<AVBoRAxisPart> GearLever;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="근사 차량") TObjectPtr<AVBoRPokeButton> GearButton;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="자리값|근사 방향지시", meta=(Units="cm")) FVector TurnSignalPivotCm = FVector(39,-44,70);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="자리값|근사 방향지시", meta=(Units="cm", ClampMin="0.1")) FVector TurnSignalSizeCm = FVector(2,16,2);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="자리값|근사 방향지시") FRotator TurnSignalAxisRotation = FRotator::ZeroRotator;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="자리값|근사 방향지시", meta=(ToolTip="Approximate detent angles in deg")) TArray<float> TurnSignalDetentAnglesDeg = {-20,0,20};
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="자리값|근사 방향지시", meta=(Units="deg")) float TurnSignalUpThresholdDeg = 15;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="자리값|근사 방향지시", meta=(Units="deg")) float TurnSignalDownThresholdDeg = 15;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="자리값|근사 방향지시", meta=(Units="deg")) float TurnSignalNudgeDeg = 7;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="근사 차량") TObjectPtr<AVBoRAxisPart> TurnSignalLever;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="자리값|근사 스티어링", meta=(Units="cm")) FVector SteeringHubCm = FVector(35,-37,87);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="자리값|근사 스티어링") FRotator SteeringAxisRotation = FRotator::ZeroRotator;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="자리값|근사 스티어링", meta=(Units="cm", ClampMin="1")) float SteeringDiameterCm = 36;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="자리값|근사 스티어링", meta=(Units="cm", ClampMin="0.1")) float SteeringThicknessCm = 3;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="자리값|근사 스티어링", meta=(Units="deg", ClampMin="0.1")) float SteeringLimitDeg = 450;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="자리값|근사 스티어링", meta=(Units="cm", ClampMin="0")) float SteeringMinGrabRadiusCm = 12;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="자리값|근사 스티어링", meta=(Units="cm", ClampMin="0")) float SteeringMaxGrabRadiusCm = 24;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="근사 차량") TObjectPtr<AVBoRSteeringWheel> SteeringWheel;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="근사 차량") TObjectPtr<AVBoRPokeButton> ReachButton;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="자리값", meta=(Units="cm")) FVector FrontKeyTableCm = FVector(330,-90,0);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="자리값", meta=(Units="cm")) FVector RearKeyTableCm = FVector(-330,-90,0);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="근사 차량") TSubclassOf<AVBoRKeyTable> KeyTableClass;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="근사 차량") TObjectPtr<AVBoRKeyTable> FrontKeyTable;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="근사 차량") TObjectPtr<AVBoRKeyTable> RearKeyTable;
	UPROPERTY(BlueprintAssignable, Category="Key") FVBoRKeyEvent OnKeyUnlockPressed;
	UPROPERTY(BlueprintAssignable, Category="Key") FVBoRKeyEvent OnKeyLockPressed;
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
private:
	friend struct FP3FlowTestAccess;
	friend struct FP5bTestAccess;
	friend struct FP5cTestAccess;
	friend struct FP5dTestAccess;
	void PlaceReachButton();
	AVBoRKeyTable* CreateKeyTable(FVector PositionCm, const TCHAR* UnlockTag, const TCHAR* LockTag);
	UFUNCTION() void KeyUnlockPressed(AVBoRKeyFob* Key);
	UFUNCTION() void KeyLockPressed(AVBoRKeyFob* Key);
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Plates;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> StartMarker;
	UPROPERTY() TObjectPtr<UWidgetComponent> MockLabel;
};
