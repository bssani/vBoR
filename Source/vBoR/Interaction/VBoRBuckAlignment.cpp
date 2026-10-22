#include "VBoRBuckAlignment.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#endif

bool FVBoRBuckAlignment::FitPoints(const TArray<FVector>& Stage, const TArray<FVector>& World, double FloorZCm, FVBoRPointFit& Out)
{
	if (Stage.Num() < 2 || Stage.Num() != World.Num() || !FMath::IsFinite(FloorZCm)) { return false; }
	FVector P = FVector::ZeroVector, W = FVector::ZeroVector;
	for (int32 I = 0; I < Stage.Num(); ++I)
	{
		if (Stage[I].ContainsNaN() || World[I].ContainsNaN()) { return false; }
		P += Stage[I]; W += World[I];
	}
	P /= Stage.Num(); W /= World.Num();
	double Cross = 0, Dot = 0;
	FVBoRPointFit Fit;
	for (int32 I = 0; I < Stage.Num(); ++I)
	{
		const FVector A = Stage[I] - P, B = World[I] - W;
		Cross += A.X * B.Y - A.Y * B.X; Dot += A.X * B.X + A.Y * B.Y;
		for (int32 J = I + 1; J < Stage.Num(); ++J)
		{ Fit.SpanCm = FMath::Max(Fit.SpanCm, FVector::Dist2D(Stage[I],Stage[J])); }
	}
	if (FMath::Square(Cross) + FMath::Square(Dot) < UE_DOUBLE_SMALL_NUMBER) { return false; }
	// +X → +Y = positive Yaw. Floor sets Z, not touched points (P4h D3, U-17, cm)
	const FRotator Rotation(0,FMath::RadiansToDegrees(FMath::Atan2(Cross,Dot)),0);
	FVector T = W - Rotation.RotateVector(P); T.Z = FloorZCm;
	Fit.Pawn = FTransform(Rotation,T);
	for (int32 I = 0; I < Stage.Num(); ++I)
	{
		Fit.XYCm.Add(FVector::Dist2D(Fit.Pawn.TransformPosition(Stage[I]),World[I]));
		Fit.HeightCm.Add(Stage[I].Z + FloorZCm - World[I].Z);
		Fit.MaxXYCm = FMath::Max(Fit.MaxXYCm, Fit.XYCm.Last());
	}
	Out = Fit; return true;
}

bool FVBoRBuckAlignment::ProjectYaw(const FQuat& Orientation, double MinHorizontalLength, double& OutYawDeg)
{
	if (Orientation.ContainsNaN() || !Orientation.IsNormalized() || !FMath::IsFinite(MinHorizontalLength) || MinHorizontalLength <= 0) { return false; }
	const FVector Forward = Orientation.GetForwardVector();
	// Use horizontal forward only (tilted jig must not tilt floor, P0-c D5, E3)
	if (FMath::Square(Forward.X) + FMath::Square(Forward.Y) < FMath::Square(MinHorizontalLength)) { return false; }
	OutYawDeg = FMath::RadiansToDegrees(FMath::Atan2(Forward.Y, Forward.X));
	return FMath::IsFinite(OutYawDeg);
}

FTransform FVBoRBuckAlignment::CalculatePawn(const FVBoRBuckCaptureResult& Capture, const FTransform& EyeWorld)
{
	const FQuat Rotation = FRotator(0, FMath::FindDeltaAngleDegrees(Capture.YawDeg, double(EyeWorld.Rotator().Yaw)), 0).Quaternion();
	// Ignore previous Pawn transform (avoid repeated Stage alignment accumulation, P0-c §3-4)
	return FTransform(Rotation, EyeWorld.GetLocation() - Rotation.RotateVector(Capture.PositionCm));
}

