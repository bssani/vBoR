#include "P3VehicleMock.h"
#include "../Interaction/VBoREyePointComponent.h"
#include "../Interaction/VBoRViewpointComponent.h"
#include "../Interaction/VBoRParticipantZones.h"
#include "../Interaction/VBoRVehicleAlignmentComponent.h"
#include "../Interaction/VBoRHingedDoor.h"
#include "../Interaction/VBoRAxisPart.h"
#include "../Interaction/VBoRPokeButton.h"
#include "../Interaction/VBoRKeyTable.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Components/WidgetComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
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
	StartMarker = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StartMarker")); StartMarker->SetupAttachment(RootComponent);
	StartMarker->SetStaticMesh(Cube.Object); StartMarker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MockLabel = CreateDefaultSubobject<UWidgetComponent>(TEXT("ApproximationLabel")); MockLabel->SetupAttachment(RootComponent);
	MockLabel->SetWidgetSpace(EWidgetSpace::World); MockLabel->SetDrawSize(FVector2D(650,100)); MockLabel->SetTwoSided(true); MockLabel->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AP3VehicleMock::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	DriverEye->SetRelativeLocation(DriverEyeCm); PassengerView->SetRelativeLocation(PassengerEyeCm); RearView->SetRelativeLocation(RearEyeCm);
	Approach->SetBoxExtent(FVector(BodySizeCm.X/2 + ApproachSurroundCm, BodySizeCm.Y/2 + ApproachSurroundCm, BodySizeCm.Z));
	// eye 아래 40 cm·위 25 cm 이므로 중심을 7.5 cm 내림 (P3 §2-4)
	Seat->SetRelativeLocation(DriverEyeCm - FVector(0,0,7.5)); Seat->SetBoxExtent(FVector(25,25,32.5));
	auto Plate = [this](int32 I, FVector P, FVector Size) { Plates[I]->SetRelativeLocation(P); Plates[I]->SetRelativeScale3D(Size/100); };
	const double X = BodySizeCm.X/2, Y = BodySizeCm.Y/2, Z = BodySizeCm.Z;
	Plate(0,FVector(0,0,1),FVector(BodySizeCm.X,BodySizeCm.Y,2));
	Plate(1,FVector(-40,0,Z),FVector(BodySizeCm.X-120,BodySizeCm.Y,3));
	Plate(2,FVector(X-50,0,65),FVector(100,BodySizeCm.Y,5));
	Plate(3,FVector(-X+10,0,55),FVector(20,BodySizeCm.Y,100));
	Plate(4,FVector(0,Y,25),FVector(BodySizeCm.X,4,50));
	Plate(5,FVector(35,Y,Z/2),FVector(5,4,Z));
	Plate(6,FVector((-X-75)/2,-Y,25),FVector(X-75,4,50));
	Plate(7,FVector(40,-Y,Z/2),FVector(5,4,Z));
	Plate(8,FVector(-80,-Y,Z/2),FVector(5,4,Z));
	Plate(9,DriverEyeCm+FVector(0,0,-70),FVector(48,46,10));
	Plate(10,DriverEyeCm+FVector(-28,0,-40),FVector(8,46,60));
	Plate(11,DashboardCenterCm,DashboardSizeCm);
	Plate(12,DriverEyeCm+FVector(55,0,-28),FVector(5,30,30));
	// 차 앞 +X 에서 -X 를 바라봄. 시작 머리 기록은 그대로 둠 (U-10, P4a D7)
	StartMarker->SetRelativeLocation(FVector(X+StartOutsideCm,0,0.5)); StartMarker->SetRelativeRotation(FRotator(0,180,0)); StartMarker->SetRelativeScale3D(FVector(0.6,0.6,0.005));
	MockLabel->SetRelativeLocation(FVector(X+StartOutsideCm-40,0,60)); MockLabel->SetRelativeRotation(FRotator::ZeroRotator); MockLabel->SetRelativeScale3D(FVector(0.08));
	PlaceReachButton();
}

