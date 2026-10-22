#pragma once

#include "CoreMinimal.h"

class UBoxComponent;
class UPrimitiveComponent;
class UStaticMeshComponent;

// Choose once at Play start (P2f D1, no runtime option changes)
UPrimitiveComponent* ConfigureVBoRGrabCollider(UStaticMeshComponent* Mesh, UBoxComponent* Box,
	bool bGrabWithMesh, bool bGrabValid, const FTransform& MeshRestFromRoot);
