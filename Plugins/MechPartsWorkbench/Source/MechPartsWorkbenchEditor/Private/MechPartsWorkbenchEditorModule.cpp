#include "Modules/ModuleManager.h"

/**
 * Entry point for the workbench editor tool. The viewport, gizmos and "write to part asset" flow are
 * built here; the runtime format they write is FMechPartAttachment.
 */
class FMechPartsWorkbenchEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override {}
	virtual void ShutdownModule() override {}
};

IMPLEMENT_MODULE(FMechPartsWorkbenchEditorModule, MechPartsWorkbenchEditor);
