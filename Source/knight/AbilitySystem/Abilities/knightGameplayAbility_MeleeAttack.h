#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/knightGameplayAbility.h"
#include "Combat/knightCombatTypes.h"
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
 * With a lock-on target, the WarpTargetName warp target is set before the montage starts, so root motion
 * with a Motion Warping notify steers into the target.
 *
 * Movement cancels the recovery: while Window.Cancel is open, any movement input stops the montage
 * (RecoveryCancelBlendOut) and ends the attack, so the Knight walks off instead of finishing the follow-through.
 *
 * (2026-10-07) 이동 캔슬은 '새로 누른' 이동 입력에만 반응한다. 공격 전부터 누르고 있던 방향키는 무시하고,
 * 뗐다가 다시 누르거나 누른 채로 다른 키를 더 눌러 방향이 크게 바뀌면(W → W+D 등) 캔슬한다.
 * 방향키를 누른 채 공격하면 이동 캔슬 없이 후딜까지 재생되어, 이동-공격이 빠르게 반복되지 않는다.
 * 또 공격·가드·대시 입력이 버퍼에 남아 있으면 이동 캔슬보다 그 입력이 우선이다.
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

protected:
	/** Called after each resolved hit. Override for on-hit effects (gauge gain, combo counters). */
	virtual void OnHitResolved(const FHitResult& Hit, EknightHitOutcome Outcome) {}

	UPROPERTY(EditDefaultsOnly, Category="Attack")
	TObjectPtr<UAnimMontage> AttackMontage;

	UPROPERTY(EditDefaultsOnly, Category="Attack", meta=(ClampMin=0.01))
	float PlayRate = 1.f;

	UPROPERTY(EditDefaultsOnly, Category="Attack")
	FName StartSection = NAME_None;

	UPROPERTY(EditDefaultsOnly, Category="Attack")
	FknightMeleeHitParams HitParams;

	UPROPERTY(EditDefaultsOnly, Category="Attack|Motion Warping")
	FName WarpTargetName = TEXT("AttackTarget");

	/** How far the warp may pull the Knight toward the locked target. */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Motion Warping", meta=(ClampMin=0, Units="cm"))
	float MaxWarpDistance = 300.f;

	/** Movement input during Window.Cancel stops the montage and ends the attack. */
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

private:
	/** ComboGraceTime 동안 콤보 단계 태그를 루즈 태그로 남긴다. */
	void KeepComboStepTags();

	UFUNCTION()
	void OnMontageFinished();

	UFUNCTION()
	void OnMeleeHitEvent(FGameplayEventData Payload);

	void CheckMoveCancel();

	/** 이동 입력을 컨트롤 회전 기준 2D 방향으로 읽는다(카메라를 돌려도 같은 키는 같은 방향). 입력이 없으면 영벡터. */
	static FVector2D GetLocalMoveInput(const ACharacter& Character);

	/** 이번 공격에서 이동 키를 새로 눌렀는지 갱신한다. 공격 전부터 누르고 있던 방향은 무시한다. */
	void UpdateFreshMovePress(const ACharacter& Character);

	FTimerHandle MoveCancelTimer;

	/** 새 누름 판정 기준 방향: 공격 시작 때 누르고 있던 방향. 키를 떼면 0이 된다. */
	FVector2D MoveInputBaseline = FVector2D::ZeroVector;

	/** 이번 공격 중 이동 키를 새로 눌렀는지(뗐다가 다시 누름, 또는 누른 채로 방향을 크게 바꿈). */
	bool bFreshMovePress = false;
};
