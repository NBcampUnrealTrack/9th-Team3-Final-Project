#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/Abilities/knightGameplayAbility.h"
#include "Combat/knightMechAttackTiming.h"
#include "knightAbilitySystemComponent.generated.h"

class UknightAbilityTagRelationshipMapping;
class UCameraShakeBase;
class UFXSystemAsset;
class USoundBase;

/**
 * 그로기 자연 회복의 순수 계산. GAS 객체 없이 테스트(knightPoiseRecoveryTest.cpp)로 검증한다.
 */
namespace knightPoiseRecovery
{
	/**
	 * 한 번의 회복 단계 후의 Poise.
	 * 아래 경우에는 회복하지 않고 CurrentPoise를 그대로 돌려준다.
	 *   - 회복 속도가 0 이하
	 *   - Poise가 0 이하 (경직·다운 중. 반응이 끝날 때 따로 채워진다)
	 *   - 이미 최대치
	 *   - 마지막 피격 후 회복 대기 시간이 아직 지나지 않음
	 * 그 외에는 초당 RecoveryRate만큼, 최대치를 넘지 않게 올린다.
	 */
	KNIGHT_API float Step(float CurrentPoise, float MaxPoise, float RecoveryRate, float DeltaSeconds,
		float SecondsSinceLastHit, float RecoveryDelay);
}

/**
 * 행동 에너지(BoostGauge, 스테미너) 자연 회복의 순수 계산. 테스트: knightBoostRecoveryTest.cpp (2026-10-09)
 */
namespace knightBoostRecovery
{
	/**
	 * 한 번의 회복 단계 후의 행동 에너지.
	 * 회복 속도가 0 이하이거나, 이미 최대치이거나, 마지막으로 쓴 뒤 회복 대기 시간이 아직 지나지 않았거나,
	 * bRecoveryPaused(달리는 중, 2026-10-13)면 그대로. 멈춤은 대기 시간을 다시 세게 하지 않는다: 대기 시간이 이미 지났으면
	 * 멈춤이 풀리는 즉시 다시 회복한다.
	 * 그 외에는 초당 RecoveryRate만큼, 최대치를 넘지 않게 올린다. (그로기와 달리 0에서도 회복한다)
	 */
	KNIGHT_API float Step(float CurrentGauge, float MaxGauge, float RecoveryRate, float DeltaSeconds,
		float SecondsSinceLastSpend, float RecoveryDelay, bool bRecoveryPaused = false);
}

/**
 * (2026-10-20) 스탠스(한손/양손)별 공격 보정 배율. 1이면 공격 BP에 적힌 값 그대로.
 * 공격 어빌리티(Ability.Attack 계열)가 피해·그로기 피해·재생 속도·행동 에너지 소모에 곱한다.
 */
USTRUCT(BlueprintType)
struct KNIGHT_API FknightStanceModifiers
{
	GENERATED_BODY()

	FknightStanceModifiers() = default;
	FknightStanceModifiers(float InDamage, float InPoise, float InSpeed, float InStamina)
		: DamageScale(InDamage), PoiseDamageScale(InPoise), AttackSpeedScale(InSpeed), StaminaCostScale(InStamina)
	{
	}

