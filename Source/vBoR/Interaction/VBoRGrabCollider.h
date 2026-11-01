#pragma once

#include "CoreMinimal.h"

class UBoxComponent;
class UPrimitiveComponent;
class UStaticMeshComponent;

// Shared read-only choice (R2c D6)
bool VBoRUsesGrabMesh(const UStaticMeshComponent* Mesh, bool bGrabWithMesh);

// Choose once at Play start (P2f D1, no runtime option changes)
UPrimitiveComponent* ConfigureVBoRGrabCollider(UStaticMeshComponent* Mesh, UBoxComponent* Box,
	bool bGrabWithMesh, bool bGrabValid, const FTransform& MeshRestFromRoot);
