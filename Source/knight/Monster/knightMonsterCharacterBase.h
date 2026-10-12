#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "AbilitySystem/knightAbilitySet.h"
#include "GameFramework/Character.h"
#include "GenericTeamAgentInterface.h"
#include "Interfaces/CombatInterface.h"
#include "Interfaces/TargetableInterface.h"
#include "knightMonsterCharacterBase.generated.h"

class UknightAbilitySystemComponent;
class UknightCoreAttributeSet;
class UknightHitReactionComponent;
class UknightHitStopComponent;
class UknightMeleeTraceComponent;
class UknightMonsterAttributeSet;
class UknightMonsterDefinition;
class UknightMonsterMovementComponent;
class UknightMonsterPartComponent;
class UknightMonsterPushOutComponent;
class UknightMonsterTargetingComponent;
class UMotionWarpingComponent;
struct FGameplayEventData;
struct FGameplayEffectSpec;

class AknightMonsterCharacterBase;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FknightMonsterDefeated, AknightMonsterCharacterBase*, Monster, AActor*, Killer);

/**
 * Base for every monster. One Blueprint per species sets the mesh, anim Blueprint, AI controller,
 * StateTree (on the controller) and MonsterDefinition.
 *
 * Collision: the capsule stays small and only handles movement. Hits land on the mesh's physics asset
 * bodies (object type PhysicsBody, query enabled), which UknightMonsterPartComponent maps to parts.
 */
UCLASS(Abstract)
class KNIGHT_API AknightMonsterCharacterBase : public ACharacter, public IAbilitySystemInterface, public ICombatInterface,
	public ITargetableInterface, public IGenericTeamAgentInterface
{
	GENERATED_BODY()

public:
	AknightMonsterCharacterBase(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//~ IAbilitySystemInterface
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	//~ ICombatInterface
	virtual bool CanReceiveHit(const AActor* Attacker) const override;
	virtual FGameplayTag ResolveHitPart(const FHitResult& Hit) const override;
	virtual float GetPartDamageMultiplier(const FGameplayTag& PartTag) const override;
	virtual bool IsDead() const override;

	//~ ITargetableInterface
	virtual bool IsTargetable() const override;
	virtual void GetTargetPoints(TArray<FknightTargetPoint>& OutPoints) const override;
	virtual float GetTargetRadius() const override;

	//~ IGenericTeamAgentInterface
	virtual FGenericTeamId GetGenericTeamId() const override;

	UknightAbilitySystemComponent* GetKnightAbilitySystemComponent() const { return AbilitySystemComponent; }
	UknightMonsterPartComponent* GetPartComponent() const { return PartComponent; }
	UknightMonsterTargetingComponent* GetTargetingComponent() const { return TargetingComponent; }
	UknightMonsterMovementComponent* GetMonsterMovementComponent() const;
	const UknightMonsterDefinition* GetMonsterDefinition() const { return MonsterDefinition; }

	/** Fires once when the monster dies. The raid flow listens to this to clear the hunt. */
	UPROPERTY(BlueprintAssignable, Category="Knight|Monster")
	FknightMonsterDefeated OnMonsterDefeated;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void PossessedBy(AController* NewController) override;

	void InitializeAbilitySystem();

	/** MonsterDefinition의 PrewarmEffects를 숨긴 채 한 번 켜서 PSO 준비를 미리 걸어 둔다. */
	void PrewarmEffects();

	void HandleOutOfHealth(AActor* DamageInstigator, AActor* DamageCauser, const FGameplayEffectSpec* EffectSpec,
		float Magnitude, float OldValue, float NewValue);
	void HandleRageMaxed(AActor* EffectInstigator, AActor* EffectCauser, const FGameplayEffectSpec* EffectSpec,
		float Magnitude, float OldValue, float NewValue);
	void HandleStaminaDepleted(AActor* EffectInstigator, AActor* EffectCauser, const FGameplayEffectSpec* EffectSpec,
		float Magnitude, float OldValue, float NewValue);

	/** 체력이 MonsterDefinition의 Phase2HealthRatio 밑으로 내려가면 2페이즈로 넘어간다. */
	void HandleDamaged(AActor* DamageInstigator, AActor* DamageCauser, const FGameplayEffectSpec* EffectSpec,
		float Magnitude, float OldValue, float NewValue);
	void HandleReactingChanged(const FGameplayTag Tag, int32 NewCount);
	void EnterPhase2();

	/** 퍼펙트 가드를 당하면(Event.Parried) MonsterDefinition의 ParriedPoiseDamageRatio만큼 경직도를 깎는다. */
	void HandleParried(const FGameplayEventData* Payload);

	/** The first target of this life sends Event.Monster.Engaged (an encounter roar ability triggers on it). */
	UFUNCTION()
	void HandleTargetChanged(AActor* OldTarget, AActor* NewTarget);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Knight|Monster")
	TObjectPtr<const UknightMonsterDefinition> MonsterDefinition;

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UknightAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UknightMonsterPartComponent> PartComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UknightMonsterTargetingComponent> TargetingComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UknightMeleeTraceComponent> MeleeTrace;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UknightHitStopComponent> HitStop;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UknightHitReactionComponent> HitReaction;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UMotionWarpingComponent> MotionWarping;

	/** 몸(몸 아래 차단 상자, 설정하면 메시 바디)과 겹친 플레이어를 밀어낸다. 검사할 몸이 없는 몬스터에서는 쉰다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UknightMonsterPushOutComponent> PushOut;

	UPROPERTY()
	TObjectPtr<UknightCoreAttributeSet> CoreAttributeSet;

	UPROPERTY()
	TObjectPtr<UknightMonsterAttributeSet> MonsterAttributeSet;

	UPROPERTY()
	FknightAbilitySet_GrantedHandles GrantedHandles;

	bool bAbilitySystemInitialized = false;
	bool bIsDying = false;
	bool bHasEngaged = false;
	bool bInPhase2 = false;
	/** 반응 중에 2페이즈 체력을 넘겨 State.Monster.Reacting이 풀리기를 기다리는 중. */
	bool bPhase2Pending = false;
	FDelegateHandle ReactingChangedHandle;
	/** Counted in the Monster.Alive perf gauge. */
	bool bCountedAlive = false;

	void SetCountedAlive(bool bAlive);
};