	/** 피해(모션 밸류·기본 피해) 배율. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stance", meta=(ClampMin=0))
	float DamageScale = 1.f;

	/** 그로기 피해(PoiseDamage) 배율. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stance", meta=(ClampMin=0))
	float PoiseDamageScale = 1.f;

	/** 공격 모션 재생 속도 배율(공격 배속에 곱한다). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stance", meta=(ClampMin=0.1))
	float AttackSpeedScale = 1.f;

	/** 행동 에너지(StaminaCost) 소모 배율. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stance", meta=(ClampMin=0))
	float StaminaCostScale = 1.f;
};

/**
 * 프로젝트 ASC. 입력 태그로 어빌리티 발동, 활성화 그룹, 데이터 기반 태그 관계를 추가한다.
 *
 * 입력 흐름: PlayerController가 InputTag 누름·뗌을 여기로 넘기고, PostProcessInput에서 매 프레임
 * ProcessAbilityInput을 한 번 호출한다. 어빌리티는 스펙의 동적 소스 태그에 저장된 InputTag로 찾으며,
 * 이 태그는 UknightAbilitySet이 어빌리티를 부여할 때 기록한다.
 *
 * 그로기 자연 회복 (2026-10-06):
 *   마지막 피격 후 PoiseRecoveryDelay초가 지나면 초당 PoiseRecoveryRate(Core 속성)만큼 Poise가 최대치까지 찬다.
 *   플레이어와 몬스터가 모두 이 클래스를 쓰므로 같은 규칙이 적용된다.
 *   회복 속도는 각자의 초기값 GE에서, 대기 시간은 각 BP의 AbilitySystemComponent 디테일에서 정한다.
 *   회복 속도 기본값이 0이라 따로 정하지 않으면 회복하지 않는다(기존 동작과 같음).
 *
 * 행동 에너지(스테미너) 자연 회복 (2026-10-09):
 *   마지막으로 쓴 뒤 BoostRecoveryDelay초(메카 속성, 기본 1.2)가 지나면 초당 BoostRecoveryRate(메카 속성, 기본 25)만큼 찬다.
 *   (2026-10-13) 달리는 동안에는 회복이 멈춘다(bPauseBoostRecoveryWhileRunning). 달리기는 쓰는 게 아니라서 대기 시간을 다시 세지 않는다.
 *   (2026-10-16) 대기 시간이 지나도 공격이 끝나기 전(휘두르는 중·후딜·콤보 창)에는 회복을 미룬다(bHoldBoostRecoveryDuringAction).
 *   (2026-10-26) 가드를 올리고 있는 동안(State.Guarding)은 회복 속도에 GuardBoostRecoveryScale을 곱한다.
 *   가불(비용이 남은 양보다 많아 음수가 된 것)도 같은 속도로 0을 거쳐 그대로 찬다.
 *   메카 속성 세트가 있는 나이트만 해당된다.
 */
UCLASS()
class KNIGHT_API UknightAbilitySystemComponent : public UAbilitySystemComponent
{
	GENERATED_BODY()

public:
	UknightAbilitySystemComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void InitAbilityActorInfo(AActor* InOwnerActor, AActor* InAvatarActor) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// 입력
	void AbilityInputTagPressed(const FGameplayTag& InputTag);
	void AbilityInputTagReleased(const FGameplayTag& InputTag);
	void ProcessAbilityInput(float DeltaTime, bool bGamePaused);
	void ClearAbilityInput();

	/** InputTag에 연결된 비활성 어빌리티 중 첫 번째를 즉시 발동한다. 입력 버퍼가 사용한다. */
	bool TryActivateAbilityByInputTag(const FGameplayTag& InputTag);

	/** 마지막 ProcessAbilityInput 이후 InputTag에 연결된 어빌리티가 발동했으면 true. 입력 버퍼가 읽는다. */
	bool WasInputTagActivatedThisFrame(const FGameplayTag& InputTag) const { return RecentlyActivatedInputTags.HasTagExact(InputTag); }

	// 활성화 그룹
	/**
	 * Exclusive_Blocking 어빌리티는 다른 모든 배타 어빌리티를 막는다.
	 * 실행 중인 Exclusive_Replaceable 어빌리티는 Window.Cancel 안에서만,
	 * 또는 새 어빌리티가 Ability.Attack일 때 Window.Combo 안에서만 교체될 수 있다.
	 * (2026-10-14) 공격 후딜(공격 실행 중의 Window.Cancel)은 퀵스텝(Ability.Boost.Dash)만 끊을 수 있다(IsAttackRecoveryDashOnly).
	 * 콤보 창이 함께 열려 있으면 다음 공격은 지금처럼 이어진다.
	 */
	bool IsActivationGroupBlocked(EknightAbilityActivationGroup Group, const FGameplayTagContainer& AbilityTags) const;

