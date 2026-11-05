#include "Combat/knightHitReactionComponent.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "AbilitySystem/Attributes/knightCoreAttributeSet.h"
#include "GameplayEffect.h"
#include "System/knightGameplayTags.h"
#include "knight.h"

FknightAnyPoiseBroken UknightHitReactionComponent::OnAnyPoiseBroken;

UknightHitReactionComponent::UknightHitReactionComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UknightHitReactionComponent::BeginPlay()
{
	Super::BeginPlay();

	UAbilitySystemComponent* const ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner());
	const UknightCoreAttributeSet* const CoreSet = ASC ? ASC->GetSet<UknightCoreAttributeSet>() : nullptr;
	if (!CoreSet)
	{
		UE_LOG(LogknightCombat, Warning, TEXT("%s: hit reactions need an ASC with UknightCoreAttributeSet."), *GetNameSafe(GetOwner()));
		return;
	}
	BoundASC = ASC;
	CoreSet->OnDamaged.AddUObject(this, &ThisClass::HandleDamaged);
	CoreSet->OnPoiseBroken.AddUObject(this, &ThisClass::HandlePoiseBroken);
	AppliedHandle = ASC->OnGameplayEffectAppliedDelegateToSelf.AddUObject(this, &ThisClass::HandleEffectExecuted);
	PeriodicHandle = ASC->OnPeriodicGameplayEffectExecuteDelegateOnSelf.AddUObject(this, &ThisClass::HandleEffectExecuted);
}

void UknightHitReactionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UAbilitySystemComponent* const ASC = BoundASC.Get())
	{
		ASC->OnGameplayEffectAppliedDelegateToSelf.Remove(AppliedHandle);
		ASC->OnPeriodicGameplayEffectExecuteDelegateOnSelf.Remove(PeriodicHandle);
		if (const UknightCoreAttributeSet* const CoreSet = ASC->GetSet<UknightCoreAttributeSet>())
		{
			CoreSet->OnDamaged.RemoveAll(this);
			CoreSet->OnPoiseBroken.RemoveAll(this);
		}
	}
	BoundASC.Reset();
	Super::EndPlay(EndPlayReason);
}

void UknightHitReactionComponent::HandleDamaged(AActor* DamageInstigator, AActor* DamageCauser, const FGameplayEffectSpec* EffectSpec,
	float Magnitude, float OldValue, float NewValue)
{
	// 지속 효과(화상 같은 주기 피해)는 체력만 깎고 움찔하지 않는다. 주기마다 피격 반응이 나가면 조작이 끊긴다.
	if (EffectSpec && EffectSpec->Def && EffectSpec->Def->DurationPolicy != EGameplayEffectDurationType::Instant)
	{
		return;
	}
	RecordPending(DamageInstigator, EffectSpec);
	PendingDamage += Magnitude;
}

void UknightHitReactionComponent::HandlePoiseBroken(AActor* DamageInstigator, AActor* DamageCauser, const FGameplayEffectSpec* EffectSpec,
	float Magnitude, float OldValue, float NewValue)
{
	RecordPending(DamageInstigator, EffectSpec);
	PendingPoiseDamage += Magnitude;
	bPendingPoiseBroken = true;
}

void UknightHitReactionComponent::RecordPending(AActor* DamageInstigator, const FGameplayEffectSpec* EffectSpec)
{
	if (!bHasPending)
	{
		bHasPending = true;
		PendingInstigator = DamageInstigator;
		PendingContext = EffectSpec ? EffectSpec->GetEffectContext() : FGameplayEffectContextHandle();
	}
}

void UknightHitReactionComponent::HandleEffectExecuted(UAbilitySystemComponent* Source, const FGameplayEffectSpec& Spec, FActiveGameplayEffectHandle Handle)
{
	FlushPendingReaction();
}

void UknightHitReactionComponent::FlushPendingReaction()
{
	if (!bHasPending)
	{
		return;
	}
	const bool bPoiseBroken = bPendingPoiseBroken;
	const float Damage = PendingDamage;
	const float PoiseDamage = PendingPoiseDamage;
	AActor* const DamageInstigator = PendingInstigator.Get();
	const FGameplayEffectContextHandle Context = PendingContext;

	bHasPending = false;
	bPendingPoiseBroken = false;
	PendingDamage = 0.f;
	PendingPoiseDamage = 0.f;
	PendingInstigator.Reset();
	PendingContext = FGameplayEffectContextHandle();

	UAbilitySystemComponent* const ASC = BoundASC.Get();
	if (!ASC || ASC->HasMatchingGameplayTag(knightGameplayTags::State_Dead))
	{
		return;
	}
	if (!bPoiseBroken && Damage <= 0.f)
	{
		return;
	}

	FGameplayEventData Payload;
	Payload.EventTag = bPoiseBroken ? knightGameplayTags::Event_PoiseBroken.GetTag() : knightGameplayTags::Event_Damaged.GetTag();
	Payload.EventMagnitude = bPoiseBroken ? PoiseDamage : Damage;
	Payload.Instigator = DamageInstigator;
	Payload.Target = GetOwner();
	Payload.ContextHandle = Context;
	ASC->HandleGameplayEvent(Payload.EventTag, &Payload);

	// 그로기 연출(큰 히트스탑·이펙트)을 맡는 쪽에 알린다. 반응은 위 이벤트가 그대로 정한다.
	if (bPoiseBroken)
	{
		OnAnyPoiseBroken.Broadcast(GetOwner(), DamageInstigator, Context);
	}
}
