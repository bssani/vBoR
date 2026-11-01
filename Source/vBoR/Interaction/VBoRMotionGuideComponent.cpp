#include "VBoRMotionGuideComponent.h"

#include "VBoRAxisPart.h"
#include "VBoRHingedPanel.h"
#include "VBoRPoweredPanel.h"
#include "VBoRFoldingMirror.h"
#include "VBoRFoldingSeat.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"

namespace
{
FVector Corner(const FBox& Box, int32 I)
{
	return FVector(I & 1 ? Box.Max.X : Box.Min.X, I & 2 ? Box.Max.Y : Box.Min.Y, I & 4 ? Box.Max.Z : Box.Min.Z);
}

void SetCachedPose(USceneComponent* Component, const FTransform& Pose)
{
	if (Component->GetComponentTransform().Equals(Pose, 1.e-8)) { return; }
	// ComponentToWorld is an unserialized cache; relative pose stays authored (R2a D8)
	Component->SetComponentToWorld(Pose);
	Component->UpdateBounds();
	Component->MarkRenderTransformDirty();
	Component->UpdateChildTransforms(EUpdateTransformFlags::SkipPhysicsUpdate);
}
}

TArray<FVBoRGuideLine> VBoRMotionGuide::Build(const FVBoRMotionGuideInput& Input)
{
	TArray<FVBoRGuideLine> Lines;
	if (!Input.PoseAt) { return Lines; }
	auto Add = [&](const FVector& A, const FVector& B, FColor Color, EVBoRGuideShape Kind, bool Arrow = false, float Thickness = 1)
	{
		Lines.Add({A, B, Color, Kind, Thickness, Arrow});
	};
	const FTransform Zero = Input.PoseAt(0);
	auto PointAt = [&](double Value, const FVector& P) { return (Input.PoseAt(Value) * Input.Frame).TransformPosition(P); };
	if (Input.bRotate)
	{
		const FVector Axis = (Input.PoseAt(1).GetRotation() * Zero.GetRotation().Inverse()).GetRotationAxis().GetSafeNormal();
		FVector Reference = FVector::VectorPlaneProject(FVector::XAxisVector, Axis).GetSafeNormal();
		if (Reference.IsNearlyZero()) { Reference = FVector::VectorPlaneProject(FVector::YAxisVector, Axis).GetSafeNormal(); }
		Reference *= 5;
		double Low = -10, High = 10, Radius = 5;
		if (Input.MeshBounds.IsValid)
		{
			Low = MAX_dbl; High = -MAX_dbl;
			for (int32 I = 0; I < 8; ++I)
			{
				const FVector P = Corner(Input.MeshBounds, I);
				const double Along = FVector::DotProduct(P, Axis);
				Low = FMath::Min(Low, Along); High = FMath::Max(High, Along);
				const double R = FVector::VectorPlaneProject(P, Axis).Length();
				if (R > Radius) { Radius = R; Reference = P; }
			}
		}
		Add(Input.Frame.TransformPosition(Axis * Low), Input.Frame.TransformPosition(Axis * High), FColor::Yellow, EVBoRGuideShape::Axis);
		const FVector Center = Axis * FVector::DotProduct(Reference, Axis);
		const FVector Radial = (Reference - Center).GetSafeNormal();
		Add(PointAt(0, Reference - Radial * FMath::Min(5., Radius)), PointAt(0, Reference), FColor::Red, EVBoRGuideShape::Rest);
		constexpr int32 Segments = 32;
		for (int32 I = 0; I < Segments; ++I)
		{
			Add(PointAt(FMath::Lerp(Input.First, Input.Last, I / double(Segments)), Reference),
				PointAt(FMath::Lerp(Input.First, Input.Last, (I + 1) / double(Segments)), Reference), FColor::Green, EVBoRGuideShape::Arc, I == Segments - 1);
		}
		for (int32 I = 0; I < Input.Detents.Num(); ++I)
		{
			Add(PointAt(Input.Detents[I], Reference - Radial * 2), PointAt(Input.Detents[I], Reference + Radial * 2), FColor::White,
				EVBoRGuideShape::Detent, false, I == Input.StartDetent ? 3 : 1);
		}
	}
	else
	{
		const FVector Start = PointAt(0, FVector::ZeroVector), End = PointAt(Input.Last, FVector::ZeroVector);
		Add(Start, End, FColor::Green, EVBoRGuideShape::Travel, true);
		FVector Side = FVector::CrossProduct((End - Start).GetSafeNormal(), FVector::UpVector).GetSafeNormal();
		if (Side.IsNearlyZero()) { Side = FVector::RightVector; }
		Add(Start - Side * 2, Start + Side * 2, FColor::Red, EVBoRGuideShape::Rest);
	}
	for (const FVBoRGuidePush& Push : Input.Pushes)
	{
		const FVector Center = Input.Frame.TransformPosition(Push.Center);
		const FVector Inward = Input.Frame.TransformVectorNoScale(Push.Inward).GetSafeNormal();
		Add(Center - Inward * 8, Center, Push.bOpening ? FColor::Green : FColor::Red, EVBoRGuideShape::Push, true);
	}
	return Lines;
}

