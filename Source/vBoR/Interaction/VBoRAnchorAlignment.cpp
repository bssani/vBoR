#include "VBoRVehicleAlignmentComponent.h"

#include "Engine/Engine.h"
#include "Engine/TargetPoint.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "HAL/IConsoleManager.h"
#include "IXRTrackingSystem.h"
#include "OculusXRAnchorBPFunctionLibrary.h"
#include "OculusXRAnchorComponent.h"
#include "OculusXRAnchors.h"
#include "String/HexToBytes.h"

namespace
{
	using OculusXRAnchors::FOculusXRAnchors;
	using UAnchorLibrary = UOculusXRAnchorBPFunctionLibrary;
	TAutoConsoleVariable<int32> CVarAnchorAlignment(TEXT("vBoR.AnchorAlignment"), 0,
		TEXT("1 = save and locate a local anchor for point alignment. Default 0 (M3b)."));
	bool HasAnchorRuntime()
	{
		if (!GEngine || !GEngine->XRSystem.IsValid()) { return false; }
		const FName Name = GEngine->XRSystem->GetSystemName();
		return Name == TEXT("OpenXR") || Name == TEXT("OculusXRHMD");
	}
}

bool UVBoRVehicleAlignmentComponent::AnchorAlignmentEnabled()
{ return CVarAnchorAlignment.GetValueOnGameThread() != 0; }

void UVBoRVehicleAlignmentComponent::AnchorResult(const FString& Message)
{
	AnchorNotice = Message;
	UE_LOG(LogTemp, Log, TEXT("Alignment %s"), *Message);
	OnPointRecord.Broadcast(Message);
}

void UVBoRVehicleAlignmentComponent::ClearAnchorActors()
{
	if (auto* Anchor = LocatedAnchor.Get())
	{ RemoveTickPrerequisiteComponent(Anchor); Anchor->GetOwner()->Destroy(); }
	LocatedAnchor.Reset();
	if (auto* Pending = PendingAnchorActor.Get()) { Pending->Destroy(); }
	PendingAnchorActor.Reset();
}

void UVBoRVehicleAlignmentComponent::SaveAlignmentAnchor(const FString& Path, const FVBoRBuckSavedAlignment& Candidate)
{
	// New point save supersedes pending callbacks even if the switch was turned off
	++AnchorSaveSerial;
	bFindingAnchor = false;
	LocatedAnchorUuid.Reset();
	ClearAnchorActors();
	if (!AnchorAlignmentEnabled() || Candidate.Version != 2) { return; }
	AnchorResult(TEXT("Saving anchor — wait"));
#if WITH_DEV_AUTOMATION_TESTS
	if (AnchorRequestForTest) { AnchorRequestForTest(); return; }
#endif
	if (!HasAnchorRuntime() || !AlignmentPawn)
	{ AnchorResult(FString::Printf(TEXT("anchor create failed result=%d (XR/Pawn unavailable); alignment saved"), int32(EOculusXRAnchorResult::Failure))); return; }
	FVector Center = FVector::ZeroVector;
	for (const auto& Point : Candidate.Points) { Center += Point.StageCm; }
	Center /= Candidate.Points.Num();
	const FTransform Stage(FRotator::ZeroRotator, Center);
	const FTransform WorldPose = Stage * AlignmentPawn->GetActorTransform();
	FActorSpawnParameters Spawn; Spawn.Owner = GetOwner(); Spawn.ObjectFlags |= RF_Transient;
	AActor* Target = GetWorld()->SpawnActor<ATargetPoint>(ATargetPoint::StaticClass(), WorldPose, Spawn);
	if (!Target)
	{ AnchorResult(FString::Printf(TEXT("anchor create failed result=%d (target unavailable); alignment saved"), int32(EOculusXRAnchorResult::Failure))); return; }
	PendingAnchorActor = Target;
	const uint32 Serial = AnchorSaveSerial;
	EOculusXRAnchorResult::Type Result = EOculusXRAnchorResult::Failure;
	const bool bStarted = FOculusXRAnchors::CreateSpatialAnchor(WorldPose, Target,
		FOculusXRSpatialAnchorCreateDelegate::CreateWeakLambda(this,
			[this, Serial, Path, Candidate, Stage](EOculusXRAnchorResult::Type Code, UOculusXRAnchorComponent* Anchor)
		{
			if (Serial != AnchorSaveSerial) { return; }
			if (!UAnchorLibrary::IsAnchorResultSuccess(Code) || !IsValid(Anchor))
			{ ClearAnchorActors(); AnchorResult(FString::Printf(TEXT("anchor create failed result=%d; alignment saved"), int32(Code))); return; }
			EOculusXRAnchorResult::Type SaveCode = EOculusXRAnchorResult::Failure;
			FOculusXRAnchors::SaveAnchors({Anchor}, FOculusXRSaveAnchorsDelegate::CreateWeakLambda(this,
				[this, Serial, Path, Candidate, Stage](EOculusXRAnchorResult::Type SavedCode, const TArray<UOculusXRAnchorComponent*>& Anchors)
				{
					if (Serial != AnchorSaveSerial) { return; }
					FString Failure;
					auto Value = Candidate;
					if (UAnchorLibrary::IsAnchorResultSuccess(SavedCode) && !Anchors.IsEmpty() && IsValid(Anchors[0]))
					{
						Value.AnchorUuid = Anchors[0]->GetUUID().ToString(); Value.AnchorStage = Stage;
						if (FVBoRBuckAlignmentStore::Save(Path, Value, Failure))
						{ AnchorResult(FString::Printf(TEXT("anchor saved UUID=%s result=%d"), *Value.AnchorUuid, int32(SavedCode))); }
						else { AnchorResult(FString::Printf(TEXT("anchor file save failed result=%d: %s; alignment saved without anchor"), int32(SavedCode), *Failure)); }
					}
					else { AnchorResult(FString::Printf(TEXT("anchor save failed result=%d; alignment saved without anchor"), int32(SavedCode))); }
					ClearAnchorActors();
				}), SaveCode);
		}), Result);
	if (!bStarted) { AnchorResult(FString::Printf(TEXT("anchor create request failed result=%d; alignment saved"), int32(Result))); }
}

