#pragma once

#include "NativeGameplayTags.h"

/**
 * Tags the C++ code refers to by name. Content-only tags (per-monster parts, per-attack cues)
 * belong in Config/Tags/*.ini instead, so designers can add them without a recompile.
 *
 * Root rules:
 *   InputTag.*     Input actions bound through UknightInputConfig
 *   Ability.*      Ability identity and activation failure reasons
 *   State.*        Loose or GE-granted tags describing what an actor is doing
 *   Window.*       Animation windows opened by AnimNotifyStates
 *   Event.*        Gameplay events sent to abilities
 *   SetByCaller.*  Magnitudes passed into GameplayEffectSpecs
 *   GameplayCue.*  Cosmetic feedback only; never read by gameplay logic
 *   Message.*      GameplayMessageRouter channels (UI and presentation listeners)
 *   Monster.Part.* Monster part identities; specific parts are defined per monster in ini
 */
namespace knightGameplayTags
{
	// Input
	KNIGHT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_Move);
	KNIGHT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_Move_Vertical);
	KNIGHT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_Look);
	KNIGHT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_Look_Mouse);
	KNIGHT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_Attack_Primary);
	KNIGHT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_Attack_Secondary);
	KNIGHT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_Guard);
	KNIGHT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_Boost);
	KNIGHT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_Flight);
	KNIGHT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_LockOn);
	KNIGHT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_LockOn_SwitchLeft);
	KNIGHT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_LockOn_SwitchRight);
	KNIGHT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_Interact);
	KNIGHT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_Item_Use);

	// Ability identity
	KNIGHT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Attack);
	KNIGHT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Attack_Charge);
	KNIGHT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Guard);
	KNIGHT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Boost_Dash);
	KNIGHT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Boost_Flight);
	KNIGHT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_HitReact);
	KNIGHT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Death);

	// Ability activation failure reasons (read by UI and debugger)
	KNIGHT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_ActivateFail_Cost);
	KNIGHT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_ActivateFail_Cooldown);
	KNIGHT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_ActivateFail_TagsBlocked);
	KNIGHT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_ActivateFail_TagsMissing);
	KNIGHT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_ActivateFail_ActivationGroup);

	// State
	KNIGHT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_InputBlocked);
	KNIGHT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Dead);
	KNIGHT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Invulnerable);
	KNIGHT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_SuperArmor);
	KNIGHT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Guarding);
	KNIGHT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Parrying);
	KNIGHT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Charging);
	KNIGHT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Boosting);
	KNIGHT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Flying);
	KNIGHT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Airborne);
	KNIGHT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_LockedOn);
	KNIGHT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Overheated);
	KNIGHT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_HitReact);
	KNIGHT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_HitReact_Flinch);
	KNIGHT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_HitReact_Stagger);
	KNIGHT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_HitReact_Knockdown);
	KNIGHT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Monster_Enraged);
	KNIGHT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Monster_Exhausted);
	KNIGHT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Monster_Reacting);

	// Animation windows
	KNIGHT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Window_Combo);
	KNIGHT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Window_Cancel);
	KNIGHT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Window_MeleeTrace);

	// Gameplay events
	KNIGHT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Melee_Hit);
	KNIGHT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Damaged);
	KNIGHT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Guarded);
	KNIGHT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Parried);
	KNIGHT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_GuardBroken);
	KNIGHT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_PoiseBroken);
	KNIGHT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_PartBroken);
	KNIGHT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Death);

	// SetByCaller magnitudes
	KNIGHT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_Damage);
	KNIGHT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_PoiseDamage);
	KNIGHT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_MotionValue);
	KNIGHT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_BoostCost);

	// Gameplay cues
	KNIGHT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_Hit_Melee);
	KNIGHT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_Hit_Weakpoint);
	KNIGHT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_Guard);
	KNIGHT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_Parry);
	KNIGHT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_PartBreak);
	KNIGHT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_Boost_Dash);

	// Message channels
	KNIGHT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Message_Combat_Hit);
	KNIGHT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Message_Combat_PartBroken);
	KNIGHT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Message_Boss_PhaseChanged);
	KNIGHT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Message_Boss_Enraged);
	KNIGHT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Message_Raid_PhaseChanged);
	KNIGHT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Message_Player_Died);

	// Monster parts
	KNIGHT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Monster_Part);

	// Items and equipment
	KNIGHT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Item_Type_Material);
	KNIGHT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Item_Type_Part);
	KNIGHT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Item_Type_Weapon);
	KNIGHT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Equipment_Slot_Head);
	KNIGHT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Equipment_Slot_Torso);
	KNIGHT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Equipment_Slot_Arms);
	KNIGHT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Equipment_Slot_Legs);
	KNIGHT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Equipment_Slot_Backpack);
	KNIGHT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Equipment_Slot_Weapon);
}
