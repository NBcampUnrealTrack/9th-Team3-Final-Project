#pragma once

#include "CoreMinimal.h"
#include "KnightMenuWidgetBase.h"
#include "KnightTitleMenuWidget.generated.h"

class UButton;
class UEditableTextBox;
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
	TObjectPtr<UEditableTextBox> Input_SlotId;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UButton> Button_UseSlot;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UTextBlock> Text_SlotInfo;

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
	void HandleUseSlotClicked();

	UFUNCTION()
	void HandleSlotIdChanged(const FText& NewText);

	UFUNCTION()
	void HandleSaveFinished(bool bSuccess);

	UFUNCTION()
	void HandleSettingsClicked();

	UFUNCTION()
	void HandleQuitClicked();

	bool ApplyPage(EKnightTitlePage NewPage, bool bAllowDuringTravel);
	void RefreshSlotInfo();
	void OpenLobby();

	EKnightTitlePage CurrentPage = EKnightTitlePage::Home;
	bool bAwaitingInitialSave = false;
};
