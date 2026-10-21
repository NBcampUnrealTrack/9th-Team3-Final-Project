#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "knightMonsterDefinition.generated.h"

class UAnimMontage;
class UGameplayEffect;
class UknightAbilitySet;
class UknightAbilityTagRelationshipMapping;
class UknightMonsterPartData;
class UNiagaraSystem;

/**
 * Everything that defines a monster species, separate from its Blueprint (mesh, anim BP, StateTree).
 * Starting attributes (Health, AttackPower, Poise, Rage limits) come from an init effect in an ability set.
 */
UCLASS(BlueprintType, Const)
class KNIGHT_API UknightMonsterDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Monster")
	FText DisplayName;

	/** Attack moves, reactions and the attribute init effect. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Abilities")
	TArray<TObjectPtr<const UknightAbilitySet>> AbilitySets;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Abilities")
	TObjectPtr<UknightAbilityTagRelationshipMapping> TagRelationshipMapping;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Parts")
	TObjectPtr<const UknightMonsterPartData> PartData;

	/** Applied when Rage fills: grants State.Monster.Enraged for a duration and resets Rage. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="States")
	TSubclassOf<UGameplayEffect> EnrageEffect;

	/** Applied when Stamina runs out: grants State.Monster.Exhausted for a duration and restores Stamina. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="States")
	TSubclassOf<UGameplayEffect> ExhaustEffect;

	/**
	 * 퍼펙트 가드를 당할 때마다 깎이는 경직도. 최대 경직도에 대한 비율(0.25 = 4번에 다운). 0이면 깎지 않는다.
	 * 일반 타격의 경직도 피해와 같은 게이지를 쓰므로 섞어서 쌓인다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="States", meta=(ClampMin=0, ClampMax=1))
	float ParriedPoiseDamageRatio = 0.f;

	/**
	 * 몬스터에 이 태그 중 하나라도 있으면 퍼펙트 가드를 당해도 경직도가 깎이지 않는다(잔향은 공명해방 중 그로기 없음).
	 * 태그가 붙는 순간 경직도를 가득 채우므로, 태그가 떨어진 뒤에는 게이지가 처음부터 다시 쌓인다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="States", meta=(Categories="State"))
	FGameplayTagContainer ParriedPoiseBlockedTags;

	/**
	 * 체력이 이 비율 밑으로 내려가면 State.Monster.Phase2를 붙인다(한 생에 한 번, 떼지 않는다). 0이면 2페이즈가 없다.
	 * 다운·경직·포효 중에 넘으면 그 반응이 끝난 뒤에 붙인다. 전환 포효는 이 태그를 트리거로 쓰는 반응 어빌리티로 만든다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="States", meta=(ClampMin=0, ClampMax=1))
	float Phase2HealthRatio = 0.f;

	/** Rough radius for lock-on camera framing. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Targeting", meta=(ClampMin=0, Units="cm"))
	float TargetRadius = 600.f;

	/** Lock-on point when PartData defines none. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Targeting")
	FName FallbackLockOnSocket = TEXT("spine_03");

	/**
	 * 공격을 고를 때 바로 전에 쓴 공격은 후보에서 뺀다. 그 공격밖에 쓸 수 없을 때(거리·각도·쿨다운)만 다시 쓴다.
	 * Knight Activate Ability 태스크가 본다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AI")
	bool bAvoidRepeatingLastAttack = false;

	/** 공용 StateTree(ST_Monster_Default)의 추격이 멈추는 거리. 몸 크기에 맞춘다. Knight Monster Target 평가기가 내보낸다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AI", meta=(ClampMin=0, Units="cm"))
	float ChaseStopDistance = 900.f;

	/** 공용 StateTree가 원거리 공격(Ability.Attack.Ranged)으로 넘어가는 거리 구간. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AI", meta=(ClampMin=0, Units="cm"))
	float RangedMinDistance = 533.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AI", meta=(ClampMin=0, Units="cm"))
	float RangedMaxDistance = 2000.f;

	/**
	 * 제자리 회전 몽타주(루트 모션으로 90° 도는 애니메이션). Knight Face Target이 타깃이 TurnMontageMinAngle 이상 벗어났을 때
	 * 재생하고, MotionWarping으로 TurnWarpStartTime~TurnWarpEndTime 구간의 회전량을 실제 각도에 맞춘다. 비어 있으면 코드 회전만 쓴다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AI|Turn")
	TObjectPtr<UAnimMontage> TurnLeftMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AI|Turn")
	TObjectPtr<UAnimMontage> TurnRightMontage;

	/** 이 각도보다 작게 벗어나면 몽타주 없이 코드로 돈다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AI|Turn", meta=(ClampMin=0, ClampMax=180, Units="deg"))
	float TurnMontageMinAngle = 45.f;

	/** 몽타주 한 번에 도는 최대 각도. 더 남으면 몽타주를 한 번 더 재생한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AI|Turn", meta=(ClampMin=0, ClampMax=180, Units="deg"))
	float TurnMontageMaxAngle = 135.f;

	/** 회전 몽타주에서 몸이 실제로 도는 구간(초). 이 구간의 회전량을 늘리거나 줄인다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AI|Turn", meta=(ClampMin=0, Units="s"))
	float TurnWarpStartTime = 0.45f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AI|Turn", meta=(ClampMin=0, Units="s"))
	float TurnWarpEndTime = 1.45f;

	/**
	 * 회전 몽타주로 도는 최대 속도(도/초). 회전 구간 길이는 같으므로 큰 각도일수록 빨리 돌게 되는데, 이 속도를 넘으면 몽타주를
	 * 느리게 재생해 발 디딤이 회전을 따라가게 한다(빨리 재생하지는 않는다). 0이면 제한하지 않는다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AI|Turn", meta=(ClampMin=0, Units="DegreesPerSecond"))
	float TurnMontageMaxSpeed = 0.f;

	/**
	 * 등장할 때 셰이더 파이프라인(PSO)을 미리 준비해 둘 이펙트(브레스, 땅울림 먼지). 처음 쓰일 때 셰이더를 즉석에서 만드느라
	 * 한 프레임이 튀는 것을 막는다. 몽타주 노티파이로 처음 등장하는 Niagara를 넣는다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Presentation")
	TArray<TObjectPtr<UNiagaraSystem>> PrewarmEffects;
};
