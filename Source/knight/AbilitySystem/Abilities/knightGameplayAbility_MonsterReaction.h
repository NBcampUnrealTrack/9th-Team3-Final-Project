#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/knightGameplayAbility.h"
#include "knightGameplayAbility_MonsterReaction.generated.h"

class UAnimMontage;

/**
 * 공격을 끊는 시간제 몬스터 반응. 유지 구간(다운, 포효, 경직)을 거친 뒤 회복한다.
 * 클래스는 하나이고, 트리거와 타이밍만 다른 Blueprint 하위 클래스 여러 개로 쓴다.
 *   다운          Triggers: Event.PoiseBroken                     StateTag State.HitReact.Knockdown, bRestorePoise
 *   부위 파괴 경직 Triggers: Event.PartBroken                      StateTag State.HitReact.Stagger
 *   분노 포효     Triggers: State.Monster.Enraged (When Tag Is Added)  StateTag State.SuperArmor
 *
 * 실행 중에는 몬스터에 State.Monster.Reacting과 StateTag가 붙고 공격이 취소·차단된다. StateTree는
 * bIsReacting(Knight Monster Target)이 true인 동안 반응 상태에서 기다려야 한다. 이 어빌리티는 현재 기술을
 * 멈출 뿐 StateTree를 붙잡아 두지는 않는다.
 *
 * 타이밍: HoldMontage를 바로 재생하고(다운 루프가 필요하면 루프 섹션을 둔다) 유지 구간은 HoldTime 동안
 * 이어진다. 그 뒤 RecoverMontage를 재생하고, 없으면 RecoverTime만큼 기다린다. 두 몽타주 모두 선택 사항이라
 * 스켈레톤이 없는 대역도 타이밍만으로 반응한다.
 */
UCLASS()
class KNIGHT_API UknightGameplayAbility_MonsterReaction : public UknightGameplayAbility
{
	GENERATED_BODY()

public:
	UknightGameplayAbility_MonsterReaction(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	/** 반응 내내 붙어 있는 루즈 태그. StateTree와 공격 차단이 이 태그를 읽는다. */
	UPROPERTY(EditDefaultsOnly, Category="Reaction", meta=(Categories="State"))
	FGameplayTag StateTag;

	UPROPERTY(EditDefaultsOnly, Category="Reaction", meta=(ClampMin=0, Units="s"))
	float HoldTime = 4.f;

	/** RecoverMontage가 설정되지 않았을 때 쓴다. */
	UPROPERTY(EditDefaultsOnly, Category="Reaction", meta=(ClampMin=0, Units="s"))
	float RecoverTime = 1.f;

	UPROPERTY(EditDefaultsOnly, Category="Reaction|Presentation")
	TObjectPtr<UAnimMontage> HoldMontage;

	UPROPERTY(EditDefaultsOnly, Category="Reaction|Presentation")
	TObjectPtr<UAnimMontage> RecoverMontage;

	/** 반응이 정상적으로 끝나면 Poise를 다시 채운다(다운용). */
	UPROPERTY(EditDefaultsOnly, Category="Reaction|Poise")
	bool bRestorePoise = false;

	/** 매번 MaxPoise가 이 비율만큼 늘어나므로 다음 다운일수록 더 많이 몰아붙여야 한다(몬스터 헌터 방식). */
	UPROPERTY(EditDefaultsOnly, Category="Reaction|Poise", meta=(EditCondition="bRestorePoise", ClampMin=0, ClampMax=2))
	float MaxPoiseGrowth = 0.25f;

private:
	UFUNCTION()
	void HandleHoldFinished();

	UFUNCTION()
	void HandleRecoverFinished();

	void RestorePoise() const;

	bool bStateTagApplied = false;
};
