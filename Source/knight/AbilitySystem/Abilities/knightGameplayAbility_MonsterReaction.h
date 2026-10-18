#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/knightGameplayAbility.h"
#include "knightGameplayAbility_MonsterReaction.generated.h"

class ACharacter;
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
 *
 * 되감기(패링 반동): RewindTime이 있으면 유지 구간 전에 끊긴 공격을 그만큼 거꾸로 재생해 "휘두르던 팔이 튕겨 나가는"
 * 모습을 보여 준다. 공격 몽타주가 아니라 그 아래 애니메이션 시퀀스를 재생하므로 몽타주에 단 판정·소리 노티파이는 다시
 * 켜지지 않는다. 몽타주 시간으로 재므로 패리 히트스톱(CustomTimeDilation)이 끝난 뒤에 보인다.
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

	/**
	 * 시작할 때 다른 반응(Ability.HitReact)을 취소한다. 다운처럼 다른 반응보다 우선하는 반응에 켠다. 패링으로 Poise가 깨지면
	 * 패리 경직과 다운이 같은 프레임에 켜지는데, 이게 없으면 나중에 끝나는 쪽의 몽타주가 다른 쪽을 덮어 자세가 튄다.
	 */
	UPROPERTY(EditDefaultsOnly, Category="Reaction")
	bool bCancelOtherReactions = false;

	/** RecoverMontage가 설정되지 않았을 때 쓴다. */
	UPROPERTY(EditDefaultsOnly, Category="Reaction", meta=(ClampMin=0, Units="s"))
	float RecoverTime = 1.f;

	UPROPERTY(EditDefaultsOnly, Category="Reaction|Presentation")
	TObjectPtr<UAnimMontage> HoldMontage;

	UPROPERTY(EditDefaultsOnly, Category="Reaction|Presentation")
	TObjectPtr<UAnimMontage> RecoverMontage;

	/**
	 * 반응이 시작될 때 끊긴 공격 애니메이션을 이만큼(애니메이션 시간) 되감는다. 0이면 끈다. 유지 구간(HoldTime)은 되감기가
	 * 끝난 뒤 시작하므로 반응 전체가 RewindTime / RewindRate만큼 길어진다.
	 */
	UPROPERTY(EditDefaultsOnly, Category="Reaction|Rewind", meta=(ClampMin=0, Units="s"))
	float RewindTime = 0.f;

	/** 되감는 속도(재생 속도 배수). */
	UPROPERTY(EditDefaultsOnly, Category="Reaction|Rewind", meta=(ClampMin=0.1, EditCondition="RewindTime > 0"))
	float RewindRate = 1.f;

	/** 반응이 정상적으로 끝나면 Poise를 다시 채운다(다운용). */
	UPROPERTY(EditDefaultsOnly, Category="Reaction|Poise")
	bool bRestorePoise = false;

	/** 매번 MaxPoise가 이 비율만큼 늘어나므로 다음 다운일수록 더 많이 몰아붙여야 한다(몬스터 헌터 방식). */
	UPROPERTY(EditDefaultsOnly, Category="Reaction|Poise", meta=(EditCondition="bRestorePoise", ClampMin=0, ClampMax=2))
	float MaxPoiseGrowth = 0.25f;

private:
	/** 끊긴 공격 몽타주를 찾아 되감기를 시작한다. 되감을 것이 없으면 false. */
	bool TryStartRewind(ACharacter& Character);

	UFUNCTION()
	void HandleRewindDelayFinished();

	UFUNCTION()
	void HandleRewindFinished();

	/** 유지 몽타주와 유지 시간을 시작한다. 되감기 끝 알림이 여러 번 와도 한 번만 시작한다. */
	void StartHold();

	UFUNCTION()
	void HandleHoldFinished();

	UFUNCTION()
	void HandleRecoverFinished();

	void RestorePoise() const;

	bool bStateTagApplied = false;
	bool bHoldStarted = false;
};
