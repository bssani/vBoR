#include "VBoRHingedPanel.h"
#include "VBoRHandRigPawn.h"
#include "VBoRAxisPart.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Components/AudioComponent.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "Interaction/IsdkGrabbableComponent.h"
#include "Interaction/IsdkGrabberComponent.h"
#include "Interaction/Pointable/IsdkInteractionPointerEvent.h"
#include "IsdkFunctionLibrary.h"
#include "IsdkContentAssetPaths.h"
#include "OculusXRInputFunctionLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Sound/SoundBase.h"
#include "UObject/ConstructorHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogAxisDoor, Log, All);

UVBoRDoorGrabTransformer::UVBoRDoorGrabTransformer() { ConfigureDoor(); }
void UVBoRDoorGrabTransformer::ConfigureDoor()
{
	bIsThrowable = false;
	MultiGrabBehavior = EIsdkMultiGrabBehavior::MultiGrab;
}

FTransform UVBoRDoorTransformer::UpdateTransform(const TArray<FIsdkGrabPose>& Poses, const FIsdkTargetTransform& Target)
{
	const AVBoRHingedPanel* Door = GetTypedOuter<AVBoRHingedPanel>();
	return Door ? Door->HingePivot->GetComponentTransform().GetRelativeTransform(Target.ParentWorldTransform) : Target.RelativeTransform;
}

FTransform UVBoRDoorTransformer::EndTransform(const FIsdkTargetTransform& Target)
{
	bActive = false;
	return UpdateTransform({}, Target);
}

AVBoRHingedPanel::AVBoRHingedPanel()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostUpdateWork;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("DoorOrigin")));
	HingePivot = CreateDefaultSubobject<USceneComponent>(TEXT("HingePivot"));
	HingePivot->SetupAttachment(RootComponent);
	DoorMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DoorMesh"));
	DoorMesh->SetupAttachment(HingePivot);
	DoorMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	DoorMesh->SetSimulatePhysics(false);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	DoorMesh->SetStaticMesh(Cube.Object);
	DoorMesh->SetRelativeLocation(FVector(55, 0, 52.5));
	DoorMesh->SetRelativeScale3D(FVector(1.1, 0.1, 1.05));
	PushFaceBox = CreateDefaultSubobject<UBoxComponent>(TEXT("PushFaceBox"));
	PushFaceBox->SetupAttachment(HingePivot);
	PushFaceBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PushFaceBox->SetGenerateOverlapEvents(false);
	PushFaceBox->SetHiddenInGame(true);
	OutsideHandle = CreateDefaultSubobject<UBoxComponent>(TEXT("OutsideHandle"));
	InsideHandle = CreateDefaultSubobject<UBoxComponent>(TEXT("InsideHandle"));
	for (UBoxComponent* Handle : {OutsideHandle.Get(), InsideHandle.Get()})
	{
		Handle->SetupAttachment(HingePivot);
		Handle->SetBoxExtent(FVector(9, 4, 4));
		Handle->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		Handle->SetCollisionResponseToAllChannels(ECR_Ignore);
		// Palm detector 의 overlap 만 허용함 (손잡이 감지, P2 E20)
		Handle->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);
		Handle->SetGenerateOverlapEvents(true);
	}
	OutsideHandle->SetRelativeLocation(FVector(110, 9, 52.5));
	InsideHandle->SetRelativeLocation(FVector(110, -9, 52.5));
	OutsideHandleMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("OutsideHandleMesh"));
	InsideHandleMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("InsideHandleMesh"));
	OutsideHandleMesh->SetupAttachment(OutsideHandle); InsideHandleMesh->SetupAttachment(InsideHandle);
	for (UStaticMeshComponent* Visual : {OutsideHandleMesh.Get(), InsideHandleMesh.Get()})
	{
		Visual->SetStaticMesh(Cube.Object);
		Visual->SetRelativeScale3D(FVector(0.18, 0.04, 0.04));
		Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	OutsideGrab = CreateDefaultSubobject<UIsdkGrabbableComponent>(TEXT("OutsideGrab"));
	OutsideGrab->SetupAttachment(OutsideHandle);
	InsideGrab = CreateDefaultSubobject<UIsdkGrabbableComponent>(TEXT("InsideGrab"));
	InsideGrab->SetupAttachment(InsideHandle);
	OutsideTransformer = CreateDefaultSubobject<UVBoRDoorGrabTransformer>(TEXT("OutsideTransformer"));
	InsideTransformer = CreateDefaultSubobject<UVBoRDoorGrabTransformer>(TEXT("InsideTransformer"));
	DoorTransformer = CreateDefaultSubobject<UVBoRDoorTransformer>(TEXT("HingeOnlyTransformer"));
	StateAudio = CreateDefaultSubobject<UAudioComponent>(TEXT("LockAudio"));
	StateAudio->SetupAttachment(HingePivot);
	StateAudio->bAutoActivate = false;
	static ConstructorHelpers::FObjectFinder<USoundBase> Sound(IsdkContentAssetPaths::Audio::Interaction_BasicPoke_ButtonPress);
	StateAudio->SetSound(Sound.Object);
	SyncSettings();
}

