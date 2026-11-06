#include "knightOperationMapCheckCommandlet.h"
#include "Menu/UI/KnightMainMenuWidget.h"
#include "Menu/UI/KnightStageMapWidget.h"
#include "Menu/UI/KnightQuestMenuWidget.h"
#include "Menu/UI/KnightOperationMapPresentation.h"
#include "Menu/UI/KnightOperationMapStateSubsystem.h"
#include "Menu/UI/KnightButtonFillExtension.h"
#include "Menu/KnightMenuGameMode.h"
#include "GameFlow/KnightGameFlowSubsystem.h"
#include "Quest/KnightQuestData.h"
#include "Progression/knightProgressionSubsystem.h"
#include "Progression/knightSaveSubsystem.h"
#include "knightItemDefinition.h"
#include "Blueprint/WidgetTree.h"
#include "CommonButtonBase.h"
#include "CommonTextBlock.h"
#include "CommonGameViewportClient.h"
#include "Widgets/Layout/SBorder.h"
#include "Styling/CoreStyle.h"
#include "Components/VerticalBox.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/ScrollBox.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/UserInterfaceSettings.h"
#include "GameFramework/PlayerController.h"
#include "Framework/Application/SlateApplication.h"
#include "Interfaces/ISlateRHIRendererModule.h"
#include "ShaderCompiler.h"
#include "Engine/Texture2D.h"
#include "TextureCompiler.h"
#include "UObject/UObjectIterator.h"
#include "Slate/WidgetRenderer.h"
#include "Input/HittestGrid.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/Material.h"
#include "MaterialShared.h"
#include "RenderingThread.h"
#include "Async/TaskGraphInterfaces.h"

