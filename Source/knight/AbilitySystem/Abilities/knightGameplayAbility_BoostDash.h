#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/knightGameplayAbility.h"
#include "knightGameplayAbility_BoostDash.generated.h"

class ACharacter;
class UAbilityTask;
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
 *   쿨타임 = 대시 이동이 끝난 뒤 DodgeCooldown(메카 속성, 초) 동안 다시 대시할 수 없다. 기본 0.5초.
 *
 * 경직과 캔슬 (2026-10-07):
 *   흐름: 대시 이동(DashDuration) → 경직(RecoveryDuration) → 종료. 몽타주는 경직까지 이어서 재생된다.
 *   경직 중에는 이동 입력으로 가속하지 않고(미끄러지며 정지) Window.Cancel을 켠다.
 *     - 공격·가드: Window.Cancel 덕분에 활성화 그룹 규칙대로 대시를 대신한다. 대시 중 미리 누른 입력은 버퍼에서 나간다.
 *     - 대시: 같은 어빌리티라 다시 발동되지 않으므로, 쿨타임이 끝났으면 이 어빌리티 안에서 새 대시를 이어서 낸다(비용 다시 냄).
 *       대시 중 미리 누른 대시(버퍼)는 경직 시작 때, 경직 중 누른 대시는 누른 순간 확인한다.
 *       쿨타임이 안 끝났을 때 누른 입력은 버퍼에 남아, 어빌리티가 끝난 뒤 쿨타임이 지나면 발동될 수 있다.
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
	/** 배율·중량 계수가 1일 때의 대시 거리. (2026-10-07 900 → 750, 너무 멀리 가서 줄임) */
	UPROPERTY(EditDefaultsOnly, Category="Dash", meta=(ClampMin=0, Units="cm"))
	float DashDistance = 750.f;

	UPROPERTY(EditDefaultsOnly, Category="Dash", meta=(ClampMin=0.05, Units="s"))
	float DashDuration = 0.35f;

	/**
	 * 대시 이동이 끝난 뒤의 경직 시간. 이 동안 공격·가드·(쿨타임이 끝난) 대시로 캔슬할 수 있다. 0이면 경직 없이 바로 끝난다.
	 * (2026-10-07 0.25 → 0.1, 플레이해 보니 경직이 길게 느껴져 줄임)
	 */
	UPROPERTY(EditDefaultsOnly, Category="Dash", meta=(ClampMin=0, Units="s"))
	float RecoveryDuration = 0.1f;

	/**
	 * 대시 몽타주 재생 속도(고정). 어빌리티가 끝나면(대시 + 경직) 그 지점에서 블렌드 아웃된다.
	 * 1.5배속, 대시 0.35초 + 경직 0.1초면 몽타주(1.33초) 약 51% 지점에서 끝난다.
	 * (2026-10-07: 대시+경직 시간에 맞춘 자동 배속(약 2.4~3배)은 대시 중에 동작이 너무 빨리 지나가서 고정 배속으로 바꿈.
	 *  1.0배속도 시험해 본 뒤 1.5배속으로 정함)
	 */
	UPROPERTY(EditDefaultsOnly, Category="Dash", meta=(ClampMin=0.1))
	float MontagePlayRate = 1.5f;

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

	/**
	 * 대시마다 실행하는 게임플레이 큐(추진기 이펙트). 비우면 실행하지 않는다.
	 * (2026-10-08 추가: 같은 이동 로직을 쓰는 구르기 회피(knightGameplayAbility_Dodge)는 추진기 큐를 쓰지 않는다)
	 */
	UPROPERTY(EditDefaultsOnly, Category="Dash", meta=(Categories="GameplayCue"))
	FGameplayTag DashCueTag;

	/** 대시 소리의 AI 청각 크기. */
	UPROPERTY(EditDefaultsOnly, Category="Dash|Noise", meta=(ClampMin=0))
	float NoiseLoudness = 1.f;

	/** 대시 소리가 들리는 최대 거리. 0이면 듣는 쪽의 청각 범위를 쓴다. */
	UPROPERTY(EditDefaultsOnly, Category="Dash|Noise", meta=(ClampMin=0, Units="cm"))
	float NoiseRange = 0.f;

private:
	UAnimMontage* SelectDashMontage(const ACharacter& Character, const FVector& Direction) const;

	/** 대시 한 번(방향·몽타주·무적·루트 모션). 첫 발동과 경직 중 이어지는 대시가 같이 쓴다. */
	void StartDash(ACharacter& Character);

	/**
	 * 몽타주 재생 속도. 지금 규칙: MontagePlayRate 고정.
	 * 바뀔 수 있는 항목이다(예: 대시 + 경직 시간에 맞춘 자동 배속, 대시 부분만 맞추기). 바꿀 때는 이 함수만 고치면 된다.
	 */
	float GetMontagePlayRate(const UAnimMontage* Montage) const;

	/** 메카 속성 값을 읽는다. 해당 속성 세트가 없으면 Fallback. */
	float GetMechAttributeValue(const FGameplayAttribute& Attribute, float Fallback) const;

	UFUNCTION()
	void OnDashFinished();

	UFUNCTION()
	void OnInvulnerabilityEnded();

	UFUNCTION()
	void OnRecoveryFinished();

	UFUNCTION()
	void OnRecoveryInputPressed(float TimeWaited);

	/** 경직 중 대시 입력을 기다린다. */
	void WaitForChainInput();

	/** 경직 중 새 대시로 이어간다. bRequireBufferedInput이면 버퍼에 대시 입력이 있을 때만. 이어졌으면 true. */
	bool TryChainDash(bool bRequireBufferedInput);

	/** 경직 상태(Window.Cancel, 이동 잠금)를 푼다. */
	void ReleaseRecoveryState();

	void StartCooldown();
	void RemoveInvulnerability();

	/** 이 어빌리티에 연결된 입력 태그(InputTag.Boost 등). 버퍼에서 대시 입력을 찾을 때 쓴다. */
	FGameplayTag GetInputTag() const;

	bool bInvulnerableApplied = false;
	bool bCancelWindowApplied = false;
	bool bMoveLockApplied = false;
	bool bInRecovery = false;

	/** 이어지는 대시에서 이전 대시의 태스크를 정리하려고 들고 있는다. */
	TWeakObjectPtr<UAbilityTask> InvulnerableTask;
	TWeakObjectPtr<UAbilityTask> MontageTask;
	TWeakObjectPtr<UAbilityTask> RecoveryTask;
	TWeakObjectPtr<UAbilityTask> ChainInputTask;

	/** 이 월드 시각(초)이 지나기 전에는 다시 발동할 수 없다. 어빌리티가 액터별 인스턴스라 액터마다 따로 기록된다. */
	float CooldownEndTime = 0.f;

	/** 대시를 시작했지만 아직 쿨타임을 걸지 않았는지. 이동이 끝나거나, 그 전에 어빌리티가 끊기면 건다. */
	bool bCooldownPending = false;
};
