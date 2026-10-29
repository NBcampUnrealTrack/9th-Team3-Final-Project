#include "Cheats/KnightCheatSubsystem.h"

#include "Cheats/KnightCheatOverlayWidget.h"
#include "knightCharacter.h"
#include "knightPlayerController.h"
#include "Menu/KnightMenuPlayerController.h"
#include "Menu/UI/KnightMenuWidgetBase.h"
#include "Progression/knightProgressionSubsystem.h"
#include "Progression/knightSaveSubsystem.h"
#include "Blueprint/UserWidget.h"
#include "Engine/GameInstance.h"
#include "Framework/Application/IInputProcessor.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"

#define LOCTEXT_NAMESPACE "KnightCheats"

class FKnightCheatInputProcessor final : public IInputProcessor
{
public:
	explicit FKnightCheatInputProcessor(UKnightCheatSubsystem* InOwner) : Owner(InOwner) {}
	virtual void Tick(const float, FSlateApplication&, TSharedRef<ICursor>) override {}
	virtual bool HandleKeyDownEvent(FSlateApplication&, const FKeyEvent& Event) override
	{
		return Owner.IsValid() && Owner->HandleGlobalKeyDown(Event);
	}
	virtual const TCHAR* GetDebugName() const override { return TEXT("KnightCheatInput"); }
private:
	TWeakObjectPtr<UKnightCheatSubsystem> Owner;
};

void UKnightCheatSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Collection.InitializeDependency<UknightSaveSubsystem>();
	Super::Initialize(Collection);
	if (UknightSaveSubsystem* const Saves = GetGameInstance()->GetSubsystem<UknightSaveSubsystem>())
		Saves->OnActiveSlotChanged.AddUniqueDynamic(this, &ThisClass::HandleActiveSlotChanged);
	if (TryRegisterInputProcessor(0.f))
		RegistrationTicker = FTSTicker::GetCoreTicker().AddTicker(
			FTickerDelegate::CreateUObject(this, &ThisClass::TryRegisterInputProcessor));
}

void UKnightCheatSubsystem::Deinitialize()
{
	CloseOverlay();
	if (UknightSaveSubsystem* const Saves = GetGameInstance()->GetSubsystem<UknightSaveSubsystem>())
		Saves->OnActiveSlotChanged.RemoveDynamic(this, &ThisClass::HandleActiveSlotChanged);
	if (RegistrationTicker.IsValid()) FTSTicker::GetCoreTicker().RemoveTicker(RegistrationTicker);
	if (FeedbackTicker.IsValid()) FTSTicker::GetCoreTicker().RemoveTicker(FeedbackTicker);
	if (FSlateApplication::IsInitialized() && InputProcessor.IsValid())
		FSlateApplication::Get().UnregisterInputPreProcessor(InputProcessor);
	InputProcessor.Reset();
	Super::Deinitialize();
}

bool UKnightCheatSubsystem::TryRegisterInputProcessor(float)
{
	if (!FSlateApplication::IsInitialized()) return true;
	InputProcessor = MakeShared<FKnightCheatInputProcessor>(this);
	FSlateApplication::Get().RegisterInputPreProcessor(InputProcessor, 0);
	RegistrationTicker.Reset();
	return false;
}

void UKnightCheatSubsystem::HandleActiveSlotChanged(const FString&)
{
	bInvulnerable = false;
	CloseOverlay();
}

bool UKnightCheatSubsystem::IsOverlayOpen() const
{
	return Overlay && Overlay->IsInViewport();
}

bool UKnightCheatSubsystem::HandleGlobalKeyDown(const FKeyEvent& Event)
{
	if (Event.GetKey() == EKeys::Tilde)
	{
		if (!Event.IsRepeat() && !IsOverlayOpen()) OpenOverlay();
		return true;
	}
	return HandleOverlayKey(Event);
}

bool UKnightCheatSubsystem::HandleOverlayKey(const FKeyEvent& Event)
{
	if (!IsOverlayOpen()) return false;
	if (Event.GetKey() == EKeys::Tilde) return true;
	if (Event.GetKey() == EKeys::Escape)
	{
		if (!Event.IsRepeat()) CloseOverlay();
		return true;
	}
	if (Event.GetKey() == EKeys::Enter)
	{
		if (!Event.IsRepeat()) SubmitCurrentText();
		return true;
	}
	return false;
}

void UKnightCheatSubsystem::OpenOverlay()
{
	if (IsOverlayOpen()) return;
	APlayerController* const PC = GetGameInstance() ? GetGameInstance()->GetFirstLocalPlayerController() : nullptr;
	if (!PC || !PC->IsLocalController() || !FSlateApplication::IsInitialized()) return;
	PreviousController = PC;
	PreviousFocus = FSlateApplication::Get().GetUserFocusedWidget(0);
	bCursorBeforeOverlay = PC->bShowMouseCursor;
	Overlay = CreateWidget<UKnightCheatOverlayWidget>(PC, UKnightCheatOverlayWidget::StaticClass());
	if (!Overlay) return;
	Overlay->AddToViewport(10000);
	FInputModeUIOnly Mode;
	Mode.SetWidgetToFocus(Overlay->TakeWidget());
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	PC->SetInputMode(Mode);
	PC->bShowMouseCursor = true;
	// The opening key's character event must land before the text field gets focus.
	FocusTicker = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateUObject(this, &ThisClass::FocusOverlayNextTick));
}

