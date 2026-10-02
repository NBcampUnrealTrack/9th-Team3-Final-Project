#pragma once

#include "CoreMinimal.h"
#include "KnightMenuWidgetBase.h"
#include "KnightTitleMenuWidget.generated.h"

class UButton;
class UPanelWidget;
class UTextBlock;

UENUM(BlueprintType)
enum class EKnightTitlePage : uint8
{
	Home,
	SaveSelect,
	Settings
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
	virtual EKnightFlowState GetExpectedFlowState() const override { return EKnightFlowState::Title; }
	virtual bool HandleBackRequested() override;
	virtual UWidget* GetPreferredFocusTarget() const override;
	virtual void SetTravelControlsEnabled(bool bEnabled) override;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UPanelWidget> Panel_Home;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UPanelWidget> Panel_SaveSelect;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UPanelWidget> Panel_Settings;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UButton> Button_StartGame;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UButton> Button_SaveSelect;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UButton> Button_Slot1;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UButton> Button_Slot2;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UButton> Button_Slot3;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UTextBlock> Text_Slot1;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UTextBlock> Text_Slot2;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UTextBlock> Text_Slot3;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UButton> Button_Settings;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UButton> Button_Quit;

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

	UFUNCTION()
	void HandleQuitClicked();

	bool ApplyPage(EKnightTitlePage NewPage, bool bAllowDuringTravel);
	void RefreshSlots();
	void OpenLobby();

	EKnightTitlePage CurrentPage = EKnightTitlePage::Home;
	bool bAwaitingInitialSave = false;
};
