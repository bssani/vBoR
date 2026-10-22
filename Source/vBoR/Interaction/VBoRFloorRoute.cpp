#include "VBoRFloorRoute.h"
#include "Components/SplineComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "IsdkContentAssetPaths.h"
#include "UObject/ConstructorHelpers.h"

AVBoRFloorRoute::AVBoRFloorRoute()
{
	Spline = CreateDefaultSubobject<USplineComponent>(TEXT("RouteSpline")); SetRootComponent(Spline);
	PrimaryActorTick.bCanEverTick = true;
	SetActorHiddenInGame(true);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Mesh(TEXT("/Engine/BasicShapes/Cube.Cube")); Cube = Mesh.Object;
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Material(IsdkContentAssetPaths::Materials::GrabbableDefault); ColorMaterial = Material.Object;
	ArrowColor = FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("0fa7c4")));
	RingColor = FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("e7a91f")));
}

void AVBoRFloorRoute::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform); BuildVisuals();
}

void AVBoRFloorRoute::BeginPlay()
{
	Super::BeginPlay(); BuildVisuals(); SetRouteVisible(false);
	UE_LOG(LogTemp, Log, TEXT("Floor route %s — Approximate geometry, placeholder values (P3b D4, cm)"), *GetName());
}

void AVBoRFloorRoute::AddStripe(const FVector& A, const FVector& B, float Width, UMaterialInstanceDynamic* Material)
{
	UStaticMeshComponent* Part = NewObject<UStaticMeshComponent>(this);
	Part->SetupAttachment(Spline); Part->SetStaticMesh(Cube); Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Part->SetCastShadow(false); Part->SetMaterial(0, Material); Part->RegisterComponent();
	Part->SetRelativeLocation((A + B) * 0.5);
	Part->SetRelativeRotation((B - A).Rotation());
	Part->SetRelativeScale3D(FVector((B - A).Size() / 100, Width / 100, 0.002));
	Pieces.Add(Part);
}

void AVBoRFloorRoute::BuildVisuals()
{
	for (auto& Part : Pieces) { if (Part) { Part->DestroyComponent(); } }
	Pieces.Reset(); ArrowMaterials.Reset();
	if (Spline->GetNumberOfSplinePoints() < 2) { return; }
	const float Length = Spline->GetSplineLength();
	for (float Distance = FMath::Max(0.f, FirstArrowCm); Distance <= Length - EndClearanceCm; Distance += FMath::Max(1.f, ArrowSpacingCm))
	{
		UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(ColorMaterial, this); ArrowMaterials.Add(Material);
		const FVector P = Spline->GetLocationAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::Local) + FVector(0, 0, FloorOffsetCm);
		const FRotator Facing(0, Spline->GetDirectionAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::Local).Rotation().Yaw, 0);
		// Include strip end caps in outer width/length (P3b E2, cm)
		double Angle = FMath::Atan2(ArrowWidthCm * 0.5, ArrowLengthCm), Run = 0, HalfY = 0;
		for (int32 Iteration = 0; Iteration < 12; ++Iteration)
		{
			Run = FMath::Max(0.1, ArrowLengthCm - StripeWidthCm * FMath::Sin(Angle));
			HalfY = FMath::Max(0.1, (ArrowWidthCm - StripeWidthCm * FMath::Cos(Angle)) * 0.5);
			Angle = FMath::Atan2(HalfY, Run);
		}
		for (int32 Sign : {-1, 1})
		{
			AddStripe(P + Facing.RotateVector(FVector(Run * 0.5, 0, 0)), P + Facing.RotateVector(FVector(-Run * 0.5, Sign * HalfY, 0)), StripeWidthCm, Material);
		}
	}
	UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(ColorMaterial, this);
	Material->SetVectorParameterValue(TEXT("ColorParam"), RingColor);
	const FVector End = Spline->GetLocationAtSplinePoint(Spline->GetNumberOfSplinePoints() - 1, ESplineCoordinateSpace::Local) + FVector(0, 0, FloorOffsetCm);
	constexpr int32 Segments = 48;
	for (int32 I = 0; I < Segments; ++I)
	{
		const double A = 2 * PI * I / Segments, B = 2 * PI * (I + 1) / Segments;
		AddStripe(End + RingRadiusCm * FVector(FMath::Cos(A), FMath::Sin(A), 0), End + RingRadiusCm * FVector(FMath::Cos(B), FMath::Sin(B), 0), RingWidthCm, Material);
	}
	UpdatePulse();
}

void AVBoRFloorRoute::SetRouteVisible(bool bVisible)
{
	SetActorHiddenInGame(!bVisible); SetActorTickEnabled(bVisible);
	if (bVisible) { PulseElapsed = 0; UpdatePulse(); }
}

void AVBoRFloorRoute::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds); PulseElapsed += FMath::Max(0.f, DeltaSeconds); UpdatePulse();
}

void AVBoRFloorRoute::UpdatePulse()
{
	const double Period = FMath::Max(0.01f, PulsePeriodSeconds);
	for (int32 I = 0; I < ArrowMaterials.Num(); ++I)
	{
		const double Phase = FMath::Fmod(PulseElapsed - I * PulseDelaySeconds + FMath::CeilToDouble(I * PulseDelaySeconds / Period) * Period, Period);
		const double Brightness = Phase < Period * 0.5 ? 1 : FMath::Clamp(DimBrightness, 0.f, 1.f);
		FLinearColor Color = ArrowColor * Brightness; Color.A = ArrowColor.A;
		ArrowMaterials[I]->SetVectorParameterValue(TEXT("ColorParam"), Color);
	}
}
