#include "VBoRMotionGuideComponent.h"

#include "VBoRAxisPart.h"
#include "VBoRHingedPanel.h"
#include "VBoRPoweredPanel.h"
#include "VBoRGrabCollider.h"
#include "VBoRObstructionBoxComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "PhysicsEngine/BodySetup.h"
#include "PhysicsEngine/BodyInstance.h"

namespace
{
FVector JudgmentCorner(const FBox& B, int32 I)
{
	return FVector(I & 1 ? B.Max.X : B.Min.X, I & 2 ? B.Max.Y : B.Min.Y, I & 4 ? B.Max.Z : B.Min.Z);
}

void JudgmentBoxLines(TArray<FVBoRGuideLine>& Lines, const FBox& B, const FTransform& Frame, FColor Color, EVBoRGuideShape Kind)
{
	for (int32 I = 0; I < 8; ++I) for (int32 Axis = 0; Axis < 3; ++Axis)
	{
		const int32 Other = I ^ (1 << Axis);
		if (I < Other) { Lines.Add({Frame.TransformPosition(JudgmentCorner(B, I)), Frame.TransformPosition(JudgmentCorner(B, Other)), Color, Kind}); }
	}
}

void JudgmentCircleLines(TArray<FVBoRGuideLine>& Lines, const FTransform& Frame, FVector U, FVector V, double Radius, FVector Center = FVector::ZeroVector)
{
	constexpr int32 Segments = 24;
	for (int32 I = 0; I < Segments; ++I)
	{
		auto Point = [&](int32 Step)
		{
			const double A = 2 * UE_DOUBLE_PI * Step / Segments;
			return Frame.TransformPosition(Center + Radius * (U * FMath::Cos(A) + V * FMath::Sin(A)));
		};
		Lines.Add({Point(I), Point(I + 1), FColor::Cyan, EVBoRGuideShape::GrabMesh});
	}
}

void JudgmentMeshGrabLines(TArray<FVBoRGuideLine>& Lines, const UStaticMeshComponent* Mesh)
{
	const UBodySetup* Body = Mesh->GetStaticMesh()->GetBodySetup();
	const FTransform Frame = Mesh->GetComponentTransform();
	if (Body->GetCollisionTraceFlag() == CTF_UseComplexAsSimple)
	{
		// Bounds only for triangle collision (R2c D1 complex approximation)
		JudgmentBoxLines(Lines, Mesh->GetStaticMesh()->GetBoundingBox(), Frame, FColor::Magenta, EVBoRGuideShape::GrabMesh);
		return;
	}
	const FVector Scale = Frame.GetScale3D();
	const FTransform Rigid(Frame.GetRotation(), Frame.GetLocation());
	for (const FKBoxElem& Elem : Body->AggGeom.BoxElems)
	{
		const FKBoxElem B = Elem.GetFinalScaled(Scale, FTransform::Identity);
		const FVector Extent = FVector(B.X, B.Y, B.Z) / 2;
		JudgmentBoxLines(Lines, FBox(-Extent, Extent), B.GetTransform() * Rigid, FColor::Cyan, EVBoRGuideShape::GrabMesh);
	}
	for (const FKSphereElem& Elem : Body->AggGeom.SphereElems)
	{
		const FKSphereElem S = Elem.GetFinalScaled(Scale, FTransform::Identity);
		const FTransform SphereFrame(FQuat::Identity, S.Center);
		JudgmentCircleLines(Lines, SphereFrame * Rigid, FVector::XAxisVector, FVector::YAxisVector, S.Radius);
		JudgmentCircleLines(Lines, SphereFrame * Rigid, FVector::XAxisVector, FVector::ZAxisVector, S.Radius);
		JudgmentCircleLines(Lines, SphereFrame * Rigid, FVector::YAxisVector, FVector::ZAxisVector, S.Radius);
	}
	for (const FKSphylElem& Elem : Body->AggGeom.SphylElems)
	{
		const FKSphylElem S = Elem.GetFinalScaled(Scale, FTransform::Identity);
		const FTransform CapsuleFrame = S.GetTransform() * Rigid;
		for (double Sign : {-1., 1.})
		{
			const FVector Center(0, 0, Sign * S.Length / 2);
			JudgmentCircleLines(Lines, CapsuleFrame, FVector::XAxisVector, FVector::YAxisVector, S.Radius, Center);
			for (FVector Radial : {FVector::XAxisVector, FVector::YAxisVector}) for (int32 I = 0; I < 12; ++I)
			{
				auto Point = [&](int32 Step)
				{
					const double A = UE_DOUBLE_PI * Step / 12;
					return CapsuleFrame.TransformPosition(Center + S.Radius * (Radial * FMath::Cos(A) + FVector::ZAxisVector * Sign * FMath::Sin(A)));
				};
				Lines.Add({Point(I), Point(I + 1), FColor::Cyan, EVBoRGuideShape::GrabMesh});
			}
		}
		for (const FVector& Radial : {FVector::XAxisVector, -FVector::XAxisVector, FVector::YAxisVector, -FVector::YAxisVector})
		{
			Lines.Add({CapsuleFrame.TransformPosition(Radial * S.Radius + FVector(0, 0, S.Length / 2)),
				CapsuleFrame.TransformPosition(Radial * S.Radius - FVector(0, 0, S.Length / 2)), FColor::Cyan, EVBoRGuideShape::GrabMesh});
		}
	}
	for (const FKConvexElem& Elem : Body->AggGeom.ConvexElems)
	{
		const FTransform HullFrame = Elem.GetTransform() * Frame;
		// Authored hull triangles only; no render-mesh traversal (R2c §5)
		for (int32 I = 0; I + 2 < Elem.IndexData.Num(); I += 3) for (int32 Edge = 0; Edge < 3; ++Edge)
		{
			const int32 A = Elem.IndexData[I + Edge], B = Elem.IndexData[I + (Edge + 1) % 3];
			if (Elem.VertexData.IsValidIndex(A) && Elem.VertexData.IsValidIndex(B))
			{
				Lines.Add({HullFrame.TransformPosition(Elem.VertexData[A]), HullFrame.TransformPosition(Elem.VertexData[B]), FColor::Cyan, EVBoRGuideShape::GrabMesh});
			}
		}
	}
}
}

