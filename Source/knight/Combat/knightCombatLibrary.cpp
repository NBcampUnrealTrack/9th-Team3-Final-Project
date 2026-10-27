#include "Combat/knightCombatLibrary.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Abilities/GameplayAbility.h"
#include "Animation/AnimMontage.h"
#include "AnimNotifyState_MotionWarping.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TimerManager.h"
#include "Combat/knightHitStopComponent.h"
#include "Combat/knightGroggyFeedbackComponent.h"
#include "Combat/knightTargetLockComponent.h"
#include "GameplayEffect.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GenericTeamAgentInterface.h"
#include "Interfaces/CombatInterface.h"
#include "Interfaces/TargetableInterface.h"
#include "MotionWarpingComponent.h"
#include "Perception/AISense_Hearing.h"
#include "System/knightGameplayTags.h"
#include "AbilitySystem/Executions/knightDamageExecution.h"
#include "AbilitySystem/Attributes/knightCoreAttributeSet.h"
#include "AbilitySystem/knightAbilitySystemComponent.h"
#include "knight.h"
#include "knightCharacter.h"
#include "Monster/knightMonsterCharacterBase.h"
#include "HAL/IConsoleManager.h"

namespace knightParryRules
{
	// (2026-10-27 팀 투표: 퍼펙트 패링의 그로기 피해 삭제) 0 = 패링은 그로기를 깎지 않는다, 1 = 예전 규칙.
	static TAutoConsoleVariable<int32> CVarParryBuildsPoise(TEXT("knight.Combat.ParryBuildsPoise"), 0,
		TEXT("1 = a perfect guard (parry) damages the attacker's poise (knight guard + monster ParriedPoiseDamageRatio), 0 = no poise damage from parries (default, team decision 2026-10-27)."));
}

bool UknightCombatLibrary::DoesParryBuildPoise()
{
	return knightParryRules::CVarParryBuildsPoise.GetValueOnGameThread() != 0;
}

namespace
{
	// 공격의 피해 이펙트가 쓰는 피해 실행(UknightDamageExecution)의 가드 범위로 판정한다. 퍼펙트 가드가 일반 가드와 같은 각도를 쓰게 하려는 것.
	// 피해 이펙트가 없거나 다른 실행을 쓰면 기본 피해 실행의 각도를 쓴다.
	// (2026-10-12) 방향은 타격 결과의 공격이 들어온 지점(TraceStart)에서 잰다. 피해 실행과 같은 기준이다.
	bool IsAttackerInGuardArc(const TSubclassOf<UGameplayEffect>& DamageEffect, const AActor* Target, const AActor* Attacker, const FHitResult& Hit)
	{
		if (const UGameplayEffect* const Effect = DamageEffect ? DamageEffect->GetDefaultObject<UGameplayEffect>() : nullptr)
		{
			for (const FGameplayEffectExecutionDefinition& Execution : Effect->Executions)
			{
				const UObject* const CalculationCDO = Execution.CalculationClass ? Execution.CalculationClass->GetDefaultObject() : nullptr;
				if (const UknightDamageExecution* const DamageExecution = Cast<UknightDamageExecution>(CalculationCDO))
				{
					return DamageExecution->IsAttackerInGuardArc(Target, Attacker, &Hit);
				}
			}
		}
		return GetDefault<UknightDamageExecution>()->IsAttackerInGuardArc(Target, Attacker, &Hit);
	}

