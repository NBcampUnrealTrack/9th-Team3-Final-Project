#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/knightGameplayAbility.h"
#include "knightGameplayAbility_BoostDash.generated.h"

class ACharacter;
class UAnimMontage;
class UCurveFloat;

/**
 * 부스트 대시 / 회피 부스트(퀵스텝). 현재 이동 입력 방향으로(입력이 없으면 뒤로) 일정한 힘의
 * 루트 모션 소스로 나이트를 움직이고, 처음 InvulnerableDuration초 동안 State.Invulnerable을 건다.
 *
 * 게이지 비용은 어빌리티의 Cost Gameplay Effect(BoostGauge 모디파이어)에 두므로,
 * 게이지가 모자라면 Ability.ActivateFail.Cost로 발동에 실패한다.
 *
 * 방향과 자세: 락온이 아니면 이동 입력이 있는 대시는 먼저 나이트를 대시 방향으로 돌리고 DashMontage(정면)를 재생한다.
 * 락온 중(State.LockedOn)이거나 입력 없는 백스텝이면 바라보는 방향을 유지하고, 그 방향 기준의 대시 방향으로
 * 몽타주를 고른다(앞 / 뒤 / 왼쪽 / 오른쪽). 지정하지 않은 방향 몽타주는 DashMontage를 쓴다.
 *
 * 스탯 연결 (2026-10-06):
 *   대시 거리 = DashDistance x DodgeSpeed(메카 속성, 배율) x 중량 계수(무브먼트 컴포넌트)
 *   대시 시간(DashDuration)은 그대로 두어 몽타주 타이밍이 어긋나지 않게 한다. 그래서 거리가 늘면 속도도 빨라진다.
 *   쿨타임 = 대시가 끝난 뒤 DodgeCooldown(메카 속성, 초) 동안 다시 발동할 수 없다. 기본 0이라 기존과 같다.
 */
UCLASS()
class KNIGHT_API UknightGameplayAbility_BoostDash : public UknightGameplayAbility
{
	GENERATED_BODY()

public:
	UknightGameplayAbility_BoostDash(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const override;
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	/** 배율·중량 계수가 1일 때의 대시 거리. */
	UPROPERTY(EditDefaultsOnly, Category="Dash", meta=(ClampMin=0, Units="cm"))
	float DashDistance = 900.f;

	UPROPERTY(EditDefaultsOnly, Category="Dash", meta=(ClampMin=0.05, Units="s"))
	float DashDuration = 0.35f;

	/** 선택 사항. 대시 동안의 0..1 커브. 거리는 크게 바꾸지 않고 순간 가속 모양만 바꾼다. */
	UPROPERTY(EditDefaultsOnly, Category="Dash")
	TObjectPtr<UCurveFloat> StrengthOverTime;

	/** 대시 시작부터의 회피(무적) 시간. */
	UPROPERTY(EditDefaultsOnly, Category="Dash", meta=(ClampMin=0, Units="s"))
	float InvulnerableDuration = 0.2f;

	/** 선택 사항. 추진기 자세(정면). 몽타주의 루트 모션은 무시하고 루트 모션 소스가 이동을 맡는다. */
	UPROPERTY(EditDefaultsOnly, Category="Dash")
	TObjectPtr<UAnimMontage> DashMontage;

	/** 바라보는 방향 기준으로 뒤로 움직일 때의 대시 자세 (락온, 또는 입력 없는 백스텝). */
	UPROPERTY(EditDefaultsOnly, Category="Dash|Directional")
	TObjectPtr<UAnimMontage> DashMontageBack;

	UPROPERTY(EditDefaultsOnly, Category="Dash|Directional")
	TObjectPtr<UAnimMontage> DashMontageLeft;

	UPROPERTY(EditDefaultsOnly, Category="Dash|Directional")
	TObjectPtr<UAnimMontage> DashMontageRight;

	/** 락온이 아니면 시작할 때 나이트를 대시 방향으로 돌려서 정면 자세와 맞춘다. */
	UPROPERTY(EditDefaultsOnly, Category="Dash|Directional")
	bool bFaceDashDirectionWhenFree = true;

	/** 대시 소리의 AI 청각 크기. */
	UPROPERTY(EditDefaultsOnly, Category="Dash|Noise", meta=(ClampMin=0))
	float NoiseLoudness = 1.f;

	/** 대시 소리가 들리는 최대 거리. 0이면 듣는 쪽의 청각 범위를 쓴다. */
	UPROPERTY(EditDefaultsOnly, Category="Dash|Noise", meta=(ClampMin=0, Units="cm"))
	float NoiseRange = 0.f;

private:
	UAnimMontage* SelectDashMontage(const ACharacter& Character, const FVector& Direction) const;

	/** 메카 속성 값을 읽는다. 해당 속성 세트가 없으면 Fallback. */
	float GetMechAttributeValue(const FGameplayAttribute& Attribute, float Fallback) const;

	UFUNCTION()
	void OnDashFinished();

	UFUNCTION()
	void OnInvulnerabilityEnded();

	void RemoveInvulnerability();

	bool bInvulnerableApplied = false;

	/** 이 월드 시각(초)이 지나기 전에는 다시 발동할 수 없다. 어빌리티가 액터별 인스턴스라 액터마다 따로 기록된다. */
	float CooldownEndTime = 0.f;

	/** 실제로 대시가 시작됐는지. 비용 부족 등으로 바로 끝난 경우에는 쿨타임을 걸지 않는다. */
	bool bDashStarted = false;
};
