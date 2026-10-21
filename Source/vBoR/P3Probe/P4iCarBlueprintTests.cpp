#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "P3VehicleMock.h"
#include "../Interaction/VBoRHingedDoor.h"
#include "../Interaction/VBoRAxisPart.h"
#include "../Interaction/VBoRPokeButton.h"
#include "../Interaction/VBoRPoweredPanel.h"
#include "../Interaction/VBoRFoldingSeat.h"
#include "../Interaction/VBoRSteeringWheel.h"
#include "../Interaction/VBoRTwoAxisPart.h"
#include "../Interaction/VBoRKnob.h"
#include "../Interaction/VBoRKeyTable.h"
#include "../Interaction/VBoRBoxStand.h"
#include "../Interaction/VBoRLoadTarget.h"
#include "../Interaction/VBoRFloorRoute.h"
#include "../Interaction/VBoRVehicleAlignmentComponent.h"
#include "../Interaction/VBoRAlignPointComponent.h"
#include "Components/BoxComponent.h"
#include "Components/ChildActorComponent.h"
#include "Components/SplineComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Blueprint.h"
#include "EdGraph/EdGraph.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/WorldSettings.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Audio/IsdkPointerEventAudioPlayer.h"
#include "Interaction/IsdkPokeButtonVisual.h"
#include "Interaction/IsdkPokeInteractable.h"
#include "Subsystem/IsdkWorldSubsystem.h"

