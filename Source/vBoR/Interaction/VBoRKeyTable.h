#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VBoRKeyFob.h"
#include "VBoRKeyTable.generated.h"

class UStaticMeshComponent;

UCLASS(Blueprintable)
class VBOR_API AVBoRKeyTable : public AActor
{
	GENERATED_BODY()
public:
	AVBoRKeyTable();
	virtual void OnConstruction(const FTransform& Transform) override;
	FTransform GetKeyHomeTransform(const AVBoRKeyFob* Key) const;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Key Table") TObjectPtr<UStaticMeshComponent> TopMesh;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Key Table") TObjectPtr<UStaticMeshComponent> StandMesh;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Key Table") TObjectPtr<AVBoRKeyFob> Key;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Key Table") TSubclassOf<AVBoRKeyFob> KeyClass;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Approximation", meta=(Units="cm", ClampMin="1")) FVector TableSizeCm = FVector(45,45,80);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Approximation", meta=(Units="cm", ClampMin="0.1")) float TopThicknessCm = 3;
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
};
