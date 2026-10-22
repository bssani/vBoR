#include "VBoRCarryObject.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"

void AVBoRCarryObject::ResolveHeldContact(const FTransform& Requested, float DeltaSeconds)
{
	FTransform Resolved = GetActorTransform();
	if (!GetWorld() || !IsPoseClear(Resolved)) { return; }
	constexpr double SkinCm = 0.005;
	FComponentQueryParams Params(SCENE_QUERY_STAT(VBoRCarryContact), this);
	// Invalid time/speed gives zero budget (frame-rate-independent push-out, P1 R6-1, cm/s × s)
	const double FrameSeconds = FMath::IsFinite(DeltaSeconds) ? FMath::Max(0.0f, DeltaSeconds) : 0.0;
	const double PushRate = FMath::IsFinite(MaxRotationPushCmPerSecond) ? FMath::Max(0.0f, MaxRotationPushCmPerSecond) : 0.0;
	double PushBudgetCm = PushRate * FrameSeconds;
	// Retry rotation after translation clears contact (no one-frame lift+tilt lag, P1 D7)
	for (int32 Stage = 0; Stage < 2; ++Stage)
	{
		const FTransform Current = Resolved;
		FQuat Rotation = Requested.GetRotation();
		if (Session.bHasRestRotation)
		{
			Rotation = FVBoRCarryMotion::LimitWorldTilt(Rotation * Session.RestRotation.Inverse(), TiltLimitDeg) * Session.RestRotation;
		}
		auto CanTranslateFromResolved = [&](const FTransform& Candidate)
		{
			const FVector Travel = Candidate.GetLocation() - Resolved.GetLocation();
			if (Travel.IsNearlyZero()) { return true; }
			TArray<FHitResult> Hits;
			GetWorld()->ComponentSweepMulti(Hits, Mesh, Resolved.GetLocation(), Candidate.GetLocation(), Resolved.GetRotation(), Params);
			for (const FHitResult& Hit : Hits)
			{
				if (!Hit.bBlockingHit) { continue; }
				if (Hit.bStartPenetrating && Hit.PenetrationDepth <= SkinCm && FVector::DotProduct(Travel, Hit.Normal) >= -1e-6) { continue; }
				if (!Hit.bStartPenetrating && (1.0 - Hit.Time) * Travel.Size() <= SkinCm) { continue; }
				return false;
			}
			return true;
		};

		// ── 1) Push out overlaps introduced by requested rotation only ──
		// Initial penetration has no single contact point. Combine plane constraints by Normal/depth (P1 D7, cm)
		auto RotationPose = [&](double Fraction, FTransform& Candidate)
		{
			Candidate = Current;
			Candidate.SetRotation(FQuat::Slerp(Current.GetRotation(), Rotation, Fraction).GetNormalized());
			if (Session.bHasRestRotation)
			{
				const FVector Up = (Candidate.GetRotation() * Session.RestRotation.Inverse()).RotateVector(FVector::UpVector);
				if (Up.Z < FMath::Cos(FMath::DegreesToRadians(static_cast<double>(TiltLimitDeg))) - 1e-6) { return false; }
			}
			for (int32 Pass = 0; Pass < 3; ++Pass)
			{
				if (IsPoseClear(Candidate)) { return CanTranslateFromResolved(Candidate); }
				TArray<FHitResult> Contacts;
				GetWorld()->ComponentSweepMulti(Contacts, Mesh, Candidate.GetLocation(), Candidate.GetLocation(), Candidate.GetRotation(), Params);
				FVector Correction = FVector::ZeroVector;
				for (const FHitResult& Hit : Contacts)
				{
					if (!Hit.bBlockingHit || !Hit.bStartPenetrating || Hit.PenetrationDepth <= SkinCm) { continue; }
					const FVector Normal = Hit.Normal.GetSafeNormal();
					Correction += Normal * FMath::Max(0.0, Hit.PenetrationDepth + SkinCm - FVector::DotProduct(Correction, Normal));
				}
				if (Correction.IsNearlyZero() || (Candidate.GetLocation() + Correction - Current.GetLocation()).Size() > PushBudgetCm) { return false; }
				Candidate.AddToTranslation(Correction);
			}
			return IsPoseClear(Candidate) && CanTranslateFromResolved(Candidate);
		};
		// Check small angular steps (avoid passing through on clear endpoints alone, P1 D7)
		const double Angle = FMath::RadiansToDegrees(Current.GetRotation().AngularDistance(Rotation));
		const int32 Steps = FMath::Clamp(FMath::CeilToInt(Angle / 5.0), 1, 36);
		double Accepted = 0;
		for (int32 Step = 1; Step <= Steps; ++Step)
		{
			const double Fraction = static_cast<double>(Step) / Steps;
			FTransform Candidate;
			if (RotationPose(Fraction, Candidate)) { Resolved = Candidate; Accepted = Fraction; continue; }
			double Upper = Fraction;
			for (int32 Search = 0; Search < 14; ++Search)
			{
				const double Mid = (Accepted + Upper) * 0.5;
				if (RotationPose(Mid, Candidate)) { Resolved = Candidate; Accepted = Mid; }
				else { Upper = Mid; }
			}
			break;
		}
		// Second rotation uses remaining frame budget (no doubled push-out cap, P1 D7, cm)
		PushBudgetCm = FMath::Max(0.0, PushBudgetCm - FVector::Distance(Current.GetLocation(), Resolved.GetLocation()));

		// ── 2) Project remaining sweep motion onto plane/two-plane intersection ──
		const FVector TranslationStart = Resolved.GetLocation();
		FVector Remaining = Requested.GetLocation() - Resolved.GetLocation();
		FVector PreviousNormal = FVector::ZeroVector;
		TArray<FVector, TInlineAllocator<3>> ContactNormals;
		for (int32 Pass = 0; Pass < 3 && !Remaining.IsNearlyZero(); ++Pass)
		{
			TArray<FHitResult> Hits;
			GetWorld()->ComponentSweepMulti(Hits, Mesh, Resolved.GetLocation(), Resolved.GetLocation() + Remaining, Resolved.GetRotation(), Params);
			const FHitResult* First = nullptr;
			for (const FHitResult& Hit : Hits)
			{
				if (!Hit.bBlockingHit) { continue; }
				if (Hit.bStartPenetrating && Hit.PenetrationDepth <= SkinCm && FVector::DotProduct(Remaining, Hit.Normal) >= -1e-6) { continue; }
				if (!First || Hit.Time < First->Time) { First = &Hit; }
			}
			const double Time = First ? FMath::Max(0.0, First->Time - SkinCm / Remaining.Size()) : 1.0;
			FTransform Next = Resolved;
			Next.AddToTranslation(Remaining * Time);
			if (!IsPoseClear(Next)) { break; }
			Resolved = Next;
			if (!First) { break; }
			Remaining *= 1.0 - Time;
			const FVector Normal = First->Normal.GetSafeNormal();
			FVector Slide = FVector::VectorPlaneProject(Remaining, Normal);
			if (!PreviousNormal.IsNearlyZero() && FVector::DotProduct(Slide, PreviousNormal) < -1e-6)
			{
				const FVector Edge = FVector::CrossProduct(PreviousNormal, Normal).GetSafeNormal();
				Slide = Edge * FVector::DotProduct(Remaining, Edge);
			}
			PreviousNormal = Normal;
			ContactNormals.Add(Normal);
			for (const FVector& ContactNormal : ContactNormals)
			{
				if (FVector::DotProduct(Slide, ContactNormal) < -1e-6) { Slide = FVector::ZeroVector; break; }
			}
			Remaining = Slide;
		}
		if (Resolved.GetLocation().Equals(TranslationStart, 1e-6)) { break; }
	}
	if (IsPoseClear(Resolved)) { SetActorTransform(Resolved, false, nullptr, ETeleportType::TeleportPhysics); }
}