void AVBoRHingedPanel::SyncSettings()
{
	if (Motion.bUseLatch != bUseLatch)
	{
		Motion.bUseLatch = bUseLatch;
		Motion.bLocked = bUseLatch && Motion.AngleDeg <= LatchAngleDeg;
		Motion.bLatchArmed = false;
	}
	if (DoorMesh->GetStaticMesh() || bUsePushFaceBox)
	{
		FBox Bounds = MeshFaceBounds();
		if (!bPushOnMeshSurface && bUsePushFaceBox && PushFaceBox->GetRelativeRotation().IsNearlyZero())
		{
			const FVector Extent = PushFaceBox->GetUnscaledBoxExtent() * PushFaceBox->GetRelativeScale3D().GetAbs();
			Bounds = FBox(PushFaceBox->GetRelativeLocation() - Extent, PushFaceBox->GetRelativeLocation() + Extent);
		}
		Motion.FaceMinCm = Bounds.Min; Motion.FaceMaxCm = Bounds.Max;
		Motion.ThicknessCm = Bounds.GetSize().Y;
	}
	Motion.MaxAngleDeg = MaxAngleDeg; Motion.MaxSpeedDegPerSecond = MaxSpeedDegPerSecond;
	Motion.LatchAngleDeg = LatchAngleDeg; Motion.RearmAngleDeg = RearmAngleDeg;
	Motion.PalmLimitDeg = PalmLimitDeg; Motion.MinAxisDistanceCm = MinAxisDistanceCm;
	Motion.bAllowPalmOpening = bAllowPalmOpening;
	if (bPushOnMeshSurface && bMeshPushReady)
	{
		Motion.QueryFace = [this](const FVector& Point, int32 Face, double& Y) { return QueryMeshFace(Point, Face, Y); };
	}
	else { Motion.QueryFace = nullptr; }
	UpdateGrabAreas();
}

void AVBoRHingedPanel::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	DoorMesh->SetRelativeTransform(MeshRestFromRoot.GetRelativeTransform(HingePivot->GetRelativeTransform()));
	ConfigurePushCollision();
	if (!bUsePushFaceBox && DoorMesh->GetStaticMesh())
	{
		const FBox Bounds = MeshFaceBounds();
		PushFaceBox->SetRelativeTransform(FTransform(FQuat::Identity, Bounds.GetCenter()));
		PushFaceBox->SetBoxExtent(Bounds.GetExtent());
	}
	SyncSettings();
}

void AVBoRHingedPanel::SetAxisHandle(int32 Handle, AVBoRAxisPart* Part)
{
	if (Handle < 0 || Handle > 1) { return; }
	// 이번 연결의 문제만 알림 (전체 설정 경고는 BeginPlay 한 번, P2e E20)
	TArray<FString> Problems;
	const bool bConnect = !Part || CanConnectAxisHandle(Part, Problems);
	for (const FString& Problem : Problems) { UE_LOG(LogAxisDoor, Warning, TEXT("%s P2e setup: %s"), *GetName(), *Problem); }
	ApplyAxisHandle(Handle, bConnect ? Part : nullptr);
}

FBox AVBoRHingedPanel::MeshFaceBounds() const
{
	return DoorMesh->GetStaticMesh() ? DoorMesh->GetStaticMesh()->GetBoundingBox().TransformBy(DoorMesh->GetRelativeTransform()) : FBox(Motion.FaceMinCm, Motion.FaceMaxCm);
}

