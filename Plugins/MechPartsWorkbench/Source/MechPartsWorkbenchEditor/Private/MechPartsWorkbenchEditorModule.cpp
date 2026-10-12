#include "Framework/Application/SlateApplication.h"
#include "Framework/Docking/TabManager.h"
#include "Modules/ModuleManager.h"
#include "SMechPartsWorkbench.h"
#include "ToolMenus.h"
#include "Widgets/Docking/SDockTab.h"

#define LOCTEXT_NAMESPACE "MechPartsWorkbench"

namespace
{
	const FName WorkbenchTabName(TEXT("MechPartsWorkbench"));
}

/**
 * Entry point for the workbench editor tool. The viewport, gizmos and "write to part asset" flow are
 * built here; the runtime format they write is FMechPartAttachment.
 *
 * (2026-10-10) 레벨 에디터 메뉴 Tools → Mech Parts Workbench로 창(SMechPartsWorkbench)을 연다.
 */
class FMechPartsWorkbenchEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		FGlobalTabmanager::Get()->RegisterNomadTabSpawner(WorkbenchTabName, FOnSpawnTab::CreateRaw(this, &FMechPartsWorkbenchEditorModule::SpawnWorkbenchTab))
			.SetDisplayName(LOCTEXT("TabTitle", "Mech Parts Workbench"))
			.SetMenuType(ETabSpawnerMenuType::Hidden);

		UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FMechPartsWorkbenchEditorModule::RegisterMenus));
	}

	virtual void ShutdownModule() override
	{
		UToolMenus::UnRegisterStartupCallback(this);
		UToolMenus::UnregisterOwner(this);
		if (FSlateApplication::IsInitialized())
		{
			FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(WorkbenchTabName);
		}
	}

private:
	void RegisterMenus()
	{
		FToolMenuOwnerScoped OwnerScoped(this);
		UToolMenu* const ToolsMenu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Tools");
		FToolMenuSection& Section = ToolsMenu->FindOrAddSection("MechPartsWorkbench", LOCTEXT("MenuSection", "Knight"));
		Section.AddMenuEntry(
			"OpenMechPartsWorkbench",
			LOCTEXT("MenuLabel", "Mech Parts Workbench"),
			LOCTEXT("MenuTooltip", "파츠 메시를 메카에 붙여 보며 위치·회전·크기를 맞추는 창을 엽니다."),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateLambda([]()
			{
				FGlobalTabmanager::Get()->TryInvokeTab(WorkbenchTabName);
			})));
	}

	TSharedRef<SDockTab> SpawnWorkbenchTab(const FSpawnTabArgs& Args)
	{
		return SNew(SDockTab)
			.TabRole(ETabRole::NomadTab)
			[
				SNew(SMechPartsWorkbench)
			];
	}
};

IMPLEMENT_MODULE(FMechPartsWorkbenchEditorModule, MechPartsWorkbenchEditor);

#undef LOCTEXT_NAMESPACE
