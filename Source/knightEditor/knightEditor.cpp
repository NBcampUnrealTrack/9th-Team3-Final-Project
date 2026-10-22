#include "knightEditor.h"
#include "Modules/ModuleManager.h"
#include "knightMonsterAttackSelectionSimulator.h"
#include "DetailLayoutBuilder.h"
#include "IDetailCustomization.h"
#include "PropertyEditorModule.h"
#include "Item/knightItemDefinitions.h"

/** Equipment keeps the shared item properties for save compatibility, but does not author them. */
class FknightPartItemDetails final : public IDetailCustomization
{
public:
	static TSharedRef<IDetailCustomization> MakeInstance()
	{
		return MakeShared<FknightPartItemDetails>();
	}

	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override
	{
		DetailBuilder.HideProperty(DetailBuilder.GetProperty(TEXT("Description"), UknightItemDefinition::StaticClass()));
		DetailBuilder.HideProperty(DetailBuilder.GetProperty(TEXT("Rarity"), UknightItemDefinition::StaticClass()));
	}
};

/** 프로젝트 에디터 모듈. 게임 코드를 아는 에디터 확장을 여기서 등록한다. */
class FknightEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		// 몬스터 공격 미리보기의 '공격 선택' 패널이 게임의 선택 판정을 쓰도록 연결한다.
		AttackSelectionSimulator = MakeShared<FknightMonsterAttackSelectionSimulator>();
		knightAnimDebugEditor::RegisterAttackSelectionSimulator(AttackSelectionSimulator);

		FPropertyEditorModule& PropertyEditor = FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));
		PropertyEditor.RegisterCustomClassLayout(UknightPartItemDefinition::StaticClass()->GetFName(),
			FOnGetDetailCustomizationInstance::CreateStatic(&FknightPartItemDetails::MakeInstance));
		PropertyEditor.RegisterCustomClassLayout(UknightWeaponItemDefinition::StaticClass()->GetFName(),
			FOnGetDetailCustomizationInstance::CreateStatic(&FknightPartItemDetails::MakeInstance));
		PropertyEditor.NotifyCustomizationModuleChanged();
	}

	virtual void ShutdownModule() override
	{
		knightAnimDebugEditor::UnregisterAttackSelectionSimulator(AttackSelectionSimulator);
		AttackSelectionSimulator.Reset();
		if (FModuleManager::Get().IsModuleLoaded(TEXT("PropertyEditor")))
		{
			FPropertyEditorModule& PropertyEditor = FModuleManager::GetModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));
			PropertyEditor.UnregisterCustomClassLayout(UknightPartItemDefinition::StaticClass()->GetFName());
			PropertyEditor.UnregisterCustomClassLayout(UknightWeaponItemDefinition::StaticClass()->GetFName());
			PropertyEditor.NotifyCustomizationModuleChanged();
		}
	}

private:
	TSharedPtr<IknightAttackSelectionSimulator> AttackSelectionSimulator;
};

IMPLEMENT_MODULE(FknightEditorModule, knightEditor);

DEFINE_LOG_CATEGORY(LogknightEditor)
