#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/Abilities/knightGameplayAbility.h"
#include "knightAbilitySystemComponent.generated.h"

class UknightAbilityTagRelationshipMapping;

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

	/**
	 * (2026-10-14) 공격 몽타주 재생 속도 배율. 공격 어빌리티가 PlayRate x 행동 속도에 곱한다(발도·가드 몽타주는 제외).
	 * 기본 1.0(10-14 새 애니로 바꾼 뒤. 1이면 몽타주 원래 속도). PIE에서 knight.Player.AttackSpeed로 바꿔 시험한다(새로 시작하는 공격부터 적용).
	 */
	float GetAttackAnimationSpeed() const { return AttackAnimationSpeed; }
	void SetAttackAnimationSpeed(float NewSpeed) { AttackAnimationSpeed = FMath::Max(0.1f, NewSpeed); }
	void CancelActivationGroupAbilities(EknightAbilityActivationGroup Group, const UknightGameplayAbility* IgnoreAbility);

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

	/** 아바타의 나이트 무브먼트가 지금 달리는 중인지. */
	bool IsAvatarRunning() const;

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
	 */
	UPROPERTY(EditDefaultsOnly, Category="Knight|Attack", meta=(ClampMin=0, Units="s", EditCondition="bOnlyDashCancelsAttackRecovery"))
	float AttackRecoveryDashOnlyTime = 0.5f;

	/** (2026-10-14) Window.Cancel 태그 수가 늘 때(창이 열릴 때) 시각을 기록한다. */
	void HandleCancelWindowChanged(const FGameplayTag Tag, int32 NewCount);
	FDelegateHandle CancelWindowHandle;
	int32 CancelWindowCount = 0;
	double CancelWindowOpenedTime = -UE_BIG_NUMBER;

	/**
	 * (2026-10-14) 공격 몽타주 재생 속도 배율(GetAttackAnimationSpeed). 바뀔 수 있는 값. 시험은 knight.Player.AttackSpeed.
	 * 1.0 → 0.7: '게임이 너무 스피디하다'는 의견으로 사용자 플레이 테스트 후 결정(10-14). 후딜 0.5초와 함께 정함.
	 * 0.7 → 1.0: EssentialAnimation 애니로 바꾼 뒤 사용자 요청(10-14). 새 애니는 원래 속도로 재생한다.
	 */
	UPROPERTY(EditDefaultsOnly, Category="Knight|Attack", meta=(ClampMin=0.1))
	float AttackAnimationSpeed = 1.0f;

	FTimerHandle BoostRecoveryTimer;

	TArray<FGameplayAbilitySpecHandle> InputPressedSpecHandles;
	TArray<FGameplayAbilitySpecHandle> InputReleasedSpecHandles;
	TArray<FGameplayAbilitySpecHandle> InputHeldSpecHandles;

	/** ProcessAbilityInput이 마지막으로 실행된 뒤 발동한 어빌리티들의 입력 태그. */
	FGameplayTagContainer RecentlyActivatedInputTags;

	int32 ActivationGroupCounts[static_cast<uint8>(EknightAbilityActivationGroup::MAX)];
};
