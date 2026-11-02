#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/SCompoundWidget.h"
#include "knightFeedbackTuningModel.h"

class APawn;
class SVerticalBox;
class UWorld;

/**
 * 전투 피드백 튜닝 탭 (도구 > Combat Feedback Tuning).
 *
 * PIE 중에 몬스터, 플레이어, 카메라의 피드백 수치(히트스톱, 셰이크, 경직, 넉백, 그로기, FOV …)를 바꾸면 바로 반영되고,
 * 같은 공격(몬스터 공격이나 플레이어 공격)을 버튼 하나로 다시 실행해 비교한 뒤 마음에 드는 몬스터 값을 에셋에 저장한다.
 * 공격 실행, 반복, AI 정지, 배속은 Gameplay Debugger와 같은 UknightAnimDebugSubsystem을 쓴다.
 */
class SknightFeedbackTuning : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SknightFeedbackTuning) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SknightFeedbackTuning() override;

private:
	UWorld* GetPlayWorld() const;
	APawn* GetMonster() const;
	APawn* GetPlayer() const;

	void Collect();
	/** 모델이 값을 새로 모았을 때. 공격 목록과 행을 다시 만든다. */
	void HandleCollected();
	void RefreshCombos();
	void HandlePIEStarted(bool bIsSimulating);
	void RebuildRows();
	bool TickLocks(float DeltaTime);
	void HandleReset(int32 Reverted);

	FReply OnRunClicked();
	FReply OnPlayerAttackClicked();
	FReply OnImpactPreviewClicked();
	void SelectImpactMoment(TSharedPtr<FString> Moment);
	FReply OnPlaceClicked();
	FReply OnSaveClicked();
	FReply OnRevertClicked();
	FReply OnCopyClicked();

	FText GetStatusText() const;

	FknightFeedbackTuningModel& Model = FknightFeedbackTuningModel::Get();
	TSharedPtr<SVerticalBox> RowBox;
	TSharedPtr<SComboBox<TSharedPtr<FString>>> AbilityCombo;
	TSharedPtr<SComboBox<TSharedPtr<FString>>> PlayerAttackCombo;
	TArray<TSharedPtr<FString>> AbilityNames;
	TSharedPtr<FString> SelectedAbility;
	TArray<TSharedPtr<FString>> PlayerAttackNames;
	TSharedPtr<FString> SelectedPlayerAttack;
	TArray<TSharedPtr<FString>> ImpactMoments;
	TSharedPtr<FString> SelectedImpactMoment;
	TSharedPtr<SComboBox<TSharedPtr<FString>>> ImpactCombo;
	TSharedPtr<class SSearchBox> SearchBox;
	/** 카메라 반응 표의 출처 이름. 순간을 고르면 "출처 순간." 으로 찾는다. */
	FString ImpactSourceLabel;
	FString Filter;
	FString LastMessage;
	FTSTicker::FDelegateHandle TickerHandle;
	float RepeatDelay = 1.5f;
	/** PIE 시작 후 이 시각(앱 시간)에 자동으로 읽는다. 어빌리티와 무기가 붙을 때까지 잠깐 기다린다. 0이면 예약 없음. */
	double AutoCollectTime = 0.0;
	bool bChangedOnly = false;
	bool bShowMonster = true;
	bool bShowPlayer = true;
	bool bShowCamera = true;
	bool bPlaceBeforeRun = true;
	bool bRepeat = false;
	bool bLockPlayerHealth = true;
	bool bLockMonsterHealth = false;
};