FTransform FVBoRBuckAlignment::CalculatePawnOnFloor(const FVBoRBuckCaptureResult& Capture, const FTransform& EyeWorld, const FTransform& BaseEyeWorld, double FloorWorldZCm)
{
	FTransform Pawn = CalculatePawn(Capture, EyeWorld);
	// Ignore saved seated eye height (real floor = virtual floor). Other views add eye-height difference only (U-17, cm)
	const FVector Location = Pawn.GetLocation();
	Pawn.SetLocation(FVector(Location.X, Location.Y, FloorWorldZCm + EyeWorld.GetLocation().Z - BaseEyeWorld.GetLocation().Z));
	return Pawn;
}

FVBoRBuckApplyDecision FVBoRBuckAlignment::CheckEnvironment(const FVBoRBuckSavedAlignment& Saved, int32 CurrentOrigin,
	const FVector2D& CurrentPlayAreaCm, double ToleranceCm)
{
	FVBoRBuckApplyDecision Decision;
	if (Saved.TrackingOrigin != CurrentOrigin)
	{
		Decision.bNeedsCheck = true;
		Decision.Reason = TEXT("tracking origin differs");
		return Decision;
	}
	Decision.bApply = true;
	if (Saved.PlayAreaCm.X <= 0 || Saved.PlayAreaCm.Y <= 0 || CurrentPlayAreaCm.X <= 0 || CurrentPlayAreaCm.Y <= 0 ||
		Saved.PlayAreaCm.ContainsNaN() || CurrentPlayAreaCm.ContainsNaN())
	{
		Decision.bNeedsCheck = true;
		Decision.Reason = TEXT("play area comparison unavailable");
	}
	else if (FMath::Abs(Saved.PlayAreaCm.X - CurrentPlayAreaCm.X) > FMath::Max(0.0, ToleranceCm) ||
		FMath::Abs(Saved.PlayAreaCm.Y - CurrentPlayAreaCm.Y) > FMath::Max(0.0, ToleranceCm))
	{
		Decision.bNeedsCheck = true;
		Decision.Reason = TEXT("suspected guardian change");
	}
	return Decision;
}

FString FVBoRBuckAlignment::DescribeTip(bool bTracked, const FVector& WorldTipCm, const FTransform& EyeWorld)
{
	if (!bTracked || WorldTipCm.ContainsNaN()) { return TEXT("not tracked"); }
	// Display cm → mm, eye point +X forward/+Y right/+Z up (P0-c §2)
	const FVector TipMm = FRotator(0, EyeWorld.Rotator().Yaw, 0).UnrotateVector(WorldTipCm - EyeWorld.GetLocation()) * 10;
	return FString::Printf(TEXT("X=%.2f Y=%.2f Z=%.2f mm"), TipMm.X, TipMm.Y, TipMm.Z);
}

bool FVBoRBuckCaptureWindow::Begin(double DurationSeconds, bool bSave)
{
	if (bActive || !FMath::IsFinite(DurationSeconds) || DurationSeconds <= 0) { return false; }
	Positions.Reset(); Yaws.Reset(); Reason.Reset();
	ElapsedSeconds = 0; TargetSeconds = DurationSeconds; DiscardedFrames = 0;
	bActive = true; bSaving = bSave;
	return true;
}

bool FVBoRBuckCaptureWindow::Sample(double DeltaSeconds, const FVBoRBuckHeadFrame& Frame, double MinHorizontalLength)
{
	if (!bActive) { return false; }
	double Yaw = 0;
	if (!Frame.bTracked)
	{
		++DiscardedFrames;
		Cancel(TEXT("HMD not TRACKED; entire capture canceled"));
		return false;
	}
	if (Frame.PositionCm.ContainsNaN() || !FVBoRBuckAlignment::ProjectYaw(Frame.Orientation, MinHorizontalLength, Yaw))
	{
		++DiscardedFrames;
		Cancel(TEXT("invalid pose or near-vertical forward; entire capture canceled"));
		return false;
	}
	if (!FMath::IsFinite(DeltaSeconds) || DeltaSeconds <= 0) { return false; }
	Positions.Add(Frame.PositionCm); Yaws.Add(Yaw);
	ElapsedSeconds += DeltaSeconds;
	// Same time window at 15 fps and 90 fps (P0-c §5)
	if (ElapsedSeconds + UE_DOUBLE_SMALL_NUMBER >= TargetSeconds)
	{
		bActive = false;
		return true;
	}
	return false;
}

