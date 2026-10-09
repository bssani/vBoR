#pragma once

#include "CoreMinimal.h"

struct FVBoRPokeButtonLayout
{
	FVector FrontCenter = FVector::ZeroVector;
	FVector PlaneCenter = FVector::ZeroVector;
	FVector2D PlaneSize = FVector2D::ZeroVector;

	static FVBoRPokeButtonLayout Calculate(const FBox& MeshBounds, const FTransform& MeshToActor,
		double PressDepthMm, double AreaScale)
	{
		FVBoRPokeButtonLayout Result;
		const FBox ActorBounds = MeshBounds.TransformBy(MeshToActor);
		Result.FrontCenter = ActorBounds.GetCenter();
		Result.FrontCenter.Z = ActorBounds.Max.Z;
		// 표시 mm → UE cm (판정 평면은 완전히 눌린 위치, P0-b D4)
		Result.PlaneCenter = Result.FrontCenter - FVector(0, 0, FMath::Max(0.0, PressDepthMm) / 10.0);
		const FVector Size = ActorBounds.GetSize();
		Result.PlaneSize = FVector2D(Size.X, Size.Y) * FMath::Max(0.0, AreaScale);
		return Result;
	}
};
