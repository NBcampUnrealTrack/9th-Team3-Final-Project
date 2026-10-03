// Monster test commands (development builds only).
//
// Every command drives the same code path a real event would take, so the commands test gameplay instead
// of bypassing it: hits go through UknightCombatLibrary::ApplyMeleeHit with a swept hit result, kills and
// poise breaks go through the Damage and PoiseDamage meta attributes, enrage and exhaustion through Rage
// and Stamina reaching their limits.
//
// The target is the player's lock-on target, otherwise the living monster nearest the player.
//
//   knight.Monster.Hit [Damage=50] [Part] [Poise=10]  one weapon-style hit, optionally aimed at a part
//   knight.Monster.BreakPart <Part>                   break a part through the normal break flow
//   knight.Monster.SetHealth <Percent>                0-100 (or 0-1); lowers through Damage, raises through Healing
//   knight.Monster.Kill [all]                         remaining Health as damage
//   knight.Monster.Enrage / Exhaust / PoiseBreak      fill Rage, empty Stamina, empty Poise
//   knight.Monster.FreezeAI [0|1]                     pause or resume every monster's StateTree; no argument toggles
//   knight.Monster.Status [all]                       attributes, state tags, target and parts in the log
//   knight.Player.Invulnerable [0|1]                  State.Invulnerable on the player; no argument toggles
//
// Part arguments match a full tag (Monster.Part.Test.Head) or its last segment (Head), case-insensitive.

#include "AIController.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "AbilitySystem/Attributes/knightCoreAttributeSet.h"
#include "AbilitySystem/Attributes/knightMonsterAttributeSet.h"
#include "AbilitySystem/Effects/knightGameplayEffect_Damage.h"
#include "BrainComponent.h"
#include "Combat/knightCombatLibrary.h"
#include "Combat/knightTargetLockComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameplayEffect.h"
#include "HAL/IConsoleManager.h"
#include "Interfaces/CombatInterface.h"
#include "Interfaces/TargetableInterface.h"
#include "Monster/knightMonsterCharacterBase.h"
#include "Monster/knightMonsterDefinition.h"
#include "Monster/knightMonsterPartComponent.h"
#include "Monster/knightMonsterTargetingComponent.h"
#include "System/knightGameplayTags.h"
#include "knightDev.h"

namespace knightMonsterCheats
{
	// Matches the melee trace's defaults so a cheat hit lands on the same bodies a sword would.
	constexpr float HitSweepRadius = 30.f;
	constexpr float HitApproachDistance = 300.f;
	constexpr float HitFollowThroughDistance = 100.f;

	static APawn* GetPlayerPawn(UWorld* World)
	{
		const APlayerController* const PC = World ? World->GetFirstPlayerController() : nullptr;
		return PC ? PC->GetPawn() : nullptr;
	}

	static bool IsAlive(const AknightMonsterCharacterBase* Monster)
	{
		return IsValid(Monster) && !Monster->IsDead();
	}

	/** Lock-on target, else the living monster nearest the player (or any living monster without a player). */
	static AknightMonsterCharacterBase* FindTargetMonster(UWorld* World)
	{
		APawn* const Player = GetPlayerPawn(World);
		if (const UknightTargetLockComponent* const Lock = Player ? Player->FindComponentByClass<UknightTargetLockComponent>() : nullptr)
		{
			AknightMonsterCharacterBase* const Locked = Cast<AknightMonsterCharacterBase>(Lock->GetLockedTarget());
			if (IsAlive(Locked))
			{
				return Locked;
			}
		}

		AknightMonsterCharacterBase* Best = nullptr;
		double BestDistanceSq = TNumericLimits<double>::Max();
		for (TActorIterator<AknightMonsterCharacterBase> It(World); It; ++It)
		{
			if (!IsAlive(*It))
			{
				continue;
			}
			const double DistanceSq = Player ? FVector::DistSquared(Player->GetActorLocation(), It->GetActorLocation()) : 0.0;
			if (DistanceSq < BestDistanceSq)
			{
				BestDistanceSq = DistanceSq;
				Best = *It;
			}
		}
		if (!Best)
		{
			UE_LOG(LogknightDev, Warning, TEXT("No living monster in this world."));
		}
		return Best;
	}

