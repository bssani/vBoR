#include "M3aAnchorSpike.h"

#include "Components/InputComponent.h"
#include "Dom/JsonObject.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "Engine/TargetPoint.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "IXRTrackingSystem.h"
#include "InputCoreTypes.h"
#include "../Interaction/VBoRHandRigPawn.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "OculusXRAnchorBPFunctionLibrary.h"
#include "OculusXRAnchorComponent.h"
#include "OculusXRAnchors.h"
#include "Serialization/JsonSerializer.h"
#include "String/HexToBytes.h"

DEFINE_LOG_CATEGORY_STATIC(LogM3a, Log, All);

namespace
{
	using OculusXRAnchors::FOculusXRAnchors;
	using UAnchorLibrary = UOculusXRAnchorBPFunctionLibrary;

	constexpr double DiscoveryTimeoutSeconds = 10.0;

	FString AnchorFilePath()
	{
		return FPaths::ProjectSavedDir() / TEXT("Calibration/AnchorSpike.json");
	}

	FVector StageToAnchor(const FTransform& AnchorStage, const FVector& PointStage)
	{
		return AnchorStage.InverseTransformPosition(PointStage);
	}

	bool HasAnchorRuntime()
	{
		// Meta anchor manager asserts if there is no supported XR system
		if (!GEngine || !GEngine->XRSystem.IsValid())
		{
			return false;
		}
		const FName System = GEngine->XRSystem->GetSystemName();
		return System == TEXT("OpenXR") || System == TEXT("OculusXRHMD");
	}

	bool WriteAnchorFile(const FOculusXRUUID& Uuid, const FTransform& Stage)
	{
		const FVector Position = Stage.GetLocation();
		const FString Path = AnchorFilePath();
		const auto Json = MakeShared<FJsonObject>();
		Json->SetStringField(TEXT("uuid"), Uuid.ToString());
		Json->SetArrayField(TEXT("stagePositionCm"), {
			MakeShared<FJsonValueNumber>(Position.X), MakeShared<FJsonValueNumber>(Position.Y),
			MakeShared<FJsonValueNumber>(Position.Z)});
		Json->SetNumberField(TEXT("stageYawDeg"), Stage.Rotator().Yaw);
		FString Text;
		return FJsonSerializer::Serialize(Json, TJsonWriterFactory<>::Create(&Text))
			&& IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true)
			&& FFileHelper::SaveStringToFile(Text, *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	}

	bool ReadAnchorFile(const FString& Path, FOculusXRUUID& OutUuid, FTransform& OutStage)
	{
		FString Text;
		TSharedPtr<FJsonObject> Json;
		FString UuidHex;
		const TArray<TSharedPtr<FJsonValue>>* Position = nullptr;
		double Yaw = 0.0;
		if (!FFileHelper::LoadFileToString(Text, *Path)
			|| !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Json) || !Json
			|| !Json->TryGetStringField(TEXT("uuid"), UuidHex) || UuidHex.Len() != 2 * UE_ARRAY_COUNT(OutUuid.UUIDBytes)
			|| !Json->TryGetArrayField(TEXT("stagePositionCm"), Position) || Position->Num() != 3
			|| !Json->TryGetNumberField(TEXT("stageYawDeg"), Yaw) || !FMath::IsFinite(Yaw))
		{
			return false;
		}

		for (TCHAR Digit : UuidHex)
		{
			if (!FChar::IsHexDigit(Digit))
			{
				return false;
			}
		}
		FVector StagePosition;
		for (int32 Axis = 0; Axis < 3; ++Axis)
		{
			if (!(*Position)[Axis]->TryGetNumber(StagePosition[Axis]) || !FMath::IsFinite(StagePosition[Axis]))
			{
				return false;
			}
		}
		UE::String::HexToBytes(FStringView(UuidHex), OutUuid.UUIDBytes);
		OutStage = FTransform(FRotator(0.0, Yaw, 0.0), StagePosition);
		return OutUuid.IsValidUUID();
	}
}

AM3aAnchorSpike::AM3aAnchorSpike()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostUpdateWork;
}

void AM3aAnchorSpike::BeginPlay()
{
	Super::BeginPlay();
	EnableInput(GetWorld()->GetFirstPlayerController());
	if (InputComponent)
	{
		InputComponent->BindKey(EKeys::N, IE_Pressed, this, &AM3aAnchorSpike::StartNewAnchorSample);
		InputComponent->BindKey(EKeys::T, IE_Pressed, this, &AM3aAnchorSpike::StartCheckPointSample);
	}
	BeginDiscovery();
}

void AM3aAnchorSpike::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	bDiscovering = false;
	ClearActiveAnchor();
	if (ATargetPoint* Pending = PendingAnchorActor.Get())
	{
		Pending->Destroy();
		PendingAnchorActor.Reset();
	}
	Super::EndPlay(EndPlayReason);
}

