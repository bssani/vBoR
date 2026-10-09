#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/IsdkIInteractableState.h"
#include "Interaction/Pointable/IsdkInteractionPointerEvent.h"
#include "VBoRPokeButton.generated.h"

class UStaticMeshComponent;
class UIsdkPokeInteractable;
class UIsdkPointablePlane;
class UIsdkClippedPlaneSurface;
class UIsdkPokeButtonVisual;
class UIsdkPointerEventAudioPlayer;
class UAudioComponent;
class UMaterialInstanceDynamic;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FVBoRPokeButtonEvent, const FIsdkInteractionPointerEvent&, Event);

UCLASS(Blueprintable)
class VBOR_API AVBoRPokeButton : public AActor
{
	GENERATED_BODY()
public:
	AVBoRPokeButton();
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void PreInitializeComponents() override;
	FVector GetRestFrontWorld() const;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Poke Button")
	TObjectPtr<UStaticMeshComponent> ButtonMesh;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Poke Button")
	TObjectPtr<UStaticMeshComponent> BaseMesh;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Poke Button")
	TObjectPtr<UIsdkPokeInteractable> Poke;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Poke Button")
	TObjectPtr<UIsdkPointablePlane> Plane;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Poke Button")
	TObjectPtr<UIsdkClippedPlaneSurface> Surface;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Poke Button")
	TObjectPtr<UIsdkPokeButtonVisual> ButtonVisual;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Poke Button")
	TObjectPtr<UIsdkPointerEventAudioPlayer> PointerAudio;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Poke Button")
	TObjectPtr<UAudioComponent> PressAudio;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Poke Button", meta = (Units = "mm", ClampMin = "0.01"))
	float PressDepthMm = 3.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Poke Button", meta = (ClampMin = "0.01"))
	float HitAreaScale = 1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Poke Button")
	FLinearColor NormalColor = FLinearColor(0.15f, 0.2f, 0.25f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Poke Button")
	FLinearColor HoverColor = FLinearColor(0.0f, 0.6f, 1.0f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Poke Button")
	FLinearColor SelectColor = FLinearColor(0.0f, 1.0f, 0.2f);
	UPROPERTY(BlueprintAssignable, Category = "Poke Button")
	FVBoRPokeButtonEvent OnButtonPressed;
	UPROPERTY(BlueprintAssignable, Category = "Poke Button")
	FVBoRPokeButtonEvent OnButtonReleased;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
private:
	FVector RestFront = FVector::ZeroVector;
	UPROPERTY()
	TArray<TObjectPtr<UMaterialInstanceDynamic>> ColorMaterials;
	UFUNCTION()
	void HandleState(const FIsdkInteractableStateEvent& Event);
	UFUNCTION()
	void HandlePointer(const FIsdkInteractionPointerEvent& Event);
};
