#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/knightGameplayAbility.h"
#include "Combat/knightCombatTypes.h"
#include "Input/knightMovePressTracker.h"
#include "knightGameplayAbility_MeleeAttack.generated.h"

class ACharacter;
class UAnimMontage;

/**
 * One melee attack: plays a montage, and turns every Event.Melee.Hit from the weapon trace into a
 * resolved hit through UknightCombatLibrary::ApplyMeleeHit.
 *
 * The montage carries the timing: Knight Melee Trace notifies for the active frames, and Knight Gameplay
 * Tag Window notifies for Window.Combo and Window.Cancel. A combo is a chain of these abilities; the next
 * step activates from the input buffer when the combo window opens and replaces this one.
 *
 * Chaining steps that share one InputTag: give step N an ActivationOwnedTag (for example Combo.Greatsword.1,
 * defined in Config/Tags), make step N+1 require it (ActivationRequiredTags), and make the first step
 * block on the whole Combo.Greatsword family (ActivationBlockedTags). Only the correct next step can then
 * activate for the press.
 *
 * The WarpTargetName warp target is set before the montage starts (the locked target, or without a lock the
 * closest enemy in the attack direction), so root motion inside a Motion Warping notify window steers into it.
 *
 * Drawing: with DrawMontage set and the weapon not in the hand (WeaponHandSocket), DrawMontage plays instead (its
 * Knight Attach Weapon notify moves the sword to the hand). With bAttackAfterDraw the attack follows at
 * DrawAttackStartTime; otherwise the press only draws, and moving once the sword is in hand ends the draw early.
 *
 * Movement cancels the recovery: while Window.Cancel is open, any movement input stops the montage
 * (RecoveryCancelBlendOut) and ends the attack, so the Knight walks off instead of finishing the follow-through.
 *
 * (2026-10-07) 이동 캔슬은 '새로 누른' 이동 입력에만 반응한다. 공격 전부터 누르고 있던 방향키는 무시하고,
 * 뗐다가 다시 누르거나 누른 채로 다른 키를 더 눌러 방향이 크게 바뀌면(W → W+D 등) 캔슬한다.
 * 방향키를 누른 채 공격하면 이동 캔슬 없이 후딜까지 재생되어, 이동-공격이 빠르게 반복되지 않는다.
 * 또 공격·가드·대시 입력이 버퍼에 남아 있으면 이동 캔슬보다 그 입력이 우선이다.
 *
 * (2026-10-08) 행동 속도(ActionSpeed): 공격·발도 몽타주는 PlayRate / DrawPlayRate에 행동 속도를 곱한 배속으로 재생한다.
 * 캔슬·콤보 창과 공격 판정은 몽타주 노티파이라 같이 빨라지고, 돌진 시간과 발도 후 공격 시작 시점도 같은 배속을 따른다.
 * 몽타주가 끝난 뒤의 콤보 유예(ComboGraceTime)는 입력 여유라서 그대로 둔다.
 */
UCLASS()
class KNIGHT_API UknightGameplayAbility_MeleeAttack : public UknightGameplayAbility
{
	GENERATED_BODY()

public:
	UknightGameplayAbility_MeleeAttack(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

	/** 검을 뽑기만 하는 누름이면 행동 에너지를 쓰지 않는다. (2026-10-09) */
	virtual void ApplyCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo) const override;

protected:
	/** Called after each resolved hit. Override for on-hit effects (gauge gain, combo counters). */
	virtual void OnHitResolved(const FHitResult& Hit, EknightHitOutcome Outcome) {}

	/** 양손 스탠스(State.Stance.TwoHanded)의 공격 몽타주. OneHandedAttackMontage가 비어 있으면 한손에서도 이것을 쓴다. */
	UPROPERTY(EditDefaultsOnly, Category="Attack")
	TObjectPtr<UAnimMontage> AttackMontage;

	/** (2026-10-15) 한손 스탠스의 공격 몽타주. 검을 뽑으면 한손이고, Q(스탠스 전환)로 양손과 바꾼다. */
	UPROPERTY(EditDefaultsOnly, Category="Attack")
	TObjectPtr<UAnimMontage> OneHandedAttackMontage;