UVBoRMotionGuideComponent::UVBoRMotionGuideComponent()
{
	bIsEditorOnly = true;
	bTickInEditor = true;
	SetHiddenInGame(true);
	SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetGenerateOverlapEvents(false);
	SetCanEverAffectNavigation(false);
	SetCastShadow(false);
}

bool UVBoRMotionGuideComponent::ReadSource(FVBoRMotionGuideInput& Input, USceneComponent*& Pivot, double& PreviewValue, double& EditorValue, EVBoRMotionGuide& Mode) const
{
	const AActor* Owner = GetOwner();
	if (!Owner) { return false; }
	UStaticMeshComponent* Mesh = nullptr;
	FTransform Rest;
	if (const auto* HingedPart = Cast<AVBoRHingedPanel>(Owner))
	{
		Pivot = HingedPart->HingePivot; Mesh = HingedPart->DoorMesh; Rest = Pivot->GetRelativeTransform();
		Input.Last = HingedPart->MaxAngleDeg; PreviewValue = HingedPart->Preview; Mode = HingedPart->MotionGuide;
		Input.PoseAt = [](double Value) { return FTransform(AVBoRHingedPanel::RotationAt(Value, FQuat::Identity)); };
		const FBox FaceBounds = HingedPart->PushFaceBounds();
		for (int32 Face = 0; Face < 2; ++Face)
		{
			const bool Opening = FVBoRDoorMotion::PushOpens(Face);
			if (Opening && !HingedPart->bAllowPalmOpening) { continue; }
			FVector Center = FaceBounds.GetCenter(); Center.Y = Face == 0 ? FaceBounds.Max.Y : FaceBounds.Min.Y;
			double SurfaceY;
			if (HingedPart->bPushOnMeshSurface && HingedPart->QueryMeshFace(Center, Face, SurfaceY)) { Center.Y = SurfaceY; }
			Input.Pushes.Add({Center, FVBoRDoorMotion::PushInward(Face), Opening});
		}
	}
	else if (const auto* AxisPart = Cast<AVBoRAxisPart>(Owner))
	{
		Pivot = AxisPart->AxisPivot; Mesh = AxisPart->PartMesh; Rest = Pivot->GetRelativeTransform();
		Input.bRotate = AxisPart->Movement == EVBoRAxisMovement::Rotate;
		Input.Last = AxisPart->Limit(); PreviewValue = AxisPart->IsDoorGrip() ? 0 : AxisPart->Preview; Mode = AxisPart->MotionGuide;
		Input.PoseAt = [AxisPart](double Value) { return AxisPart->ValueTransform(Value); };
		if (AxisPart->Release == EVBoRAxisRelease::Detent && AxisPart->DetentProblem().IsEmpty())
		{
			Input.Detents = AxisPart->DetentAnglesDeg; Input.StartDetent = AxisPart->DetentStartIndex;
			Input.First = Input.Detents[0]; Input.Last = Input.Detents.Last();
		}
		if (AxisPart->bAllowPush && Input.bRotate && Mesh->GetStaticMesh() && (AxisPart->Release != EVBoRAxisRelease::Detent || AxisPart->DetentProblem().IsEmpty()))
		{
			FBox FaceBounds; FVector Radial, Normal; AxisPart->PushGeometry(FaceBounds, Radial, Normal);
			for (int32 Face = 0; Face < 2; ++Face)
			{
				FVector P = FaceBounds.GetCenter(); P.Y = Face == 0 ? FaceBounds.Min.Y : FaceBounds.Max.Y;
				const double Direction = AVBoRAxisPart::PushAngularDirection(Face);
				Input.Pushes.Add({Radial * P.X + Normal * P.Y + AxisPart->LocalAxis() * P.Z, Normal * Direction, Direction * (AxisPart->bReverse ? -1 : 1) > 0});
			}
		}
	}
	else if (const auto* PoweredPart = Cast<AVBoRPoweredPanel>(Owner))
	{
		Pivot = PoweredPart->HingePivot; Mesh = PoweredPart->PanelMesh; Rest = PoweredPart->HingePoseAt(0);
		Input.Last = FMath::Clamp(PoweredPart->OpenAngleDeg, 1.f, 170.f); PreviewValue = PoweredPart->Preview; Mode = PoweredPart->MotionGuide;
		EditorValue = PoweredPart->bStartOpen ? Input.Last : 0;
		Input.PoseAt = [PoweredPart, Rest](double Value) { return PoweredPart->HingePoseAt(Value).GetRelativeTransform(Rest); };
	}
	else if (const auto* MirrorPart = Cast<AVBoRFoldingMirror>(Owner))
	{
		Pivot = MirrorPart->HingePivot; Mesh = MirrorPart->MirrorMesh; Rest = MirrorPart->HingePoseAt(0);
		Input.Last = FMath::Clamp(MirrorPart->FoldAngleDeg, -170.f, 170.f); PreviewValue = MirrorPart->Preview; Mode = MirrorPart->MotionGuide;
		Input.PoseAt = [MirrorPart, Rest](double Value) { return MirrorPart->HingePoseAt(Value).GetRelativeTransform(Rest); };
	}
	else if (const auto* SeatPart = Cast<AVBoRFoldingSeat>(Owner))
	{
		Pivot = SeatPart->HingePivot; Mesh = SeatPart->SeatMesh; Rest = SeatPart->HingePoseAt(0);
		Input.Last = FMath::Clamp(SeatPart->FoldAngleDeg, 1.f, 170.f); PreviewValue = SeatPart->Preview; Mode = SeatPart->MotionGuide;
		Input.PoseAt = [SeatPart, Rest](double Value) { return SeatPart->HingePoseAt(Value).GetRelativeTransform(Rest); };
	}
	else { return false; }
	if (!Pivot || !Mesh) { return false; }
	Input.Frame = Rest * Pivot->GetAttachParent()->GetComponentTransform();
	if (Mesh->GetStaticMesh()) { Input.MeshBounds = Mesh->GetStaticMesh()->GetBoundingBox().TransformBy(Mesh->GetRelativeTransform()); }
	const double Alpha = FMath::IsFinite(PreviewValue) ? FMath::Clamp(PreviewValue, 0., 1.) : 0;
	const FTransform DisplayPose = Input.PoseAt(Alpha > 0 ? FMath::Lerp(Input.First, Input.Last, Alpha) : EditorValue);
	for (FVBoRGuidePush& Push : Input.Pushes)
	{
		Push.Center = DisplayPose.TransformPosition(Push.Center);
		Push.Inward = DisplayPose.TransformVectorNoScale(Push.Inward);
	}
	return true;
}