	/**
	 * (2026-10-19 사용자 결정) 락온 부위 보정. 공격자가 이 대상의 부위에 락온 중이고, 맞은 지점이 그 락온 지점에서
	 * 락온 컴포넌트의 LockOnPartAssistRadius 안이면 그 부위에 맞은 것으로 친다(히트의 뼈·컴포넌트를 락온 지점 것으로 바꿈).
	 * 큰 몬스터를 휘두르면 칼이 노린 부위보다 앞발·몸통에 먼저 닿아 원하지 않는 부위가 맞던 문제. 락온 컴포넌트가 없는 공격자(몬스터)는 그대로.
	 * 바꾼 뼈로 부위를 다시 판정해 락온 부위가 나오지 않으면(락온 지점이 부위 뼈가 아님) 원래 히트를 쓴다.
	 */
	FHitResult ApplyLockOnPartAssist(const AActor* Attacker, const FHitResult& Hit)
	{
		AActor* const Target = Hit.GetActor();
		const UknightTargetLockComponent* const TargetLock = Attacker ? Attacker->FindComponentByClass<UknightTargetLockComponent>() : nullptr;
		if (!Target || !TargetLock || TargetLock->GetLockedTarget() != Target || TargetLock->GetLockOnPartAssistRadius() <= 0.f
			|| !Target->GetClass()->ImplementsInterface(UCombatInterface::StaticClass()))
		{
			return Hit;
		}
		const FGameplayTag LockedPart = TargetLock->GetLockedPartTag();
		FVector PartLocation;
		if (!LockedPart.IsValid() || !TargetLock->GetLockedTargetLocation(PartLocation)
			|| FVector::DistSquared(Hit.ImpactPoint, PartLocation) > FMath::Square(TargetLock->GetLockOnPartAssistRadius()))
		{
			return Hit;
		}
		const FGameplayTag HitPart = ICombatInterface::Execute_ResolveHitPartBP(Target, Hit);
		if (HitPart == LockedPart)
		{
			return Hit;
		}
		FHitResult Assisted = Hit;
		Assisted.BoneName = TargetLock->GetLockedSocket();
		if (UPrimitiveComponent* const LockedPrimitive = Cast<UPrimitiveComponent>(TargetLock->GetLockedComponent()))
		{
			Assisted.Component = LockedPrimitive;
		}
		if (ICombatInterface::Execute_ResolveHitPartBP(Target, Assisted) != LockedPart)
		{
			return Hit;
		}
		UE_LOG(LogknightCombat, Log, TEXT("%s: lock-on part assist on %s: %s -> %s (%.0f cm from the locked point)."), *GetNameSafe(Attacker),
			*GetNameSafe(Target), HitPart.IsValid() ? *HitPart.ToString() : TEXT("no part"), *LockedPart.ToString(),
			FVector::Dist(Hit.ImpactPoint, PartLocation));
		return Assisted;
	}

	/**
	 * 공격 워프 계산(ComputeAttackWarp·ComputeAttackWarpToActor 공용). TargetLocation 쪽으로 수평 이동하되,
	 * bSubtractRadius면 대상 반지름(ITargetableInterface)만큼 표면까지만, 거기서 StopDistance 앞에서 멈춘다. 이동량은 MaxDistance까지.
	 */
	bool ComputeWarpToPoint(const AActor* Attacker, AActor* TargetActor, const FVector& TargetLocation, bool bSubtractRadius,
		float MaxDistance, float StopDistance, FVector& OutLocation, FRotator& OutRotation, float& OutTravel)
	{
		const FVector AttackerLocation = Attacker->GetActorLocation();
		FVector ToTarget = TargetLocation - AttackerLocation;
		ToTarget.Z = 0.f;
		const float Distance = ToTarget.Size();
		if (Distance < KINDA_SMALL_NUMBER)
		{
			return false;
		}
		const FVector Direction = ToTarget / Distance;
		const float Radius = bSubtractRadius && TargetActor && TargetActor->GetClass()->ImplementsInterface(UTargetableInterface::StaticClass())
			? ITargetableInterface::Execute_QueryTargetRadius(TargetActor) : 0.f;
		OutTravel = FMath::Clamp(Distance - Radius - StopDistance, 0.f, MaxDistance);
		OutLocation = AttackerLocation + Direction * OutTravel;
		OutRotation = Direction.Rotation();
		// (2026-10-19) Verbose: 락온 중 매 프레임 불려서(락온 컴포넌트의 '멀다' 판정) 로그가 한 판에 10만 줄 넘게 쌓이고 랙을 만들었다.
		// (2026-10-20) VeryVerbose: 밸런스 측정 때 LogknightCombat을 Verbose로 켜도 이 줄은 나오지 않게(타격 로그만 남게).
		UE_LOG(LogknightCombat, VeryVerbose, TEXT("Attack warp -> %s: distance %.0f, radius %.0f, travel %.0f"),
			*GetNameSafe(TargetActor), Distance, Radius, OutTravel);
		return true;
	}
}

EknightHitOutcome UknightCombatLibrary::ApplyMeleeHit(AActor* Attacker, const FHitResult& Hit, const FknightMeleeHitParams& Params)
{
	return ApplyMeleeHit(Attacker, Hit, Params, nullptr);
}

