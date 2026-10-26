// Explicit opt-in real-render check; never runs in Shipping or ordinary gameplay.
#if WITH_EDITOR
#include "Settings/KnightSettingsWidget.h"
#include "Settings/knightSettingsSubsystem.h"
#include "Settings/knightSharedSettings.h"
#include "Widgets/knightSettingRow.h"
#include "Menu/KnightMaintenanceCameraRig.h"
#include "Blueprint/WidgetTree.h"
#include "Components/PanelWidget.h"
#include "Components/Slider.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "AIController.h"
#include "BrainComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Camera/PlayerCameraManager.h"
#include "Containers/Ticker.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "SceneView.h"
#include "SceneViewExtension.h"
#include "UnrealClient.h"

namespace
{
class FCalibrationObserver : public FSceneViewExtensionBase
{
public:
    FCalibrationObserver(const FAutoRegister& Auto, int32 InPriority) : FSceneViewExtensionBase(Auto), Priority(InPriority) {}
    FVector4 Gain = FVector4(1), Gamma = FVector4(1);
    float Exposure = 0;
    int32 Method = 0, Frames = 0, Priority;
    virtual int32 GetPriority() const override { return Priority; }
    virtual void SetupView(FSceneViewFamily& Family, FSceneView& View) override
    {
        Gain=View.FinalPostProcessSettings.ColorGain;
        Gamma=View.FinalPostProcessSettings.ColorGamma;
        Exposure=View.FinalPostProcessSettings.AutoExposureBias;
        Method=int32(View.FinalPostProcessSettings.AutoExposureMethod);
        ++Frames;
    }
};
struct FCalibrationCheck
{
    TWeakObjectPtr<UWorld> World;
    TWeakObjectPtr<UGameInstance> Instance;
    TWeakObjectPtr<UKnightSettingsWidget> Widget;
    TSharedPtr<FCalibrationObserver,ESPMode::ThreadSafe> Observer;
    TSharedPtr<FCalibrationObserver,ESPMode::ThreadSafe> Baseline;
    FDelegateHandle MapHandle;
    FString PendingCapture;
    int32 Assertions=0, Failures=0;
    float ExpectedB=1, ExpectedG=1;
    FString Directory;
    int32 Step=0;
    double Next=0, Deadline=0;
    void Check(bool Result,const FString& Label)
    {
        ++Assertions;
        if (!Result) { ++Failures; UE_LOG(LogTemp,Error,TEXT("CalibrationCheck FAIL %s"),*Label); }
        else UE_LOG(LogTemp,Display,TEXT("CalibrationCheck PASS %s"),*Label);
    }
    static void PrepareWorld(UWorld* InWorld)
    {
        if (!InWorld || !InWorld->IsGameWorld()) return;
        for (TActorIterator<ACharacter> I(InWorld);I;++I)
        {
            if (AAIController* AI=Cast<AAIController>(I->GetController()))
            { if (AI->GetBrainComponent()) AI->GetBrainComponent()->StopLogic(TEXT("Calibration render check")); AI->StopMovement(); }
            I->GetCharacterMovement()->DisableMovement();
        }
        TArray<UUserWidget*> Menus; UWidgetBlueprintLibrary::GetAllWidgetsOfClass(InWorld,Menus,UUserWidget::StaticClass(),true);
        for (auto* Menu:Menus) Menu->SetVisibility(ESlateVisibility::Hidden);
        if (auto* PC=InWorld->GetFirstPlayerController()) { PC->SetIgnoreLookInput(true); PC->SetIgnoreMoveInput(true); }
    }
    void OpenWidget()
    {
        Widget=CreateWidget<UKnightSettingsWidget>(World->GetFirstPlayerController(),UKnightSettingsWidget::GetSharedWidgetClass());
        Widget->AddToViewport(); Widget->Open(); Widget->SetTab(3); Widget->SetVisibility(ESlateVisibility::Hidden);
    }
    void Travel(const TCHAR* Map,const TCHAR* Name)
    {
        if (Widget.IsValid()) Widget->RemoveFromParent();
        Widget.Reset(); UGameplayStatics::OpenLevel(World.Get(),FName(Map));
        PendingCapture=Name; Next=FPlatformTime::Seconds()+12;
    }
    void Capture(const TCHAR* Name)
    {
        auto* PC=World->GetFirstPlayerController();
        auto* Settings=PC->GetLocalPlayer()->GetSubsystem<UknightSettingsSubsystem>();
        float F,B,G; Settings->GetEffectiveDisplaySettings(F,B,G);
        Check(FMath::IsNearlyEqual(B,ExpectedB) && FMath::IsNearlyEqual(G,ExpectedG),FString(Name)+TEXT(" effective values"));
        FVector4 ExpectedGain=Baseline->Gain, ExpectedGamma=Baseline->Gamma;
        ExpectedGain.W*=B; ExpectedGamma.W*=G;
        Check(Observer->Frames>0 && ExpectedGain.Equals(Observer->Gain,.0001f) && ExpectedGamma.Equals(Observer->Gamma,.0001f),FString(Name)+TEXT(" final render composition"));
        Check(Baseline->Exposure==Observer->Exposure && Baseline->Method==Observer->Method,FString(Name)+TEXT(" exposure unchanged"));
        UE_LOG(LogTemp,Display,TEXT("CalibrationCheck baseline gain=%s gamma=%s world=%s"),*Baseline->Gain.ToString(),*Baseline->Gamma.ToString(),*World->GetPathName());
        FVector2D Size; World->GetGameViewport()->GetViewportSize(Size);
        const TArray<FPostProcessSettings>* Blends; const TArray<float>* Weights;
        PC->PlayerCameraManager->GetCachedPostProcessBlends(Blends,Weights);
        UE_LOG(LogTemp,Display,TEXT("CalibrationCheck %s UI=%s Effective=%.3f,%.3f Saved=%.3f,%.3f FinalGain=%s FinalGamma=%s Exposure=%.3f Method=%d Frames=%d Blends=%d Viewport=%.0fx%.0f Camera=%s"),
            Name,*GetNameSafe(Widget.Get()),B,G,Settings->GetSharedSettings()->Brightness,Settings->GetSharedSettings()->Gamma,
            *Observer->Gain.ToString(),*Observer->Gamma.ToString(),Observer->Exposure,Observer->Method,Observer->Frames,Blends->Num(),Size.X,Size.Y,*PC->PlayerCameraManager->GetClass()->GetPathName());
        TArray<FKnightSettingSection> Sections; if(Widget.IsValid()) Widget->GetTabSections(3,Sections);
        for(const auto& Section:Sections) for(const auto& Item:Section.Items)
            if(Item.Id==TEXT("Brightness")||Item.Id==TEXT("Gamma"))
            {
                // Number() intentionally formats one decimal ("100.0"); compare the numeric value.
                UE_LOG(LogTemp,Display,TEXT("CalibrationCheck UI %s=%s"),*Item.Id.ToString(),*Item.Value);
                Check(FMath::IsNearlyEqual(FCString::Atof(*Item.Value),(Item.Id==TEXT("Brightness")?B:G)*100,.01f),FString(Name)+TEXT(" UI ")+Item.Id.ToString());
            }
        FScreenshotRequest::RequestScreenshot(FPaths::Combine(Directory,FString(Name)+TEXT(".png")),true,false);
    }
    void Edit(const TCHAR* B,const TCHAR* G)
    {
        // Broadcast the actual WBP slider delegate. This checks binding, not pointer interaction.
        for (FName Id:{FName(TEXT("Brightness")),FName(TEXT("Gamma"))})
        {
            bool Found=false;
            auto* Panel=Cast<UPanelWidget>(Widget->WidgetTree->FindWidget(TEXT("Box_Rows")));
            if(Panel) for(auto* Child:Panel->GetAllChildren())
                if(auto* Row=Cast<UknightSettingRow>(Child); Row && Row->GetItem().Id==Id)
                    if(auto* Slider=Cast<USlider>(Row->WidgetTree->FindWidget(TEXT("Slider_Value"))))
                    { Slider->OnValueChanged.Broadcast(FCString::Atof(Id==TEXT("Brightness")?B:G)); Found=true; break; }
            Check(Found,TEXT("WBP slider delegate ")+Id.ToString());
        }
        ExpectedB=FCString::Atof(B)/100.f; ExpectedG=FCString::Atof(G)/100.f;
    }
    bool Tick(float)
    {
        const double Now=FPlatformTime::Seconds();
        World=Instance.IsValid()?Instance->GetWorld():nullptr;
        if(!World.IsValid() || Now>Deadline) { UE_LOG(LogTemp,Error,TEXT("CalibrationCheck TIMEOUT")); FPlatformMisc::RequestExit(false); return false; }
        if(Now<Next || FScreenshotRequest::IsScreenshotRequested()) return true;
        auto* PC=World->GetFirstPlayerController();
        if(!PC || !PC->GetLocalPlayer() || !PC->PlayerCameraManager || !World->GetGameViewport()) return true;
        // A separate tick for screenshots prevents the next state leaking into the capture.
        if(!PendingCapture.IsEmpty()) { Capture(*PendingCapture); PendingCapture.Empty(); Next=Now+1; return true; }
        switch(Step)
        {
        case 0:
        {
            UE_LOG(LogTemp,Display,TEXT("CalibrationCheck world=%s settings=%s SavedDir=%s"),*World->GetPathName(),*GEngine->GetGameUserSettings()->GetClass()->GetPathName(),*FPaths::ProjectSavedDir());
            for(const TCHAR* Key:{TEXT("r.TonemapperGamma"),TEXT("r.HDR.EnableHDROutput"),TEXT("r.HDR.Display.OutputDevice"),TEXT("sg.PostProcessQuality"),TEXT("r.Color.Grading"),TEXT("r.NGX.DLSS.Enable"),TEXT("t.MaxFPS"),TEXT("r.VSync")})
                if(auto* V=IConsoleManager::Get().FindConsoleVariable(Key)) UE_LOG(LogTemp,Display,TEXT("CalibrationCheck %s=%s flags=%x"),Key,*V->GetString(),V->GetFlags());
            // Opaque menus and title video aren't scene pixels. Hide them only in this isolated process.
            TArray<UUserWidget*> Menus; UWidgetBlueprintLibrary::GetAllWidgetsOfClass(World.Get(),Menus,UUserWidget::StaticClass(),true);
            for(auto* Menu:Menus) Menu->SetVisibility(ESlateVisibility::Hidden);
            PrepareWorld(World.Get()); OpenWidget();
            Baseline=FSceneViewExtensions::NewExtension<FCalibrationObserver>(100);
            Observer=FSceneViewExtensions::NewExtension<FCalibrationObserver>(-100);
            MapHandle=FCoreUObjectDelegates::PostLoadMapWithWorld.AddStatic(&PrepareWorld);
            auto* Saved=PC->GetLocalPlayer()->GetSubsystem<UknightSettingsSubsystem>()->GetSharedSettings();
            ExpectedB=Saved->Brightness; ExpectedG=Saved->Gamma;
            PC->SetIgnoreLookInput(true); PC->SetIgnoreMoveInput(true);
            PendingCapture=TEXT("00_loaded"); Next=Now+10; break;
        }
        case 1: Edit(TEXT("100"),TEXT("100")); PendingCapture=TEXT("01_default"); break;
        case 2: Edit(TEXT("80"),TEXT("100")); PendingCapture=TEXT("02_brightness_min"); break;
        case 3: Edit(TEXT("120"),TEXT("100")); PendingCapture=TEXT("03_brightness_max"); break;
        case 4: Edit(TEXT("100"),TEXT("80")); PendingCapture=TEXT("04_gamma_min"); break;
        case 5: Edit(TEXT("100"),TEXT("120")); PendingCapture=TEXT("05_gamma_max"); break;
        case 6: Edit(TEXT("120"),TEXT("120")); PendingCapture=TEXT("06_combined"); break;
        case 7: Widget->Apply(); PendingCapture=TEXT("07_applied"); break;
        case 8: PC->SetPause(true); Edit(TEXT("80"),TEXT("80")); PendingCapture=TEXT("08_paused_preview"); break;
        case 9: Widget->Revert(); ExpectedB=ExpectedG=1.2f; PendingCapture=TEXT("09_cancel"); break;
        case 10: Widget->RestoreDefaults(); ExpectedB=ExpectedG=1; PendingCapture=TEXT("10_reset_preview"); break;
        case 11: Widget->Revert(); PC->SetPause(false); Widget->RemoveFromParent(); OpenWidget(); ExpectedB=ExpectedG=1.2f; PendingCapture=TEXT("11_reopened"); break;
        case 12: Widget->SetVisibility(ESlateVisibility::Visible); PendingCapture=TEXT("12_visible_calibration"); break;
        case 13:
            Widget->SetVisibility(ESlateVisibility::Hidden);
            for(TActorIterator<AKnightMaintenanceCameraRig> I(World.Get());I;++I) I->SetMaintenanceView(true);
            PendingCapture=TEXT("13_maintenance"); break;
        case 14:
            for(TActorIterator<AKnightMaintenanceCameraRig> I(World.Get());I;++I) I->SetMaintenanceView(false);
            PendingCapture=TEXT("14_lobby_return"); break;
        case 15: Travel(TEXT("/Game/Knight/Maps/L_Title"),TEXT("15_title")); break;
        case 16: Travel(TEXT("/Game/Knight/Maps/L_MainMenu"),TEXT("16_title_to_menu")); break;
        case 17: Travel(TEXT("/Game/Knight/Maps/L_Raid01"),TEXT("17_menu_to_raid")); break;
        case 18: OpenWidget(); Edit(TEXT("100"),TEXT("100")); PendingCapture=TEXT("18_raid_default"); break;
        case 19: Edit(TEXT("120"),TEXT("120")); PendingCapture=TEXT("19_raid_combined"); break;
        case 20: Widget->Revert(); PendingCapture=TEXT("20_raid_saved"); break;
        default:
            UE_LOG(LogTemp,Display,TEXT("CalibrationCheck COMPLETE assertions=%d failures=%d (WBP delegates/direct handlers, real RHI; no interactive input)"),Assertions,Failures);
            FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(MapHandle);
            if(Widget.IsValid()) Widget->RemoveFromParent();
            Observer.Reset(); Baseline.Reset(); FPlatformMisc::RequestExit(false); return false;
        }
        if((Step>0 && Step<15) || Step>17) Next=Now+4;
        ++Step; return true;
    }
};
void StartCalibrationCheck(const TArray<FString>& Args,UWorld* World)
{
    FString UserDir,Namespace;
    if(!World || Args.Num()!=1 || !FParse::Value(FCommandLine::Get(),TEXT("UserDir="),UserDir) || !UserDir.Contains(TEXT("KnightCalibration"))
        || !FParse::Value(FCommandLine::Get(),TEXT("KnightSaveTestNamespace="),Namespace) || !Namespace.StartsWith(TEXT("Calibration")))
    { UE_LOG(LogTemp,Error,TEXT("CalibrationCheck requires isolated KnightCalibration UserDir and Calibration save namespace")); return; }
    auto Run=MakeShared<FCalibrationCheck>(); Run->World=World; Run->Instance=World->GetGameInstance(); Run->Directory=Args[0]; Run->Deadline=FPlatformTime::Seconds()+600.;
    FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Run](float Delta){return Run->Tick(Delta);}));
}
FAutoConsoleCommandWithWorldAndArgs Command(TEXT("knight.CalibrationCheck"),TEXT("Isolated rendered calibration regression."),FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&StartCalibrationCheck),ECVF_Cheat);
}
#endif