	/**
	 * (2026-10-14) 지금 공격 후딜이라 퀵스텝만 끊을 수 있는지: bOnlyDashCancelsAttackRecovery가 켜져 있고,
	 * 공격(Ability.Attack, 교체 가능 배타 어빌리티)이 실행 중이며, Window.Cancel이 열린 지 AttackRecoveryDashOnlyTime초가 안 됐을 때.
	 * 그 뒤(검을 들어 올리는 꼬리 동작)는 예전처럼 무엇이든 공격을 끊는다.
	 * 교체 규칙 말고도 강공격 차지 시작·공격의 이동 캔슬이 이걸 보고 멈춘다.
	 */
	bool IsAttackRecoveryDashOnly() const;

	/** (2026-10-23) 지금 후딜이라 다음 공격도 막혀 있는지(퀵스텝만 되는 후딜이면서 bAttacksCancelAttackRecovery가 꺼져 있을 때). */
	bool IsAttackRecoveryBlockingAttacks() const { return !bAttacksCancelAttackRecovery && IsAttackRecoveryDashOnly(); }

	/**
	 * (2026-10-14) 공격 몽타주 재생 속도 배율. 공격 어빌리티가 PlayRate x 행동 속도에 곱한다(발도·가드 몽타주는 제외).
	 * 기본 0.75(10-15. 1이면 몽타주 원래 속도). PIE에서 knight.Player.AttackSpeed로 바꿔 시험한다(새로 시작하는 공격부터 적용).
	 */
	float GetAttackAnimationSpeed() const { return AttackAnimationSpeed; }
	void SetAttackAnimationSpeed(float NewSpeed) { AttackAnimationSpeed = FMath::Max(0.1f, NewSpeed); }

	/**
	 * (2026-10-20) 마지막으로 패링한 공격의 주인(투사체면 쏜 쪽). 가드가 패링에 성공하면 넣는다.
	 * 패링 반격(근접 공격 bWarpToParriedAttacker)이 락온이 아닐 때 이 대상에게 돌진한다.
	 */
	void SetParryCounterTarget(AActor* Target) { ParryCounterTarget = Target; }
	AActor* GetParryCounterTarget() const { return ParryCounterTarget.Get(); }

	/**
	 * (2026-10-20) 지금 스탠스의 공격 보정(한손 OneHandedModifiers / 양손 TwoHandedModifiers).
	 * 행동 에너지 속성이 없는 액터(몬스터)나 bApplyStanceModifiers가 꺼져 있으면 모두 1.
	 */
	const FknightStanceModifiers& GetStanceModifiers() const;

	/**
	 * (2026-10-20) 내가 때린 공격의 무게만큼 추가 카메라 흔들림(AttackImpactShake). ApplyMeleeHit이 맞힌·막힌 뒤 부른다.
	 * 세기 = 그로기 피해(스탠스·차지 배율까지 곱한 값)가 AttackShakeMinPoise~AttackShakeFullPoise 사이에서 0~AttackShakeMaxScale.
	 * 로컬 플레이어가 조종하는 아바타만. 타격 큐(GCN_Hit_Melee)의 기본 흔들림 위에 겹친다.
	 */
	void PlayAttackImpactShake(float PoiseDamage, bool bGuarded) const;

	/**
	 * (2026-10-21) 내가 때린 곳에 나이트 전용 타격 이펙트·소리(AttackImpactEffect / AttackImpactSounds). ApplyMeleeHit이 맞힌·막힌 뒤 부른다.
	 * 타격 큐(GCN_Hit_Melee)는 몬스터도 같이 쓰므로, 나이트만의 느낌은 여기서 겹친다. 비워 두면 아무것도 안 한다.
	 */
	void PlayAttackImpactEffects(const FHitResult& Hit, bool bGuarded, bool bWeakpoint = false) const;