void FVBoRBuckCaptureWindow::Cancel(const FString& InReason)
{
	bActive = false;
	Reason = InReason;
}

bool FVBoRBuckCaptureWindow::Finish(FVBoRBuckCaptureResult& Out) const
{
	if (bActive || !Reason.IsEmpty() || Positions.IsEmpty() || ElapsedSeconds + UE_DOUBLE_SMALL_NUMBER < TargetSeconds) { return false; }
	FVBoRBuckCaptureResult Result;
	double Sin = 0, Cos = 0;
	for (int32 Index = 0; Index < Positions.Num(); ++Index)
	{
		Result.PositionCm += Positions[Index];
		Sin += FMath::Sin(FMath::DegreesToRadians(Yaws[Index]));
		Cos += FMath::Cos(FMath::DegreesToRadians(Yaws[Index]));
	}
	// Preserve direction across ±180 deg (P0-c E4)
	if (FMath::Square(Sin) + FMath::Square(Cos) < UE_DOUBLE_SMALL_NUMBER) { return false; }
	Result.PositionCm /= Positions.Num();
	Result.YawDeg = FMath::RadiansToDegrees(FMath::Atan2(Sin, Cos));
	for (int32 A = 0; A < Positions.Num(); ++A)
	{
		for (int32 B = A + 1; B < Positions.Num(); ++B)
		{
			Result.PositionSpreadMm = FMath::Max(Result.PositionSpreadMm, FVector::Distance(Positions[A], Positions[B]) * 10);
			Result.YawSpreadDeg = FMath::Max(Result.YawSpreadDeg, FMath::Abs(FMath::FindDeltaAngleDegrees(Yaws[A], Yaws[B])));
		}
	}
	Result.UsedFrames = Positions.Num(); Result.DiscardedFrames = DiscardedFrames;
	Out = Result;
	return true;
}

namespace
{
	bool ValidSaved(const FVBoRBuckSavedAlignment& Saved)
	{
		FDateTime Time;
		if (!FDateTime::ParseIso8601(*Saved.SavedAt, Time) || Saved.TrackingOrigin < 0 || Saved.TrackingOrigin > 4 ||
			Saved.PlayAreaCm.ContainsNaN() || Saved.PlayAreaCm.X < 0 || Saved.PlayAreaCm.Y < 0) { return false; }
		if (Saved.Version == 2)
		{
			TSet<int32> Orders;
			for (const auto& P : Saved.Points)
			{
				if (P.Order < 1 || Orders.Contains(P.Order) || P.StageCm.ContainsNaN() || !FMath::IsFinite(P.SpreadMm) || P.SpreadMm < 0 || P.Frames < 1) { return false; }
				Orders.Add(P.Order);
			}
			return Saved.Points.Num() >= 2;
		}
		return Saved.Version == 1 && !Saved.Capture.PositionCm.ContainsNaN() && FMath::IsFinite(Saved.Capture.YawDeg) &&
			FMath::Abs(Saved.Capture.YawDeg) <= 180 && FMath::IsFinite(Saved.Capture.PositionSpreadMm) && Saved.Capture.PositionSpreadMm >= 0 &&
			FMath::IsFinite(Saved.Capture.YawSpreadDeg) && Saved.Capture.YawSpreadDeg >= 0 && Saved.Capture.YawSpreadDeg <= 180 &&
			Saved.Capture.UsedFrames > 0 && Saved.Capture.DiscardedFrames == 0;
	}
	TArray<TSharedPtr<FJsonValue>> Numbers(std::initializer_list<double> Values)
	{
		TArray<TSharedPtr<FJsonValue>> Out;
		for (double Value : Values) { Out.Add(MakeShared<FJsonValueNumber>(Value)); }
		return Out;
	}
	bool ReadNumbers(const FJsonObject& Json, const TCHAR* Field, int32 Count, TArray<double>& Out)
	{
		const TArray<TSharedPtr<FJsonValue>>* Array;
		if (!Json.TryGetArrayField(Field, Array) || Array->Num() != Count) { return false; }
		for (const auto& Value : *Array)
		{
			double Number;
			if (!Value.IsValid() || Value->Type != EJson::Number || !Value->TryGetNumber(Number) || !FMath::IsFinite(Number)) { return false; }
			Out.Add(Number);
		}
		return true;
	}
	bool ReadNumber(const FJsonObject& Json, const TCHAR* Field, double& Out)
	{
		const auto* Value = Json.Values.Find(Field);
		return Value && (*Value).IsValid() && (*Value)->Type == EJson::Number && (*Value)->TryGetNumber(Out) && FMath::IsFinite(Out);
	}
}

