#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VBoRObjectSpawnPoint.generated.h"

class AVBoRCarryObject;
class AVBoRLoadTarget;
class UArrowComponent;
class UBoxComponent;

UCLASS(Blueprintable)
class VBOR_API AVBoRObjectSpawnPoint : public AActor
{
	GENERATED_BODY()

public:
	AVBoRObjectSpawnPoint();
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION(BlueprintCallable, Category="Object Spawn")
	void ResetSpawn();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Object Spawn")
	TSubclassOf<AActor> SpawnClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Object Spawn")
	TObjectPtr<AVBoRLoadTarget> LoadTarget;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Object Spawn")
	TObjectPtr<AVBoRCarryObject> SourceBox;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
#if WITH_EDITORONLY_DATA
	UPROPERTY()
	TObjectPtr<UArrowComponent> SpawnDirection;

	UPROPERTY()
	TObjectPtr<UBoxComponent> SpawnBounds;
#endif

	UPROPERTY()
	TArray<TObjectPtr<AVBoRCarryObject>> Boxes;

	UPROPERTY()
	TObjectPtr<AActor> SpawnedActor;

	bool bSourceLifted = false;
	bool bWarnedMissingFloor = false;

	float ResolveFloorHeightCm();
	FTransform StartPose(const FVector& BoxSizeCm) const;
	void SpawnObject();
	void ClearSpawnedObjects();
	UFUNCTION() void HandleGrabbed(AVBoRCarryObject* Box);
	UFUNCTION() void HandleDestroyed(AActor* Box);
};
