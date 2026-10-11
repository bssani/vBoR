#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "InputCoreTypes.h"
#include "../Interaction/VBoRBuckAlignment.h"
#include "P0BuckBench.generated.h"

class UVBoREyePointComponent;
class UTextRenderComponent;
class UStaticMesh;
class APawn;
class APlayerController;

DECLARE_LOG_CATEGORY_EXTERN(LogP0Buck, Log, All);

UCLASS()
class VBOR_API AP0BuckBench : public AActor
{
	GENERATED_BODY()
public:
	AP0BuckBench();
	virtual void Tick(float DeltaSeconds) override;
	UPROPERTY(VisibleAnywhere, Category = "Buck") TObjectPtr<UVBoREyePointComponent> EyePoint;
	UPROPERTY(VisibleAnywhere, Category = "Buck") TObjectPtr<UTextRenderComponent> Panel;
	UPROPERTY(EditAnywhere, Category = "Buck", meta = (Units = "cm")) FVector EyeOffsetCm = FVector(0, 0, 120);
	UPROPERTY(EditAnywhere, Category = "Buck", meta = (Units = "cm")) FVector CushionTopCm = FVector(-15, 0, -65);
	UPROPERTY(EditAnywhere, Category = "Buck", meta = (Units = "cm")) FVector CushionSizeCm = FVector(45, 50, 12);
	UPROPERTY(EditAnywhere, Category = "Buck", meta = (Units = "cm")) double BackFrontXCm = -35;
	UPROPERTY(EditAnywhere, Category = "Buck", meta = (Units = "cm")) FVector BackSizeCm = FVector(10, 50, 65);
	UPROPERTY(EditAnywhere, Category = "Buck", meta = (Units = "cm")) FVector WheelCenterCm = FVector(45, 0, -20);
	UPROPERTY(EditAnywhere, Category = "Buck", meta = (Units = "cm")) double WheelDiameterCm = 38;
	UPROPERTY(EditAnywhere, Category = "Buck", meta = (Units = "cm")) double WheelTubeCm = 2;
	UPROPERTY(EditAnywhere, Category = "Buck", meta = (ClampMin = "12", ClampMax = "64")) int32 WheelSegments = 32;
	UPROPERTY(EditAnywhere, Category = "Buck", meta = (Units = "cm")) double DashboardXCm = 70;
	UPROPERTY(EditAnywhere, Category = "Buck", meta = (Units = "cm")) double DashboardCenterZCm = -10;
	UPROPERTY(EditAnywhere, Category = "Buck", meta = (Units = "cm")) FVector DashboardSizeCm = FVector(5, 130, 40);
	UPROPERTY(EditAnywhere, Category = "Buck", meta = (Units = "cm")) double GroundHalfSizeCm = 200;
	UPROPERTY(EditAnywhere, Category = "Buck", meta = (Units = "cm", ClampMin = "1")) double GridSpacingCm = 25;
	UPROPERTY(EditAnywhere, Category = "Buck", meta = (Units = "cm")) double MarkerSizeCm = 4;
	UPROPERTY(EditAnywhere, Category = "Buck", meta = (Units = "cm")) FVector PanelOffsetCm = FVector(85, -75, 40);
	UPROPERTY(EditAnywhere, Category = "Buck", meta = (Units = "cm")) double PanelTextSizeCm = 1.4;
	UPROPERTY(EditAnywhere, Category = "Capture", meta = (Units = "s", ClampMin = "0.01")) double SaveHoldSeconds = 2;
	UPROPERTY(EditAnywhere, Category = "Capture", meta = (Units = "s", ClampMin = "0.01")) double MeasureSeconds = 0.5;
	UPROPERTY(EditAnywhere, Category = "Capture", meta = (ClampMin = "0.001", ClampMax = "1")) double MinHorizontalLength = 0.1;
	UPROPERTY(EditAnywhere, Category = "Capture", meta = (Units = "cm", ClampMin = "0")) double PlayAreaToleranceCm = 1;
	UPROPERTY(EditAnywhere, Category = "Capture") FKey SaveKey = EKeys::C;
	UPROPERTY(EditAnywhere, Category = "Capture") FKey MeasureKey = EKeys::M;
	UPROPERTY(EditAnywhere, Category = "Capture") FKey TipKey = EKeys::T;
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
private:
	friend struct FP0BuckTestAccess;
	UPROPERTY() TObjectPtr<APawn> AlignmentPawn;
	UPROPERTY() TObjectPtr<APlayerController> InputController;
	FVBoRBuckSavedAlignment Saved;
	FVBoRBuckCaptureWindow Window;
	FVBoRBuckHeadFrame CurrentFrame;
	FString SavePath;
	FString State = TEXT("No saved alignment");
	FString LastCapture = TEXT("No capture");
	FString LastMeasure = TEXT("No measurement");
	FString LastTips = TEXT("No fingertip check");
	FString LastPawn = TEXT("PlayerStart retained");
	FString LastRecenter = TEXT("never");
	FString LastEvent;
	bool bHasSaved = false;
	bool bNeedsCheck = false;
	bool bSaveHeld = false;
	int32 CaptureOrigin = 0;
	TFunction<double()> CaptureClock;
	double LastCaptureSampleSeconds = 0;
	int32 RecenterCount = 0;
	double LastLogSeconds = -1;
	FDelegateHandle RecenterHandle;
	FVBoRBuckHeadFrame ReadHead() const;
	void ProcessFrame(double DeltaSeconds, const FVBoRBuckHeadFrame& Frame);
	void LoadAndApply(const FVBoRBuckHeadFrame& Frame);
	void ApplySaved(const FVBoRBuckHeadFrame& Frame, bool bFreshSave);
	void StartSave();
	void ReleaseSave();
	void StartMeasure();
	void CheckTips();
	void HandleRecenter();
	void MarkNeedsCheck(const FString& Reason);
	void Record(const FString& Text);
	void UpdatePanel();
	void CreateGeometry();
	void AddBox(UStaticMesh* Cube, const FString& Name, const FVector& EyeCenter, const FVector& Size);
	FString DescribeDelta(const FVBoRBuckCaptureResult& Sample) const;
};