void UVBoRMotionGuideComponent::AppendJudgmentShapes(TArray<FVBoRGuideLine>& Lines) const
{
	const auto* AxisPart = Cast<AVBoRAxisPart>(GetOwner());
	const auto* Panel = Cast<AVBoRHingedPanel>(GetOwner());
	const auto* Powered = Cast<AVBoRPoweredPanel>(GetOwner());
	const EVBoRMotionGuide Mode = AxisPart ? AxisPart->JudgmentGuide : Panel ? Panel->JudgmentGuide : Powered ? Powered->JudgmentGuide : EVBoRMotionGuide::Off;
	if (Mode == EVBoRMotionGuide::Off) { return; }
#if WITH_EDITOR
	if (Mode == EVBoRMotionGuide::SelectedOnly && !GetOwner()->IsSelected() && !IsSelected()) { return; }
#endif
	if (AxisPart)
	{
		if (VBoRUsesGrabMesh(AxisPart->PartMesh, AxisPart->bGrabWithMesh)) { JudgmentMeshGrabLines(Lines, AxisPart->PartMesh); }
		else
		{
			const FVector Extent = AxisPart->GrabBox->GetUnscaledBoxExtent();
			JudgmentBoxLines(Lines, FBox(-Extent, Extent), AxisPart->GrabBox->GetComponentTransform(), AxisPart->bGrabWithMesh ? FColor::Magenta : FColor::Cyan, EVBoRGuideShape::GrabBox);
		}
		if (!AxisPart->IsDoorGrip() && AxisPart->bAllowPush && AxisPart->Movement == EVBoRAxisMovement::Rotate && AxisPart->PartMesh->GetStaticMesh())
		{
			FBox FaceBounds; FVector Radial, Normal; AxisPart->PushGeometry(FaceBounds, Radial, Normal);
			FMatrix Basis = FMatrix::Identity; Basis.SetAxes(&Radial, &Normal);
			Basis.SetAxis(2, AxisPart->LocalAxis());
			JudgmentBoxLines(Lines, FaceBounds, FTransform(Basis) * AxisPart->AxisPivot->GetComponentTransform(), FColor::Blue, EVBoRGuideShape::PushPlate);
		}
	}
	else if (Panel)
	{
		const FBox FaceBounds = Panel->PushFaceBounds();
		const FTransform Frame = Panel->HingePivot->GetComponentTransform();
		// Preview moves render caches only; trace the unchanged physics pose (R2a D8, R2c D6)
		FTransform QueryFrame = Frame;
		if (Panel->bPushOnMeshSurface)
		{
			FTransform MeshPhysics = Panel->DoorMesh->BodyInstance.GetUnrealWorldTransform();
			MeshPhysics.SetScale3D(Panel->DoorMesh->GetComponentScale());
			QueryFrame = Panel->DoorMesh->GetRelativeTransform().Inverse() * MeshPhysics;
		}
		for (int32 Face = 0; Face < 2; ++Face)
		{
			if (FVBoRDoorMotion::PushOpens(Face) && !Panel->bAllowPalmOpening) { continue; }
			bool HitAny = false;
			if (Panel->bPushOnMeshSurface)
			{
				constexpr int32 Size = 8;
				FVector Points[Size][Size]; bool Hits[Size][Size] = {};
				for (int32 X = 0; X < Size; ++X) for (int32 Z = 0; Z < Size; ++Z)
				{
					FVector P(FMath::Lerp(FaceBounds.Min.X, FaceBounds.Max.X, (X + .5) / Size), 0, FMath::Lerp(FaceBounds.Min.Z, FaceBounds.Max.Z, (Z + .5) / Size));
					Hits[X][Z] = Panel->QueryMeshFace(P, Face, P.Y, QueryFrame);
					HitAny |= Hits[X][Z]; Points[X][Z] = Frame.TransformPosition(P);
				}
				for (int32 X = 0; X < Size; ++X) for (int32 Z = 0; Z < Size; ++Z)
				{
					if (!Hits[X][Z]) { continue; }
					if (X + 1 < Size && Hits[X + 1][Z]) { Lines.Add({Points[X][Z], Points[X + 1][Z], FColor::Blue, EVBoRGuideShape::PushFace}); }
					if (Z + 1 < Size && Hits[X][Z + 1]) { Lines.Add({Points[X][Z], Points[X][Z + 1], FColor::Blue, EVBoRGuideShape::PushFace}); }
				}
			}
			if (!HitAny)
			{
				const double Y = Face == 0 ? FaceBounds.Max.Y : FaceBounds.Min.Y;
				const FVector Corners[] = {{FaceBounds.Min.X, Y, FaceBounds.Min.Z}, {FaceBounds.Max.X, Y, FaceBounds.Min.Z}, {FaceBounds.Max.X, Y, FaceBounds.Max.Z}, {FaceBounds.Min.X, Y, FaceBounds.Max.Z}};
				for (int32 I = 0; I < 4; ++I)
				{
					Lines.Add({Frame.TransformPosition(Corners[I]), Frame.TransformPosition(Corners[(I + 1) % 4]), Panel->bPushOnMeshSurface ? FColor::Magenta : FColor::Blue, EVBoRGuideShape::PushFace});
				}
			}
		}
	}
	else if (Powered)
	{
		TArray<UVBoRObstructionBoxComponent*> Components; Powered->GetComponents(Components);
		Components.RemoveAll([Powered](const auto* Box) { return Powered->ObstructionBoxProblem(Box) != nullptr; });
		if (Components.IsEmpty() && !Powered->PanelMesh->GetStaticMesh()) { return; }
		const float End = FMath::Clamp(Powered->OpenAngleDeg, 1.f, 170.f);
		const int32 Steps = FMath::CeilToInt(End / 10.f);
		for (int32 Step = 0; Step <= Steps; ++Step)
		{
			auto Add = [&](const UVBoRObstructionBoxComponent* Box)
			{
				const auto Shape = Powered->ObstructionShapeAt(End * Step / Steps, Box);
				JudgmentBoxLines(Lines, FBox(-Shape.Extent, Shape.Extent), FTransform(Shape.Rotation, Shape.Center), Box ? FColor::Orange : FColor::Magenta, EVBoRGuideShape::Obstruction);
			};
			if (Components.IsEmpty()) { Add(nullptr); }
			else { for (const auto* Box : Components) { Add(Box); } }
		}
	}
}
