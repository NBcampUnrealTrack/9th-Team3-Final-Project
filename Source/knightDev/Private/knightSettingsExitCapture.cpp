// Opt-in real hangar render capture; never touches the user's settings or profiles.
#if WITH_EDITOR
#include "Screens/knightSettingsScreen.h"
#include "Menu/UI/KnightMainMenuWidget.h"
#include "Menu/UI/KnightTitleMenuWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "CommonButtonBase.h"
#include "Containers/Ticker.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "HAL/IConsoleManager.h"
#include "Misc/App.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"

namespace KnightSettingsExitCapture
{
struct FRun : TSharedFromThis<FRun>
{
	TWeakObjectPtr<UWorld> World;
	TWeakObjectPtr<UKnightMainMenuWidget> Menu;
	TWeakObjectPtr<UKnightTitleMenuWidget> Title;
	bool bTitle=false;
	UUserWidget* Caller() const { return bTitle?static_cast<UUserWidget*>(Title.Get()):Menu.Get(); }
	bool AtHome() const { return bTitle?Title->GetCurrentPage()==EKnightTitlePage::Home:Menu->GetCurrentPage()==EKnightMainMenuPage::Home; }
	TWeakObjectPtr<UknightSettingsScreen> Screen;
	FString Out, Resolution;
	int32 Step=0,Tab=0,Frame=0;
	double Next=0,Deadline=0;
	bool bSized=false,bFixed=false;
	double OldDelta=0;
	FVector CameraLocation;
	FRotator CameraRotation;
	void RestoreClock() { FApp::SetUseFixedTimeStep(bFixed);FApp::SetFixedDeltaTime(OldDelta); }
	bool Tick(float)
	{
		const double Now=FPlatformTime::Seconds();
		if(!World.IsValid()||Now>Deadline)
		{ UE_LOG(LogTemp,Error,TEXT("SettingsExitCapture timeout step=%d"),Step);RestoreClock();FPlatformMisc::RequestExit(false);return false; }
		if(Now<Next||FScreenshotRequest::IsScreenshotRequested())return true;
		if(Step==0)
		{
			TArray<UUserWidget*> Widgets;UWidgetBlueprintLibrary::GetAllWidgetsOfClass(World.Get(),Widgets,bTitle?UKnightTitleMenuWidget::StaticClass():UKnightMainMenuWidget::StaticClass(),false);
			for(auto* W:Widgets)if(W->IsInViewport()) { if(bTitle)Title=Cast<UKnightTitleMenuWidget>(W);else Menu=Cast<UKnightMainMenuWidget>(W); }
			if(!Caller())return true;
			if(bTitle)
			{
				UWidget* Intro=Title->GetWidgetFromName(TEXT("Panel_Intro"));
				if(Intro&&Intro->GetVisibility()!=ESlateVisibility::Collapsed)
				{
					FSlateApplication::Get().ProcessKeyDownEvent(FKeyEvent(EKeys::Enter,FModifierKeysState(),0,false,0,0));
					FSlateApplication::Get().ProcessKeyUpEvent(FKeyEvent(EKeys::Enter,FModifierKeysState(),0,false,0,0));
					Next=Now+.6;return true;
				}
			}
			if(!bSized) { World->GetFirstPlayerController()->ConsoleCommand(TEXT("r.SetRes ")+Resolution+TEXT("w"),false);bSized=true;Next=Now+2;return true; }
			if(!(bTitle?Title->ShowPage(EKnightTitlePage::Settings):Menu->ShowPage(EKnightMainMenuPage::Settings)))return true;
			Widgets.Reset();UWidgetBlueprintLibrary::GetAllWidgetsOfClass(World.Get(),Widgets,UknightSettingsScreen::StaticClass(),false);
			for(auto* W:Widgets)if(W->IsInViewport())Screen=Cast<UknightSettingsScreen>(W);
			if(!Screen.IsValid())return true;
			Screen->SetTab(Tab);Screen->FocusSettings();Next=Now+1.5;Step=1;return true;
		}
		if(Step==1)
		{
			FApp::SetUseFixedTimeStep(true);FApp::SetFixedDeltaTime(1.0/60.0);
			CameraLocation=World->GetFirstPlayerController()->PlayerCameraManager->GetCameraLocation();
			CameraRotation=World->GetFirstPlayerController()->PlayerCameraManager->GetCameraRotation();
			FScreenshotRequest::RequestScreenshot(Out/FString::Printf(TEXT("Tab%d_Before.png"),Tab),true,false);
			Step=2;return true;
		}
		if(Step==2)
		{
			if(Tab==1||Tab==2)
			{
				const FKey Key=Tab==1?EKeys::Escape:EKeys::Gamepad_FaceButton_Right;
				FSlateApplication::Get().ProcessKeyDownEvent(FKeyEvent(Key,FModifierKeysState(),0,false,0,0));
				FSlateApplication::Get().ProcessKeyDownEvent(FKeyEvent(Key,FModifierKeysState(),0,true,0,0));
			}
			else if(auto* Back=Cast<UCommonButtonBase>(Screen->GetWidgetFromName(TEXT("Btn_Back"))))Back->OnClicked().Broadcast();
			if(!Screen->IsClosing()) { UE_LOG(LogTemp,Error,TEXT("SettingsExitCapture input did not close tab=%d"),Tab);Screen->RequestClose(); }
			Frame=0;Step=3;
		}
		if(Step==3)
		{
			UE_LOG(LogTemp,Display,TEXT("SettingsExitCapture FRAME tab=%d frame=%d elapsed=%.4f visible=%d page=%d"),Tab,Frame,Screen->GetExitElapsed(),Screen->IsVisible(),AtHome()?0:3);
			FScreenshotRequest::RequestScreenshot(Out/FString::Printf(TEXT("Tab%d_%03d.png"),Tab,Frame++),true,false);
			Screen->RequestClose(); // Repeated requests must not restart the exit.
			if(AtHome()) { Step=4;Next=Now+.2; }
			return true;
		}
		if(Step==4)
		{
			if(Tab==1||Tab==2)
			{
				const FKey Key=Tab==1?EKeys::Escape:EKeys::Gamepad_FaceButton_Right;
				FSlateApplication::Get().ProcessKeyDownEvent(FKeyEvent(Key,FModifierKeysState(),0,true,0,0));
				FSlateApplication::Get().ProcessKeyUpEvent(FKeyEvent(Key,FModifierKeysState(),0,false,0,0));
			}
			UWidget* Focus=Caller()->GetWidgetFromName(TEXT("Button_Settings"));
			const bool Good=AtHome()&&!Screen->IsInViewport()&&Focus&&(Focus->HasUserFocus(World->GetFirstPlayerController())||Focus->HasFocusedDescendants());
			const APlayerCameraManager* Camera=World->GetFirstPlayerController()->PlayerCameraManager;
			const bool SameCamera=CameraLocation.Equals(Camera->GetCameraLocation(),.01f)&&CameraRotation.Equals(Camera->GetCameraRotation(),.01f);
			UE_LOG(LogTemp,Display,TEXT("SettingsExitCapture RESULT tab=%d returnedAndFocused=%d cameraUnchanged=%d frames=%d"),Tab,Good,SameCamera,Frame);
			if(!Good||!SameCamera)UE_LOG(LogTemp,Error,TEXT("SettingsExitCapture validation failed"));
			FScreenshotRequest::RequestScreenshot(Out/FString::Printf(TEXT("Tab%d_Return.png"),Tab),true,false);
			RestoreClock();++Tab;Step=Tab<4?0:5;Next=Now+.5;return true;
		}
		UE_LOG(LogTemp,Display,TEXT("SettingsExitCapture COMPLETE"));RestoreClock();FPlatformMisc::RequestExit(false);return false;
	}
};
static FAutoConsoleCommandWithWorldAndArgs Command(TEXT("knight.UI.CaptureSettingsExit"),TEXT("Isolated SettingsStatic exit sequence: output-folder WIDTHxHEIGHT."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args,UWorld* World)
	{
		FString UserDir,Namespace;
		if(!World||(Args.Num()!=2&&Args.Num()!=3)||!FParse::Value(FCommandLine::Get(),TEXT("UserDir="),UserDir)||!UserDir.Contains(TEXT("SettingsStatic"))
			||!FParse::Value(FCommandLine::Get(),TEXT("KnightSaveTestNamespace="),Namespace)||!Namespace.StartsWith(TEXT("SettingsStatic")))return;
		auto Run=MakeShared<FRun>();Run->World=World;Run->Out=Args[0];Run->Resolution=Args[1];Run->bTitle=Args.Num()==3&&Args[2]==TEXT("Title");Run->Deadline=FPlatformTime::Seconds()+240;
		Run->bFixed=FApp::UseFixedTimeStep();Run->OldDelta=FApp::GetFixedDeltaTime();
		IFileManager::Get().MakeDirectory(*Run->Out,true);
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Run](float Dt){return Run->Tick(Dt);}));
	}));
}
#endif