	/** One target, or every living monster when the first argument is "all". */
	static void GatherTargets(UWorld* World, const TArray<FString>& Args, TArray<AknightMonsterCharacterBase*>& OutMonsters)
	{
		if (Args.IsValidIndex(0) && Args[0].Equals(TEXT("all"), ESearchCase::IgnoreCase))
		{
			for (TActorIterator<AknightMonsterCharacterBase> It(World); It; ++It)
			{
				if (IsAlive(*It))
				{
					OutMonsters.Add(*It);
				}
			}
			return;
		}
		if (AknightMonsterCharacterBase* const Monster = FindTargetMonster(World))
		{
			OutMonsters.Add(Monster);
		}
	}

	static FGameplayTag FindPartTag(const AknightMonsterCharacterBase* Monster, const FString& Name)
	{
		const UknightMonsterPartComponent* const Parts = Monster ? Monster->GetPartComponent() : nullptr;
		if (!Parts || Name.IsEmpty())
		{
			return FGameplayTag();
		}
		for (const FknightMonsterPartState& State : Parts->GetPartStates())
		{
			const FString TagName = State.PartTag.ToString();
			if (TagName.Equals(Name, ESearchCase::IgnoreCase) || TagName.EndsWith(TEXT(".") + Name, ESearchCase::IgnoreCase))
			{
				return State.PartTag;
			}
		}
		FString Known;
		for (const FknightMonsterPartState& State : Parts->GetPartStates())
		{
			Known += TEXT(" ") + State.PartTag.ToString();
		}
		UE_LOG(LogknightDev, Warning, TEXT("%s has no part '%s'. Parts:%s"), *GetNameSafe(Monster), *Name, Known.IsEmpty() ? TEXT(" (none)") : *Known);
		return FGameplayTag();
	}

	/**
	 * Instant effect adding a SetByCaller amount to one attribute, built once per attribute. Writing meta
	 * attributes (Damage, PoiseDamage, Healing) or Rage/Stamina this way runs PostGameplayEffectExecute,
	 * which is where death, poise break, enrage and exhaustion are detected; setting the value directly would skip it.
	 */
	static const FName AmountName(TEXT("KnightDevAmount"));

	static const UGameplayEffect* GetAttributeDeltaEffect(const FGameplayAttribute& Attribute)
	{
		// Rooted rather than held in static smart pointers, which would be released after UObject shutdown on exit.
		static TMap<FString, UGameplayEffect*> Effects;
		const FString Key = Attribute.GetName();
		if (UGameplayEffect* const* const Existing = Effects.Find(Key))
		{
			return *Existing;
		}
		UGameplayEffect* const Effect = NewObject<UGameplayEffect>(GetTransientPackage(), *FString::Printf(TEXT("GE_KnightDev_%s"), *Key));
		Effect->AddToRoot();
		Effect->DurationPolicy = EGameplayEffectDurationType::Instant;

		FSetByCallerFloat SetByCaller;
		SetByCaller.DataName = AmountName;
		FGameplayModifierInfo Modifier;
		Modifier.Attribute = Attribute;
		Modifier.ModifierOp = EGameplayModOp::Additive;
		Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(SetByCaller);
		Effect->Modifiers.Add(Modifier);

		Effects.Add(Key, Effect);
		return Effect;
	}

	static void ApplyAttributeDelta(UAbilitySystemComponent* ASC, const FGameplayAttribute& Attribute, float Amount, AActor* Instigator)
	{
		if (!ASC || Amount == 0.f)
		{
			return;
		}
		FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
		if (Instigator)
		{
			Context.AddInstigator(Instigator, Instigator);
		}
		FGameplayEffectSpec Spec(GetAttributeDeltaEffect(Attribute), Context, 1.f);
		Spec.SetSetByCallerMagnitude(AmountName, Amount);
		ASC->ApplyGameplayEffectSpecToSelf(Spec);
	}

