#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/knightGameplayAbility_MonsterAreaAttack.h"
#include "knightGameplayAbility_MonsterGroundEruption.generated.h"

class UAudioComponent;
class UDecalComponent;
class UMaterialInterface;
class UNiagaraComponent;
class UNiagaraSystem;
class USoundBase;

/**
 * 몬스터 주변 바닥에서 불길이 솟는 범위 공격. 판정과 타이밍은 UknightGameplayAbility_MonsterAreaAttack 그대로이고,
 * 예고(WindupTime) 동안 범위 크기의 바닥 데칼(WarningDecal)을 그리고, 불길이 솟을 자리(ScatterCount곳)를 미리 골라
 * WarningEffect를 띄운다. 예고 끝 무렵 WarningBeats마다 박자(WarningBeatEffect, WarningBeatSound)를 내서 분출 타이밍을
 * 리듬으로 읽게 한다. 타격 순간 범위 중앙에 ImpactEffects와 EruptionSound를, 미리 고른 자리마다 ScatterEffect를 터뜨린다.
 */
UCLASS()
class KNIGHT_API UknightGameplayAbility_MonsterGroundEruption : public UknightGameplayAbility_MonsterAreaAttack
{
	GENERATED_BODY()

public:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

protected:
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

	virtual void OnImpact() override;

	/**
	 * 예고 동안 범위 중앙 바닥에 투영하는 데칼(반지름 AreaRadius). WindupTime 동안 페이드인하므로 머티리얼은
	 * DecalLifetimeOpacity(0 → 1)를 진행도로 쓴다. 머티리얼에 Seed, Beat0~Beat2 파라미터가 있으면 공격마다 무늬를 바꾸고
	 * WarningBeats 시점을 넘겨 박자에 맞춰 밝아지게 한다.
	 */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Presentation")
	TObjectPtr<UMaterialInterface> WarningDecal;

	/**
	 * 분출 뒤 같은 무늬의 균열이 식어 가며 남는 시간. 같은 머티리얼을 Cooling = 1로 다시 깔고 이 시간 동안 페이드아웃하므로
	 * 머티리얼은 DecalLifetimeOpacity(1 → 0)를 식는 정도로 쓴다. 0이면 분출 순간 바로 지운다.
	 */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Presentation", meta=(ClampMin=0, Units="s"))
	float AfterglowTime = 3.f;

	/** 예고 동안 불길이 솟을 자리마다 띄우는 효과(새어 나오는 불씨·연기). 계속 생성하는 시스템을 쓰고, 타격 순간 끈다. */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Presentation")
	TObjectPtr<UNiagaraSystem> WarningEffect;

	/** 예고 시작부터 범위 중앙에서 재생하는 소리(커지는 땅울림). 분출 순간 끊는다. */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Presentation")
	TObjectPtr<USoundBase> WarningSound;

	/** 박자 시점(WindupTime 비율, 오름차순). 간격을 점점 좁혀 분출이 다가오는 리듬을 만든다. 머티리얼에는 앞 3개를 넘긴다. */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Presentation")
	TArray<float> WarningBeats = { 0.62f, 0.78f, 0.9f };

	/** 박자마다 불길이 솟을 자리에서 한 번 터뜨리는 효과(튀어 오르는 불씨). */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Presentation")
	TObjectPtr<UNiagaraSystem> WarningBeatEffect;

	/** 박자마다 범위 중앙에서 재생하는 소리. 박자가 지날수록 음높이를 WarningBeatPitchStep씩 올린다. */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Presentation")
	TObjectPtr<USoundBase> WarningBeatSound;

	UPROPERTY(EditDefaultsOnly, Category="Attack|Presentation", meta=(ClampMin=0))
	float WarningBeatPitchStep = 0.08f;

	/** 분출 순간 범위 중앙에서 재생하는 소리. */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Presentation")
	TObjectPtr<USoundBase> EruptionSound;

	/** 타격 순간 범위 중앙 바닥에 터뜨리는 효과(충격파, 먼지). */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Presentation")
	TArray<TObjectPtr<UNiagaraSystem>> ImpactEffects;

	/** 타격 순간 범위 안 바닥 여러 곳에 터뜨리는 효과(솟구치는 불길). */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Presentation")
	TObjectPtr<UNiagaraSystem> ScatterEffect;

	UPROPERTY(EditDefaultsOnly, Category="Attack|Presentation", meta=(ClampMin=0))
	int32 ScatterCount = 12;

private:
	/** 범위 중앙(또는 그 주변 XY) 아래의 지형. 못 찾으면 몬스터 발밑 높이. */
	FVector FindGround(const FVector& Location) const;

	/** 몸 둘레부터 범위 끝까지 불길이 솟을 바닥 자리를 고른다. */
	void PickScatterPoints();

	/** 예고 데칼과 효과를 거둔다. 타격 전에 끊겨도 바닥에 남지 않게 한다. */
	void StopWarning();

	void PlayWarningBeat(int32 Index);

	/** 분출 순간 예고 데칼 자리에 식어 가는 균열을 깐다. 어빌리티가 끝나도 AfterglowTime 동안 남는다. */
	void SpawnAfterglow();

	TArray<FVector> ScatterPoints;
	float WarningSeed = 0.f;
	FVector WarningCenter = FVector::ZeroVector;
	TArray<FTimerHandle> WarningBeatTimers;
	TWeakObjectPtr<UAudioComponent> WarningSoundComponent;
	TWeakObjectPtr<UDecalComponent> WarningDecalComponent;
	TArray<TWeakObjectPtr<UNiagaraComponent>> WarningEffectComponents;
	bool bErupted = false;
};
