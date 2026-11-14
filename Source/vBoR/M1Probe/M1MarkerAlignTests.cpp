#if WITH_DEV_AUTOMATION_TESTS && defined(WITH_OPENCV) && WITH_OPENCV

#include "M1ProbeTestActor.h"
#include "../Interaction/VBoRAlignPointComponent.h"
#include "../Interaction/VBoRVehicleAlignmentComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/WorldSettings.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/CoreDelegates.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "MRUtilityKitSubsystem.h"
#include "PreOpenCVHeaders.h"
#include "opencv2/aruco.hpp"
#include "PostOpenCVHeaders.h"

bool SolveM1Marker(const FVector2f (&Pixels)[4], double SideMm, const FM1CameraModel& Model,
	const FTransform& PawnWorld, FM1MarkerObservation& Out);
void DetectM1Markers(FM1CameraFrame& Frame, TFunctionRef<void(int32, const FVector2f (&)[4])> Visit);

namespace
{
constexpr auto M1Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;

FM1CameraFrame M1Frame(const FTransform& Camera = FTransform::Identity)
{
	FM1CameraFrame Frame;
	Frame.Size = FIntPoint(1280, 960);
	Frame.Model = {Camera, FVector2D(1000, 1000), FVector2D(633.5, 482.5)};
	Frame.Timestamp = FDateTime(2026, 11, 15);
	return Frame;
}

void DrawM1Marker(FM1CameraFrame& Frame, int32 Id, const FVector& Center, const FVector& Right, const FVector& Up)
{
	const FVector Normal = FVector::CrossProduct(Right, Up);
	const auto& Camera = Frame.Model.Pose.GetLocation();

	cv::Mat Pattern;
	cv::aruco::getPredefinedDictionary(cv::aruco::DICT_4X4_50)->drawMarker(Id, 6, Pattern);
	if (Frame.Pixels.IsEmpty())
	{
		Frame.Pixels.Init(FColor::White, Frame.Size.X * Frame.Size.Y);
	}

	// Clip work to projected corners and a one-cell white margin
	FVector2D Min(Frame.Size.X, Frame.Size.Y);
	FVector2D Max = FVector2D::ZeroVector;
	for (double X : {-4.0, 4.0})
	{
		for (double Y : {-4.0, 4.0})
		{
			const auto Pixel = Frame.Model.Project(Center + Right * X + Up * Y);
			Min.X = FMath::Min(Min.X, Pixel.X);
			Min.Y = FMath::Min(Min.Y, Pixel.Y);
			Max.X = FMath::Max(Max.X, Pixel.X);
			Max.Y = FMath::Max(Max.Y, Pixel.Y);
		}
	}
	for (int32 Y = FMath::Max(0, FMath::FloorToInt(Min.Y)); Y < FMath::Min(Frame.Size.Y, FMath::CeilToInt(Max.Y)); ++Y)
	{
		for (int32 X = FMath::Max(0, FMath::FloorToInt(Min.X)); X < FMath::Min(Frame.Size.X, FMath::CeilToInt(Max.X)); ++X)
		{
			// Integrate subpixel coverage before detection (sample around pixel centers)
			int32 BlackSamples = 0;
			for (double Dy : {-0.375, -0.125, 0.125, 0.375})
			{
				for (double Dx : {-0.375, -0.125, 0.125, 0.375})
				{
					const FVector Ray = Frame.Model.Ray(FVector2D(X + Dx, Y + Dy));
					const FVector Offset = Camera + Ray * (FVector::DotProduct(Normal, Center - Camera) /
						FVector::DotProduct(Normal, Ray)) - Center;
					const int32 Col = FMath::FloorToInt(FVector::DotProduct(Offset, Right) + 3);
					const int32 Row = FMath::FloorToInt(3 - FVector::DotProduct(Offset, Up));
					if (Col >= 0 && Col < 6 && Row >= 0 && Row < 6 && Pattern.at<uint8>(Row, Col) == 0)
					{
						++BlackSamples;
					}
				}
			}
			if (BlackSamples)
			{
				const uint8 Value = static_cast<uint8>(255 - (255 * BlackSamples + 8) / 16);
				Frame.Pixels[Y * Frame.Size.X + X] = FColor(Value, Value, Value);
			}
		}
	}
}

TArray<FVector> M1Rectangle()
{
	return {FVector(-24, -23, 50), FVector(24, -23, 50), FVector(24, 23, 50), FVector(-24, 23, 50)};
}
}

