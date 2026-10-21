#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/IsdkIInteractableState.h"
#include "Interaction/Pointable/IsdkInteractionPointerEvent.h"
#include "VBoRSpectatorPart.h"
#include "VBoRSoundCue.h"
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
DECLARE_DELEGATE_RetVal_OneParam(bool, FVBoRPokePressFilter, const FIsdkInteractionPointerEvent&);

UCLASS(Blueprintable)
class VBOR_API AVBoRPokeButton : public AActor, public IVBoRSpectatorPart
{
	GENERATED_BODY()
public:
	AVBoRPokeButton();
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Poke Button|Sound") FVBoRSoundCue PressedCue;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Poke Button|Sound") FVBoRSoundCue ReleasedCue;
	// 칸 순서는 위 프로퍼티 순서와 같음 (P5s D12)
#if WITH_DEV_AUTOMATION_TESTS
	const FVBoRSoundPlayback& GetSoundPlaybackForTest(int32 Slot) const { return SoundPlayback[Slot]; }
	UAudioComponent* GetSoundAudioForTest(int32 Slot) const { return SoundAudio[Slot]; }
	uint64 GetSoundFrameForTest() const { return SoundFrame; }
#endif

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spectator") FText SpectatorName;
	virtual FText GetSpectatorName() const override { return SpectatorName; }
	virtual bool IsSpectatorInteracting() const override;

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void PreInitializeComponents() override;
	virtual void Tick(float DeltaSeconds) override;
	FVector GetRestFrontWorld() const;
	// 키의 쥔 손·테이블 위 눌림을 진행기에도 보내지 않음. 미연결 버튼은 기존 동작 (P4a D3·D8)
	FVBoRPokePressFilter PressFilter;

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
	FLinearColor HoverColor = FLinearColor(0.0f, 0.6f, 1.0f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Poke Button")
	FLinearColor SelectColor = FLinearColor(0.0f, 1.0f, 0.2f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Poke Button", meta = (ClampMin = "0", ClampMax = "1"))
	float HoverOpacity = 0.35f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Poke Button", meta = (ClampMin = "0", ClampMax = "1"))
	float SelectOpacity = 0.6f;
	UPROPERTY(BlueprintAssignable, Category = "Poke Button")
	FVBoRPokeButtonEvent OnButtonPressed;
	UPROPERTY(BlueprintAssignable, Category = "Poke Button")
	FVBoRPokeButtonEvent OnButtonReleased;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
private:
#if WITH_DEV_AUTOMATION_TESTS
	friend struct FP5sTestAccess;
#endif
	UPROPERTY() TArray<TObjectPtr<UAudioComponent>> SoundAudio;
	FVBoRSoundPlayback SoundPlayback[2];
	uint64 SoundFrame = 0;

	FVector RestFront = FVector::ZeroVector;
	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> StateOverlay;
	UFUNCTION()
	void HandleState(const FIsdkInteractableStateEvent& Event);
	UFUNCTION()
	void HandlePointer(const FIsdkInteractionPointerEvent& Event);
};
