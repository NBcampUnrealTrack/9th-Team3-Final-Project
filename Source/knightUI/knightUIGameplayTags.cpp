#include "knightUIGameplayTags.h"

namespace knightUIGameplayTags
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(UI_Layer_Game, "UI.Layer.Game", "Gameplay HUD layer.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(UI_Layer_GameMenu, "UI.Layer.GameMenu", "In-game menus over the HUD.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(UI_Layer_Menu, "UI.Layer.Menu", "Full-screen menus.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(UI_Layer_Modal, "UI.Layer.Modal", "Dialogs above every other layer.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(HUD_Slot_PlayerStatus, "HUD.Slot.PlayerStatus", "Health, boost gauge.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(HUD_Slot_BossStatus, "HUD.Slot.BossStatus", "Target monster status.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(HUD_Slot_RaidInfo, "HUD.Slot.RaidInfo", "Raid timer and phase.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(HUD_Slot_Reticle, "HUD.Slot.Reticle", "Lock-on marker.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(HUD_Slot_Notifications, "HUD.Slot.Notifications", "Part breaks, pickups, messages.");
}
