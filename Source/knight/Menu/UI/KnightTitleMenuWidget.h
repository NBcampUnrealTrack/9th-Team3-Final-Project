#pragma once

#include "CoreMinimal.h"
#include "KnightMenuWidgetBase.h"
#include "KnightTitleMenuWidget.generated.h"

class UButton;
class UPanelWidget;

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
	TObjectPtr<UButton> Button_Settings;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UButton> Button_Quit;

private:
	UFUNCTION()
	void HandleStartGameClicked();

	UFUNCTION()
	void HandleSaveSelectClicked();

	UFUNCTION()
	void HandleSettingsClicked();

	UFUNCTION()
	void HandleQuitClicked();

	bool ApplyPage(EKnightTitlePage NewPage, bool bAllowDuringTravel);

	EKnightTitlePage CurrentPage = EKnightTitlePage::Home;
};