void AM3aAnchorSpike::BeginDiscovery()
{
	const FString Path = AnchorFilePath();
	if (!IFileManager::Get().FileExists(*Path))
	{
		UE_LOG(LogM3a, Log, TEXT("No saved anchor; press N to create one"));
		return;
	}
	if (!ReadAnchorFile(Path, SavedUuid, SavedStage))
	{
		UE_LOG(LogM3a, Warning, TEXT("not found: invalid file %s"), *Path);
		return;
	}
	if (!HasAnchorRuntime())
	{
		UE_LOG(LogM3a, Warning, TEXT("not found: XR runtime unavailable"));
		return;
	}

	UOculusXRSpaceDiscoveryIdsFilter* Filter = NewObject<UOculusXRSpaceDiscoveryIdsFilter>(this);
	Filter->Uuids.Add(SavedUuid);
	FOculusXRSpaceDiscoveryInfo Info;
	Info.Filters.Add(Filter);
	DiscoveryStarted = GetWorld()->GetRealTimeSeconds();
	bDiscovering = true;
	EOculusXRAnchorResult::Type Result = EOculusXRAnchorResult::Failure;
	FOculusXRAnchors::DiscoverAnchors(Info,
		FOculusXRDiscoverAnchorsResultsDelegate::CreateUObject(this, &AM3aAnchorSpike::OnDiscovered),
		FOculusXRDiscoverAnchorsCompleteDelegate::CreateUObject(this, &AM3aAnchorSpike::OnDiscoveryComplete), Result);
}

