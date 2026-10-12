#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "Interfaces/TargetableInterface.h"
#include "knightMonsterPartComponent.generated.h"

class UAbilitySystemComponent;
class UknightMonsterPartData;
class UPrimitiveComponent;
class USkinnedMeshComponent;
struct FGameplayEffectSpec;
struct FknightMonsterPartDefinition;

/** 락온 UI가 부위 표시를 고르는 상태. 내구 비율과 데이터의 CriticalRatio로 정한다. */
UENUM(BlueprintType)
enum class EknightPartCondition : uint8
{
	/** 파괴할 수 없는 부위(몸통 등). 내구 게이지가 없다. */
	Unbreakable,
	Normal,
	/** 남은 내구 비율이 CriticalRatio 이하. 곧 부서진다. */
	Critical,
	Broken
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FknightMonsterPartBroken, FGameplayTag, PartTag, AActor*, BreakInstigator);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FknightMonsterPartDamaged, FGameplayTag, PartTag, float, RemainingRatio);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FknightMonsterPartConditionChanged, FGameplayTag, PartTag, EknightPartCondition, NewCondition);

/** 부위 하나의 런타임 상태. */
USTRUCT(BlueprintType)
struct FknightMonsterPartState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Part")
	FGameplayTag PartTag;

	/** 지금까지 받은 피해량. 파괴 가능한 부위는 이 값을 BreakDamage와 비교한다. */
	UPROPERTY(BlueprintReadOnly, Category="Part")
	float DamageTaken = 0.f;

	UPROPERTY(BlueprintReadOnly, Category="Part")
	bool bBroken = false;
};

/**
 * 몬스터 부위. 타격을 부위로 판정하고, 히트존 배율을 제공하며, 부위 피해를 누적하고 부위 파괴를 처리한다.
 *
 * 타격은 맞은 본(스켈레탈 몬스터) 또는 맞은 컴포넌트의 컴포넌트 태그(테스트 맵의 큐브 대역 같은
 * 별도 프리미티브)로 부위를 판정한다.
 *
 * 부위 피해는 타격으로 몬스터가 실제로 잃은 Health이며, 피해 이펙트 컨텍스트의 타격 결과로 찾은 부위에
 * 누적된다. 파괴 시 부위의 BreakEffect를 몬스터에 적용하고, HideBoneOnBreak(또는 부위 컴포넌트)를 숨기고,
 * 부위 위치에서 GameplayCue.PartBreak를 실행하고(부위 태그는 큐의 AggregatedSourceTags에 담긴다), 몬스터의 ASC에 Event.PartBroken을 보내고 OnPartBroken을 브로드캐스트한다.
 */
UCLASS(ClassGroup=(Knight), meta=(BlueprintSpawnableComponent))
class KNIGHT_API UknightMonsterPartComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UknightMonsterPartComponent();

	/** 런타임 상태를 만들고 소유자의 Core AttributeSet에 바인딩한다. 몬스터가 ASC 준비를 마친 뒤 호출한다. */
	void InitializeParts(const UknightMonsterPartData* InPartData, UAbilitySystemComponent* InASC);

	FGameplayTag ResolvePart(const FHitResult& Hit) const;
	float GetDamageMultiplier(const FGameplayTag& PartTag) const;

	UFUNCTION(BlueprintPure, Category="Knight|Monster")
	bool IsPartBroken(FGameplayTag PartTag) const;

	/** 남은 내구 비율(1 = 멀쩡, 0 = 파괴). 파괴할 수 없는 부위와 모르는 태그는 1. */
	UFUNCTION(BlueprintPure, Category="Knight|Monster")
	float GetPartHealthRatio(FGameplayTag PartTag) const;

	/** 모르는 태그는 Unbreakable. */
	UFUNCTION(BlueprintPure, Category="Knight|Monster")
	EknightPartCondition GetPartCondition(FGameplayTag PartTag) const;

	UFUNCTION(BlueprintPure, Category="Knight|Monster")
	const TArray<FknightMonsterPartState>& GetPartStates() const { return PartStates; }

	/** LockOnSocket이 있고 부위 파괴로 숨겨지지 않은 부위들의 락온 지점. */
	void GetTargetPoints(TArray<FknightTargetPoint>& OutPoints) const;

	/** 부위의 월드 위치. 태그가 붙은 첫 컴포넌트를, 없으면 첫 본을 쓴다. 둘 다 없으면 false. */
	bool GetPartLocation(const FGameplayTag& PartTag, FVector& OutLocation) const;

	/** 부위에 피해를 직접 더한다(스크립트 이벤트, 디버그용). 일반 타격은 Core AttributeSet을 거친다. */
	UFUNCTION(BlueprintCallable, Category="Knight|Monster")
	void ApplyPartDamage(FGameplayTag PartTag, float Damage, AActor* DamageInstigator);

	UPROPERTY(BlueprintAssignable, Category="Knight|Monster")
	FknightMonsterPartBroken OnPartBroken;

	/** 파괴 가능한 부위가 피해를 받을 때마다. 파괴하는 타격도 포함한다(RemainingRatio 0). 파괴된 뒤와 파괴할 수 없는 부위는 보내지 않는다. */
	UPROPERTY(BlueprintAssignable, Category="Knight|Monster")
	FknightMonsterPartDamaged OnPartDamaged;

	/** Normal → Critical, Normal/Critical → Broken. 한 타격에 빈사를 건너뛰고 파괴되면 Broken만 보낸다. OnPartBroken보다 먼저 온다. */
	UPROPERTY(BlueprintAssignable, Category="Knight|Monster")
	FknightMonsterPartConditionChanged OnPartConditionChanged;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void HandleDamaged(AActor* DamageInstigator, AActor* DamageCauser, const FGameplayEffectSpec* EffectSpec,
		float Magnitude, float OldValue, float NewValue);
	void BreakPart(int32 PartIndex, AActor* BreakInstigator);
	/** 부위 정의에 BreakPieceMesh가 있으면 HideBoneOnBreak 뼈의 지금 자세 그대로 조각을 만들어 떨어뜨린다. */
	void SpawnBreakPiece(const FknightMonsterPartDefinition& Definition, USkinnedMeshComponent* Mesh) const;
	int32 FindPartIndex(const FGameplayTag& PartTag) const;
	float GetHealthRatio(int32 PartIndex) const;
	EknightPartCondition GetCondition(int32 PartIndex) const;
	const FknightMonsterPartDefinition* GetDefinition(int32 PartIndex) const;
	/** 부위의 ComponentTags 중 하나를 가진 소유자 컴포넌트들. */
	void GetPartComponents(int32 PartIndex, TArray<UPrimitiveComponent*>& OutComponents) const;

	UPROPERTY()
	TObjectPtr<const UknightMonsterPartData> PartData;

	UPROPERTY()
	TArray<FknightMonsterPartState> PartStates;

	TMap<FName, int32> BoneToPart;
	TMap<FName, int32> ComponentTagToPart;
	TWeakObjectPtr<UAbilitySystemComponent> BoundASC;
};
