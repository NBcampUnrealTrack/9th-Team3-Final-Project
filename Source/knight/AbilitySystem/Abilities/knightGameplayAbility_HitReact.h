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
	Knockdown,
	/** 플레이어 그로기(기절): 시작 → 반복(GroggyDuration) → 끝. */
	Groggy
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
 *
 * 그로기(기절): GroggyLoopMontage가 있으면 Event.PoiseBroken은 휘청·다운 대신 기절이 된다.
 *   흐름: GroggyStartMontage → GroggyLoopMontage 반복 GroggyDuration초(기본 2.5) → GroggyEndMontage(한손 스탠스면 GroggyEndMontageOneHanded).
 *   그 동안 이동 입력이 잠기고(무브먼트 SetMoveInputLocked) 공격·가드·퀵스텝이 막힌다(State.HitReact).
 *   맞아도 피해만 받고 다른 피격 반응 없이 기절이 이어진다(State.HitReact.Groggy 동안 반응 이벤트 무시). 끝나면 PoiseRecoveryEffect로 그로기를 채운다.
 *   맞는 맛(넉백·흔들림·슬로)은 StaggerImpact를 쓴다. GroggyLoopMontage를 비우면 예전처럼 휘청·다운.
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

	/** 그로기 시작(무너지는 동작). 비어 있으면 바로 반복으로 간다. */
	UPROPERTY(EditDefaultsOnly, Category="Hit React|Groggy")
	TObjectPtr<UAnimMontage> GroggyStartMontage;

	/** 그로기 반복(비틀거림). 비어 있으면 그로기 없이 예전처럼 휘청·다운. 몽타주가 끝나도 GroggyDuration 동안 다시 재생한다. */
	UPROPERTY(EditDefaultsOnly, Category="Hit React|Groggy")
	TObjectPtr<UAnimMontage> GroggyLoopMontage;

	/** 그로기 끝(정신 차리는 동작), 양손 스탠스. 비어 있으면 반복이 끝나는 순간 끝난다. */
	UPROPERTY(EditDefaultsOnly, Category="Hit React|Groggy")
	TObjectPtr<UAnimMontage> GroggyEndMontage;

	/** 그로기 끝, 한손 스탠스. 비어 있으면 GroggyEndMontage. */
	UPROPERTY(EditDefaultsOnly, Category="Hit React|Groggy")
	TObjectPtr<UAnimMontage> GroggyEndMontageOneHanded;

	/** 그로기 반복 시간(초, 시작·끝 동작 제외). 바뀔 수 있는 값. */
	UPROPERTY(EditDefaultsOnly, Category="Hit React|Groggy", meta=(ClampMin=0, Units="s"))
	float GroggyDuration = 2.5f;

private:
	/** 그로기 단계: 시작 → 반복 → 끝. 몽타주가 끝날 때(블렌드 아웃) 다음 단계로 넘어간다. */
	enum class EGroggyPhase : uint8 { None, Start, Loop, End };

	void StartGroggy();
	void EnterGroggyLoop();
	/** 지금 그로기 몽타주 태스크를 끝내고(콜백 끊기) 새 몽타주를 재생한다. */
	void PlayGroggyMontage(UAnimMontage* Montage);

	UFUNCTION()
	void OnGroggyMontageDone();

	UFUNCTION()
	void OnGroggyTimeUp();

	EGroggyPhase GroggyPhase = EGroggyPhase::None;
	double GroggyLoopStartTime = 0.0;
	TWeakObjectPtr<class UAbilityTask_PlayMontageAndWait> GroggyMontageTask;
	bool bGroggyMoveLockApplied = false;

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
