#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VBoRBoxStand.generated.h"

class UStaticMeshComponent;
class AVBoRCarryObject;
class AVBoRLoadTarget;

UCLASS(Blueprintable)
class VBOR_API AVBoRBoxStand : public AActor
{
	GENERATED_BODY()
public:
	AVBoRBoxStand();
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void Tick(float DeltaSeconds) override;
	UFUNCTION(BlueprintCallable, Category="Box Stand") void ResetStand();
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Box Stand", meta=(Units="cm", ClampMin="1")) FVector StandSizeCm = FVector(60,60,5);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Box Stand", meta=(Units="cm")) float StandTopHeightCm = 75;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Box Stand", meta=(Units="cm", ClampMin="1")) FVector BoxSizeCm = FVector(45,35,30);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Box Stand") TSubclassOf<AVBoRCarryObject> BoxClass;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Box Stand") TObjectPtr<AVBoRLoadTarget> LoadTarget;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Box Stand") TObjectPtr<UStaticMeshComponent> Stand;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Box Stand") TObjectPtr<AVBoRCarryObject> SourceBox;
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
	UPROPERTY() TArray<TObjectPtr<AVBoRCarryObject>> Boxes;
	bool bSourceLifted = false;
	FTransform StartPose() const;
	void SpawnBox();
	void ClearBoxes();
	UFUNCTION() void HandleGrabbed(AVBoRCarryObject* Box);
	UFUNCTION() void HandleDestroyed(AActor* Box);
};
