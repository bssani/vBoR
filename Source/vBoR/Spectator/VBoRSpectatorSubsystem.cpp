#include "VBoRSpectatorSubsystem.h"
#include "../Interaction/VBoRSpectatorPart.h"
#include "../Interaction/VBoRScenarioRunner.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Level.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Brushes/SlateNoResource.h"
#include "IHeadMountedDisplay.h"
#include "ISpectatorScreenController.h"
#include "IXRTrackingSystem.h"
#include "Misc/App.h"
#include "Slate/SceneViewport.h"
#include "Slate/WidgetRenderer.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateTypes.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SWindow.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
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
		// VR GetSizeXY may be eye render size; use actual window client size (P5h D3)
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
	// No renderer/Slate/target in nullrhi (P5h E1)
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
	if (!ScreenLayout.Update(SpectatorWindowSize(GetWorld()),EyeAspect,MinPanelWidth)) { return false; }
	SlateWidget = MakeWidget();
	WidgetRenderer = new FWidgetRenderer(true);
	RenderTarget = FWidgetRenderer::CreateTargetFor(FVector2D(ScreenLayout.PanelPixels),TF_Bilinear,false);
	if (!RenderTarget) { Stop(); return false; }
	RenderTarget->ClearColor = BackgroundColor;
	Screen->SetSpectatorScreenModeTexturePlusEyeLayout(ScreenLayout.Layout);
	Screen->SetSpectatorScreenTexture(RenderTarget);
	Screen->SetSpectatorScreenMode(ESpectatorScreenMode::TexturePlusEye);
	bPending = false;
	RedrawCountdown = 0;
	return true;
}

void UVBoRSpectatorSubsystem::AddPart(AActor* Actor)
{
	if (Actor && Cast<IVBoRSpectatorPart>(Actor)) { Parts.AddUnique(Actor); }
}

void UVBoRSpectatorSubsystem::CollectParts()
{
	bPartsCollected = true;
#if WITH_DEV_AUTOMATION_TESTS
	++PartScans;
#endif
	for (TActorIterator<AActor> It(GetWorld()); It; ++It) { AddPart(*It); }
	// UE 5.6 spawn notification follows FinishSpawning (deferred names/state ready too, LevelActor.cpp:51-55)
	SpawnedHandle = GetWorld()->AddOnActorSpawnedHandler(FOnActorSpawned::FDelegate::CreateUObject(this,&UVBoRSpectatorSubsystem::AddPart));
	// Include later sublevel/streamed parts too (no spawn notification)
	LevelAddedHandle = FWorldDelegates::LevelAddedToWorld.AddUObject(this,&UVBoRSpectatorSubsystem::HandleLevelAdded);
}

void UVBoRSpectatorSubsystem::HandleLevelAdded(ULevel* Level, UWorld* World)
{
	// Global notification: ignore other-world levels
	if (!Level || World != GetWorld()) { return; }
	for (AActor* Actor : Level->Actors) { AddPart(Actor); }
}

void UVBoRSpectatorSubsystem::SampleParts(double NowSeconds)
{
	if (!bPartsCollected) { CollectParts(); }
	// Remove deleted Actors, preserve order (stable same-frame grabbed-name order)
	Parts.RemoveAll([](const TWeakObjectPtr<AActor>& Part) { return !Part.IsValid(); });
	TArray<FString> ActiveNames;
	for (const TWeakObjectPtr<AActor>& Actor : Parts)
	{
		const IVBoRSpectatorPart* Part = Cast<IVBoRSpectatorPart>(Actor.Get());
		if (Part && Part->IsSpectatorInteracting()) { ActiveNames.AddUnique(Part->GetSpectatorName().ToString()); }
	}
	Names.Sample(ActiveNames,NowSeconds,NameHoldSeconds);
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
		// Flat run without HMD also ends without warning (P5h D1/E1)
		if (bPending && PendingSeconds >= 30) { bPending = false; }
	}
	if (!RenderTarget) { return; }
	ISpectatorScreenController* Screen = SpectatorScreen();
	if (!Screen || !Runner.IsValid()) { Stop(); return; }
	// Collect one-frame buttons regardless of draw interval (P5h E10)
	SampleParts(GetWorld()->GetRealTimeSeconds());
	CurrentBanner = VBoRSpectator::Banner(*Runner.Get(),{ReadyLabel,MissionLabel,FreeLabel,CompletedMission,ExploreMission,UnknownMission});
	RedrawCountdown -= DeltaTime;
	if (RedrawCountdown > 0) { return; }
	const float Interval = 1.f / FMath::Max(1.f,RedrawsPerSecond);
	RedrawCountdown = Interval;
	const FIntPoint WindowSize = SpectatorWindowSize(GetWorld());
	if (WindowSize.X <= 0 || WindowSize.Y <= 0) { return; }
	if (ScreenLayout.Update(WindowSize,EyeAspect,MinPanelWidth))
	{
		Screen->SetSpectatorScreenModeTexturePlusEyeLayout(ScreenLayout.Layout);
		if (RenderTarget->SizeX != ScreenLayout.PanelPixels.X || RenderTarget->SizeY != ScreenLayout.PanelPixels.Y)
		{ RenderTarget->ResizeTarget(ScreenLayout.PanelPixels.X,ScreenLayout.PanelPixels.Y); }
	}
	// Target in panel px, Slate layout remains 1080-based (P5k D2)
	WidgetRenderer->DrawWidget(RenderTarget,SlateWidget.ToSharedRef(),ScreenLayout.DrawScale,FVector2D(ScreenLayout.PanelPixels),Interval);
}

