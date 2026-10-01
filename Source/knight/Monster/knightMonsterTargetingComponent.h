#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "knightMonsterTargetingComponent.generated.h"

class UAbilitySystemComponent;
struct FGameplayEffectSpec;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FknightMonsterTargetChanged, AActor*, OldTarget, AActor*, NewTarget);

/**
 * Chooses what the monster fights. Replaces a classic threat table: in a solo raid the candidate set is
 * the Knight plus any objectives (a power plant) or allies added later.
 *
 * Candidates come from perception (UknightBossAIController forwards sight/hearing/damage) and from
 * AddObjective. Score = Priority / (1 + distance / DistanceFalloff) + recent damage threat.
 * Damage threat decays over time, so hitting the monster pulls it for a while.
 */
UCLASS(ClassGroup=(Knight), meta=(BlueprintSpawnableComponent))
class KNIGHT_API UknightMonsterTargetingComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UknightMonsterTargetingComponent();

	/** Binds damage threat to the owner's Core attribute set. Called by the monster once its ASC is ready. */
	void InitializeTargeting(UAbilitySystemComponent* InASC);

	/** Perception updates from the AI controller. */
	void NotifyPerceived(AActor* Actor, bool bSensed);

	/** A target the monster should consider without perceiving it (defended objective). */
	UFUNCTION(BlueprintCallable, Category="Knight|Monster")
	void AddObjective(AActor* Objective, float Priority = 0.5f);

	UFUNCTION(BlueprintCallable, Category="Knight|Monster")
	void RemoveCandidate(AActor* Actor);

	/** Re-scores candidates now instead of waiting for the next evaluation. */
	UFUNCTION(BlueprintCallable, Category="Knight|Monster")
	void EvaluateTarget();

	UFUNCTION(BlueprintPure, Category="Knight|Monster")
	AActor* GetCurrentTarget() const { return CurrentTarget.Get(); }

	UPROPERTY(BlueprintAssignable, Category="Knight|Monster")
	FknightMonsterTargetChanged OnTargetChanged;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Seconds between automatic evaluations. */
	UPROPERTY(EditDefaultsOnly, Category="Knight|Monster", meta=(ClampMin=0.05, Units="s"))
	float EvaluationInterval = 0.5f;

	/** Priority of perceived hostile pawns. Objectives use the priority passed to AddObjective. */
	UPROPERTY(EditDefaultsOnly, Category="Knight|Monster", meta=(ClampMin=0))
	float PerceivedPriority = 1.f;

	UPROPERTY(EditDefaultsOnly, Category="Knight|Monster", meta=(ClampMin=1, Units="cm"))
	float DistanceFalloff = 3000.f;

	/** Threat added per point of damage taken from a candidate. */
	UPROPERTY(EditDefaultsOnly, Category="Knight|Monster", meta=(ClampMin=0))
	float ThreatPerDamage = 0.01f;

	/** Fraction of threat lost per second. */
	UPROPERTY(EditDefaultsOnly, Category="Knight|Monster", meta=(ClampMin=0, ClampMax=1))
	float ThreatDecayPerSecond = 0.1f;

	/** A new target must beat the current score by this factor, so the monster does not flip between targets. */
	UPROPERTY(EditDefaultsOnly, Category="Knight|Monster", meta=(ClampMin=1))
	float SwitchHysteresis = 1.25f;

private:
	struct FCandidate
	{
		TWeakObjectPtr<AActor> Actor;
		float Priority = 1.f;
		float Threat = 0.f;
		bool bPerceived = false;
		bool bObjective = false;
	};

	void HandleDamaged(AActor* DamageInstigator, AActor* DamageCauser, const FGameplayEffectSpec* EffectSpec,
		float Magnitude, float OldValue, float NewValue);
	FCandidate* FindCandidate(const AActor* Actor);
	float ScoreCandidate(const FCandidate& Candidate) const;
	bool IsCandidateValid(const FCandidate& Candidate) const;
	void SetCurrentTarget(AActor* NewTarget);

	TArray<FCandidate> Candidates;
	TWeakObjectPtr<AActor> CurrentTarget;
	TWeakObjectPtr<UAbilitySystemComponent> BoundASC;
	FTimerHandle EvaluationTimer;
	double LastEvaluationTime = 0.0;
};
