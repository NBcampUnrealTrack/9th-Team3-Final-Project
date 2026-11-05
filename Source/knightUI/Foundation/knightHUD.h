#pragma once

#include "CoreMinimal.h"
#include "FieldNotificationId.h"
#include "Containers/Ticker.h"
#include "GameFramework/HUD.h"
#include "Raid/knightRaidTypes.h"
#include "knightHUD.generated.h"

class UCommonActivatableWidget;
class UknightHUDFinishBanner;
class UknightVM_Raid;
class UWidget;

/**
 * Gameplay HUD actor. Creates the primary layout through UknightUISubsystem and pushes HUDLayoutClass onto
 * UI.Layer.Game. Set it as the HUD Class of gameplay GameMode Blueprints; the menu maps keep their own UI.
 * Also routes the player controller's Esc to the pause menu (UknightUISettings::PauseMenuClass) on UI.Layer.Menu,
 * and opens the raid result (UknightUISettings::RaidResultScreenClass) there when the raid succeeds or fails.
 * 처치 연출(AknightRaidGameState::GetFinishStage) 중에는 아래 대화창만 남기고 HUD를 숨기고, Banner 단계에 토벌 확인 판독 창과 [AI] 대사를 띄우고,
 * Done에서 결과 화면을 연다. 판독 창은 결과 화면이 열리면 위로 올라가 그 제목이 된다. 실패는 처치 연출 없이 바로 빨간 판독 창을 띄우고
 * FailBannerHoldTime 뒤에 결과 화면을 연다.
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

	/** 실패 판독 창이 가운데에 혼자 떠 있는 시간(실제 초). 그 뒤 결과 화면이 열리며 위로 올라간다. */
	UPROPERTY(EditDefaultsOnly, Category="Knight|UI|Result", meta=(ClampMin=0, Units="s"))
	float FailBannerHoldTime = 2.2f;

	/** 판독 창이 뜰 때 아래 대화창에 나오는 [AI] 대사. */
	UPROPERTY(EditDefaultsOnly, Category="Knight|UI|Result")
	FText RaidSuccessLine;

	UPROPERTY(EditDefaultsOnly, Category="Knight|UI|Result")
	FText KnightDestroyedLine;

	UPROPERTY(EditDefaultsOnly, Category="Knight|UI|Result")
	FText TimeUpLine;

public:
	AknightHUD();

private:
	/** Esc from the player controller: opens the pause menu, or resumes when it is already open. */
	void TogglePauseMenu();

	void HandleRaidViewModelChanged(UObject* InViewModel, UE::FieldNotification::FFieldId FieldId);
	void OpenRaidResult();
	void HandleFinishStageChanged(EknightRaidFinishStage Stage);
	/** 판독 창을 결과 화면보다 위에 띄우고 [AI] 대사를 대화창에 넣는다. */
	void ShowFinishBanner(bool bFailure, EknightRaidFailReason FailReason = EknightRaidFailReason::None);
	void RemoveFinishBanner();
	/** 처치 연출 동안 HUD 레이아웃에서 아래 대화창(MessageLog)만 남기고 숨긴다 / 되돌린다. */
	void HideHUDForFinish();
	void RestoreHUDAfterFinish();
	void StopFailBannerTicker();

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
	/** 처치 연출로 숨긴 HUD 위젯과 숨기기 전 표시 상태. */
	TArray<TPair<TWeakObjectPtr<UWidget>, ESlateVisibility>> HiddenForFinish;
	bool bHiddenForFinish = false;
	/** 실패한 월드 시각. 귀환 타이머가 이때부터 돌므로 결과 화면 카운트다운도 여기에 맞춘다. 음수면 해당 없음. */
	double FailWorldTime = -1.0;
	FTSTicker::FDelegateHandle FailBannerTicker;
};
