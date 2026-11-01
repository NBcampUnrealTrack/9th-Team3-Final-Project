#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "AbilitySystem/knightAbilitySet.h"
#include "GameFramework/Character.h"
#include "GenericTeamAgentInterface.h"
#include "Interfaces/CombatInterface.h"
#include "Interfaces/TargetableInterface.h"
#include "knightMonsterCharacterBase.generated.h"

class UAnimSequence;
class UAudioComponent;
enum class EknightHitSide : uint8;
class UknightAbilitySystemComponent;
class UknightCoreAttributeSet;
class UknightHitReactionComponent;
class UknightHitStopComponent;
class UknightMeleeTraceComponent;
class UknightMonsterAttributeSet;
class UknightMonsterDefinition;
class UknightMonsterHoverComponent;
class UknightMonsterAirTurnComponent;
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
/** 지금 얹을 덧셈 피격(Knight Hit Flinch 애님 노드가 읽는다). Count가 바뀌면 새 피격이다. */
struct FknightHitFlinchState
{
	const UAnimSequence* Sequence = nullptr;
	float Strength = 0.f;
	float PlayRate = 1.f;
	float BlendIn = 0.05f;
	float BlendOut = 0.3f;
	int32 Count = 0;
};

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

	/** 지금 얹을 덧셈 피격. Knight Hit Flinch 애님 노드가 게임 스레드(PreUpdate)에서 읽는다. */
	const FknightHitFlinchState& GetHitFlinch() const { return HitFlinchState; }

	/** Victim 기준으로 From(맞은 자리나 공격한 쪽)이 어느 쪽인가. 앞뒤 ±45° 안이면 앞·뒤, 아니면 좌·우. */
	static EknightHitSide GetHitSide(const AActor& Victim, const FVector& From);

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

	/** 받은 피해로 Rage를 올리고(RageFullDamageRatio), 체력이 Phase2HealthRatio 밑으로 내려가면 2페이즈로 넘어간다. */
	void HandleDamaged(AActor* DamageInstigator, AActor* DamageCauser, const FGameplayEffectSpec* EffectSpec,
		float Magnitude, float OldValue, float NewValue);
	void HandleReactingChanged(const FGameplayTag Tag, int32 NewCount);
	void EnterPhase2();
	void AddRageFromDamage(float Damage, AActor* DamageInstigator);

	/** 퍼펙트 가드를 당하면(Event.Parried) MonsterDefinition의 ParriedPoiseDamageRatio만큼 경직도를 깎는다. */
	void HandleParried(const FGameplayEventData* Payload);

	/** ParriedPoiseBlockedTags가 붙으면 경직도를 가득 채운다. */
	void HandleParriedPoiseBlockedChanged(const FGameplayTag Tag, int32 NewCount);

	void HandleEnragedChanged(const FGameplayTag Tag, int32 NewCount);

	/**
	 * State.Monster.Enraged가 붙거나 떨어질 때 부른다(분노 시작·끝). 분노 외형(잔향 공명 날개 색 등)을 Blueprint에서 바꾼다.
	 * 분노 중에 죽어도 끝(false)은 부르지 않는다.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category="Knight|Monster")
	void OnEnragedChanged(bool bEnraged);

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

	/** 떠 있게 만든 애니메이션을 바닥 가까이 내린다(날짐승). HoverDrop이 0인 몬스터에서는 쉰다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UknightMonsterHoverComponent> Hover;

	/** 지상 턴과 공중 턴을 번갈아 돌린다(날짐승). GroundTurnTime이 0이면 꺼져 있다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UknightMonsterAirTurnComponent> AirTurn;

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

	/** MonsterDefinition의 LoopSounds 중 bWaitForEngage가 bEngaged와 같은 것을 튼다. */
	void StartLoopSounds(bool bEngaged);
	void StopLoopSounds();
	void HandleLoopSoundMuteChanged(const FGameplayTag Tag, int32 NewCount);
	/** 태그로 줄여 두거나 BurstTime 사이에 쉬는 중이면 줄인 볼륨, 아니면 원래 볼륨. */
	float GetLoopVolume(int32 Index) const;
	/** BurstTime을 쓰는 소리를 들림 ↔ 쉼으로 바꾸고 다음 전환을 예약한다. */
	void ToggleLoopBurst(int32 Index);

	/** 분노 마무리 공격(MonsterDefinition::EnrageFinaleAttack) 진행 단계: 분노 중 대기 → 남은 시간이 짧아져 쓸 준비 → 쓰는 중. */
	enum class EEnrageFinale : uint8 { None, Waiting, Pending, Running };
	EEnrageFinale EnrageFinale = EEnrageFinale::None;
	double EnrageFinaleGiveUpTime = 0.0;
	FTimerHandle EnrageFinaleTimer;
	void UpdateEnrageFinale();
	void EndEnrageFinale();

	/** LoopSounds와 같은 순서. 아직 틀지 않은 소리는 비어 있다. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UAudioComponent>> LoopAudios;

	/** 맞은 쪽에서 움찔하는 덧셈 피격을 건다(MonsterDefinition::HitFlinch). */
	void TriggerHitFlinch(AActor* DamageInstigator, const FGameplayEffectSpec* EffectSpec, float Damage);
	FknightHitFlinchState HitFlinchState;
	double LastHitFlinchTime = -1.e9;
	/** LoopSounds와 같은 순서. BurstTime을 쓰는 소리가 지금 쉬는 중인지. */
	TArray<bool> LoopResting;
	TArray<FTimerHandle> LoopBurstTimers;
};
