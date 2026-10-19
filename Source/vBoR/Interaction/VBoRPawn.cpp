#include "VBoRPawn.h"

#include "Engine/SkeletalMesh.h"
#include "Interaction/IsdkGrabberComponent.h"
#include "IsdkContentAssetPaths.h"
#include "IsdkHandMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "Rig/IsdkHandVisualsRigComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogVBoRPawn, Log, All);

AVBoRPawn::AVBoRPawn()
{
	bAllowPalmGrab = true;
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	// 손 표시의 PrePhysics 매핑 뒤에 확인함 (P4f D3)
	PrimaryActorTick.TickGroup = TG_PostPhysics;
}

void AVBoRPawn::PreRegisterAllComponents()
{
	Super::PreRegisterAllComponents();
	// BP 복제 때 남은 CDO 부모 대신 자기 리그에 붙임 (중첩 ISDK 표시의 등록 오류 방지, P4f D5)
	for (int32 Hand = 0; Hand < 2; ++Hand)
	{
		auto* Rig = GetHandRig(Hand);
		if (!Rig->GetHandVisuals()->IsRegistered()) { Rig->GetHandVisuals()->SetupAttachment(Rig); }
	}
}

void AVBoRPawn::PreInitializeComponents()
{
	// 리그 초기화 전에 두 표시를 함께 바꿈 (poke limiter 전환 때 모양 유지, P4f D2)
	for (int32 Hand = 0; Hand < 2; ++Hand)
	{
		USkeletalMesh* Mesh = Hand == 0 ? LeftHandMesh.Get() : RightHandMesh.Get();
		UMaterialInterface* Material = Hand == 0 ? LeftHandMaterial.Get() : RightHandMaterial.Get();
		auto* Visuals = GetHandRig(Hand)->GetHandVisuals();
		for (auto* Visual : {Visuals->TrackedHandVisual.Get(), Visuals->SyntheticHandVisual.Get()})
		{
			if (Mesh)
			{
				Visual->SetSkinnedAssetAndUpdate(Mesh);
				Visual->ClearMappingState();
				Visual->EmptyOverrideMaterials();
			}
			if (Material)
			{
				for (int32 Slot = 0; Slot < Visual->GetNumMaterials(); ++Slot) { Visual->SetMaterial(Slot, Material); }
			}
		}
	}
	Super::PreInitializeComponents();
	// P3 의 기존 Pinch 허용 순서 유지 (부모가 먼저 Pinch 를 끔, P4f D1)
	if (bAllowPalmGrab)
	{
		for (int32 Hand = 0; Hand < 2; ++Hand) { GetHandRig(Hand)->GetGrabber()->SetGrabInputMethodAllowed(EIsdkGrabInputMethod::Pinch, true); }
	}
}

void AVBoRPawn::BeginPlay()
{
	Super::BeginPlay();
	CheckHandMeshMappings();
}

void AVBoRPawn::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	CheckHandMeshMappings();
}

void AVBoRPawn::CheckHandMeshMappings()
{
	bool bMappingPending = false;
	for (int32 Hand = 0; Hand < 2; ++Hand)
	{
		USkeletalMesh* Mesh = Hand == 0 ? LeftHandMesh.Get() : RightHandMesh.Get();
		if (!Mesh || bHandMeshFallback[Hand]) { continue; }
		auto* Visuals = GetHandRig(Hand)->GetHandVisuals();
		const auto Tracked = Visuals->TrackedHandVisual->GetMappingState();
		const auto Synthetic = Visuals->SyntheticHandVisual->GetMappingState();
		if (Tracked == EIsdkSkeletonMappingState::Invalid || Synthetic == EIsdkSkeletonMappingState::Invalid)
		{
			USkeletalMesh* DefaultMesh = LoadObject<USkeletalMesh>(nullptr, Hand == 0
				? IsdkContentAssetPaths::Models::Hand::OpenXRLeftHand : IsdkContentAssetPaths::Models::Hand::OpenXRRightHand);
			for (auto* Visual : {Visuals->TrackedHandVisual.Get(), Visuals->SyntheticHandVisual.Get()})
			{
				Visual->SetSkinnedAssetAndUpdate(DefaultMesh);
				Visual->EmptyOverrideMaterials();
				Visual->ClearMappingState();
			}
			// 실패한 손만 한 번 복구함 (반대 손 설정·보임 유지, P4f D3)
			bHandMeshFallback[Hand] = true;
			UE_LOG(LogVBoRPawn, Warning, TEXT("%s: %s hand mesh %s bones do not match XRHand_* - using default hand"),
				*GetName(), Hand == 0 ? TEXT("Left") : TEXT("Right"), *Mesh->GetName());
		}
		else if (Tracked == EIsdkSkeletonMappingState::None || Synthetic == EIsdkSkeletonMappingState::None)
		{
			bMappingPending = true;
		}
	}
	// 매핑이 끝나면 매 frame 확인할 필요 없음 (P4f D3)
	SetActorTickEnabled(bMappingPending);
}