EknightHitOutcome UknightCombatLibrary::ApplyMeleeHit(AActor* Attacker, const FHitResult& InHit, const FknightMeleeHitParams& Params,
	const UGameplayAbility* SourceAbility)
{
	// (2026-10-19) 플레이어가 부위에 락온 중이면 그 부위 근처 타격을 락온 부위로 보정한다. 아래는 모두 보정된 히트를 쓴다.
	const FHitResult Hit = ApplyLockOnPartAssist(Attacker, InHit);
	AActor* const Target = Hit.GetActor();
	AknightCharacter* const LocalPlayer = Cast<AknightCharacter>(Attacker);
	UknightGroggyFeedbackComponent* const Feedback = LocalPlayer
		? LocalPlayer->FindComponentByClass<UknightGroggyFeedbackComponent>() : nullptr;
	UAbilitySystemComponent* const SourceASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Attacker);
	UAbilitySystemComponent* const TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Target);
	if (!SourceASC || !TargetASC || !Target || !Target->GetClass()->ImplementsInterface(UCombatInterface::StaticClass())
		|| !ICombatInterface::Execute_CanReceiveHitBP(Target, Attacker))
	{
		if (Feedback) Feedback->ResolvePredictedMeleeContact(Target, false, SourceAbility);
		return EknightHitOutcome::Ignored;
	}

	FGameplayCueParameters CueParams;
	CueParams.Location = Hit.ImpactPoint;
	CueParams.Normal = Hit.ImpactNormal;
	CueParams.Instigator = Attacker;
	CueParams.EffectCauser = Attacker;
	CueParams.PhysicalMaterial = Hit.PhysMaterial;

	if (TargetASC->HasMatchingGameplayTag(knightGameplayTags::State_Invulnerable))
	{
		if (Feedback) Feedback->ResolvePredictedMeleeContact(Target, false, SourceAbility);
		return EknightHitOutcome::Evaded;
	}

	// 패링할 수 없는 공격은 패링 판정을 건너뛴다. 패링 구간에도 State.Guarding이 붙어 있어 아래 가드로 처리된다.
	// 패링(퍼펙트 가드)도 일반 가드처럼 가드 범위(정면) 안에서 들어온 공격만 막는다. 범위 밖이면 아래에서 그냥 맞는다. (2026-10-08)
	const bool bInParryWindow = TargetASC->HasMatchingGameplayTag(knightGameplayTags::State_Parrying);
	const bool bInGuardArc = bInParryWindow && IsAttackerInGuardArc(Params.DamageEffect, Target, Attacker, Hit);
	if (bInParryWindow && !(Params.bCanBeParried && bInGuardArc))
	{
		// 판정 시간 중인데 퍼펙트 가드가 안 된 이유. 플레이 테스트에서 '판정이 이상하다'를 가려내려고 남긴다.
		UE_LOG(LogknightCombat, Log, TEXT("%s: perfect guard missed against %s (%s)."), *GetNameSafe(Target), *GetNameSafe(Attacker),
			!Params.bCanBeParried ? TEXT("unparryable attack") : TEXT("outside guard arc"));
	}
	if (Params.bCanBeParried && bInGuardArc)
	{
		FGameplayEventData Payload;
		Payload.EventTag = knightGameplayTags::Event_Parried;
		Payload.Instigator = Target;
		Payload.Target = Attacker;
		// (2026-10-27) 팀 결정으로 패링은 그로기를 깎지 않는다(DoesParryBuildPoise) → 몬스터의 자기 감소도 막는다.
		if (!Params.bParryBuildsPoise || !DoesParryBuildPoise())
		{
			Payload.InstigatorTags.AddTag(knightGameplayTags::Event_Parried_NoPoise);
		}
		SourceASC->HandleGameplayEvent(Payload.EventTag, &Payload);
		// The defender learns about its own parry too (the guard ability opens the parry counter window).
		static const FGameplayTag ParrySucceededTag = FGameplayTag::RequestGameplayTag(TEXT("Event.ParrySucceeded"), false);
		if (ParrySucceededTag.IsValid())
		{
			FGameplayEventData DefenderPayload;
			DefenderPayload.EventTag = ParrySucceededTag;
			DefenderPayload.Instigator = Attacker;
			DefenderPayload.Target = Target;
			TargetASC->HandleGameplayEvent(DefenderPayload.EventTag, &DefenderPayload);
		}
		TargetASC->ExecuteGameplayCue(knightGameplayTags::GameplayCue_Parry, CueParams);
		if (Feedback) Feedback->ResolvePredictedMeleeContact(Target, false, SourceAbility);
		ApplyHitStopPair(Attacker, Target, Params.ParryHitStopDuration);
		UE_LOG(LogknightCombat, Log, TEXT("%s parried %s."), *GetNameSafe(Target), *GetNameSafe(Attacker));
		return EknightHitOutcome::Parried;
	}

	if (!Params.DamageEffect)
	{
		if (Feedback) Feedback->ResolvePredictedMeleeContact(Target, false, SourceAbility);
		UE_LOG(LogknightCombat, Error, TEXT("%s hit %s with no DamageEffect set."), *GetNameSafe(Attacker), *GetNameSafe(Target));
		return EknightHitOutcome::Ignored;
	}

	FGameplayEffectContextHandle Context = SourceASC->MakeEffectContext();
	Context.AddHitResult(Hit, true);
	// (2026-10-21) 어떤 공격의 피해인지 남긴다(피해 실행 로그의 [어빌리티], 밸런스 측정용).
	if (SourceAbility)
	{
		Context.SetAbility(SourceAbility);
	}
	const FGameplayEffectSpecHandle SpecHandle = SourceASC->MakeOutgoingSpec(Params.DamageEffect, 1.f, Context);
	if (!SpecHandle.IsValid())
	{
		if (Feedback) Feedback->ResolvePredictedMeleeContact(Target, false, SourceAbility);
		return EknightHitOutcome::Ignored;
	}
	SpecHandle.Data->SetSetByCallerMagnitude(knightGameplayTags::SetByCaller_Damage, Params.BaseDamage);
	SpecHandle.Data->SetSetByCallerMagnitude(knightGameplayTags::SetByCaller_MotionValue, Params.MotionValue);
	SpecHandle.Data->SetSetByCallerMagnitude(knightGameplayTags::SetByCaller_PoiseDamage, Params.PoiseDamage);

	// 적용 전에 읽어 둔다. 타격으로 타깃이 죽으면서 태그가 바뀔 수 있다.
	// (2026-10-12) 가드 중이어도 가드 범위 밖(등 뒤 등)에서 들어온 공격은 아래 일반 피격으로 처리한다.
	// 피해 실행도 같은 범위로 가드 감소를 정하므로, 가드 이펙트·히트 스톱과 실제 피해가 어긋나지 않는다.
	const bool bGuarding = TargetASC->HasMatchingGameplayTag(knightGameplayTags::State_Guarding)
		&& IsAttackerInGuardArc(Params.DamageEffect, Target, Attacker, Hit);
	const UknightCoreAttributeSet* const TargetCore = TargetASC->GetSet<UknightCoreAttributeSet>();
	const float PoiseBeforeHit = TargetCore ? TargetCore->GetPoise() : 0.f;
	const bool bWasKnockedDown = TargetASC->HasMatchingGameplayTag(knightGameplayTags::State_HitReact_Knockdown);
	// End the pre-hit local slow before the effect can trigger poise-break hit stop.
	if (Feedback) Feedback->ResolvePredictedMeleeContact(Target, true, SourceAbility);
	SourceASC->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data, TargetASC);

	if (bGuarding)
	{
		if (Feedback) Feedback->CompletePredictedMeleeHit(Target, false);
		// 가드 범위 안에서 막은 타격이다. 어떻게 반응할지는 가드 어빌리티가 Event.Guarded를 받아 정한다.
		FGameplayEventData Payload;
		Payload.EventTag = knightGameplayTags::Event_Guarded;
		Payload.Instigator = Attacker;
		Payload.Target = Target;
		Payload.ContextHandle = Context;
		TargetASC->HandleGameplayEvent(Payload.EventTag, &Payload);
		TargetASC->ExecuteGameplayCue(knightGameplayTags::GameplayCue_Guard, CueParams);
		ApplyHitStopPair(Attacker, Target, Params.HitStopDuration * 0.5f);
		// (2026-10-20) 큰 공격은 막혀도 때린 쪽 화면이 조금 흔들린다.
		if (const UknightAbilitySystemComponent* const KnightSourceASC = Cast<UknightAbilitySystemComponent>(SourceASC))
		{
			KnightSourceASC->PlayAttackImpactShake(Params.PoiseDamage, true);
			KnightSourceASC->PlayAttackImpactEffects(Hit, true);
		}
		return EknightHitOutcome::Guarded;
	}

	// Preserve the original post-effect verdict. Prediction never forces the damage or reaction outcome.
	const bool bNewKnockdown = TargetCore && PoiseBeforeHit > 0.f && TargetCore->GetPoise() <= 0.f
		&& !bWasKnockedDown && TargetASC->HasMatchingGameplayTag(knightGameplayTags::State_HitReact_Knockdown)
		&& !TargetASC->HasMatchingGameplayTag(knightGameplayTags::State_Dead);
	if (Feedback) Feedback->CompletePredictedMeleeHit(Target, bNewKnockdown);

	const float PartMultiplier = ICombatInterface::Execute_GetPartDamageMultiplierBP(
		Target, ICombatInterface::Execute_ResolveHitPartBP(Target, Hit));
	const FGameplayTag CueTag = PartMultiplier >= Params.WeakpointMultiplierThreshold
		? knightGameplayTags::GameplayCue_Hit_Weakpoint.GetTag()
		: knightGameplayTags::GameplayCue_Hit_Melee.GetTag();
	CueParams.RawMagnitude = PartMultiplier;
	// (2026-10-21) 나이트의 타격은 ShouldReplaceSharedHitCue가 켜져 있으면(전용 이펙트·소리가 지정됨) 공용 큐 대신
	// 나이트 전용 타격(PlayAttackImpactEffects)만 쓴다. 일반·약점 큐 모두 해당. 비어 있으면 공용 큐 그대로.
	const UknightAbilitySystemComponent* const KnightAttackerASC = Cast<UknightAbilitySystemComponent>(SourceASC);
	const bool bWeakpointHit = CueTag == knightGameplayTags::GameplayCue_Hit_Weakpoint.GetTag();
	// (2026-10-21) 약점 큐(번개)도 판타지 느낌이라 같이 대신한다. 약점은 나이트 타격을 더 크게(AttackImpactWeakpointScale).
	const bool bKnightReplacesCue = KnightAttackerASC && KnightAttackerASC->ShouldReplaceSharedHitCue();
	if (!bKnightReplacesCue)
	{
		TargetASC->ExecuteGameplayCue(CueTag, CueParams);
	}
	ApplyHitStopPair(Attacker, Target, Params.HitStopDuration);
	// (2026-10-20) 공격 무게(그로기 피해)만큼 때린 쪽 화면을 더 흔든다. 양손·차지·내려찍기가 크게 흔들린다.
	if (const UknightAbilitySystemComponent* const KnightSourceASC = Cast<UknightAbilitySystemComponent>(SourceASC))
	{
		KnightSourceASC->PlayAttackImpactShake(Params.PoiseDamage, false);
		// (2026-10-21) 나이트 전용 타격 이펙트·소리(몬스터 ASC는 비어 있어 아무것도 안 한다).
		KnightSourceASC->PlayAttackImpactEffects(Hit, false, bWeakpointHit);
	}
	// 전투는 소리가 난다. 아직 전투를 알아채지 못한 몬스터도 이 소리를 들을 수 있다.
	UAISense_Hearing::ReportNoiseEvent(Attacker, Hit.ImpactPoint, 1.f, Attacker, 0.f, TEXT("Hit"));
	return EknightHitOutcome::Hit;
}

