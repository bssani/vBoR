#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "../Interaction/VBoRPawn.h"
#include "../P2Probe/P2bTestFixture.h"
#include "Components/BoxComponent.h"
#include "Core/IsdkConditionalGroupAll.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "HandPoseDetection/IsdkHandFingerPinchGrabRecognizer.h"
#include "HandPoseDetection/IsdkHandPalmGrabRecognizer.h"
#include "Interaction/GrabDetectors/IsdkHandGrabDetector.h"
#include "Interaction/IsdkGrabberComponent.h"
#include "Rig/IsdkGrabInteractionRigComponent.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"

namespace
{
	struct FP5yWorld
	{
		UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
		void Play() { World->InitializeActorsForPlay(FURL()); World->GetWorldSettings()->NotifyBeginPlay(); }
		~FP5yWorld() { World->EndPlay(EEndPlayReason::Quit); World->DestroyWorld(false); }
	};

	UClass* P5yPawnClass(bool Blueprint)
	{
		return Blueprint ? LoadClass<AVBoRPawn>(nullptr, TEXT("/Game/Main/Core/BP_vBoRPawn.BP_vBoRPawn_C")) : AVBoRPawn::StaticClass();
	}

	UObject* P5yObject(UObject* Owner, const TCHAR* Name)
	{
		const auto* Property = FindFProperty<FObjectPropertyBase>(Owner->GetClass(), Name);
		return Property ? Property->GetObjectPropertyValue_InContainer(Owner) : nullptr;
	}

	TMap<FString, UObject*> P5yObjects(FAutomationTestBase& Test, AVBoRPawn* Pawn)
	{
		TMap<FString, UObject*> Result;
		for (int32 Hand = 0; Hand < 2; ++Hand)
		{
			auto* Rig = Pawn->GetHandRig(Hand);
			for (UObject* Owner : {static_cast<UObject*>(Rig), static_cast<UObject*>(Rig->GetGrabInteraction())})
			{
				// Inspect every UObject default subobject reference at both levels (P5y D1)
				for (TFieldIterator<FObjectPropertyBase> It(Owner->GetClass()); It; ++It)
				{
					UObject* Object = It->GetObjectPropertyValue_InContainer(Owner);
					if (!Object || Object->IsA<UActorComponent>() || (!Object->HasAnyFlags(RF_DefaultSubObject)
						&& !Object->GetOuter()->IsA<UActorComponent>())) { continue; }
					const FString Key = FString::Printf(TEXT("Hand%d.%s.%s"), Hand, Owner == Rig ? TEXT("Rig") : TEXT("GrabRig"), *It->GetName());
					Result.Add(Key, Object);
					const FString Detail = FString::Printf(TEXT("%s -> %s"), *Key, *Object->GetPathName());
					Test.TestTrue(*FString::Printf(TEXT("Own outer: %s"), *Detail), Object->GetOuter() == Owner);
					Test.TestTrue(*FString::Printf(TEXT("Own Pawn chain: %s"), *Detail), Object->IsIn(Pawn));
					Test.TestFalse(*FString::Printf(TEXT("Not a template: %s"), *Detail), Object->IsTemplate());
				}
			}
			for (const TCHAR* Name : {TEXT("PalmGrabRecognizer"), TEXT("PinchGrabRecognizer")})
			{
				Test.TestNotNull(Name, P5yObject(Rig, Name));
				Test.TestTrue(TEXT("Recognizer included in inspection"), Result.Contains(FString::Printf(TEXT("Hand%d.Rig.%s"), Hand, Name)));
			}
			for (const TCHAR* Name : {TEXT("EnabledConditionalGroup"), TEXT("IsActiveConditional")})
			{
				Test.TestNotNull(Name, P5yObject(Rig->GetGrabInteraction(), Name));
				Test.TestTrue(TEXT("Conditional included in inspection"), Result.Contains(FString::Printf(TEXT("Hand%d.GrabRig.%s"), Hand, Name)));
			}
		}
		return Result;
	}

	void P5yDistinct(FAutomationTestBase& Test, const TMap<FString, UObject*>& First, const TMap<FString, UObject*>& Second)
	{
		Test.TestEqual(TEXT("Same subobject inventory"), Second.Num(), First.Num());
		for (const auto& Entry : First)
		{
			UObject* const* Other = Second.Find(Entry.Key);
			Test.TestTrue(*FString::Printf(TEXT("Distinct %s; first=%s second=%s"), *Entry.Key,
				*GetPathNameSafe(Entry.Value), Other ? *GetPathNameSafe(*Other) : TEXT("missing")), Other && *Other != Entry.Value);
		}
	}

	void P5yInitialState(FAutomationTestBase& Test, AVBoRPawn* Pawn)
	{
		for (int32 Hand = 0; Hand < 2; ++Hand)
		{
			auto* Rig = Pawn->GetHandRig(Hand);
			auto* Palm = Cast<UIsdkHandPalmGrabRecognizer>(P5yObject(Rig, TEXT("PalmGrabRecognizer")));
			auto* Pinch = Cast<UIsdkHandFingerPinchGrabRecognizer>(P5yObject(Rig, TEXT("PinchGrabRecognizer")));
			if (Test.TestNotNull(TEXT("Palm recognizer"), Palm)) { Test.TestFalse(TEXT("Palm starts inactive"), Palm->IsActive()); }
			if (Test.TestNotNull(TEXT("Pinch recognizer"), Pinch)) { Test.TestFalse(TEXT("Pinch starts inactive"), Pinch->IsActive()); }
		}
	}

