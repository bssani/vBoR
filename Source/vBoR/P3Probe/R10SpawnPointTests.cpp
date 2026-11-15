#include "R10SpawnPointTestFixture.h"
#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"

AR10CollisionActor::AR10CollisionActor()
{
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
	auto* Bounds = CreateDefaultSubobject<UBoxComponent>(TEXT("Bounds"));
	Bounds->SetupAttachment(RootComponent);
	Bounds->InitBoxExtent(FVector(10, 15, 20));
	Bounds->SetRelativeLocation(FVector(5, 10, 30));
	Bounds->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
}

#if WITH_DEV_AUTOMATION_TESTS
#include "../Interaction/VBoRObjectSpawnPoint.h"
#include "../Interaction/VBoRVehicleAlignmentComponent.h"
#include "Components/ArrowComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/WorldSettings.h"
#include "Misc/AutomationTest.h"
#include "UObject/CoreRedirects.h"

namespace
{
constexpr auto R10TestFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;

struct FR10World
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);

	~FR10World()
	{
		World->EndPlay(EEndPlayReason::Quit);
		World->DestroyWorld(false);
	}

	AVBoRObjectSpawnPoint* Point(UClass* Class, const FTransform& Pose = FTransform::Identity) const
	{
		auto* Point = World->SpawnActorDeferred<AVBoRObjectSpawnPoint>(AVBoRObjectSpawnPoint::StaticClass(), Pose);
		Point->SpawnClass = Class;
		Point->FinishSpawning(Pose);
		return Point;
	}

	void AddAlignment(float FloorZCm) const
	{
		auto* Alignment = NewObject<UVBoRVehicleAlignmentComponent>(World->GetWorldSettings());
		Alignment->FloorWorldZCm = FloorZCm;
		World->GetWorldSettings()->AddInstanceComponent(Alignment);
		Alignment->RegisterComponent();
	}

	void Play() const
	{
		World->InitializeActorsForPlay(FURL());
		World->GetWorldSettings()->NotifyBeginPlay();
	}

	TArray<AActor*> OwnedActors(const AActor* Owner) const
	{
		TArray<AActor*> Result;
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (It->GetOwner() == Owner) { Result.Add(*It); }
		}
		return Result;
	}
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FR10BoxPose, "vBoR.R10.E1BoxPose", R10TestFlags)
bool FR10BoxPose::RunTest(const FString&)
{
	FR10World W;
	W.AddAlignment(0);
	const FTransform Pose(FRotator(0, 30, 0), FVector(100, 200, 120));
	const auto* Point = W.Point(AR10SizedBox::StaticClass(), Pose);
	W.Play();
	const auto* Box = Point->SourceBox.Get();
	if (!TestNotNull(TEXT("Source box"), Box)) { return false; }
	const FBox Bounds = Box->GetCollisionBoundsInPose(Box->GetActorTransform(), FTransform::Identity);
	TestTrue(TEXT("Box bottom at 120.01 cm"), Bounds.IsValid && FMath::IsNearlyEqual(Bounds.Min.Z, 120.01, .001));
	TestTrue(TEXT("Box center shares XY with point"), FVector2D(Box->GetActorLocation()).Equals(FVector2D(Pose.GetLocation())));
	TestTrue(TEXT("Box rotation matches point"), Box->GetActorQuat().Equals(Pose.GetRotation()));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FR10Preview, "vBoR.R10.E2EditorPreview", R10TestFlags)