int32 UknightCombatLibrary::ApplyAreaMeleeHits(AActor* Attacker, const FVector& Center, const FCollisionShape& Shape, const FknightMeleeHitParams& Params,
	TSet<TWeakObjectPtr<AActor>>& HitActors, TFunctionRef<bool(const AActor&)> Filter, TFunctionRef<void(AActor&, EknightHitOutcome)> OnHit,
	const FVector& FixedNormal)
{
	UWorld* const World = Attacker ? Attacker->GetWorld() : nullptr;
	if (!World)
	{
		return 0;
	}

	FCollisionObjectQueryParams ObjectParams;
	ObjectParams.AddObjectTypesToQuery(ECC_Pawn);
	ObjectParams.AddObjectTypesToQuery(ECC_PhysicsBody);
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(knightAreaMeleeHits), false, Attacker);
	TArray<FOverlapResult> Overlaps;
	World->OverlapMultiByObjectType(Overlaps, Center, FQuat::Identity, ObjectParams, Shape, QueryParams);

	int32 NewHits = 0;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* const Target = Overlap.GetActor();
		UPrimitiveComponent* const Component = Overlap.GetComponent();
		if (!Target || !Component || HitActors.Contains(Target)
			|| !Target->GetClass()->ImplementsInterface(UCombatInterface::StaticClass())
			|| !ICombatInterface::Execute_CanReceiveHitBP(Target, Attacker)
			|| FGenericTeamId::GetAttitude(Attacker, Target) != ETeamAttitude::Hostile
			|| !Filter(*Target))
		{
			continue;
		}
		HitActors.Add(Target);
		++NewHits;

		// 피해 컨텍스트, 큐, 부위 조회는 히트 결과를 필요로 하므로 가장 가까운 표면 지점으로 하나 만든다.
		FVector ImpactPoint = Component->GetComponentLocation();
		FVector ClosestPoint;
		if (Component->GetClosestPointOnCollision(Center, ClosestPoint) >= 0.f)
		{
			ImpactPoint = ClosestPoint;
		}
		FHitResult Hit(Target, Component, ImpactPoint, FixedNormal.IsZero() ? (Center - ImpactPoint).GetSafeNormal() : FixedNormal);
		Hit.bBlockingHit = true;
		Hit.TraceStart = Center;
		Hit.TraceEnd = ImpactPoint;
		const EknightHitOutcome Outcome = ApplyMeleeHit(Attacker, Hit, Params);
		UE_LOG(LogknightAI, Verbose, TEXT("%s: area hit %s (outcome %d)."), *GetNameSafe(Attacker), *GetNameSafe(Target), static_cast<int32>(Outcome));
		OnHit(*Target, Outcome);
	}
	return NewHits;
}

