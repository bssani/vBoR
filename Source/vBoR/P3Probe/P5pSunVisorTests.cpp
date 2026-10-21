#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "P5mTestFixture.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "Engine/Blueprint.h"
#include "Engine/Level.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "Interaction/IsdkGrabbableComponent.h"
#include "IsdkRuntimeSettings.h"
#include "UObject/UnrealType.h"

namespace
{
const TCHAR* P5pNames[] = {TEXT("BP_SunVisor_L"), TEXT("BP_SunVisor_R")};

struct FP5pWorld
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	void Play() { World->InitializeActorsForPlay(FURL()); World->GetWorldSettings()->NotifyBeginPlay(); }
	~FP5pWorld() { World->EndPlay(EEndPlayReason::Quit); World->DestroyWorld(false); }
};

UClass* P5pClass(FAutomationTestBase& T, const TCHAR* Name)
{
	const FString Path = FString(TEXT("/Game/Main/Car/Parts/")) + Name + TEXT(".") + Name + TEXT("_C");
	UClass* Class = LoadClass<AVBoRAxisPart>(nullptr, *Path);
	T.TestNotNull(Name, Class);
	return Class;
}

TArray<FProperty*> P5pProperties()
{
	TArray<FProperty*> Properties;
	for (TFieldIterator<FProperty> It(AVBoRAxisPart::StaticClass(), EFieldIteratorFlags::ExcludeSuper); It; ++It)
	{
		if (It->HasAnyPropertyFlags(CPF_Edit) && !It->HasAnyPropertyFlags(CPF_EditConst)) { Properties.Add(*It); }
	}
	return Properties;
}

AP5mTrackedPart* P5pPart(FP5pWorld& W, UClass* Class, bool bFlipHinge = false)
{
	if (!Class) { return nullptr; }
	const AVBoRAxisPart* Defaults = Class->GetDefaultObject<AVBoRAxisPart>();
	auto* Part = W.World->SpawnActorDeferred<AP5mTrackedPart>(AP5mTrackedPart::StaticClass(), FTransform::Identity);
	if (!Part) { return nullptr; }
	// 입력 virtual 만 시험 자식으로 바꿈. 조작 값은 BP 에서 전부 옮김 (P5p D5)
	for (FProperty* Property : P5pProperties()) { Property->CopyCompleteValue_InContainer(Part, Defaults); }
	Part->AxisPivot->SetRelativeTransform(Defaults->AxisPivot->GetRelativeTransform());
	Part->PartMesh->SetStaticMesh(Defaults->PartMesh->GetStaticMesh());
	if (bFlipHinge)
	{
		// 회사 CAD 경첩을 반대 방향(local Z 180 deg)으로 넣고 bReverse 로 맞춘 경우. 메시 자리는 그대로 (P5p R1-1)
		FTransform Pivot = Part->AxisPivot->GetRelativeTransform();
		Pivot.SetRotation(Pivot.GetRotation() * FQuat(FVector::ZAxisVector, FMath::DegreesToRadians(180.)));
		Part->AxisPivot->SetRelativeTransform(Pivot); Part->bReverse = !Part->bReverse;
	}
	Part->FinishSpawning(FTransform::Identity);
	return Part;
}

double P5pAngle(const AVBoRAxisPart* Part) { return Part->GetOpenRatio() * static_cast<double>(Part->MaxAngleDeg); }

UIsdkGrabberComponent* P5pGrabber(FP5pWorld& W, int32 Hand = 0)
{
	auto* Owner = W.World->SpawnActor<AActor>();
	auto* Grabber = NewObject<UIsdkGrabberComponent>(Owner); Owner->SetRootComponent(Grabber);
	Grabber->InteractorTags.AddTag(Hand == 0 ? IsdkGameplayTags::TAG_Isdk_Type_Device_Left : IsdkGameplayTags::TAG_Isdk_Type_Device_Right);
	Grabber->SetWorldLocation(FVector(1000));
	Grabber->SetGrabDetectionTypeAllowed(EIsdkGrabDetectorType::DistanceGrab, false);
	Grabber->SetGrabDetectionTypeAllowed(EIsdkGrabDetectorType::RayGrab, false);
	Grabber->SetGrabInputMethodAllowed(EIsdkGrabInputMethod::Palm, true);
	Grabber->SetGrabInputMethodAllowed(EIsdkGrabInputMethod::Pinch, true);
	Grabber->RegisterComponent();
	return Grabber;
}

