// Opt-in offscreen captures of the existing hangar and shared Settings entry point.
#if WITH_EDITOR
#include "Screens/knightSettingsScreen.h"
#include "Menu/UI/KnightMainMenuWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "CommonTextBlock.h"
#include "Containers/Ticker.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"

namespace KnightSettingsCapture
{
struct FRun : TSharedFromThis<FRun>
{
    TWeakObjectPtr<UWorld> World;
    TWeakObjectPtr<UKnightMainMenuWidget> Menu;
    TWeakObjectPtr<UknightSettingsScreen> Screen;
    FString Out;
    int32 Step=0;
    bool bSized=false;
    double Next=0, Deadline=0;
    bool Tick(float)
    {
        const double Now=FPlatformTime::Seconds();
        if(!World.IsValid()||Now>Deadline) { UE_LOG(LogTemp,Error,TEXT("SettingsCapture timed out step=%d"),Step); FPlatformMisc::RequestExit(false);return false; }
        if(Now<Next||FScreenshotRequest::IsScreenshotRequested())return true;
        if(Step==0)
        {
            TArray<UUserWidget*> Widgets; UWidgetBlueprintLibrary::GetAllWidgetsOfClass(World.Get(),Widgets,UKnightMainMenuWidget::StaticClass(),false);
            for(auto* W:Widgets)if(W->IsInViewport())Menu=Cast<UKnightMainMenuWidget>(W);
            if(!Menu.IsValid())return true;
            if(!bSized)
            {
                World->GetFirstPlayerController()->ConsoleCommand(TEXT("r.SetRes 1920x1080w"),false);
                bSized=true; Next=Now+1; return true;
            }
            if(!Menu->ShowPage(EKnightMainMenuPage::Settings))return true;
            Widgets.Reset(); UWidgetBlueprintLibrary::GetAllWidgetsOfClass(World.Get(),Widgets,UknightSettingsScreen::StaticClass(),false);
            for(auto* W:Widgets)if(W->IsInViewport())Screen=Cast<UknightSettingsScreen>(W);
            if(!Screen.IsValid())return true;
            Next=Now+3; ++Step; return true;
        }
        if(Step>=1&&Step<=4)
        {
            const int32 Tab=Step-1;
            if(Screen->GetCurrentTab()!=Tab) { Screen->SetTab(Tab); Next=Now+1; return true; }
            FVector2D Pixels; World->GetGameViewport()->GetViewportSize(Pixels);
            UE_LOG(LogTemp,Display,TEXT("SettingsCapture tab=%d dirty=%d world=%s viewport=%.0fx%.0f"),Tab,Screen->GetDirtyCount(),*World->GetMapName(),Pixels.X,Pixels.Y);
            FScreenshotRequest::RequestScreenshot(Out/FString::Printf(TEXT("Hangar_Tab%d.png"),Tab),true,false);
            ++Step; Next=Now+1; return true;
        }
        if(Step==5)
        {
            Screen->RequestClose(); ++Step; Next=Now+1; return true;
        }
        if(Step==7) { FPlatformMisc::RequestExit(false);return false; }
        UE_LOG(LogTemp,Display,TEXT("SettingsCapture COMPLETE returnedHome=%d settingsHidden=%d world=%s"),Menu->GetCurrentPage()==EKnightMainMenuPage::Home,!Screen->IsInViewport(),*World->GetMapName());
        FScreenshotRequest::RequestScreenshot(Out/TEXT("Hangar_Return.png"),true,false);
        if(Step++==6) {Next=Now+1;return true;}
        FPlatformMisc::RequestExit(false);return false;
    }
};
static FAutoConsoleCommandWithWorldAndArgs Command(TEXT("knight.UI.CaptureSettings"),TEXT("Isolated SettingsStatic capture; output folder required."),
    FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args,UWorld* World)
    {
        FString UserDir,Namespace;
        if(!World||Args.Num()!=1||!FParse::Value(FCommandLine::Get(),TEXT("UserDir="),UserDir)||!UserDir.Contains(TEXT("SettingsStatic"))
            ||!FParse::Value(FCommandLine::Get(),TEXT("KnightSaveTestNamespace="),Namespace)||!Namespace.StartsWith(TEXT("SettingsStatic")))return;
        auto Run=MakeShared<FRun>();Run->World=World;Run->Out=Args[0];Run->Deadline=FPlatformTime::Seconds()+120;
        IFileManager::Get().MakeDirectory(*Run->Out,true);
        FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Run](float Dt){return Run->Tick(Dt);}));
    }));
}
#endif
