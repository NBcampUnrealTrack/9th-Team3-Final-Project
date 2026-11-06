#include "knightSettingsCheckCommandlet.h"
#include "Screens/knightSettingsScreen.h"
#include "Widgets/knightSettingRow.h"
#include "Settings/knightSettingsSubsystem.h"
#include "Settings/knightSharedSettings.h"
#include "CommonTextBlock.h"
#include "CommonGameViewportClient.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/PanelWidget.h"
#include "Components/ScrollBox.h"
#include "Components/Slider.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
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
#include "RenderingThread.h"

int32 UknightSettingsCheckCommandlet::Main(const FString& Params)
{
    FString UserDir, Out;
    if (!FParse::Value(FCommandLine::Get(),TEXT("UserDir="),UserDir) || !UserDir.Contains(TEXT("SettingsStatic"))
        || !FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()).Contains(TEXT("SettingsStatic"))
        || !FParse::Value(*Params,TEXT("Out="),Out) || !FParse::Param(FCommandLine::Get(),TEXT("AllowCommandletRendering"))) return 1;
    IFileManager::Get().MakeDirectory(*Out,true);
    int32 Checks=0, Failures=0;
    auto Check=[&](bool Pass,const TCHAR* What) { ++Checks; Failures+=!Pass; UE_LOG(LogTemp,Display,TEXT("SettingsStatic %s %s"),Pass?TEXT("PASS"):TEXT("FAIL"),What); };
    if (!FSlateApplication::IsInitialized())
    {
        FSlateApplication::Create();
        FSlateApplication::Get().InitializeRenderer(FModuleManager::LoadModuleChecked<ISlateRHIRendererModule>(TEXT("SlateRHIRenderer")).CreateSlateRHIRenderer());
    }
    auto* GI=NewObject<UGameInstance>(GEngine); GI->AddToRoot(); GI->InitializeStandalone();
    UWorld* World=GI->GetWorld();
    TGuardValue<TSubclassOf<UGameViewportClient>> ViewportClass(GEngine->GameViewportClientClass,UCommonGameViewportClient::StaticClass());
    auto* UI=GetMutableDefault<UUserInterfaceSettings>();
    TGuardValue<decltype(UI->HardwareCursors)> Cursors(UI->HardwareCursors,{});
    auto* Viewport=NewObject<UCommonGameViewportClient>(GEngine); GI->GetWorldContext()->GameViewport=Viewport;
    Viewport->Init(*GI->GetWorldContext(),GI,false);
    auto* Local=NewObject<ULocalPlayer>(GEngine); GI->AddLocalPlayer(Local,FPlatformUserId::CreateFromInternalId(0));
    auto* Player=World->SpawnActor<APlayerController>(); World->AddController(Player); Player->SetPlayer(Local);
    auto* Store=Local->GetSubsystem<UknightSettingsSubsystem>();
    auto* Personal=Store?Store->GetSharedSettings():nullptr;
    Check(UknightGameUserSettings::Get()!=nullptr && Personal,TEXT("configured game settings and local settings store exist"));
    if (!Personal) return 1;
    auto* Screen=CreateWidget<UknightSettingsScreen>(Player,UKnightSettingsWidget::GetSharedWidgetClass());
    if (!Screen)return 1; Screen->AddToRoot();
    TSharedRef<SVirtualWindow> Window=SNew(SVirtualWindow); Window->SetContent(Screen->TakeWidget()); FSlateApplication::Get().RegisterVirtualWindow(Window);
    auto* Renderer=new FWidgetRenderer(true); FHittestGrid Grid; UTextureRenderTarget2D* Target=nullptr; FIntPoint Extent; float Scale=1;
    auto Draw=[&]() { ++GFrameCounter; Renderer->DrawWindow(Target,Grid,Window,Scale,FVector2D(Extent),0); FlushRenderingCommands(); };
    auto Resize=[&](FIntPoint Size)
    {
        if(Target)Target->RemoveFromRoot(); Extent=Size; Scale=UI->GetDPIScaleBasedOnSize(Size); Window->Resize(FVector2D(Size));
        Target=NewObject<UTextureRenderTarget2D>(); Target->AddToRoot(); Target->TargetGamma=1;
        Target->InitCustomFormat(Size.X,Size.Y,PF_FloatRGBA,true); Target->UpdateResourceImmediate(true); Draw(); Draw();
    };
    auto Capture=[&](const FString& Name)
    {
        Draw(); Draw(); TArray<FLinearColor> Linear; TArray<FColor> Pixels;
        Check(Target->GameThread_GetRenderTargetResource()->ReadLinearColorPixels(Linear,FReadSurfaceDataFlags(RCM_MinMax)),TEXT("real Slate RHI readback"));
        for(const auto& P:Linear)Pixels.Add(P.ToFColor(false)); TArray64<uint8> PNG;
        FImageUtils::PNGCompressImageArray(Extent.X,Extent.Y,Pixels,PNG);
        Check(FFileHelper::SaveArrayToFile(PNG,*(Out/(Name+TEXT(".png")))),TEXT("render saved"));
    };
    auto Row=[&](FName Id)->UknightSettingRow* { for(UknightSettingRow* R:Screen->Rows)if(R->GetItem().Id==Id)return R;return nullptr; };
    auto Press=[&](FKey Key) { return Screen->NativeOnPreviewKeyDown(Screen->GetCachedGeometry(),FKeyEvent(Key,FModifierKeysState(),0,false,0,0)); };
    Resize({1920,1080}); Screen->Open(); Draw();
    Check(!Screen->IsDirty()&&!Screen->Btn_Apply->GetIsEnabled()&&!Screen->Btn_Revert->GetIsEnabled(),TEXT("open is read-only; Apply and Revert reflect a clean draft"));
    Check(Screen->GetWidgetFromName(TEXT("Row_TopTabs")) && !Screen->GetWidgetFromName(TEXT("VerticalBox_Tabs")),TEXT("only the horizontal tab navigation exists"));
    Screen->FocusLevel(false); Draw();
    Check(Screen->GetTabButton(0)->HasFocusedDescendants(),TEXT("initial category accepts Slate keyboard focus"));
    Press(EKeys::Gamepad_DPad_Right); Draw();
    Check(Screen->GetCurrentTab()==1,TEXT("controller right navigates horizontal tabs"));
    Screen->GetTabButton(1)->OnTabEntered.ExecuteIfBound(); Draw();
    Check(Screen->Rows.Num()>0&&Screen->Rows[0]->HasFocusedDescendants(),TEXT("Down delegate enters first available setting"));
    if(Screen->Rows.Num()>1)
    {
        const int32 DirtyBefore=Screen->GetDirtyCount();
        Screen->Rows[1]->NativeOnHovered();
        Check(Screen->DetailRow.Get()==Screen->Rows[1],TEXT("mouse hover updates the active description"));
        Screen->Rows[0]->NativeOnPreviewKeyDown(Screen->Rows[0]->GetCachedGeometry(),FKeyEvent(EKeys::LeftShift,FModifierKeysState(),0,false,0,0));
        Check(Screen->DetailRow.Get()==Screen->Rows[0]&&Screen->GetDirtyCount()==DirtyBefore,TEXT("keyboard retakes description without changing a value"));
    }
    Screen->FocusFooter(); Draw(); Check(Screen->Btn_Defaults->HasFocusedDescendants(),TEXT("fixed footer is keyboard reachable"));
    Press(EKeys::Gamepad_DPad_Up); Draw(); Check(Screen->GetTabButton(1)->HasFocusedDescendants(),TEXT("controller up returns from footer to category"));
    const bool Reload=FParse::Param(*Params,TEXT("Reload"));
    if(Reload)
    {
        Check(FMath::IsNearlyEqual(Personal->MasterVolume,.37f)&&FMath::IsNearlyEqual(Personal->Brightness,1.09f)&&FMath::IsNearlyEqual(Personal->Gamma,.94f),TEXT("normal save pipeline persists across a fresh process"));
    }
    else
    {
        const float SavedMaster=Personal->MasterVolume, SavedBrightness=Personal->Brightness;
        Screen->SetTab(2); auto* Master=Row(TEXT("Master")); Check(Master&&Master->Slider_Value,TEXT("real audio slider present"));
        if(Master)Master->Slider_Value->OnValueChanged.Broadcast(37.f);
        Screen->SetTab(3); Screen->OptionEdited(TEXT("Brightness"),TEXT("109")); Screen->OptionEdited(TEXT("Gamma"),TEXT("94"));
        float FOV,Brightness,Gamma; Store->GetEffectiveDisplaySettings(FOV,Brightness,Gamma);
        Check(FMath::IsNearlyEqual(Brightness,1.09f)&&FMath::IsNearlyEqual(Gamma,.94f),TEXT("calibration drafts reach existing preview API independently"));
        Screen->SetTab(0); Screen->SetTab(2);
        Check(Row(TEXT("Master"))&&FCString::Atof(*Row(TEXT("Master"))->GetItem().Value)==37&&Screen->GetDirtyCount()>=3,TEXT("drafts survive category switches"));
        Check(Personal->MasterVolume==SavedMaster&&Personal->Brightness==SavedBrightness,TEXT("editing and navigation do not mutate saved settings"));
        Screen->Btn_Back->OnClicked().Broadcast();
        Check(Screen->bPromptVisible&&!Screen->Panel_Editor->GetIsEnabled(),TEXT("Back opens unsaved prompt and blocks underlying editor")); Capture(TEXT("UnsavedPrompt"));
        Press(EKeys::Escape); Check(!Screen->bPromptVisible&&Screen->IsDirty(),TEXT("Esc on prompt resumes editing without saving"));
        Screen->Btn_Revert->OnClicked().Broadcast(); Store->GetEffectiveDisplaySettings(FOV,Brightness,Gamma);
        Check(!Screen->IsDirty()&&Brightness==SavedBrightness,TEXT("Revert clears all drafts and restores calibration preview"));
        Screen->RestoreDefaults();
        Check(Personal->MasterVolume==SavedMaster&&Personal->Brightness==SavedBrightness,TEXT("Defaults stages its original all-category scope without saving")); Screen->Revert();
        Screen->SetTab(1);
        TArray<UknightSettingRow*> Keys; for(UknightSettingRow* R:Screen->Rows)if(R->GetItem().Type==EKnightSettingControl::Key)Keys.Add(R);
        Check(Keys.Num()>1,TEXT("actual registered Enhanced Input mappings populate controls"));
        if(Keys.Num()>1)
        {
            auto* K=Keys[0]; const FString Before=K->GetItem().Value;
            K->OnClicked().Broadcast(); Check(Screen->IsCapturingKey(),TEXT("key action starts capture"));
            Check(!Press(EKeys::Escape).IsEventHandled(),TEXT("screen passes Esc to key capture"));
            K->NativeOnPreviewKeyDown(K->GetCachedGeometry(),FKeyEvent(EKeys::Escape,FModifierKeysState(),0,false,0,0));
            Check(!Screen->IsCapturingKey()&&K->GetItem().Value==Before&&!Screen->bPromptVisible,TEXT("Esc cancels rebinding without closing settings"));
            Screen->OptionEdited(K->GetItem().Id,Keys[1]->GetItem().Value);
            Check(K->GetItem().Value==Before&&Screen->Txt_Status->GetText().ToString().Contains(TEXT("이미")),TEXT("duplicate mapping rejected through original conflict path"));
            Screen->OptionEdited(K->GetItem().Id,TEXT("F9")); Check(K->GetItem().Value==TEXT("F9"),TEXT("valid key can be staged")); Screen->Revert();
        }
        Screen->SetTab(2); Screen->OptionEdited(TEXT("Master"),TEXT("37")); Screen->OptionEdited(TEXT("Brightness"),TEXT("109")); Screen->OptionEdited(TEXT("Gamma"),TEXT("94"));
        Screen->Btn_Apply->OnClicked().Broadcast();
        Check(!Screen->IsDirty()&&FMath::IsNearlyEqual(Personal->MasterVolume,.37f)&&FMath::IsNearlyEqual(Personal->Gamma,.94f),TEXT("Apply saves through original settings API"));
        Screen->Open(); Check(FMath::IsNearlyEqual(Screen->DraftPersonal->Brightness,1.09f),TEXT("reopening reads applied values"));
        // Commandlets have no monitor list and may start with a 0x0 size. A valid window-size
        // fixture exercises the actual rollback route without a physical display switch.
        FKnightGraphicsChoice RollbackFixture=Screen->SavedGraphics; RollbackFixture.Resolution=FIntPoint(1920,1080);
        Screen->StartDisplayConfirm(RollbackFixture);
        Check(Screen->DisplaySecondsShown==15&&Screen->IsDisplayConfirmShown()&&!Screen->Panel_Editor->GetIsEnabled(),TEXT("15-second display confirmation locks editing")); Capture(TEXT("DisplayConfirm"));
        Screen->DisplayConfirmDeadline=FPlatformTime::Seconds()-.01;
        Check(!Screen->TickDisplayConfirm(0)&&!Screen->IsDisplayConfirmShown()&&Screen->Panel_Editor->GetIsEnabled(),TEXT("expired countdown invokes normal rollback and unlocks editor"));
    }
    for(const auto Size:{FIntPoint(1920,1080),FIntPoint(1280,720),FIntPoint(2560,1080)})
    {
        Resize(Size);
        for(int32 Tab=0;Tab<4;++Tab)
        {
            Screen->SetTab(Tab); Screen->HandleStatusChanged(FText::GetEmpty()); Draw(); Draw();
            TArray<FKnightSettingSection> Sections; Screen->GetTabSections(Tab,Sections); int32 Count=0;
            for(const auto& Section:Sections)for(const auto& Item:Section.Items)
            {
                ++Count; auto* R=Row(Item.Id); Check(R&&R->GetItem().Value==Item.Value&&R->GetIsEnabled()==Item.bEnabled,TEXT("rendered row matches live model value and availability"));
                if(R&&R->Slider_Value)Check(R->Slider_Value->GetMinValue()==Item.Min&&R->Slider_Value->GetMaxValue()==Item.Max,TEXT("slider limits preserved"));
            }
            Check(Count==Screen->Rows.Num(),TEXT("all category options retained"));
            if(Tab==0&&Row(TEXT("Effects")))Screen->HandleRowFocused(Row(TEXT("Effects")),false);
            else if(Screen->Rows.Num()>1)Screen->HandleRowFocused(Screen->Rows[1],false);
            Check(Screen->DetailRow.IsValid()&&!Screen->Txt_DetailTitle->GetText().IsEmpty()&&!Screen->Txt_DetailBody->GetText().IsEmpty(),TEXT("active row supplies current category description"));
            Capture(FString::Printf(TEXT("Tab%d_%dx%d"),Tab,Size.X,Size.Y));
            for(const TCHAR* Name:{TEXT("Row_Header"),TEXT("Panel_Footer"),TEXT("Size_Detail")})
            {
                const auto& G=Screen->GetWidgetFromName(Name)->GetCachedGeometry(); const auto A=G.LocalToAbsolute(FVector2D::ZeroVector), B=G.LocalToAbsolute(G.GetLocalSize());
                Check(A.X>=-.5&&A.Y>=-.5&&B.X<=Size.X+.5&&B.Y<=Size.Y+.5,TEXT("header, footer and description remain in viewport"));
            }
            for(UknightSettingRow* R:Screen->Rows)
            {
                auto* Label=R->GetWidgetFromName(TEXT("Txt_Label")); auto* Control=R->GetWidgetFromName(TEXT("SizeBox_Control"));
                const auto& L=Label->GetCachedGeometry(); const auto& C=Control->GetCachedGeometry();
                Check(L.LocalToAbsolute(L.GetLocalSize()).X<=C.LocalToAbsolute(FVector2D::ZeroVector).X+.5,TEXT("label and value columns never overlap"));
            }
            if(Tab<=1) { Screen->Scroll_Rows->ScrollToEnd(); Capture(FString::Printf(TEXT("Tab%d_Bottom_%dx%d"),Tab,Size.X,Size.Y)); }
        }
    }
    Screen->SetTab(0); Resize({1280,720});
    if(auto* Monitor=Row(TEXT("Display")))
    {
        auto Item=Monitor->GetItem(); Item.Label=FText::FromString(TEXT("매우 긴 현지화된 표시 모니터 항목 이름")); Item.Value=TEXT("fixture");
        Item.Options={Item.Value}; Item.OptionLabels={FText::FromString(TEXT("모니터 2 · 매우 긴 제조사와 모델 이름 ABC12345678901234567890"))};
        Monitor->Setup(Item); Screen->HandleRowFocused(Monitor,false); Capture(TEXT("LongText_720_Fixture"));
        Check(Monitor->Txt_Value->GetToolTipText().EqualTo(Item.OptionLabels[0]),TEXT("ellipsis retains full live value in tooltip"));
    }
    Screen->Revert(); Screen->RequestClose(); Check(!Screen->bPromptVisible,TEXT("clean Back does not fabricate pending edits"));
    Screen->RemoveFromRoot(); Window->SetContent(SNullWidget::NullWidget); FSlateApplication::Get().UnregisterVirtualWindow(Window); Target->RemoveFromRoot(); BeginCleanup(Renderer); FlushRenderingCommands();
    GI->Shutdown(); GEngine->DestroyWorldContext(World); World->DestroyWorld(false); GI->RemoveFromRoot();
    UE_LOG(LogTemp,Display,TEXT("SettingsStatic COMPLETE checks=%d failures=%d; Slate/delegate checks, not physical mouse/controller or monitor-mode testing"),Checks,Failures);
    return Failures?1:0;
}
