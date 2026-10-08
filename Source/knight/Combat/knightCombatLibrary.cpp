#include "Combat/knightCombatLibrary.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Abilities/GameplayAbility.h"
#include "Animation/AnimMontage.h"
#include "AnimNotifyState_MotionWarping.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TimerManager.h"
#include "Combat/knightHitStopComponent.h"
#include "Combat/knightTargetLockComponent.h"
#include "GameplayEffect.h"
#include "GameFramework/Pawn.h"
#include "Interfaces/CombatInterface.h"
#include "Interfaces/TargetableInterface.h"
#include "MotionWarpingComponent.h"
#include "Perception/AISense_Hearing.h"
#include "System/knightGameplayTags.h"
#include "AbilitySystem/Executions/knightDamageExecution.h"
#include "knight.h"

namespace
{
	// 공격의 피해 이펙트가 쓰는 피해 실행(UknightDamageExecution)의 가드 범위로 판정한다. 퍼펙트 가드가 일반 가드와 같은 각도를 쓰게 하려는 것.
	// 피해 이펙트가 없거나 다른 실행을 쓰면 기본 피해 실행의 각도를 쓴다.
	bool IsAttackerInGuardArc(const TSubclassOf<UGameplayEffect>& DamageEffect, const AActor* Target, const AActor* Attacker)
	{
		if (const UGameplayEffect* const Effect = DamageEffect ? DamageEffect->GetDefaultObject<UGameplayEffect>() : nullptr)
		{
			for (const FGameplayEffectExecutionDefinition& Execution : Effect->Executions)
			{
				const UObject* const CalculationCDO = Execution.CalculationClass ? Execution.CalculationClass->GetDefaultObject() : nullptr;
				if (const UknightDamageExecution* const DamageExecution = Cast<UknightDamageExecution>(CalculationCDO))
				{
					return DamageExecution->IsAttackerInGuardArc(Target, Attacker);
				}
			}
		}
		return GetDefault<UknightDamageExecution>()->IsAttackerInGuardArc(Target, Attacker);
	}
}