void AM3aAnchorSpike::OnDiscovered(const TArray<FOculusXRAnchorsDiscoverResult>& Results)
{
	for (const FOculusXRAnchorsDiscoverResult& Item : Results)
	{
		if (ActiveAnchor.IsValid() && ActiveAnchor->GetHandle().GetValue() == Item.Space.GetValue())
		{
			continue;
		}
		if (bDiscovering && !ActiveAnchor.IsValid() && Item.UUID == SavedUuid && Item.Space.GetValue() != 0
			&& GetWorld()->GetRealTimeSeconds() - DiscoveryStarted < DiscoveryTimeoutSeconds)
		{
			AActor* Target = UAnchorLibrary::SpawnActorWithAnchorHandle(this, Item.Space, Item.UUID,
				EOculusXRSpaceStorageLocation::Local, ATargetPoint::StaticClass(), this, nullptr,
				ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
			if (Target)
			{
				UseAnchor(Target->FindComponentByClass<UOculusXRAnchorComponent>());
				continue;
			}
		}

		// Discovery yields handles even if the deadline has passed
		if (Item.Space.GetValue() != 0)
		{
			EOculusXRAnchorResult::Type Result;
			FOculusXRAnchors::DestroyAnchor(Item.Space.GetValue(), Result);
		}
	}
}

void AM3aAnchorSpike::OnDiscoveryComplete(EOculusXRAnchorResult::Type Result)
{
	if (bDiscovering && (!UAnchorLibrary::IsAnchorResultSuccess(Result) || !ActiveAnchor.IsValid()))
	{
		bDiscovering = false;
		ClearActiveAnchor();
		UE_LOG(LogM3a, Warning, TEXT("not found: discovery completed result=%d"), static_cast<int32>(Result));
	}
}

void AM3aAnchorSpike::StartNewAnchorSample()
{
	if (bDiscovering || PendingAnchorActor.IsValid())
	{
		UE_LOG(LogM3a, Log, TEXT("N ignored: anchor operation pending"));
		return;
	}
	if (!HasAnchorRuntime())
	{
		UE_LOG(LogM3a, Warning, TEXT("N canceled: XR runtime unavailable"));
		return;
	}
	StartSample(true);
}

void AM3aAnchorSpike::StartCheckPointSample()
{
	StartSample(false);
}

void AM3aAnchorSpike::StartSample(bool bCreateAnchor)
{
	bSampling = true;
	bCreateFromSample = bCreateAnchor;
	SampleEnds = GetWorld()->GetRealTimeSeconds() + 1.0;
	TipSum = FVector::ZeroVector;
	TipSamples = 0;
	UE_LOG(LogM3a, Log, TEXT("%s: hold left index tip for 1 s"), bCreateAnchor ? TEXT("N") : TEXT("T"));
}

void AM3aAnchorSpike::CreateAnchor(const FTransform& PawnWorld, const FVector& StageTip)
{
	const FTransform WorldPose(FRotator(0.0, PawnWorld.Rotator().Yaw, 0.0), PawnWorld.TransformPosition(StageTip));
	NewAnchorStage = WorldPose.GetRelativeTransform(PawnWorld);
	FActorSpawnParameters Spawn;
	Spawn.Owner = this;
	Spawn.ObjectFlags |= RF_Transient;
	PendingAnchorActor = GetWorld()->SpawnActor<ATargetPoint>(ATargetPoint::StaticClass(), WorldPose, Spawn);
	if (!PendingAnchorActor.IsValid())
	{
		UE_LOG(LogM3a, Warning, TEXT("N failed: cannot spawn anchor target"));
		return;
	}

	EOculusXRAnchorResult::Type Result = EOculusXRAnchorResult::Failure;
	if (!FOculusXRAnchors::CreateSpatialAnchor(WorldPose, PendingAnchorActor.Get(),
		FOculusXRSpatialAnchorCreateDelegate::CreateUObject(this, &AM3aAnchorSpike::OnCreated), Result))
	{
		UE_LOG(LogM3a, Warning, TEXT("N: CreateSpatialAnchor request result=%d"), static_cast<int32>(Result));
	}
}

void AM3aAnchorSpike::OnCreated(EOculusXRAnchorResult::Type Result, UOculusXRAnchorComponent* Anchor)
{
	if (!UAnchorLibrary::IsAnchorResultSuccess(Result) || !IsValid(Anchor))
	{
		if (ATargetPoint* Pending = PendingAnchorActor.Get())
		{
			Pending->Destroy();
			PendingAnchorActor.Reset();
		}
		UE_LOG(LogM3a, Warning, TEXT("N: create failed result=%d"), static_cast<int32>(Result));
		return;
	}

	UE_LOG(LogM3a, Log, TEXT("N: created UUID=%s result=%d"), *Anchor->GetUUID().ToString(), static_cast<int32>(Result));
	FOculusXRAnchors::SaveAnchors({Anchor},
		FOculusXRSaveAnchorsDelegate::CreateUObject(this, &AM3aAnchorSpike::OnSaved), Result);
}

void AM3aAnchorSpike::OnSaved(EOculusXRAnchorResult::Type Result, const TArray<UOculusXRAnchorComponent*>& Anchors)
{
	UOculusXRAnchorComponent* Anchor = Anchors.IsEmpty() ? nullptr : Anchors[0];
	ATargetPoint* Pending = PendingAnchorActor.Get();
	PendingAnchorActor.Reset();
	if (UAnchorLibrary::IsAnchorResultSuccess(Result) && IsValid(Anchor) && Anchor->GetOwner() == Pending)
	{
		const FOculusXRUUID Uuid = Anchor->GetUUID();
		if (WriteAnchorFile(Uuid, NewAnchorStage))
		{
			SavedUuid = Uuid;
			SavedStage = NewAnchorStage;
			ClearActiveAnchor();
			UseAnchor(Anchor);
			UE_LOG(LogM3a, Log, TEXT("N: saved UUID=%s result=%d Stage cm (%s) yaw=%.2f deg"),
				*Uuid.ToString(), static_cast<int32>(Result), *SavedStage.GetLocation().ToString(), SavedStage.Rotator().Yaw);
			return;
		}
		UE_LOG(LogM3a, Warning, TEXT("N: anchor saved but JSON write failed: %s"), *AnchorFilePath());
	}
	else
	{
		UE_LOG(LogM3a, Warning, TEXT("N: save failed result=%d"), static_cast<int32>(Result));
	}
	if (Pending)
	{
		Pending->Destroy();
	}
}

void AM3aAnchorSpike::UseAnchor(UOculusXRAnchorComponent* Anchor)
{
	ActiveAnchor = Anchor;
	AddTickPrerequisiteComponent(Anchor);
}

void AM3aAnchorSpike::ClearActiveAnchor()
{
	if (UOculusXRAnchorComponent* Anchor = ActiveAnchor.Get())
	{
		RemoveTickPrerequisiteComponent(Anchor);
		Anchor->GetOwner()->Destroy();
		ActiveAnchor.Reset();
	}
}

bool AM3aAnchorSpike::ReadAnchorStage(const FTransform& PawnWorld, FTransform& OutStage) const
{
	const UOculusXRAnchorComponent* Anchor = ActiveAnchor.Get();
	FOculusXRAnchorLocationFlags Flags;
	FTransform Pose;
	if (!Anchor || !UAnchorLibrary::TryGetAnchorTransformByHandle(Anchor->GetHandle(), Pose, Flags) || !Flags.IsValid())
	{
		return false;
	}

	// Owner transform includes the plugin's Link head-space correction
	OutStage = Anchor->GetOwner()->GetActorTransform().GetRelativeTransform(PawnWorld);
	return true;
}

void AM3aAnchorSpike::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const double Now = GetWorld()->GetRealTimeSeconds();
	const AVBoRHandRigPawn* Pawn = Cast<AVBoRHandRigPawn>(UGameplayStatics::GetPlayerPawn(this, 0));
	const FTransform PawnWorld = Pawn ? Pawn->GetActorTransform() : FTransform::Identity;
	FTransform AnchorStage;
	bool bLocated = Pawn && ReadAnchorStage(PawnWorld, AnchorStage);

	if (bDiscovering)
	{
		const double Elapsed = Now - DiscoveryStarted;
		if (Elapsed >= DiscoveryTimeoutSeconds)
		{
			bDiscovering = false;
			ClearActiveAnchor();
			bLocated = false;
			UE_LOG(LogM3a, Warning, TEXT("not found: no located anchor after %.2f s"), Elapsed);
		}
		else if (bLocated)
		{
			bDiscovering = false;
			UE_LOG(LogM3a, Log, TEXT("found UUID=%s in %.2f s"), *SavedUuid.ToString(), Elapsed);
		}
	}

	if (bLocated)
	{
		const FTransform& AnchorWorld = ActiveAnchor->GetOwner()->GetActorTransform();
		DrawDebugCoordinateSystem(GetWorld(), AnchorWorld.GetLocation(), AnchorWorld.Rotator(), 8.0f, false, -1.0f, 0, 0.8f);
	}

	if (Now >= NextSummary)
	{
		NextSummary = Now + 1.0;
		if (bLocated)
		{
			const FVector Delta = AnchorStage.GetLocation() - SavedStage.GetLocation();
			const double Yaw = AnchorStage.Rotator().Yaw;
			UE_LOG(LogM3a, Log, TEXT("anchor=found Stage cm (%s) yaw=%.2f deg | delta XY=%.2f cm Z=%+.2f cm yaw=%+.2f deg | Pawn cm (%s) yaw=%.2f deg"),
				*AnchorStage.GetLocation().ToString(), Yaw, FVector2D(Delta.X, Delta.Y).Size(), Delta.Z,
				FMath::FindDeltaAngleDegrees(SavedStage.Rotator().Yaw, Yaw), *PawnWorld.GetLocation().ToString(), PawnWorld.Rotator().Yaw);
		}
		else if (Pawn)
		{
			UE_LOG(LogM3a, Log, TEXT("anchor=%s Stage and delta unavailable | Pawn cm (%s) yaw=%.2f deg"),
				bDiscovering ? TEXT("searching") : TEXT("unavailable"),
				*PawnWorld.GetLocation().ToString(), PawnWorld.Rotator().Yaw);
		}
		else
		{
			UE_LOG(LogM3a, Log, TEXT("anchor=unavailable | Pawn unavailable"));
		}
	}

	if (!bSampling)
	{
		return;
	}
	FVector WorldTip;
	if (!Pawn || !Pawn->ReadRawIndexTip(0, WorldTip))
	{
		bSampling = false;
		UE_LOG(LogM3a, Log, TEXT("%s canceled: left index tracking unavailable"), bCreateFromSample ? TEXT("N") : TEXT("T"));
		return;
	}
	TipSum += PawnWorld.InverseTransformPosition(WorldTip);
	++TipSamples;
	if (Now >= SampleEnds)
	{
		bSampling = false;
		const FVector StageTip = TipSum / TipSamples;
		if (bCreateFromSample)
		{
			CreateAnchor(PawnWorld, StageTip);
		}
		else
		{
			const FString Relative = bLocated ? StageToAnchor(AnchorStage, StageTip).ToString() : TEXT("unavailable");
			UE_LOG(LogM3a, Log, TEXT("T: Stage cm (%s) | anchor-relative cm (%s) | %d samples"),
				*StageTip.ToString(), *Relative, TipSamples);
		}
	}
}

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FM3aAnchorRelativeTest, "vBoR.M3a.AnchorRelative",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FM3aAnchorRelativeTest::RunTest(const FString& Parameters)
{
	constexpr double ToleranceCm = 0.01; // 0.1 mm
	const FVector StagePoint(70.0, 40.0, 125.0);
	const FVector Origin(20.0, 40.0, 110.0);
	const FVector ExpectedRelative[] = {
		FVector(50.0, 0.0, 15.0),
		FVector(43.3012701892, -25.0, 15.0),
	};
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(ExpectedRelative); ++Index)
	{
		const FTransform AnchorStage(FRotator(0.0, Index * 30.0, 0.0), Origin);
		const FVector Relative = StageToAnchor(AnchorStage, StagePoint);
		TestTrue(TEXT("Anchor-relative point matches the expected axes"),
			FVector::Distance(Relative, ExpectedRelative[Index]) <= ToleranceCm);
		TestTrue(TEXT("Round trip returns to the Stage point within 0.1 mm"),
			FVector::Distance(AnchorStage.TransformPosition(Relative), StagePoint) <= ToleranceCm);
	}
	return true;
}
#endif
