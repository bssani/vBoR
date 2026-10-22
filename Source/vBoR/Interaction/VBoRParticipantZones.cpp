#include "VBoRParticipantZones.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "IXRTrackingSystem.h"

DEFINE_LOG_CATEGORY(LogP3Flow);

bool VBoRReadWorldHead(UWorld* World, FTransform& OutHead)
{
	if (!World || !GEngine || !GEngine->XRSystem.IsValid()) { return false; }
	const auto& XR = GEngine->XRSystem;
	FQuat Rotation; FVector Position;
	if (!XR->IsTracking(IXRTrackingSystem::HMDDeviceId) || !XR->GetCurrentPose(IXRTrackingSystem::HMDDeviceId, Rotation, Position)) { return false; }
	// Convert Stage pose to current Pawn world coordinates (P3 D2)
	OutHead = FTransform(Rotation, Position) * XR->GetTrackingToWorldTransform();
	return !OutHead.ContainsNaN();
}

UVBoRApproachZoneComponent::UVBoRApproachZoneComponent()
{
	PrimaryComponentTick.bCanEverTick = true; PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
	SetCollisionEnabled(ECollisionEnabled::NoCollision); SetHiddenInGame(true);
	InitBoxExtent(FVector(385, 242.5, 100));
}

void UVBoRApproachZoneComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	FTransform Head; const bool bTracked = VBoRReadWorldHead(GetWorld(), Head);
	SampleHead(bTracked, Head.GetLocation(), GetWorld()->GetTimeSeconds());
}

void UVBoRApproachZoneComponent::SampleHead(bool bTracked, const FVector& WorldHead, double Now)
{
	if (!bTracked) { return; }
	const FVector Local = GetComponentTransform().InverseTransformPosition(WorldHead);
	const FVector Extent = GetUnscaledBoxExtent();
	const FVector Scale = GetComponentScale().GetAbs();
	const double MarginX = bNear ? DepartMarginCm / FMath::Max(Scale.X, UE_SMALL_NUMBER) : 0;
	const double MarginY = bNear ? DepartMarginCm / FMath::Max(Scale.Y, UE_SMALL_NUMBER) : 0;
	const bool bInside = FMath::Abs(Local.X) <= Extent.X + MarginX && FMath::Abs(Local.Y) <= Extent.Y + MarginY;
	if (bNear == bInside) { return; }
	bNear = bInside;
	if (bNear) { ++Approaches; LastApproachSeconds = Now; OnParticipantApproached.Broadcast(); }
	else { ++Departures; LastDepartureSeconds = Now; OnParticipantDeparted.Broadcast(); }
	UE_LOG(LogP3Flow, Log, TEXT("Approach near=%d approaches=%d departures=%d time=%.3f s"), bNear, Approaches, Departures, Now);
}

UVBoRSeatZoneComponent::UVBoRSeatZoneComponent()
{
	PrimaryComponentTick.bCanEverTick = true; PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
	SetCollisionEnabled(ECollisionEnabled::NoCollision); SetHiddenInGame(true); InitBoxExtent(FVector(25, 25, 32.5));
}

void UVBoRSeatZoneComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	FTransform Head; const bool bTracked = VBoRReadWorldHead(GetWorld(), Head);
	SampleHead(bTracked, Head.GetLocation(), DeltaTime);
}

void UVBoRSeatZoneComponent::SampleHead(bool bTracked, const FVector& WorldHead, double DeltaSeconds)
{
	if (!bTracked)
	{
		if (bWasTracked) { ++TrackingPauses; UE_LOG(LogP3Flow, Log, TEXT("Seat tracking paused count=%d dwell=%.3f s"), TrackingPauses, DwellSeconds); }
		bWasTracked = false; return;
	}
	const bool bRecovered = !bWasTracked; bWasTracked = true;
	const FVector P = GetComponentTransform().InverseTransformPosition(WorldHead).GetAbs();
	const FVector E = GetUnscaledBoxExtent();
	bInside = P.X <= E.X && P.Y <= E.Y && P.Z <= E.Z;
	if (!bInside) { DwellSeconds = 0; }
	// Ignore recovery-frame dt (may include tracking-loss time, P3 E7)
	else if (!bRecovered) { DwellSeconds += FMath::Max(0.0, DeltaSeconds); }
}