bool P5pGrab(FAutomationTestBase& T, AVBoRAxisPart* Part, UIsdkGrabberComponent* Grabber, bool Pinch, int32 Hand = 0)
{
	Grabber->Activate(true); Grabber->SetWorldLocation(Part->GrabBox->GetComponentLocation());
	Grabber->TickComponent(.016f, LEVELTICK_All, nullptr);
	if (Pinch) { Grabber->PinchGrab(); } else { Grabber->PalmGrab(); }
	return T.TestTrue(TEXT("SDK selects visor"), Grabber->GetGrabbedComponent() == Part->Grabbable)
		&& T.TestEqual(TEXT("Selected hand owns visor"), Part->GetHoldingHand(), Hand);
}

void P5pMoveGrab(AVBoRAxisPart* Part, UIsdkGrabberComponent* Grabber, double DeltaDeg)
{
	const FVector Pivot = Part->AxisPivot->GetComponentLocation();
	const FVector Axis = Part->AxisPivot->GetRightVector();
	const FQuat Delta(Axis, FMath::DegreesToRadians(DeltaDeg * (Part->bReverse ? -1 : 1)));
	Grabber->SetWorldLocation(Pivot + Delta.RotateVector(Grabber->GetComponentLocation() - Pivot));
	Grabber->TickComponent(.016f, LEVELTICK_All, nullptr);
}

void P5pRelease(AVBoRAxisPart* Part, UIsdkGrabberComponent* Grabber, bool Pinch)
{
	if (Pinch) { Grabber->PinchRelease(); } else { Grabber->PalmRelease(); }
	Part->Tick(1.f);
}

// 쉬는 메시의 실제 bounds 에 손 점을 놓음. fixture 입력은 경첩의 쉬는 좌표계임 (P5p E4–E8, cm)
// 반지름·면 방향은 제품 ConfigurePush 와 같은 규칙으로 bounds 에서 구함 (경첩 방향을 CAD 로 바꿔도 따라감, P5p R1-1)
struct FP5pSurface
{
	FBox Bounds;
	FVector Radial = FVector::ZeroVector, Normal = FVector::ZeroVector;
	double RadialMin = TNumericLimits<double>::Max(), RadialMax = TNumericLimits<double>::Lowest();
	double NormalMin = TNumericLimits<double>::Max(), NormalMax = TNumericLimits<double>::Lowest();
	explicit FP5pSurface(AVBoRAxisPart* Part)
		: Bounds(Part->PartMesh->GetStaticMesh()->GetBoundingBox().TransformBy(Part->PartMesh->GetRelativeTransform()))
	{
		// 축 Y 에 수직인 Z·X 중 긴 쪽이 반지름, 메시 중심 쪽 부호. 법선 = 축 × 반지름 (VBoRAxisPart.cpp ConfigurePush)
		const int32 R = Bounds.GetExtent().Z >= Bounds.GetExtent().X ? 2 : 0;
		Radial[R] = Bounds.GetCenter()[R] < 0 ? -1 : 1;
		Normal = FVector::CrossProduct(FVector::YAxisVector, Radial);
		for (int32 Corner = 0; Corner < 8; ++Corner)
		{
			const FVector P(Corner & 1 ? Bounds.Max.X : Bounds.Min.X, Corner & 2 ? Bounds.Max.Y : Bounds.Min.Y, Corner & 4 ? Bounds.Max.Z : Bounds.Min.Z);
			const double A = FVector::DotProduct(P, Radial), B = FVector::DotProduct(P, Normal);
			RadialMin = FMath::Min(RadialMin, A); RadialMax = FMath::Max(RadialMax, A);
			NormalMin = FMath::Min(NormalMin, B); NormalMax = FMath::Max(NormalMax, B);
		}
	}
	// 면 0 은 원래 각을 키움. 값 + (내림) 은 bReverse 아니면 면 0, bReverse 면 면 1 (AdvancePush·ValueTransform)
	static int32 LowerFace(const AVBoRAxisPart* Part) { return Part->bReverse ? 1 : 0; }
	FVector Point(const AVBoRAxisPart* Part, double AngleDeg, int32 Face, double OutsideCm = 0, double RadialCm = -1) const
	{
		// 면 0 은 법선 최소 쪽, 면 1 은 최대 쪽. 바깥은 각 면에서 법선 반대·같은 쪽임
		const double N = Face == 0 ? NormalMin - OutsideCm : NormalMax + OutsideCm;
		const double R = RadialCm < 0 ? FMath::Lerp(RadialMin, RadialMax, .8) : RadialCm;
		return FQuat(FVector::YAxisVector, FMath::DegreesToRadians(AngleDeg * (Part->bReverse ? -1 : 1)))
			.RotateVector(Radial * R + Normal * N + FVector(0, Bounds.GetCenter().Y, 0));
	}
};

