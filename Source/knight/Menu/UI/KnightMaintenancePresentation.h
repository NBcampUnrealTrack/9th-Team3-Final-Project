#pragma once

#include "CommonButtonBase.h"

namespace KnightMaintenancePresentation
{
	// knightUI depends on knight. Use its reflected presentation API, as with SetButtonText/SetLockedWithReason.
	inline void ConfigureButton(UCommonButtonBase* Button, bool bSelectable, bool bMainTab = false)
	{
		if (!Button) return;
		if (UFunction* Function = Button->FindFunction(TEXT("ConfigureMaintenanceButton")))
		{
			struct { bool bSelectable; bool bMainTab; } Params{bSelectable, bMainTab};
			Button->ProcessEvent(Function, &Params);
		}
	}
}
