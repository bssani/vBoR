#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "../Interaction/VBoRKeyFob.h"
#include "P3VehicleMock.generated.h"

class UStaticMeshComponent;
class UVBoRAlignPointComponent;
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
class AVBoRTwoAxisPart;
class AVBoRKnob;
class AVBoRFloorRoute;
class UBoxComponent;
class AP1TrunkMock;
class AVBoRPoweredPanel;
class AVBoRLoadTarget;
class AVBoRBoxStand;
class AVBoRFoldingSeat;
class AVBoRPanelAnchor;

UCLASS(Blueprintable)
class VBOR_API AP3VehicleMock : public AActor
{
	GENERATED_BODY()
public:
	AP3VehicleMock();
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Placeholder", EditFixedSize, meta=(ToolTip="Mock alignment points in car local cm")) TArray<FVector> AlignPointsCm = {FVector(4,-60,50),FVector(4,-14,50),FVector(-44,-14,50),FVector(-44,-60,50)};
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Approximate vehicle") TArray<TObjectPtr<UVBoRAlignPointComponent>> AlignPoints;
	virtual void OnConstruction(const FTransform& Transform) override;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Placeholder", meta=(Units="cm")) FVector BodySizeCm = FVector(470,185,145);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Placeholder", meta=(Units="cm")) FVector DriverEyeCm = FVector(-20,-37,115);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Placeholder", meta=(Units="cm")) FVector PassengerEyeCm = FVector(-20,37,115);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Placeholder", meta=(Units="cm")) FVector RearEyeCm = FVector(-100,-37,118);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Placeholder|Approximate rear seat", meta=(Units="cm", ClampMin="1")) FVector RearCushionSizeCm = FVector(48,177,10);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Placeholder|Approximate rear seat", meta=(Units="cm", ClampMin="1")) FVector RearBackSizeCm = FVector(8,177,60);
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Approximate vehicle") TObjectPtr<UStaticMeshComponent> RearCushion;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Approximate vehicle") TObjectPtr<AVBoRFoldingSeat> RearSeat;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Placeholder", meta=(Units="cm")) FVector DoorHingeCm = FVector(35,-92.5,35);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Placeholder", meta=(Units="cm")) FVector ReachButtonCm = FVector(40,25,90);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Placeholder", meta=(Units="cm")) FVector DashboardCenterCm;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Placeholder", meta=(Units="cm", ClampMin="4")) FVector DashboardSizeCm;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Placeholder", meta=(Units="cm")) float ApproachSurroundCm = 150;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Placeholder", meta=(Units="cm")) float StartOutsideCm = 250;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Approximate vehicle") TObjectPtr<UBoxComponent> Footprint;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Placeholder", meta=(Units="cm", ClampMin="0")) float FootprintMarginCm = 20;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Placeholder", meta=(ToolTip="Car-relative route point (approximate geometry, placeholder values, cm)")) TArray<FVector> DriverDoorRoutePointsCm = {FVector(-295,-20,0), FVector(-280,-150,0), FVector(-150,-165,0), FVector(-65,-165,0)};
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Approximate vehicle") TObjectPtr<AVBoRFloorRoute> DriverDoorRoute;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Placeholder", meta=(ToolTip="Car-relative route point (approximate geometry, placeholder values, cm)")) TArray<FVector> AroundCarRoutePointsCm = {FVector(360,40,0), FVector(300,170,0), FVector(-150,175,0), FVector(-270,160,0), FVector(-330,0,0)};
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Approximate vehicle") TObjectPtr<AVBoRFloorRoute> AroundCarRoute;
	// Outside, trunk, driver, passenger, rear; car-local cm/Yaw (P5z D9)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Placeholder|Approximate guidance", EditFixedSize, meta=(ToolTip="Car-local panel anchor positions, cm")) TArray<FVector> PanelAnchorPositionsCm = {FVector(95,-160,155),FVector(-335,0,155),FVector(40,-37,100),FVector(40,37,100),FVector(-40,-37,103)};
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Placeholder|Approximate guidance", EditFixedSize, meta=(ToolTip="Car-local panel anchor Yaw, deg")) TArray<float> PanelAnchorYawDeg = {-90,180,180,180,180};
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Approximate vehicle") TArray<TObjectPtr<AVBoRPanelAnchor>> PanelAnchors;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Placeholder", meta=(Units="cm")) FVector RearBoxStandCm = FVector(-340,140,0);
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Approximate vehicle") TObjectPtr<AP1TrunkMock> Trunk;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Approximate vehicle") TObjectPtr<AVBoRPoweredPanel> Liftgate;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Approximate vehicle") TObjectPtr<AVBoRLoadTarget> LoadTarget;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Approximate vehicle") TObjectPtr<AVBoRBoxStand> BoxStand;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Approximate vehicle") TObjectPtr<UVBoREyePointComponent> DriverEye;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Approximate vehicle") TObjectPtr<UVBoRViewpointComponent> PassengerView;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Approximate vehicle") TObjectPtr<UVBoRViewpointComponent> RearView;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Approximate vehicle") TObjectPtr<UVBoRApproachZoneComponent> Approach;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Approximate vehicle") TObjectPtr<UVBoRSeatZoneComponent> Seat;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Approximate vehicle") TObjectPtr<UVBoRVehicleAlignmentComponent> Alignment;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Approximate vehicle") TObjectPtr<AVBoRHingedDoor> Door;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Placeholder", meta=(Units="cm")) FVector OutsideHandlePivotCm = FVector(-65,-101.5,97);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Placeholder", meta=(Units="cm")) FVector InsideHandlePivotCm = FVector(-50,-83.5,85);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Placeholder", meta=(Units="cm")) FVector ConsoleLidPivotCm = FVector(-45,0,70);
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Approximate vehicle") TObjectPtr<AVBoRAxisPart> OutsideAxisHandle;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Approximate vehicle") TObjectPtr<AVBoRAxisPart> InsideAxisHandle;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Placeholder|Approximate console", meta=(Units="cm")) FVector ConsoleHandlePivotCm = FVector(-9,0,70);
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Approximate vehicle") TObjectPtr<AVBoRAxisPart> ConsoleHandle;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Approximate vehicle") TObjectPtr<AVBoRHingedPanel> ConsoleLid;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Placeholder|Approximate glovebox", meta=(Units="cm")) FVector GloveboxHingeCm = FVector(38,37,50);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Placeholder|Approximate glovebox", meta=(Units="cm", ClampMin="1")) FVector GloveboxSizeCm = FVector(3,40,28);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Placeholder|Approximate glovebox", meta=(Units="cm")) FVector GloveboxHandlePivotCm = FVector(37,25,76);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Placeholder|Approximate glovebox", meta=(Units="cm", ClampMin="0.1")) FVector GloveboxHandleSizeCm = FVector(1.5,10,3);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Placeholder|Approximate glovebox", meta=(Units="deg", ClampMin="1", ClampMax="170")) float GloveboxMaxAngleDeg = 80;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Placeholder|Approximate glovebox", meta=(Units="deg", ClampMin="1", ClampMax="170")) float GloveboxHandleMaxAngleDeg = 25;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Placeholder|Approximate glovebox", meta=(Units="cm", ClampMin="0")) float GloveboxMinAxisDistanceCm = 8;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Approximate vehicle") TObjectPtr<AVBoRHingedPanel> Glovebox;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Approximate vehicle") TObjectPtr<AVBoRAxisPart> GloveboxHandle;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Placeholder|Approximate gear", meta=(Units="cm")) FVector GearPivotCm = FVector(39,-30,70);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Placeholder|Approximate gear", meta=(Units="cm")) FVector GearSizeCm = FVector(2,22,2);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Placeholder|Approximate lever", meta=(Units="cm", ClampMin="0")) float LeverPushMarginCm = 1.5;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Placeholder|Approximate gear") FRotator GearAxisRotation = FRotator::ZeroRotator;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Placeholder|Approximate gear", meta=(ToolTip="Approximate detent angles in deg")) TArray<float> GearDetentAnglesDeg = {-20,0,20};
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Placeholder|Approximate gear", meta=(Units="deg")) float GearUpThresholdDeg = 15;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Placeholder|Approximate gear", meta=(Units="deg")) float GearDownThresholdDeg = 15;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Placeholder|Approximate gear", meta=(Units="cm")) FVector GearButtonSizeCm = FVector(1.6,1.6,0.6);
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Approximate vehicle") TObjectPtr<AVBoRAxisPart> GearLever;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Approximate vehicle") TObjectPtr<AVBoRPokeButton> GearButton;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Placeholder|Approximate turn signal", meta=(Units="cm")) FVector TurnSignalPivotCm = FVector(39,-44,70);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Placeholder|Approximate turn signal", meta=(Units="cm", ClampMin="0.1")) FVector TurnSignalSizeCm = FVector(2,22,2);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Placeholder|Approximate turn signal") FRotator TurnSignalAxisRotation = FRotator::ZeroRotator;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Placeholder|Approximate turn signal", meta=(ToolTip="Approximate detent angles in deg")) TArray<float> TurnSignalDetentAnglesDeg = {-20,0,20};
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Placeholder|Approximate turn signal", meta=(Units="deg")) float TurnSignalUpThresholdDeg = 15;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Placeholder|Approximate turn signal", meta=(Units="deg")) float TurnSignalDownThresholdDeg = 15;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Placeholder|Approximate turn signal", meta=(Units="deg")) float TurnSignalNudgeDeg = 7;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Approximate vehicle") TObjectPtr<AVBoRAxisPart> TurnSignalLever;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Placeholder|Approximate steering", meta=(Units="cm")) FVector SteeringHubCm = FVector(35,-37,87);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Placeholder|Approximate steering") FRotator SteeringAxisRotation = FRotator::ZeroRotator;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Placeholder|Approximate steering", meta=(Units="cm", ClampMin="1")) float SteeringDiameterCm = 36;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Placeholder|Approximate steering", meta=(Units="cm", ClampMin="0.1")) float SteeringThicknessCm = 3;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Placeholder|Approximate steering", meta=(Units="deg", ClampMin="0.1")) float SteeringLimitDeg = 450;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Placeholder|Approximate steering", meta=(Units="cm", ClampMin="0")) float SteeringMinGrabRadiusCm = 12;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Placeholder|Approximate steering", meta=(Units="cm", ClampMin="0")) float SteeringMaxGrabRadiusCm = 24;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Approximate vehicle") TObjectPtr<AVBoRSteeringWheel> SteeringWheel;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Placeholder|Approximate rearview mirror", meta=(Units="cm")) FVector MirrorPivotCm = FVector(38,0,127);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Placeholder|Approximate rearview mirror", meta=(Units="cm")) FVector MirrorCenterCm = FVector(35,0,127);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Placeholder|Approximate rearview mirror", meta=(Units="cm", ClampMin="0.1")) FVector MirrorSizeCm = FVector(2,25,7);
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Approximate vehicle") TObjectPtr<AVBoRTwoAxisPart> RearviewMirror;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Approximate vehicle") TObjectPtr<UStaticMeshComponent> MirrorSupport;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Placeholder|Approximate knob", meta=(Units="cm")) FVector KnobPivotCm = FVector(10,0,59.5);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Placeholder|Approximate knob", meta=(Units="cm", ClampMin="0.1")) float KnobDiameterCm = 6;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Placeholder|Approximate knob", meta=(Units="cm", ClampMin="0.1")) float KnobHeightCm = 2.5;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Placeholder|Approximate knob", meta=(ClampMin="0.1")) float KnobTwistGain = 2;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Placeholder|Approximate knob", meta=(Units="cm")) FVector KnobSupportCenterCm = FVector(10,0,58.5);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Placeholder|Approximate knob", meta=(Units="cm", ClampMin="0.1")) FVector KnobSupportSizeCm = FVector(20,16,2);
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Approximate vehicle") TObjectPtr<AVBoRKnob> ConsoleKnob;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Approximate vehicle") TObjectPtr<UStaticMeshComponent> KnobSupport;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Approximate vehicle") TObjectPtr<AVBoRPokeButton> ReachButton;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Placeholder", meta=(Units="cm")) FVector FrontKeyTableCm = FVector(330,-90,0);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Placeholder", meta=(Units="cm")) FVector RearKeyTableCm = FVector(-330,-90,0);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Approximate vehicle") TSubclassOf<AVBoRKeyTable> KeyTableClass;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Approximate vehicle") TObjectPtr<AVBoRKeyTable> FrontKeyTable;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Approximate vehicle") TObjectPtr<AVBoRKeyTable> RearKeyTable;
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
	friend struct FP5eTestAccess;
	friend struct FP5fTestAccess;
	void PlaceReachButton();
	void CreateRearSection();
	AVBoRKeyTable* CreateKeyTable(FVector PositionCm, const TCHAR* KeyTag, const TCHAR* UnlockTag, const TCHAR* LockTag);
	UFUNCTION() void KeyUnlockPressed(AVBoRKeyFob* Key);
	UFUNCTION() void KeyLockPressed(AVBoRKeyFob* Key);
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Plates;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> StartMarker;
	UPROPERTY() TObjectPtr<UWidgetComponent> MockLabel;
};
