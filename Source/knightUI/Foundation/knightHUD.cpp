#include "Foundation/knightHUD.h"

#include "CommonActivatableWidget.h"
#include "Engine/LocalPlayer.h"
#include "Foundation/knightPrimaryGameLayout.h"
#include "Foundation/knightUISubsystem.h"
#include "GameFramework/PlayerController.h"
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
}

void AknightHUD::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UCommonActivatableWidget* const Widget = HUDLayout.Get())
	{
		Widget->DeactivateWidget();
	}
	HUDLayout.Reset();
	Super::EndPlay(EndPlayReason);
}