EknightHitOutcome UknightCombatLibrary::ApplyMeleeHit(AActor* Attacker, const FHitResult& Hit, const FknightMeleeHitParams& Params)
{
	AActor* const Target = Hit.GetActor();
	UAbilitySystemComponent* const SourceASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Attacker);
	UAbilitySystemComponent* const TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Target);
	if (!SourceASC || !TargetASC || !Target || !Target->GetClass()->ImplementsInterface(UCombatInterface::StaticClass())
		|| !ICombatInterface::Execute_CanReceiveHitBP(Target, Attacker))
	{
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
		return EknightHitOutcome::Evaded;
	}

	// 패링할 수 없는 공격은 패링 판정을 건너뛴다. 패링 구간에도 State.Guarding이 붙어 있어 아래 가드로 처리된다.
	// 패링(퍼펙트 가드)도 일반 가드처럼 가드 범위(정면) 안에서 들어온 공격만 막는다. 범위 밖이면 아래에서 그냥 맞는다. (2026-10-08)
	const bool bInParryWindow = TargetASC->HasMatchingGameplayTag(knightGameplayTags::State_Parrying);
	const bool bInGuardArc = bInParryWindow && IsAttackerInGuardArc(Params.DamageEffect, Target, Attacker);
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
		ApplyHitStopPair(Attacker, Target, Params.ParryHitStopDuration);
		UE_LOG(LogknightCombat, Log, TEXT("%s parried %s."), *GetNameSafe(Target), *GetNameSafe(Attacker));
		return EknightHitOutcome::Parried;
	}

	if (!Params.DamageEffect)
	{
		UE_LOG(LogknightCombat, Error, TEXT("%s hit %s with no DamageEffect set."), *GetNameSafe(Attacker), *GetNameSafe(Target));
		return EknightHitOutcome::Ignored;
	}

	FGameplayEffectContextHandle Context = SourceASC->MakeEffectContext();
	Context.AddHitResult(Hit, true);
	const FGameplayEffectSpecHandle SpecHandle = SourceASC->MakeOutgoingSpec(Params.DamageEffect, 1.f, Context);
	if (!SpecHandle.IsValid())
	{
		return EknightHitOutcome::Ignored;
	}
	SpecHandle.Data->SetSetByCallerMagnitude(knightGameplayTags::SetByCaller_Damage, Params.BaseDamage);
	SpecHandle.Data->SetSetByCallerMagnitude(knightGameplayTags::SetByCaller_MotionValue, Params.MotionValue);
	SpecHandle.Data->SetSetByCallerMagnitude(knightGameplayTags::SetByCaller_PoiseDamage, Params.PoiseDamage);

	// 적용 전에 읽어 둔다. 타격으로 타깃이 죽으면서 태그가 바뀔 수 있다.
	const bool bGuarding = TargetASC->HasMatchingGameplayTag(knightGameplayTags::State_Guarding);
	SourceASC->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data, TargetASC);

	if (bGuarding)
	{
		// 타격이 가드 범위 안에서 들어왔는지, 어떻게 반응할지는 가드 어빌리티가 판단한다.
		FGameplayEventData Payload;
		Payload.EventTag = knightGameplayTags::Event_Guarded;
		Payload.Instigator = Attacker;
		Payload.Target = Target;
		Payload.ContextHandle = Context;
		TargetASC->HandleGameplayEvent(Payload.EventTag, &Payload);
		TargetASC->ExecuteGameplayCue(knightGameplayTags::GameplayCue_Guard, CueParams);
		ApplyHitStopPair(Attacker, Target, Params.HitStopDuration * 0.5f);
		return EknightHitOutcome::Guarded;
	}

	const float PartMultiplier = ICombatInterface::Execute_GetPartDamageMultiplierBP(
		Target, ICombatInterface::Execute_ResolveHitPartBP(Target, Hit));
	const FGameplayTag CueTag = PartMultiplier >= Params.WeakpointMultiplierThreshold
		? knightGameplayTags::GameplayCue_Hit_Weakpoint.GetTag()
		: knightGameplayTags::GameplayCue_Hit_Melee.GetTag();
	CueParams.RawMagnitude = PartMultiplier;
	TargetASC->ExecuteGameplayCue(CueTag, CueParams);
	ApplyHitStopPair(Attacker, Target, Params.HitStopDuration);
	// 전투는 소리가 난다. 아직 전투를 알아채지 못한 몬스터도 이 소리를 들을 수 있다.
	UAISense_Hearing::ReportNoiseEvent(Attacker, Hit.ImpactPoint, 1.f, Attacker, 0.f, TEXT("Hit"));
	return EknightHitOutcome::Hit;
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
	AActor* TargetActor = nullptr;
	FGameplayTag PartTag;
	bool bFound = false;
	if (TargetLock->GetLockedTargetLocation(TargetLocation))
	{
		TargetActor = TargetLock->GetLockedTarget();
		PartTag = TargetLock->GetLockedPartTag();
		bFound = true;
	}
	else if (bUseSoftTarget)
	{
		const APawn* const Pawn = Cast<APawn>(Attacker);
		const FVector Input = Pawn ? Pawn->GetLastMovementInputVector() : FVector::ZeroVector;
		const FVector Direction = Input.IsNearlyZero() ? Attacker->GetActorForwardVector() : Input;
		bFound = TargetLock->FindSoftTarget(Direction, SoftTargetRange, SoftTargetAngle, TargetLocation, TargetActor, PartTag);
	}
	if (!bFound)
	{
		return false;
	}

	const FVector AttackerLocation = Attacker->GetActorLocation();
	FVector ToTarget = TargetLocation - AttackerLocation;
	ToTarget.Z = 0.f;
	const float Distance = ToTarget.Size();
	if (Distance < KINDA_SMALL_NUMBER)
	{
		return false;
	}
	const FVector Direction = ToTarget / Distance;
	// 부위 지점은 몸 표면 근처라 반지름을 빼지 않는다. 단일 지점(몬스터 중심)은 반지름만큼 표면까지.
	const float Radius = !PartTag.IsValid() && TargetActor && TargetActor->GetClass()->ImplementsInterface(UTargetableInterface::StaticClass())
		? ITargetableInterface::Execute_QueryTargetRadius(TargetActor) : 0.f;
	OutTravel = FMath::Clamp(Distance - Radius - StopDistance, 0.f, MaxDistance);
	OutLocation = AttackerLocation + Direction * OutTravel;
	OutRotation = Direction.Rotation();
	UE_LOG(LogknightCombat, Log, TEXT("Attack warp -> %s: distance %.0f, radius %.0f, travel %.0f"),
		*GetNameSafe(TargetActor), Distance, Radius, OutTravel);
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