void UknightCombatLibrary::SelectBestHitPerActor(TArrayView<const FHitResult> Hits, TArray<FHitResult>& OutHits)
{
	OutHits.Reset();
	TArray<bool, TInlineAllocator<8>> HasPart;
	for (const FHitResult& Hit : Hits)
	{
		AActor* const Actor = Hit.GetActor();
		if (!Actor)
		{
			continue;
		}
		const int32 Existing = OutHits.IndexOfByPredicate([Actor](const FHitResult& Kept) { return Kept.GetActor() == Actor; });
		if (Existing != INDEX_NONE && HasPart[Existing])
		{
			continue;
		}
		const bool bResolvesPart = Actor->GetClass()->ImplementsInterface(UCombatInterface::StaticClass())
			&& ICombatInterface::Execute_ResolveHitPartBP(Actor, Hit).IsValid();
		if (Existing == INDEX_NONE)
		{
			OutHits.Add(Hit);
			HasPart.Add(bResolvesPart);
		}
		else if (bResolvesPart)
		{
			OutHits[Existing] = Hit;
			HasPart[Existing] = true;
		}
	}
}

bool UknightCombatLibrary::ComputeAttackWarp(AActor* Attacker, float MaxDistance, float StopDistance, bool bUseSoftTarget,
	float SoftTargetRange, float SoftTargetAngle, FVector& OutLocation, FRotator& OutRotation, float& OutTravel)
{
	const UknightTargetLockComponent* const TargetLock = Attacker ? Attacker->FindComponentByClass<UknightTargetLockComponent>() : nullptr;
	if (!TargetLock)
	{
		return false;
	}

	FVector TargetLocation = FVector::ZeroVector;
	if (!TargetLock->GetLockedTargetLocation(TargetLocation))
	{
		// 락온 상태가 아닐 때는 주변 적(소프트 타깃)이나 이동 키 방향으로 공격이 휘지 않고, 화면 중앙 방향으로 곧장 공격한다.
		return false;
	}
	AActor* const TargetActor = TargetLock->GetLockedTarget();
	const FGameplayTag PartTag = TargetLock->GetLockedPartTag();
	// 부위 지점은 몸 표면 근처라 반지름을 빼지 않는다. 단일 지점(몬스터 중심)은 반지름만큼 표면까지.
	return ComputeWarpToPoint(Attacker, TargetActor, TargetLocation, !PartTag.IsValid(), MaxDistance, StopDistance,
		OutLocation, OutRotation, OutTravel);
}

