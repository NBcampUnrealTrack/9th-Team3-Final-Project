#pragma once

#include "NativeGameplayTags.h"

/** UI tags. Layer tags name the stacks in UknightPrimaryGameLayout; HUD slot tags name UI extension points. */
namespace knightUIGameplayTags
{
	/** Always-on gameplay HUD. */
	KNIGHTUI_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(UI_Layer_Game);
	/** Menus that sit over gameplay without pausing it (map, item wheel). */
	KNIGHTUI_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(UI_Layer_GameMenu);
	/** Full menus (pause, settings). */
	KNIGHTUI_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(UI_Layer_Menu);
	/** Confirmation dialogs above everything. */
	KNIGHTUI_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(UI_Layer_Modal);

	KNIGHTUI_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(HUD_Slot_PlayerStatus);
	KNIGHTUI_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(HUD_Slot_BossStatus);
	KNIGHTUI_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(HUD_Slot_RaidInfo);
	KNIGHTUI_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(HUD_Slot_Reticle);
	KNIGHTUI_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(HUD_Slot_Notifications);
}