bool FR10Preview::RunTest(const FString&)
{
	FR10World W;
	W.AddAlignment(0);
	const FTransform Pose(FRotator(0, 30, 0), FVector(100, 200, 120));
	auto* Point = W.Point(AR10SizedBox::StaticClass(), Pose);
	const auto* Arrow = Point->FindComponentByClass<UArrowComponent>();
	const auto* Bounds = Point->FindComponentByClass<UBoxComponent>();
	if (!TestNotNull(TEXT("Arrow preview"), Arrow) || !TestNotNull(TEXT("Box preview"), Bounds)) { return false; }
	TestTrue(TEXT("Arrow points along actor +X"), Arrow->GetForwardVector().Equals(Point->GetActorForwardVector()));
	TestTrue(TEXT("Box preview matches class size"), Bounds->GetUnscaledBoxExtent().Equals(FVector(25, 20, 10)));
	TestTrue(TEXT("Box preview bottom at point"), (Bounds->GetRelativeLocation() - FVector(0, 0, Bounds->GetUnscaledBoxExtent().Z)).IsNearlyZero());

	Point->SpawnClass = AVBoRCarryObject::StaticClass();
	Point->OnConstruction(Pose);
	TestTrue(TEXT("Preview resizes when class changes"), Bounds->GetUnscaledBoxExtent().Equals(GetDefault<AVBoRCarryObject>()->BoxSizeCm * .5));
	TestEqual(TEXT("Construction spawns no actors"), W.OwnedActors(Point).Num(), 0);
	W.Play();
	for (const auto* Component : TInlineComponentArray<UPrimitiveComponent*>(Point))
	{
		TestTrue(TEXT("Primitive hidden in game and editor-only"), Component->bHiddenInGame && Component->IsEditorOnly());
		TestFalse(TEXT("Primitive collision disabled"), Component->IsCollisionEnabled());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FR10FloorHeight, "vBoR.R10.E3FloorHeight", R10TestFlags)
bool FR10FloorHeight::RunTest(const FString&)
{
	AddExpectedMessage(TEXT("Floor height from alignment unavailable — using 0 cm"), ELogVerbosity::Warning,
		EAutomationExpectedMessageFlags::Contains, 2);
	for (const int32 AlignmentCount : {0, 1, 2})
	{
		FR10World W;
		for (int32 Index = 0; Index < AlignmentCount; ++Index) { W.AddAlignment(50); }
		auto* Point = W.Point(AR10SizedBox::StaticClass(), FTransform(FVector(0, 0, 120)));
		W.Play();
		if (!TestNotNull(TEXT("Source box"), Point->SourceBox.Get())) { return false; }
		TestEqual(TEXT("Exactly one alignment defines the floor"), Point->SourceBox->FloorHeightCm, AlignmentCount == 1 ? 50.f : 0.f);
		Point->ResetSpawn();
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FR10OtherActor, "vBoR.R10.E4OtherActor", R10TestFlags)
bool FR10OtherActor::RunTest(const FString&)
{
	FR10World W;
	const FTransform Pose(FRotator(0, 30, 0), FVector(10, 20, 120));
	auto* Point = W.Point(AR10CollisionActor::StaticClass(), Pose);
	W.Play();
	const auto First = W.OwnedActors(Point);
	if (!TestEqual(TEXT("One actor at BeginPlay"), First.Num(), 1)) { return false; }
	const FBox Bounds = First[0]->GetComponentsBoundingBox();
	FVector BottomCenter = Bounds.GetCenter();
	BottomCenter.Z = Bounds.Min.Z;
	TestTrue(TEXT("Collision bottom center at point"), Bounds.IsValid && BottomCenter.Equals(Pose.GetLocation()));
	TestTrue(TEXT("Actor rotation matches point"), First[0]->GetActorQuat().Equals(Pose.GetRotation()));
	Point->ResetSpawn();
	const auto Restarted = W.OwnedActors(Point);
	if (!TestEqual(TEXT("One actor after restart"), Restarted.Num(), 1)) { return false; }
	TestFalse(TEXT("Restart destroys old actor"), IsValid(First[0]));
	Restarted[0]->Destroy();
	Point->Tick(0);
	TestEqual(TEXT("No refill"), W.OwnedActors(Point).Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FR10Redirect, "vBoR.R10.E5Redirect", R10TestFlags)
bool FR10Redirect::RunTest(const FString&)
{
	const FCoreRedirectObjectName OldClass(FString(TEXT("/Script/vBoR.VBoRBoxStand")));
	const FCoreRedirectObjectName NewClass(AVBoRObjectSpawnPoint::StaticClass());
	TestTrue(TEXT("Old class redirects to spawn point"), FCoreRedirects::GetRedirectedName(ECoreRedirectFlags::Type_Class, OldClass) == NewClass);
	return true;
}
#endif
