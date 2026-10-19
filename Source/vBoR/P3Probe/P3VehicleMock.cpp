#include "P3VehicleMock.h"
#include "../P1Probe/P1TrunkMock.h"
#include "../Interaction/VBoRPoweredPanel.h"
#include "../Interaction/VBoRFoldingSeat.h"
#include "../Interaction/VBoRLoadTarget.h"
#include "../Interaction/VBoRBoxStand.h"
#include "../Interaction/VBoRFloorRoute.h"
#include "Components/BoxComponent.h"
#include "Components/SplineComponent.h"
#include "../Interaction/VBoREyePointComponent.h"
#include "../Interaction/VBoRViewpointComponent.h"
#include "../Interaction/VBoRParticipantZones.h"
#include "../Interaction/VBoRVehicleAlignmentComponent.h"
#include "../Interaction/VBoRHingedDoor.h"
#include "../Interaction/VBoRAxisPart.h"
#include "../Interaction/VBoRPokeButton.h"
#include "../Interaction/VBoRKeyTable.h"
#include "../Interaction/VBoRSteeringWheel.h"
#include "../Interaction/VBoRTwoAxisPart.h"
#include "../Interaction/VBoRKnob.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Components/WidgetComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Math/RotationMatrix.h"
#include "UObject/ConstructorHelpers.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Text/STextBlock.h"
#include "IsdkContentAssetPaths.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"

