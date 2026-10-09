#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Foundation/knightActivatableWidget.h"
#include "knightPauseMenu.generated.h"

class UknightButtonBase;
class USoundBase;
enum class EknightConfirmResult : uint8;

/**
 * In-raid pause menu on UI.Layer.Menu. Opened and closed with Esc through AknightHUD. Replaces the editor-only
 * WBP_FlowTestReturn overlay for players.
 *
 * Pauses the game while open (unless something else already paused it). Settings opens the existing settings window
 * on top; Abandon asks through the confirm modal, then gives up the raid.
 *
 * Layout lives in the Widget Blueprint designer (WBP_KnightPauseMenu). Required names: Btn_Resume, Btn_Settings,
 * Btn_Abandon (UknightButtonBase).
 */
UCLASS(Abstract, Blueprintable, meta=(DisableNativeTick))
class KNIGHTUI_API UknightPauseMenu : public UknightActivatableWidget
{
	GENERATED_BODY()

public:
	UknightPauseMenu(const FObjectInitializer& ObjectInitializer);

	UFUNCTION(BlueprintCallable, Category="Knight|Pause")
	void Resume();

	/** Whether the pause menu may open now: not once the raid has ended. */
	static bool CanOpen(const UWorld* World);

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;
	virtual void NativeDestruct() override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	/** Played when Esc resumes. The buttons play their own sounds from their style. */
	UPROPERTY(EditAnywhere, Category="Knight|Pause")
	TObjectPtr<USoundBase> ResumeSound;

	UPROPERTY(EditAnywhere, Category="Knight|Pause|Motion", meta=(ClampMin=0, Units="s"))
	float IntroDuration = 0.12f;

	UPROPERTY(BlueprintReadOnly, Category="Knight|Pause", meta=(BindWidget))
	TObjectPtr<UknightButtonBase> Btn_Resume;

	UPROPERTY(BlueprintReadOnly, Category="Knight|Pause", meta=(BindWidget))
	TObjectPtr<UknightButtonBase> Btn_Settings;

	UPROPERTY(BlueprintReadOnly, Category="Knight|Pause", meta=(BindWidget))
	TObjectPtr<UknightButtonBase> Btn_Abandon;

private:
	void HandleSettingsClicked();
	void HandleSettingsClosed();
	void HandleAbandonClicked();
	void HandleAbandonAnswered(EknightConfirmResult Result);
	void RefreshAbandonLock();
	bool TickIntro(float DeltaTime);
	void StopIntro();

	bool bPausedGame = false;
	float IntroTime = 0.f;
	FDelegateHandle SettingsClosedHandle;
	FTSTicker::FDelegateHandle IntroTicker;
};
