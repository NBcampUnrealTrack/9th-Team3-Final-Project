#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/knightGameplayAbility_MonsterAttack.h"
#include "Combat/knightCombatTypes.h"
#include "knightGameplayAbility_MonsterProjectileAttack.generated.h"

class UAbilityTask_Repeat;
class UAnimMontage;
class UNiagaraSystem;

/**
 * 몬스터가 MuzzleSocket에서 타깃 쪽으로 직선 투사체(AknightMonsterSonicProjectile)를 쏘는 원거리 공격(잔향 초음파 발사).
 * 발동 뒤 FireTime에 첫 발을 쏘고, ShotInterval 간격으로 ShotCount발(분노 중이면 EnragedShotCount발)을 쏜다.
 * 쏠 때마다 그 순간의 타깃 위치를 겨눈다. 쏘기 전까지는 타깃 쪽으로 몸을 돌린다.
 * 마지막 발 뒤 RecoveryTime이 지나면 끝난다.
 */
UCLASS()
class KNIGHT_API UknightGameplayAbility_MonsterProjectileAttack : public UknightGameplayAbility_MonsterAttack
{
	GENERATED_BODY()

public:
	UknightGameplayAbility_MonsterProjectileAttack(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

protected:
	/** 선택 사항. 연출용으로 재생하며 어빌리티가 끝나면 멈춘다. */
	UPROPERTY(EditDefaultsOnly, Category="Attack")
	TObjectPtr<UAnimMontage> Montage;

	UPROPERTY(EditDefaultsOnly, Category="Attack")
	FknightMeleeHitParams HitParams;

	/** 발동부터 첫 발까지. 예고 동작 시간이다. */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Projectile", meta=(ClampMin=0, Units="s"))
	float FireTime = 0.6f;

	UPROPERTY(EditDefaultsOnly, Category="Attack|Projectile", meta=(ClampMin=1))
	int32 ShotCount = 1;

	/** State.Monster.Enraged일 때 쏘는 발 수(잔향 공명해방 3연발). */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Projectile", meta=(ClampMin=1))
	int32 EnragedShotCount = 1;

	UPROPERTY(EditDefaultsOnly, Category="Attack|Projectile", meta=(ClampMin=0, Units="s"))
	float ShotInterval = 0.6f;

	/** 투사체가 나가는 뼈(가슴). */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Projectile")
	FName MuzzleSocket = TEXT("spine_02");

	/** MuzzleSocket에서 몬스터 방향 기준으로 더하는 오프셋(X가 정면). */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Projectile")
	FVector MuzzleOffset = FVector(200.f, 0.f, 0.f);

	UPROPERTY(EditDefaultsOnly, Category="Attack|Projectile", meta=(ClampMin=1, Units="cm/s"))
	float ProjectileSpeed = 3000.f;

	UPROPERTY(EditDefaultsOnly, Category="Attack|Projectile", meta=(ClampMin=1, Units="cm"))
	float ProjectileRadius = 120.f;

	UPROPERTY(EditDefaultsOnly, Category="Attack|Projectile", meta=(ClampMin=1, Units="cm"))
	float MaxDistance = 4000.f;

	/** 선택 사항. 없으면 기본 구체가 보인다. */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Projectile")
	TObjectPtr<UNiagaraSystem> ProjectileEffect;

	UPROPERTY(EditDefaultsOnly, Category="Attack|Projectile", meta=(ClampMin=0, Units="deg"))
	float AimTurnRate = 180.f;

	UPROPERTY(EditDefaultsOnly, Category="Attack|Timing", meta=(ClampMin=0, Units="s"))
	float RecoveryTime = 0.8f;

private:
	UFUNCTION()
	void HandleTick(int32 ActionNumber);

	void Fire();

	TWeakObjectPtr<UAbilityTask_Repeat> TickTask;
	float Elapsed = 0.f;
	float NextShotTime = 0.f;
	int32 ShotsLeft = 0;
	float EndTime = 0.f;
};