TArray<FVBoRGuideLine> UVBoRMotionGuideComponent::BuildShapes() const
{
	if (!GetWorld() || GetWorld()->IsGameWorld()) { return {}; }
	FVBoRMotionGuideInput Input; USceneComponent* Pivot = nullptr;
	double PreviewValue = 0, EditorValue = 0; EVBoRMotionGuide Mode = EVBoRMotionGuide::Off;
	if (!ReadSource(Input, Pivot, PreviewValue, EditorValue, Mode)) { return {}; }
	bool ShowMotion = Mode != EVBoRMotionGuide::Off;
	if (const auto* Part = Cast<AVBoRAxisPart>(GetOwner()); Part && Part->IsDoorGrip()) { ShowMotion = false; }
#if WITH_EDITOR
	if (Mode == EVBoRMotionGuide::SelectedOnly && !GetOwner()->IsSelected() && !IsSelected()) { ShowMotion = false; }
#endif
	TArray<FVBoRGuideLine> Lines = ShowMotion ? VBoRMotionGuide::Build(Input) : TArray<FVBoRGuideLine>();
	AppendJudgmentShapes(Lines);
	return Lines;
}

void UVBoRMotionGuideComponent::ClearPreview()
{
	FVBoRMotionGuideInput Input; USceneComponent* Pivot = nullptr;
	double PreviewValue = 0, EditorValue = 0; EVBoRMotionGuide Mode;
	if (ReadSource(Input, Pivot, PreviewValue, EditorValue, Mode))
	{
		// Restore from authored relatives, including game-world copies (R2a D9)
		Pivot->UpdateComponentToWorld(EUpdateTransformFlags::SkipPhysicsUpdate);
		Pivot->UpdateChildTransforms(EUpdateTransformFlags::SkipPhysicsUpdate);
	}
}