void P5pPoint(AP5mTrackedPart* Part, FVector Point, int32 Hand = 0)
{
	FVBoRDoorHand& Input = Part->Hands[Hand];
	if (!Input.bTracked) { for (FVector& Other : Input.Points) { Other = FVector(1000); } }
	Input.bTracked = true; Input.Points[1] = Point; Part->Tick(.001f);
}

void P5pPushStart(AP5mTrackedPart* Part, const FP5pSurface& Surface, int32 Face, double RadialCm = -1)
{
	P5pPoint(Part, Surface.Point(Part, P5pAngle(Part), Face, 1, RadialCm));
}

void P5pPushTo(AP5mTrackedPart* Part, const FP5pSurface& Surface, int32 Face, double Target)
{
	const double From = P5pAngle(Part), Direction = Target >= From ? 1 : -1;
	// 얇은 판을 한 frame 에 관통하지 않게 1 deg 간격으로 손 점을 이동함 (P5p E4·E5)
	for (double Angle = From + Direction; Direction * (Target - Angle) > 0; Angle += Direction)
	{
		P5pPoint(Part, Surface.Point(Part, Angle, Face));
	}
	P5pPoint(Part, Surface.Point(Part, Target, Face));
}

void P5pDetach(AP5mTrackedPart* Part) { P5pPoint(Part, FVector(1000)); }