AP3VehicleMock::AP3VehicleMock()
{
	KeyTableClass = AVBoRKeyTable::StaticClass();
	DashboardCenterCm = FVector(50,0,85); DashboardSizeCm = FVector(25,175,12);
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("FixedVehicleOrigin")));
	DriverEye = CreateDefaultSubobject<UVBoREyePointComponent>(TEXT("DriverEye")); DriverEye->SetupAttachment(RootComponent);
	PassengerView = CreateDefaultSubobject<UVBoRViewpointComponent>(TEXT("PassengerView")); PassengerView->SetupAttachment(RootComponent); PassengerView->ComponentTags.Add(TEXT("Car.PassengerView"));
	RearView = CreateDefaultSubobject<UVBoRViewpointComponent>(TEXT("RearView")); RearView->SetupAttachment(RootComponent); RearView->ComponentTags.Add(TEXT("Car.RearView"));
	Approach = CreateDefaultSubobject<UVBoRApproachZoneComponent>(TEXT("Approach")); Approach->SetupAttachment(RootComponent); Approach->ComponentTags.Add(TEXT("Car.Approach"));
	Seat = CreateDefaultSubobject<UVBoRSeatZoneComponent>(TEXT("DriverSeat")); Seat->SetupAttachment(RootComponent); Seat->ComponentTags.Add(TEXT("Car.DriverSeat"));
	Alignment = CreateDefaultSubobject<UVBoRVehicleAlignmentComponent>(TEXT("Alignment")); Alignment->BaseEye = DriverEye;
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	const TCHAR* Names[] = { TEXT("Floor"),TEXT("Roof"),TEXT("Front"),TEXT("Rear"),TEXT("RightSill"),TEXT("RightPillar"),TEXT("LeftRearSill"),TEXT("LeftFrontPillar"),TEXT("LeftRearPillar"),TEXT("SeatCushion"),TEXT("SeatBack"),TEXT("Dashboard"),TEXT("Steering") };
	for (const TCHAR* Name : Names)
	{
		UStaticMeshComponent* Part = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		Part->SetupAttachment(RootComponent); Part->SetStaticMesh(Cube.Object); Part->SetCollisionEnabled(ECollisionEnabled::NoCollision); Plates.Add(Part);
	}
	Footprint = CreateDefaultSubobject<UBoxComponent>(TEXT("CarFootprint")); Footprint->SetupAttachment(RootComponent);
	Footprint->ComponentTags.Add(TEXT("Car.Footprint")); Footprint->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Footprint->SetHiddenInGame(true);
	StartMarker = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StartMarker")); StartMarker->SetupAttachment(RootComponent);
	StartMarker->ComponentTags.Add(TEXT("Car.Start"));
	MirrorSupport = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ApproximateMirrorSupport")); MirrorSupport->SetupAttachment(RootComponent);
	MirrorSupport->SetStaticMesh(Cube.Object); MirrorSupport->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	KnobSupport = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ApproximateKnobConsoleSupport")); KnobSupport->SetupAttachment(RootComponent);
	KnobSupport->SetStaticMesh(Cube.Object); KnobSupport->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	StartMarker->SetStaticMesh(Cube.Object); StartMarker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MockLabel = CreateDefaultSubobject<UWidgetComponent>(TEXT("ApproximationLabel")); MockLabel->SetupAttachment(RootComponent);
	MockLabel->SetWidgetSpace(EWidgetSpace::World); MockLabel->SetDrawSize(FVector2D(650,100)); MockLabel->SetTwoSided(true); MockLabel->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AP3VehicleMock::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	Footprint->SetRelativeLocation(FVector(0,0,BodySizeCm.Z*0.5));
	Footprint->SetBoxExtent(FVector(BodySizeCm.X*0.5 + FootprintMarginCm, BodySizeCm.Y*0.5 + FootprintMarginCm, BodySizeCm.Z*0.5));
	// 받침은 차에 고정함 (노브 회전에 따라 콘솔이 움직이지 않게, P5f D8, cm)
	KnobSupport->SetRelativeLocation(KnobSupportCenterCm); KnobSupport->SetRelativeScale3D(KnobSupportSizeCm/100);
	// 받침은 차에 고정함 (거울 회전에 따라 지붕이 움직이지 않게, P5e D8, cm)
	const double MirrorRoofZ = BodySizeCm.Z-1.5;
	MirrorSupport->SetRelativeLocation(FVector(MirrorPivotCm.X,MirrorPivotCm.Y,(MirrorRoofZ+MirrorPivotCm.Z)/2));
	MirrorSupport->SetRelativeScale3D(FVector(1.5,1.5,FMath::Max(0.1,MirrorRoofZ-MirrorPivotCm.Z))/100);
	DriverEye->SetRelativeLocation(DriverEyeCm); PassengerView->SetRelativeLocation(PassengerEyeCm); RearView->SetRelativeLocation(RearEyeCm);
	Approach->SetBoxExtent(FVector(BodySizeCm.X/2 + ApproachSurroundCm, BodySizeCm.Y/2 + ApproachSurroundCm, BodySizeCm.Z));
	// eye 아래 40 cm·위 25 cm 이므로 중심을 7.5 cm 내림 (P3 §2-4)
	Seat->SetRelativeLocation(DriverEyeCm - FVector(0,0,7.5)); Seat->SetBoxExtent(FVector(25,25,32.5));
	auto Plate = [this](int32 I, FVector P, FVector Size) { Plates[I]->SetRelativeLocation(P); Plates[I]->SetRelativeScale3D(Size/100); };
	const double X = BodySizeCm.X/2, Y = BodySizeCm.Y/2, Z = BodySizeCm.Z;
	Plate(0,FVector(0,0,1),FVector(BodySizeCm.X,BodySizeCm.Y,2));
	Plate(1,FVector(-40,0,Z),FVector(BodySizeCm.X-120,BodySizeCm.Y,3));
	Plate(2,FVector(X-50,0,65),FVector(100,BodySizeCm.Y,5));
	// 개구부 바닥 아래만 남김 (트렁크 안을 뒤 벽 판이 가리지 않게, P4d D2, cm)
	const float TrunkFloor = GetDefault<AP1TrunkMock>()->FloorHeightCm;
	Plate(3,FVector(-X+10,0,TrunkFloor/2),FVector(20,BodySizeCm.Y,TrunkFloor));
	Plate(4,FVector(0,Y,25),FVector(BodySizeCm.X,4,50));
	Plate(5,FVector(35,Y,Z/2),FVector(5,4,Z));
	Plate(6,FVector((-X-75)/2,-Y,25),FVector(X-75,4,50));
	Plate(7,FVector(40,-Y,Z/2),FVector(5,4,Z));
	Plate(8,FVector(-80,-Y,Z/2),FVector(5,4,Z));
	Plate(9,DriverEyeCm+FVector(0,0,-70),FVector(48,46,10));
	Plate(10,DriverEyeCm+FVector(-28,0,-40),FVector(8,46,60));
	Plate(11,DashboardCenterCm,DashboardSizeCm);
	Plate(12,DriverEyeCm+FVector(55,0,-28),FVector(5,30,30));
	// 차 앞 +X 에서 -X 를 바라봄. 이 표시 위에서 시작 머리 기록함 (U-10, P3b D9)
	StartMarker->SetRelativeLocation(FVector(X+StartOutsideCm,0,0.5)); StartMarker->SetRelativeRotation(FRotator(0,180,0)); StartMarker->SetRelativeScale3D(FVector(0.6,0.6,0.005));
	MockLabel->SetRelativeLocation(FVector(X+StartOutsideCm-40,0,60)); MockLabel->SetRelativeRotation(FRotator::ZeroRotator); MockLabel->SetRelativeScale3D(FVector(0.08));
	PlaceReachButton();
}