void AVBoRHingedPanel::ConfigurePushCollision()
{
	// 컴포넌트 trace 만 허용함 (손 감지·짐·월드 질의와 분리, P2g D6)
	DoorMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	DoorMesh->SetGenerateOverlapEvents(false);
	DoorMesh->SetCollisionEnabled(bPushOnMeshSurface ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
}

bool AVBoRHingedPanel::QueryMeshFace(const FVector& Point, int32 Face, double& OutY) const
{
	const FBox Bounds = MeshFaceBounds();
	OutY = Face == 0 ? Bounds.Max.Y : Bounds.Min.Y;
	constexpr double TraceMarginCm = 1; // bounds 밖에서 시작함 (면 위 시작의 모호함 방지, P2g §5)
	FVector Start(Point.X, Bounds.Max.Y + TraceMarginCm, Point.Z);
	FVector End(Point.X, Bounds.Min.Y - TraceMarginCm, Point.Z);
	if (Face == 1) { Swap(Start, End); }
	const FTransform Frame = HingePivot->GetComponentTransform();
	FCollisionQueryParams Params;
	Params.bTraceComplex = true;
	FHitResult Hit;
	if (!DoorMesh->LineTraceComponent(Hit, Frame.TransformPosition(Start), Frame.TransformPosition(End), Params)) { return false; }
	OutY = Frame.InverseTransformPosition(Hit.ImpactPoint).Y;
	return true;
}

bool AVBoRHingedPanel::HasTraceableMesh() const
{
	const FBox Bounds = MeshFaceBounds();
	// 모서리는 피하고 내부 3×3만 검사함 (시작·설정 검사 공통, P2g D7)
	for (int32 X = 1; X <= 3; ++X)
	{
		for (int32 Z = 1; Z <= 3; ++Z)
		{
			double Y;
			if (QueryMeshFace(FVector(FMath::Lerp(Bounds.Min.X, Bounds.Max.X, X / 4.0), 0,
				FMath::Lerp(Bounds.Min.Z, Bounds.Max.Z, Z / 4.0)), 0, Y)) { return true; }
		}
	}
	return false;
}

void AVBoRHingedPanel::CollectHandleCandidates(TArray<AVBoRAxisPart*> (&Candidates)[2]) const
{
	ForEachAttachedActors([&](AActor* Actor)
	{
		AVBoRAxisPart* Part = Cast<AVBoRAxisPart>(Actor);
		if (Part && Part->DoorHandleSide != EVBoRDoorHandleSide::None)
		{
			Candidates[Part->DoorHandleSide == EVBoRDoorHandleSide::Outside ? 0 : 1].AddUnique(Part);
		}
		return true;
	});
}

TArray<AVBoRAxisPart*> AVBoRHingedPanel::GetHandleClaims() const
{
	TArray<AVBoRAxisPart*> Candidates[2]; CollectHandleCandidates(Candidates);
	TArray<AVBoRAxisPart*> Claims = Candidates[0];
	for (AVBoRAxisPart* Part : Candidates[1]) { Claims.AddUnique(Part); }
	for (AVBoRAxisPart* Part : {OutsideAxisHandle.Get(), InsideAxisHandle.Get()}) { if (Part) { Claims.AddUnique(Part); } }
	return Claims;
}

bool AVBoRHingedPanel::IsHandleShared(const AVBoRAxisPart* Part) const
{
	if (!GetWorld()) { return false; }
	for (TActorIterator<AVBoRHingedPanel> It(GetWorld()); It; ++It)
	{
		if (*It != this && It->GetHandleClaims().Contains(Part)) { return true; }
	}
	return false;
}

bool AVBoRHingedPanel::CanConnectAxisHandle(AVBoRAxisPart* Part, TArray<FString>& Problems) const
{
	bool bConnect = true;
	const bool bUnderHinge = Part->GetRootComponent()->IsAttachedTo(HingePivot);
	if (Part->IsChildActor() && !bUnderHinge)
	{
		Problems.AddUnique(FString::Printf(TEXT("Child Actor %s: move its Child Actor component under HingePivot."), *Part->GetName()));
		bConnect = false;
	}
	// 먼저 연결한 판은 건드리지 않음 (최초 후보 기록 없이 현재 연결만 확인, P2e D5⑤·Q3)
	if (Part->LinkedDoor && Part->LinkedDoor != this)
	{
		Problems.AddUnique(FString::Printf(TEXT("shared handle %s; first connected panel keeps it."), *Part->GetName()));
		bConnect = false;
	}
	if (bConnect && !bUnderHinge && Motion.AngleDeg > 0)
	{
		Problems.AddUnique(FString::Printf(TEXT("%s attaches to an open panel (%.2f deg); current world pose is kept."), *Part->GetName(), Motion.AngleDeg));
	}
	return bConnect;
}

AVBoRHingedPanel::FSetupAssessment AVBoRHingedPanel::EvaluateSetup() const
{
	FSetupAssessment Result;
	TArray<AVBoRAxisPart*> Candidates[2]; CollectHandleCandidates(Candidates);
	for (AVBoRAxisPart* Claim : GetHandleClaims())
	{
		if (IsHandleShared(Claim)) { Result.Problems.AddUnique(FString::Printf(TEXT("shared handle %s; first connected panel keeps it."), *Claim->GetName())); }
	}
	for (int32 I = 0; I < 2; ++I)
	{
		const TCHAR* Side = I == 0 ? TEXT("outside") : TEXT("inside");
		AVBoRAxisPart* Explicit = I == 0 ? OutsideAxisHandle.Get() : InsideAxisHandle.Get();
		AVBoRAxisPart* Part = Explicit;
		if (Explicit)
		{
			if (Candidates[I].ContainsByPredicate([&](const AVBoRAxisPart* Candidate) { return Candidate != Explicit; }))
			{
				Result.Problems.Add(FString::Printf(TEXT("%s slot %s overrides different attached candidates."), Side, *Explicit->GetName()));
			}
			if (Explicit->DoorHandleSide == (I == 0 ? EVBoRDoorHandleSide::Inside : EVBoRDoorHandleSide::Outside))
			{
				Result.Problems.Add(FString::Printf(TEXT("%s slot %s has the opposite DoorHandleSide; slot wins."), Side, *Explicit->GetName()));
			}
		}
		else
		{
			// 반대쪽 칸에 지정한 손잡이는 다시 고르지 않음 (칸이 쪽 값보다 우선, P2e E9)
			TArray<AVBoRAxisPart*> Available = Candidates[I];
			Available.Remove(OutsideAxisHandle); Available.Remove(InsideAxisHandle);
			if (Available.Num() == 1) { Part = Available[0]; }
			else if (Available.Num() > 1) { Result.Problems.Add(FString::Printf(TEXT("%s has multiple attached candidates; assign the slot explicitly."), Side)); }
		}
		if (!Part) { continue; }
		if (CanConnectAxisHandle(Part, Result.Problems)) { Result.Handles[I] = Part; }
	}
	if (DoorMesh->GetStaticMesh() && DoorMesh->GetStaticMesh()->GetPathName() != TEXT("/Engine/BasicShapes/Cube.Cube") &&
		MeshRestFromRoot.Equals(GetDefault<AVBoRHingedPanel>()->MeshRestFromRoot))
	{
		Result.Problems.Add(TEXT("Non-cube DoorMesh still uses mock MeshRestFromRoot; check CAD car-origin pivot (usually identity)."));
	}
	if (bPushOnMeshSurface)
	{
		Result.bMeshTraceable = HasTraceableMesh();
		if (!Result.bMeshTraceable) { Result.Problems.Add(TEXT("DoorMesh has no traceable collision; push uses bounds.")); }
		if (bUsePushFaceBox) { Result.Notices.Add(TEXT("PushFaceBox ignored while pushing on the mesh surface.")); }
	}
	if (!bPushOnMeshSurface && !bUsePushFaceBox && MeshFaceBounds().GetSize().Y > PushFaceWarnThicknessCm)
	{
		Result.Problems.Add(FString::Printf(TEXT("Mesh face thickness %.2f cm exceeds %.2f cm; check merged protrusions or use PushFaceBox."), MeshFaceBounds().GetSize().Y, PushFaceWarnThicknessCm));
	}
	if (!bPushOnMeshSurface && bUsePushFaceBox && !PushFaceBox->GetRelativeRotation().IsNearlyZero())
	{
		Result.Problems.Add(TEXT("PushFaceBox relative rotation must be zero; using mesh bounds."));
	}
	return Result;
}

void AVBoRHingedPanel::CheckSetup()
{
	const FSetupAssessment Setup = EvaluateSetup();
	SetupReport = Setup.Problems.IsEmpty() ? TEXT("문제 없음") : FString::Join(Setup.Problems, TEXT("\n"));
	for (const FString& Notice : Setup.Notices) { SetupReport += TEXT("\n") + Notice; }
	UE_LOG(LogAxisDoor, Log, TEXT("%s setup check:\n%s"), *GetName(), *SetupReport);
}

void AVBoRHingedPanel::ApplyAxisHandle(int32 Handle, AVBoRAxisPart* Part)
{
	AVBoRAxisPart* Previous = Handle == 0 ? OutsideAxisHandle.Get() : InsideAxisHandle.Get();
	if (Previous && Previous != Part && Previous->LinkedDoor == this)
	{
		Previous->LinkedDoor = nullptr; Previous->DoorHandle = INDEX_NONE;
	}
	if (Part && !Part->GetRootComponent()->IsAttachedTo(HingePivot))
	{
		if (Part->AttachToComponent(HingePivot, FAttachmentTransformRules::KeepWorldTransform))
		{
			UE_LOG(LogAxisDoor, Log, TEXT("%s P2e attached handle=%s to HingePivot (world pose kept)"), *GetName(), *Part->GetName());
		}
		else { Part = nullptr; }
	}
	if (Handle == 0) { OutsideAxisHandle = Part; } else { InsideAxisHandle = Part; }
	UpdateGrabAreas();
	if (Part) { Part->ConnectDoor(this, Handle); }
}

bool AVBoRHingedPanel::IsGrabAreaEnabled(int32 Handle) const
{
	return Handle == 0 ? bUseOutsideGrabArea && !OutsideAxisHandle : bUseInsideGrabArea && !InsideAxisHandle;
}

void AVBoRHingedPanel::UpdateGrabAreas()
{
	for (int32 I = 0; I < 2; ++I)
	{
		const bool bEnabled = IsGrabAreaEnabled(I);
		UBoxComponent* Box = I == 0 ? OutsideHandle.Get() : InsideHandle.Get();
		UIsdkGrabbableComponent* Grab = I == 0 ? OutsideGrab.Get() : InsideGrab.Get();
		Box->SetCollisionEnabled(bEnabled ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
		Box->SetGenerateOverlapEvents(bEnabled);
		Box->SetVisibility(bEnabled, true);
		Grab->SetGrabInputMethodAllowed(EIsdkGrabInputMethod::Palm, bEnabled);
	}
}

bool AVBoRHingedPanel::Unlock()
{
	SyncSettings();
	const int32 Unlocks = Motion.Metrics.Unlocks;
	if (!Motion.Unlock()) { return false; }
	// SDK 상태는 건드리지 않음 (버튼 포인터 이벤트에서도 안전하게 호출, P2d D3)
	ApplyState(Motion.AngleDeg, Unlocks, Motion.Metrics.Latches);
	return true;
}

bool AVBoRHingedPanel::BeginGrabFromAxisPart(AVBoRAxisPart* Part)
{
	if (!Part || (Part != OutsideAxisHandle && Part != InsideAxisHandle) || Part->HoldingHand == INDEX_NONE) { return false; }
	FVBoRDoorHand Input; ReadHand(Part->HoldingHand, Input);
	return BeginAxisGrab(Part, Input);
}

bool AVBoRHingedPanel::BeginAxisGrab(AVBoRAxisPart* Part, const FVBoRDoorHand& Input)
{
	const int32 Hand = Part->HoldingHand, Handle = Part == OutsideAxisHandle ? 0 : 1;
	if (bNeedsPalmOpen[Hand] || Part->GetOpenRatio() < Part->OpenThreshold) { return false; }
	const int32 Unlocks = Motion.Metrics.Unlocks;
	const bool bWasLocked = Motion.bLocked;
	if (!Motion.BeginGrab(Hand, Handle, Input)) { return false; }
	HoldingAxisPart = Part;
	AxisHoldHand = Hand; AxisHoldHandle = Handle;
	AxisHoldStartSeconds = Motion.Metrics.HeldSeconds + Motion.Metrics.PausedSeconds;
	UE_LOG(LogAxisDoor, Log, TEXT("%s axis hold begin part=%s handle=%s hand=%s locked=%d angle=%.2f deg"),
		*GetName(), *Part->GetName(), Handle == 0 ? TEXT("outside") : TEXT("inside"), Hand == 0 ? TEXT("L") : TEXT("R"), bWasLocked, Motion.AngleDeg);
	bNeedsPalmOpen[Hand] = true;
	ApplyState(Motion.AngleDeg, Unlocks, Motion.Metrics.Latches);
	return true;
}

void AVBoRHingedPanel::PreInitializeComponents()
{
	ConfigurePushCollision();
	SyncSettings();
	HandleVisualRestCm[0] = OutsideHandleMesh->GetRelativeLocation();
	HandleVisualRestCm[1] = InsideHandleMesh->GetRelativeLocation();
	ClosedHingeRotation = HingePivot->GetRelativeRotation().Quaternion();
	for (int32 I = 0; I < 2; ++I)
	{
		UIsdkGrabbableComponent* Grab = I == 0 ? OutsideGrab.Get() : InsideGrab.Get();
		UVBoRDoorGrabTransformer* Transformer = I == 0 ? OutsideTransformer.Get() : InsideTransformer.Get();
		Grab->SetGrabInputMethodAllowed(EIsdkGrabInputMethod::Palm, IsGrabAreaEnabled(I));
		Grab->SetGrabInputMethodAllowed(EIsdkGrabInputMethod::Pinch, false);
		Grab->SetGrabCollider(I == 0 ? OutsideHandle.Get() : InsideHandle.Get());
		Transformer->ConfigureDoor();
		Transformer->SetSingleGrabTransformerObject(DoorTransformer);
		Transformer->SetMultiGrabTransformerObject(DoorTransformer);
		Transformer->SetTransformTarget(HingePivot);
		Grab->SetGrabTransformer(Transformer);
	}
	Super::PreInitializeComponents();
}

void AVBoRHingedPanel::BeginPlay()
{
	Super::BeginPlay();
	UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, IsdkContentAssetPaths::Materials::GrabbableDefault);
	StateOverlay = Material ? UMaterialInstanceDynamic::Create(Material, this) : nullptr;
	OutsideGrab->GetInteractionPointerEventDelegate().AddUniqueDynamic(this, &AVBoRHingedPanel::HandleOutsidePointer);
	InsideGrab->GetInteractionPointerEventDelegate().AddUniqueDynamic(this, &AVBoRHingedPanel::HandleInsidePointer);
	const FSetupAssessment Setup = EvaluateSetup();
	bMeshPushReady = Setup.bMeshTraceable;
	SyncSettings();
	for (const FString& Problem : Setup.Problems) { UE_LOG(LogAxisDoor, Warning, TEXT("%s P2e setup: %s"), *GetName(), *Problem); }
	for (const FString& Notice : Setup.Notices) { UE_LOG(LogAxisDoor, Log, TEXT("%s: %s"), *GetName(), *Notice); }
	ApplyAxisHandle(0, Setup.Handles[0]); ApplyAxisHandle(1, Setup.Handles[1]);
	if (bUseLatch && !bUseOutsideGrabArea && !bUseInsideGrabArea && !OutsideAxisHandle && !InsideAxisHandle)
	{
		UE_LOG(LogAxisDoor, Warning, TEXT("%s: Latched panel has no opening handle or grab area. %s"), *GetName(),
			bAllowPalmOpening ? TEXT("버튼으로 Unlock을 부르면 무시.") : TEXT("버튼 Unlock 뒤에도 열 수 없음 (손바닥 열기 꺼짐)."));
	}
	ApplyState(Motion.AngleDeg, Motion.Metrics.Unlocks, Motion.Metrics.Latches);
}

FTransform AVBoRHingedPanel::HingeFrame() const
{
	FTransform Frame = HingePivot->GetComponentTransform();
	Frame.SetRotation(HingePivot->GetAttachParent()->GetComponentQuat() * ClosedHingeRotation);
	return Frame;
}

bool AVBoRHingedPanel::ReadHand(int32 Hand, FVBoRDoorHand& Input)
{
	Input = FVBoRDoorHand();
	if (!HandPawn) { HandPawn = Cast<AVBoRHandRigPawn>(UGameplayStatics::GetPlayerPawn(this, 0)); }
	const EOculusXRHandType Type = Hand == 0 ? EOculusXRHandType::HandLeft : EOculusXRHandType::HandRight;
	if (!HandPawn || !UOculusXRInputFunctionLibrary::IsHandPositionValid(Type) ||
		UOculusXRInputFunctionLibrary::GetTrackingConfidence(Type) != EOculusXRTrackingConfidence::High ||
		!HandPawn->ReadRawPalmAndTips(Hand, Input)) { Input.bTracked = false; return false; }
	const FTransform Frame = HingeFrame();
	for (FVector& Point : Input.Points) { Point = Frame.InverseTransformPosition(Point); }
	Input.PalmNormal = Frame.InverseTransformVectorNoScale(Input.PalmNormal).GetSafeNormal();
	return true;
}

bool AVBoRHingedPanel::ReadPalmClosed(int32 Hand, bool& bClosed) const
{
	if (!HandPawn) { return false; }
	return Hand == 0 ? HandPawn->LeftRig->ReadPalmGrab(bClosed) : HandPawn->RightRig->ReadPalmGrab(bClosed);
}

void AVBoRHingedPanel::HandleOutsidePointer(const FIsdkInteractionPointerEvent& Event) { HandlePointer(0, Event); }
void AVBoRHingedPanel::HandleInsidePointer(const FIsdkInteractionPointerEvent& Event) { HandlePointer(1, Event); }

void AVBoRHingedPanel::HandlePointer(int32 Handle, const FIsdkInteractionPointerEvent& Event)
{
	if (!IsGrabAreaEnabled(Handle)) { return; }
	UIsdkGrabberComponent* Grabber = Cast<UIsdkGrabberComponent>(Event.Interactor);
	EIsdkHandedness Handedness;
	if (!Grabber || !UIsdkFunctionLibrary::GetHandednessFromInteractor(Grabber, Handedness)) { return; }
	const int32 Hand = Handedness == EIsdkHandedness::Left ? 0 : 1;
	if (Event.Type == EIsdkPointerEventType::Hover)
	{
		if (!Hovering[Handle].Contains(Grabber)) { Hovering[Handle].Add(Grabber); ++Motion.Metrics.Attempts[Hand][Handle]; }
	}
	else if (Event.Type == EIsdkPointerEventType::Unhover) { Hovering[Handle].Remove(Grabber); }
	else if (Event.Type == EIsdkPointerEventType::Select)
	{
		if (bNeedsPalmOpen[Hand] || Motion.GrabHand == Hand) { return; }
		const int32 Unlocks = Motion.Metrics.Unlocks;
		FVBoRDoorHand Input;
		ReadHand(Hand, Input);
		// 선택 취소·걸쇠 뒤 닫힌 손이 재선택되지 않게 함 (P2 D4)
		bNeedsPalmOpen[Hand] = true;
		if (Motion.BeginGrab(Hand, Handle, Input)) { HoldingGrabber = Grabber; }
		ApplyState(Motion.AngleDeg, Unlocks, Motion.Metrics.Latches);
	}
	else if ((Event.Type == EIsdkPointerEventType::Unselect || Event.Type == EIsdkPointerEventType::Cancel) &&
		HoldingGrabber == Grabber && Motion.GrabHand == Hand && Motion.GrabHandle == Handle)
	{
		FVBoRDoorHand Input;
		if (Motion.ReleaseGrab(ReadHand(Hand, Input))) { HoldingGrabber = nullptr; UpdateHandleVisuals(); }
	}
}

void AVBoRHingedPanel::EndAxisHold(const TCHAR* Reason, bool bLatch)
{
	if (!HoldingAxisPart) { return; }
	UE_LOG(LogAxisDoor, Log, TEXT("%s axis hold end part=%s handle=%s hand=%s reason=%s angle=%.2f deg held=%.3f s"),
		*GetName(), *HoldingAxisPart->GetName(), AxisHoldHandle == 0 ? TEXT("outside") : TEXT("inside"),
		AxisHoldHand == 0 ? TEXT("L") : TEXT("R"), Reason, Motion.AngleDeg, Motion.Metrics.HeldSeconds + Motion.Metrics.PausedSeconds - AxisHoldStartSeconds);
	HoldingAxisPart->EndDoorHold(bLatch); HoldingAxisPart = nullptr;
	AxisHoldHand = AxisHoldHandle = INDEX_NONE;
}

void AVBoRHingedPanel::CancelSelections(const TCHAR* Reason)
{
	EndAxisHold(Reason, true);
	for (AVBoRAxisPart* Part : {OutsideAxisHandle.Get(), InsideAxisHandle.Get()})
	{
		if (Part) { Part->EndDoorHold(true); }
	}
	HoldingGrabber = nullptr;
	// Reset 도 이벤트에서 올 수 있어 SDK 취소는 Tick 에 예약함 (P2b R1-3)
	bCancelPending = true;
	Hovering[0].Reset(); Hovering[1].Reset();
	UpdateHandleVisuals();
}

void AVBoRHingedPanel::UpdateHandleVisuals()
{
	for (int32 I = 0; I < 2; ++I)
	{
		UStaticMeshComponent* Visual = I == 0 ? OutsideHandleMesh.Get() : InsideHandleMesh.Get();
		const bool bPressed = Motion.GrabHand != INDEX_NONE && Motion.GrabHandle == I && !HoldingAxisPart && IsGrabAreaEnabled(I);
		// 표시만 쉬는 자리 기준으로 옮김 (상자 이동·누적 오프셋으로 SDK 선택 흔들림 방지, P2d R1 D8)
		Visual->SetRelativeLocation(HandleVisualRestCm[I] + (bPressed ? HandleVisualPressOffsetCm : FVector::ZeroVector));
	}
}

void AVBoRHingedPanel::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bCancelPending)
	{
		bCancelPending = false;
		OutsideTransformer->ForceCancel(); InsideTransformer->ForceCancel();
	}
	SyncSettings();
	FVBoRDoorHand Hands[2];
	ReadHand(0, Hands[0]); ReadHand(1, Hands[1]);
	const double Before = Motion.AngleDeg;
	const int32 Unlocks = Motion.Metrics.Unlocks, Latches = Motion.Metrics.Latches;
	const bool bRecovering = Motion.bPaused;
	Motion.Advance(Hands, DeltaSeconds);
	if (HoldingAxisPart && bRecovering != Motion.bPaused)
	{
		// 추적 손실은 끝이 아닌 보류임 (잡기 로그와 구분, P2b D7·R1-4)
		UE_LOG(LogAxisDoor, Log, TEXT("%s axis hold %s part=%s handle=%s hand=%s reason=tracking loss pause angle=%.2f deg held=%.3f s"),
			*GetName(), Motion.bPaused ? TEXT("pause") : TEXT("resume"), *HoldingAxisPart->GetName(),
			AxisHoldHandle == 0 ? TEXT("outside") : TEXT("inside"), AxisHoldHand == 0 ? TEXT("L") : TEXT("R"),
			Motion.AngleDeg, Motion.Metrics.HeldSeconds + Motion.Metrics.PausedSeconds - AxisHoldStartSeconds);
	}
	for (int32 H = 0; H < 2; ++H)
	{
		bool bClosed = true;
		if (Hands[H].bTracked && ReadPalmClosed(H, bClosed) && !bClosed && !bRecovering && !Motion.bPaused)
		{
			bNeedsPalmOpen[H] = false;
			if (Motion.GrabHand == H && Motion.ReleaseGrab(true)) { HoldingGrabber = nullptr; EndAxisHold(TEXT("palm open"), false); }
		}
	}
	if (HoldingAxisPart && Motion.GrabHand == INDEX_NONE && Motion.Metrics.Latches == Latches)
	{
		EndAxisHold(TEXT("other"), false);
	}
	ApplyState(Before, Unlocks, Latches);
}