namespace
{
struct FP4iWorld
{
	UWorld* World;
	explicit FP4iWorld(EWorldType::Type Type = EWorldType::Game) : World(UWorld::CreateWorld(Type, false)) {}
	void Play() { World->InitializeActorsForPlay(FURL()); World->GetWorldSettings()->NotifyBeginPlay(); }
	~FP4iWorld() { World->EndPlay(EEndPlayReason::Quit); World->DestroyWorld(false); }
};
struct FP4iBP { const TCHAR* Name; UClass* Parent; };
TArray<FP4iBP> P4iBlueprints()
{
	return {{TEXT("BP_Car_Core"), AActor::StaticClass()}, {TEXT("BP_Door_FL"), AVBoRHingedDoor::StaticClass()},
		{TEXT("BP_DoorHandle_FL_Ext"), AVBoRAxisPart::StaticClass()}, {TEXT("BP_DoorHandle_FL_Int"), AVBoRAxisPart::StaticClass()},
		{TEXT("BP_ConsoleLid"), AVBoRHingedPanel::StaticClass()}, {TEXT("BP_Glovebox"), AVBoRHingedPanel::StaticClass()},
		{TEXT("BP_GloveboxHandle"), AVBoRAxisPart::StaticClass()}, {TEXT("BP_GearLever"), AVBoRAxisPart::StaticClass()},
		{TEXT("BP_TurnSignalLever"), AVBoRAxisPart::StaticClass()}, {TEXT("BP_GearButton"), AVBoRPokeButton::StaticClass()},
		{TEXT("BP_ReachButton"), AVBoRPokeButton::StaticClass()}, {TEXT("BP_SteeringWheel"), AVBoRSteeringWheel::StaticClass()},
		{TEXT("BP_RearviewMirror"), AVBoRTwoAxisPart::StaticClass()}, {TEXT("BP_ConsoleKnob"), AVBoRKnob::StaticClass()},
		{TEXT("BP_Liftgate"), AVBoRPoweredPanel::StaticClass()}, {TEXT("BP_RearSeatBack"), AVBoRFoldingSeat::StaticClass()},
		{TEXT("BP_KeyFob_Front"), AVBoRKeyFob::StaticClass()}, {TEXT("BP_KeyFob_Rear"), AVBoRKeyFob::StaticClass()}};
}
UWorld* P4iMainWorld()
{
	UPackage* Package=LoadPackage(nullptr,TEXT("/Game/Main/L_Main"),LOAD_None);
	return Package ? UWorld::FindWorldInPackage(Package) : nullptr;
}
// 회사에서 Approx 폴더를 지우면 근사 차와 같은 값인지는 더 보지 않음 (CAD 로 바뀜, P4i R1-1)
bool P4iApproximate()
{
	UWorld* W=P4iMainWorld(); if(!W) { return false; }
#if WITH_EDITOR
	for(ULevel* Level:W->GetLevels()) { for(AActor* A:Level->Actors) { if(A && A->GetFolderPath().ToString().StartsWith(TEXT("Approx"))) { return true; } } }
#endif
	return false;
}
FString P4iPath(const FString& Name) { return TEXT("/Game/Main/Car/") + FString(Name == TEXT("BP_Car_Core") ? TEXT("Core/") : TEXT("Parts/")) + Name; }
UClass* P4iClass(const FString& Name) { return LoadClass<AActor>(nullptr, *(P4iPath(Name) + TEXT(".") + Name + TEXT("_C"))); }
void P4iTransform(FAutomationTestBase& T, const FString& Label, const FTransform& A, const FTransform& B)
{
	T.TestTrue(Label + TEXT(" location cm"), A.GetLocation().Equals(B.GetLocation(), .01));
	T.TestTrue(Label + TEXT(" rotation deg"), FMath::RadiansToDegrees(A.GetRotation().AngularDistance(B.GetRotation())) <= .01);
	T.TestTrue(Label + TEXT(" scale"), A.GetScale3D().Equals(B.GetScale3D(), 1.e-4));
}
void P4iComponents(FAutomationTestBase& T, AActor* Expected, AActor* Actual)
{
	if (!T.TestNotNull(TEXT("Expected part"), Expected) || !T.TestNotNull(TEXT("Blueprint part"), Actual)) { return; }
	TArray<USceneComponent*> Components; Expected->GetComponents(Components);
	TArray<USceneComponent*> Other; Actual->GetComponents(Other);
	for (auto* C : Components)
	{
		if (!(C->IsA<UStaticMeshComponent>() || C->IsA<UBoxComponent>() || C->GetName().Contains(TEXT("Pivot")))) { continue; }
		USceneComponent** Match = Other.FindByPredicate([C](USceneComponent* B) { return B->GetFName() == C->GetFName(); });
		if (T.TestTrue(Actual->GetName() + TEXT(" component ") + C->GetName(), Match != nullptr))
		{
			P4iTransform(T, Actual->GetName() + TEXT(".") + C->GetName(), C->GetComponentTransform(), (*Match)->GetComponentTransform());
			if (auto* Box = Cast<UBoxComponent>(C)) { T.TestTrue(TEXT("Box extent"), Box->GetUnscaledBoxExtent().Equals(CastChecked<UBoxComponent>(*Match)->GetUnscaledBoxExtent(), .01)); }
		}
	}
	// Actor 원점 보정값·조립용 side 만 다름. 조작 설정은 부모와 같아야 함 (P4i D2·D3·D5)
	for (TFieldIterator<FProperty> It(Expected->GetClass()); It; ++It)
	{
		FProperty* P = *It;
		if (!P->GetOwnerClass()->GetName().StartsWith(TEXT("VBoR")) || !P->HasAnyPropertyFlags(CPF_Edit)
			|| P->IsA<FObjectPropertyBase>() || P->GetName() == TEXT("MeshRestFromRoot")
			|| P->GetName() == TEXT("HingeLocationCm") || P->GetName() == TEXT("DoorHandleSide")) { continue; }
		if (auto* Text = CastField<FTextProperty>(P))
		{
			T.TestEqual(P->GetName(), Text->GetPropertyValue_InContainer(Expected).ToString(), Text->GetPropertyValue_InContainer(Actual).ToString());
		}
		else { T.TestTrue(Actual->GetName() + TEXT(" setting ") + P->GetName(), P->Identical_InContainer(Expected, Actual)); }
	}
}
TMap<FString, AActor*> P4iSpawn(UWorld* W, FAutomationTestBase& T)
{
	TMap<FString, AActor*> Parts;
	for (const auto& BP : P4iBlueprints())
	{
		const FString N(BP.Name);
		if (N.Contains(TEXT("KeyFob")) || N == TEXT("BP_GearButton")) { continue; }
		UClass* C = P4iClass(N);
		if (!T.TestNotNull(N, C)) { continue; }
		Parts.Add(N, W->SpawnActor<AActor>(C));
	}
	if (Parts.Num() != 15) { return Parts; }
	Parts[TEXT("BP_ReachButton")]->SetActorLocation(FVector(40,25,91.5));
	for (const auto& Pair : TArray<TPair<FString,FString>>{{TEXT("BP_DoorHandle_FL_Ext"),TEXT("BP_Door_FL")},{TEXT("BP_DoorHandle_FL_Int"),TEXT("BP_Door_FL")},{TEXT("BP_GloveboxHandle"),TEXT("BP_Glovebox")}})
	{
		Parts[Pair.Key]->AttachToActor(Parts[Pair.Value], FAttachmentTransformRules::KeepWorldTransform);
	}
	return Parts;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP4iPreview, "vBoR.P4i.E7ButtonPreview", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP4iPreview::RunTest(const FString&)
{
	FP4iWorld Preview(EWorldType::EditorPreview);
	TestNull(TEXT("Preview has no ISDK subsystem"), UIsdkWorldSubsystem::TryGet(Preview.World));
	AVBoRPokeButton* Button = Preview.World->SpawnActor<AVBoRPokeButton>();
	Preview.World->InitializeActorsForPlay(FURL());
	// 썸네일은 BeginPlay 없이 초기화함 (P4i E7·Q2)
	Button->PreInitializeComponents();
	TestNull(TEXT("Preview SDK wiring skipped"), Button->ButtonVisual->GetPokeInteractable());
	TestNull(TEXT("Preview audio skipped"), Button->PointerAudio->GetSelectAudio());
	FP4iWorld Game;
	TestNotNull(TEXT("Game has ISDK subsystem"), UIsdkWorldSubsystem::TryGet(Game.World));
	AVBoRPokeButton* Live = Game.World->SpawnActor<AVBoRPokeButton>(); Game.Play();
	TestTrue(TEXT("Game visual connected"), Live->ButtonVisual->GetPokeInteractable() == Live->Poke);
	TestTrue(TEXT("Game pointable connected"), Live->PointerAudio->GetPointable().GetObject() == Live->Poke);
	TestTrue(TEXT("Game audio connected"), Live->PointerAudio->GetSelectAudio() == Live->PressAudio);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP4iAssets, "vBoR.P4i.E1Blueprints", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP4iAssets::RunTest(const FString&)
{
	for (const auto& BP : P4iBlueprints())
	{
		UClass* C = P4iClass(BP.Name); if (!TestNotNull(BP.Name,C)) { continue; }
		TestTrue(TEXT("Exact native parent"), C->GetSuperClass() == BP.Parent);
		UBlueprint* Asset = Cast<UBlueprint>(C->ClassGeneratedBy);
		if (TestNotNull(TEXT("Blueprint source"), Asset))
		{
			for (UEdGraph* Graph : Asset->UbergraphPages) { TestEqual(TEXT("Empty Event Graph"), Graph->Nodes.Num(), 0); }
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP4iPoses, "vBoR.P4i.E2WorldTransforms", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP4iPoses::RunTest(const FString&)
{
	if(!P4iApproximate()) { AddInfo(TEXT("L_Main has no Approx folder; mock comparison skipped (CAD state)")); return true; }
	FP4iWorld MockWorld, BPWorld;
	auto* Mock = MockWorld.World->SpawnActor<AP3VehicleMock>(); MockWorld.Play();
	auto Parts = P4iSpawn(BPWorld.World,*this); if (Parts.Num()!=15) { return false; } BPWorld.Play();
	TArray<TPair<AActor*, FString>> Pairs = {{Mock->Door,TEXT("BP_Door_FL")},{Mock->OutsideAxisHandle,TEXT("BP_DoorHandle_FL_Ext")},
		{Mock->InsideAxisHandle,TEXT("BP_DoorHandle_FL_Int")},{Mock->ConsoleLid,TEXT("BP_ConsoleLid")},{Mock->Glovebox,TEXT("BP_Glovebox")},
		{Mock->GloveboxHandle,TEXT("BP_GloveboxHandle")},{Mock->GearLever,TEXT("BP_GearLever")},{Mock->TurnSignalLever,TEXT("BP_TurnSignalLever")},
		{Mock->ReachButton,TEXT("BP_ReachButton")},{Mock->SteeringWheel,TEXT("BP_SteeringWheel")},{Mock->RearviewMirror,TEXT("BP_RearviewMirror")},
		{Mock->ConsoleKnob,TEXT("BP_ConsoleKnob")},{Mock->Liftgate,TEXT("BP_Liftgate")},{Mock->RearSeat,TEXT("BP_RearSeatBack")}};
	for (const auto& P : Pairs) { P4iComponents(*this,P.Key,Parts[P.Value]); }
	auto* Child = Parts[TEXT("BP_GearLever")]->FindComponentByClass<UChildActorComponent>();
	if (TestNotNull(TEXT("Gear Child Actor"), Child)) { P4iComponents(*this,Mock->GearButton,Child->GetChildActor()); }
	auto* Gate = CastChecked<AVBoRPoweredPanel>(Parts[TEXT("BP_Liftgate")]);
	P4iComponents(*this,Mock->Liftgate->OpenButton,Gate->OpenButton); P4iComponents(*this,Mock->Liftgate->CloseButton,Gate->CloseButton);
	TestTrue(TEXT("Open button closed location"), Gate->OpenButton->GetActorLocation().Equals(FVector(-241,20,85),.01));
	TestTrue(TEXT("Close button closed location"), Gate->CloseButton->GetActorLocation().Equals(FVector(-241,-20,85),.01));
	AActor* Core = Parts[TEXT("BP_Car_Core")];
	TArray<USceneComponent*> CoreComponents; Core->GetComponents(CoreComponents);
	TArray<USceneComponent*> MockComponents; Mock->GetComponents(MockComponents);
	for (auto* C : CoreComponents)
	{
		if (C == Core->GetRootComponent()) { continue; }
		FString Name = C->GetName();
		if (Name == TEXT("DriverView")) { Name = TEXT("DriverEye"); }
		USceneComponent** Match = MockComponents.FindByPredicate([&Name,C](USceneComponent* M)
		{
			if (M->GetName()!=Name) { return false; }
			// 점마다 같은 이름의 표시 자식이 있음 (다른 점의 sphere 와 비교하지 않게, P4i E2)
			return Name!=TEXT("PointSphere") && Name!=TEXT("PointNumber")
				|| (M->GetAttachParent() && C->GetAttachParent() && M->GetAttachParent()->GetFName()==C->GetAttachParent()->GetFName());
		});
		if (C->IsA<UVBoRVehicleAlignmentComponent>()) { continue; }
		if (TestTrue(TEXT("Core counterpart ")+Name,Match!=nullptr))
		{
			P4iTransform(*this,Name,(*Match)->GetComponentTransform(),C->GetComponentTransform());
			if (auto* B=Cast<UBoxComponent>(C)) { TestTrue(TEXT("Core extent"),B->GetUnscaledBoxExtent().Equals(CastChecked<UBoxComponent>(*Match)->GetUnscaledBoxExtent(),.01)); }
			if (auto* Point=Cast<UVBoRAlignPointComponent>(C)) { auto* M=CastChecked<UVBoRAlignPointComponent>(*Match); TestEqual(TEXT("Point order"),Point->Order,M->Order); TestEqual(TEXT("Point label"),Point->Label,M->Label); }
		}
	}
	// 축을 움직인 뒤 자식의 world 자세도 같아야 함 (P4i E2, deg)
	for (AActor* Door : {static_cast<AActor*>(Mock->Door),Parts[TEXT("BP_Door_FL")]})
	{
		USceneComponent* H=CastChecked<AVBoRHingedPanel>(Door)->HingePivot; H->AddLocalRotation(FRotator(0,40,0));
	}
	P4iComponents(*this,Mock->OutsideAxisHandle,Parts[TEXT("BP_DoorHandle_FL_Ext")]);
	P4iComponents(*this,Mock->InsideAxisHandle,Parts[TEXT("BP_DoorHandle_FL_Int")]);
	for (AActor* Gear : {static_cast<AActor*>(Mock->GearLever),Parts[TEXT("BP_GearLever")]}) { CastChecked<AVBoRAxisPart>(Gear)->AxisPivot->AddLocalRotation(FRotator(0,0,20)); }
	if (Child) { P4iComponents(*this,Mock->GearButton,Child->GetChildActor()); }
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP4iLinks, "vBoR.P4i.E3Connections", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP4iLinks::RunTest(const FString&)
{
	FP4iWorld W; auto Parts=P4iSpawn(W.World,*this); if(Parts.Num()!=15) { return false; }
	TArray<AVBoRKeyTable*> Tables;
	for (const TCHAR* Name : {TEXT("BP_KeyFob_Front"),TEXT("BP_KeyFob_Rear")})
	{
		auto* Table=W.World->SpawnActorDeferred<AVBoRKeyTable>(AVBoRKeyTable::StaticClass(),FTransform::Identity);
		Table->KeyClass=P4iClass(Name); Table->FinishSpawning(FTransform::Identity); Tables.Add(Table);
	}
	W.Play();
	auto* Door=CastChecked<AVBoRHingedPanel>(Parts[TEXT("BP_Door_FL")]);
	auto* Glove=CastChecked<AVBoRHingedPanel>(Parts[TEXT("BP_Glovebox")]);
	TestTrue(TEXT("Outside linked"),Door->OutsideAxisHandle==Parts[TEXT("BP_DoorHandle_FL_Ext")]);
	TestTrue(TEXT("Inside linked"),Door->InsideAxisHandle==Parts[TEXT("BP_DoorHandle_FL_Int")]);
	TestTrue(TEXT("Glove linked"),Glove->OutsideAxisHandle==Parts[TEXT("BP_GloveboxHandle")]);
	for (auto* Handle : {Door->OutsideAxisHandle.Get(),Door->InsideAxisHandle.Get(),Glove->OutsideAxisHandle.Get()})
	{
		if (TestNotNull(TEXT("Connected handle"),Handle)) { TestTrue(TEXT("Handle attached to hinge"),Handle->GetRootComponent()->GetAttachParent()==(Handle==Glove->OutsideAxisHandle ? Glove->HingePivot.Get():Door->HingePivot.Get())); }
	}
	auto* Child=Parts[TEXT("BP_GearLever")]->FindComponentByClass<UChildActorComponent>();
	if(TestNotNull(TEXT("Gear child component"),Child)) { TestTrue(TEXT("Child attached to AxisPivot"),Child->GetAttachParent()==CastChecked<AVBoRAxisPart>(Parts[TEXT("BP_GearLever")])->AxisPivot); TestTrue(TEXT("Gear BP class"),Child->GetChildActorClass()==P4iClass(TEXT("BP_GearButton"))); }
	for (int32 I=0;I<2;++I) { if(TestNotNull(TEXT("Table spawned key"),Tables[I]->Key.Get())) { TestTrue(TEXT("BP key class"),Tables[I]->Key->GetClass()==Tables[I]->KeyClass); TestTrue(TEXT("Key tag"),Tables[I]->Key->ActorHasTag(I==0?TEXT("Car.FrontKey"):TEXT("Car.RearKey"))); } }
	TMap<FName,int32> Tags;
	for (TActorIterator<AActor> It(W.World);It;++It) { for(FName Tag:It->Tags) { if(Tag.ToString().StartsWith(TEXT("Car."))) { ++Tags.FindOrAdd(Tag); } } TArray<UActorComponent*> Cs;It->GetComponents(Cs);for(auto* C:Cs) { for(FName Tag:C->ComponentTags) { if(Tag.ToString().StartsWith(TEXT("Car."))) { ++Tags.FindOrAdd(Tag); } } } }
	for(const TCHAR* Tag:{TEXT("Car.DriverDoor"),TEXT("Car.ReachButton"),TEXT("Car.Liftgate"),TEXT("Car.RearSeat"),TEXT("Car.FrontKey"),TEXT("Car.RearKey"),TEXT("Car.DriverView"),TEXT("Car.PassengerView"),TEXT("Car.RearView"),TEXT("Car.DriverSeat"),TEXT("Car.Footprint"),TEXT("Car.Start")}) { TestEqual(Tag,Tags.FindRef(FName(Tag)),1); }
	// 접근 구역은 지금 단계에서 안 쓰므로 0–1개 (Blueprint-Guide §3)
	TestTrue(TEXT("Car.Approach at most one"),Tags.FindRef(FName(TEXT("Car.Approach")))<=1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP4iLevel, "vBoR.P4i.E4MainLevel", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP4iLevel::RunTest(const FString&)
{
	UWorld* W=P4iMainWorld(); if(!TestNotNull(TEXT("Main map package"),W)) { return false; }
	const bool bApprox=P4iApproximate();
	TMap<UClass*,int32> Counts; TArray<AActor*> Actors;
	for(ULevel* Level:W->GetLevels()) { for(AActor* A:Level->Actors) { if(A) { Actors.Add(A);++Counts.FindOrAdd(A->GetClass()); } } }
	TestEqual(TEXT("No vehicle mock"),Counts.FindRef(AP3VehicleMock::StaticClass()),0);
	int32 AlignmentCount=0; AVBoRLoadTarget* Target=nullptr; AVBoRBoxStand* Stand=nullptr;
	for(AActor* A:Actors)
	{
		TArray<UVBoRVehicleAlignmentComponent*> Alignments;A->GetComponents(Alignments);AlignmentCount+=Alignments.Num();
		if(auto* T=Cast<AVBoRLoadTarget>(A)) { Target=T; } if(auto* S=Cast<AVBoRBoxStand>(A)) { Stand=S; }
#if WITH_EDITOR
		if(A->GetFolderPath().ToString().StartsWith(TEXT("Approx"))) { for(FName Tag:A->Tags) { TestFalse(TEXT("Approx has no route/car actor tag"),Tag.ToString().StartsWith(TEXT("Car."))||Tag.ToString().StartsWith(TEXT("Route."))); } TArray<UActorComponent*> Cs;A->GetComponents(Cs);for(auto* C:Cs) { for(FName Tag:C->ComponentTags) { TestFalse(TEXT("Approx has no route/car component tag"),Tag.ToString().StartsWith(TEXT("Car."))||Tag.ToString().StartsWith(TEXT("Route."))); } } }
#endif
	}
	TestEqual(TEXT("One alignment"),AlignmentCount,1);
	for(const auto& BP:P4iBlueprints()) { FString N(BP.Name);TestEqual(N,Counts.FindRef(P4iClass(N)),N.Contains(TEXT("KeyFob"))?0:1); }
	if(TestNotNull(TEXT("Stand"),Stand)&&TestNotNull(TEXT("Target"),Target)) { TestTrue(TEXT("Stand load target"),Stand->LoadTarget==Target); }
	for(const auto& Pair:TArray<TPair<FString,FString>>{{TEXT("BP_DoorHandle_FL_Ext"),TEXT("BP_Door_FL")},{TEXT("BP_DoorHandle_FL_Int"),TEXT("BP_Door_FL")},{TEXT("BP_GloveboxHandle"),TEXT("BP_Glovebox")}})
	{
		AActor** A=Actors.FindByPredicate([&](AActor* X){return X->GetClass()==P4iClass(Pair.Key);});
		if(TestTrue(TEXT("Placed handle"),A!=nullptr)) { TestTrue(TEXT("Outliner parent"),(*A)->GetAttachParentActor()&&(*A)->GetAttachParentActor()->GetClass()==P4iClass(Pair.Value)); }
	}
	int32 Tables=0,Routes=0; const AP3VehicleMock* Default=GetDefault<AP3VehicleMock>();
	TSet<UClass*> KeyClasses;
	for(AActor* A:Actors)
	{
		if(auto* Table=Cast<AVBoRKeyTable>(A))
		{
			++Tables; KeyClasses.Add(Table->KeyClass);
			const bool Front=Table->KeyClass==P4iClass(TEXT("BP_KeyFob_Front"));
			if(bApprox) { TestTrue(TEXT("Table position"),A->GetActorLocation().Equals(Front?Default->FrontKeyTableCm:Default->RearKeyTableCm,.01)); }
		}
		if(auto* Route=Cast<AVBoRFloorRoute>(A))
		{
			++Routes;bool Around=A->ActorHasTag(TEXT("Route.AroundCar"));TestTrue(TEXT("Route tag"),Around||A->ActorHasTag(TEXT("Route.ToDriverDoor")));
			// 경로 점은 실제 방에 맞춰 옮기므로 근사 상태에서만 근사 차와 비교함 (P4i R1-1)
			if(!bApprox) { continue; }
			const auto& Points=Around?Default->AroundCarRoutePointsCm:Default->DriverDoorRoutePointsCm;TestEqual(TEXT("Route points"),Route->Spline->GetNumberOfSplinePoints(),Points.Num());for(int32 I=0;I<Points.Num();++I) { TestTrue(TEXT("Route point cm"),Route->Spline->GetLocationAtSplinePoint(I,ESplineCoordinateSpace::Local).Equals(Points[I],.01)); }
		}
	}
	TestEqual(TEXT("Two tables"),Tables,2); TestEqual(TEXT("Two routes"),Routes,2);
	TestTrue(TEXT("Front and rear key classes"),KeyClasses.Contains(P4iClass(TEXT("BP_KeyFob_Front")))&&KeyClasses.Contains(P4iClass(TEXT("BP_KeyFob_Rear"))));
	// ── 근사 상태와 상관없이 지킬 것 ──
	// 부품 BP Actor 는 차 원점. 메시·축만 부품 자리로 감 (Blueprint-Guide §4.1-2, P4i D3·R1-1)
	for(const auto& BP:P4iBlueprints())
	{
		const FString N(BP.Name);
		if(N.Contains(TEXT("KeyFob"))||N==TEXT("BP_GearButton")||N==TEXT("BP_ReachButton")) { continue; }
		AActor** A=Actors.FindByPredicate([&](AActor* X){return X->GetClass()==P4iClass(N);});
		if(A && TestNotNull(TEXT("Placed root"),(*A)->GetRootComponent())) { P4iTransform(*this,N+TEXT(" placed at car origin"),(*A)->GetRootComponent()->GetRelativeTransform(),FTransform::Identity); }
	}
	// 4점 정렬은 Order 1–4 가 하나씩 필요함 (P4h D2, U-18)
	AActor** Core=Actors.FindByPredicate([](AActor* X){return X->GetClass()==P4iClass(TEXT("BP_Car_Core"));});
	if(Core)
	{
		TArray<UVBoRAlignPointComponent*> Points;(*Core)->GetComponents(Points);
		TSet<int32> Orders;for(auto* P:Points) { Orders.Add(P->Order); }
		TestEqual(TEXT("Four alignment points"),Points.Num(),4);
		TestTrue(TEXT("Alignment orders 1-4"),Orders.Num()==4&&Orders.Contains(1)&&Orders.Contains(2)&&Orders.Contains(3)&&Orders.Contains(4));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP4iDependencies, "vBoR.P4i.E5Dependencies", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP4iDependencies::RunTest(const FString&)
{
	auto& Registry=FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	Registry.ScanPathsSynchronous({TEXT("/Game/Main")},true);
	TArray<FAssetData> Assets;Registry.GetAssetsByPath(TEXT("/Game/Main"),Assets,true);TestTrue(TEXT("Main assets found"),Assets.Num()>=18);
	for(const auto& A:Assets) { TArray<FName> Dependencies;Registry.GetDependencies(A.PackageName,Dependencies);for(FName D:Dependencies) { TestFalse(A.PackageName.ToString()+TEXT(" -> ")+D.ToString(),D.ToString().StartsWith(TEXT("/Game/Development/"))); } }
	return true;
}
#endif
