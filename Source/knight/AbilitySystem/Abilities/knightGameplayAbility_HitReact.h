#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/knightGameplayAbility.h"
#include "knightGameplayAbility_HitReact.generated.h"

class UAnimMontage;
class UCameraShakeBase;
class UGameplayEffect;

UENUM(BlueprintType)
enum class EknightHitReactType : uint8
{
	Flinch,
	Stagger,
	Knockdown
};

/** One reaction animated per hit direction. Missing directions fall back to Front. */
USTRUCT(BlueprintType)
struct FknightDirectionalMontages
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, Category="Hit React")
	TObjectPtr<UAnimMontage> Front = nullptr;

	UPROPERTY(EditDefaultsOnly, Category="Hit React")
	TObjectPtr<UAnimMontage> Back = nullptr;

	UPROPERTY(EditDefaultsOnly, Category="Hit React")
	TObjectPtr<UAnimMontage> Left = nullptr;

	UPROPERTY(EditDefaultsOnly, Category="Hit React")
	TObjectPtr<UAnimMontage> Right = nullptr;

	UAnimMontage* Pick(const FVector& AvatarForward, const FVector& AvatarRight, const FVector& ToInstigator) const;
};

/**
 * 맞는 맛 (2026-10-12). 반응 종류(경직·크게 휘청·다운)마다 따로 정한다. 모두 0이면 아무것도 하지 않는다.
 *   넉백: 때린 쪽에서 멀어지는 방향(수평)으로 KnockbackTime 동안 KnockbackDistance만큼 밀린다.
 *   흔들림·슬로: 플레이어가 조종하는 폰일 때만. 슬로는 게임 전체 시간을 SlowMoDilation배로 SlowMoDuration(실제 초) 동안 늦춘다.
 */
USTRUCT(BlueprintType)
struct FknightHitImpactFeel
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, Category="Impact", meta=(ClampMin=0, Units="cm"))
	float KnockbackDistance = 0.f;

	UPROPERTY(EditDefaultsOnly, Category="Impact", meta=(ClampMin=0.02, Units="s"))
	float KnockbackTime = 0.2f;

	UPROPERTY(EditDefaultsOnly, Category="Impact")
	TSubclassOf<UCameraShakeBase> CameraShake;

	UPROPERTY(EditDefaultsOnly, Category="Impact", meta=(ClampMin=0))
	float CameraShakeScale = 1.f;

	/** 1이면 슬로 없음. 0.3이면 30% 속도. */
	UPROPERTY(EditDefaultsOnly, Category="Impact", meta=(ClampMin=0.05, ClampMax=1))
	float SlowMoDilation = 1.f;

	UPROPERTY(EditDefaultsOnly, Category="Impact", meta=(ClampMin=0, Units="s"))
	float SlowMoDuration = 0.f;
};

/**
 * Plays hit reactions from UknightHitReactionComponent and parry events.
 *   Event.Damaged     -> Flinch (skipped under State.SuperArmor, below MinFlinchDamage, or during a heavier reaction)
 *   Event.PoiseBroken -> Stagger, or Knockdown when poise damage reaches KnockdownPoiseDamage
 *   Event.Parried     -> Stagger (the attacker recoils)
 *
 * Independent group with retrigger, so a stagger can interrupt a flinch. Attacks, guard and boost are
 * cancelled on activation and blocked while it runs.
 */
UCLASS()
class KNIGHT_API UknightGameplayAbility_HitReact : public UknightGameplayAbility
{
	GENERATED_BODY()

public:
	UknightGameplayAbility_HitReact(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual bool ShouldAbilityRespondToEvent(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayEventData* Payload) const override;
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	UPROPERTY(EditDefaultsOnly, Category="Hit React")
	FknightDirectionalMontages FlinchMontages;

	UPROPERTY(EditDefaultsOnly, Category="Hit React")
	FknightDirectionalMontages StaggerMontages;

	UPROPERTY(EditDefaultsOnly, Category="Hit React")
	FknightDirectionalMontages KnockdownMontages;

	/** Health lost below this does not flinch. */
	UPROPERTY(EditDefaultsOnly, Category="Hit React", meta=(ClampMin=0))
	float MinFlinchDamage = 1.f;

	/** Poise damage at or above this turns a stagger into a knockdown. */
	UPROPERTY(EditDefaultsOnly, Category="Hit React", meta=(ClampMin=0))
	float KnockdownPoiseDamage = 60.f;

	/** Applied to self when a stagger or knockdown ends, typically an instant override of Poise to MaxPoise. */
	UPROPERTY(EditDefaultsOnly, Category="Hit React")
	TSubclassOf<UGameplayEffect> PoiseRecoveryEffect;

	/** 맞는 맛 (2026-10-12). 반응 종류별 넉백·흔들림·슬로. */
	UPROPERTY(EditDefaultsOnly, Category="Hit React|Impact")
	FknightHitImpactFeel FlinchImpact;

	UPROPERTY(EditDefaultsOnly, Category="Hit React|Impact")
	FknightHitImpactFeel StaggerImpact;

	UPROPERTY(EditDefaultsOnly, Category="Hit React|Impact")
	FknightHitImpactFeel KnockdownImpact;

private:
	void ApplyImpactFeel(AActor& Avatar, const AActor* EventInstigator);
	const FknightHitImpactFeel& GetImpactFeel(EknightHitReactType Type) const;

	UFUNCTION()
	void OnMontageFinished();

	EknightHitReactType ResolveType(const FGameplayEventData& Payload) const;
	const FknightDirectionalMontages& GetMontages(EknightHitReactType Type) const;
	static FGameplayTag GetStateTag(EknightHitReactType Type);

	EknightHitReactType ActiveType = EknightHitReactType::Flinch;
	bool bStateTagApplied = false;
};