bool UknightCombatLibrary::ComputeAttackWarpToActor(AActor* Attacker, AActor* Target, float MaxDistance, float StopDistance,
	FVector& OutLocation, FRotator& OutRotation, float& OutTravel)
{
	if (!Attacker || !Target)
	{
		return false;
	}
	// 대상 중심에서 반지름만큼 표면까지. 락온한 몬스터(부위 없음)와 같은 계산이다.
	return ComputeWarpToPoint(Attacker, Target, Target->GetActorLocation(), true, MaxDistance, StopDistance, OutLocation, OutRotation, OutTravel);
}

bool UknightCombatLibrary::SetAttackWarpTargetToActor(AActor* Attacker, FName WarpTargetName, AActor* Target, float MaxDistance,
	float StopDistance, FVector& OutLocation, FRotator& OutRotation, float& OutTravel)
{
	UMotionWarpingComponent* const MotionWarping = Attacker ? Attacker->FindComponentByClass<UMotionWarpingComponent>() : nullptr;
	if (!MotionWarping)
	{
		return false;
	}
	if (!ComputeAttackWarpToActor(Attacker, Target, MaxDistance, StopDistance, OutLocation, OutRotation, OutTravel))
	{
		MotionWarping->RemoveWarpTarget(WarpTargetName);
		return false;
	}
	MotionWarping->AddOrUpdateWarpTargetFromLocationAndRotation(WarpTargetName, OutLocation, OutRotation);
	return true;
}

