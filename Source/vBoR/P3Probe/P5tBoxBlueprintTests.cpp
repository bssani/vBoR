#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "P5tTestFixture.h"
#include "../Interaction/VBoRBoxStand.h"
#include "../Interaction/VBoRLoadTarget.h"
#include "Components/StaticMeshComponent.h"
#include "EdGraph/EdGraph.h"
#include "Engine/Blueprint.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/WorldSettings.h"
#include "Misc/OutputDevice.h"
#include "Misc/OutputDeviceRedirector.h"
#include "UObject/UnrealType.h"

struct FP5tTestAccess
{
	static bool Grab(AVBoRCarryObject* Box) { return Box->BeginCarry(0, nullptr, Box->GetActorTransform()); }
	static void Pivot(const AVBoRCarryObject* Box, const FBox& Bounds, const FVector& Scale)
	{ Box->WarnIfOffCenterPivot(Bounds, Scale); }
};

namespace
{
struct FP5tWorld
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	void Play() { World->InitializeActorsForPlay(FURL()); World->GetWorldSettings()->NotifyBeginPlay(); }
	~FP5tWorld() { World->EndPlay(EEndPlayReason::Quit); World->DestroyWorld(false); }
	AVBoRBoxStand* Stand(UClass* Class)
	{
		auto* S = World->SpawnActorDeferred<AVBoRBoxStand>(AVBoRBoxStand::StaticClass(), FTransform(FVector(100, 200, 10)));
		S->BoxClass = Class;
		S->LoadTarget = World->SpawnActor<AVBoRLoadTarget>();
		S->FinishSpawning(S->GetActorTransform());
		return S;
	}
};
struct FP5tWarnings : FOutputDevice
{
	TArray<FString> Messages;
	FP5tWarnings() { GLog->FlushThreadedLogs(); GLog->AddOutputDevice(this); }
	~FP5tWarnings() { GLog->FlushThreadedLogs(); GLog->RemoveOutputDevice(this); }
	// 비동기 로그 전달이 끝난 뒤 횟수를 읽음 (P5t E7·E8)
	int32 Count() { GLog->FlushThreadedLogs(); return Messages.Num(); }
	virtual void Serialize(const TCHAR* Text, ELogVerbosity::Type Verbosity, const FName&) override
	{
		const FString S(Text);
		// 예상 Warning 은 UE 가 Verbose 로 낮춰 전달함 (P5t E7·E8, AutomationTest.cpp:289-291)
		if ((Verbosity == ELogVerbosity::Warning || Verbosity == ELogVerbosity::Verbose)
			&& (S.StartsWith(TEXT("Carry box '")) || S.Contains(TEXT("BoxClass is empty")))) { Messages.Add(S); }
	}
};
UClass* P5tBoxClass()
{ return LoadClass<AVBoRCarryObject>(nullptr, TEXT("/Game/Main/Car/Parts/BP_Box.BP_Box_C")); }
UWorld* P5tMain(FAutomationTestBase& Test)
{
	UPackage* Package = LoadPackage(nullptr, TEXT("/Game/Main/L_Main"), LOAD_None);
	UWorld* W = Package ? UWorld::FindWorldInPackage(Package) : nullptr;
	Test.TestNotNull(TEXT("L_Main loads"), W);
	return W;
}
int32 P5tBoxes(UWorld* W, AActor* Stand)
{
	int32 Count = 0;
	for (TActorIterator<AVBoRCarryObject> It(W); It; ++It) { Count += It->GetOwner() == Stand; }
	return Count;
}
void P5tPlacement(FAutomationTestBase& Test, AVBoRBoxStand* Stand, const FVector& Size)
{
	const auto* Box = Stand->SourceBox.Get();
	if (!Test.TestNotNull(TEXT("Stand spawned box"), Box)) { return; }
	const FBox Bounds = Box->GetCollisionBoundsInPose(Box->GetActorTransform(), Stand->GetActorTransform());
	Test.TestTrue(TEXT("Collision dimensions use box class cm"), Bounds.IsValid && Bounds.GetSize().Equals(Size, .02));
	Test.TestTrue(TEXT("Box bottom is stand top + 0.01 cm"), FMath::IsNearlyEqual(Bounds.Min.Z, double(Stand->StandTopHeightCm) + .01, .02));
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5tAsset, "vBoR.P5t.E1BoxBlueprint", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5tAsset::RunTest(const FString&)
{
	UClass* Class = P5tBoxClass(); if (!TestNotNull(TEXT("BP_Box class"), Class)) { return false; }
	TestTrue(TEXT("Exact native parent"), Class->GetSuperClass() == AVBoRCarryObject::StaticClass());
	const auto* BP = Cast<UBlueprint>(Class->ClassGeneratedBy);
	if (!TestNotNull(TEXT("Blueprint source"), BP)) { return false; }
	for (const UEdGraph* Graph : BP->UbergraphPages) { TestEqual(TEXT("Empty Event Graph"), Graph->Nodes.Num(), 0); }
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5tMainStand, "vBoR.P5t.E2MainStandClass", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5tMainStand::RunTest(const FString&)
{
	UWorld* W = P5tMain(*this); UClass* Class = P5tBoxClass();
	if (!W || !TestNotNull(TEXT("BP_Box class"), Class)) { return false; }
	int32 Count = 0;
	for (ULevel* Level : W->GetLevels()) { for (AActor* Actor : Level->Actors)
	{
		if (auto* Stand = Cast<AVBoRBoxStand>(Actor))
		{
			++Count;
			TestTrue(TEXT("Stand tag retained"), Stand->ActorHasTag(TEXT("Car.BoxStand")));
			TestTrue(TEXT("Main stand uses BP_Box_C"), Stand->BoxClass.Get() == Class);
		}
	} }
	TestEqual(TEXT("One stand"), Count, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5tSize, "vBoR.P5t.E3ClassSizeAndPolicies", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5tSize::RunTest(const FString&)
{
	FP5tWorld W; auto* S = W.Stand(AP5tSizedBox::StaticClass()); W.Play();
	P5tPlacement(*this, S, FVector(20, 30, 40));
	const auto* Box = S->SourceBox.Get(); if (!Box) { return false; }
	TestTrue(TEXT("Class size retained"), Box->BoxSizeCm.Equals(FVector(20, 30, 40)));
	TestTrue(TEXT("Target assigned"), Box->LoadTarget == S->LoadTarget);
	TestTrue(TEXT("Fixture starts with assist enabled"), GetDefault<AP5tSizedBox>()->bUsePlacementAssist);
	TestFalse(TEXT("Stand disables assist"), Box->bUsePlacementAssist);
	TestTrue(TEXT("Floor destruction enabled"), Box->bDestroyAtFloor);
	TestEqual(TEXT("Floor height is stand Z cm"), Box->FloorHeightCm, float(S->GetActorLocation().Z));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5tSupply, "vBoR.P5t.E4ClassSizeSupplySlot", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5tSupply::RunTest(const FString&)
{
	FP5tWorld W; auto* S = W.Stand(AP5tSizedBox::StaticClass()); W.Play();
	auto* First = S->SourceBox.Get(); if (!TestNotNull(TEXT("Source"), First)) { return false; }
	const FTransform Start = First->GetActorTransform();
	TestTrue(TEXT("Grab accepted"), FP5tTestAccess::Grab(First));
	First->SetActorLocation(Start.GetLocation() + FVector(0, 0, 41)); S->Tick(0);
	TestEqual(TEXT("40 + 1 cm boundary still occupied"), P5tBoxes(W.World, S), 1);
	First->SetActorLocation(Start.GetLocation() + FVector(0, 0, 50)); S->Tick(0);
	TestEqual(TEXT("50 cm lift supplies one box"), P5tBoxes(W.World, S), 2);
	if (TestNotNull(TEXT("Replacement"), S->SourceBox.Get()))
	{ TestTrue(TEXT("Replacement at original pose"), S->SourceBox->GetActorTransform().Equals(Start)); }
	S->Tick(0); TestEqual(TEXT("No duplicate supply"), P5tBoxes(W.World, S), 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5tBPSpawn, "vBoR.P5t.E5BlueprintSpawn", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5tBPSpawn::RunTest(const FString&)
{
	UClass* Class = P5tBoxClass(); if (!TestNotNull(TEXT("BP_Box class"), Class)) { return false; }
	FP5tWorld W; auto* S = W.Stand(Class); W.Play();
	if (!TestNotNull(TEXT("BP source"), S->SourceBox.Get())) { return false; }
	TestTrue(TEXT("Spawned BP_Box_C"), S->SourceBox->GetClass() == Class);
	const auto* Defaults = Class->GetDefaultObject<AVBoRCarryObject>();
	P5tPlacement(*this, S, Defaults->BoxSizeCm.GetAbs().ComponentMax(FVector(1)));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5tDefaults, "vBoR.P5t.E6ApproximateDefaults", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5tDefaults::RunTest(const FString&)
{
	UWorld* W = P5tMain(*this); if (!W) { return false; }
	bool bApprox = false;
#if WITH_EDITOR
	for (ULevel* Level : W->GetLevels()) { for (AActor* Actor : Level->Actors)
	{ bApprox |= Actor && Actor->GetFolderPath().ToString().StartsWith(TEXT("Approx")); } }
#endif
	if (!bApprox) { AddInfo(TEXT("L_Main has no Approx folder; defaults comparison skipped (CAD state)")); return true; }
	UClass* Class = P5tBoxClass(); if (!TestNotNull(TEXT("BP_Box class"), Class)) { return false; }
	const auto* Actual = Class->GetDefaultObject<AVBoRCarryObject>();
	const auto* Expected = GetDefault<AVBoRCarryObject>();
	TestTrue(TEXT("CarryMesh uses native mesh"), Actual->Mesh->GetStaticMesh() == Expected->Mesh->GetStaticMesh());
	for (TFieldIterator<FProperty> It(AVBoRCarryObject::StaticClass()); It; ++It)
	{
		FProperty* P = *It;
		if (P->GetOwnerClass()->GetName().StartsWith(TEXT("VBoR")) && P->HasAnyPropertyFlags(CPF_Edit) && !P->IsA<FObjectPropertyBase>())
		{ TestTrue(TEXT("Native setting ") + P->GetName(), P->Identical_InContainer(Expected, Actual)); }
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5tPivot, "vBoR.P5t.E7PivotWarning", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5tPivot::RunTest(const FString&)
{
	FP5tWarnings Warnings; FP5tWorld W;
	auto* Box = W.World->SpawnActor<AVBoRCarryObject>(); W.Play();
	TestEqual(TEXT("Default Cube BeginPlay has no pivot warning"), Warnings.Count(), 0);
	const auto BoundsAt = [](double X) { return FBox(FVector(X - 5, -5, -5), FVector(X + 5, 5, 5)); };
	FP5tTestAccess::Pivot(Box, BoundsAt(.5), FVector(1));
	FP5tTestAccess::Pivot(Box, BoundsAt(1), FVector(1));
	TestEqual(TEXT("0.5 and exactly 1 cm have no warning"), Warnings.Count(), 0);
	AddExpectedMessage(TEXT("mesh bounds center is 2.00 cm"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 1);
	FP5tTestAccess::Pivot(Box, BoundsAt(2), FVector(1));
	TestEqual(TEXT("2 cm warns once"), Warnings.Count(), 1);
	AddExpectedMessage(TEXT("mesh bounds center is 3.00 cm"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 1);
	FP5tTestAccess::Pivot(Box, BoundsAt(1), FVector(3));
	TestEqual(TEXT("Scale converts mesh units to cm"), Warnings.Count(), 2);
	for (const FString& Message : Warnings.Messages)
	{
		TestTrue(TEXT("Warning names box"), Message.Contains(Box->GetName()));
		TestTrue(TEXT("Warning explains pivot correction"), Message.Contains(TEXT("move the mesh pivot to the box center")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5tEmpty, "vBoR.P5t.E8EmptyClass", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5tEmpty::RunTest(const FString&)
{
	FP5tWarnings Warnings; FP5tWorld W; auto* S = W.Stand(nullptr);
	AddExpectedMessage(TEXT("BoxClass is empty"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 1);
	W.Play(); TestEqual(TEXT("BeginPlay warns once"), Warnings.Count(), 1);
	if (!Warnings.Messages.IsEmpty()) { TestTrue(TEXT("Warning names stand"), Warnings.Messages[0].Contains(S->GetName())); }
	TestEqual(TEXT("No initial box"), P5tBoxes(W.World, S), 0);
	S->ResetStand(); S->Tick(1);
	TestNull(TEXT("No source after reset and tick"), S->SourceBox.Get());
	TestEqual(TEXT("No boxes after reset and tick"), P5tBoxes(W.World, S), 0);
	TestEqual(TEXT("Warning not repeated"), Warnings.Count(), 1);
	return true;
}
#endif