	static float GetAttribute(const UAbilitySystemComponent* ASC, const FGameplayAttribute& Attribute)
	{
		bool bFound = false;
		return ASC ? ASC->GetGameplayAttributeValue(Attribute, bFound) : 0.f;
	}

	static void LogHealth(const AknightMonsterCharacterBase* Monster, const TCHAR* Label)
	{
		const UAbilitySystemComponent* const ASC = Monster->GetAbilitySystemComponent();
		UE_LOG(LogknightDev, Display, TEXT("%s %s: Health %.1f / %.1f  Poise %.1f  Dead %d"), Label, *Monster->GetName(),
			GetAttribute(ASC, UknightCoreAttributeSet::GetHealthAttribute()), GetAttribute(ASC, UknightCoreAttributeSet::GetMaxHealthAttribute()),
			GetAttribute(ASC, UknightCoreAttributeSet::GetPoiseAttribute()), Monster->IsDead() ? 1 : 0);
	}

	// knight.Monster.Hit

	/** Where a sword would aim: the part, else the default lock-on point, else the actor origin. */
	static FVector GetAimPoint(AknightMonsterCharacterBase* Monster, const FGameplayTag& PartTag)
	{
		FVector Aim = Monster->GetActorLocation();
		if (PartTag.IsValid() && Monster->GetPartComponent()->GetPartLocation(PartTag, Aim))
		{
			return Aim;
		}
		const TArray<FknightTargetPoint> Points = ITargetableInterface::Execute_QueryTargetPoints(Monster);
		return Points.Num() > 0 && Points[0].Component ? Points[0].GetLocation() : Aim;
	}

	static void HitCommand(const TArray<FString>& Args, UWorld* World)
	{
		APawn* const Attacker = GetPlayerPawn(World);
		const UAbilitySystemComponent* const AttackerASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Attacker);
		if (!AttackerASC || !AttackerASC->GetSet<UknightCoreAttributeSet>())
		{
			UE_LOG(LogknightDev, Warning, TEXT("knight.Monster.Hit: the player pawn needs an ability system with knightCoreAttributeSet (damage reads both sides)."));
			return;
		}
		AknightMonsterCharacterBase* const Monster = FindTargetMonster(World);
		if (!Monster)
		{
			return;
		}

		const float Damage = Args.IsValidIndex(0) ? FCString::Atof(*Args[0]) : 50.f;
		const FGameplayTag AimPart = Args.IsValidIndex(1) ? FindPartTag(Monster, Args[1]) : FGameplayTag();
		if (Args.IsValidIndex(1) && !AimPart.IsValid())
		{
			return;
		}
		const float Poise = Args.IsValidIndex(2) ? FCString::Atof(*Args[2]) : 10.f;

		// Sweep in from the player's side toward the aim point with the weapon's object types.
		const FVector Aim = GetAimPoint(Monster, AimPart);
		const FVector Direction = (Aim - Attacker->GetActorLocation()).GetSafeNormal(UE_SMALL_NUMBER, Monster->GetActorForwardVector() * -1.f);
		const FVector Start = Aim - Direction * HitApproachDistance;
		const FVector End = Aim + Direction * HitFollowThroughDistance;