struct FM1ProbeFixture
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	APawn* Pawn;
	AActor* Car;
	UVBoRVehicleAlignmentComponent* Settings;
	AM1ProbeTestActor* Probe;
	UMRUKSubsystem* Mruk = nullptr;
	bool bOriginalWorldLock = false;
	double Now = 10;

	explicit FM1ProbeFixture(bool bCameraWorks = true, bool bWorldLock = false)
	{
		GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
		if (bWorldLock)
		{
			auto* GameInstance = NewObject<UGameInstance>(GEngine);
			GameInstance->InitializeStandalone();
			World->SetGameInstance(GameInstance);
			Mruk = GameInstance->GetSubsystem<UMRUKSubsystem>();
			if (Mruk)
			{
				Mruk->EnableWorldLock = true;
				bOriginalWorldLock = Mruk->EnableWorldLock;
			}
		}
		Pawn = World->SpawnActor<APawn>();
		Pawn->SetRootComponent(NewObject<USceneComponent>(Pawn));
		Pawn->GetRootComponent()->RegisterComponent();

		Car = World->SpawnActor<AActor>();
		Settings = NewObject<UVBoRVehicleAlignmentComponent>(Car);
		Car->AddInstanceComponent(Settings);
		Settings->RegisterComponent();
		const TArray<FVector> Points = M1Rectangle();
		for (int32 Index = 0; Index < Points.Num(); ++Index)
		{
			auto* Point = NewObject<UVBoRAlignPointComponent>(Car);
			Car->AddInstanceComponent(Point);
			Point->Order = Index + 1;
			Point->Label = FString::Printf(TEXT("Bolt %d"), Point->Order);
			Point->RegisterComponent();
			Point->SetWorldLocation(Points[Index]);
		}
		Probe = World->SpawnActor<AM1ProbeTestActor>();
		Probe->Pawn = Pawn;
		Probe->bCanStart = bCameraWorks;
		Probe->bCollectTestMessages = true;
		World->InitializeActorsForPlay(FURL());
		World->GetWorldSettings()->NotifyBeginPlay();
	}

	~FM1ProbeFixture()
	{
		World->EndPlay(EEndPlayReason::Quit);
		UGameInstance* Instance = World->GetGameInstance();
		UWorld* InstanceWorld = Instance ? Instance->GetWorld() : nullptr;
		if (Instance)
		{
			Instance->Shutdown();
		}
		GEngine->DestroyWorldContext(World);
		World->DestroyWorld(false);
		if (InstanceWorld && InstanceWorld != World)
		{
			GEngine->DestroyWorldContext(InstanceWorld);
			InstanceWorld->DestroyWorld(false);
		}
	}

	void Start() { Probe->StartCapture(Now); }
	void Frame(FM1CameraFrame Image)
	{
		Probe->Frame = MoveTemp(Image);
		Probe->Tick(0);
	}
	void Record(int32 Id, const FM1MarkerObservation& Observation)
	{
		Probe->RecordObservation(Id, &Observation, false);
	}
	void Finish() { Probe->FinishCapture(Now + 1); }
	void Timeout()
	{
		Probe->CaptureStart = World->GetRealTimeSeconds() - Probe->MarkerSessionSeconds;
		Probe->Tick(0);
	}
	void Apply() { Probe->ApplyFit(); }
	void Restore() { Probe->RestorePawn(); }
	void Tip(double EndOffset = 0) { Probe->ProcessTip(Probe->TipEnd + EndOffset); }
	void StartTip() { Probe->StartTip(); }
	auto Fit() const { return Probe->Fit; }
	int32 Samples(int32 Id) const
	{
		const auto* Marker = Probe->Markers.Find(Id);
		return Marker ? Marker->Samples.Num() : 0;
	}
	FVector Center(int32 Id) const { return Probe->Markers[Id].CenterStage; }
	int32 Rejected(int32 Id) const { return Probe->Markers[Id].Rejected; }
	bool Capturing() const { return Probe->bCapturing; }
	bool SamplingTip() const { return Probe->bSamplingTip; }
	FString Messages() const { return FString::Join(Probe->TestMessages, TEXT("\n")); }
	void ClearMessages() { Probe->TestMessages.Reset(); }
	FString Check(const FVector& WorldTip) const { return Probe->DescribeCheckPoint(WorldTip); }
	void Fill(const TArray<FVector>& Stage, double ArrowYaw = 0)
	{
		for (int32 Index = 0; Index < Stage.Num(); ++Index)
		{
			Record(Index + 1, {Stage[Index], ArrowYaw, 0, 0.2, 40});
		}
	}
	void FillFit()
	{
		Probe->MarkerMinFrames = 1;
		Start();
		Fill(M1Rectangle());
		Finish();
	}
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FM1E1, "vBoR.M1.E1", M1Flags)
bool FM1E1::RunTest(const FString&)
{
	for (const FRotator Pose : {FRotator::ZeroRotator, FRotator(-40, 30, 0), FRotator(-22, 35, 15)})
	{
		const auto Model = M1Frame(FTransform(Pose, FVector(30, -70, 110))).Model;
		for (const FVector2D Pixel : {FVector2D(64, 640), FVector2D(920, 370), FVector2D(550, 465)})
		{
			const FVector World = Model.Pose.GetLocation() + 85 * Model.Ray(Pixel);
			TestTrue(TEXT("Pixel round trip < 0.01 px"), FVector2D::Distance(Pixel, Model.Project(World)) < 0.01);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FM1E2, "vBoR.M1.E2", M1Flags)
bool FM1E2::RunTest(const FString&)
{
	for (const FRotator Pose : {FRotator::ZeroRotator, FRotator(-40, 30, 0), FRotator(-22, 35, 15)})
	{
		for (double Distance : {60.0, 120.0})
		{
			auto Image = M1Frame(FTransform(Pose, FVector(10, -22, 111)));
			const auto& Camera = Image.Model.Pose;
			const FVector Local(Distance, 6, 6);
			const FVector Center = Camera.TransformPosition(Local);
			const FQuat Tilt = Distance == 60 && Pose.Roll != 0 ? FRotator(12, -17, 6).Quaternion() : FQuat::Identity;
			DrawM1Marker(Image, 2, Center, Camera.TransformVectorNoScale(Tilt.GetRightVector()),
				Camera.TransformVectorNoScale(Tilt.GetUpVector()));
			int32 Count = 0;
			DetectM1Markers(Image, [&](int32 Id, const FVector2f (&Corners)[4])
			{
				++Count;
				TestEqual(TEXT("Marker ID"), Id, 2);
				FM1MarkerObservation Found;
				if (TestTrue(TEXT("Solve rendered marker"), SolveM1Marker(Corners, 60, Image.Model, FTransform::Identity, Found)))
				{
					const double ErrorMm = FVector::Distance(Found.CenterStage, Center) * 10;
					TestTrue(FString::Printf(TEXT("Center error %.2f mm at %.0f cm"), ErrorMm, Distance),
						ErrorMm <= (Distance == 60 ? 2 : 4));
					TestTrue(TEXT("Reprojection <= 1 px"), Found.ReprojectionPx <= 1);
					TestTrue(TEXT("Center stays above optical axis"), Camera.InverseTransformPosition(Found.CenterStage).Z > 0);
				}
			});
			TestEqual(TEXT("One marker detected"), Count, 1);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FM1E3, "vBoR.M1.E3", M1Flags)
bool FM1E3::RunTest(const FString&)
{
	for (bool bMirrored : {true, false})
	{
		FM1ProbeFixture W;
		W.Probe->bMirrorProjection = bMirrored;
		W.Frame(M1Frame(FTransform(FRotator(-40, 30, 15), FVector(87, 54, 0))));
		W.Start();
		TestEqual(TEXT("Capture only works after expected projection"), W.Capturing(), bMirrored);
		TestTrue(TEXT("Check outcome shown"), W.Messages().Contains(bMirrored ?
			TEXT("Camera model check passed") : TEXT("Camera model mismatch")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FM1E4, "vBoR.M1.E4", M1Flags)
bool FM1E4::RunTest(const FString&)
{
	FM1ProbeFixture W;
	W.Start();
	auto Image = M1Frame();
	DrawM1Marker(Image, 1, FVector(60, -10, 0), FVector::RightVector, FVector::UpVector);
	W.Frame(Image);
	W.Frame(MoveTemp(Image));
	TestEqual(TEXT("Duplicate timestamp counted once"), W.Samples(1), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FM1E5, "vBoR.M1.E5", M1Flags)
bool FM1E5::RunTest(const FString&)
{
	FM1ProbeFixture W;
	const FTransform Before = W.Pawn->GetActorTransform();
	W.Probe->MarkerSessionSeconds = 0.01;
	W.Start();
	for (int32 Id : {1, 2, 4})
	{
		for (int32 Index = 0; Index < 30; ++Index)
		{
			W.Record(Id, {M1Rectangle()[Id - 1], 0, 0, 0.2, 45});
		}
	}
	W.Timeout();
	TestTrue(TEXT("Missing ID and count"), W.Messages().Contains(TEXT("Incomplete — missing ID 3 (0/30 frames)")));
	TestFalse(TEXT("No fit"), W.Fit().IsSet());
	TestTrue(TEXT("Pawn stays put"), Before.Equals(W.Pawn->GetActorTransform()));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FM1E6, "vBoR.M1.E6", M1Flags)
bool FM1E6::RunTest(const FString&)
{
	FM1ProbeFixture W;
	W.Probe->MarkerMinFrames = 2;
	W.Start();
	const auto Stage = M1Rectangle();
	for (double Noise : {-0.8, 0.8})
	{
		auto Samples = Stage;
		Samples[1].X += Noise;
		W.Fill(Samples);
	}
	W.Finish();
	TestTrue(TEXT("ID 2 rejected at 16 mm"), W.Messages().Contains(TEXT("Marker 2 rejected: spread 16.00 mm")));
	TestFalse(TEXT("No fit"), W.Fit().IsSet());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FM1E7, "vBoR.M1.E7", M1Flags)
bool FM1E7::RunTest(const FString&)
{
	FM1ProbeFixture W;
	W.Start();
	const FM1CameraModel Model = M1Frame().Model;
	for (double Depth : {70.0, 275.0})
	{
		FVector2f Corners[4] = {
			FVector2f(Model.Project(FVector(Depth, -3, 3))),
			FVector2f(Model.Project(FVector(Depth, 3, 3))),
			FVector2f(Model.Project(FVector(Depth, 3, -3))),
			FVector2f(Model.Project(FVector(Depth, -3, -3))),
		};
		if (Depth == 70)
		{
			Corners[0].X += 5;
		}
		FM1MarkerObservation Sample;
		if (TestTrue(TEXT("PnP result available for rejection"), SolveM1Marker(Corners, 60, Model, FTransform::Identity, Sample)))
		{
			TestTrue(TEXT("Wanted filter fired"), Depth == 70 ? Sample.ReprojectionPx > 1 : Sample.SidePx < 24);
			W.Record(1, Sample);
		}
	}
	TestEqual(TEXT("Both frames rejected"), W.Rejected(1), 2);
	TestEqual(TEXT("No frames accepted"), W.Samples(1), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FM1E8, "vBoR.M1.E8", M1Flags)
bool FM1E8::RunTest(const FString&)
{
	FM1ProbeFixture W;
	W.Start();
	auto Image = M1Frame();
	DrawM1Marker(Image, 1, FVector(60, -10, 0), FVector::RightVector, FVector::UpVector);
	DrawM1Marker(Image, 7, FVector(60, 10, 0), FVector::RightVector, FVector::UpVector);
	W.Frame(MoveTemp(Image));
	W.Finish();
	TestEqual(TEXT("Known ID counted"), W.Samples(1), 1);
	TestEqual(TEXT("Unknown ID ignored"), W.Samples(7), 0);
	TestFalse(TEXT("Unknown ID omitted from results"), W.Messages().Contains(TEXT("Marker 7")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FM1E9, "vBoR.M1.E9", M1Flags)
bool FM1E9::RunTest(const FString&)
{
	FM1ProbeFixture W;
	W.Probe->MarkerMinFrames = 1;
	W.Start();
	const auto World = M1Rectangle();
	const FTransform Expected(FRotator(0, 1.5, 0), FVector(3, -2, 0));
	TArray<FVector> Stage;
	for (const FVector& Point : World)
	{
		Stage.Add(Expected.InverseTransformPosition(Point));
	}
	W.Fill(Stage, -1.5);
	W.Finish();
	FVBoRPointFit Direct;
	TestTrue(TEXT("Direct FitPoints"), FVBoRBuckAlignment::FitPoints(Stage, World, 0, Direct));
	if (TestTrue(TEXT("Fit available"), W.Fit().IsSet()))
	{
		TestTrue(TEXT("Same as FitPoints"), W.Fit()->Pawn.Equals(Direct.Pawn, 0.01));
		TestTrue(TEXT("Known translation and rotation"), W.Fit()->Pawn.Equals(Expected, 0.0001));
	}
	const FString At70 = FString::Printf(TEXT("at 70 cm %.2f cm"), 70 * FMath::Tan(FMath::DegreesToRadians(1.5)));
	TestTrue(TEXT("Reported delta and 70 cm equivalent"), W.Messages().Contains(
		TEXT("dX +3.00 cm | dY -2.00 cm | dYaw +1.50 deg | ") + At70));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FM1E10, "vBoR.M1.E10", M1Flags)
bool FM1E10::RunTest(const FString&)
{
	FM1ProbeFixture W;
	W.Probe->MarkerMinFrames = 1;
	W.Probe->MarkerHeightOffsetCm = 1.5;
	W.Start();
	auto Stage = M1Rectangle();
	for (FVector& Point : Stage) { Point.Z += 1.5; }
	W.Fill(Stage);
	W.Finish();
	if (TestTrue(TEXT("Fit available"), W.Fit().IsSet()))
	{
		for (double Height : W.Fit()->HeightCm)
		{
			TestTrue(TEXT("Cap thickness subtracted"), FMath::IsNearlyZero(Height, 0.01));
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FM1E11, "vBoR.M1.E11", M1Flags)
bool FM1E11::RunTest(const FString&)
{
	FM1ProbeFixture W;
	const FTransform Before(FRotator(0, 14, 0), FVector(15, 20, 30));
	W.Pawn->SetActorTransform(Before);
	W.Apply();
	TestTrue(TEXT("Cannot apply with no fit"), W.Messages().Contains(TEXT("No marker fit")) &&
		W.Pawn->GetActorTransform().Equals(Before));
	W.FillFit();
	W.Apply();
	TestTrue(TEXT("Preview transform applied"), W.Pawn->GetActorTransform().Equals(FTransform::Identity));
	W.Apply();
	W.Restore();
	TestTrue(TEXT("Original transform restored, even after repeated G"), W.Pawn->GetActorTransform().Equals(Before));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FM1E12, "vBoR.M1.E12", M1Flags)
bool FM1E12::RunTest(const FString&)
{
	FM1ProbeFixture W;
	const FString Path = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Calibration/BuckEye.json"));
	const bool bExisted = IFileManager::Get().FileExists(*Path);
	const FDateTime Timestamp = IFileManager::Get().GetTimeStamp(*Path);
	TArray<uint8> Before, After;
	if (bExisted)
	{
		TestTrue(TEXT("Read existing file"), FFileHelper::LoadFileToArray(Before, *Path));
	}
	const auto State = W.Settings->State;
	const FString Reason = W.Settings->Reason;
	W.FillFit();
	W.Apply();
	W.Restore();
	W.Probe->Tip = M1Rectangle()[0];
	W.Probe->bTracked = true;
	W.StartTip();
	W.Tip();
	TestEqual(TEXT("File presence"), IFileManager::Get().FileExists(*Path), bExisted);
	if (bExisted)
	{
		TestTrue(TEXT("File bytes preserved"), FFileHelper::LoadFileToArray(After, *Path) && Before == After);
		TestTrue(TEXT("Modification timestamp preserved"), IFileManager::Get().GetTimeStamp(*Path) == Timestamp);
	}
	TestTrue(TEXT("State and reason preserved"), W.Settings->State == State && W.Settings->Reason == Reason);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FM1E13, "vBoR.M1.E13", M1Flags)
bool FM1E13::RunTest(const FString&)
{
	FM1ProbeFixture W;
	W.FillFit();
	W.Probe->Tip = M1Rectangle()[0] - FVector(0.3, 0.4, 0.2);
	W.Probe->bTracked = true;
	W.StartTip();
	W.Tip(-0.5);
	TestTrue(TEXT("Window remains open before 1 s"), W.SamplingTip());
	W.Tip();
	TestTrue(TEXT("Known fingertip offset"), W.Messages().Contains(TEXT("XY 5.0 mm | height +2.0 mm | 3D 5.4 mm")));
	W.StartTip();
	W.Tip(-0.7);
	W.Probe->bTracked = false;
	W.Tip(-0.5);
	TestTrue(TEXT("One missed frame cancels"), !W.SamplingTip() &&
		W.Messages().Contains(TEXT("Fingertip comparison canceled")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FM1E14, "vBoR.M1.E14", M1Flags)
bool FM1E14::RunTest(const FString&)
{
	FM1ProbeFixture W;
	W.Start();
	W.Fill(M1Rectangle());
	FCoreDelegates::VRHeadsetRecenter.Broadcast();
	TestFalse(TEXT("Recenter stops capture"), W.Capturing());
	TestEqual(TEXT("Recenter clears samples"), W.Samples(1), 0);
	TestTrue(TEXT("Recenter reason"), W.Messages().Contains(TEXT("Recenter — marker capture canceled")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FM1E15, "vBoR.M1.E15", M1Flags)
bool FM1E15::RunTest(const FString&)
{
	FM1ProbeFixture W(false, true);
	if (W.Mruk)
	{
		TestFalse(TEXT("World Lock disabled during probe"), W.Mruk->EnableWorldLock);
		W.World->EndPlay(EEndPlayReason::Quit);
		TestEqual(TEXT("Original World Lock restored"), W.Mruk->EnableWorldLock, W.bOriginalWorldLock);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FM1E16, "vBoR.M1.E16", M1Flags)
bool FM1E16::RunTest(const FString&)
{
	FM1ProbeFixture W;
	for (int32 Index : {0, 1})
	{
		auto* Check = NewObject<USceneComponent>(W.Car, Index == 0 ? TEXT("NearCheck") : TEXT("FarCheck"));
		W.Car->AddInstanceComponent(Check);
		Check->ComponentTags.Add(TEXT("Align.Check"));
		Check->RegisterComponent();
		Check->SetWorldLocation(FVector(20 * Index, 0, 50));
	}
	W.FillFit();
	W.Apply();
	W.Probe->bTracked = true;
	W.Probe->Tip = FVector(1, 0, 50.5);
	W.Probe->Tick(0);
	TestEqual(TEXT("Closest checkpoint and offset"), W.Check(W.Probe->Tip), FString(TEXT("NearCheck | XY 1.0 cm | height +0.5 cm")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FM1E17, "vBoR.M1.E17", M1Flags)
bool FM1E17::RunTest(const FString&)
{
	FM1ProbeFixture W(false);
	W.Start();
	TestFalse(TEXT("M refused"), W.Capturing());
	TestTrue(TEXT("Link settings hint"), W.Messages().Contains(TEXT("Passthrough over Link, camera API permission")));
	W.StartTip();
	TestTrue(TEXT("T responds"), W.SamplingTip());
	W.Tip();
	W.Apply();
	TestTrue(TEXT("G responds"), W.Messages().Contains(TEXT("No marker fit")));
	W.Restore();
	TestFalse(TEXT("H leaves pawn unchanged with no preview"), W.Fit().IsSet());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FM1E18, "vBoR.M1.E18", M1Flags)
bool FM1E18::RunTest(const FString&)
{
	FM1ProbeFixture W;
	W.Probe->MarkerMinFrames = 1;
	for (int32 Case : {0, 1, 2, 3})
	{
		W.ClearMessages();
		W.Start();
		const auto Points = M1Rectangle();
		for (int32 Index = 0; Index < Points.Num(); ++Index)
		{
			const int32 Position = Case == 1 || Case == 3 ? (Index + 2) % 4 : Index;
			const double Yaw = Case == 3 ? 180 : (Case == 2 && Index == 2 ? 90 : 0);
			W.Record(Index + 1, {Points[Position], Yaw, 0, 0.2, 50});
		}
		W.Finish();
		TestEqual(FString::Printf(TEXT("D16 fit presence for E18 case %d"), Case + 1), W.Fit().IsSet(), Case == 0 || Case == 3);
		if (Case == 1 || Case == 2)
		{
			TestTrue(TEXT("Specific marker and angle reported"), W.Messages().Contains(Case == 1 ?
				TEXT("Marker 1 arrow 180 deg off car front") : TEXT("Marker 3 arrow 90 deg off car front")));
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FM1E19, "vBoR.M1.E19", M1Flags)
bool FM1E19::RunTest(const FString&)
{
	FM1ProbeFixture W;
	W.Start();
	auto Image = M1Frame();
	DrawM1Marker(Image, 1, FVector(60, -10, 0), FVector::RightVector, FVector::UpVector);
	W.Frame(Image);
	Image.Model.Pose.SetRotation(FRotator(0, 2, 0).Quaternion());
	Image.Timestamp += FTimespan::FromMilliseconds(33);
	W.Frame(MoveTemp(Image));
	TestEqual(TEXT("Moving frame rejected"), W.Rejected(1), 1);
	TestEqual(TEXT("Still frame accepted"), W.Samples(1), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FM1E20, "vBoR.M1.E20", M1Flags)
bool FM1E20::RunTest(const FString&)
{
	FM1ProbeFixture W;
	const FTransform Pawn(FRotator(0, 35, 0), FVector(-770, -530, 570));
	W.Pawn->SetActorTransform(Pawn);
	W.Start();
	auto Image = M1Frame(Pawn);
	const FVector Stage(60, 6, 6);
	DrawM1Marker(Image, 1, Pawn.TransformPosition(Stage), Pawn.GetRotation().GetRightVector(), FVector::UpVector);
	W.Frame(MoveTemp(Image));
	if (TestEqual(TEXT("One accepted sample"), W.Samples(1), 1))
	{
		TestTrue(FString::Printf(TEXT("Center converted to Stage: %s"), *W.Center(1).ToString()), W.Center(1).Equals(Stage, 0.1));
	}
	return true;
}

#endif
