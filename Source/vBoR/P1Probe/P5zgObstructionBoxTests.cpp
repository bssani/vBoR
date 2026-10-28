#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "P4bTestObserver.h"
#include "../Interaction/VBoRObstructionBoxComponent.h"
#include "../Interaction/VBoRCarryObject.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/WorldSettings.h"
#include "Misc/OutputDevice.h"
#include "Misc/OutputDeviceRedirector.h"

namespace
{
struct FP5zgWorld
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	AVBoRPoweredPanel* Gate;
	UP4bTestObserver* Observer = nullptr;
	FP5zgWorld()
	{
		World->SpawnActor<APlayerController>();
		Gate = World->SpawnActor<AVBoRPoweredPanel>();
		Gate->bStartOpen = true;
		// Bulky bounds mimic curved CAD enclosing empty trunk space (P5zg E1)
		Gate->MeshRestFromRoot.SetScale3D(FVector(1.2, 1.3, 0.7));
		Gate->OnConstruction(Gate->GetActorTransform());
	}
	~FP5zgWorld() { World->EndPlay(EEndPlayReason::Quit); World->DestroyWorld(false); }
	UVBoRObstructionBoxComponent* Shape(const TCHAR* Name = TEXT("GateSkin"),
		FVector Position = FVector(-2.5, 0, -35), FVector Extent = FVector(2.5, 65, 35), USceneComponent* Parent = nullptr)
	{
		auto* Component = NewObject<UVBoRObstructionBoxComponent>(Gate, Name);
		Gate->AddInstanceComponent(Component);
		Component->SetupAttachment(Parent ? Parent : Gate->HingePivot.Get());
		Component->SetRelativeLocation(Position);
		Component->SetBoxExtent(Extent);
		Component->RegisterComponent();
		return Component;
	}
	void Begin()
	{
		Gate->OnConstruction(Gate->GetActorTransform());
		World->InitializeActorsForPlay(FURL());
		World->GetWorldSettings()->NotifyBeginPlay();
		Observer = NewObject<UP4bTestObserver>(World);
		Gate->OnObstructed.AddDynamic(Observer, &UP4bTestObserver::Obstruction);
		Gate->OnOpened.AddDynamic(Observer, &UP4bTestObserver::Open);
		Gate->OnClosed.AddDynamic(Observer, &UP4bTestObserver::Close);
	}
	AVBoRCarryObject* Box(FVector Position, FVector Size = FVector(20))
	{
		const FTransform Pose(Gate->GetActorQuat(), Gate->GetActorTransform().TransformPosition(Position));
		auto* Object = World->SpawnActorDeferred<AVBoRCarryObject>(AVBoRCarryObject::StaticClass(), Pose,
			nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		Object->BoxSizeCm = Size;
		Object->FinishSpawning(Pose);
		return Object;
	}
};

struct FP5zgLogs : FOutputDevice
{
	TArray<FString> Lines;
	FP5zgLogs() { GLog->FlushThreadedLogs(); GLog->AddOutputDevice(this); }
	~FP5zgLogs() { GLog->RemoveOutputDevice(this); }
	virtual void Serialize(const TCHAR* Text, ELogVerbosity::Type Verbosity, const FName& Category) override
	{ if (Category == TEXT("LogPoweredPanel") && Verbosity == ELogVerbosity::Log) { Lines.Add(Text); } }
	int32 Count(const FString& Text)
	{
		GLog->FlushThreadedLogs();
		int32 Matches = 0;
		for (const FString& Line : Lines) { Matches += Line.Contains(Text); }
		return Matches;
	}
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5zgClear, "vBoR.P5zg.E1CurvedBoundsClear", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5zgClear::RunTest(const FString&)
{
	FP5zgWorld W; W.Shape(); W.Begin();
	auto* Box = W.Box(FVector(40, 0, 110)); const FTransform Before = Box->GetActorTransform();
	W.Gate->RequestClose(); W.Gate->Tick(8);
	TestTrue(TEXT("Skin boxes allow fully loaded trunk to close"), W.Gate->GetState() == EVBoRPoweredState::Closed);
	TestEqual(TEXT("No obstruction"), W.Gate->GetObstructionCount(), 0);
	TestEqual(TEXT("Closed arrival once"), W.Observer->Closed, 1);
	TestTrue(TEXT("Cargo unchanged"), Box->GetActorTransform().Equals(Before));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5zgFallback, "vBoR.P5zg.E2MeshFallback", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5zgFallback::RunTest(const FString&)
{
	FP5zgWorld W; W.Begin(); auto* Box = W.Box(FVector(40, 0, 110));
	W.Gate->RequestClose(); W.Gate->Tick(8);
	TestEqual(TEXT("Legacy bulky mesh detects cargo"), W.Gate->GetObstructionCount(), 1);
	TestTrue(TEXT("Obstruction event identifies cargo"), W.Observer->HitBox == Box);
	TestTrue(TEXT("Legacy reversal"), W.Gate->GetState() == EVBoRPoweredState::Opening);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5zgSweep, "vBoR.P5zg.E3SweepAndReversal", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5zgSweep::RunTest(const FString&)
{
	float SmallAngle = 0;
	for (bool bLarge : {false, true})
	{
		FP5zgWorld W;
		W.Gate->SetActorTransform(FTransform(FRotator(0, 37, 0), FVector(200, -100, 90)));
		auto* Skin = W.Shape(); W.Begin(); auto* Box = W.Box(FVector(-25, 15, 110));
		const FTransform Before = Box->GetActorTransform();
		W.Gate->RequestClose(); W.Gate->Tick(1);
		for (int32 I = 0; I < (bLarge ? 1 : 240) && W.Observer->Obstructed == 0; ++I) { W.Gate->Tick(bLarge ? 4 : 1.0f / 60); }
		TestEqual(TEXT("One obstruction"), W.Observer->Obstructed, 1);
		TestEqual(TEXT("Count matches event"), W.Gate->GetObstructionCount(), 1);
		TestTrue(TEXT("Event cargo"), W.Observer->HitBox == Box);
		TestTrue(TEXT("Immediate reopening"), W.Gate->GetState() == EVBoRPoweredState::Opening);
		TestFalse(TEXT("Accepted skin pose never penetrates cargo"), Box->Mesh->OverlapComponent(Skin->GetComponentLocation(),
			Skin->GetComponentQuat(), FCollisionShape::MakeBox(Skin->GetScaledBoxExtent())));
		TestTrue(TEXT("Cargo never pushed"), Box->GetActorTransform().Equals(Before));
		if (bLarge) { TestTrue(TEXT("Large and small tick angles within 1.5 deg"), FMath::Abs(W.Gate->GetAngleDeg() - SmallAngle) <= 1.5f); }
		else { SmallAngle = W.Gate->GetAngleDeg(); }
		const float Angle = W.Gate->GetAngleDeg(); W.Gate->Tick(0.25);
		TestTrue(TEXT("Reopening has no wait"), W.Gate->GetAngleDeg() > Angle);
		W.Gate->Tick(4);
		TestEqual(TEXT("Open arrival once"), W.Observer->Opened, 1);
		TestEqual(TEXT("No closed arrival"), W.Observer->Closed, 0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5zgMultiple, "vBoR.P5zg.E4SecondBoxAndDescendants", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5zgMultiple::RunTest(const FString&)
{
	FP5zgWorld W;
	W.Shape(TEXT("UprightSkin"), FVector(-2.5, 0, -35), FVector(2.5, 30, 35));
	auto* Mount = NewObject<USceneComponent>(W.Gate, TEXT("SkinMount")); W.Gate->AddInstanceComponent(Mount);
	Mount->SetupAttachment(W.Gate->HingePivot); Mount->SetRelativeLocation(FVector(0, 55, 0)); Mount->RegisterComponent();
	W.Shape(TEXT("LowerSkin"), FVector(-2.5, 0, -65), FVector(2.5, 25, 5), Mount);
	W.Begin(); auto* Box = W.Box(FVector(-25, 70, 85));
	W.Gate->RequestClose(); W.Gate->Tick(8);
	TestEqual(TEXT("Second skin on descendant detects cargo"), W.Gate->GetObstructionCount(), 1);
	TestTrue(TEXT("Second skin event cargo"), W.Observer->HitBox == Box);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5zgPose, "vBoR.P5zg.E5RotationAndScale", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5zgPose::RunTest(const FString&)
{
	const FQuat Rotation = FRotator(30, 0, 0).Quaternion();
	for (int32 Case = 0; Case < 3; ++Case)
	{
		FP5zgWorld W;
		W.Gate->SetActorTransform(FTransform(FRotator(0, 32, 0), FVector(200, -80, 40), FVector(1.25)));
		W.Gate->OpenAngleDeg = 1;
		auto* Skin = W.Shape(TEXT("TiltedSkin"), FVector(0, 0, -35), FVector(2, 10, 20));
		Skin->SetRelativeRotation(Rotation); Skin->SetRelativeScale3D(FVector(Case == 2 ? -2 : 2, 1, 0.5));
		W.Begin();
		const FVector Offset = Case == 0 ? FVector(3, 0, 8) : Rotation.RotateVector(Case == 1 ? FVector(3, 0, -8) : FVector(3.5, 0, 0));
		W.Box(FVector(0, 0, 110) + Offset, FVector(1));
		W.Gate->RequestClose(); W.Gate->Tick(8);
		TestEqual(TEXT("Rotation and absolute scale determine overlap"), W.Gate->GetObstructionCount(), Case == 0 ? 0 : 1);
		TestTrue(TEXT("Clear tilted skin closes; occupied skin reverses"), W.Gate->GetState() == (Case == 0 ? EVBoRPoweredState::Closed : EVBoRPoweredState::Opening));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5zgWrongParent, "vBoR.P5zg.E6WrongParentFallback", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5zgWrongParent::RunTest(const FString&)
{
	AddExpectedMessagePlain(TEXT("MisplacedSkin: ignored obstruction box (not a descendant of HingePivot)"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 1);
	FP5zgWorld W; W.Shape(TEXT("MisplacedSkin"), FVector(-2.5, 0, 110), FVector(2.5, 65, 35), W.Gate->GetRootComponent());
	W.Begin(); W.Box(FVector(40, 0, 110)); W.Gate->RequestClose(); W.Gate->Tick(8);
	TestEqual(TEXT("Invalid attachment falls back to mesh"), W.Gate->GetObstructionCount(), 1);
	W.Gate->Tick(4); W.Gate->ResetPanel();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5zgZero, "vBoR.P5zg.E7ZeroExtentIgnored", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5zgZero::RunTest(const FString&)
{
	AddExpectedMessagePlain(TEXT("FlatSkin: ignored obstruction box (zero extent or scale)"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 1);
	FP5zgWorld W; W.Shape(TEXT("FlatSkin"), FVector(40, 0, -35), FVector(0, 65, 35)); W.Shape();
	W.Begin(); W.Box(FVector(40, 0, 110)); W.Gate->RequestClose(); W.Gate->Tick(8);
	TestEqual(TEXT("Remaining valid skin excludes cargo in bulky mesh"), W.Gate->GetObstructionCount(), 0);
	TestTrue(TEXT("Valid skin closes"), W.Gate->GetState() == EVBoRPoweredState::Closed);
	W.Gate->ResetPanel(); auto* Box = W.Box(FVector(-25, 0, 110)); W.Gate->RequestClose(); W.Gate->Tick(8);
	TestEqual(TEXT("Remaining valid skin still detects obstruction"), W.Gate->GetObstructionCount(), 1);
	TestTrue(TEXT("Valid skin event cargo"), W.Observer->HitBox == Box);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5zgIsolation, "vBoR.P5zg.E8HiddenAndNonBlocking", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5zgIsolation::RunTest(const FString&)
{
	FP5zgWorld W; auto* Skin = W.Shape(TEXT("PassThroughSkin"), FVector(100, 0, -35)); W.Begin();
	TestTrue(TEXT("Skin hidden in game"), Skin->bHiddenInGame);
	TestTrue(TEXT("Skin visible in editor"), Skin->IsVisibleInEditor());
	TestTrue(TEXT("No collision"), Skin->GetCollisionEnabled() == ECollisionEnabled::NoCollision);
	TestFalse(TEXT("No overlap events"), Skin->GetGenerateOverlapEvents());
	TestEqual(TEXT("No component tags"), Skin->ComponentTags.Num(), 0);
	const FVector Center = Skin->GetComponentLocation();
	auto* Box = W.Box(FVector(300, 0, 110));
	Box->ResetToPose(FTransform(Center + FVector(40, 0, 0)));
	TestTrue(TEXT("Begin carry"), Box->BeginCarry(0, nullptr, Box->GetActorTransform()));
	const FTransform Desired(Center - FVector(40, 0, 0));
	Box->MoveHeldPose(Desired, 1.0f / 60);
	TestFalse(TEXT("Held cargo never blocked by skin"), Box->IsBlocked());
	TestTrue(TEXT("Held cargo passes through skin"), Box->GetActorLocation().Equals(Desired.GetLocation(), 0.01));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5zgBasisLog, "vBoR.P5zg.E9BasisLog", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5zgBasisLog::RunTest(const FString&)
{
	for (bool bShapes : {false, true})
	{
		FP5zgWorld W; if (bShapes) { W.Shape(); W.Shape(TEXT("SecondSkin"), FVector(-10, 0, -70)); }
		FP5zgLogs Logs; W.Begin(); W.Gate->ResetPanel(); W.Gate->Tick(0);
		const FString Prefix = W.Gate->GetName() + TEXT(" obstruction basis: ");
		TestEqual(TEXT("One BeginPlay basis log"), Logs.Count(Prefix), 1);
		TestEqual(TEXT("Correct basis and valid count"), Logs.Count(Prefix + (bShapes ? TEXT("2 obstruction boxes") : TEXT("mesh bounds (approximate)"))), 1);
	}
	return true;
}
#endif
