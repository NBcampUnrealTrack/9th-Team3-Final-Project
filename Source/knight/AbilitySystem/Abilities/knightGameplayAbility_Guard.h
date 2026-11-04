#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/knightGameplayAbility.h"
#include "Input/knightMovePressTracker.h"
#include "knightGameplayAbility_Guard.generated.h"

class ACharacter;
class UAnimMontage;
class UGameplayEffect;

/**
 * Hold to guard. The first ParryWindow seconds after pressing also count as a parry.
 *
 * While active the owner has State.Guarding (UknightDamageExecution reduces frontal hits) and, during the
 * parry window, State.Parrying (UknightCombatLibrary zeroes the hit and staggers the attacker).
 * Releasing the input ends the guard.
 *
 * Parry counter: when a parry lands, UknightCombatLibrary sends ParrySucceededEventTag (Event.ParrySucceeded) to
 * the defender. The guard then gives the owner ParryCounterTag (State.ParryCounter, which the counter attack
 * requires) for ParryCounterWindow seconds, applies the optional ParryCounterEffect, and ends, so the attack key
 * can start the counter.
 *
 * (2026-10-08) 행동 속도(ActionSpeed): GuardMontage는 행동 속도 배속으로 재생한다. ParryWindow는 초 단위 그대로.
 *
 * (2026-10-08) 퍼펙트 가드 = 이 클래스의 패리(Parry). 태그·이벤트 이름은 그대로 Parry를 쓴다.
 *   판정 시간: 메카 속성 PerfectGuardWindow(기본 0.25초, 파츠로 변경 가능)를 가드를 누를 때 읽는다. 메카 속성이 없으면 ParryWindow.
 *   판정: 패리 가능한 공격(bCanBeParried)을 가드 범위(정면) 안에서 막았을 때만 (UknightCombatLibrary::ApplyMeleeHit). 그 외는 일반 가드·피격.
 *   결과: 받는 피해 0 + 반격 창(위) + 공격한 적에게 그로기 피해
 *         그로기 = (PerfectGuardPoiseDamage + 내 공격력 x PerfectGuardPoiseMotionValue / 100) x 내 경직치(StaggerPower)
 *         (2026-10-09) 몬스터도 Event.Parried를 받으면 스스로 '최대 그로기 x ParriedPoiseDamageRatio'를 깎으므로 둘이 더해진다.
 *         몬스터별 기본은 몬스터 쪽, 여기는 공격력 보너스만 맡도록 PerfectGuardPoiseDamage 기본값을 0으로 내렸다.
 *         피해 실행(UknightDamageExecution)을 그대로 거치므로 적의 그로기가 0이 되면 평소처럼 그로기 반응이 나온다.
 *   ParrySucceededEventTag가 비어 있으면 반격 창과 그로기 피해 둘 다 없다(같은 이벤트로 받는다).
 *   키를 떼면 판정 시간 중이라도 바로 가드를 내린다(사용자 결정). 전에는 판정 시간이 끝날 때까지 가드가 유지됐다(짧게 눌러도 패리 유지).
 *
 * (2026-10-08) 가드 중 다른 입력 (사용자 결정):
 *   - 가드 중에는 Window.Cancel을 켜 두어 공격·강공격·퀵스텝(나중에 점프 어빌리티)이 바로 가드를 대신한다.
 *   - 가드 중에는 이동을 잠근다(제자리, 몸도 돌지 않음). 새로 누른 이동 입력이 들어오면 가드를 내리고 이동한다.
 *     (2026-10-26) 기본은 이동 입력으로 가드가 풀리지 않는다(bMoveInputCancelsGuard). 가드 키를 떼거나 퀵스텝·공격·점프로 끊는다.
 *     가드를 올릴 때 누르고 있던 방향키는 새 입력이 아니다: 그 방향을 바로 바라보는 데에만 쓴다(락온 중 제외).
 *   - 가드 키를 누른 채 다른 행동을 하고 나도 가드는 자동으로 다시 올라가지 않는다
 *     (자동으로 올라가면 키를 새로 누르지 않아도 퍼펙트 가드 판정 시간이 다시 생기므로).
 *
 * (2026-10-15 사용자 결정) 누른 순간 막혀 있던 가드(공격 후딜 등)는 키를 누르고 있는 동안 가드할 수 있게 되는 순간 올린다
 *   (bRetryPressWhileHeld, 한 번 올라가면 그 누름은 끝이라 위 규칙은 그대로). 퍼펙트 가드 판정 시간은 가드가 올라간 순간부터
 *   전부 준다(미리 눌러 둔 가드도 같음. 올린 순간 맞았는데 퍼펙트가 안 되면 불쾌하다는 사용자 판단).
 *
 * (2026-10-26 사용자 결정) 가드 연타 막기 — 누르고·떼고·누르기를 반복해 퍼펙트 가드 판정을 계속 새로 받던 것:
 *   - 퍼펙트 가드 없이 가드를 내린 뒤 ReguardLockoutTime(0.15초) 동안은 다시 올릴 수 없다(CanActivateAbility).
 *     그동안 누르고 있으면 위 bRetryPressWhileHeld로 잠금이 풀리는 순간 올라간다.
 *   - 내린 뒤 PerfectGuardRearmTime(0.3초) 안에 다시 올리면 퍼펙트 가드 판정이 RearmPerfectGuardScale(0.5)배,
 *     그렇게 연속 두 번째부터는 0(일반 가드만). 그 시간이 지나 올리면 원래대로, 연속 횟수도 처음부터.
 *   - 퍼펙트 가드에 성공해 내려간 직후는 둘 다 적용하지 않는다(연속 공격을 이어서 패링하는 플레이는 그대로).
 */
