#include "KnightCodexCheckCommandlet.h"
#include "Codex/KnightCodexWidget.h"
#include "Codex/KnightCodexReveal.h"
#include "CommonButtonBase.h"
#include "Menu/UI/KnightMainMenuWidget.h"
#include "Menu/KnightMenuGameMode.h"
#include "Progression/knightSaveSubsystem.h"
#include "Progression/knightProgressionSubsystem.h"
#include "Quest/KnightQuestData.h"
#include "Raid/knightRaidDefinition.h"
#include "CommonGameViewportClient.h"
#include "Components/Image.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/UserInterfaceSettings.h"
#include "GameFramework/PlayerController.h"
#include "Framework/Application/SlateApplication.h"
#include "Interfaces/ISlateRHIRendererModule.h"
#include "Widgets/SVirtualWindow.h"
#include "Slate/WidgetRenderer.h"
#include "Input/HittestGrid.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "TextureCompiler.h"
#include "UObject/UObjectIterator.h"
#include "RenderingThread.h"
#include "Async/TaskGraphInterfaces.h"

int32 UKnightCodexCheckCommandlet::Main(const FString& Params)
{
    FString Namespace, Out, UserDir;
    if (!FParse::Value(FCommandLine::Get(), TEXT("KnightSaveTestNamespace="), Namespace) || !Namespace.StartsWith(TEXT("Codex"))
        || !FParse::Value(FCommandLine::Get(), TEXT("UserDir="), UserDir) || UserDir.IsEmpty()
        || !FParse::Value(*Params, TEXT("Out="), Out) || !FParse::Param(FCommandLine::Get(), TEXT("AllowCommandletRendering"))) return 1;
    IFileManager::Get().MakeDirectory(*Out,true);
    if (!FSlateApplication::IsInitialized())
    {
        FSlateApplication::Create();
        FSlateApplication::Get().InitializeRenderer(FModuleManager::LoadModuleChecked<ISlateRHIRendererModule>(TEXT("SlateRHIRenderer")).CreateSlateRHIRenderer());
    }
    int32 Checks=0, Failures=0;
    auto Check=[&](bool Pass,const TCHAR* What)
    { ++Checks; Failures+=!Pass; UE_LOG(LogTemp,Display,TEXT("CodexStatic %s %s"),Pass?TEXT("PASS"):TEXT("FAIL"),What); };
    auto* GI=NewObject<UGameInstance>(GEngine); GI->AddToRoot(); GI->InitializeStandalone();
    UWorld* World=GI->GetWorld();
    TGuardValue<TSubclassOf<UGameViewportClient>> ViewportClass(GEngine->GameViewportClientClass,UCommonGameViewportClient::StaticClass());
    auto* Settings=GetMutableDefault<UUserInterfaceSettings>();
    TGuardValue<decltype(Settings->HardwareCursors)> CursorGuard(Settings->HardwareCursors,{});
    auto* Viewport=NewObject<UCommonGameViewportClient>(GEngine);
    GI->GetWorldContext()->GameViewport=Viewport; Viewport->Init(*GI->GetWorldContext(),GI,false);
    auto* Local=NewObject<ULocalPlayer>(GEngine); GI->AddLocalPlayer(Local,FPlatformUserId::CreateFromInternalId(0));
    auto* Player=World->SpawnActor<APlayerController>(); World->AddController(Player); Player->SetPlayer(Local);
    auto* Save=GI->GetSubsystem<UknightSaveSubsystem>(); auto* Progress=GI->GetSubsystem<UknightProgressionSubsystem>();
    const FString Profile=TEXT("Art")+FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(10);
    if (!Save || !Progress || !Save->SelectSlot(Profile)) return 1;
    auto DrainSave=[&]()
    {
        const double Deadline=FPlatformTime::Seconds()+15.;
        while (!Save->CanStartAtomicTransaction() && FPlatformTime::Seconds()<Deadline)
        { FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread); FTSTicker::GetCoreTicker().Tick(.01f); FPlatformProcess::Sleep(.01f); }
        Check(Save->CanStartAtomicTransaction(),TEXT("isolated async save completed"));
    };
    auto ModeClass=LoadClass<AKnightMenuGameMode>(nullptr,TEXT("/Game/Knight/Blueprints/BP_GM_MainMenu.BP_GM_MainMenu_C"));
    auto* Mode=ModeClass?World->SpawnActor<AKnightMenuGameMode>(ModeClass):nullptr; if(!Mode)return 1;
    FString Error; Mode->InitGame(TEXT("CodexFixture"),FString(),Error);
    auto MenuClass=LoadClass<UKnightMainMenuWidget>(nullptr,TEXT("/Game/Knight/UI/Menu/WBP_KnightMainMenu.WBP_KnightMainMenu_C"));
    auto* Menu=MenuClass?CreateWidget<UKnightMainMenuWidget>(Player,MenuClass):nullptr; if(!Menu)return 1;
    Menu->AddToRoot();
    TSharedRef<SVirtualWindow> Window=SNew(SVirtualWindow); Window->SetContent(Menu->TakeWidget()); FSlateApplication::Get().RegisterVirtualWindow(Window);
    auto* Renderer=new FWidgetRenderer(true); FHittestGrid Grid;
    UTextureRenderTarget2D* Target=nullptr; FIntPoint Extent; float Scale=1.f;
    auto Draw=[&](float Dt=0.f) { ++GFrameCounter; Renderer->DrawWindow(Target,Grid,Window,Scale,FVector2D(Extent),Dt); FlushRenderingCommands(); };
    auto Resize=[&](FIntPoint Size)
    {
        if(Target)Target->RemoveFromRoot(); Extent=Size; Scale=Settings->GetDPIScaleBasedOnSize(Size); Window->Resize(FVector2D(Size));
        Target=NewObject<UTextureRenderTarget2D>(); Target->AddToRoot(); Target->ClearColor=FLinearColor::Transparent;
        Target->TargetGamma=1.f; Target->InitCustomFormat(Size.X,Size.Y,PF_FloatRGBA,true); Target->UpdateResourceImmediate(true); Draw(); Draw();
    };
    Resize(FIntPoint(1920,1080)); Check(Menu->ShowPage(EKnightMainMenuPage::Codex),TEXT("actual menu opens Codex")); Draw(.5f); Draw();
    auto* Host=Cast<UPanelWidget>(Menu->GetWidgetFromName(TEXT("Panel_Codex")));
    auto* Codex=Host?Cast<UKnightCodexWidget>(Host->GetChildAt(0)):nullptr; if(!Codex)return 1;
    auto W=[&](const TCHAR* Name){return Codex->GetWidgetFromName(Name);};
    auto Text=[&](const TCHAR* Name){const auto* T=Cast<UTextBlock>(W(Name));return T?T->GetText().ToString():FString();};
    auto* Data=LoadObject<UKnightCodexData>(nullptr,TEXT("/Game/Knight/Data/DA_Codex.DA_Codex")); if(!Data)return 1;
    auto* Portrait=CastChecked<UImage>(W(TEXT("Image_Portrait")));
    auto* Drops=CastChecked<UPanelWidget>(W(TEXT("Box_Drops")));
    auto* Scroll=CastChecked<UScrollBox>(W(TEXT("Scroll_BossDetail")));
    auto* Bosses=CastChecked<UPanelWidget>(W(TEXT("Box_Bosses")));
    auto DropText=[&]()
    {
        FString Result;
        for(UWidget* Child:Drops->GetAllChildren()) if(auto* Row=Cast<UUserWidget>(Child))
            for(const TCHAR* Name:{TEXT("Text_Name"),TEXT("Text_Details")})
                if(auto* T=Cast<UTextBlock>(Row->GetWidgetFromName(Name)))Result+=T->GetText().ToString()+TEXT("\n");
        return Result;
    };
    TSet<UTexture2D*> Prepared;
    auto Capture=[&](const FString& Name,bool bRequireCodex=true)
    {
        Check(!bRequireCodex||Menu->GetCurrentPage()==EKnightMainMenuPage::Codex,TEXT("capture is the expected visible page"));
        FTextureCompilingManager::Get().FinishAllCompilation();
        for(TObjectIterator<UTexture2D> It;It;++It) if(It->GetPathName().StartsWith(TEXT("/Game/Knight/"))&&!Prepared.Contains(*It))
        { It->SetForceMipLevelsToBeResident(300); It->WaitForStreaming(); Prepared.Add(*It); }
        Draw(); Draw(); TArray<FLinearColor> Linear; TArray<FColor> Pixels;
        Check(Target->GameThread_GetRenderTargetResource()->ReadLinearColorPixels(Linear,FReadSurfaceDataFlags(RCM_MinMax)),TEXT("real Slate RHI readback"));
        for(const auto& Pixel:Linear)Pixels.Add(Pixel.ToFColor(false));
        TArray64<uint8> PNG; FImageUtils::PNGCompressImageArray(Extent.X,Extent.Y,Pixels,PNG);
        Check(FFileHelper::SaveArrayToFile(PNG,*(Out/(Name+TEXT(".png")))),TEXT("capture saved"));
    };
    Codex->SelectCategory(2); Draw(); Check(Bosses->GetChildrenCount()==Data->Bosses.Num(),TEXT("catalog order and entry count preserved"));
    Check(!Portrait->GetBrush().GetResourceObject()&&Drops->GetChildrenCount()==0,TEXT("fresh profile conceals illustration and rewards")); Capture(TEXT("01_Locked_1080"));
    const int64 Money=Progress->GetMoney(); TArray<FknightItemStack> InitialItems; Progress->GetOwnedItems(InitialItems);
    Check(Progress->RecordQuestCompleted(TEXT("Quest_0001")),TEXT("normal quest unlocks Janyeol through existing event")); DrainSave(); Draw();
    const TMap<FName,FString> Art={{TEXT("Janyeol"),TEXT("/Game/Knight/UI/Book/Janyeol2.Janyeol2")},{TEXT("Janhyang"),TEXT("/Game/Knight/UI/Book/janhyang3.janhyang3")},{TEXT("Heukgwan"),TEXT("/Game/Knight/UI/Book/heukgwan1.heukgwan1")}};
    for(const auto& Boss:Data->Bosses)Check(Boss.Portrait.ToSoftObjectPath().ToString()==Art.FindRef(Boss.BossId),TEXT("stable ID has approved explicit artwork reference"));
    Codex->SelectBoss(TEXT("Janyeol")); Draw();
    Check(Portrait->GetBrush().GetResourceObject()&&Drops->GetChildrenCount()==8,TEXT("unowned quest reward items and artwork shown for unlocked Janyeol"));
    const FString JanyeolRewards=DropText();
    Check(JanyeolRewards.Contains(TEXT("보통 · 완료"))&&JanyeolRewards.Contains(TEXT("어려움 · 완료"))&&JanyeolRewards.Contains(TEXT("부위 파괴: Head")),TEXT("difficulty and conditional sources remain separate"));
    Capture(TEXT("02_Janyeol_1080"));
    auto CheckLayout=[&]()
    {
        const auto& G=Portrait->GetCachedGeometry(); const auto S=G.GetLocalSize(); const auto A=G.LocalToAbsolute(FVector2D::ZeroVector); const auto B=G.LocalToAbsolute(S);
        Check(FMath::IsNearlyEqual(S.X/S.Y,1.5f,.001f)&&A.X>=0&&A.Y>=0&&B.X<=Extent.X+1&&B.Y<=Extent.Y+1,TEXT("whole 3:2 illustration fits viewport"));
        Check(W(TEXT("Panel_Illustration"))->GetCachedGeometry().GetLocalSize().X>W(TEXT("Panel_Reader"))->GetCachedGeometry().GetLocalSize().X,TEXT("illustration is largest column"));
        UE_LOG(LogTemp,Display,TEXT("CodexStatic GEOMETRY %dx%d dpi=%.3f art=(%.1f,%.1f)-(%.1f,%.1f)"),Extent.X,Extent.Y,Scale,A.X,A.Y,B.X,B.Y);
    };
    CheckLayout(); const FVector2D BeforeScroll=Portrait->GetCachedGeometry().LocalToAbsolute(FVector2D::ZeroVector);
    Scroll->ScrollToEnd(); Capture(TEXT("03_Janyeol_Rewards_1080"));
    Check(Scroll->GetScrollOffset()>0&&Portrait->GetCachedGeometry().LocalToAbsolute(FVector2D::ZeroVector).Equals(BeforeScroll,.1f),TEXT("information scroll leaves illustration stationary"));
    for(UWidget* Child:Drops->GetAllChildren())
    { auto* Row=CastChecked<UUserWidget>(Child); Check(CastChecked<UImage>(Row->GetWidgetFromName(TEXT("Image_Icon")))->GetBrush().GetResourceObject()!=nullptr,TEXT("configured reward uses actual material icon")); }
    Check(Menu->SelectQuest(TEXT("Quest_Janhyang_Hard")),TEXT("different quest can be selected independently"));
    // SelectQuest navigates to the map. Complete that existing transition before
    // returning to Codex so subsequent captures cannot paint the wrong screen.
    Draw(); Draw(.5f); Draw();
    Check(Menu->ShowPage(EKnightMainMenuPage::Codex),TEXT("return from quest selection to visible Codex"));
    Draw(); Codex->SelectCategory(2);
    Codex->SelectBoss(TEXT("Janyeol")); Check(DropText()==JanyeolRewards,TEXT("selected quest cannot change another monster's drops"));
    Check(Progress->RecordQuestCompleted(TEXT("Quest_0002")),TEXT("normal quest unlocks Janhyang")); DrainSave();
    Codex->SelectBoss(TEXT("Janhyang")); Draw();
    Check(Drops->GetChildrenCount()==8&&DropText()!=JanyeolRewards&&DropText().Contains(TEXT("부위 파괴: Chest"))&&DropText().Contains(TEXT("부위 파괴: Tail")),TEXT("switch updates matching Janhyang quest rewards and actual tags"));
    Check(Portrait->GetBrush().GetResourceObject()->GetPathName()==Art.FindChecked(TEXT("Janhyang")),TEXT("switch updates artwork without stale Janyeol image"));
    Capture(TEXT("04_Janhyang_1080"));
    Resize(FIntPoint(1280,720)); Capture(TEXT("05_Janhyang_720")); CheckLayout();
    Scroll->ScrollToEnd(); Capture(TEXT("06_Janhyang_Rewards_720"));
    Resize(FIntPoint(2560,1080)); Capture(TEXT("07_Janhyang_Ultrawide")); CheckLayout();
    Codex->SelectBoss(TEXT("Heukgwan")); Check(!Portrait->GetBrush().GetResourceObject()&&Drops->GetChildrenCount()==0,TEXT("Heukgwan remains locked; old artwork and drops cleared"));
    Capture(TEXT("08_Heukgwan_Locked_Ultrawide"));
    // Presentation-only fixture: temporarily borrow an already completed quest in memory.
    // Never save the asset or introduce a new quest/progression route for the unfinished monster.
    auto* Heukgwan=Data->Bosses.FindByPredicate([](const auto& B){return B.BossId==TEXT("Heukgwan");});
    if(Heukgwan)
    {
        TGuardValue<FName> UnlockFixture(Heukgwan->RequiredQuestId,TEXT("Quest_0002"));
        Codex->Open(); Codex->SelectCategory(2);
        Codex->SelectBoss(TEXT("Heukgwan")); Resize(FIntPoint(1920,1080)); Capture(TEXT("09_Heukgwan_ArtworkFixture")); CheckLayout();
        Check(Drops->GetChildrenCount()==0,TEXT("artwork fixture fabricates no rewards or monster data"));
        TGuardValue<TSoftObjectPtr<UTexture2D>> MissingArt(Heukgwan->Portrait,{});
        Codex->SelectBoss(TEXT("Heukgwan")); Check(!Portrait->GetBrush().GetResourceObject()&&W(TEXT("Text_NoPortrait"))->IsVisible(),TEXT("missing artwork clears old resource and has safe fallback"));
    }
    Codex->Open(); Codex->SelectCategory(2); Codex->SelectBoss(TEXT("Janyeol"));
    {
        auto* Row=CastChecked<UKnightCodexDropEntryWidget>(Drops->GetChildAt(0));
        Row->ConfigureQuestDrop(nullptr,{});
        Check(!CastChecked<UImage>(Row->GetWidgetFromName(TEXT("Image_Icon")))->GetBrush().GetResourceObject()
            && Row->GetWidgetFromName(TEXT("Size_ItemIcon"))->IsVisible(),TEXT("missing icon clears stale image while preserving material alignment"));
        Codex->SelectBoss(TEXT("Janyeol"));
    }
    auto* First=CastChecked<UUserWidget>(Bosses->GetChildAt(0)); auto* Second=CastChecked<UUserWidget>(Bosses->GetChildAt(1));
    auto* Selected=CastChecked<UKnightCodexArchiveButton>(First->GetWidgetFromName(TEXT("Button_Select")));
    auto* Focused=CastChecked<UKnightCodexArchiveButton>(Second->GetWidgetFromName(TEXT("Button_Select")));
    FSlateApplication::Get().SetKeyboardFocus(Focused->TakeWidget(),EFocusCause::Navigation); Draw();
    Check(Selected->IsRecordSelected()&&!Focused->IsRecordSelected()&&Focused->HasKeyboardFocus(),TEXT("input focus on another entry does not change selection")); Capture(TEXT("10_SelectedAndFocused"));
    for(UWidget* Child:Bosses->GetAllChildren())
    {
        auto* Entry=CastChecked<UKnightCodexBossEntryWidget>(Child);
        auto* Icon=CastChecked<UImage>(Entry->GetWidgetFromName(TEXT("Image_Icon")));
        const auto* Record=Data->Bosses.FindByPredicate([&](const auto& B){return B.BossId==Entry->GetBossId();});
        if(Record && Progress->IsQuestCompleted(Record->RequiredQuestId))
        {
            Check(Icon->GetBrush().GetResourceObject()==Record->ListIcon.Get(),TEXT("thumbnail has explicit matching monster texture"));
            const FBox2f UV=Icon->GetBrush().GetUVRegion();
            Check(UV.GetSize().X>0 && UV.GetSize().Y>0 && UV.GetSize().X<.30 && UV.GetSize().Y<.35,TEXT("thumbnail shows the head detail, not a shrunken dossier"));
        }
        else Check(!Icon->GetBrush().GetResourceObject(),TEXT("undiscovered thumbnail remains concealed"));
    }
    FPointerEvent Pointer;
    Focused->TakeWidget()->OnMouseEnter(Focused->GetCachedGeometry(),Pointer); Capture(TEXT("11_Hover")); Focused->TakeWidget()->OnMouseLeave(Pointer);
    Check(!Focused->IsHovered()&&Selected->IsRecordSelected(),TEXT("hover clears without losing genuine selection"));
    {
        auto* Entry=CastChecked<UKnightCodexBossEntryWidget>(First);
        Entry->Configure(TEXT("Janyeol"),FText::FromString(TEXT("아주 긴 한국어 몬스터 이름 줄바꿈 확인")),nullptr,true);
        Resize(FIntPoint(1280,720)); Capture(TEXT("12_LongName_720"));
        Check(First->GetCachedGeometry().GetLocalSize().X<=W(TEXT("Size_EntryList"))->GetCachedGeometry().GetLocalSize().X,TEXT("long Korean name stays within list column"));
    }
    // Exercise real tab/record delegates, then preserve their independently scrolled content.
    Codex->Open(); Codex->SelectCategory(2); Codex->SelectBoss(TEXT("Janhyang")); Draw();
    Scroll->SetScrollOffset(90); Draw(); const float MonsterOffset=Scroll->GetScrollOffset();
    FString Titles[2], Bodies[2]; float ReaderOffsets[2]={}, ListOffsets[2]={};
    for(int32 Category=0; Category<2; ++Category)
    {
        CastChecked<UButton>(W(Category==0?TEXT("Button_World"):TEXT("Button_Mech")))->OnClicked.Broadcast(); Draw();
        const auto& Sections=Category==0?Data->WorldSections:Data->MechSections;
        auto* List=CastChecked<UPanelWidget>(W(Category==0?TEXT("Box_WorldEntries"):TEXT("Box_MechEntries")));
        int32 ReaderIndex=0, LongestIndex=0, LongestLength=-1;
        for(const auto& Section:Sections) if(Section.bShowInReader&&!Section.GetReaderBody().IsEmpty())
        {
            if(Section.GetReaderBody().ToString().Len()>LongestLength){LongestLength=Section.GetReaderBody().ToString().Len(); LongestIndex=ReaderIndex;}
            ++ReaderIndex;
        }
        Check(List->GetChildrenCount()==ReaderIndex,TEXT("all existing public records retained in authored order"));
        auto* Holder=CastChecked<UPanelWidget>(List->GetChildAt(LongestIndex));
        CastChecked<UButton>(Holder->GetChildAt(0))->OnClicked.Broadcast(); Draw(); Draw();
        Titles[Category]=Text(Category==0?TEXT("Text_WorldTitle"):TEXT("Text_MechTitle"));
        Bodies[Category]=Text(Category==0?TEXT("Text_WorldBody"):TEXT("Text_MechBody"));
        auto* Reader=CastChecked<UScrollBox>(W(Category==0?TEXT("Scroll_World"):TEXT("Scroll_Mech")));
        auto* ListScroll=CastChecked<UScrollBox>(W(Category==0?TEXT("Scroll_WorldEntries"):TEXT("Scroll_MechEntries")));
        Reader->SetScrollOffset(120); ListScroll->SetScrollOffset(60); Draw(); Draw();
        ReaderOffsets[Category]=Reader->GetScrollOffset(); ListOffsets[Category]=ListScroll->GetScrollOffset();
        Check(!Titles[Category].IsEmpty()&&Bodies[Category].Len()==LongestLength,TEXT("selected article shows complete existing Korean prose"));
    }
    for(const FIntPoint Size:{FIntPoint(1920,1080),FIntPoint(1280,720),FIntPoint(2560,1080)})
    {
        Resize(Size);
        for(int32 Category=0;Category<2;++Category)
        {
            CastChecked<UButton>(W(Category==0?TEXT("Button_World"):TEXT("Button_Mech")))->OnClicked.Broadcast(); Draw(); Draw();
            Check(Text(Category==0?TEXT("Text_WorldTitle"):TEXT("Text_MechTitle"))==Titles[Category]
                && Text(Category==0?TEXT("Text_WorldBody"):TEXT("Text_MechBody"))==Bodies[Category],TEXT("tab return retains selected record and full contents"));
            auto* Reader=CastChecked<UScrollBox>(W(Category==0?TEXT("Scroll_World"):TEXT("Scroll_Mech")));
            auto* ListScroll=CastChecked<UScrollBox>(W(Category==0?TEXT("Scroll_WorldEntries"):TEXT("Scroll_MechEntries")));
            Check(FMath::IsNearlyEqual(Reader->GetScrollOffset(),ReaderOffsets[Category],1.f)
                && FMath::IsNearlyEqual(ListScroll->GetScrollOffset(),ListOffsets[Category],1.f),TEXT("category reader and list scroll offsets retained"));
            Capture(FString::Printf(TEXT("%s_%dx%d"),Category==0?TEXT("World"):TEXT("Mech"),Size.X,Size.Y));
        }
    }
    Codex->SelectCategory(2); Draw(); Draw();
    Check(Portrait->GetBrush().GetResourceObject()->GetPathName()==Art.FindChecked(TEXT("Janhyang"))
        && FMath::IsNearlyEqual(Scroll->GetScrollOffset(),MonsterOffset,1.f),TEXT("monster selection and materials scroll retained across other tabs"));
    Check(!Codex->SelectBoss(TEXT("MissingRecord")),TEXT("invalid record rejected without stale content"));
    Check(Menu->RequestBack(),TEXT("shared Back/Esc route accepts animated return")); Draw(.5f); Draw();
    Check(Menu->GetCurrentPage()==EKnightMainMenuPage::Home,TEXT("shared Back/Esc route returns to menu"));
    Check(Menu->ShowPage(EKnightMainMenuPage::Codex),TEXT("Codex reopens through actual menu")); Draw(.5f); Draw();
    Check(Codex->GetFocusButton()->HasUserFocus(Player),TEXT("reentry restores category focus"));
    CastChecked<UButton>(W(TEXT("Button_Close")))->OnClicked.Broadcast(); Draw(.5f); Draw(); Check(Menu->GetCurrentPage()==EKnightMainMenuPage::Home,TEXT("footer button returns to menu"));
    Menu->ShowPage(EKnightMainMenuPage::Codex); Draw(.5f); Draw(); Codex->SelectCategory(2); Codex->SelectBoss(TEXT("Janhyang")); DrainSave();
    Check(Save->SelectSlot(Profile+TEXT("B")),TEXT("switch to independent empty profile")); Draw();
    Check(!Portrait->GetBrush().GetResourceObject()&&Drops->GetChildrenCount()==0,TEXT("profile switch clears unlocked artwork and rewards"));
    Check(Save->SelectSlot(Profile),TEXT("restore isolated profile")); Draw();
    TArray<FknightItemStack> FinalItems; Progress->GetOwnedItems(FinalItems);
    Check(Progress->GetMoney()==Money&&InitialItems.Num()==FinalItems.Num(),TEXT("read-only Codex grants no money or inventory")); DrainSave();
    if(FParse::Param(*Params,TEXT("Motion")))
    {
        Check(!W(TEXT("Brand_Accent"))&&!W(TEXT("Accent_Label_CodexEntries"))&&!W(TEXT("Accent_Label_Observation"))&&!W(TEXT("Accent_Label_Drops")),TEXT("standalone slashes and their layout widgets removed"));
        auto* Motion=Menu->CodexTransition.Get();
        auto* ImageMask=CastChecked<UKnightCodexReveal>(W(TEXT("Reveal_Monster")));
        const FWidgetTransform IdleIcon=Portrait->GetRenderTransform();
        const FWidgetTransform IdleList=W(TEXT("Size_EntryList"))->GetRenderTransform();
        auto Frame=[&](const FString& Name,float Dt) { Draw(Dt); Capture(Name); };
        auto Phase=[&](EKnightCodexPhase Expected){return Motion->GetPhase()==Expected;};
        auto PointerBlocked=[&]()
        {
            const auto Path=Grid.GetBubblePath(FVector2D(Extent.X*.5f,Extent.Y*.5f),0,false);
            return Motion->Shield && !Path.IsEmpty() && Path.Last().Widget==Motion->Shield->TakeWidget();
        };
        for(const FIntPoint Size:{FIntPoint(1920,1080),FIntPoint(1280,720),FIntPoint(2560,1080)})
        {
            Resize(Size);
            for(int32 Category=0;Category<3;++Category)
            {
                Codex->SelectCategory(Category); Draw();
                const FString Prefix=FString::Printf(TEXT("Motion_%dx%d_C%d"),Size.X,Size.Y,Category);
                const FString Record=Text(Category==0?TEXT("Text_WorldTitle"):Category==1?TEXT("Text_MechTitle"):TEXT("Text_BossName"));
                auto* ReaderScroll=CastChecked<UScrollBox>(W(Category==0?TEXT("Scroll_World"):Category==1?TEXT("Scroll_Mech"):TEXT("Scroll_BossDetail")));
                const float ReaderOffset=ReaderScroll->GetScrollOffset();
                Menu->RequestBack(); Draw(.5f); Draw();
                Check(Menu->ShowPage(EKnightMainMenuPage::Codex),TEXT("open selected category through actual Home route")); Draw();
                Check(Phase(EKnightCodexPhase::Entering)&&ImageMask->Progress==0&&ImageMask->Alpha==0,TEXT("initialized invisible before first visible frame"));
                Check(PointerBlocked(),TEXT("transition hit grid routes pointer to shield, not underlying controls"));
                Frame(Prefix+TEXT("_01_early"),.04f); Frame(Prefix+TEXT("_02_scan"),.08f); Frame(Prefix+TEXT("_03_visible"),.32f);
                Check(Phase(EKnightCodexPhase::Visible)&&!ImageMask->bActive&&!Motion->Shield,TEXT("visible state removes mask, glow and input shield"));
                Check(W(TEXT("Size_EntryList"))->GetRenderTransform()==IdleList&&Portrait->GetRenderTransform()==IdleIcon,TEXT("exact idle transforms restored; illustration never rotated or scaled"));
                Check(Text(Category==0?TEXT("Text_WorldTitle"):Category==1?TEXT("Text_MechTitle"):TEXT("Text_BossName"))==Record,TEXT("category and record retained after close/reopen"));
                Check(FMath::IsNearlyEqual(ReaderScroll->GetScrollOffset(),ReaderOffset,1.f),TEXT("valid reader scroll retained after animated close/reopen"));
                CastChecked<UButton>(W(TEXT("Button_Close")))->OnClicked.Broadcast();
                Frame(Prefix+TEXT("_04_exit"),.14f);
                const float Before=Motion->GetReveal();
                for(int32 I=0;I<12;++I) Menu->RequestBack();
                Check(Motion->GetReveal()==Before&&Phase(EKnightCodexPhase::Exiting)&&Menu->GetCurrentPage()==EKnightMainMenuPage::Codex,TEXT("repeated Back neither restarts exit nor navigates early"));
                Check(PointerBlocked(),TEXT("closing page cannot click through to Home"));
                Draw(.18f); Draw();
                // CommonButtonBase forwards focus to its inner Slate button in NativeOnFocusReceived.
                const bool bHomeCodexFocused=Menu->Button_Codex->HasUserFocus(Player)||Menu->Button_Codex->HasUserFocusedDescendants(Player);
                UE_LOG(LogTemp,Display,TEXT("CodexMotion focus self=%d descendant=%d preferred=%d"),Menu->Button_Codex->HasUserFocus(Player),Menu->Button_Codex->HasUserFocusedDescendants(Player),Menu->GetPreferredFocusTarget()==Menu->Button_Codex);
                Check(Phase(EKnightCodexPhase::Hidden)&&Menu->GetCurrentPage()==EKnightMainMenuPage::Home&&bHomeCodexFocused&&Menu->GetPreferredFocusTarget()==Menu->Button_Codex&&!Motion->Shield,TEXT("one completed return restores Codex Home focus and normal input"));
                Menu->ShowPage(EKnightMainMenuPage::Codex); Draw(.5f); Draw();
            }
        }
        Resize(FIntPoint(1920,1080)); Codex->SelectCategory(2); Codex->SelectBoss(TEXT("Janyeol")); Draw();
        Menu->RequestBack(); Draw(.5f); Draw(); Menu->ShowPage(EKnightMainMenuPage::Codex); Draw();
        // Complete 60 Hz rendered sequence, including fully transparent initial/final frames.
        for(int32 I=0;I<=27;++I) Frame(FString::Printf(TEXT("Sequence_%03d"),I),I?1.f/60.f:0.f);
        for(int32 I=28;I<40;++I) Frame(FString::Printf(TEXT("Sequence_%03d"),I),1.f/60.f);
        Menu->RequestBack();
        for(int32 I=40;I<58;++I) Frame(FString::Printf(TEXT("Sequence_%03d"),I),1.f/60.f);
        // Capture() requires the Codex page; last outgoing frame is still transparent before navigation.
        Draw(.02f); Draw(); Capture(TEXT("Sequence_058"),false);
        for(int32 I=59;I<70;++I) { Draw(1.f/60.f); Capture(FString::Printf(TEXT("Sequence_%03d"),I),false); }
        Menu->ShowPage(EKnightMainMenuPage::Codex); Draw(.09f); Draw();
        const float EntryReveal=Motion->GetReveal(),EntryAlpha=Motion->GetOpacity();
        FKeyEvent Escape(EKeys::Escape,FModifierKeysState(),0,false,0,0);
        Check(Menu->NativeOnPreviewKeyDown(Menu->GetCachedGeometry(),Escape).IsEventHandled(),TEXT("Escape during entry handled by real preview-key route"));
        Check(Phase(EKnightCodexPhase::Exiting)&&Motion->GetReveal()==EntryReveal&&Motion->GetOpacity()==EntryAlpha,TEXT("interrupted entry reverses from current values without snap"));
        Frame(TEXT("Motion_InterruptedExit"),.06f);
        FKeyEvent RepeatEscape(EKeys::Escape,FModifierKeysState(),0,true,0,0);
        Menu->NativeOnPreviewKeyDown(Menu->GetCachedGeometry(),RepeatEscape); Draw(.3f); Draw();
        Check(Menu->NativeOnPreviewKeyDown(Menu->GetCachedGeometry(),RepeatEscape).IsEventHandled()&&Menu->GetCurrentPage()==EKnightMainMenuPage::Home,TEXT("held Back does not activate Home settings after return"));
        Menu->NativeOnKeyUp(Menu->GetCachedGeometry(),Escape);
        Menu->ShowPage(EKnightMainMenuPage::Codex); Draw(.5f); Draw();
        Codex->SelectCategory(1); Draw(); Codex->SelectCategory(2); Draw();
        Check(Phase(EKnightCodexPhase::Visible)&&!ImageMask->bActive,TEXT("tab switching and scrolling never replay entrance"));
        Codex->SelectBoss(TEXT("Heukgwan")); Menu->RequestBack(); Draw(.5f); Draw();
        Menu->ShowPage(EKnightMainMenuPage::Codex); Frame(TEXT("Motion_LockedMonster"),.12f);
        Check(!Portrait->GetBrush().GetResourceObject()&&Drops->GetChildrenCount()==0,TEXT("scan cannot expose locked artwork or rewards"));
        FKeyEvent ControllerBack(EKeys::Gamepad_FaceButton_Right,FModifierKeysState(),0,false,0,0);
        Check(Menu->NativeOnPreviewKeyDown(Menu->GetCachedGeometry(),ControllerBack).IsEventHandled(),TEXT("controller Back shares animated return route")); Draw(.5f); Draw();
        Menu->NativeOnKeyUp(Menu->GetCachedGeometry(),ControllerBack);
        Menu->ShowPage(EKnightMainMenuPage::Codex); Draw(.04f); Draw(); Motion->Cancel();
        Check(Phase(EKnightCodexPhase::Hidden)&&!Motion->Shield&&!ImageMask->bActive,TEXT("cancellation releases temporary visual and input state"));
    }
    FSlateApplication::Get().UnregisterVirtualWindow(Window); BeginCleanup(Renderer); FlushRenderingCommands();
    Target->RemoveFromRoot(); Menu->RemoveFromRoot(); GI->Shutdown(); GEngine->DestroyWorldContext(World); World->DestroyWorld(false); GI->RemoveFromRoot();
    UE_LOG(LogTemp,Display,TEXT("CodexStatic COMPLETE checks=%d failures=%d output=%s"),Checks,Failures,*Out);
    return Failures?1:0;
}
