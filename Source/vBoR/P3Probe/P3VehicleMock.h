#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
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
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="자리값", meta=(Units="cm")) FVector ConsoleTabOffsetCm = FVector(35,0,3.5);
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="근사 차량") TObjectPtr<AVBoRAxisPart> OutsideAxisHandle;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="근사 차량") TObjectPtr<AVBoRAxisPart> InsideAxisHandle;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="근사 차량") TObjectPtr<AVBoRAxisPart> ConsoleTab;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="근사 차량") TObjectPtr<AVBoRHingedPanel> ConsoleLid;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="근사 차량") TObjectPtr<AVBoRPokeButton> ReachButton;
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
private:
	friend struct FP3FlowTestAccess;
	void PlaceReachButton();
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Plates;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> StartMarker;
	UPROPERTY() TObjectPtr<UWidgetComponent> MockLabel;
};
