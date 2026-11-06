#include "Foundation/knightHUD.h"

#include "CommonActivatableWidget.h"
#include "Engine/LocalPlayer.h"
#include "Foundation/knightPrimaryGameLayout.h"
#include "Foundation/knightUISettings.h"
#include "Foundation/knightUISubsystem.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Blueprint/WidgetTree.h"
#include "Components/PanelWidget.h"
#include "HUD/knightHUDFinishBanner.h"
#include "HUD/knightHUDMessageLog.h"
#include "Monster/knightMonsterDefinition.h"
#include "Raid/knightRaidDefinition.h"
#include "Raid/knightRaidGameState.h"
#include "Engine/GameInstance.h"
#include "Raid/knightRaidSubsystem.h"
#include "Screens/knightPauseMenu.h"
#include "Screens/knightRaidResultScreen.h"
#include "ViewModels/knightVM_Raid.h"
#include "Widgets/CommonActivatableWidgetContainer.h"
#include "knightPlayerController.h"
#include "knightUI.h"
#include "knightUIGameplayTags.h"

#define LOCTEXT_NAMESPACE "knightHUD"

namespace knightHUDResult
{
	/** HUD 레이아웃(WBP_KnightHUDLayout)에서 처치 연출 중에도 남기는 아래 대화창 이름. */
	const FName MessageLogName(TEXT("MessageLog"));
	/** 판독 창은 결과 화면(UI.Layer.Menu, 레이아웃은 플레이어 화면 1000)보다 위에 올린다. */
	constexpr int32 BannerZOrder = 1100;
}

AknightHUD::AknightHUD()
{
	RaidSuccessLine = LOCTEXT("RaidSuccessLine", "목표 생명 반응 소실 확인. 토벌 성공입니다.");
	KnightDestroyedLine = LOCTEXT("KnightDestroyedLine", "기체 손상 한계. 작전을 중단합니다.");
	TimeUpLine = LOCTEXT("TimeUpLine", "작전 제한 시간 초과. 기지로 귀환합니다.");
}

void AknightHUD::BeginPlay()
{
	Super::BeginPlay();

	APlayerController* const PC = GetOwningPlayerController();
	const ULocalPlayer* const LocalPlayer = PC ? PC->GetLocalPlayer() : nullptr;
	UknightUISubsystem* const UI = LocalPlayer ? LocalPlayer->GetSubsystem<UknightUISubsystem>() : nullptr;
	if (!UI || !UI->GetOrCreateLayout(PC))
	{
		UE_LOG(LogknightUI, Warning, TEXT("%s: no UI layout; the HUD is not shown."), *GetName());
		return;
	}

	if (UClass* const LayoutClass = HUDLayoutClass.LoadSynchronous())
	{
		HUDLayout = UI->PushWidgetToLayer(knightUIGameplayTags::UI_Layer_Game, LayoutClass);
	}

	if (AknightPlayerController* const KnightPC = Cast<AknightPlayerController>(PC))
	{
		KnightPC->PauseMenuRequestHandler.BindUObject(this, &ThisClass::TogglePauseMenu);
	}

	if (UknightVM_Raid* const VM = UI->GetRaidViewModel())
	{
		RaidViewModel = VM;
		VM->AddFieldValueChangedDelegate(UknightVM_Raid::FFieldNotificationClassDescriptor::Phase,
			INotifyFieldValueChanged::FFieldValueChangedDelegate::CreateUObject(this, &ThisClass::HandleRaidViewModelChanged));
	}

	if (AknightRaidGameState* const RaidState = GetWorld() ? GetWorld()->GetGameState<AknightRaidGameState>() : nullptr)
	{
		FinishStageHandle = RaidState->OnFinishStageChanged.AddUObject(this, &ThisClass::HandleFinishStageChanged);
	}

	// 처음 죽거나 Esc를 누를 때 그 자리에서 읽느라 멈추지 않게, 로딩 화면이 떠 있는 지금 미리 읽어 둔다.
	const UknightUISettings* const Settings = GetDefault<UknightUISettings>();
	for (UClass* const ScreenClass : { static_cast<UClass*>(Settings->RaidResultScreenClass.LoadSynchronous()),
		static_cast<UClass*>(Settings->PauseMenuClass.LoadSynchronous()) })
	{
		if (ScreenClass)
		{
			PreloadedScreenClasses.Add(ScreenClass);
		}
	}
}

