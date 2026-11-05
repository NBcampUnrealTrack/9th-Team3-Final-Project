#pragma once

#include "CoreMinimal.h"
#include "FieldNotificationId.h"
#include "GameFramework/HUD.h"
#include "Raid/knightRaidTypes.h"
#include "knightHUD.generated.h"

class UCommonActivatableWidget;
class UknightHUDFinishBanner;
class UknightVM_Raid;

/**
 * Gameplay HUD actor. Creates the primary layout through UknightUISubsystem and pushes HUDLayoutClass onto
 * UI.Layer.Game. Set it as the HUD Class of gameplay GameMode Blueprints; the menu maps keep their own UI.
 * Also routes the player controller's Esc to the pause menu (UknightUISettings::PauseMenuClass) on UI.Layer.Menu,
 * and opens the raid result (UknightUISettings::RaidResultScreenClass) there when the raid succeeds or fails.
 * 처치 연출(AknightRaidGameState::GetFinishStage) 중에는 HUD를 숨기고, Banner 단계에 토벌 확인 배너를 띄우고, Done에서 결과 화면을 연다.
 */
UCLASS(Blueprintable)
class KNIGHTUI_API AknightHUD : public AHUD
{
	GENERATED_BODY()

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** HUD layout widget. Its HUD widgets read the view models from UknightUISubsystem. */
	UPROPERTY(EditDefaultsOnly, Category="Knight|UI")
	TSoftClassPtr<UCommonActivatableWidget> HUDLayoutClass;

private:
	/** Esc from the player controller: opens the pause menu, or resumes when it is already open. */
	void TogglePauseMenu();

	void HandleRaidViewModelChanged(UObject* InViewModel, UE::FieldNotification::FFieldId FieldId);
	void OpenRaidResult();
	void HandleFinishStageChanged(EknightRaidFinishStage Stage);
	void ShowFinishBanner();
	void RemoveFinishBanner();

	/**
	 * 결과 화면·일시정지 메뉴 클래스를 BeginPlay(로딩 화면이 떠 있는 동안)에 미리 읽어 붙잡아 둔다.
	 * 전에는 처음 죽을 때 결과 화면을 그 자리에서 읽느라(FlushAsyncLoading) 한 프레임이 0.6초 멈췄다('죽을 때 랙').
	 */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UClass>> PreloadedScreenClasses;

	TWeakObjectPtr<UCommonActivatableWidget> HUDLayout;
	TWeakObjectPtr<UCommonActivatableWidget> PauseMenu;
	TWeakObjectPtr<UCommonActivatableWidget> RaidResult;
	TWeakObjectPtr<UknightVM_Raid> RaidViewModel;

	UPROPERTY(Transient)
	TObjectPtr<UknightHUDFinishBanner> FinishBanner;

	FDelegateHandle FinishStageHandle;
	/** 처치 연출로 숨기기 전 HUD 레이아웃의 표시 상태. */
	TOptional<ESlateVisibility> HUDLayoutVisibilityBeforeFinish;
};