	UPROPERTY(EditDefaultsOnly, Category="Attack", meta=(ClampMin=0.01))
	float PlayRate = 1.f;

	UPROPERTY(EditDefaultsOnly, Category="Attack")
	FName StartSection = NAME_None;

	UPROPERTY(EditDefaultsOnly, Category="Attack")
	FknightMeleeHitParams HitParams;

	/** Played first when the weapon is not in the hand (sword on the back). Leave empty to draw instantly. */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Draw")
	TObjectPtr<UAnimMontage> DrawMontage;

	UPROPERTY(EditDefaultsOnly, Category="Attack|Draw", meta=(ClampMin=0.01))
	float DrawPlayRate = 1.f;

	/** False: the first press with the sword on the back only draws it. True: the attack follows the draw. */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Draw")
	bool bAttackAfterDraw = false;

	/** Montage time (s) at which the attack takes over from the draw, so the settle at its end is skipped. 0: when it blends out. */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Draw", meta=(ClampMin=0, Units="s"))
	float DrawAttackStartTime = 0.f;

	/** The weapon counts as drawn while it is attached to this socket. */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Draw")
	FName WeaponHandSocket = TEXT("weapon_r");

	UPROPERTY(EditDefaultsOnly, Category="Attack|Draw")
	FName WeaponComponentTag = TEXT("Weapon");

	UPROPERTY(EditDefaultsOnly, Category="Attack|Motion Warping")
	FName WarpTargetName = TEXT("AttackTarget");

	/** How far the warp may pull the Knight toward the locked target. */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Motion Warping", meta=(ClampMin=0, Units="cm"))
	float MaxWarpDistance = 300.f;

	/**
	 * (2026-10-14) 코드 돌진(StartAttackLunge)을 쓸지. 공격 애니에 앞으로 가는 루트 모션이 있으면 끈다.
	 * 루트 모션 + 모션 워핑 위에 코드 돌진까지 겹치면 이동이 두 번 들어가 순간 이동처럼 미끄러진다.
	 * 제자리 공격 애니(루트 모션 없음)일 때만 켠다.
	 */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Motion Warping")
	bool bLungeWithCode = false;

	/** The lunge toward the warp location takes at most this long and ends with the warp window (the first trace). */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Motion Warping", meta=(ClampMin=0.05, Units="s", EditCondition="bLungeWithCode"))
	float MaxLungeTime = 0.35f;

	/**
	 * (2026-10-14) 공격 중 애니 루트 모션 이동 거리 배율(ACharacter::AnimRootMotionTranslationScale).
	 * 1 = 애니 그대로, 0.5 = 절반만 나아간다. 공격이 끝나면 1로 되돌린다.
	 */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Motion Warping", meta=(ClampMin=0, ClampMax=2))
	float RootMotionTranslationScale = 1.f;

	/** The warp stops this far in front of the target's surface (or part point), so the blade lands instead of the body. */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Motion Warping", meta=(ClampMin=0, Units="cm"))
	float WarpStopDistance = 150.f;

	/** Without a lock, steer into the closest enemy in the attack direction. */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Motion Warping")
	bool bUseSoftTarget = true;

	UPROPERTY(EditDefaultsOnly, Category="Attack|Motion Warping", meta=(ClampMin=0, Units="cm", EditCondition="bUseSoftTarget"))
	float SoftTargetRange = 500.f;

	UPROPERTY(EditDefaultsOnly, Category="Attack|Motion Warping", meta=(ClampMin=1, ClampMax=180, Units="deg", EditCondition="bUseSoftTarget"))
	float SoftTargetAngle = 75.f;

	/**
	 * Movement input during Window.Cancel stops the montage and ends the attack.
	 * (2026-10-14) ASC의 bOnlyDashCancelsAttackRecovery가 켜져 있으면(공격 후딜은 퀵스텝만) 이동으로는 끊지 않는다.
	 */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Recovery")
	bool bCancelRecoveryOnMove = true;