void AP3VehicleMock::BeginPlay()
{
	Super::BeginPlay();
	MockLabel->SetSlateWidget(SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Regular", 32)).ColorAndOpacity(FLinearColor::Yellow).Text(FText::FromString(TEXT("시작 위치 / 근사 형상·자리값"))));
	FrontKeyTable = CreateKeyTable(FrontKeyTableCm, TEXT("Car.FrontKey"), TEXT("Car.FrontKey.Unlock"), TEXT("Car.FrontKey.Lock"));
	RearKeyTable = CreateKeyTable(RearKeyTableCm, TEXT("Car.RearKey"), TEXT("Car.RearKey.Unlock"), TEXT("Car.RearKey.Lock"));
	CreateRearSection();
	// 운전석과 같은 eye 기준 높이. 차 안쪽 폭 한 판으로 둠 (P4e D2, 근사 cm)
	RearCushion = NewObject<UStaticMeshComponent>(this, TEXT("ApproximateRearCushion"));
	RearCushion->SetupAttachment(RootComponent);
	RearCushion->SetStaticMesh(Plates[9]->GetStaticMesh());
	RearCushion->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RearCushion->SetRelativeLocation(FVector(RearEyeCm.X,0,RearEyeCm.Z-70));
	RearCushion->SetRelativeScale3D(RearCushionSizeCm/100);
	RearCushion->RegisterComponent();
	RearSeat = GetWorld()->SpawnActorDeferred<AVBoRFoldingSeat>(AVBoRFoldingSeat::StaticClass(), GetActorTransform(), this);
	RearSeat->Tags.Add(TEXT("Car.RearSeat"));
	RearSeat->HingeLocationCm = FVector(RearEyeCm.X-28,0,RearEyeCm.Z-40-RearBackSizeCm.Z/2);
	RearSeat->MeshRestFromRoot = FTransform(FQuat::Identity, FVector(RearEyeCm.X-28,0,RearEyeCm.Z-40), RearBackSizeCm/100);
	RearSeat->FinishSpawning(GetActorTransform()); RearSeat->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);
	AroundCarRoute = GetWorld()->SpawnActorDeferred<AVBoRFloorRoute>(AVBoRFloorRoute::StaticClass(), GetActorTransform(), this);
	AroundCarRoute->Tags.Add(TEXT("Route.AroundCar"));
	AroundCarRoute->Spline->SetSplinePoints(AroundCarRoutePointsCm, ESplineCoordinateSpace::Local);
	AroundCarRoute->FinishSpawning(GetActorTransform()); AroundCarRoute->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);
	DriverDoorRoute = GetWorld()->SpawnActorDeferred<AVBoRFloorRoute>(AVBoRFloorRoute::StaticClass(), GetActorTransform(), this);
	DriverDoorRoute->Tags.Add(TEXT("Route.ToDriverDoor"));
	DriverDoorRoute->Spline->SetSplinePoints(DriverDoorRoutePointsCm, ESplineCoordinateSpace::Local);
	DriverDoorRoute->FinishSpawning(GetActorTransform()); DriverDoorRoute->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);
	Door = GetWorld()->SpawnActorDeferred<AVBoRHingedDoor>(AVBoRHingedDoor::StaticClass(), FTransform(FRotator(0,180,0).Quaternion(), GetActorTransform().TransformPosition(DoorHingeCm)), this);
	const FTransform DoorWorld(FRotator(0,GetActorRotation().Yaw+180,0), GetActorTransform().TransformPosition(DoorHingeCm));
	Door->Tags.Add(TEXT("Car.DriverDoor")); Door->SpectatorName = FText::FromString(TEXT("Driver Door"));
	Door->FinishSpawning(DoorWorld); Door->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);
	// Actor 원점은 차 원점, 메시·축만 부품 자리로 옮김 (차 원점 피벗 흉내, P2b D3)
	OutsideAxisHandle = GetWorld()->SpawnActorDeferred<AVBoRAxisPart>(AVBoRAxisPart::StaticClass(), GetActorTransform(), this);
	OutsideAxisHandle->AxisPivot->SetRelativeLocation(OutsideHandlePivotCm);
	OutsideAxisHandle->MeshRestFromRoot = FTransform(FQuat::Identity, OutsideHandlePivotCm - FVector(0,0,2), FVector(0.18,0.04,0.04));
	OutsideAxisHandle->Axis = EAxis::X; OutsideAxisHandle->bReverse = true;
	OutsideAxisHandle->SpectatorName = FText::FromString(TEXT("Driver Door"));
	OutsideAxisHandle->FinishSpawning(GetActorTransform());
	OutsideAxisHandle->AttachToComponent(Door->HingePivot, FAttachmentTransformRules::KeepWorldTransform);
	Door->SetAxisHandle(0, OutsideAxisHandle);
	// 차 +Y 가 실내 쪽. 레버 아래를 당기면 +X 축으로 젖혀짐 (근사 자리값, P2d D5)
	InsideAxisHandle = GetWorld()->SpawnActorDeferred<AVBoRAxisPart>(AVBoRAxisPart::StaticClass(), GetActorTransform(), this);
	InsideAxisHandle->AxisPivot->SetRelativeLocation(InsideHandlePivotCm);
	InsideAxisHandle->MeshRestFromRoot = FTransform(FQuat::Identity, InsideHandlePivotCm - FVector(0,0,2), FVector(0.18,0.04,0.04));
	InsideAxisHandle->Axis = EAxis::X;
	InsideAxisHandle->SpectatorName = FText::FromString(TEXT("Driver Door"));
	InsideAxisHandle->FinishSpawning(GetActorTransform());
	InsideAxisHandle->AttachToComponent(Door->HingePivot, FAttachmentTransformRules::KeepWorldTransform);
	Door->SetAxisHandle(1, InsideAxisHandle);
	// +Y 윗면·+Z 차 -Y 축. 양의 열림이 위로 향함 (가로 경첩, P2c D3)
	ConsoleLid = GetWorld()->SpawnActorDeferred<AVBoRHingedPanel>(AVBoRHingedPanel::StaticClass(), GetActorTransform(), this);
	ConsoleLid->HingePivot->SetRelativeTransform(FTransform(FQuat(FVector::XAxisVector, PI / 2), ConsoleLidPivotCm));
	ConsoleLid->MeshRestFromRoot = FTransform(FQuat::Identity, ConsoleLidPivotCm + FVector(17.5,0,0), FVector(0.35,0.24,0.03));
	// 앞면 가운데를 잡음. 표시는 차 -X 로 0.75 cm 들어감 (근사 자리값, P2d R1 D8)
	ConsoleLid->OutsideHandle->SetRelativeLocation(FVector(36,0,0));
	ConsoleLid->OutsideHandle->SetBoxExtent(FVector(4,4,8));
	ConsoleLid->OutsideHandleMesh->SetRelativeScale3D(FVector(0.02,0.03,0.14));
	ConsoleLid->HandleVisualPressOffsetCm = FVector(-0.75,0,0);
	ConsoleLid->InsideHandle->SetRelativeLocation(FVector(35,-5.5,0));
	ConsoleLid->MaxAngleDeg = 100;
	ConsoleLid->bUseLatch = true;
	ConsoleLid->bAllowPalmOpening = false;
	ConsoleLid->bUseInsideGrabArea = false;
	ConsoleLid->SpectatorName = FText::FromString(TEXT("Console Box"));
	ConsoleLid->FinishSpawning(GetActorTransform());
	ConsoleLid->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);
	// +X 차 +Z·+Y 차 -X·+Z 차 -Y. 아래 경첩에서 앉은 쪽으로 내려옴 (근사 cm, P5a D2·D6)
	Glovebox = GetWorld()->SpawnActorDeferred<AVBoRHingedPanel>(AVBoRHingedPanel::StaticClass(), GetActorTransform(), this);
	Glovebox->HingePivot->SetRelativeTransform(FTransform(FRotationMatrix::MakeFromXZ(FVector::ZAxisVector, -FVector::YAxisVector).ToQuat(), GloveboxHingeCm));
	Glovebox->MeshRestFromRoot = FTransform(FQuat::Identity, GloveboxHingeCm + FVector(0,0,GloveboxSizeCm.Z/2), GloveboxSizeCm/100);
	Glovebox->bUseLatch = true; Glovebox->bAllowPalmOpening = false; Glovebox->bUseInsideGrabArea = false;
	Glovebox->MaxAngleDeg = GloveboxMaxAngleDeg; Glovebox->MinAxisDistanceCm = GloveboxMinAxisDistanceCm;
	// 윗변 +Y 축. 아랫변을 -X·위로 당김 (손잡이만 복귀, 근사 cm, P5a D3·D4)
	GloveboxHandle = GetWorld()->SpawnActorDeferred<AVBoRAxisPart>(AVBoRAxisPart::StaticClass(), GetActorTransform(), this);
	GloveboxHandle->AxisPivot->SetRelativeLocation(GloveboxHandlePivotCm);
	GloveboxHandle->MeshRestFromRoot = FTransform(FQuat::Identity, GloveboxHandlePivotCm - FVector(0,0,GloveboxHandleSizeCm.Z/2), GloveboxHandleSizeCm/100);
	GloveboxHandle->Axis = EAxis::Y; GloveboxHandle->MaxAngleDeg = GloveboxHandleMaxAngleDeg;
	GloveboxHandle->Movement = EVBoRAxisMovement::Rotate; GloveboxHandle->Release = EVBoRAxisRelease::Return;
	GloveboxHandle->SpectatorName = FText::FromString(TEXT("Glovebox"));
	GloveboxHandle->FinishSpawning(GetActorTransform());
	Glovebox->OutsideAxisHandle = GloveboxHandle;
	Glovebox->SpectatorName = FText::FromString(TEXT("Glovebox"));
	Glovebox->FinishSpawning(GetActorTransform());
	Glovebox->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);
	GloveboxHandle->AttachToComponent(Glovebox->HingePivot, FAttachmentTransformRules::KeepWorldTransform);
	// 차 원점 메시·축 자리값. +X 회전으로 끝이 위로 감 (근사 cm·deg, P5b D9)
	GearLever = GetWorld()->SpawnActorDeferred<AVBoRAxisPart>(AVBoRAxisPart::StaticClass(), GetActorTransform(), this);
	GearLever->AxisPivot->SetRelativeTransform(FTransform(GearAxisRotation, GearPivotCm));
	GearLever->MeshRestFromRoot = FTransform(GearAxisRotation.Quaternion(), GearPivotCm + GearAxisRotation.RotateVector(FVector(0,GearSizeCm.Y/2,0)), GearSizeCm/100);
	GearLever->Axis = EAxis::X; GearLever->Release = EVBoRAxisRelease::Detent;
	GearLever->DetentAnglesDeg = GearDetentAnglesDeg; GearLever->DetentStartIndex = 1;
	GearLever->DetentUpThresholdDeg = GearUpThresholdDeg; GearLever->DetentDownThresholdDeg = GearDownThresholdDeg;
	GearLever->bAllowPalmGrab = false; GearLever->bAllowPinchGrab = true; GearLever->bAllowPush = true;
	GearLever->PushMarginCm = LeverPushMarginCm;
	GearLever->SpectatorName = FText::FromString(TEXT("Gear Lever"));
	GearLever->FinishSpawning(GetActorTransform()); GearLever->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);
	// 버튼 +Z 면을 레버 끝 +Y 로 놓음. 잡기·단계와 연결하지 않음 (P5b D8)
	const FTransform ButtonFromPivot(FRotationMatrix::MakeFromZ(FVector::YAxisVector).ToQuat(), FVector(0,GearSizeCm.Y,0));
	const FTransform ButtonWorld = ButtonFromPivot * GearLever->AxisPivot->GetComponentTransform();
	GearButton = GetWorld()->SpawnActorDeferred<AVBoRPokeButton>(AVBoRPokeButton::StaticClass(), ButtonWorld, this);
	GearButton->ButtonMesh->SetRelativeScale3D(GearButtonSizeCm/100); GearButton->HitAreaScale = 1;
	GearButton->SpectatorName = FText::FromString(TEXT("Gear Button"));
	GearButton->FinishSpawning(ButtonWorld); GearButton->AttachToComponent(GearLever->AxisPivot, FAttachmentTransformRules::KeepWorldTransform);
	// 왼쪽 -Y 레버는 회전 부호를 뒤집음 (값 + 일 때 끝이 위로, 근사 cm·deg, P5c D7)
	TurnSignalLever = GetWorld()->SpawnActorDeferred<AVBoRAxisPart>(AVBoRAxisPart::StaticClass(), GetActorTransform(), this);
	TurnSignalLever->AxisPivot->SetRelativeTransform(FTransform(TurnSignalAxisRotation, TurnSignalPivotCm));
	TurnSignalLever->MeshRestFromRoot = FTransform(TurnSignalAxisRotation.Quaternion(), TurnSignalPivotCm + TurnSignalAxisRotation.RotateVector(FVector(0,-TurnSignalSizeCm.Y/2,0)), TurnSignalSizeCm/100);
	TurnSignalLever->Axis = EAxis::X; TurnSignalLever->bReverse = true; TurnSignalLever->Release = EVBoRAxisRelease::Detent;
	TurnSignalLever->DetentAnglesDeg = TurnSignalDetentAnglesDeg; TurnSignalLever->DetentStartIndex = 1;
	TurnSignalLever->DetentUpThresholdDeg = TurnSignalUpThresholdDeg; TurnSignalLever->DetentDownThresholdDeg = TurnSignalDownThresholdDeg;
	TurnSignalLever->DetentNudgeDeg = TurnSignalNudgeDeg;
	TurnSignalLever->bAllowPalmGrab = false; TurnSignalLever->bAllowPinchGrab = true; TurnSignalLever->bAllowPush = true;
	TurnSignalLever->PushMarginCm = LeverPushMarginCm;
	TurnSignalLever->SpectatorName = FText::FromString(TEXT("Turn Signal Lever"));
	TurnSignalLever->FinishSpawning(GetActorTransform()); TurnSignalLever->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);
	// 판은 에디터·기존 시험 기준으로 남김 (차 원점 피벗, 근사 cm·deg, P5d D8)
	SteeringWheel = GetWorld()->SpawnActorDeferred<AVBoRSteeringWheel>(AVBoRSteeringWheel::StaticClass(),GetActorTransform(),this);
	if (SteeringWheel)
	{
		SteeringWheel->AxisPivot->SetRelativeTransform(FTransform(SteeringAxisRotation,SteeringHubCm));
		SteeringWheel->MeshRestFromRoot = FTransform(SteeringAxisRotation.Quaternion()*FRotator(90,0,0).Quaternion(),SteeringHubCm,FVector(SteeringDiameterCm,SteeringDiameterCm,SteeringThicknessCm)/100);
		SteeringWheel->MarkerRestFromPivot.SetLocation(FVector(-SteeringThicknessCm/2-0.1,0,SteeringDiameterCm/2-1.5));
		SteeringWheel->LimitAngleDeg = SteeringLimitDeg;
		SteeringWheel->MinGrabRadiusCm = SteeringMinGrabRadiusCm; SteeringWheel->MaxGrabRadiusCm = SteeringMaxGrabRadiusCm;
		SteeringWheel->SpectatorName = FText::FromString(TEXT("Steering Wheel"));
		SteeringWheel->FinishSpawning(GetActorTransform()); SteeringWheel->AttachToActor(this,FAttachmentTransformRules::KeepWorldTransform);
		Plates[12]->SetHiddenInGame(true);
	}
	RearviewMirror = GetWorld()->SpawnActorDeferred<AVBoRTwoAxisPart>(AVBoRTwoAxisPart::StaticClass(),GetActorTransform(),this);
	if (RearviewMirror)
	{
		RearviewMirror->Pivot->SetRelativeLocation(MirrorPivotCm);
		RearviewMirror->MeshRestFromRoot = FTransform(FQuat::Identity,MirrorCenterCm,MirrorSizeCm/100);
		RearviewMirror->SpectatorName = FText::FromString(TEXT("Rearview Mirror"));
		RearviewMirror->FinishSpawning(GetActorTransform()); RearviewMirror->AttachToActor(this,FAttachmentTransformRules::KeepWorldTransform);
		// 근사 색은 근사 차에서만 칠함 (Blueprint 자식의 실제 재질 보존, P5l D4)
		if (auto* Material = LoadObject<UMaterialInterface>(nullptr,IsdkContentAssetPaths::Materials::GrabbableDefault))
		{
			auto* FaceMaterial = UMaterialInstanceDynamic::Create(Material,RearviewMirror);
			FaceMaterial->SetVectorParameterValue(TEXT("ColorParam"),FLinearColor(0.15,0.3,0.4,1));
			RearviewMirror->PartMesh->SetMaterial(0,FaceMaterial);
		}
	}
	// Actor 원점은 차 원점, +Z 가 노브 윗면임 (근사 지름·높이 cm, P5f D8·U-09)
	ConsoleKnob = GetWorld()->SpawnActorDeferred<AVBoRKnob>(AVBoRKnob::StaticClass(),GetActorTransform(),this);
	if (ConsoleKnob)
	{
		ConsoleKnob->Pivot->SetRelativeLocation(KnobPivotCm);
		ConsoleKnob->MeshRestFromRoot = FTransform(FQuat::Identity,KnobPivotCm+FVector(0,0,KnobHeightCm/2),FVector(KnobDiameterCm,KnobDiameterCm,KnobHeightCm)/100);
		ConsoleKnob->MarkerRestFromPivot.SetLocation(FVector(KnobDiameterCm/2-0.8,0,KnobHeightCm+0.15));
		ConsoleKnob->SpectatorName = FText::FromString(TEXT("Console Knob"));
		ConsoleKnob->TwistGain = KnobTwistGain;
		ConsoleKnob->FinishSpawning(GetActorTransform()); ConsoleKnob->AttachToActor(this,FAttachmentTransformRules::KeepWorldTransform);
	}
	ReachButton = GetWorld()->SpawnActorDeferred<AVBoRPokeButton>(AVBoRPokeButton::StaticClass(), FTransform::Identity, this);
	ReachButton->Tags.Add(TEXT("Car.ReachButton")); ReachButton->HitAreaScale = 1;
	ReachButton->ButtonMesh->SetRelativeScale3D(FVector(0.04,0.04,0.01));
	ReachButton->SpectatorName = FText::FromString(TEXT("Button"));
	ReachButton->FinishSpawning(FTransform(GetActorRotation(), GetActorTransform().TransformPosition(ReachButtonCm))); ReachButton->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);
	PlaceReachButton();
	UMaterialInterface* ColorMaterial = LoadObject<UMaterialInterface>(nullptr, IsdkContentAssetPaths::Materials::GrabbableDefault);
	if (ColorMaterial)
	{
		UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(ColorMaterial, ReachButton);
		Material->SetVectorParameterValue(TEXT("ColorParam"), FLinearColor(0.05, 0.25, 1, 1));
		ReachButton->ButtonMesh->SetMaterial(0, Material);
	}
}