void AknightHUD::HandleRaidViewModelChanged(UObject* InViewModel, UE::FieldNotification::FFieldId FieldId)
{
	const UknightVM_Raid* const VM = RaidViewModel.Get();
	const EknightRaidPhase Phase = VM ? VM->GetPhase() : EknightRaidPhase::None;
	if (Phase != EknightRaidPhase::Success && Phase != EknightRaidPhase::Failed)
	{
		return;
	}
	// 처치 연출 중이면 결과 화면은 연출이 끝날 때(Done) 연다.
	const AknightRaidGameState* const RaidState = GetWorld() ? GetWorld()->GetGameState<AknightRaidGameState>() : nullptr;
	if (RaidState && RaidState->IsFinishPresentationPlaying())
	{
		return;
	}
	// Abandoning travels back in the same frame; the menu shows that result instead.
	const UGameInstance* const GameInstance = GetGameInstance();
	const UknightRaidSubsystem* const RaidSubsystem = GameInstance ? GameInstance->GetSubsystem<UknightRaidSubsystem>() : nullptr;
	FknightRaidResult Result;
	const bool bHasResult = RaidSubsystem && RaidSubsystem->GetLastResult(Result);
	if (bHasResult && Result.FailReason == EknightRaidFailReason::Abandoned)
	{
		return;
	}
	if (Phase == EknightRaidPhase::Failed && !FinishBanner && !FailBannerTicker.IsValid() && FailBannerHoldTime > 0.f)
	{
		// 실패: 빨간 판독 창을 먼저 혼자 보여 주고 결과 화면을 연다. 귀환 타이머는 지금부터 돈다.
		FailWorldTime = GetWorld() ? GetWorld()->GetTimeSeconds() : -1.0;
		ShowFinishBanner(true, bHasResult ? Result.FailReason : EknightRaidFailReason::KnightDestroyed);
		FailBannerTicker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateWeakLambda(this, [this](float)
		{
			FailBannerTicker.Reset();
			OpenRaidResult();
			return false;
		}), FailBannerHoldTime);
		return;
	}
	OpenRaidResult();
}

void AknightHUD::OpenRaidResult()
{
	if (UCommonActivatableWidget* const Open = RaidResult.Get(); Open && Open->IsActivated())
	{
		return;
	}
	APlayerController* const PC = GetOwningPlayerController();
	const ULocalPlayer* const LocalPlayer = PC ? PC->GetLocalPlayer() : nullptr;
	UknightUISubsystem* const UI = LocalPlayer ? LocalPlayer->GetSubsystem<UknightUISubsystem>() : nullptr;
	if (!UI || !UI->GetLayout())
	{
		return;
	}
	if (UCommonActivatableWidget* const Menu = PauseMenu.Get())
	{
		Menu->DeactivateWidget();
	}
	if (UClass* const ScreenClass = GetDefault<UknightUISettings>()->RaidResultScreenClass.LoadSynchronous())
	{
		// 첫 사망 때 멈춤을 확인하려고 여는 데 걸린 시간을 남긴다.
		const double StartSeconds = FPlatformTime::Seconds();
		RaidResult = UI->PushWidgetToLayer(knightUIGameplayTags::UI_Layer_Menu, ScreenClass);
		UE_LOG(LogknightUI, Log, TEXT("Raid result screen opened in %.0f ms."), (FPlatformTime::Seconds() - StartSeconds) * 1000.0);
		if (UCommonActivatableWidget* const Screen = RaidResult.Get())
		{
			if (UknightRaidResultScreen* const ResultScreen = Cast<UknightRaidResultScreen>(Screen); ResultScreen && FailWorldTime >= 0.0)
			{
				ResultScreen->SetReturnStartTime(FailWorldTime);
			}
			// 판독 창은 결과 화면의 제목으로 남았다가 같이 닫힌다.
			Screen->OnDeactivated().AddUObject(this, &ThisClass::RemoveFinishBanner);
		}
		if (FinishBanner)
		{
			FinishBanner->BeginRise();
		}
	}
	else
	{
		UE_LOG(LogknightUI, Error, TEXT("Knight UI settings have no RaidResultScreenClass."));
	}
}