	/**
	 * (2026-10-21) 나이트가 때린 일반 타격에서 공용 타격 큐(GCN_Hit_Melee: 반짝이 + SC_Hit_Cue + SC_Janyeol_Slam)를
	 * 건너뛰고 위 나이트 전용 이펙트·소리만 쓸지. 가드 큐와 몬스터가 때릴 때는 그대로.
	 * (2026-10-21) 약점 큐(GCN_Hit_Weakpoint: 번개)도 대신한다. 약점은 AttackImpactWeakpointScale만큼 크게.
	 */
	bool ShouldReplaceSharedHitCue() const { return bReplaceSharedHitCue && (!AttackImpactEffects.IsEmpty() || !AttackImpactSounds.IsEmpty()); }

	/** (2026-10-20) 기계식 공격 타이밍 설정(락 → 버스트 → 스톱). 공격 어빌리티가 모션을 재생할 때 읽는다. */
	const FknightMechTimingSettings& GetMechTimingSettings() const { return MechTiming; }

	void CancelActivationGroupAbilities(EknightAbilityActivationGroup Group, const UknightGameplayAbility* IgnoreAbility);

	/**
	 * (2026-10-26) 실행 중인 어빌리티의 활성화 그룹이 OldGroup에서 NewGroup으로 바뀌었을 때 그룹별 실행 수를 고친다
	 * (UknightGameplayAbility::ChangeActivationGroupWhileActive가 부른다). 배타 그룹으로 바뀌면 새로 발동할 때처럼
	 * 실행 중인 교체 가능 배타 어빌리티를 끊는다.
	 */
	void NotifyActivationGroupChanged(const UknightGameplayAbility* Ability, EknightAbilityActivationGroup OldGroup, EknightAbilityActivationGroup NewGroup);

	// 태그 관계
	void SetTagRelationshipMapping(UknightAbilityTagRelationshipMapping* NewMapping);
	void GetAdditionalActivationTagRequirements(const FGameplayTagContainer& AbilityTags,
		FGameplayTagContainer& OutActivationRequired, FGameplayTagContainer& OutActivationBlocked) const;

protected:
	virtual void AbilitySpecInputPressed(FGameplayAbilitySpec& Spec) override;
	virtual void AbilitySpecInputReleased(FGameplayAbilitySpec& Spec) override;
	virtual void NotifyAbilityActivated(const FGameplayAbilitySpecHandle Handle, UGameplayAbility* Ability) override;
	virtual void NotifyAbilityEnded(FGameplayAbilitySpecHandle Handle, UGameplayAbility* Ability, bool bWasCancelled) override;
	virtual void ApplyAbilityBlockAndCancelTags(const FGameplayTagContainer& AbilityTags, UGameplayAbility* RequestingAbility,
		bool bEnableBlockTags, const FGameplayTagContainer& BlockTags, bool bExecuteCancelTags, const FGameplayTagContainer& CancelTags) override;

	void TryActivateAbilitiesOnSpawn();

	/** 그로기 자연 회복 타이머를 켠다. 이미 켜져 있으면 아무것도 하지 않는다. */
	void StartPoiseRecovery();

	/** 타이머마다 호출: 조건을 확인하고 Poise를 올린다. */
	void TickPoiseRecovery();

	/** 행동 에너지 자연 회복 타이머를 켠다. 메카 속성 세트(UknightAttributeSet)가 없는 액터는 틱에서 아무것도 하지 않는다. (2026-10-09) */
	void StartBoostRecovery();

	/** 타이머마다 호출: 마지막으로 쓴 뒤 BoostRecoveryDelay(메카 속성)초가 지났으면 BoostRecoveryRate만큼 올린다. */
	void TickBoostRecovery();