UCLASS()
class KNIGHT_API UknightGameplayAbility_Guard : public UknightGameplayAbility
{
	GENERATED_BODY()

public:
	UknightGameplayAbility_Guard(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr,
		FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	/**
	 * Seconds after the press during which an incoming hit is parried. Zero disables parrying.
	 * (2026-10-08) 메카 속성 PerfectGuardWindow가 있으면(플레이어) 그 값을 쓰고, 이 값은 속성 세트가 없을 때만 쓰는 예비값이다.
	 */
	UPROPERTY(EditDefaultsOnly, Category="Guard", meta=(ClampMin=0, Units="s"))
	float ParryWindow = 0.2f;

	/**
	 * 락온 중 가드하는 동안 대상을 따라 도는 속도(도/초). 0이면 누를 때 한 번만 대상을 본다. (2026-10-12)
	 * 누른 순간에는 항상 대상을 바로 본다(패링은 정면 범위에서만 되므로).
	 */
	UPROPERTY(EditDefaultsOnly, Category="Guard", meta=(ClampMin=0, Units="deg"))
	float GuardLockOnTurnRate = 540.f;

	/**
	 * (2026-10-26 사용자 결정 '비숙련자는 가드 중 움직이면 가드가 풀리는 게 불편') 켜면 가드 중 새로 누른 이동 입력이 가드를 내리고 이동한다(예전 동작).
	 * 끄면(기본) 가드 키를 누르고 있는 동안 방향키를 눌러도 가드가 유지된다. 몸 방향은 그대로 카메라·락온 대상을 따라간다.
	 */
	UPROPERTY(EditDefaultsOnly, Category="Guard")
	bool bMoveInputCancelsGuard = false;

	/** Optional looping guard pose. The ability ends on input release, not when this montage ends. */
	UPROPERTY(EditDefaultsOnly, Category="Guard")
	TObjectPtr<UAnimMontage> GuardMontage;

	/** Event sent to the defender when a parry lands. Empty disables the counter. */
	UPROPERTY(EditDefaultsOnly, Category="Guard|Parry Counter", meta=(Categories="Event"))
	FGameplayTag ParrySucceededEventTag;

	/** Loose tag given to the owner for ParryCounterWindow seconds after a successful parry. */
	UPROPERTY(EditDefaultsOnly, Category="Guard|Parry Counter", meta=(Categories="State"))
	FGameplayTag ParryCounterTag;

	UPROPERTY(EditDefaultsOnly, Category="Guard|Parry Counter", meta=(ClampMin=0, Units="s"))
	float ParryCounterWindow = 0.8f;

	/** Optional extra effect applied to self on a successful parry (gauge refill, buff). */
	UPROPERTY(EditDefaultsOnly, Category="Guard|Parry Counter")
	TSubclassOf<UGameplayEffect> ParryCounterEffect;

	/**
	 * 패링 성공 때 뒤로 살짝 주춤하는 모션. 가드가 끝난 뒤에 재생되므로 반격(공격 키)이 바로 끊고 들어갈 수 있다. (2026-10-13)
	 * 비어 있으면 모션 없이 밀리기만 한다.
	 */
	UPROPERTY(EditDefaultsOnly, Category="Guard|Parry Recoil")
	TObjectPtr<UAnimMontage> ParryRecoilMontage;

	UPROPERTY(EditDefaultsOnly, Category="Guard|Parry Recoil", meta=(ClampMin=0.1))
	float ParryRecoilPlayRate = 1.f;

	/** 패링 성공 때 공격한 적 반대쪽(수평)으로 밀리는 거리. 0이면 밀리지 않는다. (2026-10-13) */
	UPROPERTY(EditDefaultsOnly, Category="Guard|Parry Recoil", meta=(ClampMin=0, Units="cm"))
	float ParryRecoilDistance = 0.f;

	UPROPERTY(EditDefaultsOnly, Category="Guard|Parry Recoil", meta=(ClampMin=0.02, Units="s"))
	float ParryRecoilTime = 0.15f;

	/**
	 * 퍼펙트 가드 때 공격한 적에게 주는 기본 그로기 피해. (2026-10-08 임시값 20 → 2026-10-09 0)
	 * 몬스터도 퍼펙트 가드를 당하면 스스로 '최대 그로기 x 몬스터별 비율'을 깎는다(MonsterDefinition.ParriedPoiseDamageRatio, Samooyang).
	 * 그래서 몬스터별 기본은 몬스터 쪽이 맡고, 여기서는 아래 공격력 몫만 보너스로 준다(사용자 결정: 역할 나눠 둘 다 유지).
	 */
	UPROPERTY(EditDefaultsOnly, Category="Guard|Perfect Guard", meta=(ClampMin=0))
	float PerfectGuardPoiseDamage = 0.f;

	/**
	 * 퍼펙트 가드 그로기에 더하는 공격력 계수(%). 그로기 += 내 공격력 x 이 값 / 100.
	 * 피해 공식의 모션 계수와 같은 방식. 일반 공격 모션 계수 기본값(30)으로 시작 (2026-10-08 임시값, 조정 대상).
	 */
	UPROPERTY(EditDefaultsOnly, Category="Guard|Perfect Guard", meta=(ClampMin=0))
	float PerfectGuardPoiseMotionValue = 30.f;

	/** 퍼펙트 가드 그로기 피해에 쓰는 이펙트. 실행이 UknightDamageExecution이어야 한다. 기본은 C++ 기본 피해 이펙트. */
	UPROPERTY(EditDefaultsOnly, Category="Guard|Perfect Guard")
	TSubclassOf<UGameplayEffect> PerfectGuardDamageEffect;

	/** (2026-10-26) 퍼펙트 가드 없이 가드를 내린 뒤 다시 올릴 수 없는 시간. 0이면 끈다. 바뀔 수 있는 값. */
	UPROPERTY(EditDefaultsOnly, Category="Guard|Reguard", meta=(ClampMin=0, Units="s"))
	float ReguardLockoutTime = 0.15f;

	/** (2026-10-26) 가드를 내린 뒤 이 시간 안에 다시 올리면 퍼펙트 가드 판정을 줄인다(아래 배율, 연속 두 번째부터 0). 0이면 끈다. 바뀔 수 있는 값. */
	UPROPERTY(EditDefaultsOnly, Category="Guard|Reguard", meta=(ClampMin=0, Units="s"))
	float PerfectGuardRearmTime = 0.3f;

	/** (2026-10-26) 위 시간 안에 처음 다시 올린 가드의 퍼펙트 가드 판정 배율. 바뀔 수 있는 값. */
	UPROPERTY(EditDefaultsOnly, Category="Guard|Reguard", meta=(ClampMin=0, ClampMax=1))
	float RearmPerfectGuardScale = 0.5f;

private:
	/** 이번 가드의 퍼펙트 가드 판정 시간(초). 메카 속성 PerfectGuardWindow, 없으면 ParryWindow. */
	float GetPerfectGuardWindow() const;

	/** 공격한 적에게 퍼펙트 가드 그로기 피해를 준다 (피해는 0). */
	void ApplyPerfectGuardPoiseDamage(const AActor* Attacker) const;

	/** 패링 성공 뒤 주춤: 밀리기 + 모션. 가드를 끝낸 뒤에 부른다(어빌리티가 끝나도 유지되게). */
	void PlayParryRecoil(ACharacter* Character, const AActor* Attacker) const;

	UFUNCTION()
	void OnParrySucceeded(FGameplayEventData Payload);

	UFUNCTION()
	void OnParryWindowEnded();

	UFUNCTION()
	void OnGuardReleased(float TimeHeld);

	/** 가드 중에 공중에 떴다(State.Airborne). 가드를 내린다. */
	UFUNCTION()
	void OnLeftGround();

	void RemoveParryTag();
	void WaitForRelease();

	/** 락온 대상 쪽으로 몸을 돌린다. MaxDeltaYaw < 0이면 바로 돌리고, 아니면 최대 그 각도만큼. 락온이 없으면 false. */
	bool FaceLockedTarget(float MaxDeltaYaw) const;

	/** 가드 자세 상태를 켠다: 캔슬 창(다른 행동이 가드를 대신), 이동 잠금, 새 이동 입력 감시. */
	void EnterGuardStance();

	/** EnterGuardStance로 켠 것을 끈다. */
	void ExitGuardStance();

	/** 가드 중 몸 돌리기(락온 대상·카메라), 그리고 bMoveInputCancelsGuard면 새로 누른 이동 입력에 가드를 내린다(잠금이 풀리며 그대로 이동). */
	void CheckMoveCancel();

	bool bParryTagApplied = false;
	bool bCancelWindowApplied = false;
	bool bMoveLockApplied = false;

	/** 가드를 올릴 때 누르고 있던 방향은 새 이동 입력이 아니다(방향 전환에만 씀). */
	FknightMovePressTracker MovePress;
	FTimerHandle MoveCancelTimer;

	/** (2026-10-26) 가드 연타 막기: 퍼펙트 가드 없이 가드를 내린 시각(없으면 -1), 빨리 다시 올린 연속 횟수, 이번 가드가 올라갔는지·패링했는지. */
	double LastGuardEndTime = -1.0;
	int32 QuickReguardCount = 0;
	bool bGuardRaised = false;
	bool bParriedThisGuard = false;
};
