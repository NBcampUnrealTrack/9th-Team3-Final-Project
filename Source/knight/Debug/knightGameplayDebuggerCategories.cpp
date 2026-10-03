#include "Debug/knightGameplayDebuggerCategories.h"

#if WITH_GAMEPLAY_DEBUGGER

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "AbilitySystem/Abilities/knightGameplayAbility.h"
#include "AbilitySystem/Abilities/knightGameplayAbility_MonsterAttack.h"
#include "AbilitySystem/Attributes/knightAttributeSet.h"
#include "AbilitySystem/Attributes/knightCoreAttributeSet.h"
#include "AbilitySystem/Attributes/knightMonsterAttributeSet.h"
#include "Combat/knightHitStopComponent.h"
#include "Combat/knightMeleeTraceComponent.h"
#include "Combat/knightTargetLockComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameplayDebugger.h"
#include "Input/knightInputBufferComponent.h"
#include "Monster/knightMonsterCharacterBase.h"
#include "Monster/knightMonsterDefinition.h"
#include "Monster/knightMonsterPartComponent.h"
#include "Monster/knightMonsterTargetingComponent.h"
#include "System/knightGameplayTags.h"

namespace
{
	FString AttributeLine(const UAbilitySystemComponent& ASC, const TCHAR* Label, const FGameplayAttribute& Current, const FGameplayAttribute& Max)
	{
		bool bFound = false;
		const float Value = ASC.GetGameplayAttributeValue(Current, bFound);
		if (!bFound)
		{
			return FString();
		}
		const float MaxValue = ASC.GetGameplayAttributeValue(Max, bFound);
		return FString::Printf(TEXT("%s {white}%.0f / %.0f"), Label, Value, MaxValue);
	}

	FString FlagText(bool bValue)
	{
		return bValue ? TEXT("{green}yes") : TEXT("{grey}no");
	}

	FString GroupName(EknightAbilityActivationGroup Group)
	{
		switch (Group)
		{
		case EknightAbilityActivationGroup::Exclusive_Replaceable: return TEXT("Replaceable");
		case EknightAbilityActivationGroup::Exclusive_Blocking: return TEXT("Blocking");
		default: return TEXT("Independent");
		}
	}
}

FknightGameplayDebuggerCategoryBase::FknightGameplayDebuggerCategoryBase()
{
	SetDataPackReplication<FRepData>(&DataPack);
}

void FknightGameplayDebuggerCategoryBase::CollectData(APlayerController* OwnerPC, AActor* DebugActor)
{
	DataPack.Lines.Reset();
	const AActor* const Actor = DebugActor ? DebugActor : (OwnerPC ? OwnerPC->GetPawn() : nullptr);
	if (!Actor)
	{
		DataPack.Lines.Add(TEXT("{grey}No debug actor."));
		return;
	}
	DataPack.Lines.Add(FString::Printf(TEXT("{yellow}%s"), *Actor->GetName()));
	CollectLines(*Actor, DataPack.Lines);
}

void FknightGameplayDebuggerCategoryBase::DrawData(APlayerController* OwnerPC, FGameplayDebuggerCanvasContext& CanvasContext)
{
	for (const FString& Line : DataPack.Lines)
	{
		CanvasContext.Print(Line);
	}
}

