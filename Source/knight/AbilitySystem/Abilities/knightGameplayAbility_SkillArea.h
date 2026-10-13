#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/knightGameplayAbility_Skill.h"
#include "knightGameplayAbility_SkillArea.generated.h"

class UNiagaraSystem;

/** 범위 타격 한 번. 몽타주 시작부터 Time초에, 액터 기준 Offset(X 앞, Y 오른쪽, Z 위)에 반지름 Radius로 친다. */
USTRUCT(BlueprintType)
struct FknightSkillAreaPulse
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category="Pulse", meta=(ClampMin=0, Units="s"))
	float Time = 0.3f;

	UPROPERTY(EditAnywhere, Category="Pulse")
	FVector Offset = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, Category="Pulse", meta=(ClampMin=10, Units="cm"))
	float Radius = 300.f;

	/** 켜면 이 타격 지점(바닥)에 PulseEffect·PulseShake를 낸다. */
	UPROPERTY(EditAnywhere, Category="Pulse")
	bool bPlayEffect = true;
};

/**
 * 범위 스킬 (2026-10-12). 몽타주를 재생하고, Pulses에 적힌 시간·위치마다 범위 타격(MeleeTrace HitArea → Event.Melee.Hit)을 한다.
 *   Q 회전 베기: 제자리 둘레를 짧은 간격으로 여러 번 친다(같은 대상도 매번 맞는다, bHitOncePerActor 끔).
 *   E 충격파: 앞으로 점점 멀리 퍼지는 타격을 차례로 낸다(같은 대상은 한 번만, bHitOncePerActor 켬).
 * 시간은 몽타주 초 기준이며 재생 속도(PlayRate x 행동 속도)에 맞춰 줄어든다.
 */
UCLASS()
class KNIGHT_API UknightGameplayAbility_SkillArea : public UknightGameplayAbility_Skill
{
	GENERATED_BODY()

public:
	UknightGameplayAbility_SkillArea(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	virtual void StartSkill() override;

	UPROPERTY(EditDefaultsOnly, Category="Skill|Montage")
	TObjectPtr<UAnimMontage> SkillMontage;

	UPROPERTY(EditDefaultsOnly, Category="Skill|Montage", meta=(ClampMin=0.01))
	float PlayRate = 1.f;

	UPROPERTY(EditDefaultsOnly, Category="Skill|Area")
	TArray<FknightSkillAreaPulse> Pulses;

	/** 켜면 이번 발동에서 같은 대상은 한 번만 맞는다. */
	UPROPERTY(EditDefaultsOnly, Category="Skill|Area")
	bool bHitOncePerActor = false;

	/** 타격 지점 바닥에 내는 이펙트 (예: NS_Hit_Scifi_Once). */
	UPROPERTY(EditDefaultsOnly, Category="Skill|Area")
	TObjectPtr<UNiagaraSystem> PulseEffect;

	UPROPERTY(EditDefaultsOnly, Category="Skill|Area", meta=(ClampMin=0))
	float PulseEffectScale = 1.5f;

	UPROPERTY(EditDefaultsOnly, Category="Skill|Area")
	TSubclassOf<UCameraShakeBase> PulseShake;

	UPROPERTY(EditDefaultsOnly, Category="Skill|Area", meta=(ClampMin=0))
	float PulseShakeScale = 0.6f;

	/** 디버그: 타격 범위를 그린다. */
	UPROPERTY(EditDefaultsOnly, Category="Skill|Area")
	bool bDrawPulses = false;

private:
	void FirePulse(int32 Index);

	TArray<FTimerHandle> PulseTimers;
};
