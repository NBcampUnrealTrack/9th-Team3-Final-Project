#include "knightEditor.h"
#include "Modules/ModuleManager.h"
#include "knightMonsterAttackSelectionSimulator.h"

/** 프로젝트 에디터 모듈. 게임 코드를 아는 에디터 확장을 여기서 등록한다. */
class FknightEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		// 몬스터 공격 미리보기의 '공격 선택' 패널이 게임의 선택 판정을 쓰도록 연결한다.
		AttackSelectionSimulator = MakeShared<FknightMonsterAttackSelectionSimulator>();
		knightAnimDebugEditor::RegisterAttackSelectionSimulator(AttackSelectionSimulator);
	}

	virtual void ShutdownModule() override
	{
		knightAnimDebugEditor::UnregisterAttackSelectionSimulator(AttackSelectionSimulator);
		AttackSelectionSimulator.Reset();
	}

private:
	TSharedPtr<IknightAttackSelectionSimulator> AttackSelectionSimulator;
};

IMPLEMENT_MODULE(FknightEditorModule, knightEditor);

DEFINE_LOG_CATEGORY(LogknightEditor)