void AVBoRHingedPanel::ApplyState(double BeforeAngle, int32 BeforeUnlocks, int32 BeforeLatches)
{
	HingePivot->SetRelativeRotation(ClosedHingeRotation * FQuat(FVector::UpVector, FMath::DegreesToRadians(Motion.AngleDeg)));
	UpdateHandleVisuals();
	if (Motion.Metrics.Unlocks != BeforeUnlocks) { StateAudio->Play(); NotifyUnlocked(); }
	if (Motion.Metrics.Latches != BeforeLatches) { CancelSelections(TEXT("latch")); StateAudio->Play(); NotifyLocked(); }
	if (Motion.AngleDeg != BeforeAngle) { OnAngleChanged.Broadcast(static_cast<float>(Motion.AngleDeg)); }
	if (StateOverlay)
	{
		const FLinearColor Color = Motion.bPaused ? PausedColor : Motion.bLocked ? LockedColor :
			Motion.AngleDeg >= Motion.MaxAngleDeg ? MaximumColor : Motion.GrabHand != INDEX_NONE ? GrabbedColor :
			Motion.PushHand != INDEX_NONE ? PushingColor : UnlockedColor;
		StateOverlay->SetVectorParameterValue(TEXT("ColorParam"), Color);
		DoorMesh->SetOverlayMaterial(StateOverlay);
	}
}

