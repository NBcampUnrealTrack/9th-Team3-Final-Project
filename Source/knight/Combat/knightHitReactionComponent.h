#pragma once

#include "CoreMinimal.h"
#include "ActiveGameplayEffectHandle.h"
#include "Components/ActorComponent.h"
#include "GameplayEffectTypes.h"
#include "knightHitReactionComponent.generated.h"

class UAbilitySystemComponent;
struct FGameplayEffectSpec;

/** (2026-10-14) 어느 액터든 그로기가 터졌다(Victim, 일으킨 Instigator, 그 피해의 효과 문맥). */
DECLARE_MULTICAST_DELEGATE_ThreeParams(FknightAnyPoiseBroken, AActor* /*Victim*/, AActor* /*Instigator*/, const FGameplayEffectContextHandle& /*Context*/);

/**
 * Turns one damaging effect into exactly one reaction event on the owner's ASC.
 *
 * A single hit changes Health and Poise through separate attribute callbacks, so listening to those
 * directly could fire both a flinch and a stagger for one blow. This component collects what the
 * effect did and, once the effect has finished executing, sends:
 *   Event.PoiseBroken (magnitude: poise damage) if Poise broke, otherwise
 *   Event.Damaged     (magnitude: health lost)  if Health dropped.
 * Nothing is sent for a lethal hit; death is handled by the owner.
 *
 * Works on any actor whose ASC has UknightCoreAttributeSet (Knight, monsters, objectives).
 */
UCLASS(ClassGroup=(Knight), meta=(BlueprintSpawnableComponent))
class KNIGHT_API UknightHitReactionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UknightHitReactionComponent();

	/**
	 * (2026-10-14) 어느 액터든 그로기가 터져 Event.PoiseBroken을 보낸 직후 알린다(일반 공격·스킬·패링 그로기 모두 여기를 지난다).
	 * 플레이어의 그로기 연출(UknightGroggyFeedbackComponent)이 듣고, 자기가 일으킨 그로기만 처리한다. 반응 자체는 바꾸지 않는다.
	 */
	static FknightAnyPoiseBroken OnAnyPoiseBroken;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void HandleDamaged(AActor* DamageInstigator, AActor* DamageCauser, const FGameplayEffectSpec* EffectSpec,
		float Magnitude, float OldValue, float NewValue);
	void HandlePoiseBroken(AActor* DamageInstigator, AActor* DamageCauser, const FGameplayEffectSpec* EffectSpec,
		float Magnitude, float OldValue, float NewValue);
	void HandleEffectExecuted(UAbilitySystemComponent* Source, const FGameplayEffectSpec& Spec, FActiveGameplayEffectHandle Handle);

	void RecordPending(AActor* DamageInstigator, const FGameplayEffectSpec* EffectSpec);
	void FlushPendingReaction();

	TWeakObjectPtr<UAbilitySystemComponent> BoundASC;
	FDelegateHandle AppliedHandle;
	FDelegateHandle PeriodicHandle;

	TWeakObjectPtr<AActor> PendingInstigator;
	FGameplayEffectContextHandle PendingContext;
	float PendingDamage = 0.f;
	float PendingPoiseDamage = 0.f;
	bool bPendingPoiseBroken = false;
	bool bHasPending = false;
};