bool UKnightCheatSubsystem::FocusOverlayNextTick(float)
{
	FocusTicker.Reset();
	if (IsOverlayOpen()) Overlay->FocusInput();
	return false;
}

void UKnightCheatSubsystem::CloseOverlay()
{
	if (FocusTicker.IsValid()) FTSTicker::GetCoreTicker().RemoveTicker(FocusTicker);
	if (FeedbackTicker.IsValid()) FTSTicker::GetCoreTicker().RemoveTicker(FeedbackTicker);
	FocusTicker.Reset();
	FeedbackTicker.Reset();
	if (Overlay) Overlay->RemoveFromParent();
	Overlay = nullptr;
	APlayerController* const PC = PreviousController.Get();
	PreviousController.Reset();
	if (!PC || !PC->IsLocalController()) return;
	if (bCursorBeforeOverlay)
	{
		TSharedPtr<SWidget> Focus = PreviousFocus.Pin();
		if (!Focus.IsValid())
			if (AKnightMenuPlayerController* const Menu = Cast<AKnightMenuPlayerController>(PC))
				if (UKnightMenuWidgetBase* const Root = Menu->GetMenuWidget()) Focus = Root->TakeWidget();
		if (Cast<AknightPlayerController>(PC))
		{
			// Raid settings opened from CommonUI use GameAndUI so their close path can resume gameplay.
			FInputModeGameAndUI Mode;
			if (Focus.IsValid()) Mode.SetWidgetToFocus(Focus);
			Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
			Mode.SetHideCursorDuringCapture(false);
			PC->SetInputMode(Mode);
		}
		else
		{
			FInputModeUIOnly Mode;
			if (Focus.IsValid()) Mode.SetWidgetToFocus(Focus);
			Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
			PC->SetInputMode(Mode);
		}
		PC->bShowMouseCursor = true;
	}
	else
	{
		FInputModeGameOnly Mode;
		PC->SetInputMode(Mode);
		PC->bShowMouseCursor = false;
	}
	PreviousFocus.Reset();
}

bool UKnightCheatSubsystem::ExecuteCommand(const FString& Command, FText& OutFeedback)
{
	FString Lower = Command.ToLower();
	TArray<FString> Words;
	Lower.ParseIntoArrayWS(Words);
	const FString Normalized = FString::Join(Words, TEXT(" "));
	if (Normalized != TEXT("show me the money") && Normalized != TEXT("power overwhelming"))
	{
		OutFeedback = LOCTEXT("UnknownCommand", "알 수 없는 명령입니다.");
		return false;
	}
	const UknightSaveSubsystem* const Saves = GetGameInstance() ? GetGameInstance()->GetSubsystem<UknightSaveSubsystem>() : nullptr;
	if (!Saves || !Saves->HasSelectedSlot())
	{
		OutFeedback = LOCTEXT("NoProfile", "먼저 프로필을 선택하세요.");
		return false;
	}
	if (Normalized == TEXT("power overwhelming"))
	{
		bInvulnerable = true;
		OutFeedback = LOCTEXT("Invulnerable", "무적이 활성화되었습니다.");
		return true;
	}
	UknightProgressionSubsystem* const Progression = GetGameInstance()->GetSubsystem<UknightProgressionSubsystem>();
	int32 MaterialKinds = 0;
	if (!Progression || !Progression->GrantCheatResources(MaterialKinds))
	{
		OutFeedback = LOCTEXT("GrantBlocked", "지급할 수 없습니다. 저장 상태와 아이템 데이터를 확인하세요.");
		return false;
	}
	OutFeedback = FText::Format(LOCTEXT("Granted", "돈 999,999 및 소재 {0}종 각 99개 지급"), FText::AsNumber(MaterialKinds));
	return true;
}

void UKnightCheatSubsystem::SubmitCurrentText()
{
	if (!IsOverlayOpen() || bSubmitting) return;
	bSubmitting = true;
	FText Feedback;
	const bool bSuccess = ExecuteCommand(Overlay->GetCommandText(), Feedback);
	if (bSuccess) Overlay->SetCommandText(FText::GetEmpty());
	DisplayFeedback(Feedback);
	bSubmitting = false;
}

void UKnightCheatSubsystem::DisplayFeedback(const FText& Message)
{
	if (!IsOverlayOpen()) return;
	Overlay->ShowFeedback(Message);
	if (FeedbackTicker.IsValid()) FTSTicker::GetCoreTicker().RemoveTicker(FeedbackTicker);
	FeedbackTicker = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateUObject(this, &ThisClass::ClearFeedbackAfterDelay), 2.5f);
}

bool UKnightCheatSubsystem::ClearFeedbackAfterDelay(float)
{
	FeedbackTicker.Reset();
	if (IsOverlayOpen()) Overlay->ClearFeedback();
	return false;
}

bool UKnightCheatSubsystem::IsPlayerInvulnerable(const AActor* Actor) const
{
	if (!bInvulnerable || !Cast<AknightCharacter>(Actor)) return false;
	const UknightSaveSubsystem* const Saves = GetGameInstance() ? GetGameInstance()->GetSubsystem<UknightSaveSubsystem>() : nullptr;
	const APlayerController* const PC = GetGameInstance() ? GetGameInstance()->GetFirstLocalPlayerController() : nullptr;
	return Saves && Saves->HasSelectedSlot() && PC && PC->GetPawn() == Actor;
}

#undef LOCTEXT_NAMESPACE