AVBoRKeyTable* AP3VehicleMock::CreateKeyTable(FVector PositionCm, const TCHAR* KeyTag, const TCHAR* UnlockTag, const TCHAR* LockTag)
{
	if (!KeyTableClass) { return nullptr; }
	const FTransform TableWorld = FTransform(PositionCm) * GetActorTransform();
	AVBoRKeyTable* Table = GetWorld()->SpawnActorDeferred<AVBoRKeyTable>(KeyTableClass, TableWorld, this);
	Table->FinishSpawning(TableWorld); Table->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);
	if (Table->Key)
	{
		Table->Key->Tags.Add(KeyTag);
		Table->Key->SpectatorName = FText::FromString(TEXT("Key"));
		Table->Key->UnlockButton->Tags.Add(UnlockTag); Table->Key->LockButton->Tags.Add(LockTag);
		Table->Key->OnUnlockPressed.AddDynamic(this, &AP3VehicleMock::KeyUnlockPressed);
		Table->Key->OnLockPressed.AddDynamic(this, &AP3VehicleMock::KeyLockPressed);
	}
	return Table;
}

void AP3VehicleMock::KeyUnlockPressed(AVBoRKeyFob* Key) { OnKeyUnlockPressed.Broadcast(Key); }
void AP3VehicleMock::KeyLockPressed(AVBoRKeyFob* Key) { OnKeyLockPressed.Broadcast(Key); }