	int32 P5yConditionCount(AVBoRPawn* Pawn, int32 Hand)
	{
		TArray<UIsdkConditional*> Conditions;
		Pawn->GetHandRig(Hand)->GetGrabInteraction()->GetEnabledConditional()->GetConditionals(Conditions);
		return Conditions.Num();
	}

	bool P5ySameWorld(FAutomationTestBase& Test, bool Blueprint)
	{
		UClass* Class = P5yPawnClass(Blueprint);
		if (!Test.TestNotNull(TEXT("Pawn class loads"), Class)) { return false; }
		FP5yWorld W;
		auto* First = W.World->SpawnActor<AVBoRPawn>(Class);
		auto* Second = W.World->SpawnActor<AVBoRPawn>(Class, FVector(1000), FRotator::ZeroRotator);
		if (!Test.TestNotNull(TEXT("First Pawn spawns"), First) || !Test.TestNotNull(TEXT("Second Pawn spawns"), Second)) { return false; }
		P5yDistinct(Test, P5yObjects(Test, First), P5yObjects(Test, Second));
		W.Play();
		P5yDistinct(Test, P5yObjects(Test, First), P5yObjects(Test, Second));
		return true;
	}

	void P5yPalmSignal(FAutomationTestBase& Test, FP5yWorld& W, AVBoRPawn* Pawn)
	{
		// Real near overlap; force active only for absent HMD tracking (P5y E3)
		auto* Part = W.World->SpawnActor<AP2bTrackedPart>(FVector(300, 0, 100), FRotator::ZeroRotator);
		for (int32 Hand = 0; Hand < 2; ++Hand)
		{
			auto* Rig = Pawn->GetHandRig(Hand);
			auto* Grabber = Rig->GetGrabber();
			Grabber->Activate(true);
			const FVector Offset = Grabber->GetHandGrabDetector()->GetPalmGrabCollider()->GetComponentLocation() - Grabber->GetComponentLocation();
			Grabber->SetWorldLocation(Part->GrabBox->GetComponentLocation() - Offset);
			Grabber->TickComponent(0.016f, LEVELTICK_All, nullptr);
			Test.TestTrue(TEXT("Real palm overlap finds part"), Grabber->GetHandGrabDetector()->GetHoveredGrabbables().Contains(Part->Grabbable));
			Test.TestFalse(TEXT("No select before signal"), Grabber->IsGrabbing());
			Rig->OnPalmSelectDelegate.Broadcast(Rig);
			Test.TestTrue(TEXT("Palm signal reaches grabber selection"), Grabber->GetGrabbedComponent() == Part->Grabbable
				&& Grabber->IsInputMethodActive(EIsdkGrabInputMethod::Palm));
			Rig->OnPalmUnselectDelegate.Broadcast(Rig);
			Test.TestFalse(TEXT("Palm unselect reaches grabber release"), Grabber->IsGrabbing());
			Grabber->SetWorldLocation(FVector(1000));
		}
	}

	bool P5yRepeatWorld(FAutomationTestBase& Test, bool Blueprint, bool Signal)
	{
		UClass* Class = P5yPawnClass(Blueprint);
		if (!Test.TestNotNull(TEXT("Pawn class loads"), Class)) { return false; }
		TMap<FString, UObject*> FirstObjects;
		// Retain first objects across world destruction (no allocator address reuse)
		TArray<TStrongObjectPtr<UObject>> KeepFirst;
		int32 FirstCounts[2] = {};
		{
			FP5yWorld First;
			auto* Pawn = First.World->SpawnActor<AVBoRPawn>(Class);
			if (!Test.TestNotNull(TEXT("First Pawn spawns"), Pawn)) { return false; }
			P5yInitialState(Test, Pawn);
			First.Play();
			FirstObjects = P5yObjects(Test, Pawn);
			for (const auto& Entry : FirstObjects) { KeepFirst.Emplace(Entry.Value); }
			for (int32 Hand = 0; Hand < 2; ++Hand) { FirstCounts[Hand] = P5yConditionCount(Pawn, Hand); }
		}
		{
			FP5yWorld Second;
			auto* Pawn = Second.World->SpawnActor<AVBoRPawn>(Class);
			if (!Test.TestNotNull(TEXT("Second Pawn spawns"), Pawn)) { return false; }
			P5yInitialState(Test, Pawn);
			P5yDistinct(Test, FirstObjects, P5yObjects(Test, Pawn));
			Second.Play();
			for (int32 Hand = 0; Hand < 2; ++Hand)
			{
				Test.TestTrue(TEXT("First Play binds conditions"), FirstCounts[Hand] > 0);
				Test.TestEqual(TEXT("Second Play does not accumulate conditions"), P5yConditionCount(Pawn, Hand), FirstCounts[Hand]);
			}
			P5yInitialState(Test, Pawn);
			if (Signal) { P5yPalmSignal(Test, Second, Pawn); }
		}
		return true;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5yBPInstances, "vBoR.P5y.E1BlueprintInstances", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5yBPInstances::RunTest(const FString&) { return P5ySameWorld(*this, true); }

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5yNativeInstances, "vBoR.P5y.E1NativeInstances", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5yNativeInstances::RunTest(const FString&) { return P5ySameWorld(*this, false); }

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5yBPRepeat, "vBoR.P5y.E2BlueprintRepeatWorld", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5yBPRepeat::RunTest(const FString&) { return P5yRepeatWorld(*this, true, false); }

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5yPalmSignal, "vBoR.P5y.E3SecondWorldPalmSignal", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5yPalmSignal::RunTest(const FString&) { return P5yRepeatWorld(*this, true, true); }

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5yNativeRepeat, "vBoR.P5y.E5NativeRepeatWorld", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5yNativeRepeat::RunTest(const FString&) { return P5yRepeatWorld(*this, false, true); }
#endif