		FCollisionObjectQueryParams ObjectParams;
		ObjectParams.AddObjectTypesToQuery(ECC_Pawn);
		ObjectParams.AddObjectTypesToQuery(ECC_PhysicsBody);
		ObjectParams.AddObjectTypesToQuery(ECC_Destructible);
		FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(knightDevMonsterHit), false, Attacker);

		TArray<FHitResult> SweepHits;
		World->SweepMultiByObjectType(SweepHits, Start, End, FQuat::Identity, ObjectParams, FCollisionShape::MakeSphere(HitSweepRadius), QueryParams);
		// A named part wins when the sweep reached it, so overlapping parts can still be tested one by one.
		// Otherwise, the same per-actor choice the weapon trace makes.
		const FHitResult* Hit = AimPart.IsValid()
			? SweepHits.FindByPredicate([Monster, &AimPart](const FHitResult& Candidate)
				{ return Candidate.GetActor() == Monster && ICombatInterface::Execute_ResolveHitPartBP(Monster, Candidate) == AimPart; })
			: nullptr;
		TArray<FHitResult> Hits;
		if (!Hit)
		{
			UknightCombatLibrary::SelectBestHitPerActor(SweepHits, Hits);
			Hit = Hits.FindByPredicate([Monster](const FHitResult& Candidate) { return Candidate.GetActor() == Monster; });
		}

#if ENABLE_DRAW_DEBUG
		DrawDebugLine(World, Start, End, Hit ? FColor::Red : FColor::White, false, 2.f, 0, 2.f);
		if (Hit)
		{
			DrawDebugSphere(World, Hit->ImpactPoint, HitSweepRadius, 12, FColor::Red, false, 2.f);
		}
