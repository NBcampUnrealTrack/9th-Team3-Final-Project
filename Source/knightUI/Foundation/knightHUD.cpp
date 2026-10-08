#include "Foundation/knightHUD.h"

#include "CommonActivatableWidget.h"
#include "Engine/LocalPlayer.h"
#include "Foundation/knightPrimaryGameLayout.h"
#include "Foundation/knightUISettings.h"
#include "Foundation/knightUISubsystem.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Screens/knightPauseMenu.h"
#include "Widgets/CommonActivatableWidgetContainer.h"
#include "knightPlayerController.h"
#include "knightUI.h"
#include "knightUIGameplayTags.h"

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
