#include "knightMonsterAnimDebugEditorModule.h"

#include "Framework/Docking/TabManager.h"
#include "Modules/ModuleManager.h"
#include "SknightAnimCoverageBoard.h"
#include "SknightAttackPreview.h"
#include "SknightFeedbackTuning.h"
#include "knightFeedbackTuningModel.h"
#include "ToolMenus.h"
#include "Widgets/Docking/SDockTab.h"
#include "knightAttackSelectionSimulator.h"

namespace knightAnimDebugEditor
{
	static const FName CoverageTabName(TEXT("knightMonsterAnimCoverage"));
	static const FName PreviewTabName(TEXT("knightMonsterAttackPreview"));
	static const FName TuningTabName(TEXT("knightCombatFeedbackTuning"));

	/** 열려 있는 미리보기 탭의 내용. 커버리지 보드에서 열 때 선택을 넘긴다. */
	static TWeakPtr<SknightAttackPreview> LivePreview;

	/**
	 * 게임 쪽 에디터 모듈이 등록한 공격 선택 시뮬레이터. 함수 안의 정적 변수라서 이 모듈의 StartupModule보다
	 * 먼저 등록돼도 된다(같은 로딩 단계에서는 프로젝트 모듈이 플러그인보다 먼저 시작한다).
	 */
	static TSharedPtr<IknightAttackSelectionSimulator>& SimulatorSlot()
	{
		static TSharedPtr<IknightAttackSelectionSimulator> Simulator;
		return Simulator;
	}

	void RegisterAttackSelectionSimulator(const TSharedPtr<IknightAttackSelectionSimulator>& Simulator)
	{
		SimulatorSlot() = Simulator;
	}

	void UnregisterAttackSelectionSimulator(const TSharedPtr<IknightAttackSelectionSimulator>& Simulator)
	{
		if (SimulatorSlot() == Simulator)
		{
			SimulatorSlot().Reset();
		}
	}

	TSharedPtr<IknightAttackSelectionSimulator> GetAttackSelectionSimulator()
	{
		return SimulatorSlot();
	}

	static FAutoConsoleCommand OpenTuningCmd(TEXT("knight.FeedbackTuning.Open"),
		TEXT("knight.FeedbackTuning.Open. Opens the Combat Feedback Tuning tab."),
		FConsoleCommandDelegate::CreateLambda([]() { FGlobalTabmanager::Get()->TryInvokeTab(TuningTabName); }));

	void OpenAttackPreview(const UClass* MonsterClass, const UClass* AbilityClass)
	{
		FGlobalTabmanager::Get()->TryInvokeTab(PreviewTabName);
		if (const TSharedPtr<SknightAttackPreview> Preview = LivePreview.Pin())
		{
			Preview->SelectAbility(MonsterClass, AbilityClass);
		}
	}
}