	/** 선택 사항. 아바타가 자신의 폰 데이터나 기본값에서 지정한다. */
	UPROPERTY()
	TObjectPtr<UknightAbilityTagRelationshipMapping> TagRelationshipMapping;

	/**
	 * 마지막 피격(가드로 막은 피격 포함) 후 그로기 자연 회복이 시작되기까지의 시간(초).
	 * 캐릭터·몬스터 BP의 AbilitySystemComponent 디테일에서 각자 바꿀 수 있다.
	 */
	UPROPERTY(EditDefaultsOnly, Category="Knight|Poise", meta=(ClampMin=0, Units="s"))
	float PoiseRecoveryDelay = 0.25f;

	/** 회복 확인 간격(초). 매 프레임 대신 이 간격으로만 계산한다. */
	UPROPERTY(EditDefaultsOnly, Category="Knight|Poise", meta=(ClampMin=0.02, Units="s"))
	float PoiseRecoveryInterval = 0.1f;

	FTimerHandle PoiseRecoveryTimer;

	/** 행동 에너지 회복 확인 간격(초). 회복 지연·속도는 메카 속성(BoostRecoveryDelay·BoostRecoveryRate)이다. */
	UPROPERTY(EditDefaultsOnly, Category="Knight|Boost", meta=(ClampMin=0.02, Units="s"))
	float BoostRecoveryInterval = 0.1f;

	/**
	 * (2026-10-13 사용자 결정) 달리는 동안(달리기 키 + 이동 입력, 땅 위, UknightMovementComponent::IsRunning) 행동 에너지 회복을 멈춘다.
	 * 전투 여부와 상관없이 적용한다. 깎지는 않고, 회복 대기 시간도 다시 세지 않는다. 끄면 달리는 중에도 회복한다.
	 */
	UPROPERTY(EditDefaultsOnly, Category="Knight|Boost")
	bool bPauseBoostRecoveryWhileRunning = true;

	/**
	 * (2026-10-26 사용자 결정 '가드 중엔 스테미너 회복량 감소') 가드를 올리고 있는 동안(State.Guarding) 회복 속도에 곱하는 배율.
	 * 0.5 = 절반(초당 25 → 12.5), 0 = 가드 중엔 회복 없음, 1 = 줄이지 않음. 바뀔 수 있는 값.
	 */
	UPROPERTY(EditDefaultsOnly, Category="Knight|Boost", meta=(ClampMin=0, ClampMax=1))
	float GuardBoostRecoveryScale = 0.5f;

	/** 아바타의 나이트 무브먼트가 지금 달리는 중인지. */
	bool IsAvatarRunning() const;

	/**
	 * (2026-10-16 사용자 결정) 켜면 BoostRecoveryDelay가 지나도 공격이 끝날 때까지 행동 에너지 회복을 미룬다.
	 * 끝 = 콤보 창(Window.Combo)과 후딜(AttackRecoveryDashOnlyTime)이 모두 끝나 이동으로 끊을 수 있는 때. 끄면 대기 시간만 본다.
	 */
	UPROPERTY(EditDefaultsOnly, Category="Knight|Boost")
	bool bHoldBoostRecoveryDuringAction = true;

	/** 공격이 아직 끝나지 않았는지: 휘두르는 중(캔슬 창 전), 후딜, 콤보 창. */
	bool IsActionHoldingBoostRecovery() const;

	/** 교체 가능 배타 그룹의 공격(Ability.Attack) 어빌리티가 실행 중인지. */
	bool IsExclusiveAttackActive() const;

	/**
	 * (2026-10-14 사용자 결정, 시험 중) 공격 후딜은 퀵스텝만 끊을 수 있다. '게임이 너무 스피디하다'는 의견으로 시험.
	 * 켜면 공격 캔슬 창에서 가드·강공격(탭·차지)·스킬·새 공격(콤보 창 밖)·이동 캔슬이 막히고 퀵스텝만 된다.
	 * 끄면 예전처럼 캔슬 창에서 무엇이든 공격을 끊는다. 가드·퀵스텝 경직의 캔슬 창은 영향 없음.
	 */
	UPROPERTY(EditDefaultsOnly, Category="Knight|Attack")
	bool bOnlyDashCancelsAttackRecovery = true;