void UVBoRVehicleAlignmentComponent::BeginAnchorDiscovery()
{
	bFindingAnchor = true; AnchorStarted = GetWorld()->GetRealTimeSeconds();
	AnchorDiscoveryResult = int32(EOculusXRAnchorResult::Failure);
	bAnchorCandidate = false; AnchorPoseChanges = 0;
#if WITH_DEV_AUTOMATION_TESTS
	if (AnchorRequestForTest) { AnchorRequestForTest(); return; }
#endif
	if (!HasAnchorRuntime()) { FinishAnchorDiscovery(nullptr, AnchorDiscoveryResult); return; }
	FOculusXRUUID Uuid;
	UE::String::HexToBytes(FStringView(Saved.AnchorUuid), Uuid.UUIDBytes);
	auto* Filter = NewObject<UOculusXRSpaceDiscoveryIdsFilter>(this); Filter->Uuids.Add(Uuid);
	FOculusXRSpaceDiscoveryInfo Info; Info.Filters.Add(Filter);
	EOculusXRAnchorResult::Type Result = EOculusXRAnchorResult::Failure;
	FOculusXRAnchors::DiscoverAnchors(Info,
		FOculusXRDiscoverAnchorsResultsDelegate::CreateWeakLambda(this, [this, Uuid](const TArray<FOculusXRAnchorsDiscoverResult>& Results)
		{
			for (const auto& Item : Results)
			{
				if (LocatedAnchor.IsValid() && LocatedAnchor->GetHandle().GetValue() == Item.Space.GetValue()) { continue; }
				if (bFindingAnchor && !LocatedAnchor.IsValid() && Item.UUID == Uuid && Item.Space.GetValue() != 0 &&
					GetWorld()->GetRealTimeSeconds() - AnchorStarted < 10.0)
				{
					AActor* Target = UAnchorLibrary::SpawnActorWithAnchorHandle(this, Item.Space, Item.UUID,
						EOculusXRSpaceStorageLocation::Local, ATargetPoint::StaticClass(), GetOwner(), nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
					if (Target)
					{
						if (auto* Anchor = Target->FindComponentByClass<UOculusXRAnchorComponent>())
						{ UseLocatedAnchor(Anchor); continue; }
						Target->Destroy();
					}
				}
				if (Item.Space.GetValue() != 0) { EOculusXRAnchorResult::Type Code; FOculusXRAnchors::DestroyAnchor(Item.Space.GetValue(), Code); }
			}
		}),
		FOculusXRDiscoverAnchorsCompleteDelegate::CreateWeakLambda(this, [this](EOculusXRAnchorResult::Type Code)
		{
			AnchorDiscoveryResult = int32(Code);
			if (bFindingAnchor && (!UAnchorLibrary::IsAnchorResultSuccess(Code) || !LocatedAnchor.IsValid()))
			{ FinishAnchorDiscovery(nullptr, int32(Code)); }
		}), Result);
	if (bFindingAnchor && !UAnchorLibrary::IsAnchorResultSuccess(Result)) { FinishAnchorDiscovery(nullptr, int32(Result)); }
}

void UVBoRVehicleAlignmentComponent::UseLocatedAnchor(UOculusXRAnchorComponent* Anchor)
{
	LocatedAnchor = Anchor;
	AnchorActorCreatedFrame = GFrameCounter;
	AnchorActorInitialWorld = Anchor->GetOwner()->GetActorTransform();
	AddTickPrerequisiteComponent(Anchor);
}

void UVBoRVehicleAlignmentComponent::AdvanceAnchorDiscovery()
{
	if (!bFindingAnchor) { return; }
	if (GetWorld()->GetRealTimeSeconds() - AnchorStarted >= 10.0)
	{ FinishAnchorDiscovery(nullptr, AnchorDiscoveryResult); return; }
	if (!LocatedAnchor.IsValid() || !AlignmentPawn) { return; }
	const FTransform AnchorWorld = LocatedAnchor->GetOwner()->GetActorTransform();
	// Spawn has no pose; wait for the plugin's Link correction Tick (M3b review R1)
	if (GFrameCounter <= AnchorActorCreatedFrame || AnchorWorld.Equals(AnchorActorInitialWorld)) { return; }
	FOculusXRAnchorLocationFlags Flags; FTransform Pose;
	const bool bLocated =
#if WITH_DEV_AUTOMATION_TESTS
		bAnchorLocationValidForTest ||
#endif
		(UAnchorLibrary::TryGetAnchorTransformByHandle(LocatedAnchor->GetHandle(), Pose, Flags) && Flags.IsValid());
	if (!bLocated) { bAnchorCandidate = false; return; }
	// Owner includes Link head-space correction (M3a); cache Stage before moving Pawn
	const FTransform Stage = AnchorWorld.GetRelativeTransform(AlignmentPawn->GetActorTransform());
	// Link reports unsettled pose in first frames (L_Main read at +0.03 s was wrong, M3c).
	// Use only after pose holds within tolerance of window start for AnchorStableSeconds
	const double Now = GetWorld()->GetRealTimeSeconds();
	const bool bSame = bAnchorCandidate &&
		FVector::Dist(Stage.GetLocation(), AnchorCandidateStage.GetLocation()) <= AnchorStableCm &&
		FMath::RadiansToDegrees(Stage.GetRotation().AngularDistance(AnchorCandidateStage.GetRotation())) <= AnchorStableDeg;
	if (!bSame)
	{
		if (bAnchorCandidate) { ++AnchorPoseChanges; }
		bAnchorCandidate = true; AnchorCandidateStage = Stage; AnchorStableSince = Now;
		return;
	}
	if (Now - AnchorStableSince < AnchorStableSeconds) { return; }
	FinishAnchorDiscovery(&Stage, int32(EOculusXRAnchorResult::Success));
}

void UVBoRVehicleAlignmentComponent::FinishAnchorDiscovery(const FTransform* Stage, int32 Result)
{
	if (!bFindingAnchor) { return; }
	bFindingAnchor = false;
	bAnchorApplied = Stage != nullptr;
	if (Stage) { LocatedAnchorStage = *Stage; LocatedAnchorUuid = Saved.AnchorUuid; }
	else { LocatedAnchorUuid.Reset(); ClearAnchorActors(); }
	const auto Value = Saved;
	FString Found;
	if (Stage)
	{
		// Stage drift = how far room moved in Stage coords since save. Near 0 if headset never slept (M3c)
		const FVector Move = Stage->GetLocation() - Value.AnchorStage.GetLocation();
		const double Yaw = FMath::FindDeltaAngleDegrees(Value.AnchorStage.Rotator().Yaw, Stage->Rotator().Yaw);
		Found = FString::Printf(TEXT("anchor found result=%d after %.2f s, pose changes %d | Stage drift since save XY %.2f cm Z %+.2f cm Yaw %+.2f deg"),
			Result, GetWorld()->GetRealTimeSeconds() - AnchorStarted, AnchorPoseChanges, Move.Size2D(), Move.Z, Yaw);
	}
	int32 Origin; FVector2D Bounds; ReadPointEnvironment(Origin, Bounds);
	InitializeSetup(AlignmentPawn, &Value, Origin, Bounds, true);
	AnchorResult(Stage ? Found :
		// Pose changes > 0 here = anchor seen but never settled within timeout (M3c)
		FString::Printf(TEXT("anchor not found result=%d, pose changes %d; using saved Stage points"), Result, AnchorPoseChanges));
	if (Stage && LocatedAnchor.IsValid())
	{
		RemoveTickPrerequisiteComponent(LocatedAnchor.Get()); LocatedAnchor->GetOwner()->Destroy(); LocatedAnchor.Reset();
	}
	OnPawnMoved.Broadcast();
}
