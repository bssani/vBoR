#pragma once

#include "CoreMinimal.h"
#include "VBoRHandRigPawn.h"
#include "VBoRPawn.generated.h"

class USkeletalMesh;
class UMaterialInterface;

UCLASS(Blueprintable)
class VBOR_API AVBoRPawn : public AVBoRHandRigPawn
{
	GENERATED_BODY()
public:
	AVBoRPawn();
	virtual void PreRegisterAllComponents() override;
	virtual void PreInitializeComponents() override;
	virtual void BeginPlay() override;

	UPROPERTY(EditAnywhere, Category = "손 표시")
	TObjectPtr<USkeletalMesh> LeftHandMesh;
	UPROPERTY(EditAnywhere, Category = "손 표시")
	TObjectPtr<USkeletalMesh> RightHandMesh;
	UPROPERTY(EditAnywhere, Category = "손 표시")
	TObjectPtr<UMaterialInterface> LeftHandMaterial;
	UPROPERTY(EditAnywhere, Category = "손 표시")
	TObjectPtr<UMaterialInterface> RightHandMaterial;

private:
	friend struct FP4fPawnTestAccess;
	void CheckHandMeshMappings();
	bool bHandMeshFallback[2] = {false, false};
};
