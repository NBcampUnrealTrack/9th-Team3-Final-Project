#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/knightGameplayAbility_MonsterAttack.h"
#include "Combat/knightCombatTypes.h"
#include "knightGameplayAbility_MonsterDiveAttack.generated.h"

class UAbilityTask_ApplyRootMotionConstantForce;
class UAbilityTask_Repeat;
class UAnimMontage;

/**
 * 공중 턴의 날짐승이 날개를 접고 타깃에게 일직선으로 내리꽂는 공격(잔향 송곳 다이브).
 *
 *   접기: Montage의 FoldSection을 재생하며 FoldTime 동안 타깃 쪽으로 몸을 돌린다. 끝나는 순간 타깃 위치로 방향·거리를 고정한다.
 *   돌진: LoopSection을 반복하며 DiveSpeed로 직선 이동하고, 그동안 메시가 공중 높이에서 바닥 높이로 내려온다.
 *         머리(HitSocket) 앞 구체에 닿은 대상을 한 번씩 때린다.
 *   결과: 맞히거나 막혔으면 StuckMontage(땅에 박혔다 빠짐), 피했으면 FallMontage(쓰러졌다 일어남, 딜 기회).
 *         FallGetUpMontage가 있으면 FallMontage 뒤에 블렌드로 이어 재생한다(일어나기).
 *         퍼펙트 가드당하면 돌진이 끝나기를 기다리지 않고 그 순간 FallMontage로 넘어간다(튕겨 나는 반동부터).
 *   끝나면 공중 턴이 이어지는 동안 다시 공중 높이로 오른다.
 *
 * 다이브 중에는 State.Monster.NoFlinch를 붙여 패링·부위 파괴 경직이 다이브를 끊지 않게 한다(패링 결과는 이 어빌리티가 고른다).
 * 그로기 게이지가 다 차서 다운되면 다운이 다이브를 끊는다(격추).
 *
 * 디버그: knight.Monster.DrawAttacks 1 이면 판정 구체(빨강, 맞으면 자홍)와 고정한 도착 지점(노랑)을 그린다.
 */
UCLASS()
class KNIGHT_API UknightGameplayAbility_MonsterDiveAttack : public UknightGameplayAbility_MonsterAttack
{
	GENERATED_BODY()

public:
	UknightGameplayAbility_MonsterDiveAttack(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	/** FoldSection → LoopSection(자기 자신으로 반복) 몽타주. */
	UPROPERTY(EditDefaultsOnly, Category="Attack")
	TObjectPtr<UAnimMontage> Montage;

	UPROPERTY(EditDefaultsOnly, Category="Attack")
	FName FoldSection = TEXT("Fold");

	UPROPERTY(EditDefaultsOnly, Category="Attack")
	FName LoopSection = TEXT("Loop");

	/** 맞히거나 막혔을 때: 땅에 박혔다 빠져나온다. */
	UPROPERTY(EditDefaultsOnly, Category="Attack")
	TObjectPtr<UAnimMontage> StuckMontage;

	/** StuckMontage를 이 시각(초)부터 재생한다. 앞부분이 다른 동작이면 건너뛴다. */
	UPROPERTY(EditDefaultsOnly, Category="Attack", meta=(ClampMin=0, Units="s"))
	float StuckStartTime = 0.f;

	/** 피했거나 퍼펙트 가드당했을 때: 쓰러졌다 일어난다. */
	UPROPERTY(EditDefaultsOnly, Category="Attack")
	TObjectPtr<UAnimMontage> FallMontage;

	/** FallMontage를 이 시각(초)부터 재생한다. 앞부분이 급강하 자세를 붙잡고 있으면 건너뛴다. */
	UPROPERTY(EditDefaultsOnly, Category="Attack", meta=(ClampMin=0, Units="s"))
	float FallStartTime = 0.f;

	/**
	 * 선택 사항. FallMontage가 블렌드 아웃을 시작하면 이어서 재생해 쓰러진 자세에서 블렌드로 일어난다.
	 * 한 몽타주 안에서 애니를 이어 붙이면 이음매에서 자세가 끊겨 튀므로 일어나기는 따로 둔다.
	 */
	UPROPERTY(EditDefaultsOnly, Category="Attack")
	TObjectPtr<UAnimMontage> FallGetUpMontage;

	/** 퍼펙트 가드당하면 그 자리에서 돌진을 끊고 FallMontage로 넘어간다. 아직 공중이면 이 시간 동안 바닥 높이로 내린다. */
	UPROPERTY(EditDefaultsOnly, Category="Attack", meta=(ClampMin=0, Units="s"))
	float ParriedDropTime = 0.3f;

	UPROPERTY(EditDefaultsOnly, Category="Attack")
	FknightMeleeHitParams HitParams;

	/** 날개를 접는 시간. 이 동안 타깃 쪽으로 돌고, 끝나는 순간 방향을 고정한다. */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Dive", meta=(ClampMin=0, Units="s"))
	float FoldTime = 1.1f;

	UPROPERTY(EditDefaultsOnly, Category="Attack|Dive", meta=(ClampMin=0, Units="deg"))
	float FoldTurnRate = 240.f;

	UPROPERTY(EditDefaultsOnly, Category="Attack|Dive", meta=(ClampMin=1, Units="cm/s"))
	float DiveSpeed = 3000.f;

	/**
	 * 고정한 타깃 위치에서 머리가 이만큼 더 지나가도록 몸(캡슐)의 도착 지점을 잡는다. 접은 자세의 머리는 몸보다 HeadReach만큼
	 * 앞에 있으므로 도착 지점 = 타깃 위치 + 방향 x (Overshoot - HeadReach).
	 */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Dive", meta=(Units="cm"))
	float Overshoot = 200.f;

	UPROPERTY(EditDefaultsOnly, Category="Attack|Dive", meta=(ClampMin=0, Units="cm"))
	float HeadReach = 300.f;

	/** 돌진 시간의 최솟값. 아주 가까워도 이만큼은 내려온다. */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Dive", meta=(ClampMin=0.05, Units="s"))
	float MinDiveTime = 0.3f;

	/** 끝난 뒤 공중 높이로 다시 오르는 시간. */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Dive", meta=(ClampMin=0, Units="s"))
	float RiseTime = 1.2f;

	UPROPERTY(EditDefaultsOnly, Category="Attack|Hit")
	FName HitSocket = TEXT("head");

	UPROPERTY(EditDefaultsOnly, Category="Attack|Hit")
	FVector HitOffset = FVector(100.f, 0.f, 0.f);

	UPROPERTY(EditDefaultsOnly, Category="Attack|Hit", meta=(ClampMin=1, Units="cm"))
	float HitRadius = 250.f;

private:
	UFUNCTION()
	void HandleTick(int32 ActionNumber);

	UFUNCTION()
	void HandleOutcomeFinished();

	UFUNCTION()
	void HandleFallBlendOut();

	void StartDive();
	void FinishDive();
	void ApplyHits();

	enum class EPhase : uint8 { Fold, Dive, Outcome };

	TWeakObjectPtr<UAbilityTask_Repeat> TickTask;
	TWeakObjectPtr<UAbilityTask_ApplyRootMotionConstantForce> DiveTask;
	TSet<TWeakObjectPtr<AActor>> HitActors;
	EPhase Phase = EPhase::Fold;
	float PhaseTime = 0.f;
	float DiveDuration = 0.f;
	/** 맞히거나 가드로 막혔다(땅에 박힘). */
	bool bConnected = false;
	bool bParried = false;
	bool bNoFlinchApplied = false;
	bool bFallGetUpStarted = false;
};