bool UknightCombatLibrary::SetAttackWarpTarget(AActor* Attacker, FName WarpTargetName, float MaxDistance, float StopDistance,
	bool bUseSoftTarget, float SoftTargetRange, float SoftTargetAngle)
{
	UMotionWarpingComponent* const MotionWarping = Attacker ? Attacker->FindComponentByClass<UMotionWarpingComponent>() : nullptr;
	if (!MotionWarping)
	{
		return false;
	}
	// 공격 시작 순간 플레이어와 가장 가까운 부위로 한 번 더 즉시 갱신해, 대시 직후 공격해도 눈앞의 부위로 정확히 워프·보정되게 한다.
	if (UknightTargetLockComponent* const TargetLock = Attacker->FindComponentByClass<UknightTargetLockComponent>())
	{
		TargetLock->RefreshAutoNearestPart(true);
	}
	FVector Location;
	FRotator Rotation;
	float Travel = 0.f;
	if (!ComputeAttackWarp(Attacker, MaxDistance, StopDistance, bUseSoftTarget, SoftTargetRange, SoftTargetAngle, Location, Rotation, Travel))
	{
		MotionWarping->RemoveWarpTarget(WarpTargetName);
		return false;
	}
	MotionWarping->AddOrUpdateWarpTargetFromLocationAndRotation(WarpTargetName, Location, Rotation);
	return true;
}

float UknightCombatLibrary::GetWarpWindowEnd(const UAnimMontage* Montage)
{
	float End = -1.f;
	if (Montage)
	{
		for (const FAnimNotifyEvent& Event : Montage->Notifies)
		{
			if (Cast<UAnimNotifyState_MotionWarping>(Event.NotifyStateClass) && (End < 0.f || Event.GetEndTriggerTime() < End))
			{
				End = Event.GetEndTriggerTime();
			}
		}
	}
	return End;
}

