#include "P3VehicleMock.h"
#include "../Interaction/VBoREyePointComponent.h"
#include "../Interaction/VBoRViewpointComponent.h"
#include "../Interaction/VBoRParticipantZones.h"
#include "../Interaction/VBoRVehicleAlignmentComponent.h"
#include "../Interaction/VBoRHingedDoor.h"
#include "../Interaction/VBoRPokeButton.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "UObject/ConstructorHelpers.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Text/STextBlock.h"

AP3VehicleMock::AP3VehicleMock()
{
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
	Plate(11,FVector(50,0,85),FVector(25,BodySizeCm.Y-10,12));
	Plate(12,DriverEyeCm+FVector(55,0,-28),FVector(5,30,30));
	StartMarker->SetRelativeLocation(FVector(DriverEyeCm.X,-Y-StartOutsideCm,0.5)); StartMarker->SetRelativeScale3D(FVector(0.6,0.6,0.005));
	MockLabel->SetRelativeLocation(FVector(DriverEyeCm.X,-Y-StartOutsideCm+40,60)); MockLabel->SetRelativeRotation(FRotator(0,-90,0)); MockLabel->SetRelativeScale3D(FVector(0.08));
}

void AP3VehicleMock::BeginPlay()
{
	Super::BeginPlay();
	MockLabel->SetSlateWidget(SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Regular", 32)).ColorAndOpacity(FLinearColor::Yellow).Text(FText::FromString(TEXT("시작 위치 / 근사 형상·자리값"))));
	Door = GetWorld()->SpawnActorDeferred<AVBoRHingedDoor>(AVBoRHingedDoor::StaticClass(), FTransform(FRotator(0,180,0).Quaternion(), GetActorTransform().TransformPosition(DoorHingeCm)), this);
	const FTransform DoorWorld(FRotator(0,GetActorRotation().Yaw+180,0), GetActorTransform().TransformPosition(DoorHingeCm));
	Door->Tags.Add(TEXT("Car.DriverDoor")); Door->FinishSpawning(DoorWorld); Door->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);
	ReachButton = GetWorld()->SpawnActorDeferred<AVBoRPokeButton>(AVBoRPokeButton::StaticClass(), FTransform::Identity, this);
	ReachButton->Tags.Add(TEXT("Car.ReachButton")); ReachButton->HitAreaScale = 1;
	ReachButton->ButtonMesh->SetRelativeScale3D(FVector(0.04,0.04,0.01));
	ReachButton->FinishSpawning(FTransform(GetActorRotation(), GetActorTransform().TransformPosition(ReachButtonCm))); ReachButton->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);
}

void AP3VehicleMock::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (Door) { Door->Destroy(); }
	if (ReachButton) { ReachButton->Destroy(); }
	MockLabel->SetSlateWidget(nullptr);
	Super::EndPlay(EndPlayReason);
}
