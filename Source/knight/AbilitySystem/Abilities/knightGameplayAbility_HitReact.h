#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/knightGameplayAbility.h"
#include "knightGameplayAbility_HitReact.generated.h"

class UAnimMontage;
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

private:
	UFUNCTION()
	void OnMontageFinished();

	EknightHitReactType ResolveType(const FGameplayEventData& Payload) const;
	const FknightDirectionalMontages& GetMontages(EknightHitReactType Type) const;
	static FGameplayTag GetStateTag(EknightHitReactType Type);

	EknightHitReactType ActiveType = EknightHitReactType::Flinch;
	bool bStateTagApplied = false;
};