#endif
		if (!Hit)
		{
			UE_LOG(LogknightDev, Warning, TEXT("knight.Monster.Hit: the sweep did not reach %s (no query-enabled Pawn/PhysicsBody collision there?)."), *Monster->GetName());
			return;
		}

		const FGameplayTag HitPart = ICombatInterface::Execute_ResolveHitPartBP(Monster, *Hit);
		FknightMeleeHitParams Params;
		Params.DamageEffect = UknightGameplayEffect_Damage::StaticClass();
		Params.BaseDamage = Damage;
		Params.MotionValue = 0.f;
		Params.PoiseDamage = Poise;

		LogHealth(Monster, TEXT("before"));
		const EknightHitOutcome Outcome = UknightCombatLibrary::ApplyMeleeHit(Attacker, *Hit, Params);
		UE_LOG(LogknightDev, Display, TEXT("knight.Monster.Hit %.1f -> %s: outcome %s, component %s, bone %s, part %s (aimed at %s)"),
			Damage, *Monster->GetName(), *UEnum::GetValueAsString(Outcome), *GetNameSafe(Hit->GetComponent()), *Hit->BoneName.ToString(),
			HitPart.IsValid() ? *HitPart.ToString() : TEXT("NONE"), AimPart.IsValid() ? *AimPart.ToString() : TEXT("default point"));
		LogHealth(Monster, TEXT("after "));
	}

	// knight.Monster.BreakPart <Part>

	static void BreakPartCommand(const TArray<FString>& Args, UWorld* World)
	{
		AknightMonsterCharacterBase* const Monster = FindTargetMonster(World);
		if (!Monster || !Args.IsValidIndex(0))
		{
			if (Monster)
			{
				UE_LOG(LogknightDev, Warning, TEXT("Usage: knight.Monster.BreakPart <Part>"));
			}
			return;
		}
		const FGameplayTag PartTag = FindPartTag(Monster, Args[0]);
		UknightMonsterPartComponent* const Parts = Monster->GetPartComponent();
		if (!PartTag.IsValid())
		{
			return;
		}
		if (Parts->IsPartBroken(PartTag))
		{
			UE_LOG(LogknightDev, Display, TEXT("%s: %s is already broken."), *Monster->GetName(), *PartTag.ToString());
			return;
		}
		// Enough part damage for any break threshold; non-breakable parts just accumulate it.
		Parts->ApplyPartDamage(PartTag, 1.e9f, GetPlayerPawn(World));
		UE_LOG(LogknightDev, Display, TEXT("%s: %s %s."), *Monster->GetName(), *PartTag.ToString(),
			Parts->IsPartBroken(PartTag) ? TEXT("broken") : TEXT("is not breakable"));
	}

	// knight.Monster.SetHealth <Percent>

	static void SetHealthCommand(const TArray<FString>& Args, UWorld* World)
	{
		if (!Args.IsValidIndex(0))
		{
			UE_LOG(LogknightDev, Warning, TEXT("Usage: knight.Monster.SetHealth <Percent 0-100>"));
			return;
		}
		AknightMonsterCharacterBase* const Monster = FindTargetMonster(World);
		UAbilitySystemComponent* const ASC = Monster ? Monster->GetAbilitySystemComponent() : nullptr;
		if (!ASC)
		{
			return;
		}
		float Fraction = FCString::Atof(*Args[0]);
		Fraction = FMath::Clamp(Fraction > 1.f ? Fraction / 100.f : Fraction, 0.f, 1.f);
		const float Health = GetAttribute(ASC, UknightCoreAttributeSet::GetHealthAttribute());
		const float Desired = GetAttribute(ASC, UknightCoreAttributeSet::GetMaxHealthAttribute()) * Fraction;

		LogHealth(Monster, TEXT("before"));
		if (Desired < Health)
		{
			ApplyAttributeDelta(ASC, UknightCoreAttributeSet::GetDamageAttribute(), Health - Desired, GetPlayerPawn(World));
		}
		else
		{
			ApplyAttributeDelta(ASC, UknightCoreAttributeSet::GetHealingAttribute(), Desired - Health, GetPlayerPawn(World));
		}
		LogHealth(Monster, TEXT("after "));
	}

	// knight.Monster.Kill [all]

	static void KillCommand(const TArray<FString>& Args, UWorld* World)
	{
		TArray<AknightMonsterCharacterBase*> Monsters;
		GatherTargets(World, Args, Monsters);
		for (AknightMonsterCharacterBase* const Monster : Monsters)
		{
			UAbilitySystemComponent* const ASC = Monster->GetAbilitySystemComponent();
			ApplyAttributeDelta(ASC, UknightCoreAttributeSet::GetDamageAttribute(),
				GetAttribute(ASC, UknightCoreAttributeSet::GetHealthAttribute()) + 1.f, GetPlayerPawn(World));
			UE_LOG(LogknightDev, Display, TEXT("knight.Monster.Kill: %s dead %d"), *Monster->GetName(), Monster->IsDead() ? 1 : 0);
		}
	}

	// knight.Monster.Enrage / Exhaust / PoiseBreak

	static void EnrageCommand(const TArray<FString>& Args, UWorld* World)
	{
		AknightMonsterCharacterBase* const Monster = FindTargetMonster(World);
		UAbilitySystemComponent* const ASC = Monster ? Monster->GetAbilitySystemComponent() : nullptr;
		if (!ASC)
		{
			return;
		}
		const UknightMonsterDefinition* const Definition = Monster->GetMonsterDefinition();
		if (!Definition || !Definition->EnrageEffect)
		{
			UE_LOG(LogknightDev, Warning, TEXT("%s has no EnrageEffect; Rage fills but nothing applies State.Monster.Enraged."), *Monster->GetName());
		}
		ApplyAttributeDelta(ASC, UknightMonsterAttributeSet::GetRageAttribute(), GetAttribute(ASC, UknightMonsterAttributeSet::GetMaxRageAttribute()), GetPlayerPawn(World));
		UE_LOG(LogknightDev, Display, TEXT("knight.Monster.Enrage: %s enraged %d"), *Monster->GetName(),
			ASC->HasMatchingGameplayTag(knightGameplayTags::State_Monster_Enraged) ? 1 : 0);
	}

	static void ExhaustCommand(const TArray<FString>& Args, UWorld* World)
	{
		AknightMonsterCharacterBase* const Monster = FindTargetMonster(World);
		UAbilitySystemComponent* const ASC = Monster ? Monster->GetAbilitySystemComponent() : nullptr;
		if (!ASC)
		{
			return;
		}
		const UknightMonsterDefinition* const Definition = Monster->GetMonsterDefinition();
		if (!Definition || !Definition->ExhaustEffect)
		{
			UE_LOG(LogknightDev, Warning, TEXT("%s has no ExhaustEffect; Stamina empties but nothing applies State.Monster.Exhausted."), *Monster->GetName());
		}
		ApplyAttributeDelta(ASC, UknightMonsterAttributeSet::GetStaminaAttribute(), -GetAttribute(ASC, UknightMonsterAttributeSet::GetStaminaAttribute()) - 1.f, GetPlayerPawn(World));
		UE_LOG(LogknightDev, Display, TEXT("knight.Monster.Exhaust: %s exhausted %d"), *Monster->GetName(),
			ASC->HasMatchingGameplayTag(knightGameplayTags::State_Monster_Exhausted) ? 1 : 0);
	}

	static void PoiseBreakCommand(const TArray<FString>& Args, UWorld* World)
	{
		AknightMonsterCharacterBase* const Monster = FindTargetMonster(World);
		UAbilitySystemComponent* const ASC = Monster ? Monster->GetAbilitySystemComponent() : nullptr;
		if (!ASC)
		{
			return;
		}
		ApplyAttributeDelta(ASC, UknightCoreAttributeSet::GetPoiseDamageAttribute(), GetAttribute(ASC, UknightCoreAttributeSet::GetPoiseAttribute()) + 1.f, GetPlayerPawn(World));
		LogHealth(Monster, TEXT("knight.Monster.PoiseBreak"));
	}

	// knight.Monster.FreezeAI [0|1]

	static void FreezeAICommand(const TArray<FString>& Args, UWorld* World)
	{
		static bool bFrozen = false;
		bFrozen = Args.IsValidIndex(0) ? FCString::Atoi(*Args[0]) != 0 : !bFrozen;
		int32 Count = 0;
		for (TActorIterator<AknightMonsterCharacterBase> It(World); It; ++It)
		{
			AAIController* const Controller = Cast<AAIController>(It->GetController());
			UBrainComponent* const Brain = Controller ? Controller->GetBrainComponent() : nullptr;
			if (!Brain || It->IsDead())
			{
				continue;
			}
			if (bFrozen)
			{
				Controller->StopMovement();
				Brain->PauseLogic(TEXT("knight.Monster.FreezeAI"));
			}
			else
			{
				Brain->ResumeLogic(TEXT("knight.Monster.FreezeAI"));
			}
			++Count;
		}
		UE_LOG(LogknightDev, Display, TEXT("knight.Monster.FreezeAI: %s %d monster(s)."), bFrozen ? TEXT("froze") : TEXT("resumed"), Count);
	}

	// knight.Monster.Status [all]

	static void StatusCommand(const TArray<FString>& Args, UWorld* World)
	{
		TArray<AknightMonsterCharacterBase*> Monsters;
		GatherTargets(World, Args, Monsters);
		for (const AknightMonsterCharacterBase* const Monster : Monsters)
		{
			const UAbilitySystemComponent* const ASC = Monster->GetAbilitySystemComponent();
			LogHealth(Monster, TEXT("status"));
			UE_LOG(LogknightDev, Display, TEXT("  Rage %.1f / %.1f  Stamina %.1f / %.1f  Target %s"),
				GetAttribute(ASC, UknightMonsterAttributeSet::GetRageAttribute()), GetAttribute(ASC, UknightMonsterAttributeSet::GetMaxRageAttribute()),
				GetAttribute(ASC, UknightMonsterAttributeSet::GetStaminaAttribute()), GetAttribute(ASC, UknightMonsterAttributeSet::GetMaxStaminaAttribute()),
				*GetNameSafe(Monster->GetTargetingComponent()->GetCurrentTarget()));

			FGameplayTagContainer Tags;
			ASC->GetOwnedGameplayTags(Tags);
			UE_LOG(LogknightDev, Display, TEXT("  Tags %s"), *Tags.ToStringSimple());

			for (const FknightMonsterPartState& Part : Monster->GetPartComponent()->GetPartStates())
			{
				UE_LOG(LogknightDev, Display, TEXT("  Part %s  damage %.1f  broken %d  multiplier %.2f"), *Part.PartTag.ToString(),
					Part.DamageTaken, Part.bBroken ? 1 : 0, Monster->GetPartComponent()->GetDamageMultiplier(Part.PartTag));
			}
		}
	}

	// knight.Player.Invulnerable [0|1]

	static void PlayerInvulnerableCommand(const TArray<FString>& Args, UWorld* World)
	{
		UAbilitySystemComponent* const ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetPlayerPawn(World));
		if (!ASC)
		{
			UE_LOG(LogknightDev, Warning, TEXT("knight.Player.Invulnerable: no player pawn with an ability system."));
			return;
		}
		const FGameplayTag Tag = knightGameplayTags::State_Invulnerable;
		const bool bHas = ASC->GetTagCount(Tag) > 0;
		const bool bWant = Args.IsValidIndex(0) ? FCString::Atoi(*Args[0]) != 0 : !bHas;
		// Loose tags count; set the count to exactly 1 or 0 so repeated commands do not stack.
		ASC->SetLooseGameplayTagCount(Tag, bWant ? 1 : 0);
		UE_LOG(LogknightDev, Display, TEXT("knight.Player.Invulnerable: %d (hits on the player now resolve as Evaded)."), bWant ? 1 : 0);
	}

	static FAutoConsoleCommandWithWorldAndArgs HitCmd(TEXT("knight.Monster.Hit"),
		TEXT("knight.Monster.Hit [Damage=50] [Part] [Poise=10]. One weapon-style hit on the target monster through ApplyMeleeHit."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&HitCommand), ECVF_Cheat);
	static FAutoConsoleCommandWithWorldAndArgs BreakPartCmd(TEXT("knight.Monster.BreakPart"),
		TEXT("knight.Monster.BreakPart <Part>. Breaks a part through the normal break flow."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&BreakPartCommand), ECVF_Cheat);
	static FAutoConsoleCommandWithWorldAndArgs SetHealthCmd(TEXT("knight.Monster.SetHealth"),
		TEXT("knight.Monster.SetHealth <Percent>. 0-100 or 0-1, through Damage/Healing."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&SetHealthCommand), ECVF_Cheat);
	static FAutoConsoleCommandWithWorldAndArgs KillCmd(TEXT("knight.Monster.Kill"),
		TEXT("knight.Monster.Kill [all]. Remaining Health as damage."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&KillCommand), ECVF_Cheat);
	static FAutoConsoleCommandWithWorldAndArgs EnrageCmd(TEXT("knight.Monster.Enrage"),
		TEXT("Fills the target monster's Rage, which applies its EnrageEffect."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&EnrageCommand), ECVF_Cheat);
	static FAutoConsoleCommandWithWorldAndArgs ExhaustCmd(TEXT("knight.Monster.Exhaust"),
		TEXT("Empties the target monster's Stamina, which applies its ExhaustEffect."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ExhaustCommand), ECVF_Cheat);
	static FAutoConsoleCommandWithWorldAndArgs PoiseBreakCmd(TEXT("knight.Monster.PoiseBreak"),
		TEXT("Empties the target monster's Poise through PoiseDamage."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&PoiseBreakCommand), ECVF_Cheat);
	static FAutoConsoleCommandWithWorldAndArgs FreezeAICmd(TEXT("knight.Monster.FreezeAI"),
		TEXT("knight.Monster.FreezeAI [0|1]. Pauses or resumes every monster's StateTree; no argument toggles."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&FreezeAICommand), ECVF_Cheat);
	static FAutoConsoleCommandWithWorldAndArgs StatusCmd(TEXT("knight.Monster.Status"),
		TEXT("knight.Monster.Status [all]. Logs attributes, tags, target and parts."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&StatusCommand), ECVF_Cheat);
	static FAutoConsoleCommandWithWorldAndArgs PlayerInvulnerableCmd(TEXT("knight.Player.Invulnerable"),
		TEXT("knight.Player.Invulnerable [0|1]. Toggles State.Invulnerable on the player."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&PlayerInvulnerableCommand), ECVF_Cheat);
}