/** 커버리지 보드, 공격 미리보기, 전투 피드백 튜닝 탭을 등록하고 레벨 에디터의 도구 메뉴에 넣는다. */
class FknightMonsterAnimDebugEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		FGlobalTabmanager::Get()->RegisterNomadTabSpawner(knightAnimDebugEditor::CoverageTabName,
			FOnSpawnTab::CreateStatic(&FknightMonsterAnimDebugEditorModule::SpawnCoverageTab))
			.SetDisplayName(FText::FromString(TEXT("Monster Anim Coverage")))
			.SetTooltipText(FText::FromString(TEXT("몬스터별로 빠진 애니메이션과 타이밍 불일치를 표로 본다.")))
			.SetMenuType(ETabSpawnerMenuType::Hidden);

		FGlobalTabmanager::Get()->RegisterNomadTabSpawner(knightAnimDebugEditor::PreviewTabName,
			FOnSpawnTab::CreateStatic(&FknightMonsterAnimDebugEditorModule::SpawnPreviewTab))
			.SetDisplayName(FText::FromString(TEXT("Monster Attack Preview")))
			.SetTooltipText(FText::FromString(TEXT("게임을 실행하지 않고 공격 몽타주의 판정 궤적과 범위를 본다.")))
			.SetMenuType(ETabSpawnerMenuType::Hidden);

		FGlobalTabmanager::Get()->RegisterNomadTabSpawner(knightAnimDebugEditor::TuningTabName,
			FOnSpawnTab::CreateStatic(&FknightMonsterAnimDebugEditorModule::SpawnTuningTab))
			.SetDisplayName(FText::FromString(TEXT("Combat Feedback Tuning")))
			.SetTooltipText(FText::FromString(TEXT("PIE 중에 전투 피드백 수치를 바꾸고 같은 공격을 다시 실행해 비교한 뒤 저장한다.")))
			.SetMenuType(ETabSpawnerMenuType::Hidden);

		UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FknightMonsterAnimDebugEditorModule::RegisterMenus));
	}

	virtual void ShutdownModule() override
	{
		FknightFeedbackTuningModel::Shutdown();
		UToolMenus::UnRegisterStartupCallback(this);
		UToolMenus::UnregisterOwner(this);
		if (FSlateApplication::IsInitialized())
		{
			FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(knightAnimDebugEditor::CoverageTabName);
			FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(knightAnimDebugEditor::PreviewTabName);
			FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(knightAnimDebugEditor::TuningTabName);
		}
	}

private:
	static TSharedRef<SDockTab> SpawnCoverageTab(const FSpawnTabArgs& Args)
	{
		return SNew(SDockTab)
			.TabRole(ETabRole::NomadTab)
			[
				SNew(SknightAnimCoverageBoard)
			];
	}

	static TSharedRef<SDockTab> SpawnPreviewTab(const FSpawnTabArgs& Args)
	{
		TSharedRef<SknightAttackPreview> Preview = SNew(SknightAttackPreview);
		knightAnimDebugEditor::LivePreview = Preview;
		return SNew(SDockTab)
			.TabRole(ETabRole::NomadTab)
			[
				Preview
			];
	}

	static TSharedRef<SDockTab> SpawnTuningTab(const FSpawnTabArgs& Args)
	{
		return SNew(SDockTab)
			.TabRole(ETabRole::NomadTab)
			[
				SNew(SknightFeedbackTuning)
			];
	}

	void RegisterMenus()
	{
		FToolMenuOwnerScoped OwnerScoped(this);
		UToolMenu* const Menu = UToolMenus::Get()->ExtendMenu(TEXT("LevelEditor.MainMenu.Tools"));
		FToolMenuSection& Section = Menu->FindOrAddSection(TEXT("Knight"), FText::FromString(TEXT("Knight")));
		Section.AddMenuEntry(TEXT("OpenMonsterAnimCoverage"),
			FText::FromString(TEXT("Monster Anim Coverage")),
			FText::FromString(TEXT("몬스터별로 빠진 애니메이션과 타이밍 불일치를 표로 본다.")),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateLambda([]() { FGlobalTabmanager::Get()->TryInvokeTab(knightAnimDebugEditor::CoverageTabName); })));
		Section.AddMenuEntry(TEXT("OpenMonsterAttackPreview"),
			FText::FromString(TEXT("Monster Attack Preview")),
			FText::FromString(TEXT("게임을 실행하지 않고 공격 몽타주의 판정 궤적과 범위를 본다.")),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateLambda([]() { FGlobalTabmanager::Get()->TryInvokeTab(knightAnimDebugEditor::PreviewTabName); })));
		Section.AddMenuEntry(TEXT("OpenCombatFeedbackTuning"),
			FText::FromString(TEXT("Combat Feedback Tuning")),
			FText::FromString(TEXT("PIE 중에 전투 피드백 수치를 바꾸고 같은 공격을 다시 실행해 비교한 뒤 저장한다.")),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateLambda([]() { FGlobalTabmanager::Get()->TryInvokeTab(knightAnimDebugEditor::TuningTabName); })));
	}
};

IMPLEMENT_MODULE(FknightMonsterAnimDebugEditorModule, knightMonsterAnimDebugEditor);
