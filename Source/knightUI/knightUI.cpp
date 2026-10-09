#include "knightUI.h"
#include "Modules/ModuleManager.h"

#if !UE_BUILD_SHIPPING
#include "Framework/Application/SlateApplication.h"
#include "HAL/IConsoleManager.h"
#include "InputCoreTypes.h"
#endif

IMPLEMENT_MODULE(FDefaultModuleImpl, knightUI);

DEFINE_LOG_CATEGORY(LogknightUI)

#if !UE_BUILD_SHIPPING
namespace
{
	FString DescribeFocus()
	{
		const TSharedPtr<SWidget> Focused = FSlateApplication::Get().GetUserFocusedWidget(0);
		return Focused.IsValid() ? Focused->ToString() : TEXT("none");
	}
}

/** Lets automated UI tests press keys: the key goes through Slate exactly like a real key press. */
static FAutoConsoleCommand GknightUISimKeyCommand(
	TEXT("knight.UI.SimKey"),
	TEXT("Presses and releases a key through Slate (knight.UI.SimKey Right). Logs the focused widget before and after."),
	FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
	{
		if (Args.Num() == 0 || !FSlateApplication::IsInitialized())
		{
			return;
		}
		const FKey Key(*Args[0]);
		const FString Before = DescribeFocus();
		const FKeyEvent Event(Key, FSlateApplication::Get().GetModifierKeys(), 0, false, 0, 0);
		const bool bHandled = FSlateApplication::Get().ProcessKeyDownEvent(Event);
		FSlateApplication::Get().ProcessKeyUpEvent(Event);
		UE_LOG(LogknightUI, Display, TEXT("SimKey %s handled=%d focus %s -> %s"), *Key.ToString(), bHandled, *Before, *DescribeFocus());
	}));
#endif
