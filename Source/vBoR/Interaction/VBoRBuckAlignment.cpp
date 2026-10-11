#include "VBoRBuckAlignment.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#endif

bool FVBoRBuckAlignment::ProjectYaw(const FQuat& Orientation, double MinHorizontalLength, double& OutYawDeg)
{
	if (Orientation.ContainsNaN() || !Orientation.IsNormalized() || !FMath::IsFinite(MinHorizontalLength) || MinHorizontalLength <= 0) { return false; }
	const FVector Forward = Orientation.GetForwardVector();
	// 기울어진 지그도 바닥을 기울이지 않게 수평 정면만 사용함 (P0-c D5, E3)
	if (FMath::Square(Forward.X) + FMath::Square(Forward.Y) < FMath::Square(MinHorizontalLength)) { return false; }
	OutYawDeg = FMath::RadiansToDegrees(FMath::Atan2(Forward.Y, Forward.X));
	return FMath::IsFinite(OutYawDeg);
}

FTransform FVBoRBuckAlignment::CalculatePawn(const FVBoRBuckCaptureResult& Capture, const FTransform& EyeWorld)
{
	const FQuat Rotation = FRotator(0, FMath::FindDeltaAngleDegrees(Capture.YawDeg, double(EyeWorld.Rotator().Yaw)), 0).Quaternion();
	// 이전 Pawn 변환을 읽지 않음 (Stage 저장값 재적용 시 누적 방지, P0-c §3-4)
	return FTransform(Rotation, EyeWorld.GetLocation() - Rotation.RotateVector(Capture.PositionCm));
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
	// 표시 cm → mm, eye point +X 앞·+Y 오른쪽·+Z 위 (P0-c §2)
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
	// 15 fps 와 90 fps 에서 같은 초 길이 사용 (P0-c §5)
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
	// ±180 deg 경계의 방향을 보존함 (P0-c E4)
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
		return FDateTime::ParseIso8601(*Saved.SavedAt, Time) && Saved.TrackingOrigin >= 0 && Saved.TrackingOrigin <= 4 &&
			!Saved.PlayAreaCm.ContainsNaN() && Saved.PlayAreaCm.X >= 0 && Saved.PlayAreaCm.Y >= 0 &&
			!Saved.Capture.PositionCm.ContainsNaN() && FMath::IsFinite(Saved.Capture.YawDeg) &&
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
	double Version = 0, Origin = 0, Used = 0, Discarded = 0;
	TArray<double> Position, Bounds;
	if (!ReadNumber(*Json, TEXT("version"), Version) || Version != FVBoRBuckSavedAlignment::FormatVersion ||
		!Json->TryGetStringField(TEXT("savedAt"), Candidate.SavedAt) || !ReadNumber(*Json, TEXT("trackingOrigin"), Origin) ||
		!ReadNumbers(*Json, TEXT("stagePositionCm"), 3, Position) || !ReadNumbers(*Json, TEXT("playAreaCm"), 2, Bounds) ||
		!ReadNumber(*Json, TEXT("stageYawDeg"), Candidate.Capture.YawDeg) ||
		!ReadNumber(*Json, TEXT("positionSpreadMm"), Candidate.Capture.PositionSpreadMm) ||
		!ReadNumber(*Json, TEXT("yawSpreadDeg"), Candidate.Capture.YawSpreadDeg) ||
		!ReadNumber(*Json, TEXT("usedFrames"), Used) || !ReadNumber(*Json, TEXT("discardedFrames"), Discarded) ||
		Origin < 0 || Origin > 4 || Origin != FMath::FloorToDouble(Origin) || Used < 1 || Used > MAX_int32 || Used != FMath::FloorToDouble(Used) || Discarded != 0)
	{ Reason = TEXT("save schema or format version invalid"); return false; }
	Candidate.TrackingOrigin = static_cast<int32>(Origin);
	Candidate.Capture.UsedFrames = static_cast<int32>(Used);
	Candidate.Capture.DiscardedFrames = 0;
	Candidate.Capture.PositionCm = FVector(Position[0], Position[1], Position[2]);
	Candidate.PlayAreaCm = FVector2D(Bounds[0], Bounds[1]);
	if (!ValidSaved(Candidate)) { Reason = TEXT("save contains invalid values"); return false; }
	Out = Candidate; Reason.Reset();
	return true;
}

bool FVBoRBuckAlignmentStore::Save(const FString& Path, const FVBoRBuckSavedAlignment& Candidate, FString& Reason)
{
	if (!ValidSaved(Candidate)) { Reason = TEXT("candidate contains invalid values"); return false; }
	const auto Json = MakeShared<FJsonObject>();
	Json->SetNumberField(TEXT("version"), FVBoRBuckSavedAlignment::FormatVersion);
	Json->SetStringField(TEXT("savedAt"), Candidate.SavedAt);
	Json->SetNumberField(TEXT("trackingOrigin"), Candidate.TrackingOrigin);
	Json->SetArrayField(TEXT("stagePositionCm"), Numbers({Candidate.Capture.PositionCm.X, Candidate.Capture.PositionCm.Y, Candidate.Capture.PositionCm.Z}));
	Json->SetNumberField(TEXT("stageYawDeg"), Candidate.Capture.YawDeg);
	Json->SetArrayField(TEXT("playAreaCm"), Numbers({Candidate.PlayAreaCm.X, Candidate.PlayAreaCm.Y}));
	Json->SetNumberField(TEXT("positionSpreadMm"), Candidate.Capture.PositionSpreadMm);
	Json->SetNumberField(TEXT("yawSpreadDeg"), Candidate.Capture.YawSpreadDeg);
	Json->SetNumberField(TEXT("usedFrames"), Candidate.Capture.UsedFrames);
	Json->SetNumberField(TEXT("discardedFrames"), Candidate.Capture.DiscardedFrames);
	FString Text;
	const FString CandidatePath = Path + TEXT(".candidate");
	if (!FJsonSerializer::Serialize(Json, TJsonWriterFactory<>::Create(&Text)) ||
		!IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true) ||
		!FFileHelper::SaveStringToFile(Text, *CandidatePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{ Reason = TEXT("candidate file write failed; previous save retained"); return false; }
	FVBoRBuckSavedAlignment Verified;
	// IFileManager::Move 는 기존 파일부터 삭제함. 검산된 후보를 원자 교체함 (Win64, P0-c §3-7)
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
