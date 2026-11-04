#include "Screens/knightPauseMenu.h"

#include "Engine/World.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Raid/knightRaidGameMode.h"
#include "Raid/knightRaidGameState.h"
#include "Screens/knightConfirmModal.h"
#include "Widgets/knightButtonBase.h"
#include "Menu/UI/KnightButtonFillExtension.h"
#include "knightPlayerController.h"

UknightPauseMenu::UknightPauseMenu(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InputMode = EknightWidgetInputMode::Menu;
}

bool UknightPauseMenu::CanOpen(const UWorld* World)
{
	const AknightRaidGameState* const RaidState = World ? World->GetGameState<AknightRaidGameState>() : nullptr;
	if (!RaidState)
	{
		// Test maps without a raid still get the menu (Abandon is locked there).
		return World != nullptr;
	}
	const EknightRaidPhase Phase = RaidState->GetRaidPhase();
	return Phase != EknightRaidPhase::Success && Phase != EknightRaidPhase::Failed && Phase != EknightRaidPhase::Returning;
}

void UknightPauseMenu::Resume()
{
	DeactivateWidget();
}

void UknightPauseMenu::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	for (UknightButtonBase* Button : { Btn_Resume.Get(), Btn_Settings.Get(), Btn_Abandon.Get() })
		UKnightButtonFillExtension::SetEnabled(Button, true);
	Btn_Resume->OnClicked().AddUObject(this, &ThisClass::Resume);
	Btn_Settings->OnClicked().AddUObject(this, &ThisClass::HandleSettingsClicked);
	Btn_Abandon->OnClicked().AddUObject(this, &ThisClass::HandleAbandonClicked);
}

void UknightPauseMenu::NativeOnActivated()
{
	Super::NativeOnActivated();
	for (UknightButtonBase* Button : { Btn_Resume.Get(), Btn_Settings.Get(), Btn_Abandon.Get() })
		UKnightButtonFillExtension::Reset(Button);

	APlayerController* const PC = GetOwningPlayer();
	if (AknightPlayerController* const KnightPC = Cast<AknightPlayerController>(PC))
	{
		KnightPC->CancelKnockdownPresentation();
	}
	bPausedGame = PC && !UGameplayStatics::IsGamePaused(this) && PC->SetPause(true);
	RefreshAbandonLock();
	Btn_Resume->SetFocus();

	// Fade in. FTSTicker keeps running while the game is paused.
	StopIntro();
	IntroTime = 0.f;
	SetRenderOpacity(IntroDuration > 0.f ? 0.f : 1.f);
	if (IntroDuration > 0.f)
	{
		IntroTicker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &ThisClass::TickIntro));
	}
}

void UknightPauseMenu::NativeOnDeactivated()
{
	for (UknightButtonBase* Button : { Btn_Resume.Get(), Btn_Settings.Get(), Btn_Abandon.Get() })
		UKnightButtonFillExtension::Reset(Button);
	StopIntro();
	SetRenderOpacity(1.f);
	if (AknightPlayerController* const PC = Cast<AknightPlayerController>(GetOwningPlayer()))
	{
		PC->OnSettingsClosed.Remove(SettingsClosedHandle);
	}
	SettingsClosedHandle.Reset();
	if (bPausedGame)
	{
		if (APlayerController* const PC = GetOwningPlayer())
		{
			PC->SetPause(false);
		}
		bPausedGame = false;
	}
	Super::NativeOnDeactivated();
}

void UknightPauseMenu::NativeDestruct()
{
	StopIntro();
	Super::NativeDestruct();
}

UWidget* UknightPauseMenu::NativeGetDesiredFocusTarget() const
{
	return Btn_Resume;
}

FReply UknightPauseMenu::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	// Caught here rather than by the controller's Esc binding, which only sees Esc when no widget handles it.
	if (InKeyEvent.GetKey() == EKeys::Escape)
	{
		if (ResumeSound)
		{
			UGameplayStatics::PlaySound2D(this, ResumeSound);
		}
		Resume();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UknightPauseMenu::HandleSettingsClicked()
{
	AknightPlayerController* const PC = Cast<AknightPlayerController>(GetOwningPlayer());
	if (!PC)
	{
		return;
	}
	SettingsClosedHandle = PC->OnSettingsClosed.AddUObject(this, &ThisClass::HandleSettingsClosed);
	// The settings window sits in the viewport above this layout but leaves the rest of the screen exposed;
	// the menu stays visible underneath without taking clicks.
	SetVisibility(ESlateVisibility::HitTestInvisible);
	PC->OpenSettingsFromMenu();
}

void UknightPauseMenu::HandleSettingsClosed()
{
	if (AknightPlayerController* const PC = Cast<AknightPlayerController>(GetOwningPlayer()))
	{
		PC->OnSettingsClosed.Remove(SettingsClosedHandle);
	}
	SettingsClosedHandle.Reset();
	SetVisibility(ESlateVisibility::Visible);
	Btn_Settings->SetFocus();
}

void UknightPauseMenu::HandleAbandonClicked()
{
	FknightConfirmRequest Request;
	Request.Title = NSLOCTEXT("knightUI", "AbandonTitle", "레이드 포기");
	Request.Message = NSLOCTEXT("knightUI", "AbandonMessage", "지금 포기하면 이번 레이드는 실패로 처리됩니다.\n기지로 돌아가시겠습니까?");
	Request.ConfirmText = NSLOCTEXT("knightUI", "AbandonConfirm", "포기");
	UknightConfirmModal::Show(GetOwningPlayer(), Request, FknightConfirmResultDelegate::CreateUObject(this, &ThisClass::HandleAbandonAnswered));
}

void UknightPauseMenu::HandleAbandonAnswered(EknightConfirmResult Result)
{
	if (Result != EknightConfirmResult::Confirmed)
	{
		Btn_Abandon->SetFocus();
		return;
	}
	UWorld* const World = GetWorld();
	AknightRaidGameMode* const RaidMode = World ? World->GetAuthGameMode<AknightRaidGameMode>() : nullptr;
	// Unpause first: the raid's return-to-base timers do not run while the game is paused.
	Resume();
	if (RaidMode)
	{
		RaidMode->AbandonRaid();
	}
}

void UknightPauseMenu::RefreshAbandonLock()
{
	const UWorld* const World = GetWorld();
	const bool bInRaid = World && World->GetAuthGameMode<AknightRaidGameMode>() && CanOpen(World);
	Btn_Abandon->SetLockedWithReason(!bInRaid, NSLOCTEXT("knightUI", "AbandonLocked", "레이드 중에만 포기할 수 있습니다."));
}

bool UknightPauseMenu::TickIntro(float DeltaTime)
{
	IntroTime += DeltaTime;
	const float Alpha = FMath::Clamp(IntroTime / IntroDuration, 0.f, 1.f);
	SetRenderOpacity(1.f - FMath::Square(1.f - Alpha));
	if (Alpha >= 1.f)
	{
		IntroTicker.Reset();
		return false;
	}
	return true;
}

void UknightPauseMenu::StopIntro()
{
	if (IntroTicker.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(IntroTicker);
		IntroTicker.Reset();
	}
}
