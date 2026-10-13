#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/knightGameplayAbility.h"
#include "Combat/knightCombatTypes.h"
#include "knightGameplayAbility_Skill.generated.h"

class ACharacter;
class UAnimMontage;
class UCameraShakeBase;
class UNiagaraComponent;
class UNiagaraSystem;
class USoundBase;

/**
 * 스킬 공용 부모 (2026-10-12). Q·E·T·G (InputTag.Skill.1~4)에 하나씩 등록한다.
 *
 * 공통 규칙 (사용자 결정: 부스트 게이지 + 개별 쿨타임):
 *   - 비용: StaminaCost(부스트 게이지). bRequireFullStaminaCost가 켜져 있어 비용만큼 있어야 나간다.
 *   - 쿨타임: 발동한 순간부터 Cooldown초. 스킬마다 따로 센다(어빌리티가 액터별 인스턴스라 액터마다 따로).
 *     HUD는 GetCooldownRemaining / GetCooldownDuration으로 쿨타임 아이콘을 그릴 수 있다.
 *   - 피격·사망 중에는 못 쓴다. 활성화 그룹은 공격과 같다(Exclusive_Replaceable: 캔슬·콤보 창에서 끼어든다).
 *   - 방향: 락온 중이면 락온 대상을, 아니면 누른 방향키 쪽을 보고 시작한다.
 *   - 연출: CastSound, CastShake(시작할 때).
 * 등 부스터 도우미(StartBackThrusters 등)는 대시와 같은 방식이며, 자식 스킬이 필요할 때 쓴다.
 */
UCLASS(Abstract)
class KNIGHT_API UknightGameplayAbility_Skill : public UknightGameplayAbility
{
	GENERATED_BODY()

public:
	UknightGameplayAbility_Skill(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const override;
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

	/** 남은 쿨타임(초). 0이면 쓸 수 있다. */
	UFUNCTION(BlueprintPure, Category="Knight|Skill")
	float GetCooldownRemaining() const;

	UFUNCTION(BlueprintPure, Category="Knight|Skill")
	float GetCooldownDuration() const { return Cooldown; }

protected:
	/** 자식 스킬의 본 동작. 비용·쿨타임을 낸 뒤, 방향을 정한 다음 불린다. 끝날 때 EndAbility를 직접 부른다. */
	virtual void StartSkill() {}

	/** 몽타주를 재생하고 끝나면(또는 끊기면) 어빌리티를 끝낸다. */
	void PlaySkillMontage(UAnimMontage* Montage, float Rate);

	/** 락온 대상 쪽으로 즉시 돌린다. 락온이 없으면 false. */
	bool FaceLockedTarget() const;

	/** Event.Melee.Hit를 받아 HitParams로 처리하기 시작한다. bHitOncePerActor면 이번 발동에서 같은 대상은 한 번만 맞는다. */
	void ListenForMeleeHits(bool bHitOncePerActor);

	/** 피해를 준 직후 (자식이 연출을 붙일 때). */
	virtual void OnSkillHit(const FHitResult& Hit, EknightHitOutcome Outcome) {}

	// 등 부스터 (대시 등 부스터와 같은 방식). Direction은 몸이 나가는 방향(수평).
	void StartBackThrusters(const FVector& Direction);
	void StopBackThrusters(float Linger = 0.f);

	UPROPERTY(EditDefaultsOnly, Category="Skill", meta=(ClampMin=0, Units="s"))
	float Cooldown = 8.f;

	UPROPERTY(EditDefaultsOnly, Category="Skill|Hit")
	FknightMeleeHitParams HitParams;

	UPROPERTY(EditDefaultsOnly, Category="Skill|Effects")
	TObjectPtr<USoundBase> CastSound;

	UPROPERTY(EditDefaultsOnly, Category="Skill|Effects")
	TSubclassOf<UCameraShakeBase> CastShake;

	UPROPERTY(EditDefaultsOnly, Category="Skill|Effects", meta=(ClampMin=0))
	float CastShakeScale = 0.5f;

	UPROPERTY(EditDefaultsOnly, Category="Skill|Back Booster")
	TObjectPtr<UNiagaraSystem> BackThrusterEffect;

	UPROPERTY(EditDefaultsOnly, Category="Skill|Back Booster")
	FName BackThrusterBone = TEXT("spine_03");

	UPROPERTY(EditDefaultsOnly, Category="Skill|Back Booster")
	TArray<FVector> BackThrusterOffsets;

	UPROPERTY(EditDefaultsOnly, Category="Skill|Back Booster", meta=(ClampMin=0.1))
	float BackThrusterScale = 1.f;

	UPROPERTY(EditDefaultsOnly, Category="Skill|Back Booster", meta=(ClampMin=0, ClampMax=80, Units="deg"))
	float BackThrusterDownAngle = 35.f;

private:
	UFUNCTION()
	void OnSkillMontageFinished();

	UFUNCTION()
	void OnMeleeHitEvent(FGameplayEventData Payload);

	void UpdateBackThrusters();
	void ClearBackThrusters();

	UPROPERTY(Transient)
	TArray<TObjectPtr<UNiagaraComponent>> BackThrusters;

	FTimerHandle BackThrusterTimer;
	FTimerHandle BackThrusterStopTimer;
	FVector ThrusterDirection = FVector::ZeroVector;

	TSet<TWeakObjectPtr<AActor>> HitActors;
	bool bHitOnceThisCast = false;

	/** 이 월드 시각(초)이 지나기 전에는 다시 쓸 수 없다. */
	float CooldownEndTime = 0.f;
};