void FknightGameplayDebuggerCategory_Abilities::CollectLines(const AActor& Actor, TArray<FString>& OutLines) const
{
	const UAbilitySystemComponent* const ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(&Actor);
	if (!ASC)
	{
		OutLines.Add(TEXT("{grey}No ability system."));
		return;
	}

	TArray<FString> StateTags;
	for (const FGameplayTag& Tag : ASC->GetOwnedGameplayTags())
	{
		const FString Name = Tag.ToString();
		if (Name.StartsWith(TEXT("State.")) || Name.StartsWith(TEXT("Window.")))
		{
			StateTags.Add(Name);
		}
	}
	OutLines.Add(FString::Printf(TEXT("Tags: {white}%s"), StateTags.IsEmpty() ? TEXT("-") : *FString::Join(StateTags, TEXT(", "))));

	OutLines.Add(TEXT("Active abilities:"));
	bool bAnyActive = false;
	for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
	{
		if (!Spec.IsActive() || !Spec.Ability)
		{
			continue;
		}
		bAnyActive = true;
		const UknightGameplayAbility* const KnightAbility = Cast<UknightGameplayAbility>(Spec.Ability);
		OutLines.Add(FString::Printf(TEXT("  {green}%s {grey}[%s] %s"), *Spec.Ability->GetClass()->GetName(),
			KnightAbility ? *GroupName(KnightAbility->GetActivationGroup()) : TEXT("-"),
			*Spec.GetDynamicSpecSourceTags().ToStringSimple()));
	}
	if (!bAnyActive)
	{
		OutLines.Add(TEXT("  {grey}none"));
	}

	for (const FString& Line : {
		AttributeLine(*ASC, TEXT("Health"), UknightCoreAttributeSet::GetHealthAttribute(), UknightCoreAttributeSet::GetMaxHealthAttribute()),
		AttributeLine(*ASC, TEXT("Poise"), UknightCoreAttributeSet::GetPoiseAttribute(), UknightCoreAttributeSet::GetMaxPoiseAttribute()),
		AttributeLine(*ASC, TEXT("Boost"), UknightAttributeSet::GetBoostGaugeAttribute(), UknightAttributeSet::GetMaxBoostGaugeAttribute()),
		AttributeLine(*ASC, TEXT("Rage"), UknightMonsterAttributeSet::GetRageAttribute(), UknightMonsterAttributeSet::GetMaxRageAttribute()),
		AttributeLine(*ASC, TEXT("Stamina"), UknightMonsterAttributeSet::GetStaminaAttribute(), UknightMonsterAttributeSet::GetMaxStaminaAttribute()) })
	{
		if (!Line.IsEmpty())
		{
			OutLines.Add(Line);
		}
	}
	bool bFound = false;
	OutLines.Add(FString::Printf(TEXT("Attack {white}%.0f{grey}  Defense {white}%.0f{grey}  Crit {white}%.0f%%"),
		ASC->GetGameplayAttributeValue(UknightCoreAttributeSet::GetAttackPowerAttribute(), bFound),
		ASC->GetGameplayAttributeValue(UknightCoreAttributeSet::GetDefenseAttribute(), bFound),
		ASC->GetGameplayAttributeValue(UknightCoreAttributeSet::GetCriticalChanceAttribute(), bFound) * 100.f));
}

void FknightGameplayDebuggerCategory_Combat::CollectLines(const AActor& Actor, TArray<FString>& OutLines) const
{
	if (const UknightInputBufferComponent* const Buffer = Actor.FindComponentByClass<UknightInputBufferComponent>())
	{
		const FGameplayTag Buffered = Buffer->GetBufferedInputTag();
		OutLines.Add(FString::Printf(TEXT("Input buffer: {white}%s"), Buffered.IsValid() ? *Buffered.ToString() : TEXT("-")));
	}
	if (const UknightTargetLockComponent* const Lock = Actor.FindComponentByClass<UknightTargetLockComponent>())
	{
		OutLines.Add(Lock->IsLockedOn()
			? FString::Printf(TEXT("Lock-on: {green}%s {grey}(%s)"), *GetNameSafe(Lock->GetLockedTarget()), *Lock->GetLockedPartTag().ToString())
			: FString(TEXT("Lock-on: {grey}none")));
	}
	if (const UknightHitStopComponent* const HitStop = Actor.FindComponentByClass<UknightHitStopComponent>())
	{
		OutLines.Add(FString::Printf(TEXT("Hit stop: %s"), *FlagText(HitStop->IsInHitStop())));
	}
	if (const UknightMeleeTraceComponent* const Trace = Actor.FindComponentByClass<UknightMeleeTraceComponent>())
	{
		OutLines.Add(FString::Printf(TEXT("Weapon trace: %s"), *FlagText(Trace->IsTracing())));
	}
	if (const UAbilitySystemComponent* const ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(&Actor))
	{
		OutLines.Add(FString::Printf(TEXT("Combo %s{grey}  Cancel %s{grey}  Invulnerable %s{grey}  SuperArmor %s{grey}  Guard %s{grey}  Parry %s"),
			*FlagText(ASC->HasMatchingGameplayTag(knightGameplayTags::Window_Combo)),
			*FlagText(ASC->HasMatchingGameplayTag(knightGameplayTags::Window_Cancel)),
			*FlagText(ASC->HasMatchingGameplayTag(knightGameplayTags::State_Invulnerable)),
			*FlagText(ASC->HasMatchingGameplayTag(knightGameplayTags::State_SuperArmor)),
			*FlagText(ASC->HasMatchingGameplayTag(knightGameplayTags::State_Guarding)),
			*FlagText(ASC->HasMatchingGameplayTag(knightGameplayTags::State_Parrying))));
	}
}

