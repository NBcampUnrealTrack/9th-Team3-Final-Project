#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "GameplayEffectTypes.h"
#include "knightAnimInstance.generated.h"

class UAbilitySystemComponent;
class UAnimMontage;

/**
 * Base Anim Instance for the Knight and monsters.
 *
 * GameplayTagPropertyMap keeps Anim Blueprint variables in sync with the owner's ASC tags
 * (for example State.Flying -> bIsFlying, State.Monster.Enraged -> bIsEnraged), so the Anim Blueprint
 * never polls gameplay state itself. Common locomotion values are filled natively on the worker thread.
 */
UCLASS()
class KNIGHT_API UknightAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	/** Binds the tag map to an ASC. Called automatically on init when the owner implements IAbilitySystemInterface. */
	virtual void InitializeWithAbilitySystem(UAbilitySystemComponent* ASC);

protected:
	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;
	virtual void NativeThreadSafeUpdateAnimation(float DeltaSeconds) override;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

	/** Tag to Blueprint property bindings. Properties must be bool, int32, float or double. */
	UPROPERTY(EditDefaultsOnly, Category="Knight|GameplayTags")
	FGameplayTagBlueprintPropertyMap GameplayTagPropertyMap;

	UPROPERTY(BlueprintReadOnly, Category="Knight|Locomotion")
	float GroundSpeed = 0.f;

	UPROPERTY(BlueprintReadOnly, Category="Knight|Locomotion")
	FVector Velocity = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category="Knight|Locomotion")
	bool bIsFalling = false;

	UPROPERTY(BlueprintReadOnly, Category="Knight|Locomotion")
	bool bHasAcceleration = false;

	/** (2026-10-15) 양손 스탠스인지 (State.Stance.TwoHanded). false면 한손. 애님 블루프린트가 대기·이동 자세를 고를 때 쓴다. */
	UPROPERTY(BlueprintReadOnly, Category="Knight|Combat")
	bool bTwoHandedStance = false;

	/**
	 * 공중에서 하체(골반·다리)를 몽타주 대신 기본 공중 자세로 덮는 정도 (0~1). (2026-10-12)
	 * 공중이고, 몽타주가 없거나 AirLegLockMontages 중 하나가 재생 중일 때 1.
	 * 그 밖의 몽타주(공격 등)는 0이라 공중에서도 다리까지 몽타주를 따른다.
	 * 애님 블루프린트의 하체 레이어 블렌드 가중치로 쓴다.
	 */
	UPROPERTY(BlueprintReadOnly, Category="Knight|Locomotion")
	float AirLegLockAlpha = 0.f;

	/** 공중에서도 하체를 기본 공중 자세로 두는 몽타주. 검 뽑기·넣기처럼 땅 위 다리 동작이 들어 있는 것. */
	UPROPERTY(EditDefaultsOnly, Category="Knight|Locomotion")
	TArray<TObjectPtr<UAnimMontage>> AirLegLockMontages;

	/** AirLegLockAlpha가 0↔1로 바뀌는 데 걸리는 시간. */
	UPROPERTY(EditDefaultsOnly, Category="Knight|Locomotion", meta=(ClampMin=0, Units="s"))
	float AirLegLockBlendTime = 0.15f;

private:
	// Game thread copies read by the thread-safe update.
	FVector CachedVelocity = FVector::ZeroVector;
	bool bCachedIsFalling = false;
	bool bCachedHasAcceleration = false;
	bool bAbilitySystemBound = false;
	TWeakObjectPtr<UAbilitySystemComponent> BoundASC;
};