	/**
	 * (2026-10-14 사용자 결정) 후딜 길이 = 공격의 캔슬 창이 열린 뒤 퀵스텝만 끊을 수 있는 시간(초, 실제 시간). 바뀔 수 있는 값.
	 * 공격 몽타주의 캔슬 창은 타격 직후부터 몽타주 끝(검을 등 쪽으로 들어 올리는 동작)까지라, 창 전체를 후딜로 잡으면
	 * 0.7배속에서 1타 기준 2.5초 동안 아무것도 못 했다. 공격 배속과 상관없이 이 시간만큼만 묶인다.
	 * (2026-10-21 사용자 결정) 0.5 → 0.25: 기계식 타이밍·양손 배속이 들어온 뒤 판정 끝에서 가드·이동까지 약 0.72초라
	 * 반응이 너무 늦었다(특히 콤보 창이 없는 4타·대쉬공격·돌진 베기·패링 반격). 0.25면 약 0.47초.
	 */
	UPROPERTY(EditDefaultsOnly, Category="Knight|Attack", meta=(ClampMin=0, Units="s", EditCondition="bOnlyDashCancelsAttackRecovery"))
	float AttackRecoveryDashOnlyTime = 0.25f;

	/**
	 * (2026-10-23 사용자 결정) 위 후딜 시간에 퀵스텝뿐 아니라 다음 공격(Ability.Attack)도 허용한다. 가드·이동·점프는 그대로 기다린다.
	 * 1~3타는 콤보 창으로 이미 이어지지만, 콤보 창이 없는 공격(4타·대쉬공격·패링 반격·돌진 베기·점프 공격)은
	 * 다음 공격까지 판정 끝에서 약 0.47초(양손)가 걸려 느렸다 → 퀵스텝과 같은 약 0.22초. 끄면 예전처럼 퀵스텝만. 바뀔 수 있는 값.
	 */
	UPROPERTY(EditDefaultsOnly, Category="Knight|Attack", meta=(EditCondition="bOnlyDashCancelsAttackRecovery"))
	bool bAttacksCancelAttackRecovery = true;

	/** (2026-10-14) Window.Cancel 태그 수가 늘 때(창이 열릴 때) 시각을 기록한다. */
	void HandleCancelWindowChanged(const FGameplayTag Tag, int32 NewCount);
	FDelegateHandle CancelWindowHandle;
	int32 CancelWindowCount = 0;
	double CancelWindowOpenedTime = -UE_BIG_NUMBER;

	/**
	 * (2026-10-14) 공격 몽타주 재생 속도 배율(GetAttackAnimationSpeed). 바뀔 수 있는 값. 시험은 knight.Player.AttackSpeed.
	 * 1.0 → 0.7: '게임이 너무 스피디하다'는 의견으로 사용자 플레이 테스트 후 결정(10-14). 후딜 0.5초와 함께 정함.
	 * 0.7 → 1.0: EssentialAnimation 애니로 바꾼 뒤 사용자 요청(10-14). 새 애니는 원래 속도로 재생한다.
	 * 1.0 → 0.75: 새 애니 기준으로 다시 조금 느리게(10-15 사용자 요청).
	 */
	UPROPERTY(EditDefaultsOnly, Category="Knight|Attack", meta=(ClampMin=0.1))
	float AttackAnimationSpeed = 0.75f;

	/**
	 * (2026-10-20) 스탠스별 공격 차이를 쓸지. 끄면 한손·양손이 모션만 다르고 수치는 같다.
	 * 테스트: PIE 콘솔 knight.Player.StanceModifiers 0/1
	 */
	UPROPERTY(EditDefaultsOnly, Category="Knight|Stance")
	bool bApplyStanceModifiers = true;

