#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "P3VehicleMock.h"
#include "../P2Probe/P2dTestFixture.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "Interaction/IsdkGrabbableComponent.h"
#include "IsdkFunctionLibrary.h"
#include "Misc/OutputDevice.h"
#include "Misc/OutputDeviceRedirector.h"

namespace
{
struct FP5xWorld
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	AP3VehicleMock* Car = nullptr;
	AP2cTrackedPanel* Panel = nullptr;
	AP2bTrackedPart* Handle = nullptr;
	void Play() { World->InitializeActorsForPlay(FURL()); World->GetWorldSettings()->NotifyBeginPlay(); }
	void Console()
	{
		Car = World->SpawnActor<AP3VehicleMock>(); Play();
		// Copy actual console; replace tracking only (P5x E2/E3)
		const AVBoRHingedPanel* Source = Car->ConsoleLid;
		Panel = World->SpawnActorDeferred<AP2cTrackedPanel>(AP2cTrackedPanel::StaticClass(), FTransform::Identity);
		Panel->HingePivot->SetRelativeTransform(Source->HingePivot->GetRelativeTransform());
		Panel->MeshRestFromRoot = Source->MeshRestFromRoot;
		Panel->bUseLatch = Source->bUseLatch; Panel->bAllowPalmOpening = Source->bAllowPalmOpening;
		Panel->MaxAngleDeg = Source->MaxAngleDeg;
		const AVBoRAxisPart* Part = Car->ConsoleHandle;
		Handle = World->SpawnActorDeferred<AP2bTrackedPart>(AP2bTrackedPart::StaticClass(), FTransform::Identity);
		Handle->AxisPivot->SetRelativeTransform(Part->AxisPivot->GetRelativeTransform());
		Handle->MeshRestFromRoot = Part->MeshRestFromRoot;
		Handle->Movement = Part->Movement; Handle->Axis = Part->Axis; Handle->bReverse = Part->bReverse;
		Handle->MaxTravelCm = Part->MaxTravelCm; Handle->Release = Part->Release;
		Handle->OpenThreshold = Part->OpenThreshold; Handle->DoorHandleSide = Part->DoorHandleSide;
		Handle->FinishSpawning(FTransform::Identity);
		Panel->OutsideAxisHandle = Handle; Panel->FinishSpawning(FTransform::Identity);
	}
	~FP5xWorld() { World->EndPlay(EEndPlayReason::Quit); World->DestroyWorld(false); }
};
FVBoRDoorHand P5xHand(double Angle, double Y = 0, FVector Normal = FVector(0,-1,0))
{
	const FQuat R(FVector::ZAxisVector, FMath::DegreesToRadians(Angle));
	FVBoRDoorHand Hand; Hand.bTracked = true; Hand.PalmNormal = R.RotateVector(Normal);
	for (FVector& Point : Hand.Points) { Point = R.RotateVector(FVector(30,Y,0)); }
	return Hand;
}
void P5xPress(AVBoRAxisPart* Handle, double Cm)
{
	const FVector Rest = Handle->PartMesh->GetComponentLocation();
	FP2dTestAccess::Begin(Handle, 0, UIsdkFunctionLibrary::MakeGrabPoseStruct(11, Rest, FQuat::Identity));
	FP2dTestAccess::Move(Handle, UIsdkFunctionLibrary::MakeGrabPoseStruct(11, Rest-FVector(Cm,0,0), FQuat::Identity));
	Handle->Tick(0.01f);
}
struct FP5xWarnings : FOutputDevice
{
	int32 Count = 0;
	FP5xWarnings() { GLog->AddOutputDevice(this); }
	~FP5xWarnings() { GLog->RemoveOutputDevice(this); }
	virtual void Serialize(const TCHAR* Message, ELogVerbosity::Type Verbosity, const FName& Category) override
	{
		if (Category == TEXT("LogAxisDoor") && FString(Message).Contains(TEXT("Latched panel has no opening handle"))) { ++Count; }
	}
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5xComponents, "vBoR.P5x.E1PanelComponents", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5xComponents::RunTest(const FString&)
{
	TArray<UClass*> Classes = {AVBoRHingedPanel::StaticClass(), AVBoRHingedDoor::StaticClass()};
	for (const TCHAR* Name : {TEXT("BP_Door_FL"), TEXT("BP_ConsoleLid"), TEXT("BP_Glovebox")})
	{
		const FString Path = FString::Printf(TEXT("/Game/Main/Car/Parts/%s.%s_C"),Name,Name);
		UClass* Class = LoadClass<AActor>(nullptr,*Path);
		if (TestNotNull(Name,Class)) { Classes.Add(Class); }
	}
	for (UClass* Class : Classes)
	{
		const auto* Panel = Class->GetDefaultObject<AVBoRHingedPanel>();
		TArray<UBoxComponent*> Boxes; Panel->GetComponents(Boxes);
		TArray<UIsdkGrabbableComponent*> Grabs; Panel->GetComponents(Grabs);
		TestEqual(Class->GetName()+TEXT(" one push box"),Boxes.Num(),1);
		if (Boxes.Num() == 1) { TestTrue(TEXT("Only PushFaceBox"),Boxes[0] == Panel->PushFaceBox); }
		TestEqual(Class->GetName()+TEXT(" no built-in grab"),Grabs.Num(),0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5xPartialPress, "vBoR.P5x.E2PartialConsolePress", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5xPartialPress::RunTest(const FString&)
{
	FP5xWorld W; W.Console(); W.Panel->Hands[0] = P5xHand(0);
	P5xPress(W.Handle,0.5); W.Panel->Tick(0.1f);
	TestTrue(TEXT("Handle travels 0.5 cm"),FMath::IsNearlyEqual(W.Handle->GetOpenRatio()*W.Handle->MaxTravelCm,0.5f));
	TestTrue(TEXT("Still locked"),W.Panel->GetMotion().bLocked);
	TestEqual(TEXT("No unlock"),W.Panel->GetMotion().Metrics.Unlocks,0);
	TestEqual(TEXT("Lid stays closed deg"),W.Panel->GetMotion().AngleDeg,0.0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5xFullPress, "vBoR.P5x.E3FullConsolePressAndRelease", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5xFullPress::RunTest(const FString&)
{
	FP5xWorld W; W.Console(); W.Panel->Hands[0] = P5xHand(0);
	P5xPress(W.Handle,0.75);
	TestFalse(TEXT("Full press unlocks"),W.Panel->GetMotion().bLocked);
	TestEqual(TEXT("One unlock"),W.Panel->GetMotion().Metrics.Unlocks,1);
	TestEqual(TEXT("Same hand owns lid"),W.Panel->GetMotion().GrabHand,0);
	W.Panel->Hands[0] = P5xHand(35); W.Panel->Tick(1);
	const double Angle = W.Panel->GetMotion().AngleDeg;
	TestTrue(TEXT("Same hand raises lid"),Angle > 0);
	W.Panel->bPalmClosed[0] = false; W.Panel->Tick(0.1f); W.Handle->Tick(1);
	TestEqual(TEXT("Release ends lid grab"),W.Panel->GetMotion().GrabHand,INDEX_NONE);
	TestTrue(TEXT("Handle returns to zero cm"),FMath::IsNearlyZero(W.Handle->GetOpenRatio()));
	TestEqual(TEXT("Lid stays at release angle"),W.Panel->GetMotion().AngleDeg,Angle);
	AVBoRHingedPanel* Actual = W.Car->ConsoleLid; AVBoRAxisPart* Handle = W.Car->ConsoleHandle;
	W.Car->Destroy();
	TestTrue(TEXT("Mock destroys console and handle"),Actual->IsActorBeingDestroyed() && Handle->IsActorBeingDestroyed());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5xNoHandle, "vBoR.P5x.E4NoHandleUnlockAndPush", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5xNoHandle::RunTest(const FString&)
{
	AddExpectedMessagePlain(TEXT("Latched panel has no opening handle"), ELogVerbosity::Warning);
	FP5xWorld W; FP5xWarnings Warnings;
	W.Panel = W.World->SpawnActorDeferred<AP2cTrackedPanel>(AP2cTrackedPanel::StaticClass(),FTransform::Identity);
	W.Panel->bUseLatch = true; W.Panel->FinishSpawning(FTransform::Identity); W.Play();
	W.Panel->Tick(0.1f); GLog->FlushThreadedLogs();
	TestEqual(TEXT("One BeginPlay warning"),Warnings.Count,1);
	TestNull(TEXT("No outside handle"),W.Panel->OutsideAxisHandle.Get());
	TestNull(TEXT("No inside handle"),W.Panel->InsideAxisHandle.Get());
	TArray<UIsdkGrabbableComponent*> Grabs; W.Panel->GetComponents(Grabs);
	TestEqual(TEXT("No grab target"),Grabs.Num(),0);
	TestTrue(TEXT("Button unlock allowed"),W.Panel->Unlock());
	W.Panel->Hands[0] = P5xHand(0,8); W.Panel->Tick(0.1f);
	W.Panel->Hands[0] = P5xHand(0,4); W.Panel->Tick(0.1f);
	TestEqual(TEXT("Outside face accepts push"),W.Panel->GetMotion().PushFace,0);
	TestEqual(TEXT("Outside push starts once"),W.Panel->GetMotion().Metrics.PushStarts[0][0],1);
	return true;
}
#endif