void FknightGameplayDebuggerCategory_Monster::CollectLines(const AActor& Actor, TArray<FString>& OutLines) const
{
	const AknightMonsterCharacterBase* const Monster = Cast<AknightMonsterCharacterBase>(&Actor);
	if (!Monster)
	{
		OutLines.Add(TEXT("{grey}Not a monster. Select one with the debugger (look at it and press the apostrophe key)."));
		return;
	}
	const UknightMonsterDefinition* const Definition = Monster->GetMonsterDefinition();
	OutLines.Add(FString::Printf(TEXT("Definition: {white}%s{grey}  Dead: %s"), *GetNameSafe(Definition), *FlagText(Monster->IsDead())));

	const UknightMonsterTargetingComponent* const Targeting = Monster->GetTargetingComponent();
	const AActor* const Target = Targeting ? Targeting->GetCurrentTarget() : nullptr;
	const FknightMonsterAttackContext AttackContext = FknightMonsterAttackContext::Make(Monster, Target);
	OutLines.Add(FString::Printf(TEXT("Target: {white}%s{grey}  Distance {white}%.0f{grey}  Angle {white}%.0f"),
		Target ? *GetNameSafe(Target) : TEXT("-"), AttackContext.Distance, AttackContext.AngleToTarget));

	// Attack candidates as the StateTree's Knight Activate Ability task would weigh them now.
	if (const UAbilitySystemComponent* const ASC = Monster->GetAbilitySystemComponent())
	{
		OutLines.Add(FString::Printf(TEXT("Reacting: %s  Attacks:"), *FlagText(ASC->HasMatchingGameplayTag(knightGameplayTags::State_Monster_Reacting))));
		for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
		{
			const UGameplayAbility* const Instance = Spec.GetPrimaryInstance() ? Spec.GetPrimaryInstance() : Spec.Ability.Get();
			const UknightGameplayAbility_MonsterAttack* const Attack = Cast<UknightGameplayAbility_MonsterAttack>(Instance);
			if (!Attack)
			{
				continue;
			}
			const FknightMonsterAttackSelection& Selection = Attack->GetSelection();
			const float Weight = Attack->GetSelectionWeight(AttackContext);
			OutLines.Add(FString::Printf(TEXT("  %s%s {grey}weight %.1f  range %.0f-%.0f  angle %.0f  cooldown %.1f%s"),
				Weight > 0.f ? TEXT("{green}") : TEXT("{white}"), *Attack->GetClass()->GetName(), Weight,
				Selection.MinRange, Selection.MaxRange, Selection.MaxAngle, Attack->GetCooldownRemaining(AttackContext.WorldTime),
				Spec.IsActive() ? TEXT("  {yellow}ACTIVE") : TEXT("")));
		}
	}

	const UknightMonsterPartComponent* const Parts = Monster->GetPartComponent();
	if (!Parts || Parts->GetPartStates().IsEmpty())
	{
		OutLines.Add(TEXT("{grey}No parts."));
		return;
	}
	OutLines.Add(TEXT("Parts:"));
	for (const FknightMonsterPartState& State : Parts->GetPartStates())
	{
		OutLines.Add(FString::Printf(TEXT("  %s%s {grey}x%.2f  taken {white}%.0f"),
			State.bBroken ? TEXT("{red}") : TEXT("{white}"), *State.PartTag.ToString(),
			Parts->GetDamageMultiplier(State.PartTag), State.DamageTaken));
	}
}

namespace knightGameplayDebugger
{
	static const FName AbilitiesCategory(TEXT("KnightAbilities"));
	static const FName CombatCategory(TEXT("KnightCombat"));
	static const FName MonsterCategory(TEXT("KnightMonster"));

	void RegisterCategories()
	{
		IGameplayDebugger& Debugger = IGameplayDebugger::Get();
		Debugger.RegisterCategory(AbilitiesCategory, IGameplayDebugger::FOnGetCategory::CreateStatic(&FknightGameplayDebuggerCategory_Abilities::MakeInstance),
			EGameplayDebuggerCategoryState::EnabledInGameAndSimulate);
		Debugger.RegisterCategory(CombatCategory, IGameplayDebugger::FOnGetCategory::CreateStatic(&FknightGameplayDebuggerCategory_Combat::MakeInstance),
			EGameplayDebuggerCategoryState::EnabledInGame);
		Debugger.RegisterCategory(MonsterCategory, IGameplayDebugger::FOnGetCategory::CreateStatic(&FknightGameplayDebuggerCategory_Monster::MakeInstance),
			EGameplayDebuggerCategoryState::EnabledInGameAndSimulate);
		Debugger.NotifyCategoriesChanged();
	}

	void UnregisterCategories()
	{
		if (IGameplayDebugger::IsAvailable())
		{
			IGameplayDebugger& Debugger = IGameplayDebugger::Get();
			Debugger.UnregisterCategory(AbilitiesCategory);
			Debugger.UnregisterCategory(CombatCategory);
			Debugger.UnregisterCategory(MonsterCategory);
			Debugger.NotifyCategoriesChanged();
		}
	}
}

#endif // WITH_GAMEPLAY_DEBUGGER