void P5pSameTransform(FAutomationTestBase& T, const FString& Label, const FTransform& A, const FTransform& B)
{
	T.TestTrue(Label + TEXT(" position cm"), A.GetLocation().Equals(B.GetLocation(), .01));
	T.TestTrue(Label + TEXT(" rotation deg"), FMath::RadiansToDegrees(A.GetRotation().AngularDistance(B.GetRotation())) < .01);
	T.TestTrue(Label + TEXT(" scale"), A.GetScale3D().Equals(B.GetScale3D(), 1.e-4));
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5pDefaults, "vBoR.P5p.E1BlueprintDefaults", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5pDefaults::RunTest(const FString&)
{
	for (const TCHAR* Name : P5pNames)
	{
		UClass* Class = P5pClass(*this, Name); if (!Class) { continue; }
		TestTrue(TEXT("Exact native parent"), Class->GetSuperClass() == AVBoRAxisPart::StaticClass());
		const auto* BP = Cast<UBlueprint>(Class->ClassGeneratedBy);
		if (TestNotNull(TEXT("Blueprint source"), BP))
		{
			for (UEdGraph* Graph : BP->UbergraphPages) { TestEqual(TEXT("Event graph empty"), Graph->Nodes.Num(), 0); }
			for (UEdGraph* Graph : BP->FunctionGraphs)
			{
				for (UEdGraphNode* Node : Graph->Nodes) { TestEqual(TEXT("Function has entry only"), Node->GetClass()->GetFName(), FName(TEXT("K2Node_FunctionEntry"))); }
				TestTrue(TEXT("At most one function entry"), Graph->Nodes.Num() <= 1);
			}
		}
		const auto* Part = Class->GetDefaultObject<AVBoRAxisPart>();
		TestEqual(TEXT("Stay"), Part->Release, EVBoRAxisRelease::Stay);
		TestEqual(TEXT("Rotate"), Part->Movement, EVBoRAxisMovement::Rotate);
		TestEqual(TEXT("Y axis"), Part->Axis.GetValue(), EAxis::Y);
		TestTrue(TEXT("Palm pinch push enabled"), Part->bAllowPalmGrab && Part->bAllowPinchGrab && Part->bAllowPush);
		TestEqual(TEXT("No door assembly"), Part->DoorHandleSide, EVBoRDoorHandleSide::None);
		TestEqual(TEXT("Spectator name"), Part->SpectatorName.ToString(), FString(TEXT("Sun Visor")));
		TestTrue(TEXT("Positive angle limit"), Part->MaxAngleDeg > 0);
		TestFalse(TEXT("Box grab for approximate cube"), Part->bGrabWithMesh);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5pPalm, "vBoR.P5p.E2PalmDownAndStay", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5pPalm::RunTest(const FString&)
{
	for (const TCHAR* Name : P5pNames)
	{
		FP5pWorld W; auto* Part = P5pPart(W, P5pClass(*this, Name)); if (!TestNotNull(Name, Part)) { continue; }
		auto* Grabber = P5pGrabber(W); W.Play();
		const double TipBefore = Part->PartMesh->Bounds.GetBox().Min.Z;
		const double Target = FMath::Min(40., Part->MaxAngleDeg * .5);
		if (!P5pGrab(*this, Part, Grabber, false)) { continue; }
		P5pMoveGrab(Part, Grabber, Target);
		TestTrue(TEXT("Palm lowers to positive value"), FMath::IsNearlyEqual(P5pAngle(Part), Target, .001));
		TestTrue(TEXT("Actual mesh bottom moves downward"), Part->PartMesh->Bounds.GetBox().Min.Z < TipBefore);
		P5pRelease(Part, Grabber, false);
		TestEqual(TEXT("Palm released"), Part->GetHoldingHand(), INDEX_NONE);
		TestTrue(TEXT("Palm release stays"), FMath::IsNearlyEqual(P5pAngle(Part), Target, .001));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5pPinch, "vBoR.P5p.E3PinchUpAndStay", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5pPinch::RunTest(const FString&)
{
	for (const TCHAR* Name : P5pNames)
	{
		FP5pWorld W; auto* Part = P5pPart(W, P5pClass(*this, Name)); if (!TestNotNull(Name, Part)) { continue; }
		auto* Grabber = P5pGrabber(W); W.Play();
		const double Down = FMath::Min(40., Part->MaxAngleDeg * .5);
		if (!P5pGrab(*this, Part, Grabber, false)) { continue; }
		P5pMoveGrab(Part, Grabber, Down); P5pRelease(Part, Grabber, false);
		const double BottomBefore = Part->PartMesh->Bounds.GetBox().Min.Z;
		if (!P5pGrab(*this, Part, Grabber, true)) { continue; }
		TestTrue(TEXT("SDK input is pinch"), Part->GetDebugText().Contains(TEXT("grab=pinch")));
		P5pMoveGrab(Part, Grabber, -Down / 2); P5pRelease(Part, Grabber, true);
		TestEqual(TEXT("Pinch released"), Part->GetHoldingHand(), INDEX_NONE);
		TestTrue(TEXT("Pinch raises value and stays"), FMath::IsNearlyEqual(P5pAngle(Part), Down / 2, .001));
		TestTrue(TEXT("Actual mesh bottom rises"), Part->PartMesh->Bounds.GetBox().Min.Z > BottomBefore);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5pFront, "vBoR.P5p.E4FrontPushAndMax", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5pFront::RunTest(const FString&)
{
	for (const TCHAR* Name : P5pNames) { for (bool bFlip : {false, true})
	{
		FP5pWorld W; auto* Part = P5pPart(W, P5pClass(*this, Name), bFlip); if (!TestNotNull(Name, Part)) { continue; }
		const FString Label = FString(Name) + (bFlip ? TEXT(" flipped hinge") : TEXT(""));
		const FP5pSurface Surface(Part); const int32 Face = FP5pSurface::LowerFace(Part);
		W.Play(); const double BottomBefore = Part->PartMesh->Bounds.GetBox().Min.Z;
		P5pPushStart(Part, Surface, Face);
		P5pPushTo(Part, Surface, Face, Part->MaxAngleDeg * .5);
		TestTrue(Label + TEXT(" lowering face increases value"), P5pAngle(Part) > 0 && Part->GetDebugText().Contains(FString::Printf(TEXT("face=%d"), Face)));
		TestTrue(Label + TEXT(" push lowers actual mesh bottom"), Part->PartMesh->Bounds.GetBox().Min.Z < BottomBefore);
		P5pPushTo(Part, Surface, Face, Part->MaxAngleDeg + 1);
		TestTrue(Label + TEXT(" front push clamps at maximum deg"), FMath::IsNearlyEqual(P5pAngle(Part), static_cast<double>(Part->MaxAngleDeg), .001));
	} }
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5pBack, "vBoR.P5p.E5BackPushAndZero", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5pBack::RunTest(const FString&)
{
	for (const TCHAR* Name : P5pNames) { for (bool bFlip : {false, true})
	{
		FP5pWorld W; auto* Part = P5pPart(W, P5pClass(*this, Name), bFlip); if (!TestNotNull(Name, Part)) { continue; }
		const FString Label = FString(Name) + (bFlip ? TEXT(" flipped hinge") : TEXT(""));
		const FP5pSurface Surface(Part); const int32 Down = FP5pSurface::LowerFace(Part), Up = 1 - Down;
		W.Play(); P5pPushStart(Part, Surface, Down);
		P5pPushTo(Part, Surface, Down, Part->MaxAngleDeg * .5); P5pDetach(Part);
		const double Before = P5pAngle(Part); TestTrue(Label + TEXT(" visor initially lowered"), Before > 0);
		P5pPushStart(Part, Surface, Up); P5pPushTo(Part, Surface, Up, Before / 2);
		TestTrue(Label + TEXT(" raising face reduces value"), P5pAngle(Part) < Before && Part->GetDebugText().Contains(FString::Printf(TEXT("face=%d"), Up)));
		P5pPushTo(Part, Surface, Up, -1);
		TestTrue(Label + TEXT(" back push clamps at zero deg"), FMath::IsNearlyZero(P5pAngle(Part), .001));
	} }
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5pPushStay, "vBoR.P5p.E6PushDetachAndStay", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5pPushStay::RunTest(const FString&)
{
	for (const TCHAR* Name : P5pNames)
	{
		FP5pWorld W; auto* Part = P5pPart(W, P5pClass(*this, Name)); if (!TestNotNull(Name, Part)) { continue; }
		const FP5pSurface Surface(Part); const int32 Face = FP5pSurface::LowerFace(Part);
		W.Play(); P5pPushStart(Part, Surface, Face);
		P5pPushTo(Part, Surface, Face, Part->MaxAngleDeg * .4);
		const double Before = P5pAngle(Part); TestTrue(TEXT("Push moved visor"), Before > 0);
		P5pDetach(Part); Part->Tick(1.f);
		TestFalse(TEXT("Detached hand ends push"), Part->IsSpectatorInteracting());
		TestTrue(TEXT("Push release stays"), FMath::IsNearlyEqual(P5pAngle(Part), Before, .001));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5pPriority, "vBoR.P5p.E7GrabPriority", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5pPriority::RunTest(const FString&)
{
	for (const TCHAR* Name : P5pNames) { for (int32 Hand : {0, 1})
	{
		FP5pWorld W; auto* Part = P5pPart(W, P5pClass(*this, Name)); if (!TestNotNull(Name, Part)) { continue; }
		const FP5pSurface Surface(Part); const int32 Face = FP5pSurface::LowerFace(Part);
		auto* Grabber = P5pGrabber(W, Hand); Part->PushGrabbers[Hand] = Grabber; W.Play();
		P5pPushStart(Part, Surface, Face); P5pPushTo(Part, Surface, Face, Part->MaxAngleDeg * .2);
		TestTrue(TEXT("Push active before grab"), Part->IsSpectatorInteracting());
		const double Before = P5pAngle(Part);
		if (!P5pGrab(*this, Part, Grabber, true, Hand)) { continue; }
		TestTrue(TEXT("Grab immediately clears push"), Part->GetDebugText().Contains(TEXT("push=-1")));
		P5pMoveGrab(Part, Grabber, 5);
		TestTrue(TEXT("Selected hand drives visor"), FMath::IsNearlyEqual(P5pAngle(Part), Before + 5, .001));
		P5pPoint(Part, Surface.Point(Part, Before + 15, Face), 1 - Hand);
		TestTrue(TEXT("Other contact cannot push held visor"), Part->GetDebugText().Contains(TEXT("push=-1")));
		TestTrue(TEXT("Held angle follows grab only"), FMath::IsNearlyEqual(P5pAngle(Part), Before + 5, .001));
		Part->Hands[0] = Part->Hands[1] = FVBoRDoorHand(); P5pRelease(Part, Grabber, true);
	} }
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5pHinge, "vBoR.P5p.E8NearHingeRejected", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5pHinge::RunTest(const FString&)
{
	for (const TCHAR* Name : P5pNames)
	{
		FP5pWorld W; auto* Part = P5pPart(W, P5pClass(*this, Name)); if (!TestNotNull(Name, Part)) { continue; }
		const FP5pSurface Surface(Part); const int32 Face = FP5pSurface::LowerFace(Part); W.Play();
		const double Radius = Part->PushMinAxisDistanceCm * .1;
		P5pPushStart(Part, Surface, Face, Radius);
		const FVector Contact = Surface.Point(Part, 0, Face, -.05, Radius);
		TestTrue(TEXT("Contact is inside excluded axis radius cm"), FVector(Contact.X, 0, Contact.Z).Size() < Part->PushMinAxisDistanceCm);
		P5pPoint(Part, Contact);
		TestFalse(TEXT("Hinge contact does not start push"), Part->IsSpectatorInteracting());
		TestTrue(TEXT("Hinge contact does not move visor"), FMath::IsNearlyZero(P5pAngle(Part), .001));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5pIndependent, "vBoR.P5p.E9IndependentVisors", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5pIndependent::RunTest(const FString&)
{
	FP5pWorld W; auto* Left = P5pPart(W, P5pClass(*this, P5pNames[0])); auto* Right = P5pPart(W, P5pClass(*this, P5pNames[1]));
	if (!TestNotNull(TEXT("Left"), Left) || !TestNotNull(TEXT("Right"), Right)) { return false; }
	auto* Grabber = P5pGrabber(W); W.Play(); const FTransform RightBefore = Right->PartMesh->GetComponentTransform();
	if (!P5pGrab(*this, Left, Grabber, false)) { return false; }
	P5pMoveGrab(Left, Grabber, FMath::Min(40., Left->MaxAngleDeg * .5)); P5pRelease(Left, Grabber, false); Right->Tick(1.f);
	TestTrue(TEXT("Left lowered"), P5pAngle(Left) > 0);
	TestTrue(TEXT("Right remains zero"), FMath::IsNearlyZero(P5pAngle(Right), .001));
	P5pSameTransform(*this, TEXT("Right mesh independent"), Right->PartMesh->GetComponentTransform(), RightBefore);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5pLevel, "vBoR.P5p.E10MainLevel", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5pLevel::RunTest(const FString&)
{
	UPackage* Package = LoadPackage(nullptr, TEXT("/Game/Main/L_Main"), LOAD_None);
	UWorld* World = Package ? UWorld::FindWorldInPackage(Package) : nullptr;
	if (!TestNotNull(TEXT("Main map package"), World)) { return false; }
	for (const TCHAR* Name : P5pNames)
	{
		UClass* Class = P5pClass(*this, Name); if (!Class) { continue; }
		int32 Count = 0;
		for (ULevel* Level : World->GetLevels()) { for (AActor* Actor : Level->Actors)
		{
			if (!Actor || Actor->GetClass() != Class) { continue; }
			++Count; TestNotNull(TEXT("Placed root"), Actor->GetRootComponent());
			if (Actor->GetRootComponent()) { P5pSameTransform(*this, Name, Actor->GetRootComponent()->GetRelativeTransform(), FTransform::Identity); }
#if WITH_EDITOR
			TestFalse(TEXT("Real part outside Approx folder"), Actor->GetFolderPath().ToString().StartsWith(TEXT("Approx")));
#endif
		} }
		TestEqual(FString(Name) + TEXT(" placed once"), Count, 1);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP5pCopy, "vBoR.P5p.E11CopiedBlueprintValues", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FP5pCopy::RunTest(const FString&)
{
	const auto Properties = P5pProperties(); TSet<FName> Names;
	for (FProperty* Property : Properties) { Names.Add(Property->GetFName()); }
	TestTrue(TEXT("Copied list is not empty"), Properties.Num() > 0);
	for (const TCHAR* Required : {TEXT("Release"), TEXT("Axis"), TEXT("MaxAngleDeg"), TEXT("bAllowPalmGrab"), TEXT("bAllowPinchGrab"), TEXT("bAllowPush"), TEXT("MeshRestFromRoot"), TEXT("SpectatorName")})
	{
		TestTrue(FString(TEXT("Copied list includes ")) + Required, Names.Contains(FName(Required)));
	}
	for (const TCHAR* Name : P5pNames)
	{
		// 실제 BP 와 시험 자식은 Play 없는 별도 world 에서만 겹쳐 놓음 (P5p E11)
		FP5pWorld W; UClass* Class = P5pClass(*this, Name); if (!Class) { continue; }
		auto* Actual = W.World->SpawnActor<AVBoRAxisPart>(Class);
		auto* Copy = P5pPart(W, Class);
		if (!TestNotNull(TEXT("Actual BP"), Actual) || !TestNotNull(TEXT("Copied fixture"), Copy)) { continue; }
		for (FProperty* Property : Properties) { TestTrue(FString(Name) + TEXT(" copied ") + Property->GetName(), Property->Identical_InContainer(Actual, Copy)); }
		P5pSameTransform(*this, TEXT("AxisPivot relative"), Actual->AxisPivot->GetRelativeTransform(), Copy->AxisPivot->GetRelativeTransform());
		TestTrue(TEXT("Static mesh copied"), Actual->PartMesh->GetStaticMesh() == Copy->PartMesh->GetStaticMesh());
		P5pSameTransform(*this, TEXT("Mesh world after construction"), Actual->PartMesh->GetComponentTransform(), Copy->PartMesh->GetComponentTransform());
		TestTrue(TEXT("GrabBox relative location cm"), Actual->GrabBox->GetRelativeLocation().Equals(Copy->GrabBox->GetRelativeLocation(), .01));
		TestTrue(TEXT("GrabBox extent cm"), Actual->GrabBox->GetUnscaledBoxExtent().Equals(Copy->GrabBox->GetUnscaledBoxExtent(), .01));
	}
	return true;
}
#endif