void AP3VehicleMock::PlaceReachButton()
{
	if (!ReachButton || !ReachButton->ButtonMesh->GetStaticMesh()) { return; }
	// 윗면·크기를 판에서 읽음. Z 자리값은 쓰지 않음 (대시보드에 묻힘 방지, R1-6)
	const FBox Dashboard = Plates[11]->GetStaticMesh()->GetBoundingBox().TransformBy(Plates[11]->GetRelativeTransform());
	const FBox Button = ReachButton->ButtonMesh->GetStaticMesh()->GetBoundingBox().TransformBy(
		ReachButton->ButtonMesh->GetComponentTransform().GetRelativeTransform(ReachButton->GetActorTransform()));
	const FVector Position(FMath::Clamp(ReachButtonCm.X, Dashboard.Min.X - Button.Min.X, Dashboard.Max.X - Button.Max.X),
		FMath::Clamp(ReachButtonCm.Y, Dashboard.Min.Y - Button.Min.Y, Dashboard.Max.Y - Button.Max.Y), Dashboard.Max.Z - Button.Min.Z);
	ReachButton->SetActorRelativeLocation(Position);
}

void AP3VehicleMock::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (RearSeat) { RearSeat->Destroy(); }
	if (RearCushion) { RearCushion->DestroyComponent(); }
	if (BoxStand) { BoxStand->Destroy(); }
	if (Liftgate) { Liftgate->Destroy(); }
	if (LoadTarget) { LoadTarget->Destroy(); }
	if (Trunk) { Trunk->Destroy(); }
	if (AroundCarRoute) { AroundCarRoute->Destroy(); }
	if (DriverDoorRoute) { DriverDoorRoute->Destroy(); }
	if (Door) { Door->Destroy(); }
	if (OutsideAxisHandle) { OutsideAxisHandle->Destroy(); }
	if (InsideAxisHandle) { InsideAxisHandle->Destroy(); }
	if (ConsoleLid) { ConsoleLid->Destroy(); }
	if (GloveboxHandle) { GloveboxHandle->Destroy(); }
	if (Glovebox) { Glovebox->Destroy(); }
	if (GearButton) { GearButton->Destroy(); }
	if (GearLever) { GearLever->Destroy(); }
	if (TurnSignalLever) { TurnSignalLever->Destroy(); }
	if (SteeringWheel) { SteeringWheel->Destroy(); }
	if (RearviewMirror) { RearviewMirror->Destroy(); }
	if (ConsoleKnob) { ConsoleKnob->Destroy(); }
	if (MirrorSupport) { MirrorSupport->DestroyComponent(); }
	if (ReachButton) { ReachButton->Destroy(); }
	if (FrontKeyTable) { FrontKeyTable->Destroy(); }
	if (RearKeyTable) { RearKeyTable->Destroy(); }
	MockLabel->SetSlateWidget(nullptr);
	Super::EndPlay(EndPlayReason);
}

