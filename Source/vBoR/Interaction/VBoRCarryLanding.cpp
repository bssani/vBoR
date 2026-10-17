#include "VBoRCarryObject.h"
#include "VBoRLoadTarget.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"

bool AVBoRCarryObject::CalculateReleaseDrop(FTransform& DropStart, FTransform& Landing, bool& bLevelBlocked) const
{
	DropStart = Landing = GetActorTransform();
	bLevelBlocked = false;
	if (!IsPoseClear(DropStart)) { return false; }
	if (bLevelOnRelease && !bUsePlacementAssist)
	{
		const FQuat Rest = Session.bHasRestRotation ? Session.RestRotation : FQuat::Identity;
		const FQuat Relative = DropStart.GetRotation() * Rest.Inverse();
		FTransform Level = DropStart;
		Level.SetRotation((FRotator(0, Relative.Rotator().Yaw, 0).Quaternion() * Rest).GetNormalized());
		if (!IsPoseClear(Level))
		{
			// 매 frame 질의를 줄이도록 +Z 만 1 cm 간격으로 찾고 경계를 0.001 cm 안으로 좁힘 (P1c R1-1)
			const FVector Origin = Level.GetLocation();
			const double MaxLift = FMath::IsFinite(LevelLiftMaxCm) ? FMath::Max(0.0, double(LevelLiftMaxCm)) : 0.0;
			double BlockedLift = 0;
			bool bFound = false;
			for (double Lift = FMath::Min(1.0, MaxLift); MaxLift > 0; Lift = FMath::Min(Lift + 1.0, MaxLift))
			{
				Level.SetLocation(Origin + FVector(0, 0, Lift));
				if (IsPoseClear(Level))
				{
					double ClearLift = Lift;
					while (ClearLift - BlockedLift > 0.001)
					{
						const double Middle = (ClearLift + BlockedLift) * 0.5;
						Level.SetLocation(Origin + FVector(0, 0, Middle));
						if (IsPoseClear(Level)) { ClearLift = Middle; }
						else { BlockedLift = Middle; }
					}
					Level.SetLocation(Origin + FVector(0, 0, ClearLift));
					bFound = true;
					break;
				}
				BlockedLift = Lift;
				if (Lift >= MaxLift) { break; }
			}
			bLevelBlocked = !bFound;
		}
		if (!bLevelBlocked) { DropStart = Level; }
	}

	// 실제 낙하와 같은 Mesh·접촉 여유 0.005 cm 사용 (P1c D3, 최대 100 m)
	constexpr double SweepLengthCm = 10000;
	const FVector Travel(0, 0, -SweepLengthCm);
	FComponentQueryParams Params(SCENE_QUERY_STAT(VBoRCarryLanding), this);
	TArray<FHitResult> Hits;
	GetWorld()->ComponentSweepMulti(Hits, Mesh, DropStart.GetLocation(), DropStart.GetLocation() + Travel, DropStart.GetRotation(), Params);
	double StopTime = 1.0;
	bool bHit = false;
	for (const FHitResult& Hit : Hits)
	{
		if (!Hit.bBlockingHit) { continue; }
		if (Hit.bStartPenetrating && FVector::DotProduct(Travel, Hit.Normal) >= 0 &&
			(Hit.PenetrationDepth <= 0.005 || FVector::DotProduct(Travel, Hit.Normal) > KINDA_SMALL_NUMBER)) { continue; }
		bHit = true;
		StopTime = FMath::Min(StopTime, FMath::Max(0.0, Hit.Time - 0.005 / SweepLengthCm));
	}
	Landing = DropStart;
	Landing.AddToTranslation(Travel * StopTime);
	return bHit && IsPoseClear(Landing);
}

void AVBoRCarryObject::UpdateLandingPreview()
{
	const bool bVisible = [this]()
	{
		if (!bShowLandingPreview || Session.Hand == INDEX_NONE || Session.bPaused || bUsePlacementAssist || !PreviewMaterial) { return false; }
		FTransform DropStart;
		FTransform Landing;
		bool bLevelBlocked = false;
		if (!CalculateReleaseDrop(DropStart, Landing, bLevelBlocked)) { return false; }
		const FBox Bounds = GetCollisionBoundsInPose(Landing, FTransform::Identity);
		if (!Bounds.IsValid || Bounds.Min.Z <= FloorHeightCm + FloorToleranceCm) { return false; }
		FTransform RegionFrame = LoadTarget ? LoadTarget->LoadingRegion->GetComponentTransform() : FTransform::Identity;
		RegionFrame.RemoveScaling();
		// CalculateReleaseDrop 이 착지 겹침까지 확인했으므로 영역만 판정함 (P1c R1-1)
		bPreviewInside = LoadTarget && LoadTarget->ContainsPose(this, Landing, RegionFrame, LoadTarget->LoadingRegion->GetScaledBoxExtent().GetAbs());
		PreviewMaterial->SetVectorParameterValue(TEXT("ColorParam"), bPreviewInside ? SeatedColor : ReleasedColor);
		LandingPreview->SetStaticMesh(Mesh->GetStaticMesh());
		for (int32 Slot = 0; Slot < LandingPreview->GetNumMaterials(); ++Slot)
		{
			if (LandingPreview->GetMaterial(Slot) != PreviewMaterial) { LandingPreview->SetMaterial(Slot, PreviewMaterial); }
		}
		LandingPreview->SetWorldTransform(Landing);
		return true;
	}();
	// 보임이 같으면 render state 재생성을 요청하지 않음 (P1c R1-1)
	if (LandingPreview->IsVisible() != bVisible) { LandingPreview->SetVisibility(bVisible); }
}
