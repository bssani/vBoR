#include "VBoRSpectatorSubsystem.h"
#include "../Interaction/VBoRSpectatorPart.h"
#include "../Interaction/VBoRScenarioRunner.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "IHeadMountedDisplay.h"
#include "ISpectatorScreenController.h"
#include "IXRTrackingSystem.h"
#include "Misc/App.h"
#include "Slate/SceneViewport.h"
#include "Slate/WidgetRenderer.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SConstraintCanvas.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SWindow.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
	const FVector2D DrawSize(1920,1080);
	ISpectatorScreenController* SpectatorScreen(bool bRequireActive = true)
	{
		if (!FApp::CanEverRender() || !GEngine || !GEngine->XRSystem.IsValid()) { return nullptr; }
		IHeadMountedDisplay* HMD = GEngine->XRSystem->GetHMDDevice();
		return HMD && (!bRequireActive || (GEngine->IsStereoscopic3D() && HMD->IsHMDConnected() && HMD->IsHMDEnabled()))
			? HMD->GetSpectatorScreenController() : nullptr;
	}
	FIntPoint SpectatorWindowSize(UWorld* World)
	{
		UGameViewportClient* Client = World ? World->GetGameViewport() : nullptr;
		FSceneViewport* Viewport = Client ? Client->GetGameViewport() : nullptr;
		// VR 의 GetSizeXY 는 눈 렌더 크기일 수 있어 실제 창의 client 크기 사용 (P5h D3)
		const TSharedPtr<SWindow> Window = Viewport ? Viewport->FindWindow() : nullptr;
		if (!Window) { return FIntPoint::ZeroValue; }
		const FVector2D Size = Window->GetClientSizeInScreen();
		return FIntPoint(FMath::RoundToInt(Size.X),FMath::RoundToInt(Size.Y));
	}
}

bool UVBoRSpectatorSubsystem::DoesSupportWorldType(EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UVBoRSpectatorSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	// nullrhi 에서는 renderer·Slate·target 을 만들지 않음 (P5h E1)
	bPending = FApp::CanEverRender();
	PendingSeconds = RetryCountdown = 0;
}

bool UVBoRSpectatorSubsystem::TryStart()
{
	ISpectatorScreenController* Screen = SpectatorScreen();
	if (!Screen || !FSlateApplication::IsInitialized() || !GetWorld()->GetFirstPlayerController()) { return false; }
	for (TActorIterator<AVBoRScenarioRunner> It(GetWorld()); It; ++It) { Runner = *It; break; }
	if (!Runner.IsValid()) { return false; }
	ScreenLayout = FVBoRSpectatorLayout();
	if (!ScreenLayout.Update(SpectatorWindowSize(GetWorld()),EyeAspect,BandHeight)) { return false; }
	SlateWidget = MakeWidget();
	WidgetRenderer = new FWidgetRenderer(true);
	RenderTarget = FWidgetRenderer::CreateTargetFor(DrawSize,TF_Bilinear,false);
	if (!RenderTarget) { Stop(); return false; }
	RenderTarget->ClearColor = FLinearColor::Transparent;
	Screen->SetSpectatorScreenModeTexturePlusEyeLayout(ScreenLayout.Layout);
	Screen->SetSpectatorScreenTexture(RenderTarget);
	Screen->SetSpectatorScreenMode(ESpectatorScreenMode::TexturePlusEye);
	bPending = false;
	RedrawCountdown = 0;
	return true;
}

void UVBoRSpectatorSubsystem::SampleParts(double NowSeconds)
{
	TArray<FString> ActiveNames;
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		const IVBoRSpectatorPart* Part = Cast<IVBoRSpectatorPart>(*It);
		if (Part && Part->IsSpectatorInteracting()) { ActiveNames.AddUnique(Part->GetSpectatorName().ToString()); }
	}
	Names.Sample(ActiveNames,NowSeconds,NameHoldSeconds);
	PartLine = Names.Line(InteractingPrefix.ToString());
}

void UVBoRSpectatorSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (bPending)
	{
		PendingSeconds += DeltaTime; RetryCountdown -= DeltaTime;
		if (RetryCountdown <= 0)
		{
			RetryCountdown = 0.5f;
			TryStart();
		}
		// HMD 가 없는 평면 실행도 경고 없이 끝남 (P5h D1·E1)
		if (bPending && PendingSeconds >= 30) { bPending = false; }
	}
	if (!RenderTarget) { return; }
	ISpectatorScreenController* Screen = SpectatorScreen();
	if (!Screen || !Runner.IsValid()) { Stop(); return; }
	// 그리기 간격과 무관하게 1 frame 버튼도 수집함 (P5h E10)
	SampleParts(GetWorld()->GetRealTimeSeconds());
	CurrentBanner = VBoRSpectator::Banner(*Runner.Get(),{ReadyLabel,MissionLabel,FreeLabel,CompletedMission,ExploreMission,UnknownMission});
	RedrawCountdown -= DeltaTime;
	if (RedrawCountdown > 0) { return; }
	const float Interval = 1.f / FMath::Max(1.f,RedrawsPerSecond);
	RedrawCountdown = Interval;
	if (ScreenLayout.Update(SpectatorWindowSize(GetWorld()),EyeAspect,BandHeight))
	{ Screen->SetSpectatorScreenModeTexturePlusEyeLayout(ScreenLayout.Layout); }
	WidgetRenderer->DrawWidget(RenderTarget,SlateWidget.ToSharedRef(),DrawSize,Interval);
}

TSharedRef<SWidget> UVBoRSpectatorSubsystem::MakeWidget()
{
	return SNew(SConstraintCanvas)
	+ SConstraintCanvas::Slot().Offset_Lambda([this] {
		const float Height = FMath::Clamp(BandHeight,0.01f,0.99f);
		return FMargin(0,DrawSize.Y*(1-Height),DrawSize.X,DrawSize.Y*Height); })
	[
		SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
		.BorderBackgroundColor_Lambda([this] { return BackgroundColor; }).Padding(FMargin(32,12))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().FillHeight(1)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(0.7f).Padding(0,0,24,0)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(STextBlock).Text_Lambda([this] { return CurrentBanner.Label; })
						.Font_Lambda([this] { return FCoreStyle::GetDefaultFontStyle("Bold",LabelFontSize); })
						.ColorAndOpacity_Lambda([this] { return FSlateColor(AccentColor); })
					]
					+ SVerticalBox::Slot().FillHeight(1)
					[
						SNew(STextBlock).Text_Lambda([this] { return CurrentBanner.Mission; }).AutoWrapText(true)
						.Font_Lambda([this] { return FCoreStyle::GetDefaultFontStyle("Regular",MissionFontSize); })
						.ColorAndOpacity_Lambda([this] { return FSlateColor(TextColor); })
					]
				]
				+ SHorizontalBox::Slot().FillWidth(0.3f).VAlign(VAlign_Center)
				[
					SNew(STextBlock).Text_Lambda([this] { return FText::FromString(PartLine); }).AutoWrapText(true)
					.Visibility_Lambda([this] { return PartLine.IsEmpty() ? EVisibility::Hidden : EVisibility::Visible; })
					.Font_Lambda([this] { return FCoreStyle::GetDefaultFontStyle("Regular",PartFontSize); })
					.ColorAndOpacity_Lambda([this] { return FSlateColor(TextColor); })
				]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0,4,0,0)
			[
				SNew(SHorizontalBox).Visibility_Lambda([this] { return CurrentBanner.bShowProgress ? EVisibility::Visible : EVisibility::Hidden; })
				+ SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)
				[
					SNew(SBox).HeightOverride(10)
					[
						SNew(SProgressBar).Percent_Lambda([this] { return TOptional<float>(CurrentBanner.Progress()); })
						.FillColorAndOpacity_Lambda([this] { return FSlateColor(AccentColor); })
					]
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(20,0,0,0)
				[
					SNew(STextBlock).Text_Lambda([this] { return FText::FromString(FString::Printf(TEXT("%d / %d"),CurrentBanner.Completed,CurrentBanner.Total)); })
					.Font(FCoreStyle::GetDefaultFontStyle("Regular",24))
					.ColorAndOpacity_Lambda([this] { return FSlateColor(TextColor); })
				]
			]
		]
	];
}

void UVBoRSpectatorSubsystem::Stop()
{
	if (RenderTarget)
	{
		if (ISpectatorScreenController* Screen = SpectatorScreen(false))
		{
			Screen->SetSpectatorScreenTexture(nullptr);
			Screen->SetSpectatorScreenMode(ESpectatorScreenMode::SingleEyeCroppedToFill);
		}
	}
	// 렌더 스레드가 쓰는 Slate 자원은 지연 정리함 (P5h D1)
	if (WidgetRenderer) { BeginCleanup(WidgetRenderer); WidgetRenderer = nullptr; }
	SlateWidget.Reset(); RenderTarget = nullptr; Runner.Reset();
	bPending = false;
}

void UVBoRSpectatorSubsystem::Deinitialize()
{
	Stop();
	Super::Deinitialize();
}