void AVBoRHingedPanel::ResetDoor()
{
	const double Before = Motion.AngleDeg;
	Motion.Reset();
	for (int32 H = 0; H < 2; ++H)
	{
		bool bClosed = true;
		bNeedsPalmOpen[H] = !ReadPalmClosed(H, bClosed) || bClosed;
	}
	CancelSelections(TEXT("reset"));
	ApplyState(Before, Motion.Metrics.Unlocks, Motion.Metrics.Latches);
}

FString AVBoRHingedPanel::GetStateText() const
{
	return FString::Printf(TEXT("%s | %.2f deg | grab=%s push=%s/%s | paused=%d | maximum=%d"),
		Motion.bLocked ? TEXT("Locked") : TEXT("Unlocked"), Motion.AngleDeg,
		Motion.GrabHand == 0 ? TEXT("L") : Motion.GrabHand == 1 ? TEXT("R") : TEXT("none"),
		Motion.PushHand == 0 ? TEXT("L") : Motion.PushHand == 1 ? TEXT("R") : TEXT("none"),
		Motion.PushFace == 0 ? TEXT("outside") : Motion.PushFace == 1 ? TEXT("inside") : TEXT("none"), Motion.bPaused, Motion.AngleDeg >= MaxAngleDeg);
}

FString AVBoRHingedPanel::GetMeasurementText(const FVBoRDoorMetrics* Baseline) const
{
	const FVBoRDoorMetrics M = Baseline ? Motion.Metrics.Since(*Baseline) : Motion.Metrics;
	return FString::Printf(TEXT("Unlock=%d latch=%d last unlock=%s\nPush L outside/inside=%d/%d R=%d/%d | moved %.2f deg\n"
		"Ignored contacts: locked=%d palm direction=%d behind=%d near axis=%d outside=%d tracking Low=%d opening disabled=%d\n"
		"Handle attempts/grabs: L outside=%d/%d inside=%d/%d R outside=%d/%d inside=%d/%d | held %.3f s\n"
		"Rate capped frames=%d | grab pauses=%d %.3f s | recovery unapplied=%.2f cm | ignored SDK release=%d | push tracking losses=%d\n"
		"Palm-to-face angle L=%.1f R=%.1f deg (-1: no candidate) | palm axis -Z inferred; not-tested"),
		M.Unlocks, M.Latches, M.LastUnlockHandle == 0 ? TEXT("outside") : M.LastUnlockHandle == 1 ? TEXT("inside") : TEXT("none"),
		M.PushStarts[0][0], M.PushStarts[0][1], M.PushStarts[1][0], M.PushStarts[1][1], M.PushedDeg,
		M.Ignored[0], M.Ignored[1], M.Ignored[2], M.Ignored[3], M.Ignored[4], M.Ignored[5], M.Ignored[6],
		M.Attempts[0][0], M.Grabs[0][0], M.Attempts[0][1], M.Grabs[0][1], M.Attempts[1][0], M.Grabs[1][0], M.Attempts[1][1], M.Grabs[1][1],
		M.HeldSeconds, M.RateLimitedFrames, M.Pauses, M.PausedSeconds, M.RecoveryDistanceCm, M.IgnoredReleases, M.PushTrackingLosses, Motion.PalmAngleDeg[0], Motion.PalmAngleDeg[1]);
}

void AVBoRHingedPanel::EndPlay(const EEndPlayReason::Type Reason)
{
	EndAxisHold(TEXT("end play"), false);
	OutsideGrab->GetInteractionPointerEventDelegate().RemoveDynamic(this, &AVBoRHingedPanel::HandleOutsidePointer);
	InsideGrab->GetInteractionPointerEventDelegate().RemoveDynamic(this, &AVBoRHingedPanel::HandleInsidePointer);
	DoorMesh->SetOverlayMaterial(nullptr);
	Super::EndPlay(Reason);
}

void AVBoRHingedPanel::NotifyUnlocked() { OnPanelUnlocked.Broadcast(this); }
void AVBoRHingedPanel::NotifyLocked() { OnPanelLocked.Broadcast(this); }
