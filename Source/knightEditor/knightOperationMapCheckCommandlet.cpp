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
#include "Components/VerticalBox.h"
#include "Components/Border.h"
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
	FWidgetRenderer* Renderer = new FWidgetRenderer(true);
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
		Window->Resize(FVector2D(Size)); Target = FWidgetRenderer::CreateTargetFor(FVector2D(Size), TF_Bilinear, true); Target->AddToRoot();
		Draw(); Draw();
	};
	auto Capture = [&](const FString& Name)
	{
		TArray<FColor> Pixels; FReadSurfaceDataFlags Flags(RCM_UNorm); Flags.SetLinearToGamma(false);
		Check(Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels, Flags), TEXT("real Slate/RHI pixels read"));
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
	Check(Panel->DetailRewards->GetVisibility() == ESlateVisibility::Collapsed && Panel->DetailTarget->GetVisibility() == ESlateVisibility::Collapsed, TEXT("map exploration does not present boss or rewards"));
	for (UCommonButtonBase* Button : Panel->QuestButtons)
	{
		const auto* Fill = Button->GetExtension<UKnightButtonFillExtension>();
		Check(!Fill || !Fill->IsEnabled(), TEXT("operation quest buttons have no animated fill"));
		Check(Button->GetStyleCDO() && Button->GetStyleCDO()->GetClass()->GetName() == TEXT("BS_Knight_OperationMap_C"), TEXT("quest state style is scoped blue operation-map style"));
	}
	Progress->RecordQuestCompleted(Q1); Draw();
	Check(State(Q1).bCompleted && State(Q1).bCanLaunch && State(Q2).bCanLaunch, TEXT("normal clear history remains separate from replay and next unlock"));
	Menu->SelectQuest(Q2); Draw(.4f); Draw(); Capture(TEXT("NormalClear_NextReady"));
	Progress->RecordQuestCompleted(Q2); Draw();
	Check(State(Q2).bCompleted && State(Q2).bCanLaunch && State(Q3).bCanLaunch && !State(Q3).bCompleted, TEXT("Janhyang normal opens exploration without fabricating exploration clear"));
	Menu->SelectQuest(Q3); Draw(.4f); Draw(); Capture(TEXT("ExplorationReady"));
	Check(Panel->DetailAvailability->GetText().ToString().Contains(TEXT("맵 탐색 가능")), TEXT("exploration explicitly identified in information panel"));
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
	Check(Map->Time - Map->SelectTime >= Map->SweepDuration, TEXT("restoration never replays selection or unlock celebration"));
	DrainSave(); Check(Saves->SelectSlot(ProfileB), TEXT("switch to separate isolated profile")); Draw();
	Check(Menu->SelectedStageId.IsNone() && Panel->GetSelectedQuestId().IsNone() && !State(Q1).bCompleted && State(Q3).bProgressionLocked,
		TEXT("new profile gets no old selection or clear/unlock indicators"));
	Menu->SelectQuest(Q1); Draw(.4f); DrainSave(); Check(Saves->SelectSlot(ProfileA), TEXT("return to first isolated profile")); Draw();
	Check(Menu->SelectedStageId == R3 && Panel->GetSelectedQuestId() == Q3 && State(Q2).bCompleted && State(Q3).bCanLaunch,
		TEXT("first profile restores own state and actual progression"));
	for (const FIntPoint Resolution : { FIntPoint(1280,720), FIntPoint(2560,1440) })
	{
		SetSize(Resolution, Resolution.X == 1280 ? 1.25f : 1.f);
		Map->RestoreView(FVector2D(0,0), 110.f); Menu->SelectQuest(Q2); Draw(.4f); Draw();
		const auto View = Map->MakeView(Map->GetCachedGeometry().GetLocalSize());
		const int32 Index = Map->Markers.IndexOfByPredicate([&](const FKnightMapMarker& M) { return M.StageId == R2; });
		Check(Index != INDEX_NONE && Map->FindMarkerAt(Map->ToLocal(Map->Markers[Index].Location, View), View.LocalSize) == Index,
			TEXT("painted region and hit detection agree at alternate DPI"));
		Check(Map->LeaderTarget.Get() == Panel, TEXT("connector keeps the actual details target"));
		Capture(FString::Printf(TEXT("Scale_%d"), Resolution.X));
	}
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