void UknightCombatLibrary::StartAttackLunge(UGameplayAbility* Ability, const UAnimMontage* Montage, float PlayRate, const FVector& Location,
	float Travel, float MaxLungeTime, float MinTravel)
{
	UWorld* const World = Ability ? Ability->GetWorld() : nullptr;
	const float WindowEnd = GetWarpWindowEnd(Montage);
	if (!World || Travel < MinTravel || WindowEnd <= 0.f)
	{
		return;
	}
	const float WindowSeconds = WindowEnd / FMath::Max(PlayRate, 0.01f);
	const float Duration = FMath::Clamp(WindowSeconds, 0.05f, MaxLungeTime);
	const float Delay = WindowSeconds - Duration;
	ACharacter* const Character = Cast<ACharacter>(Ability->GetAvatarActorFromActorInfo());
	if (!Character || !Character->GetCharacterMovement())
	{
		return;
	}

	// 루트 모션 힘 대신 매 프레임 스윕 이동으로 Location까지 미끄러진다. 벽·몬스터에 막히면 그 자리에서 멈춘다.
	struct FLunge
	{
		TWeakObjectPtr<UGameplayAbility> WeakAbility;
		TWeakObjectPtr<ACharacter> WeakCharacter;
		const UAnimMontage* LungeMontage = nullptr;
		FVector TargetLocation = FVector::ZeroVector;
		FVector StartLocation = FVector::ZeroVector;
		float LungeDuration = 0.f;
		double StartTime = -1.0;
		FTimerHandle Timer;
	};
	TSharedRef<FLunge> Lunge = MakeShared<FLunge>();
	Lunge->WeakAbility = Ability;
	Lunge->WeakCharacter = Character;
	Lunge->LungeMontage = Montage;
	Lunge->TargetLocation = Location;
	Lunge->LungeDuration = Duration;
	const double BeginTime = World->GetTimeSeconds() + FMath::Max(0.f, Delay);

	World->GetTimerManager().SetTimer(Lunge->Timer, FTimerDelegate::CreateLambda([Lunge, BeginTime]()
	{
		UGameplayAbility* const A = Lunge->WeakAbility.Get();
		ACharacter* const C = Lunge->WeakCharacter.Get();
		UWorld* const W = C ? C->GetWorld() : nullptr;
		if (!W)
		{
			return;
		}
		const double Now = W->GetTimeSeconds();
		if (!A || !A->IsActive() || A->GetCurrentMontage() != Lunge->LungeMontage)
		{
			W->GetTimerManager().ClearTimer(Lunge->Timer);
			return;
		}
		if (Now < BeginTime)
		{
			return;
		}
		if (Lunge->StartTime < 0.0)
		{
			Lunge->StartTime = Now;
			Lunge->StartLocation = C->GetActorLocation();
		}
		const float Alpha = FMath::Clamp(static_cast<float>((Now - Lunge->StartTime) / Lunge->LungeDuration), 0.f, 1.f);
		// 빠르게 출발해서 타격 직전에 감속한다.
		const float Eased = 1.f - FMath::Square(1.f - Alpha);
		FVector Desired = FMath::Lerp(Lunge->StartLocation, Lunge->TargetLocation, Eased);
		FVector Delta = Desired - C->GetActorLocation();
		Delta.Z = 0.f;
		if (!Delta.IsNearlyZero())
		{
			FHitResult Hit;
			C->GetCharacterMovement()->SafeMoveUpdatedComponent(Delta, C->GetActorQuat(), true, Hit);
			if (Hit.IsValidBlockingHit())
			{
				// 막힌 면을 따라 남은 이동만큼 미끄러진다.
				FVector Slide = FVector::VectorPlaneProject(Delta * (1.f - Hit.Time), Hit.Normal);
				Slide.Z = 0.f;
				if (!Slide.IsNearlyZero())
				{
					FHitResult SlideHit;
					C->GetCharacterMovement()->SafeMoveUpdatedComponent(Slide, C->GetActorQuat(), true, SlideHit);
				}
			}
		}
		if (Alpha >= 1.f)
		{
			W->GetTimerManager().ClearTimer(Lunge->Timer);
		}
	}), 1.f / 120.f, true);
}

bool UknightCombatLibrary::SetWarpTargetToLockedTarget(AActor* Attacker, FName WarpTargetName, float MaxDistance)
{
	return SetAttackWarpTarget(Attacker, WarpTargetName, MaxDistance, 0.f, false, 0.f, 0.f);
}

void UknightCombatLibrary::ApplyHitStopPair(AActor* Attacker, AActor* Target, float Duration)
{
	if (Duration <= 0.f)
	{
		return;
	}
	for (AActor* const Actor : { Attacker, Target })
	{
		if (UknightHitStopComponent* const HitStop = Actor ? Actor->FindComponentByClass<UknightHitStopComponent>() : nullptr)
		{
			HitStop->ApplyHitStop(Duration);
		}
	}
}