TSharedRef<SWidget> UVBoRSpectatorSubsystem::MakeWidget()
{
	static const FSlateRoundedBoxBrush Rounded(FLinearColor::White,4.f);
	static const FProgressBarStyle ProgressStyle = FProgressBarStyle()
		.SetBackgroundImage(FSlateNoResource()).SetFillImage(Rounded);
	const auto HeadingFont = [this] {
		FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle("Regular",HeadingFontSize);
		Font.LetterSpacing = 80; return Font;
	};
	const auto Interacting = SNew(SVerticalBox)
		.Visibility_Lambda([this] { return Names.RecentNames().IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible; })
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(STextBlock).Text_Lambda([this] { return InteractingPrefix; }).Font_Lambda(HeadingFont)
			.ColorAndOpacity_Lambda([this] { return FSlateColor(SecondaryTextColor); })
		];
	for (int32 Index = 0; Index < 2; ++Index)
	{
		Interacting->AddSlot().AutoHeight().Padding(0,8,0,0)
		[
			SNew(STextBlock).Text_Lambda([this,Index] {
				const auto Recent = Names.RecentNames();
				return Recent.IsValidIndex(Index) ? FText::FromString(Recent[Index]) : FText::GetEmpty(); })
			.Visibility_Lambda([this,Index] { return Names.RecentNames().IsValidIndex(Index) ? EVisibility::Visible : EVisibility::Collapsed; })
			.Font_Lambda([this] { return FCoreStyle::GetDefaultFontStyle("Regular",PartFontSize); })
			.ColorAndOpacity_Lambda([this] { return FSlateColor(TextColor); })
			.OverflowPolicy(ETextOverflowPolicy::Ellipsis)
		];
	}
	return SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
	.BorderBackgroundColor_Lambda([this] { return BackgroundColor; }).Padding(0)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth()
		[
			SNew(SBox).WidthOverride(2)
			[
				SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).Padding(0)
				.BorderBackgroundColor_Lambda([this] { return LineColor; })
			]
		]
		+ SHorizontalBox::Slot().FillWidth(1).Padding(32,40)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(STextBlock).Text_Lambda([this] { return CurrentBanner.Label; })
				.Font_Lambda([this] { return FCoreStyle::GetDefaultFontStyle("Bold",LabelFontSize); })
				.ColorAndOpacity_Lambda([this] { return FSlateColor(AccentColor); })
				// Explicit width for wrapped height from first prepass (P5k D3/Q1)
				.WrapTextAt_Lambda([this] { return FMath::Max(1.0,ScreenLayout.DesignSize().X-66); })
			]
			+ SVerticalBox::Slot().FillHeight(1).Padding(0,24,0,0)
			[
				SNew(STextBlock).Text_Lambda([this] { return CurrentBanner.Mission; })
				.Font_Lambda([this] { return FCoreStyle::GetDefaultFontStyle("Regular",MissionFontSize); })
				.ColorAndOpacity_Lambda([this] { return FSlateColor(TextColor); })
				.WrapTextAt_Lambda([this] { return FMath::Max(1.0,ScreenLayout.DesignSize().X-66); })
				.OverflowPolicy(ETextOverflowPolicy::Ellipsis).Clipping(EWidgetClipping::ClipToBounds)
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SVerticalBox).Visibility_Lambda([this] { return CurrentBanner.bShowProgress ? EVisibility::Visible : EVisibility::Collapsed; })
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(STextBlock).Text(FText::FromString(TEXT("PROGRESS"))).Font_Lambda(HeadingFont)
					.ColorAndOpacity_Lambda([this] { return FSlateColor(SecondaryTextColor); })
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0,12,0,0)
				[
					SNew(SBox).HeightOverride(12)
					[
						SNew(SBorder).BorderImage(&Rounded).Padding(0)
						.BorderBackgroundColor_Lambda([this] { return ProgressBackgroundColor; })
						[
							SNew(SProgressBar).Style(&ProgressStyle).BarFillStyle(EProgressBarFillStyle::Scale)
							.Percent_Lambda([this] { return TOptional<float>(CurrentBanner.Progress()); })
							.FillColorAndOpacity_Lambda([this] { return FSlateColor(AccentColor); })
						]
					]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0,12,0,0)
				[
					SNew(STextBlock).Text_Lambda([this] { return FText::FromString(FString::Printf(TEXT("%d of %d complete"),CurrentBanner.Completed,CurrentBanner.Total)); })
					.Font_Lambda([this] { return FCoreStyle::GetDefaultFontStyle("Regular",ProgressFontSize); })
					.ColorAndOpacity_Lambda([this] { return FSlateColor(SecondaryTextColor); })
				]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0,32,0,0)
			[
				SNew(SBox).HeightOverride(2)
				[
					SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).Padding(0)
					.BorderBackgroundColor_Lambda([this] { return LineColor; })
				]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0,32,0,0)[Interacting]
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
	// Defer release of render-thread Slate resources (P5h D1)
	if (WidgetRenderer) { BeginCleanup(WidgetRenderer); WidgetRenderer = nullptr; }
	SlateWidget.Reset(); RenderTarget = nullptr; Runner.Reset();
	bPending = false;
}

void UVBoRSpectatorSubsystem::Deinitialize()
{
	Stop();
	if (UWorld* World = GetWorld()) { World->RemoveOnActorSpawnedHandler(SpawnedHandle); }
	FWorldDelegates::LevelAddedToWorld.Remove(LevelAddedHandle);
	Parts.Reset(); bPartsCollected = false;
	Super::Deinitialize();
}