void AknightHUD::HandleFinishStageChanged(EknightRaidFinishStage Stage)
{
	switch (Stage)
	{
	case EknightRaidFinishStage::Slow:
		// 슬로모션 컷과 쓰러지는 장면은 화면만 보이게 한다(대화창만 남긴다).
		HideHUDForFinish();
		break;
	case EknightRaidFinishStage::Banner:
		ShowFinishBanner(false);
		break;
	case EknightRaidFinishStage::Done:
		// 판독 창은 지우지 않는다. 결과 화면이 열리면 위로 올라가 제목이 된다.
		RestoreHUDAfterFinish();
		OpenRaidResult();
		break;
	default:
		break;
	}
}

void AknightHUD::ShowFinishBanner(bool bFailure, EknightRaidFailReason FailReason)
{
	APlayerController* const PC = GetOwningPlayerController();
	const AknightRaidGameState* const RaidState = GetWorld() ? GetWorld()->GetGameState<AknightRaidGameState>() : nullptr;
	if (!PC || FinishBanner)
	{
		return;
	}
	const UknightRaidDefinition* const Raid = RaidState ? RaidState->GetRaidDefinition() : nullptr;
	const UknightMonsterDefinition* const Target = Raid ? Raid->TargetMonster.Get() : nullptr;
	const FText FailStatus = FailReason == EknightRaidFailReason::TimeUp ? LOCTEXT("TimeUpStatus", "TIME OVER") : LOCTEXT("KnightDownStatus", "KNIGHT DOWN");
	FinishBanner = CreateWidget<UknightHUDFinishBanner>(PC, UknightHUDFinishBanner::StaticClass());
	if (FinishBanner)
	{
		FinishBanner->Setup(Target ? Target->DisplayName : FText::GetEmpty(), RaidState ? RaidState->GetElapsedRaidTime() : 0.f,
			bFailure ? FailBannerHoldTime : (Raid ? Raid->FinishPresentation.BannerTime : 3.5f), bFailure, FailStatus);
		FinishBanner->AddToPlayerScreen(knightHUDResult::BannerZOrder);
	}

	const FText& Line = !bFailure ? RaidSuccessLine : FailReason == EknightRaidFailReason::TimeUp ? TimeUpLine : KnightDestroyedLine;
	UUserWidget* const Layout = HUDLayout.Get();
	if (UknightHUDMessageLog* const Log = Layout ? Cast<UknightHUDMessageLog>(Layout->GetWidgetFromName(knightHUDResult::MessageLogName)) : nullptr;
		Log && !Line.IsEmpty())
	{
		Log->AddLine(Line, false);
	}
}

