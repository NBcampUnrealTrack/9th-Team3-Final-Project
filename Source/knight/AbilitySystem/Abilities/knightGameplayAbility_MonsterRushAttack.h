#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/knightGameplayAbility_MonsterAttack.h"
#include "Combat/knightCombatTypes.h"
#include "knightGameplayAbility_MonsterRushAttack.generated.h"

class UAbilityTask_Repeat;
class UAnimMontage;

/** 몰아치기 한 동작. 제자리 애니에서 몸이 크게 움직이는 동작(회전 등)도 ContactPoint로 타깃을 맞춘다. */
USTRUCT(BlueprintType)
struct KNIGHT_API FknightMonsterRushStep
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rush")
	TObjectPtr<UAnimMontage> Montage;

	/** 이 동작에서 타깃에 닿는 시각(몽타주 초). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rush", meta=(ClampMin=0, Units="s"))
	float ContactTime = 1.f;

	/**
	 * ContactTime에 HitBone이 있는 곳(메시 공간, 스케일 적용 전). 동작 전에 이 지점이 타깃 위치에 오도록 몸을 돌리고 옮긴다.
	 * 애니 뼈 위치를 재서 넣는다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rush")
	FVector2D ContactPoint = FVector2D(100.f, 0.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rush")
	FName HitBone = TEXT("spine_02");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rush", meta=(ClampMin=1, Units="cm"))
	float HitRadius = 400.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rush", meta=(ClampMin=0, Units="s"))
	float HitStartTime = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rush", meta=(ClampMin=0, Units="s"))
	float HitEndTime = 1.f;

	/** 이 시각(몽타주 초)에 다음 동작으로 넘어간다. 0이면 몽타주 끝까지 재생한다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rush", meta=(ClampMin=0, Units="s"))
	float EndTime = 0.f;
};

/**
 * 공중 턴의 날짐승이 바닥 가까이 내려와 여러 동작을 빠르게 이어 치는 공격(잔향 몰아치기, 공명해방 전용).
 *
 *   동작마다: 자리 잡기 → 동작 재생. 자리 잡기는 그 동작의 ContactPoint가 타깃 위치에 오도록 RepositionSpeed로 날아가며 돈다
 *             (RepositionMinTime ~ RepositionMaxTime). 동작 중에는 제자리에서 HitBone 구체로 때린다(동작마다 한 번씩).
 *   시작할 때 바닥 높이로 내려오고, 끝나면 다시 공중 높이로 떠오른다.
 *
 * 몰아치는 동안 State.Monster.NoFlinch를 붙여 패링 경직이 끊지 않는다. 그로기 게이지가 다 차면 다운이 끊는다(격추).
 * 디버그: knight.Monster.DrawAttacks 1 이면 판정 구체를 그린다.
 */
UCLASS()
class KNIGHT_API UknightGameplayAbility_MonsterRushAttack : public UknightGameplayAbility_MonsterAttack
{
	GENERATED_BODY()

public:
	UknightGameplayAbility_MonsterRushAttack(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	UPROPERTY(EditDefaultsOnly, Category="Attack")
	TArray<FknightMonsterRushStep> Steps;

	UPROPERTY(EditDefaultsOnly, Category="Attack")
	FknightMeleeHitParams HitParams;

	UPROPERTY(EditDefaultsOnly, Category="Attack|Reposition", meta=(ClampMin=1, Units="cm/s"))
	float RepositionSpeed = 3000.f;

	UPROPERTY(EditDefaultsOnly, Category="Attack|Reposition", meta=(ClampMin=0, Units="s"))
	float RepositionMinTime = 0.25f;

	UPROPERTY(EditDefaultsOnly, Category="Attack|Reposition", meta=(ClampMin=0, Units="s"))
	float RepositionMaxTime = 0.8f;

	/** 시작할 때 바닥 높이로 내려오는 시간. */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Hover", meta=(ClampMin=0, Units="s"))
	float DescendTime = 0.5f;

	/** 끝난 뒤 공중 높이로 떠오르는 시간. */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Hover", meta=(ClampMin=0, Units="s"))
	float RiseTime = 1.f;

private:
	UFUNCTION()
	void HandleTick(int32 ActionNumber);

	void StartReposition();
	void StartStep();
	void ApplyHits(const FknightMonsterRushStep& Step);
	void SetNoFlinch(bool bOn);

	enum class EPhase : uint8 { Reposition, Step };

	TWeakObjectPtr<UAbilityTask_Repeat> TickTask;
	TSet<TWeakObjectPtr<AActor>> HitActors;
	EPhase Phase = EPhase::Reposition;
	float PhaseTime = 0.f;
	float PhaseDuration = 0.f;
	int32 StepIndex = 0;
	float StartYaw = 0.f;
	float TargetYaw = 0.f;
	bool bNoFlinchApplied = false;
};