	/**
	 * Also cancel while Window.Combo is open. True: movement right after the swing walks off at once, but holding
	 * a direction while mashing attack can break the combo. False: movement only cancels after the combo window.
	 */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Recovery", meta=(EditCondition="bCancelRecoveryOnMove"))
	bool bMoveCancelInComboWindow = true;

	/** Blend out used when movement cancels the recovery. */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Recovery", meta=(ClampMin=0, Units="s", EditCondition="bCancelRecoveryOnMove"))
	float RecoveryCancelBlendOut = 0.25f;

	/**
	 * 공격이 캔슬 없이 끝난 뒤에도 콤보 단계 태그(ActivationOwnedTags, 예: Combo.Greatsword.1)를 남겨 두는 시간.
	 * 이 안에 누르면 1타로 돌아가지 않고 다음 단계가 나간다. 공격은 몽타주 블렌드 아웃이 시작될 때 끝나므로, 그 직후의 늦은 입력을 받아 준다.
	 * (2026-10-07 플레이 피드백으로 0.05초 추가)
	 */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Combo", meta=(ClampMin=0, Units="s"))
	float ComboGraceTime = 0.05f;

	/**
	 * (2026-10-15) 콤보 이어받기: 다른 공격 뒤에 이 공격이 이어지게 한다. 이 태그 중 하나가 있으면(앞 공격의 콤보 단계 태그)
	 * ActivationBlockedTags 대신 ComboLinkBlockedTags로 막힘을 본다. 필요한 태그·다른 어빌리티가 막는 태그는 그대로 본다.
	 * 예: 1타(GA_Knight_Attack_1) = Combo.Greatsword.Run(대쉬공격), Combo.Greatsword.4(4타).
	 *     1타는 평소 Combo.Greatsword 계열과 달리기 중(State.Sprinting)에 막혀서, 이것 없이는 대쉬공격 직후 Shift를 누른 채로는 이어지지 않는다.
	 * 비우면 지금과 같다.
	 */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Combo")
	FGameplayTagContainer ComboLinkFromTags;

	/** 콤보 이어받기 중에 대신 보는 막는 태그. 예: 1타 = 원래 목록에서 콤보 계열·달리기·먼 락온만 뺀 것(사망·피격·공중·반격 대기). */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Combo")
	FGameplayTagContainer ComboLinkBlockedTags;

	/** 콤보 이어받기(ComboLinkFromTags)가 걸려 있으면 막는 태그를 ComboLinkBlockedTags로 바꿔 본다. 아니면 기본 검사. */
	virtual bool DoesAbilitySatisfyTagRequirements(const UAbilitySystemComponent& AbilitySystemComponent,
		const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr,
		FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

private:
	/** ComboGraceTime 동안 콤보 단계 태그를 루즈 태그로 남긴다. */
	void KeepComboStepTags();

	UFUNCTION()
	void OnMontageFinished();

	UFUNCTION()
	void OnMeleeHitEvent(FGameplayEventData Payload);

	UFUNCTION()
	void OnDrawFinished();

	UFUNCTION()
	void OnDrawInterrupted();

	void StartAttack();
	/** 지금 스탠스에 맞는 공격 몽타주. */
	UAnimMontage* GetStanceAttackMontage() const;
	void CheckDrawMoveCancel();
	bool IsWeaponInHand() const;
	void CheckMoveCancel();

	FTimerHandle MoveCancelTimer;
	FTimerHandle DrawTimer;
	bool bAttackStarted = false;

	/** 이 공격이 루트 모션 배율을 바꿨는지. 끝날 때 1로 되돌린다. */
	bool bRootMotionScaleApplied = false;

	/** 이번 발동이 검을 뽑기만 하는지(공격 없음). 비용을 내기 전에 정한다. */
	bool bDrawOnlyActivation = false;

	/** 이번 공격 중 이동 키를 새로 눌렀는지. 공격 전부터 누르고 있던 방향은 무시한다. (2026-10-08 가드와 같이 쓰려고 공용 구조체로 옮김) */
	FknightMovePressTracker MovePress;
};