void AknightHUD::HideHUDForFinish()
{
	UCommonActivatableWidget* const Layout = HUDLayout.Get();
	UPanelWidget* const Root = Layout ? Cast<UPanelWidget>(Layout->GetRootWidget()) : nullptr;
	if (!Root || bHiddenForFinish)
	{
		return;
	}
	bHiddenForFinish = true;
	for (UWidget* const Child : Root->GetAllChildren())
	{
		if (Child && Child->GetFName() != knightHUDResult::MessageLogName && Child->GetVisibility() != ESlateVisibility::Collapsed)
		{
			HiddenForFinish.Emplace(Child, Child->GetVisibility());
			Child->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
}

void AknightHUD::RestoreHUDAfterFinish()
{
	for (const TPair<TWeakObjectPtr<UWidget>, ESlateVisibility>& Entry : HiddenForFinish)
	{
		if (UWidget* const Child = Entry.Key.Get())
		{
			Child->SetVisibility(Entry.Value);
		}
	}
	HiddenForFinish.Reset();
	bHiddenForFinish = false;
}

void AknightHUD::StopFailBannerTicker()
{
	if (FailBannerTicker.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(FailBannerTicker);
		FailBannerTicker.Reset();
	}
}

void AknightHUD::RemoveFinishBanner()
{
	if (FinishBanner)
	{
		FinishBanner->RemoveFromParent();
		FinishBanner = nullptr;
	}
}

void AknightHUD::TogglePauseMenu()
{
	APlayerController* const PC = GetOwningPlayerController();
	const ULocalPlayer* const LocalPlayer = PC ? PC->GetLocalPlayer() : nullptr;
	UknightUISubsystem* const UI = LocalPlayer ? LocalPlayer->GetSubsystem<UknightUISubsystem>() : nullptr;
	const UknightPrimaryGameLayout* const Layout = UI ? UI->GetLayout() : nullptr;
	if (!Layout)
	{
		return;
	}

	// A confirm modal answers its own Esc. This only runs when focus left it, and must not close the menu beneath.
	const UCommonActivatableWidgetContainerBase* const ModalLayer = Layout->GetLayer(knightUIGameplayTags::UI_Layer_Modal);
	if (ModalLayer && ModalLayer->GetActiveWidget())
	{
		return;
	}

	if (UknightPauseMenu* const OpenMenu = Cast<UknightPauseMenu>(PauseMenu.Get()); OpenMenu && OpenMenu->IsActivated())
	{
		OpenMenu->Resume();
		return;
	}
	if (!UknightPauseMenu::CanOpen(GetWorld()))
	{
		return;
	}
	if (UClass* const MenuClass = GetDefault<UknightUISettings>()->PauseMenuClass.LoadSynchronous())
	{
		PauseMenu = UI->PushWidgetToLayer(knightUIGameplayTags::UI_Layer_Menu, MenuClass);
	}
	else
	{
		UE_LOG(LogknightUI, Error, TEXT("Knight UI settings have no PauseMenuClass."));
	}
}

#if !UE_BUILD_SHIPPING
static FAutoConsoleCommandWithWorldAndArgs GknightUITogglePauseCommand(
	TEXT("knight.UI.TogglePause"),
	TEXT("Same as pressing Esc in gameplay: opens or closes the pause menu."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (AknightPlayerController* const PC = Cast<AknightPlayerController>(World ? World->GetFirstPlayerController() : nullptr))
		{
			PC->PauseMenuRequestHandler.ExecuteIfBound();
		}
	}));
#endif

void AknightHUD::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (AknightPlayerController* const KnightPC = Cast<AknightPlayerController>(GetOwningPlayerController()))
	{
		KnightPC->PauseMenuRequestHandler.Unbind();
	}
	if (UknightVM_Raid* const VM = RaidViewModel.Get())
	{
		VM->RemoveAllFieldValueChangedDelegates(this);
	}
	RaidViewModel.Reset();
	if (AknightRaidGameState* const RaidState = GetWorld() ? GetWorld()->GetGameState<AknightRaidGameState>() : nullptr)
	{
		RaidState->OnFinishStageChanged.Remove(FinishStageHandle);
	}
	FinishStageHandle.Reset();
	StopFailBannerTicker();
	RemoveFinishBanner();
	RestoreHUDAfterFinish();
	if (UCommonActivatableWidget* const Screen = RaidResult.Get())
	{
		Screen->DeactivateWidget();
	}
	RaidResult.Reset();
	if (UCommonActivatableWidget* const Menu = PauseMenu.Get())
	{
		Menu->DeactivateWidget();
	}
	PauseMenu.Reset();
	if (UCommonActivatableWidget* const Widget = HUDLayout.Get())
	{
		Widget->DeactivateWidget();
	}
	HUDLayout.Reset();
	Super::EndPlay(EndPlayReason);
}

#undef LOCTEXT_NAMESPACE