void UVBoRMotionGuideComponent::Refresh()
{
	if (!GetWorld() || GetWorld()->IsGameWorld()) { return; }
	FVBoRMotionGuideInput Input; USceneComponent* Pivot = nullptr;
	double PreviewValue = 0, EditorValue = 0; EVBoRMotionGuide Mode;
	if (ReadSource(Input, Pivot, PreviewValue, EditorValue, Mode))
	{
		const double Alpha = FMath::IsFinite(PreviewValue) ? FMath::Clamp(PreviewValue, 0., 1.) : 0;
		const double Value = Alpha > 0 ? FMath::Lerp(Input.First, Input.Last, Alpha) : EditorValue;
		SetCachedPose(Pivot, Input.PoseAt(Value) * Input.Frame);
	}
	TArray<FBatchedLine> NewLines;
	for (const FVBoRGuideLine& Line : BuildShapes())
	{
		NewLines.Emplace(Line.Start, Line.End, FLinearColor(Line.Color), -1, Line.Thickness, 0);
		if (Line.bArrow && !Line.Start.Equals(Line.End))
		{
			const FVector Direction = (Line.End - Line.Start).GetSafeNormal();
			FVector Side, Other; Direction.FindBestAxisVectors(Side, Other);
			const double Size = FMath::Min(3., FVector::Distance(Line.Start, Line.End) * .5);
			NewLines.Emplace(Line.End, Line.End - Direction * Size + Side * Size * .5, FLinearColor(Line.Color), -1, Line.Thickness, 0);
			NewLines.Emplace(Line.End, Line.End - Direction * Size - Side * Size * .5, FLinearColor(Line.Color), -1, Line.Thickness, 0);
		}
	}
	bool Changed = NewLines.Num() != BatchedLines.Num();
	for (int32 I = 0; !Changed && I < NewLines.Num(); ++I)
	{
		Changed = !NewLines[I].Start.Equals(BatchedLines[I].Start) || !NewLines[I].End.Equals(BatchedLines[I].End) ||
			NewLines[I].Color != BatchedLines[I].Color || NewLines[I].Thickness != BatchedLines[I].Thickness;
	}
	if (Changed) { BatchedLines = MoveTemp(NewLines); UpdateBounds(); MarkRenderStateDirty(); }
}

void UVBoRMotionGuideComponent::OnRegister()
{
	Super::OnRegister();
	Refresh();
}

void UVBoRMotionGuideComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	// Persistent editor lines; avoid lifetime expiry and runtime work (R2a D2)
	Refresh();
}