bool FVBoRBuckAlignmentStore::Load(const FString& Path, FVBoRBuckSavedAlignment& Out, FString& Reason)
{
	FString Text;
	if (!FFileHelper::LoadFileToString(Text, *Path)) { Reason = TEXT("save file missing or unreadable"); return false; }
	TSharedPtr<FJsonObject> Json;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Json) || !Json.IsValid())
	{ Reason = TEXT("save file corrupt JSON"); return false; }
	FVBoRBuckSavedAlignment Candidate;
	double Version = 0, Origin = 0;
	TArray<double> Bounds;
	if (!ReadNumber(*Json,TEXT("version"),Version) || (Version != 1 && Version != 2) ||
		!Json->TryGetStringField(TEXT("savedAt"),Candidate.SavedAt) || !ReadNumber(*Json,TEXT("trackingOrigin"),Origin) ||
		!ReadNumbers(*Json,TEXT("playAreaCm"),2,Bounds) || Origin < 0 || Origin > 4 || Origin != FMath::FloorToDouble(Origin))
	{ Reason = TEXT("save schema or format version invalid"); return false; }
	Candidate.Version = static_cast<int32>(Version); Candidate.TrackingOrigin = static_cast<int32>(Origin);
	Candidate.PlayAreaCm = FVector2D(Bounds[0],Bounds[1]);
	if (Candidate.Version == 2)
	{
		const TArray<TSharedPtr<FJsonValue>>* Points;
		if (!Json->TryGetArrayField(TEXT("points"),Points)) { Reason = TEXT("save points missing"); return false; }
		for (const auto& Value : *Points)
		{
			const TSharedPtr<FJsonObject>* Point;
			TArray<double> Position; double Order = 0, Frames = 0; FVBoRAlignPointSample P;
			if (!Value.IsValid() || !Value->TryGetObject(Point) || !Point->IsValid() ||
				!ReadNumber(**Point,TEXT("order"),Order) || Order < 1 || Order > MAX_int32 || Order != FMath::FloorToDouble(Order) ||
				!ReadNumbers(**Point,TEXT("stagePositionCm"),3,Position) || !ReadNumber(**Point,TEXT("spreadMm"),P.SpreadMm) ||
				!ReadNumber(**Point,TEXT("frames"),Frames) || Frames < 1 || Frames > MAX_int32 || Frames != FMath::FloorToDouble(Frames))
			{ Reason = TEXT("save point schema invalid"); return false; }
			P.Order = static_cast<int32>(Order); P.Frames = static_cast<int32>(Frames); P.StageCm = FVector(Position[0],Position[1],Position[2]);
			Candidate.Points.Add(P);
		}
	}
	else
	{
		TArray<double> Position; double Used = 0, Discarded = 0;
		if (!ReadNumbers(*Json,TEXT("stagePositionCm"),3,Position) || !ReadNumber(*Json,TEXT("stageYawDeg"),Candidate.Capture.YawDeg) ||
			!ReadNumber(*Json,TEXT("positionSpreadMm"),Candidate.Capture.PositionSpreadMm) || !ReadNumber(*Json,TEXT("yawSpreadDeg"),Candidate.Capture.YawSpreadDeg) ||
			!ReadNumber(*Json,TEXT("usedFrames"),Used) || !ReadNumber(*Json,TEXT("discardedFrames"),Discarded) ||
			Used < 1 || Used > MAX_int32 || Used != FMath::FloorToDouble(Used) || Discarded != 0)
		{ Reason = TEXT("save schema or format version invalid"); return false; }
		Candidate.Capture.PositionCm = FVector(Position[0],Position[1],Position[2]); Candidate.Capture.UsedFrames = static_cast<int32>(Used);
	}
	if (!ValidSaved(Candidate)) { Reason = TEXT("save contains invalid values"); return false; }
	Out = Candidate; Reason.Reset();
	return true;
}

