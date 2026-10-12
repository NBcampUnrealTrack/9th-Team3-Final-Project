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
	 * 회복 속도가 0 이하이거나, 이미 최대치이거나, 마지막으로 쓴 뒤 회복 대기 시간이 아직 지나지 않았으면 그대로.
	 * 그 외에는 초당 RecoveryRate만큼, 최대치를 넘지 않게 올린다. (그로기와 달리 0에서도 회복한다)
	 */
	KNIGHT_API float Step(float CurrentGauge, float MaxGauge, float RecoveryRate, float DeltaSeconds,
		float SecondsSinceLastSpend, float RecoveryDelay);
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
 *   마지막으로 쓴 뒤 BoostRecoveryDelay초(메카 속성, 기본 1.5)가 지나면 초당 BoostRecoveryRate(메카 속성, 기본 25)만큼 찬다.
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
	 */
	bool IsActivationGroupBlocked(EknightAbilityActivationGroup Group, const FGameplayTagContainer& AbilityTags) const;
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

	FTimerHandle BoostRecoveryTimer;

	TArray<FGameplayAbilitySpecHandle> InputPressedSpecHandles;
	TArray<FGameplayAbilitySpecHandle> InputReleasedSpecHandles;
	TArray<FGameplayAbilitySpecHandle> InputHeldSpecHandles;

	/** ProcessAbilityInput이 마지막으로 실행된 뒤 발동한 어빌리티들의 입력 태그. */
	FGameplayTagContainer RecentlyActivatedInputTags;

	int32 ActivationGroupCounts[static_cast<uint8>(EknightAbilityActivationGroup::MAX)];
};