	/** (2026-10-20 임시값) 한손: 빠르고 가볍다. 피해·그로기는 낮고 행동 에너지를 덜 쓴다. */
	UPROPERTY(EditDefaultsOnly, Category="Knight|Stance", meta=(EditCondition="bApplyStanceModifiers"))
	FknightStanceModifiers OneHandedModifiers = FknightStanceModifiers(0.85f, 0.75f, 1.1f, 0.8f);

	/** (2026-10-20 임시값) 양손: 느리고 무겁다. 피해·그로기가 높고 행동 에너지를 더 쓴다. */
	UPROPERTY(EditDefaultsOnly, Category="Knight|Stance", meta=(EditCondition="bApplyStanceModifiers"))
	FknightStanceModifiers TwoHandedModifiers = FknightStanceModifiers(1.2f, 1.35f, 0.92f, 1.25f);

	/** (2026-10-20) 큰 공격이 맞았을 때 더하는 카메라 흔들림. 비우면 추가 흔들림 없음. BP_Knight에서 CS_Knight_Impact. */
	UPROPERTY(EditDefaultsOnly, Category="Knight|Attack Feel")
	TSubclassOf<UCameraShakeBase> AttackImpactShake;

	/** 이 그로기 피해 이하는 추가 흔들림 없음(약공격). 예: 한손 3타 15, 양손 3타 27. */
	UPROPERTY(EditDefaultsOnly, Category="Knight|Attack Feel", meta=(ClampMin=0))
	float AttackShakeMinPoise = 20.f;

	/** 이 그로기 피해 이상이면 AttackShakeMaxScale. 예: 양손 내려찍기 91, 양손 풀차지 108. */
	UPROPERTY(EditDefaultsOnly, Category="Knight|Attack Feel", meta=(ClampMin=1))
	float AttackShakeFullPoise = 80.f;

	UPROPERTY(EditDefaultsOnly, Category="Knight|Attack Feel", meta=(ClampMin=0))
	float AttackShakeMaxScale = 1.f;

	/** 적이 가드로 막았을 때의 비율. */
	UPROPERTY(EditDefaultsOnly, Category="Knight|Attack Feel", meta=(ClampMin=0, ClampMax=1))
	float AttackShakeGuardedRatio = 0.5f;

	/** 한 번 휘두르기가 여러 부위·여러 적을 동시에 맞혀도 흔들림이 겹치지 않게 하는 최소 간격(초). */
	UPROPERTY(EditDefaultsOnly, Category="Knight|Attack Feel", meta=(ClampMin=0, Units="s"))
	float AttackShakeCooldown = 0.12f;

	mutable double LastAttackShakeTime = -UE_BIG_NUMBER;

	/**
	 * (2026-10-21) 내가 때린 곳에 동시에 나오는 타격 이펙트들(불꽃·파편 등을 겹쳐 쓴다). 비우면 없음.
	 * 나이아가라와 예전 파티클(Cascade, P_ 이름) 모두 된다.
	 */
	UPROPERTY(EditDefaultsOnly, Category="Knight|Attack Feel|Impact")
	TArray<TObjectPtr<UFXSystemAsset>> AttackImpactEffects;

	/** 켜면 나이트의 일반 타격에서 공용 타격 큐(GCN_Hit_Melee)를 대신한다. 끄면 둘 다 나온다. */
	UPROPERTY(EditDefaultsOnly, Category="Knight|Attack Feel|Impact")
	bool bReplaceSharedHitCue = true;

	/** 공용 큐를 대신할 때, 큐가 하던 기본 화면 흔들림(AttackImpactShake) 세기. */
	UPROPERTY(EditDefaultsOnly, Category="Knight|Attack Feel|Impact", meta=(ClampMin=0))
	float AttackImpactBaseShakeScale = 1.f;

