#include "System/knightGameplayTags.h"

namespace knightGameplayTags
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_Move, "InputTag.Move", "Movement input, handled natively.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_Move_Vertical, "InputTag.Move.Vertical", "Ascend and descend during boost flight, handled natively.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_Look, "InputTag.Look", "Camera input, handled natively.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_Look_Mouse, "InputTag.Look.Mouse", "Mouse camera input, handled natively.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_Attack_Primary, "InputTag.Attack.Primary", "Greatsword main attack. Hold to charge.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_Attack_Secondary, "InputTag.Attack.Secondary", "Greatsword secondary attack.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_Guard, "InputTag.Guard", "Guard. Pressing on an incoming hit is a parry.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_Boost, "InputTag.Boost", "Boost dash and evasive boost.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_Flight, "InputTag.Flight", "Toggle boost flight.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_LockOn, "InputTag.LockOn", "Toggle target lock.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_LockOn_SwitchLeft, "InputTag.LockOn.SwitchLeft", "Switch to the target point on the left.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_LockOn_SwitchRight, "InputTag.LockOn.SwitchRight", "Switch to the target point on the right.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_Interact, "InputTag.Interact", "Interact with gathering points and raid exits.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_Item_Use, "InputTag.Item.Use", "Use the selected consumable.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Attack, "Ability.Attack", "Any attack ability.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Attack_Charge, "Ability.Attack.Charge", "Charged greatsword attack.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Guard, "Ability.Guard", "Guard and parry ability.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Boost_Dash, "Ability.Boost.Dash", "Boost dash ability.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Boost_Flight, "Ability.Boost.Flight", "Boost flight ability.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_HitReact, "Ability.HitReact", "Hit reaction ability, activated by damage events.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Death, "Ability.Death", "Death ability, activated when Health reaches zero.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_ActivateFail_Cost, "Ability.ActivateFail.Cost", "Activation failed: cost not met.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_ActivateFail_Cooldown, "Ability.ActivateFail.Cooldown", "Activation failed: on cooldown.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_ActivateFail_TagsBlocked, "Ability.ActivateFail.TagsBlocked", "Activation failed: blocked by tags.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_ActivateFail_TagsMissing, "Ability.ActivateFail.TagsMissing", "Activation failed: required tags missing.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_ActivateFail_ActivationGroup, "Ability.ActivateFail.ActivationGroup", "Activation failed: an exclusive ability is running.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_InputBlocked, "State.InputBlocked", "Ability input is ignored and cleared (death, cinematics, menus).");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Dead, "State.Dead", "Health reached zero.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Invulnerable, "State.Invulnerable", "Hits are ignored (evasion frames, cinematics).");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_SuperArmor, "State.SuperArmor", "Damage applies but no hit reaction plays.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Guarding, "State.Guarding", "Guard is raised.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Parrying, "State.Parrying", "Parry window is active.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Charging, "State.Charging", "Holding a charged attack.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Boosting, "State.Boosting", "Boost dash in progress.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Flying, "State.Flying", "Boost flight movement mode.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Airborne, "State.Airborne", "Not on the ground for any reason.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_LockedOn, "State.LockedOn", "The player has a lock-on target (drives the lock-on camera mode).");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Overheated, "State.Overheated", "Boost gauge is depleted and recovering.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_HitReact, "State.HitReact", "Any hit reaction is playing.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_HitReact_Flinch, "State.HitReact.Flinch", "Light hit reaction.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_HitReact_Stagger, "State.HitReact.Stagger", "Poise broken.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_HitReact_Knockdown, "State.HitReact.Knockdown", "Knocked to the ground.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Monster_Enraged, "State.Monster.Enraged", "Monster rage state.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Monster_Exhausted, "State.Monster.Exhausted", "Monster fatigue state.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Window_Combo, "Window.Combo", "Buffered attack input may chain into the next attack.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Window_Cancel, "Window.Cancel", "Buffered evasive input may cancel the current action.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Window_MeleeTrace, "Window.MeleeTrace", "Weapon traces are active.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Melee_Hit, "Event.Melee.Hit", "A melee trace hit a valid target.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Damaged, "Event.Damaged", "Damage was applied to this actor.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Guarded, "Event.Guarded", "An incoming hit was guarded.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Parried, "Event.Parried", "An incoming hit was parried.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_GuardBroken, "Event.GuardBroken", "Guard was broken.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_PoiseBroken, "Event.PoiseBroken", "Poise reached zero.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_PartBroken, "Event.PartBroken", "A monster part was broken.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Death, "Event.Death", "Health reached zero.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(SetByCaller_Damage, "SetByCaller.Damage", "Base damage magnitude.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(SetByCaller_PoiseDamage, "SetByCaller.PoiseDamage", "Base poise damage magnitude.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(SetByCaller_MotionValue, "SetByCaller.MotionValue", "Per-attack motion value multiplier.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(SetByCaller_BoostCost, "SetByCaller.BoostCost", "Boost gauge cost.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(GameplayCue_Hit_Melee, "GameplayCue.Hit.Melee", "Melee hit feedback.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(GameplayCue_Hit_Weakpoint, "GameplayCue.Hit.Weakpoint", "Weak point hit feedback.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(GameplayCue_Guard, "GameplayCue.Guard", "Guard feedback.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(GameplayCue_Parry, "GameplayCue.Parry", "Parry feedback.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(GameplayCue_PartBreak, "GameplayCue.PartBreak", "Monster part break feedback.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(GameplayCue_Boost_Dash, "GameplayCue.Boost.Dash", "Boost dash thrusters.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Message_Combat_Hit, "Message.Combat.Hit", "Broadcast after a hit resolves.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Message_Combat_PartBroken, "Message.Combat.PartBroken", "Broadcast when a monster part breaks.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Message_Boss_PhaseChanged, "Message.Boss.PhaseChanged", "Broadcast when the boss enters a new phase.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Message_Boss_Enraged, "Message.Boss.Enraged", "Broadcast when the boss becomes enraged.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Message_Raid_PhaseChanged, "Message.Raid.PhaseChanged", "Broadcast when the raid phase changes.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Message_Player_Died, "Message.Player.Died", "Broadcast when the player's Knight is destroyed.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Monster_Part, "Monster.Part", "Root for monster part tags. Add specific parts in Config/Tags.");
}
