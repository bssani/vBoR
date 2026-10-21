#include "VBoRGrabCollider.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Actor.h"
#include "PhysicsEngine/BodySetup.h"

DEFINE_LOG_CATEGORY_STATIC(LogVBoRGrabCollider, Log, All);

UPrimitiveComponent* ConfigureVBoRGrabCollider(UStaticMeshComponent* Mesh, UBoxComponent* Box,
	bool bGrabWithMesh, bool bGrabValid, const FTransform& MeshRestFromRoot)
{
	bool bUseMesh = false;
	if (bGrabWithMesh)
	{
		const UBodySetup* Body = Mesh->GetStaticMesh() ? Mesh->GetStaticMesh()->GetBodySetup() : nullptr;
		bUseMesh = Body && (Body->AggGeom.GetElementCount() > 0 || Body->GetCollisionTraceFlag() == CTF_UseComplexAsSimple);
		if (!bUseMesh)
		{
			UE_LOG(LogVBoRGrabCollider, Warning, TEXT("%s: grab mesh has no simple collision (or Use Complex As Simple); using GrabBox"), *Mesh->GetOwner()->GetName());
		}
		if (MeshRestFromRoot.GetLocation().SizeSquared() <= 1.0)
		{
			UE_LOG(LogVBoRGrabCollider, Warning, TEXT("%s: grab mesh pivot is at the actor origin; ISDK picks the nearest collider origin - recentre the mesh pivot and put the offset in MeshRestFromRoot"), *Mesh->GetOwner()->GetName());
		}
	}
	Mesh->SetCollisionEnabled(bUseMesh && bGrabValid ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
	Mesh->SetGenerateOverlapEvents(bUseMesh && bGrabValid);
	if (bUseMesh)
	{
		// 짐 착지 질의가 막힘으로 세지 않게 overlap 만 받음 (P2f D2, UE cm)
		Mesh->SetCollisionObjectType(ECC_WorldDynamic);
		Mesh->SetCollisionResponseToAllChannels(ECR_Ignore);
		Mesh->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);
	}
	Box->SetCollisionEnabled(!bUseMesh && bGrabValid ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
	Box->SetGenerateOverlapEvents(!bUseMesh && bGrabValid);
	return bUseMesh ? static_cast<UPrimitiveComponent*>(Mesh) : Box;
}
