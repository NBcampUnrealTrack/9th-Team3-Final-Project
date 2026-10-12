#pragma once

#include "CoreMinimal.h"
#include "KnightMenuWidgetBase.h"
#include "KnightTitleMenuWidget.generated.h"

class UCommonButtonBase;
class UPanelWidget;
class UKnightSaveSlotCard;
class UKnightSettingsWidget;
class USoundBase;

UENUM(BlueprintType)
enum class EKnightTitlePage : uint8
{
	Home,
	SaveSelect,
	Settings
};

/** Before the menu: startup logos (first time only), then "press any key", then the menu items appear. */
enum class EKnightTitleIntroPhase : uint8
{
	StudioLogo,
	EngineLogo,
	FadeIn,
	PressKey,
	Reveal,
	Menu
};

/** C++ parent for the title root WBP. Pages are direct Switcher_Pages children. */
UCLASS()
class KNIGHT_API UKnightTitleMenuWidget : public UKnightMenuWidgetBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category="Knight|Menu")
	EKnightTitlePage GetCurrentPage() const { return CurrentPage; }

	UFUNCTION(BlueprintCallable, Category="Knight|Menu")
	bool ShowPage(EKnightTitlePage NewPage);

	/** Same path as a fixed slot button; useful for UI automation. */
	void SelectFixedSlot(int32 SlotNumber);

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply NativeOnPreviewMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual EKnightFlowState GetExpectedFlowState() const override { return EKnightFlowState::Title; }
	virtual bool HandleBackRequested() override;
	virtual bool ShouldAllowEscapeToChild() const override;
	virtual UWidget* GetPreferredFocusTarget() const override;
	virtual void SetTravelControlsEnabled(bool bEnabled) override;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UPanelWidget> Panel_Home;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UPanelWidget> Panel_SaveSelect;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UPanelWidget> Panel_Settings;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UCommonButtonBase> Button_StartGame;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UCommonButtonBase> Button_SaveSelect;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UKnightSaveSlotCard> Button_Slot1;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UKnightSaveSlotCard> Button_Slot2;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UKnightSaveSlotCard> Button_Slot3;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UCommonButtonBase> Button_Settings;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UCommonButtonBase> Button_Quit;

	/** Startup logo screen over everything. Without it the title starts at "press any key". */
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UWidget> Panel_Intro;

	/** Black backdrop of Panel_Intro. It also takes the mouse clicks until the menu is shown. */
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UWidget> Img_IntroBackdrop;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UWidget> Img_StudioLogo;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UWidget> Img_EngineLogo;

	/** Shown where the menu will be. Without it the menu is shown right away. */
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UWidget> Txt_PressKey;

	/** Its children appear one by one after a key press. */
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UPanelWidget> VerticalBox_Menu;

	/** Each logo: fade in, hold, fade out, then a short black gap. */
	UPROPERTY(EditAnywhere, Category="Knight|Title Intro", meta=(ClampMin=0.05, Units="s"))
	float LogoFadeTime = 0.6f;

	UPROPERTY(EditAnywhere, Category="Knight|Title Intro", meta=(ClampMin=0, Units="s"))
	float LogoHoldTime = 1.6f;

	UPROPERTY(EditAnywhere, Category="Knight|Title Intro", meta=(ClampMin=0, Units="s"))
	float LogoGapTime = 0.3f;

	/** Logos grow from this scale to 1 while they are on screen. */
	UPROPERTY(EditAnywhere, Category="Knight|Title Intro", meta=(ClampMin=0.5, ClampMax=1))
	float LogoStartScale = 0.96f;

	UPROPERTY(EditAnywhere, Category="Knight|Title Intro", meta=(ClampMin=0.2, Units="s"))
	float PressKeyBlinkPeriod = 2.f;

	UPROPERTY(EditAnywhere, Category="Knight|Title Intro", meta=(ClampMin=0.05, Units="s"))
	float PressKeyFadeOutTime = 0.25f;

	/** Each menu item slides in from the left over this time, the next one this much later. */
	UPROPERTY(EditAnywhere, Category="Knight|Title Intro", meta=(ClampMin=0.05, Units="s"))
	float ItemRevealTime = 0.3f;

	UPROPERTY(EditAnywhere, Category="Knight|Title Intro", meta=(ClampMin=0, Units="s"))
	float ItemRevealStagger = 0.07f;

	UPROPERTY(EditAnywhere, Category="Knight|Title Intro")
	float ItemRevealOffset = -24.f;

	/** Played when "press any key" is answered. */
	UPROPERTY(EditAnywhere, Category="Knight|Title Intro")
	TObjectPtr<USoundBase> PressKeySound;

private:
	UFUNCTION()
	void HandleStartGameClicked();

	UFUNCTION()
	void HandleSaveSelectClicked();

	UFUNCTION()
	void HandleSlot1Clicked();

	UFUNCTION()
	void HandleSlot2Clicked();

	UFUNCTION()
	void HandleSlot3Clicked();

	UFUNCTION()
	void HandleSaveFinished(bool bSuccess);

	UFUNCTION()
	void HandleSettingsClicked();
	UFUNCTION() void HandleSettingsClosed();

	UFUNCTION()
	void HandleQuitClicked();

	bool ApplyPage(EKnightTitlePage NewPage, bool bAllowDuringTravel);
	void RefreshSlots();
	/** The slot whose file was written last, or 0 when no slot has been saved yet. */
	int32 FindLatestSlot() const;
	void OpenLobby();
	void EnsureSettingsWidget();

	void EnterIntroPhase(EKnightTitleIntroPhase NewPhase);
	/** A key or click before the menu: skips the logos, or answers "press any key". */
	void AdvanceIntro();
	void TickIntro(float DeltaTime);
	void ShowLogo(UWidget* Logo, float Time) const;
	void SetMenuItemsReveal(float Time) const;
	bool IsMenuShown() const { return IntroPhase == EKnightTitleIntroPhase::Menu; }

	UPROPERTY(Transient)
	TObjectPtr<UKnightSettingsWidget> SettingsWidget;

	EKnightTitleIntroPhase IntroPhase = EKnightTitleIntroPhase::Menu;
	float IntroPhaseTime = 0.f;
	EKnightTitlePage CurrentPage = EKnightTitlePage::Home;
	bool bAwaitingInitialSave = false;
};
