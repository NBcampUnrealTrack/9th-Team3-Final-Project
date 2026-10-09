#include "Screens/knightConfirmModal.h"

#include "CommonTextBlock.h"
#include "Engine/LocalPlayer.h"
#include "Foundation/knightUISettings.h"
#include "Foundation/knightUISubsystem.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Widgets/knightButtonBase.h"
#include "knightUI.h"
#include "knightUIGameplayTags.h"

UknightConfirmModal::UknightConfirmModal(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InputMode = EknightWidgetInputMode::Menu;
	bIsModal = true;
	// Not a CommonUI back handler: without a back action in the input data the router logs an error on every
	// activation. Esc is handled in NativeOnKeyDown until the project sets up IA_UI_Back.
	bIsBackHandler = false;
}

UknightConfirmModal* UknightConfirmModal::Show(APlayerController* PlayerController, const FknightConfirmRequest& Request, FknightConfirmResultDelegate OnResult)
{
	const ULocalPlayer* const LocalPlayer = PlayerController ? PlayerController->GetLocalPlayer() : nullptr;
	UknightUISubsystem* const UI = LocalPlayer ? LocalPlayer->GetSubsystem<UknightUISubsystem>() : nullptr;
	if (!UI || !UI->GetOrCreateLayout(PlayerController))
	{
		UE_LOG(LogknightUI, Warning, TEXT("Confirm modal not shown: no UI layout for %s."), *GetNameSafe(PlayerController));
		return nullptr;
	}
	UClass* const ModalClass = GetDefault<UknightUISettings>()->ConfirmModalClass.LoadSynchronous();
	if (!ModalClass)
	{
		UE_LOG(LogknightUI, Error, TEXT("Knight UI settings have no ConfirmModalClass."));
		return nullptr;
	}

	// The stack activates the widget inside the push, which resets it; the request and callback are applied after.
	UknightConfirmModal* const Modal = Cast<UknightConfirmModal>(UI->PushWidgetToLayer(knightUIGameplayTags::UI_Layer_Modal, ModalClass));
	if (!Modal)
	{
		return nullptr;
	}
	Modal->Setup(Request);
	Modal->NativeOnResult = MoveTemp(OnResult);
	return Modal;
}

void UknightConfirmModal::Setup(const FknightConfirmRequest& Request)
{
	if (Txt_Title)
	{
		Txt_Title->SetText(Request.Title);
		Txt_Title->SetVisibility(Request.Title.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
	if (Txt_Message)
	{
		Txt_Message->SetText(Request.Message);
	}
	Btn_Confirm->SetButtonText(Request.ConfirmText.IsEmpty() ? DefaultConfirmText : Request.ConfirmText);
	Btn_Cancel->SetButtonText(Request.CancelText.IsEmpty() ? DefaultCancelText : Request.CancelText);
}

void UknightConfirmModal::Confirm()
{
	Finish(EknightConfirmResult::Confirmed);
}

void UknightConfirmModal::Cancel()
{
	Finish(EknightConfirmResult::Cancelled);
}

void UknightConfirmModal::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// Button texts set in the designer are the defaults for requests that leave them empty.
	DefaultConfirmText = Btn_Confirm->GetButtonText();
	DefaultCancelText = Btn_Cancel->GetButtonText();
	Btn_Confirm->OnClicked().AddUObject(this, &ThisClass::Confirm);
	Btn_Cancel->OnClicked().AddUObject(this, &ThisClass::Cancel);
}

void UknightConfirmModal::NativeOnActivated()
{
	// The stack pools widgets, so a reused modal starts clean here rather than in NativeOnInitialized.
	bFinished = false;
	NativeOnResult.Unbind();

	Super::NativeOnActivated();

	// The action router also focuses the desired target; doing it here keeps Esc working if that is skipped.
	Btn_Cancel->SetFocus();

	StopIntro();
	IntroTime = 0.f;
	ApplyIntro(IntroDuration > 0.f ? 0.f : 1.f);
	if (IntroDuration > 0.f)
	{
		// FTSTicker keeps running while the game is paused, which is when most confirms are open.
		IntroTicker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &ThisClass::TickIntro));
	}
}

void UknightConfirmModal::NativeOnDeactivated()
{
	StopIntro();
	ApplyIntro(1.f);
	Super::NativeOnDeactivated();
}

void UknightConfirmModal::NativeDestruct()
{
	StopIntro();
	Super::NativeDestruct();
}

UWidget* UknightConfirmModal::NativeGetDesiredFocusTarget() const
{
	return Btn_Cancel;
}

bool UknightConfirmModal::NativeOnHandleBackAction()
{
	if (CancelSound)
	{
		UGameplayStatics::PlaySound2D(this, CancelSound);
	}
	Cancel();
	return true;
}

FReply UknightConfirmModal::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	// The project has no CommonUI back action yet, so Esc is caught here. Handling it also keeps it away from the
	// player controller, whose own Esc binding would open the settings window underneath.
	if (InKeyEvent.GetKey() == EKeys::Escape)
	{
		NativeOnHandleBackAction();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UknightConfirmModal::Finish(EknightConfirmResult Result)
{
	if (bFinished)
	{
		return;
	}
	bFinished = true;

	// Deactivate first so a callback can push the next screen (or another modal) on a settled stack.
	FknightConfirmResultDelegate Callback = MoveTemp(NativeOnResult);
	NativeOnResult.Unbind();
	DeactivateWidget();

	Callback.ExecuteIfBound(Result);
	OnResult.Broadcast(Result);
	OnResult.Clear();
}

bool UknightConfirmModal::TickIntro(float DeltaTime)
{
	IntroTime += DeltaTime;
	const float Alpha = FMath::Clamp(IntroTime / IntroDuration, 0.f, 1.f);
	ApplyIntro(Alpha);
	if (Alpha >= 1.f)
	{
		IntroTicker.Reset();
		return false;
	}
	return true;
}

void UknightConfirmModal::ApplyIntro(float Alpha)
{
	const float Eased = 1.f - FMath::Square(1.f - Alpha);
	SetRenderOpacity(Eased);
	if (Panel)
	{
		Panel->SetRenderScale(FVector2D(FMath::Lerp(IntroStartScale, 1.f, Eased)));
	}
}

void UknightConfirmModal::StopIntro()
{
	if (IntroTicker.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(IntroTicker);
		IntroTicker.Reset();
	}
}

#if !UE_BUILD_SHIPPING
static FAutoConsoleCommandWithWorldAndArgs GknightUITestConfirmCommand(
	TEXT("knight.UI.TestConfirm"),
	TEXT("Shows the confirm modal with sample text and logs the result."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		APlayerController* const PC = World ? World->GetFirstPlayerController() : nullptr;
		FknightConfirmRequest Request;
		Request.Title = NSLOCTEXT("knightUI", "TestConfirmTitle", "레이드 포기");
		Request.Message = NSLOCTEXT("knightUI", "TestConfirmMessage", "지금 포기하면 이번 레이드의 보상을 받을 수 없습니다.\n기지로 돌아가시겠습니까?");
		Request.ConfirmText = NSLOCTEXT("knightUI", "TestConfirmYes", "포기");
		UknightConfirmModal::Show(PC, Request, FknightConfirmResultDelegate::CreateLambda([](EknightConfirmResult Result)
		{
			UE_LOG(LogknightUI, Display, TEXT("knight.UI.TestConfirm result: %s"), Result == EknightConfirmResult::Confirmed ? TEXT("Confirmed") : TEXT("Cancelled"));
		}));
	}));
#endif