void AP3VehicleMock::CreateRearSection()
{
	const FTransform RearPose(GetActorQuat(), GetActorTransform().TransformPosition(FVector(-BodySizeCm.X/2,0,0)));
	Trunk = GetWorld()->SpawnActor<AP1TrunkMock>(AP1TrunkMock::StaticClass(), RearPose);
	Trunk->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);
	Liftgate = GetWorld()->SpawnActorDeferred<AVBoRPoweredPanel>(AVBoRPoweredPanel::StaticClass(), RearPose, this);
	Liftgate->Tags.Add(TEXT("Car.Liftgate"));
	// 개구부 +X 가 차 안쪽. P1 시험대와 같은 축·버튼 자리값 (P4d D2, cm)
	Liftgate->HingeLocationCm = FVector(0,0,Trunk->FloorHeightCm+Trunk->InsideHeightCm);
	Liftgate->MeshRestFromRoot = FTransform(FQuat::Identity, FVector(-Trunk->PanelThicknessCm*0.5,0,Trunk->FloorHeightCm+Trunk->InsideHeightCm*0.5), FVector(Trunk->PanelThicknessCm,Trunk->InsideWidthCm,Trunk->InsideHeightCm)/100);
	Liftgate->OpenButtonFromHinge.SetLocation(FVector(-Trunk->PanelThicknessCm-1,20,-Trunk->InsideHeightCm+10));
	Liftgate->CloseButtonFromHinge.SetLocation(FVector(-Trunk->PanelThicknessCm-1,-20,-Trunk->InsideHeightCm+10));
	Liftgate->FinishSpawning(RearPose); Liftgate->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);
	const FVector Size(Trunk->InsideDepthCm,Trunk->InsideWidthCm,Trunk->InsideHeightCm);
	const FTransform Goal(GetActorQuat(), RearPose.TransformPosition(FVector(Size.X/2,0,Trunk->FloorHeightCm+15)));
	LoadTarget = GetWorld()->SpawnActorDeferred<AVBoRLoadTarget>(AVBoRLoadTarget::StaticClass(), Goal, this);
	LoadTarget->bShowGuide = false; LoadTarget->GoalSizeCm = FVector(45,35,30);
	LoadTarget->FinishSpawning(Goal); LoadTarget->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);
	LoadTarget->LoadingRegion->SetBoxExtent(Size/2);
	LoadTarget->LoadingRegion->SetWorldTransform(FTransform(GetActorQuat(), RearPose.TransformPosition(FVector(Size.X/2,0,Trunk->FloorHeightCm+Size.Z/2))));
	const FTransform StandPose(GetActorQuat(), GetActorTransform().TransformPosition(RearBoxStandCm));
	BoxStand = GetWorld()->SpawnActorDeferred<AVBoRBoxStand>(AVBoRBoxStand::StaticClass(), StandPose, this);
	BoxStand->Tags.Add(TEXT("Car.BoxStand")); BoxStand->LoadTarget = LoadTarget;
	BoxStand->FinishSpawning(StandPose); BoxStand->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);
}
