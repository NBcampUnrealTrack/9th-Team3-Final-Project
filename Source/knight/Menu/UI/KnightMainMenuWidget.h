#pragma once

#include "CoreMinimal.h"
#include "KnightMenuWidgetBase.h"
#include "KnightMainMenuWidget.generated.h"

class UButton;
class UPanelWidget;

UENUM(BlueprintType)
enum class EKnightMainMenuPage : uint8
{
	Home,
	Stage,
	Maintenance,
	Settings,
	Codex
};

/** C++ parent for the main-menu root WBP. Stage detail/back behavior extends here in step 3. */
UCLASS()
class KNIGHT_API UKnightMainMenuWidget : public UKnightMenuWidgetBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category="Knight|Menu")
	EKnightMainMenuPage GetCurrentPage() const { return CurrentPage; }

	UFUNCTION(BlueprintCallable, Category="Knight|Menu")
	bool ShowPage(EKnightMainMenuPage NewPage);

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual EKnightFlowState GetExpectedFlowState() const override { return EKnightFlowState::MainMenu; }
	virtual bool HandleBackRequested() override;
	virtual UWidget* GetPreferredFocusTarget() const override;
	virtual void SetTravelControlsEnabled(bool bEnabled) override;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UPanelWidget> Panel_Home;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UPanelWidget> Panel_Stage;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UPanelWidget> Panel_Maintenance;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UPanelWidget> Panel_Settings;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UPanelWidget> Panel_Codex;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UButton> Button_Stage;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UButton> Button_Maintenance;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UButton> Button_Settings;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UButton> Button_Codex;

private:
	UFUNCTION()
	void HandleStageClicked();

	UFUNCTION()
	void HandleMaintenanceClicked();

	UFUNCTION()
	void HandleSettingsClicked();

	UFUNCTION()
	void HandleCodexClicked();

	bool ApplyPage(EKnightMainMenuPage NewPage, bool bAllowDuringTravel);

	EKnightMainMenuPage CurrentPage = EKnightMainMenuPage::Home;
};