int32 UknightOperationMapCheckCommandlet::Main(const FString& Params)
{
	FString Namespace, Out, UserDir;
	if (!FParse::Value(FCommandLine::Get(), TEXT("KnightSaveTestNamespace="), Namespace) || !Namespace.StartsWith(TEXT("OpMap")) || Namespace.Len() > 32
		|| !FParse::Value(FCommandLine::Get(), TEXT("UserDir="), UserDir) || UserDir.IsEmpty()
		|| !FParse::Value(*Params, TEXT("Out="), Out) || !FParse::Param(FCommandLine::Get(), TEXT("AllowCommandletRendering"))) return 1;
	IFileManager::Get().MakeDirectory(*Out, true);
	if (!FSlateApplication::IsInitialized())
	{
		FSlateApplication::Create();
		FSlateApplication::Get().InitializeRenderer(FModuleManager::LoadModuleChecked<ISlateRHIRendererModule>(TEXT("SlateRHIRenderer")).CreateSlateRHIRenderer());
	}
	int32 Failures = 0, Checks = 0;
	auto Check = [&](bool Pass, const TCHAR* What)
	{
		++Checks; if (!Pass) ++Failures;
		UE_LOG(LogTemp, Display, TEXT("OperationMap %s %s"), Pass ? TEXT("PASS") : TEXT("FAIL"), What);
	};
	UGameInstance* GI = NewObject<UGameInstance>(GEngine); GI->AddToRoot(); GI->InitializeStandalone();
	UWorld* World = GI->GetWorld();
	TGuardValue<TSubclassOf<UGameViewportClient>> ViewportClass(GEngine->GameViewportClientClass, UCommonGameViewportClient::StaticClass());
    // Offscreen Slate has no OS cursor. Keep this fixture-only override in memory;
    // normal viewport cursor settings and project configuration are unchanged.
    auto* UISettings = GetMutableDefault<UUserInterfaceSettings>();
    TGuardValue<decltype(UISettings->HardwareCursors)> CursorGuard(UISettings->HardwareCursors, {});
	UGameViewportClient* Viewport = NewObject<UCommonGameViewportClient>(GEngine);
	GI->GetWorldContext()->GameViewport = Viewport; Viewport->Init(*GI->GetWorldContext(), GI, false);
	ULocalPlayer* LocalPlayer = NewObject<ULocalPlayer>(GEngine); GI->AddLocalPlayer(LocalPlayer, FPlatformUserId::CreateFromInternalId(0));
	APlayerController* Player = World->SpawnActor<APlayerController>(); World->AddController(Player); Player->SetPlayer(LocalPlayer);
	auto* Saves = GI->GetSubsystem<UknightSaveSubsystem>(); auto* Progress = GI->GetSubsystem<UknightProgressionSubsystem>();
	auto* Flow = GI->GetSubsystem<UKnightGameFlowSubsystem>();
	const FString ProfileA = TEXT("MapA") + FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(10);
	const FString ProfileB = TEXT("MapB") + FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(10);
	if (!Saves || !Progress || !Flow || !Saves->SelectSlot(ProfileA)) return 1;
	auto DrainSave = [&]()
	{
		const double Deadline = FPlatformTime::Seconds() + 15.;
		while (!Saves->CanStartAtomicTransaction() && FPlatformTime::Seconds() < Deadline)
		{
			FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread);
			FTSTicker::GetCoreTicker().Tick(.01f); FPlatformProcess::Sleep(.01f);
		}
		Check(Saves->CanStartAtomicTransaction(), TEXT("isolated normal async save completed"));
	};
	// Use the real menu GameMode initialization hook, not a fabricated flow state.
	const auto ModeClass = LoadClass<AKnightMenuGameMode>(nullptr, TEXT("/Game/Knight/Blueprints/BP_GM_MainMenu.BP_GM_MainMenu_C"));
	auto* Mode = ModeClass ? World->SpawnActor<AKnightMenuGameMode>(ModeClass) : nullptr;
	if (!Mode) return 1;
	FString Error; Mode->InitGame(TEXT("OperationMapFixture"), FString(), Error);
	Check(Flow->GetCurrentFlowState() == EKnightFlowState::MainMenu, TEXT("real menu GameMode registers MainMenu"));
	const auto* Catalog = UKnightQuestSettings::LoadCatalog(); if (!Catalog) return 1;
	const auto Class = LoadClass<UKnightMainMenuWidget>(nullptr, TEXT("/Game/Knight/UI/Menu/WBP_KnightMainMenu.WBP_KnightMainMenu_C"));
	UKnightMainMenuWidget* Menu = Class ? CreateWidget<UKnightMainMenuWidget>(Player, Class) : nullptr;
	if (!Menu) return 1;
	Menu->AddToRoot();
    // Render linear HDR values and encode sRGB only during readback. The default
    // 8-bit WidgetRenderer target applies display gamma twice in this offscreen setup.
    FWidgetRenderer* Renderer = new FWidgetRenderer(false);
    auto MakeCaptureTarget = [](FIntPoint Extent)
    {
        auto* Result = NewObject<UTextureRenderTarget2D>(); Result->ClearColor=FLinearColor::Transparent;
        Result->TargetGamma=1.f; Result->InitCustomFormat(Extent.X,Extent.Y,PF_FloatRGBA,true); Result->UpdateResourceImmediate(true);
        return Result;
    };
    auto* Probe = MakeCaptureTarget(FIntPoint(16,16));
    Renderer->DrawWidget(Probe, SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
        .BorderBackgroundColor(FLinearColor(FColor(9,20,31))),FVector2D(16,16),0.f); FlushRenderingCommands();
    TArray<FLinearColor> ProbePixels;
    Probe->GameThread_GetRenderTargetResource()->ReadLinearColorPixels(ProbePixels,FReadSurfaceDataFlags(RCM_MinMax));
    const FColor ProbeColor=ProbePixels[128].ToFColor(true);
    UE_LOG(LogTemp,Display,TEXT("OperationMap CAPTURE linear float + sRGB readback: sample=%d,%d,%d expected=9,20,31"),ProbeColor.R,ProbeColor.G,ProbeColor.B);
    Check(FMath::Abs(int32(ProbeColor.R)-9)+FMath::Abs(int32(ProbeColor.G)-20)+FMath::Abs(int32(ProbeColor.B)-31)<=6,
        TEXT("offscreen Slate colour calibration agrees with authored sRGB colour"));
	TSharedRef<SVirtualWindow> Window = SNew(SVirtualWindow);
	Window->SetContent(Menu->TakeWidget()); FSlateApplication::Get().RegisterVirtualWindow(Window);
	FHittestGrid Grid; UTextureRenderTarget2D* Target = nullptr; FIntPoint Size; float Scale = 1.f;
	auto Draw = [&](float Dt = 0.f)
	{
		++GFrameCounter;
		Renderer->DrawWindow(Target, Grid, Window, Scale, FVector2D(Size), Dt); FlushRenderingCommands();
	};
	auto SetSize = [&](FIntPoint NewSize, float ExtraScale = 1.f)
	{
		if (Target) Target->RemoveFromRoot(); Size = NewSize;
		Scale = GetDefault<UUserInterfaceSettings>()->GetDPIScaleBasedOnSize(Size) * ExtraScale;
		Window->Resize(FVector2D(Size)); Target = MakeCaptureTarget(Size); Target->AddToRoot();
		Draw(); Draw();
	};
    TSet<UTexture2D*> PreparedTextures;
	auto Capture = [&](const FString& Name)
	{
        FTextureCompilingManager::Get().FinishAllCompilation();
        for (TObjectIterator<UTexture2D> It; It; ++It)
            if (It->GetPathName().StartsWith(TEXT("/Game/Knight/Items/Materials/")) && !PreparedTextures.Contains(*It))
            { It->SetForceMipLevelsToBeResident(300.f); It->WaitForStreaming(); PreparedTextures.Add(*It); }
        Draw(); Draw();
        // FColor HDR ReadPixels normalizes each channel against the brightest
        // highlight in the frame. Raw linear readback avoids that colour shift.
        TArray<FLinearColor> LinearPixels; TArray<FColor> Pixels;
        Check(Target->GameThread_GetRenderTargetResource()->ReadLinearColorPixels(LinearPixels,FReadSurfaceDataFlags(RCM_MinMax)), TEXT("real Slate/RHI pixels read"));
        Pixels.Reserve(LinearPixels.Num()); for (const auto& Pixel : LinearPixels) Pixels.Add(Pixel.ToFColor(true));
		TArray64<uint8> Png; FImageUtils::PNGCompressImageArray(Size.X, Size.Y, Pixels, Png);
		Check(FFileHelper::SaveArrayToFile(Png, *(Out / (Name + TEXT(".png")))), TEXT("render saved"));
	};
	SetSize(FIntPoint(1920,1080)); Menu->ShowPage(EKnightMainMenuPage::Stage); Draw(); Draw();
	UKnightStageMapWidget* Map = Menu->Map_Stages; UKnightQuestMenuWidget* Panel = Menu->QuestWidget;
	if (!Map || !Panel || !Map->MapMID) return 1;
	GShaderCompilingManager->FinishAllCompilation(); FTextureCompilingManager::Get().FinishAllCompilation();
	const FMaterialResource* MapResource = Map->MapMaterial->GetMaterial()->GetMaterialResource(GMaxRHIShaderPlatform);
	Check(MapResource && MapResource->GetCompileErrors().IsEmpty() && MapResource->GetGameThreadShaderMap(), TEXT("final operation-map material has a compiled shader map and zero compile errors"));
	for (TObjectIterator<UTexture2D> It; It; ++It) if (It->GetPathName().StartsWith(TEXT("/Game/Knight/")))
	{
		It->SetForceMipLevelsToBeResident(60.f); It->WaitForStreaming();
	}
	Draw(); Draw();
	const int64 InitialMoney = Progress->GetMoney(); TArray<FknightItemStack> InitialItems; Progress->GetOwnedItems(InitialItems);
	const FName Q1(TEXT("Quest_0001")), Q2(TEXT("Quest_0002")), Q3(TEXT("Quest_0003"));
	const FName R1 = Menu->QuestRegionById.FindRef(Q1), R2 = Menu->QuestRegionById.FindRef(Q2), R3 = Menu->QuestRegionById.FindRef(Q3);
	Check(!R1.IsNone() && !R2.IsNone() && R3 == TEXT("Region_Heukgwan"), TEXT("authoritative region mapping including south storage"));
	Check(Map->Markers.Num() == 3 && !Map->Markers.ContainsByPredicate([](const FKnightMapMarker& M) { return M.StageId == TEXT("Stage_0003"); }), TEXT("three existing regions; no duplicate Raid03"));
	auto State = [&](FName Quest) { return KnightOperationMapPresentation::Query(Catalog, Catalog->FindQuest(Quest), GI); };
	Check(State(Q1).bCanLaunch && !State(Q1).bCompleted, TEXT("fresh profile first quest available without fabricated clear"));
	Check(State(Q2).bProgressionLocked && !State(Q2).bCanLaunch && State(Q3).bProgressionLocked, TEXT("fresh profile prerequisite and map exploration locks"));
	for (const UKnightQuestDefinition* Quest : Catalog->Quests)
	{
		if (!Quest) continue; const auto S = State(Quest->QuestId);
		UE_LOG(LogTemp, Display, TEXT("OperationMap DATA %s region=%s ready=%d locked=%d completed=%d status=%s reason=%s"), *Quest->QuestId.ToString(),
			*Menu->QuestRegionById.FindRef(Quest->QuestId).ToString(), S.bCanLaunch, S.bProgressionLocked, S.bCompleted, *S.Availability.ToString(), *S.Reason.ToString());
		if (Quest->Difficulty == EKnightQuestDifficulty::Hard) Check(!S.bCanLaunch && !S.bProgressionLocked, TEXT("unfinished hard content is unavailable, not mislabeled progression-locked"));
	}
	// Never travel from this renderer fixture. Observe the existing launch delegate instead.
	Panel->OnLaunchRequested.RemoveAll(Menu); int32 Launches = 0; FName Launched;
	Panel->OnLaunchRequested.AddLambda([&](FName Quest) { ++Launches; Launched = Quest; });
	Menu->SelectQuest(Q1); Draw(.20f); Draw(); Capture(TEXT("Fresh_Ready"));
	Panel->HandleLaunchClicked(); Check(Launches == 1 && Launched == Q1, TEXT("settled displayed quest action has matching authoritative target"));
	Menu->SelectQuest(Q2); Capture(TEXT("Sweep_00"));
	const FVector2D PanelSize = Panel->GetCachedGeometry().GetLocalSize();
	for (int32 Frame = 1; Frame <= 24; ++Frame)
	{
		Draw(1.f / 60.f);
		if (Frame <= 10) { Panel->HandleLaunchClicked(); Check(Launches == 1, TEXT("launch blocked throughout information transition")); }
		Check(Panel->GetCachedGeometry().GetLocalSize().Equals(PanelSize, .1f), TEXT("panel geometry remains stable"));
		Capture(FString::Printf(TEXT("Sweep_%02d"), Frame));
	}
	Check(!Panel->IsPanelChanging() && Panel->DisplayedQuestId == Q2 && !Panel->LaunchButton->GetIsEnabled(), TEXT("latest locked quest displayed and not launchable"));
	Check(Map->SelectionWeights.FindRef(R1) == 0.f && Map->SelectionWeights.FindRef(R2) == 1.f, TEXT("previous region fully settles to normal"));
	const float SelectedAt = Map->SelectTime; Menu->SelectQuest(Q2); Draw(.1f); Menu->HandleQuestProgressChanged(); Draw();
	Check(Map->SelectTime == SelectedAt, TEXT("same region click and data refresh do not restart sweep"));
	Menu->SelectQuest(Q1); Draw(.025f); Menu->SelectQuest(Q2); Draw(.025f); Menu->SelectQuest(Q3); Draw(.2f); Draw();
	Check(Panel->ActiveRegionId == R3 && Panel->DisplayedQuestId == Q3 && !Panel->IsPanelChanging(), TEXT("rapid changes resolve to latest region and quest together"));
	Capture(TEXT("Fresh_ExplorationLocked"));
	Check(!Panel->CurrencyText && Panel->RewardIcons.IsEmpty(), TEXT("map exploration does not invent currency or reward items"));
    {
        auto* Button = Panel->LaunchButton.Get();
        const auto* Fill = Button->GetExtension<UKnightButtonFillExtension>();
        Check(!Fill || !Fill->IsEnabled(), TEXT("launch uses static operation-map feedback"));
        int32 ScrollCount=0, ButtonCount=0;
        Panel->WidgetTree->ForEachWidget([&](UWidget* Widget) { ScrollCount += Widget->IsA<UScrollBox>(); ButtonCount += Widget->IsA<UCommonButtonBase>(); });
        Check(ScrollCount==1 && ButtonCount==1, TEXT("one rewards scroller and launch only; no mission selector or stale focus targets"));
    }
	Progress->RecordQuestCompleted(Q1); Draw();
	Check(State(Q1).bCompleted && State(Q1).bCanLaunch && State(Q2).bCanLaunch, TEXT("normal clear history remains separate from replay and next unlock"));
	Menu->SelectQuest(Q2); Draw(.4f); Draw(); Capture(TEXT("NormalClear_NextReady"));
	Progress->RecordQuestCompleted(Q2); Draw();
	Check(State(Q2).bCompleted && State(Q2).bCanLaunch && State(Q3).bCanLaunch && !State(Q3).bCompleted, TEXT("Janhyang normal opens exploration without fabricating exploration clear"));
	Menu->SelectQuest(Q3); Draw(.4f); Draw(); Capture(TEXT("ExplorationReady"));
	Check(Panel->DifficultyBadge->GetText().ToString() == TEXT("맵 탐색") && Panel->DetailAvailability->GetVisibility() == ESlateVisibility::Collapsed, TEXT("exploration mode identified without redundant readiness label"));
	Panel->HandleLaunchClicked(); Check(Launches == 2 && Launched == Q3, TEXT("exploration button routes the actual map-test quest, not an old combat selection"));
	// Real pointer handlers exercise panning and wheel zoom without a desktop/controller driver.
	const auto Geometry = Map->GetCachedGeometry(); const FVector2D P = Geometry.LocalToAbsolute(Geometry.GetLocalSize() * .5);
	FPointerEvent Down(0, P, P, TSet<FKey>{ EKeys::LeftMouseButton }, EKeys::LeftMouseButton, 0, FModifierKeysState());
	FPointerEvent Move(0, P + FVector2D(35,18), P, TSet<FKey>{ EKeys::LeftMouseButton }, EKeys::Invalid, 0, FModifierKeysState());
	FPointerEvent Up(0, P + FVector2D(35,18), P + FVector2D(35,18), TSet<FKey>(), EKeys::LeftMouseButton, 0, FModifierKeysState());
	const float DragSelectionTime = Map->SelectTime, LatitudeBefore = Map->VisibleLatitude; const FVector2D CenterBefore = Map->Center;
	Map->NativeOnMouseButtonDown(Geometry, Down); Map->NativeOnMouseMove(Geometry, Move); Map->NativeOnMouseButtonUp(Geometry, Up);
	FPointerEvent Wheel(0, P, P, TSet<FKey>(), EKeys::Invalid, 1, FModifierKeysState()); Map->NativeOnMouseWheel(Geometry, Wheel); Draw();
	Check(!Map->Center.Equals(CenterBefore) && Map->VisibleLatitude < LatitudeBefore && Map->SelectTime == DragSelectionTime, TEXT("drag and zoom work without restarting selection"));
	const FVector2D SavedCenter = Map->Center; const float SavedLatitude = Map->VisibleLatitude;
	Menu->ShowPage(EKnightMainMenuPage::Home); Menu->ShowPage(EKnightMainMenuPage::Stage); Draw();
	Check(Menu->SelectedStageId == R3 && Panel->GetSelectedQuestId() == Q3 && Map->Center.Equals(SavedCenter,.01) && Map->VisibleLatitude == SavedLatitude,
		TEXT("same profile reopening restores region difficulty and map view without sweep"));
	Check(Map->SelectedStageId == R3 && Map->SelectionWeights.FindRef(R3) == 1.f, TEXT("restoration retains single steady selection and resumes its loop"));
	DrainSave(); Check(Saves->SelectSlot(ProfileB), TEXT("switch to separate isolated profile")); Draw();
	Check(Menu->SelectedStageId.IsNone() && Panel->GetSelectedQuestId().IsNone() && !State(Q1).bCompleted && State(Q3).bProgressionLocked,
		TEXT("new profile gets no old selection or clear/unlock indicators"));
	Menu->SelectQuest(Q1); Draw(.4f); DrainSave(); Check(Saves->SelectSlot(ProfileA), TEXT("return to first isolated profile")); Draw();
	Check(Menu->SelectedStageId == R3 && Panel->GetSelectedQuestId() == Q3 && State(Q2).bCompleted && State(Q3).bCanLaunch,
		TEXT("first profile restores own state and actual progression"));
	for (const FIntPoint Resolution : { FIntPoint(1280,720), FIntPoint(2560,1440), FIntPoint(3440,1440) })
	{
		SetSize(Resolution);
		Map->RestoreView(FVector2D(0,0), 110.f); Menu->SelectQuest(Q2); Draw(.4f); Draw();
		const auto View = Map->MakeView(Map->GetCachedGeometry().GetLocalSize());
		const int32 Index = Map->Markers.IndexOfByPredicate([&](const FKnightMapMarker& M) { return M.StageId == R2; });
		Check(Index != INDEX_NONE && Map->FindMarkerAt(Map->ToLocal(Map->Markers[Index].Location, View), View.LocalSize) == Index,
			TEXT("painted region and hit detection agree at alternate DPI"));
		Check(Map->LeaderTarget.Get() == Panel, TEXT("connector keeps the actual details target"));
        const auto ScrollGeometry = Panel->RewardScroll->GetCachedGeometry();
        const auto LaunchGeometry = Panel->LaunchButton->GetCachedGeometry();
        const float ScrollBottom = ScrollGeometry.GetAbsolutePosition().Y + ScrollGeometry.GetAbsoluteSize().Y;
        Check(ScrollBottom <= LaunchGeometry.GetAbsolutePosition().Y && LaunchGeometry.GetAbsoluteSize().Y > 20.f, TEXT("reward scroll stays above fixed accessible launch button at every resolution"));
        const FName BeforeWheel = Panel->DisplayedQuestId;
        const FVector2D Cursor = ScrollGeometry.GetAbsolutePosition()+FVector2D(20,20);
        FPointerEvent RewardWheel(0,Cursor,Cursor,TSet<FKey>(),EKeys::Invalid,-1,FModifierKeysState());
        Panel->RewardScroll->TakeWidget()->OnMouseWheel(ScrollGeometry,RewardWheel); Draw();
        Check(Panel->DisplayedQuestId == BeforeWheel, TEXT("reward wheel scrolling never selects a difficulty"));
        Panel->RewardScroll->SetScrollOffset(0); Draw();
		Capture(FString::Printf(TEXT("Scale_%d"), Resolution.X));
	}
    // All map pins use the same source for heading, badge, reward definitions and launch target.
    SetSize(FIntPoint(1920,1080)); Map->RestoreView(FVector2D(0,0),110.f);
    for (const auto& Pin : Map->QuestMarkers)
    {
        Menu->SelectQuest(Pin.QuestId); Draw(.2f); Draw();
        const auto* Quest = Catalog->FindQuest(Pin.QuestId);
        Check(Quest && Panel->DisplayedQuestId == Pin.QuestId && Map->SelectedQuestId == Pin.QuestId, TEXT("every map mode synchronizes selected and displayed quest"));
        if (!Quest) continue;
        Check(Panel->LaunchButton->GetIsEnabled() == State(Pin.QuestId).bCanLaunch, TEXT("each selected mode retains authoritative launch restriction"));
        if (Quest->bMapExplorationOnly) continue;
        Check(Panel->CurrencyText && Panel->CurrencyText->GetText().ToString() == FString::Printf(TEXT("%dgold"),Quest->MoneyReward), TEXT("currency bound to selected quest without separators"));
        TArray<FknightRaidReward> Rewards = Quest->ClearRewards;
        for (const auto& Part : Quest->PartBreakRewards) Rewards.Add(Part.Reward);
        Check(Panel->RewardIcons.Num() == Rewards.Num(), TEXT("one row per actual quest reward; no raid-reward concatenation"));
        for (int32 I=0; I<Rewards.Num(); ++I)
        {
            auto* Texture = Rewards[I].Item ? Rewards[I].Item->Icon.LoadSynchronous() : nullptr;
            Check(Texture && Panel->RewardIcons[I]->GetBrush().GetResourceObject() == Texture, TEXT("row resolves actual material icon from item definition"));
            UE_LOG(LogTemp, Display, TEXT("OperationMap REWARD quest=%s item=%s icon=%s count=%d..%d chance=%g condition=%s shown=%s"),
                *Pin.QuestId.ToString(), *GetPathNameSafe(Rewards[I].Item), *GetPathNameSafe(Texture), Rewards[I].MinCount, Rewards[I].MaxCount,
                Rewards[I].Chance, *Rewards[I].RequiredBrokenPart.ToString(), *Panel->RewardValues[I]->GetText().ToString());
        }
    }
    Menu->SelectQuest(Q1); Draw(.2f); Draw();
    Check(Panel->DetailAvailability->GetVisibility()==ESlateVisibility::Collapsed, TEXT("available quest has no readiness/clear label"));
    for (const auto& Region : Map->Markers) Check(Region.ClearHistory.IsEmpty() && !Region.Availability.ToString().Contains(TEXT("출격 가능")), TEXT("redundant region status text removed, progression retained"));
    Capture(TEXT("Redesign_1080"));
    const float LoopOrigin = Map->SelectTime;
    for (int32 Frame=0; Frame<=195; ++Frame)
    {
        Draw(1.f/30.f);
        if (Frame==45 || Frame==110) { Menu->HandleQuestProgressChanged(); Map->SetHovered(1); }
        Capture(FString::Printf(TEXT("Loop_%03d"),Frame));
    }
    Check(Map->SelectTime==LoopOrigin && Map->Time-Map->SelectTime > 2.f*Map->LoopDuration, TEXT("continuous perimeter exceeds two laps with refresh and hover, no restart"));
    Menu->ShowPage(EKnightMainMenuPage::Home); const float ClosedTime=Map->Time; Draw(.5f);
    Check(Map->Time==ClosedTime, TEXT("closed operation map stops animation clock"));
    Menu->ShowPage(EKnightMainMenuPage::Stage); Draw(.2f); Draw();
    Check(Map->SelectedStageId==R1 && Panel->DisplayedQuestId==Q1, TEXT("reopening has no stale loop or mission"));
    const FName BeforeCycle=Map->SelectedQuestId; Map->CycleQuestMarker(1); Draw(.2f);
    Check(Map->SelectedQuestId!=BeforeCycle && Panel->DisplayedQuestId==Map->SelectedQuestId, TEXT("map keyboard/controller mode cycling keeps panel in sync"));
    Menu->SelectQuest(Q1); Draw(.2f);
    Menu->NativeOnPreviewKeyDown(Menu->GetCachedGeometry(), FKeyEvent(EKeys::Gamepad_DPad_Down,FModifierKeysState(),0,false,0,0)); Draw(.2f);
    Check(Map->SelectedQuestId != Q1 && Panel->DisplayedQuestId == Map->SelectedQuestId, TEXT("controller D-pad uses map selection, no removed list targets"));
    Menu->NativeOnNavigation(Menu->GetCachedGeometry(), FNavigationEvent(FModifierKeysState(),0,EUINavigation::Up,ENavigationGenesis::Controller),FNavigationReply::Escape()); Draw(.2f);
    Check(Map->SelectedQuestId == Q1 && Panel->DisplayedQuestId == Q1, TEXT("controller navigation event returns to map normal mode"));
	Check(Progress->GetMoney() == InitialMoney, TEXT("browsing and UI transitions grant/spend no money"));
	for (const auto& Item : InitialItems) Check(Progress->GetItemCount(Item.Item) == Item.Count, TEXT("browsing leaves inventory quantities unchanged"));
	Check(!Flow->IsTravelPending() && Flow->GetActiveStageId().IsNone(), TEXT("browsing does not start gameplay or change active stage"));
	// A fresh widget in the same GameInstance models menu reconstruction after travel.
	Menu->SaveOperationMapContext(); const auto Context = GI->GetSubsystem<UKnightOperationMapStateSubsystem>()->Profiles.FindRef(ProfileA);
	auto* Returned = CreateWidget<UKnightMainMenuWidget>(Player, Class); Returned->AddToRoot();
	Window->SetContent(Returned->TakeWidget()); Returned->ShowPage(EKnightMainMenuPage::Stage); Draw(); Draw();
	Check(Returned->SelectedStageId == Context.RegionId && Returned->QuestWidget->GetSelectedQuestId() == Context.QuestId,
		TEXT("reconstructed menu restores same-profile context after travel lifecycle"));
	Capture(TEXT("ReconstructedMenu"));
	Returned->RemoveFromRoot(); Menu->RemoveFromRoot();
	FSlateApplication::Get().UnregisterVirtualWindow(Window); BeginCleanup(Renderer); FlushRenderingCommands(); if (Target) Target->RemoveFromRoot();
	GI->Shutdown(); GEngine->DestroyWorldContext(World); World->DestroyWorld(false); GI->RemoveFromRoot();
	UE_LOG(LogTemp, Display, TEXT("OperationMap COMPLETE checks=%d failures=%d output=%s"), Checks, Failures, *Out);
	return Failures ? 1 : 0;
}
