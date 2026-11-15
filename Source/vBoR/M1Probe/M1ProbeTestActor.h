#pragma once

#include "M1MarkerAlignProbe.h"
#include "M1ProbeTestActor.generated.h"

UCLASS(NotPlaceable, Transient)
class AM1ProbeTestActor : public AM1MarkerAlignProbe
{
	GENERATED_BODY()
public:
	APawn* Pawn = nullptr;
	FM1CameraFrame Frame;
	FVector Tip = FVector::ZeroVector;
	bool bTracked = false;
	bool bCanStart = true;
	bool bMirrorProjection = true;

protected:
	bool StartCamera() override { return bCanStart; }
	bool ReadCameraFrame(FM1CameraFrame& Out, bool bReadPixels) override
	{
		Out.Timestamp = Frame.Timestamp;
		Out.Model = Frame.Model;
		Out.Size = Frame.Size;
		if (bReadPixels)
		{
			Out.Pixels = MoveTemp(Frame.Pixels);
		}
		return true;
	}
	FVector2D ProjectMruk(const FVector& World) const override
	{
		FVector Local = Frame.Model.Pose.InverseTransformPosition(World);
		if (bMirrorProjection)
		{
			Local.Z = -Local.Z;
		}
		return Frame.Model.Project(Frame.Model.Pose.TransformPosition(Local)) / FVector2D(Frame.Size.X, Frame.Size.Y);
	}
	APawn* ReadPawn() const override { return Pawn; }
	bool ReadTip(FVector& World) const override { World = Tip; return bTracked; }
};