bool FVBoRBuckAlignmentStore::Save(const FString& Path, const FVBoRBuckSavedAlignment& Candidate, FString& Reason)
{
	if (!ValidSaved(Candidate)) { Reason = TEXT("candidate contains invalid values"); return false; }
	const auto Json = MakeShared<FJsonObject>();
	Json->SetNumberField(TEXT("version"), Candidate.Version);
	Json->SetStringField(TEXT("savedAt"), Candidate.SavedAt);
	Json->SetNumberField(TEXT("trackingOrigin"), Candidate.TrackingOrigin);
	Json->SetArrayField(TEXT("playAreaCm"), Numbers({Candidate.PlayAreaCm.X, Candidate.PlayAreaCm.Y}));
	if (Candidate.Version == 2)
	{
		TArray<TSharedPtr<FJsonValue>> Points;
		for (const auto& P : Candidate.Points)
		{
			const auto Point = MakeShared<FJsonObject>();
			Point->SetNumberField(TEXT("order"),P.Order);
			Point->SetArrayField(TEXT("stagePositionCm"),Numbers({P.StageCm.X,P.StageCm.Y,P.StageCm.Z}));
			Point->SetNumberField(TEXT("spreadMm"),P.SpreadMm); Point->SetNumberField(TEXT("frames"),P.Frames);
			Points.Add(MakeShared<FJsonValueObject>(Point));
		}
		Json->SetArrayField(TEXT("points"),Points);
	}
	else
	{
		Json->SetArrayField(TEXT("stagePositionCm"), Numbers({Candidate.Capture.PositionCm.X, Candidate.Capture.PositionCm.Y, Candidate.Capture.PositionCm.Z}));
		Json->SetNumberField(TEXT("stageYawDeg"), Candidate.Capture.YawDeg);
		Json->SetNumberField(TEXT("positionSpreadMm"), Candidate.Capture.PositionSpreadMm);
		Json->SetNumberField(TEXT("yawSpreadDeg"), Candidate.Capture.YawSpreadDeg);
		Json->SetNumberField(TEXT("usedFrames"), Candidate.Capture.UsedFrames);
		Json->SetNumberField(TEXT("discardedFrames"), Candidate.Capture.DiscardedFrames);
	}
	FString Text;
	const FString CandidatePath = Path + TEXT(".candidate");
	if (!FJsonSerializer::Serialize(Json, TJsonWriterFactory<>::Create(&Text)) ||
		!IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true) ||
		!FFileHelper::SaveStringToFile(Text, *CandidatePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{ Reason = TEXT("candidate file write failed; previous save retained"); return false; }
	FVBoRBuckSavedAlignment Verified;
	// IFileManager::Move deletes existing file first. Atomically replace with verified candidate (Win64, P0-c §3-7)
	bool bReplaced = false;
	uint32 ReplacementError = 0;
	if (Load(CandidatePath, Verified, Reason))
	{
#if PLATFORM_WINDOWS
		bReplaced = ::MoveFileExW(*FPaths::ConvertRelativePathToFull(CandidatePath), *FPaths::ConvertRelativePathToFull(Path), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
		if (!bReplaced) { ReplacementError = ::GetLastError(); }
#else
		Reason = TEXT("safe replacement available only on Windows PCVR");
#endif
	}
	if (!bReplaced)
	{
		IFileManager::Get().Delete(*CandidatePath);
		Reason = FString::Printf(TEXT("candidate replacement failed (OS error %u); previous save retained"), ReplacementError);
		return false;
	}
	Reason.Reset();
	return true;
}
