#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "../Interaction/VBoRPoweredPanel.h"
#include "../Interaction/VBoRPokeButton.h"
#include "Components/ChildActorComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/WorldSettings.h"
#include "Interaction/IsdkPokeInteractable.h"
#include "Sound/SoundWave.h"

namespace
{
	struct FR14World
	{
		UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
		AVBoRPoweredPanel* Gate = World->SpawnActor<AVBoRPoweredPanel>();
		~FR14World() { World->EndPlay(EEndPlayReason::Quit); World->DestroyWorld(false); }
		void Play() { World->InitializeActorsForPlay(FURL()); World->GetWorldSettings()->NotifyBeginPlay(); }
		AVBoRPokeButton* Place(const TCHAR* Tag)
		{
			AVBoRPokeButton* Button = World->SpawnActor<AVBoRPokeButton>();
			Button->Tags.Add(Tag);
			Button->SetActorTransform(FTransform(FRotator(15, 35, 10), FVector(-10, 25, 80)));
			Button->AttachToComponent(Gate->GetRootComponent(), FAttachmentTransformRules::KeepWorldTransform);
			return Button;
		}
		int32 ButtonCount() const
		{
			int32 Count = 0;
			for (TActorIterator<AVBoRPokeButton> It(World); It; ++It) { ++Count; }
			return Count;
		}
	};
	void Poke(AVBoRPokeButton* Button)
	{
		FIsdkInteractionPointerEvent Event;
		Event.Type = EIsdkPointerEventType::Select;
		Event.Interactable = Button->Poke.Get();
		Button->Poke->GetInteractionPointerEventDelegate().Broadcast(Event);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FR14PlacedButtonsTest, "vBoR.R14.PlacedButtonsMoveAndRequest", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FR14PlacedButtonsTest::RunTest(const FString& Parameters)
{
	FR14World F;
	AVBoRPokeButton* Open = F.Place(TEXT("Gate.OpenButton"));
	UChildActorComponent* Child = NewObject<UChildActorComponent>(F.Gate);
	Child->SetupAttachment(F.Gate->HingePivot);
	Child->SetRelativeLocation(FVector(-6, -20, -60));
	Child->SetChildActorClass(AVBoRPokeButton::StaticClass());
	Child->RegisterComponent();
	AVBoRPokeButton* Close = CastChecked<AVBoRPokeButton>(Child->GetChildActor());
	Close->Tags.Add(TEXT("Gate.CloseButton"));
	UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	Open->ButtonMesh->SetStaticMesh(Mesh);
	Open->ButtonMesh->SetRelativeScale3D(FVector(0.07, 0.04, 0.02));
	Open->PressedCue.Sound = NewObject<USoundWave>(F.World);
	Open->ReleasedCue.Sound = NewObject<USoundWave>(F.World);
	Open->PressedCue.StartDelaySeconds = 0.25f;
	const FTransform OpenPose = Open->GetActorTransform();
	const FTransform ClosePose = Close->GetActorTransform();
	const FTransform MeshPose = Open->ButtonMesh->GetRelativeTransform();
	USoundBase* PressedSound = Open->PressedCue.Sound;
	USoundBase* ReleasedSound = Open->ReleasedCue.Sound;
	F.Play();
	TestEqual(TEXT("E1 exactly two buttons, no spawn"), F.ButtonCount(), 2);
	TestTrue(TEXT("E1 uses both placed buttons"), F.Gate->OpenButton == Open && F.Gate->CloseButton == Close);
	TestTrue(TEXT("E1 reparents open button under hinge"), Open->GetRootComponent()->GetAttachParent() == F.Gate->HingePivot);
	TestTrue(TEXT("E1 preserves child actor parent under hinge"), Close->GetRootComponent()->GetAttachParent() == Child);
	TestTrue(TEXT("E1 preserves world poses"), Open->GetActorTransform().Equals(OpenPose) && Close->GetActorTransform().Equals(ClosePose));
	TestTrue(TEXT("E1 preserves mesh and size"), Open->ButtonMesh->GetStaticMesh() == Mesh && Open->ButtonMesh->GetRelativeTransform().Equals(MeshPose));
	TestTrue(TEXT("E1 preserves sounds"), Open->PressedCue.Sound == PressedSound && Open->ReleasedCue.Sound == ReleasedSound);
	TestEqual(TEXT("E1 preserves sound delay"), Open->PressedCue.StartDelaySeconds, 0.25f);
	const FTransform OpenFromHinge = OpenPose.GetRelativeTransform(F.Gate->HingePivot->GetComponentTransform());
	const FTransform CloseFromHinge = ClosePose.GetRelativeTransform(F.Gate->HingePivot->GetComponentTransform());
	Poke(Open);
	TestTrue(TEXT("E1 placed open requests opening"), F.Gate->GetState() == EVBoRPoweredState::OpeningWait);
	F.Gate->Tick(2);
	TestTrue(TEXT("E1 gate moving"), F.Gate->GetAngleDeg() > 0 && F.Gate->GetState() == EVBoRPoweredState::Opening);
	TestTrue(TEXT("E1 open follows hinge"), Open->GetActorTransform().Equals(OpenFromHinge * F.Gate->HingePivot->GetComponentTransform()));
	TestTrue(TEXT("E1 close follows hinge"), Close->GetActorTransform().Equals(CloseFromHinge * F.Gate->HingePivot->GetComponentTransform()));
	F.Gate->Tick(4);
	Poke(Close);
	TestTrue(TEXT("E1 placed close requests closing"), F.Gate->GetState() == EVBoRPoweredState::ClosingWait);
	F.Gate->Tick(5);
	TestTrue(TEXT("E1 reaches closed"), F.Gate->GetState() == EVBoRPoweredState::Closed);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FR14PartialButtonsTest, "vBoR.R14.MissingCloseUsesSpawn", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FR14PartialButtonsTest::RunTest(const FString& Parameters)
{
	FR14World F;
	AVBoRPokeButton* Open = F.Place(TEXT("Gate.OpenButton"));
	F.Play();
	TestTrue(TEXT("E2 uses placed open"), F.Gate->OpenButton == Open);
	TestEqual(TEXT("E2 exactly one fallback button"), F.ButtonCount(), 2);
	AVBoRPokeButton* Close = F.Gate->CloseButton;
	if (!TestNotNull(TEXT("E2 close spawned"), Close)) { return false; }
	TestTrue(TEXT("E2 fallback owner"), Close->GetOwner() == F.Gate);
	TestTrue(TEXT("E2 fallback pose"), Close->GetActorTransform().Equals(F.Gate->CloseButtonFromHinge * F.Gate->HingePivot->GetComponentTransform()));
	Poke(Open);
	F.Gate->Tick(5);
	Poke(Close);
	TestTrue(TEXT("E2 fallback close requests closing"), F.Gate->GetState() == EVBoRPoweredState::ClosingWait);
	F.Gate->RouteEndPlay(EEndPlayReason::RemovedFromWorld);
	TestFalse(TEXT("E2 placed open survives EndPlay"), Open->IsActorBeingDestroyed());
	TestTrue(TEXT("E2 spawned close destroyed on EndPlay"), Close->IsActorBeingDestroyed());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FR14EndPlayTest, "vBoR.R14.PlacedButtonsSurviveEndPlay", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FR14EndPlayTest::RunTest(const FString& Parameters)
{
	FR14World F;
	AVBoRPokeButton* Open = F.Place(TEXT("Gate.OpenButton"));
	AVBoRPokeButton* Close = F.Place(TEXT("Gate.CloseButton"));
	F.Play();
	F.Gate->RouteEndPlay(EEndPlayReason::RemovedFromWorld);
	TestFalse(TEXT("E3 placed open survives EndPlay"), Open->IsActorBeingDestroyed());
	TestFalse(TEXT("E3 placed close survives EndPlay"), Close->IsActorBeingDestroyed());
	TestEqual(TEXT("E3 both placed buttons remain"), F.ButtonCount(), 2);
	Poke(Open);
	TestTrue(TEXT("E3 ended gate unbound"), F.Gate->GetState() == EVBoRPoweredState::Closed);
	return true;
}
#endif
