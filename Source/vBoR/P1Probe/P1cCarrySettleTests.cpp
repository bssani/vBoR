#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "../Interaction/VBoRCarryObject.h"
#include "../Interaction/VBoRLoadTarget.h"
#include "P1LoadBench.h"
#include "../Interaction/VBoRHandRigPawn.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/WorldSettings.h"
#include "Interaction/IsdkGrabbableComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/OutputDevice.h"
#include "Misc/OutputDeviceRedirector.h"
#include <limits>

namespace
{
	// Count directly (automation expected messages skip normal Log, P1c E5)
	struct FLevelBlockedLog : FOutputDevice
	{
		int32 Count = 0;
		FLevelBlockedLog() { GLog->AddOutputDevice(this); }
		~FLevelBlockedLog() { GLog->RemoveOutputDevice(this); }
		int32 GetCount() { GLog->FlushThreadedLogs(); return Count; }
		virtual void Serialize(const TCHAR* Message, ELogVerbosity::Type Verbosity, const FName& Category) override
		{
			if (Category == TEXT("LogVBoRCarry") && FString(Message).Contains(TEXT("level blocked"))) { ++Count; }
		}
	};
	struct FSettleWorld
	{
		UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
		AVBoRCarryObject* Box = World->SpawnActor<AVBoRCarryObject>();
		AVBoRLoadTarget* Target = World->SpawnActor<AVBoRLoadTarget>();
		FSettleWorld()
		{
			Box->SetActorLocation(FVector(0, 0, 100));
			Target->SetActorLocation(FVector(0, 0, 100));
			Target->LoadingRegion->SetBoxExtent(FVector(150, 150, 100));
			Box->LoadTarget = Target;
		}
		AActor* Block(const FVector& Center, const FVector& Half)
		{
			AActor* Actor = World->SpawnActor<AActor>();
			UBoxComponent* Shape = NewObject<UBoxComponent>(Actor);
			Actor->SetRootComponent(Shape);
			Shape->SetBoxExtent(Half);
			Shape->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
			Shape->SetCollisionResponseToAllChannels(ECR_Block);
			Shape->RegisterComponent();
			Actor->SetActorLocation(Center);
			return Actor;
		}
		void Begin()
		{
			World->InitializeActorsForPlay(FURL());
			World->GetWorldSettings()->NotifyBeginPlay();
		}
		~FSettleWorld() { World->EndPlay(EEndPlayReason::Quit); World->DestroyWorld(false); }
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP1cLevelTest, "vBoR.P1c.LevelAndYaw", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP1cLevelTest::RunTest(const FString& Parameters)
{
	FSettleWorld F;
	F.Box->bSnapYawInLoadRegion = false; // Check existing Yaw preservation only (P5n §3 exception 3)
	F.Block(FVector(0, 0, 45), FVector(500, 500, 5));
	F.Begin();
	for (const FRotator Tilt : {FRotator(30, 0, 0), FRotator(0, 40, 20)})
	{
		F.Box->ResetToPose(FTransform(FVector(0, 0, 100)));
		F.Box->BeginCarry(0, nullptr, F.Box->GetActorTransform());
		F.Box->SetActorRotation(Tilt);
		const FVector Before = F.Box->GetActorLocation();
		F.Box->ReleaseCarry();
		TestTrue(TEXT("E1 release immediately levels without overlap"), F.Box->IsPoseClear(F.Box->GetActorTransform()));
		for (int32 Frame = 0; Frame < 60 && F.Box->bDropping; ++Frame)
		{
			F.Box->Tick(1.0f / 60);
			TestTrue(TEXT("E1 every falling pose remains clear"), F.Box->IsPoseClear(F.Box->GetActorTransform()));
		}
		const FBox Bounds = F.Box->GetCollisionBoundsInPose(F.Box->GetActorTransform(), FTransform::Identity);
		TestTrue(TEXT("E1 flat bottom reaches floor top within 0.01 cm"), FMath::IsNearlyEqual(Bounds.Min.Z, 50.0, 0.01));
		TestTrue(TEXT("E1 seated inside region"), F.Box->IsRestingInside());
		TestTrue(TEXT("E1 drop keeps XY"), FVector2D(F.Box->GetActorLocation()).Equals(FVector2D(Before), 0.001));
		TestTrue(TEXT("E2 yaw preserved and pitch/roll removed"), F.Box->GetActorRotation().Equals(FRotator(0, Tilt.Yaw, 0), 0.01));
		TestTrue(TEXT("E1 upright box up vector is vertical"), F.Box->GetActorUpVector().Equals(FVector::UpVector, 0.0001));
	}
	// Also check child mesh upright pose != world Identity (P1c D1)
	const FQuat Rest = FRotator(0, 0, 90).Quaternion();
	F.Box->ResetToPose(FTransform(Rest, FVector(0, 0, 120)));
	F.Box->BeginCarry(0, nullptr, F.Box->GetActorTransform());
	F.Box->SetActorRotation(FRotator(30, 40, 0).Quaternion() * Rest);
	F.Box->ReleaseCarry();
	TestTrue(TEXT("D1 levels relative to saved rest rotation"), F.Box->GetActorQuat().Equals(FRotator(0, 40, 0).Quaternion() * Rest, 0.0001));
	F.Box->ResetToPose(FTransform(FVector(0, 0, 120)));
	F.Box->BeginCarry(0, nullptr, F.Box->GetActorTransform());
	F.Box->Session.bHasRestRotation = false;
	F.Box->SetActorRotation(FRotator(30, 40, 0));
	F.Box->ReleaseCarry();
	TestTrue(TEXT("D1 missing rest uses world yaw"), F.Box->GetActorRotation().Equals(FRotator(0, 40, 0), 0.01));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP1cStackTest, "vBoR.P1c.FlatStack", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP1cStackTest::RunTest(const FString& Parameters)
{
	FSettleWorld F;
	F.Block(FVector(0, 0, 45), FVector(500, 500, 5));
	AVBoRCarryObject* Lower = F.World->SpawnActor<AVBoRCarryObject>();
	Lower->SetActorLocation(FVector(0, 0, 65.005));
	F.Box->SetActorLocation(FVector(0, 0, 130));
	F.Begin();
	const FTransform LowerPose = Lower->GetActorTransform();
	F.Box->BeginCarry(0, nullptr, F.Box->GetActorTransform());
	F.Box->SetActorRotation(FRotator(30, 0, 0));
	F.Box->UpdateVisual();
	const FTransform Preview = F.Box->LandingPreview->GetComponentTransform();
	TestTrue(TEXT("E7 stacking preview shown"), F.Box->LandingPreview->IsVisible());
	F.Box->ReleaseCarry();
	F.Box->Tick(1);
	const FBox Bounds = F.Box->GetCollisionBoundsInPose(F.Box->GetActorTransform(), FTransform::Identity);
	TestTrue(TEXT("E3 upper box bottom touches lower top"), FMath::IsNearlyEqual(Bounds.Min.Z, 80.005, 0.01));
	TestTrue(TEXT("E3 upper box flat"), F.Box->GetActorUpVector().Equals(FVector::UpVector));
	TestTrue(TEXT("E3 lower box never moves"), Lower->GetActorTransform().Equals(LowerPose));
	TestTrue(TEXT("E7 stack release agrees with preview within 0.01 cm/deg"),
		F.Box->GetActorLocation().Equals(Preview.GetLocation(), 0.01) && FVBoRCarryMotion::AngleDeg(Preview, F.Box->GetActorTransform()) < 0.01);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP1cLiftTest, "vBoR.P1c.LevelLift", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP1cLiftTest::RunTest(const FString& Parameters)
{
	FSettleWorld F;
	F.Box->BoxSizeCm = FVector(20, 60, 20);
	F.Box->ApplyBoxSize();
	F.Box->SetActorLocation(FVector(0, 0, 80));
	F.Block(FVector(0, 0, 45), FVector(500, 500, 5));
	F.Block(FVector(0, 33, 70), FVector(50, 4, 5));
	F.Begin();
	F.Box->BeginCarry(0, nullptr, F.Box->GetActorTransform());
	F.Box->SetActorRotation(FRotator(0, 0, 45));
	TestTrue(TEXT("E4 tilted release starts clear"), F.Box->IsPoseClear(F.Box->GetActorTransform()));
	TestFalse(TEXT("E4 leveling at current height overlaps lip"), F.Box->IsPoseClear(FTransform(FQuat::Identity, F.Box->GetActorLocation(), F.Box->GetActorScale3D())));
	F.Box->PoseClearCalls = 0;
	F.Box->UpdateVisual();
	TestTrue(TEXT("R1-1 E4 preview uses at most 28 pose checks"), F.Box->PoseClearCalls <= 28);
	AddInfo(FString::Printf(TEXT("R1-1 E4 preview pose checks: %d (limit 28)"), F.Box->PoseClearCalls));
	const FTransform Preview = F.Box->LandingPreview->GetComponentTransform();
	TestTrue(TEXT("E4 preview visible over lip"), F.Box->LandingPreview->IsVisible());
	F.Box->ReleaseCarry();
	TestTrue(TEXT("E4 only Z raised to lowest clear height within 0.01 cm"), F.Box->GetActorLocation().Equals(FVector(0, 0, 85), 0.01));
	TestTrue(TEXT("E4 immediate leveled pose clear"), F.Box->IsPoseClear(F.Box->GetActorTransform()));
	for (int32 Frame = 0; Frame < 5; ++Frame)
	{
		F.Box->Tick(1.0f / 60);
		TestTrue(TEXT("E4 no overlapping Tick"), F.Box->IsPoseClear(F.Box->GetActorTransform()));
	}
	TestTrue(TEXT("E7 raised release agrees with preview"), F.Box->GetActorTransform().Equals(Preview, 0.01));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP1cLevelBlockedTest, "vBoR.P1c.LevelBlocked", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP1cLevelBlockedTest::RunTest(const FString& Parameters)
{
	FSettleWorld F;
	F.Box->BoxSizeCm = FVector(20, 60, 20);
	F.Box->ApplyBoxSize();
	F.Box->SetActorLocation(FVector(0, 0, 80));
	F.Block(FVector(0, 0, 45), FVector(500, 500, 5));
	F.Block(FVector(0, 33, 100), FVector(50, 4, 100));
	F.Begin();
	F.Box->BeginCarry(0, nullptr, F.Box->GetActorTransform());
	F.Box->SetActorRotation(FRotator(0, 0, 45));
	const FTransform Tilted = F.Box->GetActorTransform();
	F.Box->PoseClearCalls = 0;
	F.Box->UpdateVisual();
	TestTrue(TEXT("R1-1 E5 preview uses at most 28 pose checks"), F.Box->PoseClearCalls <= 28);
	AddInfo(FString::Printf(TEXT("R1-1 E5 preview pose checks: %d (limit 28)"), F.Box->PoseClearCalls));
	const FTransform Preview = F.Box->LandingPreview->GetComponentTransform();
	TestTrue(TEXT("E5 tilted pose clear in narrow gap"), F.Box->IsPoseClear(Tilted));
	FLevelBlockedLog Logs;
	F.Box->ReleaseCarry();
	TestTrue(TEXT("E5 blocked leveling retains exact release pose"), F.Box->GetActorTransform().Equals(Tilted));
	for (int32 Frame = 0; Frame < 10; ++Frame)
	{
		F.Box->Tick(0.1f);
		TestTrue(TEXT("E5 fallback drop stays clear"), F.Box->IsPoseClear(F.Box->GetActorTransform()));
	}
	TestTrue(TEXT("E5 tilted rotation retained"), F.Box->GetActorQuat().Equals(Tilted.GetRotation()));
	TestTrue(TEXT("E7 blocked fallback agrees with preview"), F.Box->GetActorTransform().Equals(Preview, 0.01));
	TestTrue(TEXT("E5 debug retains reason after rest"), F.Box->GetMeasurementText().Contains(TEXT("level blocked")));
	TestEqual(TEXT("E5 one log for the release despite preview and later Ticks"), Logs.GetCount(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP1cLegacyTest, "vBoR.P1c.LevelOptOutAndAssist", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP1cLegacyTest::RunTest(const FString& Parameters)
{
	FSettleWorld F;
	F.Block(FVector(0, 0, 45), FVector(500, 500, 5));
	F.Begin();
	F.Box->bLevelOnRelease = false;
	F.Box->BeginCarry(0, nullptr, F.Box->GetActorTransform());
	const FQuat Tilt = FRotator(30, 0, 0).Quaternion();
	F.Box->SetActorRotation(Tilt);
	F.Box->ReleaseCarry();
	F.Box->Tick(1);
	TestTrue(TEXT("E6 disabling level preserves tilt"), F.Box->GetActorQuat().Equals(Tilt));
	F.Box->ResetToPose(FTransform(FVector(0, 0, 100)));
	F.Box->bLevelOnRelease = true;
	F.Box->bUsePlacementAssist = true;
	F.Box->BeginCarry(0, nullptr, F.Box->GetActorTransform());
	F.Box->SetActorRotation(Tilt);
	F.Target->SetActorTransform(FTransform(Tilt, F.Box->GetActorLocation()));
	F.Target->GoalSizeCm = FVector(200);
	F.Box->ReleaseCarry();
	TestTrue(TEXT("E6 existing assistance still starts"), F.Box->bAssisting);
	F.Box->AssistStartedAt = F.World->GetTimeSeconds() - 1;
	F.Box->Tick(0.1f);
	TestTrue(TEXT("E6 assistance retains configured tilted goal"), F.Box->GetActorQuat().Equals(Tilt));
	F.Box->BeginCarry(0, nullptr, F.Box->GetActorTransform());
	F.Box->LoadTarget = nullptr;
	F.Box->ReleaseCarry();
	F.Box->Tick(1);
	TestTrue(TEXT("E6 assistance enabled but unavailable retains legacy drop"), F.Box->GetActorQuat().Equals(Tilt));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP1cPreviewTest, "vBoR.P1c.PreviewVisibilityColorAndFrame", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP1cPreviewTest::RunTest(const FString& Parameters)
{
	FSettleWorld F;
	F.Block(FVector(0, 0, 45), FVector(500, 500, 5));
	F.Begin();
	TestFalse(TEXT("E9 no hand hides preview"), F.Box->LandingPreview->IsVisible());
	F.Box->BeginCarry(0, nullptr, F.Box->GetActorTransform());
	F.Box->SetActorRotation(FRotator(30, 40, 0));
	F.Box->UpdateVisual();
	TestTrue(TEXT("E7 held preview shown"), F.Box->LandingPreview->IsVisible());
	UMaterialInstanceDynamic* Material = Cast<UMaterialInstanceDynamic>(F.Box->LandingPreview->GetMaterial(0));
	FLinearColor Color;
	if (TestNotNull(TEXT("E8 preview material exists"), Material))
	{
		TestTrue(TEXT("E8 inside preview color readable"), Material->GetVectorParameterValue(FMaterialParameterInfo(TEXT("ColorParam")), Color));
		TestTrue(TEXT("E8 inside preview green"), Color.Equals(F.Box->SeatedColor));
	}
	const FTransform Preview = F.Box->LandingPreview->GetComponentTransform();
	F.Box->ReleaseCarry();
	TestFalse(TEXT("E9 release hides preview immediately"), F.Box->LandingPreview->IsVisible());
	F.Box->Tick(1);
	TestTrue(TEXT("E7 predicted position within 0.01 cm"), F.Box->GetActorLocation().Equals(Preview.GetLocation(), 0.01));
	TestTrue(TEXT("E7 predicted angle within 0.01 deg"), FVBoRCarryMotion::AngleDeg(Preview, F.Box->GetActorTransform()) < 0.01);
	F.Box->ResetToPose(FTransform(FVector(0, 0, 100)));
	F.Box->BeginCarry(0, nullptr, F.Box->GetActorTransform());
	// Hand/contact/visual update in one frame, same order as AdvanceHeldPose (P1c E11)
	FTransform Desired;
	F.Box->Session.Update(true, FTransform(FVector(300, 0, 100)), F.Box->GetActorTransform(), 1, F.Box->TiltLimitDeg, Desired);
	F.Box->MoveHeldPose(Desired, 1.0f / 60);
	F.Box->UpdateVisual();
	TestTrue(TEXT("E11 preview follows same-frame hand motion"), FMath::IsNearlyEqual(F.Box->LandingPreview->GetComponentLocation().X, 300.0, 0.01));
	if (Material)
	{
		Material->GetVectorParameterValue(FMaterialParameterInfo(TEXT("ColorParam")), Color);
		TestTrue(TEXT("E8 outside preview gray"), Color.Equals(F.Box->ReleasedColor));
	}
	TestTrue(TEXT("E8 debug reports outside"), F.Box->GetMeasurementText().Contains(TEXT("preview outside")));
	F.Box->Session.Pause(1);
	F.Box->UpdateVisual();
	TestFalse(TEXT("E9 tracking pause hides"), F.Box->LandingPreview->IsVisible());
	F.Box->Session.bPaused = false;
	F.Box->bUsePlacementAssist = true;
	F.Box->UpdateVisual();
	TestFalse(TEXT("E9 assist enabled hides"), F.Box->LandingPreview->IsVisible());
	F.Box->bUsePlacementAssist = false;
	F.Box->bShowLandingPreview = false;
	F.Box->UpdateVisual();
	TestFalse(TEXT("Preview opt-out hides"), F.Box->LandingPreview->IsVisible());
	F.Box->bShowLandingPreview = true;
	F.Box->SetActorLocation(FVector(700, 0, 100));
	F.Box->UpdateVisual();
	TestFalse(TEXT("E8 no surface below hides"), F.Box->LandingPreview->IsVisible());
	F.Box->SetActorLocation(FVector(0, 0, 100));
	F.Box->FloorHeightCm = 60;
	F.Box->UpdateVisual();
	TestFalse(TEXT("E8 landing below destruction height hides"), F.Box->LandingPreview->IsVisible());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP1cPreviewIsolationTest, "vBoR.P1c.PreviewIsolation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP1cPreviewIsolationTest::RunTest(const FString& Parameters)
{
	FSettleWorld F;
	F.Block(FVector(0, 0, 45), FVector(500, 500, 5));
	F.Begin();
	F.Box->BeginCarry(0, nullptr, F.Box->GetActorTransform());
	F.Box->UpdateVisual();
	UStaticMeshComponent* Preview = F.Box->LandingPreview;
	TestTrue(TEXT("E10 same mesh and size"), Preview->GetStaticMesh() == F.Box->Mesh->GetStaticMesh() && Preview->GetComponentScale().Equals(F.Box->GetActorScale3D()));
	TestTrue(TEXT("E10 preview has no collision"), Preview->GetCollisionEnabled() == ECollisionEnabled::NoCollision);
	TestFalse(TEXT("E10 no overlap events"), Preview->GetGenerateOverlapEvents());
	TestFalse(TEXT("E10 no shadow"), Preview->CastShadow);
	TestFalse(TEXT("E10 no physics"), Preview->IsSimulatingPhysics());
	TestTrue(TEXT("E10 ISDK grab collider remains real mesh"), F.Box->Grabbable->GetGrabCollider() == F.Box->Mesh);
	TArray<UIsdkGrabbableComponent*> Grabbables;
	F.Box->GetComponents(Grabbables);
	TestEqual(TEXT("E10 no preview grabbable"), Grabbables.Num(), 1);
	AVBoRCarryObject* Other = F.World->SpawnActor<AVBoRCarryObject>();
	Other->SetActorLocation(FVector(0, 0, 65.005));
	TestTrue(TEXT("E10 another box can occupy visible preview"), Other->IsPoseClear(Other->GetActorTransform()));
	FTransform Desired = Other->GetActorTransform();
	Desired.AddToTranslation(FVector(150, 0, 0));
	TestTrue(TEXT("E10 another box sweeps through preview"), Other->TryMoveToPose(Desired));
	F.Box->ReleaseCarry();
	F.Box->Tick(1);
	TestTrue(TEXT("E10 own sweep reaches floor through preview"), FMath::IsNearlyEqual(F.Box->GetActorLocation().Z, 65.005, 0.01));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP1cPreviewRenderStateTest, "vBoR.P1c.PreviewRenderState", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP1cPreviewRenderStateTest::RunTest(const FString& Parameters)
{
	FSettleWorld F;
	F.Block(FVector(0, 0, 45), FVector(500, 500, 5));
	F.Begin();
	F.Box->BeginCarry(0, nullptr, F.Box->GetActorTransform());
	F.Box->UpdateLandingPreview();
	UStaticMeshComponent* Preview = F.Box->LandingPreview;
	TestTrue(TEXT("R1-1 first update shows preview"), Preview->IsVisible());
	// Dirty assertion checks real render state even in nullrhi (P1c R1-1)
	Preview->RecreateRenderState_Concurrent();
	TestTrue(TEXT("R1-1 preview has render state"), Preview->IsRenderStateCreated());
	Preview->SetVisibility(false);
	TestTrue(TEXT("R1-1 visibility change dirties render state"), Preview->IsRenderStateDirty());
	F.Box->UpdateLandingPreview();
	F.World->SendAllEndOfFrameUpdates();
	TestTrue(TEXT("R1-1 preview visible before consecutive update"), Preview->IsVisible());
	TestFalse(TEXT("R1-1 first update render state flushed"), Preview->IsRenderStateDirty());
	F.Box->UpdateLandingPreview();
	TestTrue(TEXT("R1-1 consecutive update keeps preview visible"), Preview->IsVisible());
	TestFalse(TEXT("R1-1 consecutive update does not dirty render state"), Preview->IsRenderStateDirty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP1cDefaultsTest, "vBoR.P1c.DefaultsAndLiftValidation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP1cDefaultsTest::RunTest(const FString& Parameters)
{
	for (float Invalid : {-1.0f, std::numeric_limits<float>::quiet_NaN()})
	{
		FSettleWorld F;
		F.Box->LevelLiftMaxCm = Invalid;
		AddExpectedMessagePlain(TEXT("Invalid LevelLiftMaxCm"), ELogVerbosity::Warning);
		F.Begin();
		TestEqual(TEXT("Invalid lift clamps to zero before play"), F.Box->LevelLiftMaxCm, 0.0f);
	}
	FSettleWorld F;
	APlayerController* Controller = F.World->SpawnActor<APlayerController>();
	Controller->Possess(F.World->SpawnActor<AVBoRHandRigPawn>());
	AP1LoadBench* Bench = F.World->SpawnActor<AP1LoadBench>();
	F.Begin();
	Bench->PlaceFromHead(FTransform(FVector(0, 0, 165)));
	if (TestNotNull(TEXT("E12 supply box exists"), Bench->Bag.Get()))
	{
		TestTrue(TEXT("E12 leveling defaults on"), Bench->Bag->bLevelOnRelease);
		TestTrue(TEXT("E12 preview defaults on"), Bench->Bag->bShowLandingPreview);
		TestEqual(TEXT("Lift default is half default box height cm"), Bench->Bag->LevelLiftMaxCm, float(Bench->Bag->BoxSizeCm.Z * 0.5));
	}
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5nBoxSnapTest, "vBoR.P5n.E10E14BoxYawSnap", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5nBoxSnapTest::RunTest(const FString&)
{
	for (double RestYaw : {0.0,20.0})
	{
		for (double Yaw : {37.0,52.0,45.0,-45.0})
		{
			FSettleWorld F;
			F.Target->SetActorRotation(FRotator(0,30,0));
			F.Block(FVector(0,0,45),FVector(500,500,5)); F.Begin();
			F.Box->ResetToPose(FTransform(FRotator(0,RestYaw,0),FVector(0,0,100)));
			F.Box->BeginCarry(0,nullptr,F.Box->GetActorTransform());
			// RestYaw + tilt slightly changes world Yaw on upright correction. Measure half step horizontally (P5n E10)
			const bool Half=FMath::Abs(Yaw)==45;
			F.Box->SetActorRotation(FRotator(Half?0:15,30+Yaw,Half?0:10));
			const double Expected=Yaw==52 || Yaw==45?120:30;
			TestTrue(TEXT("E10 held box remains at hand yaw before release"),FMath::IsNearlyEqual(F.Box->GetActorRotation().Yaw,30+Yaw,0.001));
			F.Box->UpdateVisual();
			TestTrue(TEXT("E13 landing preview visible"),F.Box->LandingPreview->IsVisible());
			const FTransform Preview=F.Box->LandingPreview->GetComponentTransform();
			AddInfo(FString::Printf(TEXT("E10 rest yaw=%.0f offset=%.0f preview yaw=%.9f expected=%.0f deg"),RestYaw,Yaw,Preview.Rotator().Yaw,Expected));
			TestTrue(TEXT("E13 preview uses region yaw plus right angles"),Preview.Rotator().Equals(FRotator(0,Expected,0),0.001));
			F.Box->ReleaseCarry();
			TestTrue(TEXT("E10 world yaw snaps independently of rest yaw; half ties go positive"),F.Box->GetActorRotation().Equals(FRotator(0,Expected,0),0.001));
			F.Box->Tick(1);
			TestTrue(TEXT("E13 released box agrees with snapped preview"),F.Box->GetActorTransform().Equals(Preview,0.01));
		}
	}
	for (int32 Mode=0; Mode<5; ++Mode)
	{
		FSettleWorld F; F.Block(FVector(0,0,45),FVector(500,500,5));
		if (Mode==1) { F.Box->bSnapYawInLoadRegion=false; }
		if (Mode==2) { F.Box->LoadTarget=nullptr; }
		if (Mode==3) { F.Box->bLevelOnRelease=false; }
		F.Begin();
		const FVector Center(Mode==0?300:0,0,100);
		F.Box->ResetToPose(FTransform(Center)); F.Box->BeginCarry(0,nullptr,F.Box->GetActorTransform());
		F.Box->SetActorRotation(FRotator(0,37,0));
		if (Mode==4)
		{
			// Region XY only, ignore height (P5n D4)
			F.Target->SetActorLocation(FVector(0,0,-1000));
		}
		F.Box->ReleaseCarry();
		TestTrue(TEXT("E12/E14 opt-out, outside, no target and no leveling retain yaw"),F.Box->GetActorRotation().Equals(FRotator(0,Mode==4?0:37,0),0.001));
	}
	for (double LocalX : {299.0,301.0})
	{
		FSettleWorld F; F.Target->SetActorTransform(FTransform(FRotator(0,30,0),FVector(0,0,-1000),FVector(2,0.5,1)));
		F.Block(FVector(0,0,45),FVector(1000,1000,5)); F.Begin();
		const FVector Center=FRotator(0,30,0).RotateVector(FVector(LocalX,0,0))+FVector(0,0,100);
		F.Box->ResetToPose(FTransform(Center)); F.Box->BeginCarry(0,nullptr,F.Box->GetActorTransform()); F.Box->SetActorRotation(FRotator(0,67,0));
		F.Box->ReleaseCarry();
		TestTrue(TEXT("D4 region XY uses rotated frame and scaled extent, independent of height"),F.Box->GetActorRotation().Equals(FRotator(0,LocalX<300?30:67,0),0.001));
	}
	for (bool BothBlocked : {false,true})
	{
		// Tall wall blocks +Z escape too (P5n E11, cm)
		FSettleWorld F; F.Box->BoxSizeCm=FVector(20,60,20); F.Box->ApplyBoxSize();
		F.Block(FVector(0,0,45),FVector(500,500,5));
		F.Block(FVector(0,BothBlocked?28.8:29.8,100),FVector(100,0.1,200)); F.Begin();
		F.Box->BeginCarry(0,nullptr,F.Box->GetActorTransform()); F.Box->SetActorRotation(FRotator(0,40,45));
		TestTrue(TEXT("E11 tilted release starts clear"),F.Box->IsPoseClear(F.Box->GetActorTransform()));
		TestFalse(TEXT("E11 snapped upright pose blocked"),F.Box->IsPoseClear(FTransform(FQuat::Identity,FVector(0,0,100))));
		TestEqual(TEXT("E11 unsnapped upright clearance matches wall"),F.Box->IsPoseClear(FTransform(FRotator(0,40,0),FVector(0,0,100))),!BothBlocked);
		F.Box->UpdateVisual(); const FTransform Preview=F.Box->LandingPreview->GetComponentTransform();
		F.Box->ReleaseCarry();
		TestTrue(TEXT("E11 falls back to unsnapped upright, then original tilt if both blocked"),F.Box->GetActorRotation().Equals(FRotator(0,40,BothBlocked?45:0),0.001));
		TestTrue(TEXT("E11 fallback starts clear"),F.Box->IsPoseClear(F.Box->GetActorTransform()));
		F.Box->Tick(1);
		TestTrue(TEXT("E11 preview agrees with fallback"),F.Box->GetActorTransform().Equals(Preview,0.01));
	}
	return true;
}
#endif
