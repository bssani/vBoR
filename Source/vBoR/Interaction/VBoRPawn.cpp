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
}

void AVBoRPawn::PreRegisterAllComponents()
{
	Super::PreRegisterAllComponents();
	// Attach to own rig, not stale CDO parent after BP duplication (avoid nested ISDK visual registration error, P4f D5)
	for (int32 Hand = 0; Hand < 2; ++Hand)
	{
		auto* Rig = GetHandRig(Hand);
		if (!Rig->GetHandVisuals()->IsRegistered()) { Rig->GetHandVisuals()->SetupAttachment(Rig); }
	}
}

void AVBoRPawn::PreInitializeComponents()
{
	// Replace both visuals before rig init (same appearance across poke limiter switch, P4f D2)
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
	// Keep P3 Pinch enable order (parent disables Pinch first, P4f D1)
	if (bAllowPalmGrab)
	{
		for (int32 Hand = 0; Hand < 2; ++Hand) { GetHandRig(Hand)->GetGrabber()->SetGrabInputMethodAllowed(EIsdkGrabInputMethod::Pinch, true); }
	}
}

void AVBoRPawn::BeginPlay()
{
	Super::BeginPlay();
	// Visual BeginPlay ends mapping as Valid/Invalid (IsdkHandMeshComponent.cpp:161-164/205-224). No Actor Tick (preserve BP Event Tick, P4f review R1-1)
	CheckHandMeshMappings();
}

void AVBoRPawn::CheckHandMeshMappings()
{
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
			// Restore failed hand once only (preserve other hand settings/visibility, P4f D3)
			bHandMeshFallback[Hand] = true;
			UE_LOG(LogVBoRPawn, Warning, TEXT("%s: %s hand mesh %s bones do not match XRHand_* - using default hand"),
				*GetName(), Hand == 0 ? TEXT("Left") : TEXT("Right"), *Mesh->GetName());
		}
	}
}