	/** (2026-10-21) 약점(머리 등) 타격일 때 이펙트 크기·흔들림·소리 배율. 예전에는 약점만 번개 큐가 따로 나왔다. */
	UPROPERTY(EditDefaultsOnly, Category="Knight|Attack Feel|Impact", meta=(ClampMin=1))
	float AttackImpactWeakpointScale = 1.5f;

	UPROPERTY(EditDefaultsOnly, Category="Knight|Attack Feel|Impact", meta=(ClampMin=0))
	float AttackImpactEffectScale = 1.f;

	/**
	 * (2026-10-21) 켜면 이펙트의 위쪽(Z)을 맞은 면 바깥쪽으로 돌린다(발자국·불꽃처럼 '위로 튀는' 이펙트용).
	 * 끄면 이펙트의 앞쪽(X)을 맞춘다(총탄 타격처럼 '앞으로 튀는' 이펙트용).
	 */
	UPROPERTY(EditDefaultsOnly, Category="Knight|Attack Feel|Impact")
	bool bAttackImpactEffectUpAlongNormal = true;

	/** 적이 가드로 막았을 때 이펙트 크기 비율. 0이면 막혔을 때는 안 나온다. */
	UPROPERTY(EditDefaultsOnly, Category="Knight|Attack Feel|Impact", meta=(ClampMin=0, ClampMax=1))
	float AttackImpactGuardedEffectRatio = 0.6f;

	/** 타격 소리 후보. 매번 하나를 무작위로. 비우면 없음(타격 큐의 기본 소리만 남). */
	UPROPERTY(EditDefaultsOnly, Category="Knight|Attack Feel|Impact")
	TArray<TObjectPtr<USoundBase>> AttackImpactSounds;

	UPROPERTY(EditDefaultsOnly, Category="Knight|Attack Feel|Impact", meta=(ClampMin=0))
	float AttackImpactSoundVolume = 1.f;

	/** 한 번 휘두르기가 여러 부위를 동시에 맞혀도 소리가 겹치지 않게 하는 최소 간격(초). */
	UPROPERTY(EditDefaultsOnly, Category="Knight|Attack Feel|Impact", meta=(ClampMin=0, Units="s"))
	float AttackImpactSoundCooldown = 0.1f;

	mutable double LastAttackImpactSoundTime = -UE_BIG_NUMBER;

	/**
	 * (2026-10-20 사용자 요청 '사람이 아니라 로봇이 하는 느낌') 기계식 공격 타이밍.
	 * 준비 끝에서 뚝 멈췄다가(락) 모터가 확 돌고(버스트) 판정 끝에서 뚝 멈춘다(스톱). 자세한 설명은 knightMechAttackTiming.h.
	 */
	UPROPERTY(EditDefaultsOnly, Category="Knight|Attack Feel")
	FknightMechTimingSettings MechTiming;

	FTimerHandle BoostRecoveryTimer;

	TArray<FGameplayAbilitySpecHandle> InputPressedSpecHandles;
	TArray<FGameplayAbilitySpecHandle> InputReleasedSpecHandles;
	TArray<FGameplayAbilitySpecHandle> InputHeldSpecHandles;

	/**
	 * (2026-10-15) 누른 뒤 아직 발동하지 못한 bRetryPressWhileHeld 어빌리티(가드). 키를 누르고 있는 동안 매 프레임 다시 시도한다.
	 * 발동하거나(NotifyAbilityActivated) 키를 떼면 빠진다.
	 */
	TArray<FGameplayAbilitySpecHandle> PendingHeldPressHandles;

	/** ProcessAbilityInput이 마지막으로 실행된 뒤 발동한 어빌리티들의 입력 태그. */
	FGameplayTagContainer RecentlyActivatedInputTags;

	/** (2026-10-20) SetParryCounterTarget 참고. */
	TWeakObjectPtr<AActor> ParryCounterTarget;

	int32 ActivationGroupCounts[static_cast<uint8>(EknightAbilityActivationGroup::MAX)];
};
