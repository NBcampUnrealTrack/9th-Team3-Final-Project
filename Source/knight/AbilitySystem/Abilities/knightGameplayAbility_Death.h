#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/knightGameplayAbility.h"
#include "knightGameplayAbility_Death.generated.h"

class UAnimMontage;
class UCameraShakeBase;
class UNiagaraSystem;
class USoundBase;

/**
 * Death presentation. AknightCharacter::HandleOutOfHealth adds State.Dead, sends Event.Death (this ability's
 * trigger) and then cancels every ability except Ability.Death, so this is the last thing that runs.
 * The raid result is reported by the character through the GameMode (HandlePlayerDeath -> NotifyPlayerDefeated);
 * this ability only plays the animation.
 *
 * The montage should have auto blend out disabled so the final pose holds. The ability stays active until the
 * avatar is destroyed (map travel, respawn) and never stops the montage itself.
 *
 * (2026-10-19 사용자 결정 '로봇이 쓰러지는 것보다 터지는 게 좋다') 폭발: bExplodeOnDeath면 사망 몽타주를 시작한 뒤
 * ExplodeDelay초에 몸 중심(ExplosionSocket)에서 폭발 이펙트·소리·카메라 흔들림을 내고 메카(본체·부위·파츠·무기 메시)를 모두 숨긴다.
 * 이펙트는 지금 있는 것을 섞은 임시값(GA_Knight_Death에서 교체). 캡슐은 남겨 둔다(지우면 카메라가 바닥 아래로 떨어진다).
 */
UCLASS()
class KNIGHT_API UknightGameplayAbility_Death : public UknightGameplayAbility
{
	GENERATED_BODY()

public:
	UknightGameplayAbility_Death(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

protected:
	/** Played for an ordinary death. */
	UPROPERTY(EditDefaultsOnly, Category="Death")
	TObjectPtr<UAnimMontage> DeathMontage;

	/** Played when the Knight dies while knocked down (State.HitReact.Knockdown). Falls back to DeathMontage. */
	UPROPERTY(EditDefaultsOnly, Category="Death")
	TObjectPtr<UAnimMontage> KnockdownDeathMontage;

	/** (2026-10-19) 죽으면 폭발하며 사라진다. 끄면 예전처럼 쓰러진 채 남는다. */
	UPROPERTY(EditDefaultsOnly, Category="Death|Explode")
	bool bExplodeOnDeath = true;

	/** 사망 몽타주를 시작하고 폭발까지의 시간(초). 0이면 죽는 순간 바로. 바뀔 수 있는 값. */
	UPROPERTY(EditDefaultsOnly, Category="Death|Explode", meta=(ClampMin=0, Units="s", EditCondition="bExplodeOnDeath"))
	float ExplodeDelay = 0.3f;

	/** 폭발 위치(메시 소켓·뼈). 없으면 액터 위치. */
	UPROPERTY(EditDefaultsOnly, Category="Death|Explode", meta=(EditCondition="bExplodeOnDeath"))
	FName ExplosionSocket = TEXT("spine_03");

	/** 폭발 이펙트(여러 개를 겹쳐 낸다). */
	UPROPERTY(EditDefaultsOnly, Category="Death|Explode", meta=(EditCondition="bExplodeOnDeath"))
	TArray<TObjectPtr<UNiagaraSystem>> ExplosionEffects;

	UPROPERTY(EditDefaultsOnly, Category="Death|Explode", meta=(ClampMin=0.1, EditCondition="bExplodeOnDeath"))
	float ExplosionScale = 1.5f;

	UPROPERTY(EditDefaultsOnly, Category="Death|Explode", meta=(EditCondition="bExplodeOnDeath"))
	TObjectPtr<USoundBase> ExplosionSound;

	UPROPERTY(EditDefaultsOnly, Category="Death|Explode", meta=(EditCondition="bExplodeOnDeath"))
	TSubclassOf<UCameraShakeBase> ExplosionShake;

	UPROPERTY(EditDefaultsOnly, Category="Death|Explode", meta=(ClampMin=0, EditCondition="bExplodeOnDeath"))
	float ExplosionShakeScale = 3.f;

private:
	/** 폭발 이펙트·소리·흔들림을 내고 메카 메시를 숨긴다. */
	UFUNCTION()
	void Explode();
};
