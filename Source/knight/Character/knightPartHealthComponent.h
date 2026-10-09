#pragma once

#include "CoreMinimal.h"
#include "ActiveGameplayEffectHandle.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "knightPartHealthComponent.generated.h"

class UAbilitySystemComponent;
class UGameplayEffect;
struct FGameplayEffectSpec;
struct FHitResult;
struct FOnAttributeChangeData;

/** 나이트의 필수 4부위. 순서는 부위 체력 배열 순서와 같다. */
UENUM(BlueprintType)
enum class EknightBodyPart : uint8
{
	Head,
	Torso,
	Arms,
	Legs,
	Count UMETA(Hidden)
};

/**
 * 부위 체력 순수 계산. GAS 없이 테스트(knightPartHealthTest.cpp)로 검증한다.
 */
namespace knightPartHealth
{
	constexpr int32 PartCount = static_cast<int32>(EknightBodyPart::Count);

	/**
	 * 피해를 부위 체력에 나눠 뺀다.
	 *   HitPart가 0~3이면 그 부위에서만 뺀다 (이미 0이면 그대로 0. 전체 체력은 따로 줄어든다).
	 *   HitPart가 -1(부위를 알 수 없는 피해: 범위 공격·포효·지속 피해)이면 파괴되지 않은 부위에 똑같이 나눠 뺀다.
	 * 부위 체력은 0 아래로 내려가지 않는다.
	 */
	KNIGHT_API void DistributeDamage(float (&InOutHealth)[PartCount], float Damage, int32 HitPart);
}

/**
 * 부위 체력 (2026-10-06, 1차 MVP용 간단 구현).
 *
 *   - 최대 체력(Core MaxHealth) = 네 부위 최대 체력의 합. 현재 체력은 부위와 따로 있고, 사망은 전체 체력 0일 때만.
 *   - 부위에 맞으면 그 부위 체력과 전체 체력이 같이 준다. 부위는 맞은 본 이름으로 정한다(BoneToPart).
 *     본이 없는 히트(이동 캡슐)는 맞은 지점에서 가장 가까운 몸체 본으로 정한다(2026-10-09, FindHitPart).
 *     히트 정보가 없는 피해는 파괴되지 않은 부위에 나눠 준다.
 *   - 부위 체력 0 이하 = 파괴(State.PartBroken.* 태그 + 페널티), 1 이상 = 복구.
 *     머리: 받는 피해 증가 / 몸통: MaxPoise 감소 / 팔: 공격력 감소 / 다리: 걷기·달리기 속도 감소
 *   - 파괴된 부위를 또 맞으면 피해에 BrokenPartHitMultiplier를 곱한다 (맞는 쪽 기준 크리티컬).
 *
 * 간단 구현의 한계 (다음 단계에서 보완):
 *   - 머리 파괴의 피해 증가와 파괴 부위 크리티컬은 히트 정보가 있는 피격에만 적용된다(GetDamageMultiplier 경로).
 *   - 낙하 피해를 다리로 보내는 처리, 수리 키트(낮은 부위부터 회복·파괴 부위당 +0.2초)는 아직 없다.
 */
UCLASS(ClassGroup=(Knight), meta=(BlueprintSpawnableComponent))
class KNIGHT_API UknightPartHealthComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UknightPartHealthComponent();

	/** 소유 캐릭터의 ASC가 준비되고 파츠가 장착된 뒤 한 번 호출한다. 이벤트 연결과 MaxHealth 합산을 한다. */
	void InitializeWithAbilitySystem(UAbilitySystemComponent* InASC);

	/** 맞은 위치 → 부위 태그(Knight.Part.*). 정하는 방법은 FindHitPart, 못 정하면 빈 태그. ICombatInterface::ResolveHitPart에서 쓴다. */
	FGameplayTag ResolveHitPartTag(const FHitResult& Hit) const;

	/** 맞은 부위의 피해 배율. 파괴된 부위면 크리티컬, 머리가 파괴돼 있으면 피해 증가. ICombatInterface::GetPartDamageMultiplier에서 쓴다. */
	float GetDamageMultiplier(const FGameplayTag& PartTag) const;

	UFUNCTION(BlueprintPure, Category="Knight|PartHealth")
	bool IsPartBroken(EknightBodyPart Part) const;

	UFUNCTION(BlueprintPure, Category="Knight|PartHealth")
	int32 GetBrokenPartCount() const;

	static FGameplayTag GetPartTag(EknightBodyPart Part);

