#include "P0BuckGroundGridComponent.h"
#include "MeshElementCollector.h"
#include "PrimitiveDrawInterface.h"
#include "PrimitiveSceneProxy.h"
#include "PrimitiveViewRelevance.h"
#include "SceneView.h"

namespace
{
	class FP0BuckGroundGridProxy final : public FPrimitiveSceneProxy
	{
	public:
		explicit FP0BuckGroundGridProxy(const UP0BuckGroundGridComponent* Component)
			: FPrimitiveSceneProxy(Component), Lines(Component->BuildLines()), Settings(Component->GetSettings())
		{
			bWillEverBeLit = false;
		}
		virtual SIZE_T GetTypeHash() const override
		{
			static size_t Unique;
			return reinterpret_cast<SIZE_T>(&Unique);
		}
		virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily&, uint32 VisibilityMap, FMeshElementCollector& Collector) const override
		{
			for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ++ViewIndex)
			{
				if (!(VisibilityMap & (1u << ViewIndex))) { continue; }
				FPrimitiveDrawInterface* PDI = Collector.GetPDI(ViewIndex);
				for (const FP0BuckGridLine& Line : Lines)
				{
					// Keep actual Z=0 cm; small drawing bias for coplanar floor only (P0-c F1)
					PDI->DrawLine(GetLocalToWorld().TransformPosition(Line.Start), GetLocalToWorld().TransformPosition(Line.End),
						Settings.Color, SDPG_World, Settings.ThicknessPx, Settings.DepthBias, true);
				}
			}
		}
		virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override
		{
			FPrimitiveViewRelevance Result;
			Result.bDrawRelevance = IsShown(View);
			Result.bDynamicRelevance = true;
			Result.bRenderInMainPass = ShouldRenderInMainPass();
			Result.bOpaque = true;
			return Result;
		}
		// Skip whole-plane floor occlusion query, keep per-line world depth test (P0-c F1)
		virtual bool CanBeOccluded() const override { return false; }
		virtual uint32 GetMemoryFootprint() const override { return sizeof(*this) + GetAllocatedSize() + Lines.GetAllocatedSize(); }
	private:
		const TArray<FP0BuckGridLine> Lines;
		const FP0BuckGridSettings Settings;
	};
}

UP0BuckGroundGridComponent::UP0BuckGroundGridComponent()
{
	SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetGenerateOverlapEvents(false);
	SetCastShadow(false);
}

void UP0BuckGroundGridComponent::Configure(double HalfSizeCm, double SpacingCm, const FLinearColor& Color, float ThicknessPx, float DepthBias)
{
	Settings.HalfSizeCm = FMath::IsFinite(HalfSizeCm) ? FMath::Max(0.0, HalfSizeCm) : 0;
	Settings.SpacingCm = FMath::IsFinite(SpacingCm) ? SpacingCm : 0;
	Settings.Color = Color;
	Settings.ThicknessPx = FMath::IsFinite(ThicknessPx) ? FMath::Clamp(ThicknessPx, 0.5f, 10.f) : 2;
	Settings.DepthBias = FMath::IsFinite(DepthBias) ? FMath::Clamp(DepthBias, 0.f, 0.0001f) : 0.00001f;
	UpdateBounds();
	MarkRenderStateDirty();
}

TArray<FP0BuckGridLine> UP0BuckGroundGridComponent::BuildLines() const
{
	TArray<FP0BuckGridLine> Lines;
	if (Settings.HalfSizeCm <= 0 || Settings.SpacingCm <= 0) { return Lines; }
	const double Half = Settings.HalfSizeCm;
	const int32 Count = static_cast<int32>(FMath::Min(100.0, FMath::FloorToDouble(Half / Settings.SpacingCm)));
	for (int32 Index = -Count; Index <= Count; ++Index)
	{
		const double Offset = Index * Settings.SpacingCm;
		Lines.Add({ FVector(-Half, Offset, 0), FVector(Half, Offset, 0) });
		Lines.Add({ FVector(Offset, -Half, 0), FVector(Offset, Half, 0) });
	}
	return Lines;
}

FPrimitiveSceneProxy* UP0BuckGroundGridComponent::CreateSceneProxy() { return new FP0BuckGroundGridProxy(this); }

FBoxSphereBounds UP0BuckGroundGridComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	// Z 1 cm expands culling bounds only, line reference height fixed at 0 cm (P0-c F1)
	return FBoxSphereBounds(FBox(FVector(-Settings.HalfSizeCm, -Settings.HalfSizeCm, -1), FVector(Settings.HalfSizeCm, Settings.HalfSizeCm, 1))).TransformBy(LocalToWorld);
}
