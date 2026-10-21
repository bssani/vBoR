#pragma once

#include "CoreMinimal.h"

class UBoxComponent;
class UPrimitiveComponent;
class UStaticMeshComponent;

// Play 시작 때 한 번만 고름 (P2f D1, 실행 중 옵션 변경은 받지 않음)
UPrimitiveComponent* ConfigureVBoRGrabCollider(UStaticMeshComponent* Mesh, UBoxComponent* Box,
	bool bGrabWithMesh, bool bGrabValid, const FTransform& MeshRestFromRoot);