protected:
	/** 본 이름 → 부위. 기본값은 생성자에 나이트 메시 기준으로 들어 있다(메시가 바뀌면 BP에서 수정). 없는 본은 FindHitPart가 가까운 몸체 본으로 다시 찾는다. */
	UPROPERTY(EditDefaultsOnly, Category="Knight|PartHealth")
	TMap<FName, EknightBodyPart> BoneToPart;

	/** 맞은 부위를 정하지 못했을 때(FindHitPart 실패) 피해를 받을 부위(HandleDamaged). 이때 파괴 부위 크리티컬은 붙지 않는다. */
	UPROPERTY(EditDefaultsOnly, Category="Knight|PartHealth")
	EknightBodyPart DefaultHitPart = EknightBodyPart::Torso;

	/** 파괴된 부위를 또 맞았을 때의 피해 배율 (맞는 쪽 기준 크리티컬). */
	UPROPERTY(EditDefaultsOnly, Category="Knight|PartHealth", meta=(ClampMin=1))
	float BrokenPartHitMultiplier = 1.5f;

	/** 머리가 파괴돼 있을 때 받는 피해 배율. */
	UPROPERTY(EditDefaultsOnly, Category="Knight|PartHealth", meta=(ClampMin=1))
	float HeadBrokenDamageMultiplier = 1.2f;

	/** 몸통·팔·다리 파괴 시 해당 스탯을 줄이는 비율 (0.15 = 15% 감소). 10~20%에서 시작해 테스트로 조정. */
	UPROPERTY(EditDefaultsOnly, Category="Knight|PartHealth", meta=(ClampMin=0, ClampMax=0.9))
	float BrokenStatPenalty = 0.15f;

private:
	/**
	 * 히트 → 부위. BoneToPart에 있는 본에 맞았으면 그 부위.
	 * 본이 없거나 매핑에 없는 본이면(주로 이동 캡슐) 맞은 지점에서 가장 가까운 몸체 본(물리 에셋에 바디가 있는 본)으로 정한다.
	 * (2026-10-09 PIE: 잔열 공격이 몸체보다 바깥인 캡슐에 먼저 닿아 21번 중 11번이 몸통 기본값으로 들어가서 넣음, 사용자 결정)
	 * OutBone = 부위를 정한 본. 못 정하면 false.
	 */
	bool FindHitPart(const FHitResult& Hit, EknightBodyPart& OutPart, FName& OutBone) const;

	/** 피해가 Health를 줄였을 때 (Core OnDamaged). 부위 체력에 같은 양을 나눠 뺀다. */
	void HandleDamaged(AActor* DamageInstigator, AActor* DamageCauser, const FGameplayEffectSpec* EffectSpec,
		float Magnitude, float OldValue, float NewValue);

	/** 부위 체력이 바뀔 때: 0 이하로 떨어지면 파괴, 1 이상으로 오르면 복구. */
	void HandlePartHealthChanged(const FOnAttributeChangeData& Data, EknightBodyPart Part);

	/** 부위 최대 체력이 바뀔 때: Core MaxHealth를 네 부위 최대 체력의 합으로 맞춘다. */
	void SyncMaxHealth();

	void BreakPart(EknightBodyPart Part);
	void RepairPart(EknightBodyPart Part);

	/** 파괴 페널티 GE (무한 지속, 스탯에 곱하기). 머리는 GE 없이 GetDamageMultiplier로 처리한다. */
	const UGameplayEffect* GetPenaltyEffect(EknightBodyPart Part);

	static FGameplayTag GetBrokenTag(EknightBodyPart Part);

	TWeakObjectPtr<UAbilitySystemComponent> ASC;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UGameplayEffect>> PenaltyEffects;

	FActiveGameplayEffectHandle PenaltyHandles[knightPartHealth::PartCount];
};