void AP3VehicleMock::BeginPlay()
{
	Super::BeginPlay();
	MockLabel->SetSlateWidget(SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Regular", 32)).ColorAndOpacity(FLinearColor::Yellow).Text(FText::FromString(TEXT("Start / Approximate geometry"))));
	FrontKeyTable = CreateKeyTable(FrontKeyTableCm, TEXT("Car.FrontKey.Unlock"), TEXT("Car.FrontKey.Lock"));
	RearKeyTable = CreateKeyTable(RearKeyTableCm, TEXT("Car.RearKey.Unlock"), TEXT("Car.RearKey.Lock"));
	Door = GetWorld()->SpawnActorDeferred<AVBoRHingedDoor>(AVBoRHingedDoor::StaticClass(), FTransform(FRotator(0,180,0).Quaternion(), GetActorTransform().TransformPosition(DoorHingeCm)), this);
	const FTransform DoorWorld(FRotator(0,GetActorRotation().Yaw+180,0), GetActorTransform().TransformPosition(DoorHingeCm));
	Door->Tags.Add(TEXT("Car.DriverDoor")); Door->FinishSpawning(DoorWorld); Door->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);
	// Actor 원점은 차 원점, 메시·축만 부품 자리로 옮김 (차 원점 피벗 흉내, P2b D3)
	OutsideAxisHandle = GetWorld()->SpawnActorDeferred<AVBoRAxisPart>(AVBoRAxisPart::StaticClass(), GetActorTransform(), this);
	OutsideAxisHandle->AxisPivot->SetRelativeLocation(OutsideHandlePivotCm);
	OutsideAxisHandle->MeshRestFromRoot = FTransform(FQuat::Identity, OutsideHandlePivotCm - FVector(0,0,2), FVector(0.18,0.04,0.04));
	OutsideAxisHandle->Axis = EAxis::X; OutsideAxisHandle->bReverse = true;
	OutsideAxisHandle->FinishSpawning(GetActorTransform());
	OutsideAxisHandle->AttachToComponent(Door->HingePivot, FAttachmentTransformRules::KeepWorldTransform);
	Door->SetAxisHandle(0, OutsideAxisHandle);
	// 차 +Y 가 실내 쪽. 레버 아래를 당기면 +X 축으로 젖혀짐 (근사 자리값, P2d D5)
	InsideAxisHandle = GetWorld()->SpawnActorDeferred<AVBoRAxisPart>(AVBoRAxisPart::StaticClass(), GetActorTransform(), this);
	InsideAxisHandle->AxisPivot->SetRelativeLocation(InsideHandlePivotCm);
	InsideAxisHandle->MeshRestFromRoot = FTransform(FQuat::Identity, InsideHandlePivotCm - FVector(0,0,2), FVector(0.18,0.04,0.04));
	InsideAxisHandle->Axis = EAxis::X;
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
	ConsoleLid->FinishSpawning(GetActorTransform());
	ConsoleLid->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);
	ReachButton = GetWorld()->SpawnActorDeferred<AVBoRPokeButton>(AVBoRPokeButton::StaticClass(), FTransform::Identity, this);
	ReachButton->Tags.Add(TEXT("Car.ReachButton")); ReachButton->HitAreaScale = 1;
	ReachButton->ButtonMesh->SetRelativeScale3D(FVector(0.04,0.04,0.01));
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

AVBoRKeyTable* AP3VehicleMock::CreateKeyTable(FVector PositionCm, const TCHAR* UnlockTag, const TCHAR* LockTag)
{
	if (!KeyTableClass) { return nullptr; }
	const FTransform TableWorld = FTransform(PositionCm) * GetActorTransform();
	AVBoRKeyTable* Table = GetWorld()->SpawnActorDeferred<AVBoRKeyTable>(KeyTableClass, TableWorld, this);
	Table->FinishSpawning(TableWorld); Table->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);
	if (Table->Key)
	{
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
	if (Door) { Door->Destroy(); }
	if (OutsideAxisHandle) { OutsideAxisHandle->Destroy(); }
	if (InsideAxisHandle) { InsideAxisHandle->Destroy(); }
	if (ConsoleLid) { ConsoleLid->Destroy(); }
	if (ReachButton) { ReachButton->Destroy(); }
	if (FrontKeyTable) { FrontKeyTable->Destroy(); }
	if (RearKeyTable) { RearKeyTable->Destroy(); }
	MockLabel->SetSlateWidget(nullptr);
	Super::EndPlay(EndPlayReason);
}
