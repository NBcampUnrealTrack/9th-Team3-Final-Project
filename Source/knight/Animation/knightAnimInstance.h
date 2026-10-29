#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "GameplayEffectTypes.h"
#include "knightAnimInstance.generated.h"

class ACharacter;
class UAbilitySystemComponent;
class UAnimMontage;
class UAudioComponent;
class UNiagaraSystem;
class USoundBase;

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

	/**
	 * 루트 모션 몽타주가 캡슐을 움직이는 동안 GroundSpeed를 0으로 둔다. 몬스터용: 다운·구르기처럼 몽타주가 몸을 옮기면 그 속도로
	 * 이동 블렌드스페이스가 걷기로 바뀌고, 몽타주가 덜 섞인 블렌드 구간(특히 히트스톱으로 멈춘 첫 순간)에 그 걷기 자세가 비쳐 몸이 튄다.
	 */
	UPROPERTY(EditDefaultsOnly, Category="Knight|Locomotion")
	bool bIgnoreRootMotionVelocity = false;

	/**
	 * (2026-10-22) 발소리. 걷기·달리기 애니에 노티파이를 넣지 않고, 발 뼈(FootstepBones)가 들렸다가 땅에 닿는 순간을 찾아 낸다.
	 * 발 높이 = 뼈 높이 - 메시 원점 높이. 발마다 최근 최저·최고 높이를 기억해 그 폭(발을 드는 높이)에 맞춘다:
	 * 폭의 FootstepLiftRatio 위로 올랐다가 FootstepPlantRatio 아래로 내려오면 한 걸음. 메카 크기·걸음에 상관없이 맞는다.
	 * (처음에는 고정 높이 10cm였는데 이 메카 걸음에서는 한 번도 걸리지 않았다.) 땅 위에서 FootstepMinSpeed보다 빠를 때만.
	 * 달리는 중(UknightMovementComponent::IsRunning)이면 FootstepRunSound, 아니면 FootstepWalkSound. 비우면 나지 않는다(몬스터 기본).
	 */
	UPROPERTY(EditDefaultsOnly, Category="Knight|Footstep")
	TObjectPtr<USoundBase> FootstepWalkSound;

	UPROPERTY(EditDefaultsOnly, Category="Knight|Footstep")
	TObjectPtr<USoundBase> FootstepRunSound;

	/**
	 * (2026-10-28) 발소리 변주. 비어 있지 않으면 FootstepWalkSound / FootstepRunSound 대신 매 걸음 여기서 무작위로
	 * (직전과 다른 것을) 고른다. Sound Cue 없이 변주 수를 늘릴 수 있다.
	 */
	UPROPERTY(EditDefaultsOnly, Category="Knight|Footstep")
	TArray<TObjectPtr<USoundBase>> FootstepWalkSounds;

	UPROPERTY(EditDefaultsOnly, Category="Knight|Footstep")
	TArray<TObjectPtr<USoundBase>> FootstepRunSounds;

	/** 매 걸음 피치를 1 ± 이 값 안에서 흔든다(같은 소리가 기계적으로 반복되지 않게). */
	UPROPERTY(EditDefaultsOnly, Category="Knight|Footstep", meta=(ClampMin=0, ClampMax=0.5))
	float FootstepPitchVariance = 0.04f;

	/**
	 * (2026-10-28) 물 발소리. 디딘 발이 수면 아래면(UknightWaterRippleSubsystem::FindWaterSurface)
	 *   얕을 때: 발소리 위에 FootstepWaterSounds(물 가르기 레이어)를 겹친다.
	 *   FootstepDeepWaterDepth 이상 깊을 때: 발소리 대신 FootstepDeepWaterSounds를 낸다.
	 * 비우면 물을 확인하지 않는다.
	 */
	UPROPERTY(EditDefaultsOnly, Category="Knight|Footstep|Water")
	TArray<TObjectPtr<USoundBase>> FootstepWaterSounds;

	UPROPERTY(EditDefaultsOnly, Category="Knight|Footstep|Water")
	TArray<TObjectPtr<USoundBase>> FootstepDeepWaterSounds;

	/** 발 뼈에서 수면까지 이 깊이 이상이면 깊은 물 발소리. */
	UPROPERTY(EditDefaultsOnly, Category="Knight|Footstep|Water", meta=(ClampMin=0, Units="cm"))
	float FootstepDeepWaterDepth = 45.f;

	UPROPERTY(EditDefaultsOnly, Category="Knight|Footstep|Water", meta=(ClampMin=0))
	float FootstepWaterVolume = 0.8f;

	/**
	 * (2026-10-28) 서보 구동 루프. 땅 위를 움직이는 동안 작게 깔리는 기계 구동음으로, 속도에 따라 볼륨·피치가 오른다.
	 * 멈추거나 공중이면 서서히 사라진다. 비우면 없다(몬스터 기본).
	 */
	UPROPERTY(EditDefaultsOnly, Category="Knight|Servo")
	TObjectPtr<USoundBase> ServoLoopSound;

	UPROPERTY(EditDefaultsOnly, Category="Knight|Servo", meta=(ClampMin=0))
	float ServoLoopMaxVolume = 0.35f;

	/** 이 수평 속도에서 볼륨·피치가 최대가 된다. */
	UPROPERTY(EditDefaultsOnly, Category="Knight|Servo", meta=(ClampMin=1, Units="CentimetersPerSecond"))
	float ServoLoopFullSpeed = 650.f;

	UPROPERTY(EditDefaultsOnly, Category="Knight|Servo", meta=(ClampMin=0.1, ClampMax=4))
	float ServoLoopPitchSlow = 0.9f;

	UPROPERTY(EditDefaultsOnly, Category="Knight|Servo", meta=(ClampMin=0.1, ClampMax=4))
	float ServoLoopPitchFast = 1.1f;

	/** 볼륨이 목표를 따라가는 속도(초당 최대 볼륨 비율). */
	UPROPERTY(EditDefaultsOnly, Category="Knight|Servo", meta=(ClampMin=0.1))
	float ServoLoopFadeRate = 3.f;

	UPROPERTY(EditDefaultsOnly, Category="Knight|Footstep")
	TArray<FName> FootstepBones = { TEXT("foot_l"), TEXT("foot_r") };

	UPROPERTY(EditDefaultsOnly, Category="Knight|Footstep", meta=(ClampMin=0))
	float FootstepVolume = 1.f;

	UPROPERTY(EditDefaultsOnly, Category="Knight|Footstep", meta=(ClampMin=0, Units="CentimetersPerSecond"))
	float FootstepMinSpeed = 50.f;

	/** 발 높이 폭(최저~최고)에서 이 비율 위로 올라가면 '발을 들었다'. */
	UPROPERTY(EditDefaultsOnly, Category="Knight|Footstep", meta=(ClampMin=0, ClampMax=1))
	float FootstepLiftRatio = 0.5f;

	/** 폭에서 이 비율 아래로 내려오면 '땅에 닿았다'(소리). 소리가 늦으면 올린다. */
	UPROPERTY(EditDefaultsOnly, Category="Knight|Footstep", meta=(ClampMin=0, ClampMax=1))
	float FootstepPlantRatio = 0.25f;

	/** 폭이 이보다 작으면(제자리·미끄러짐) 걸음으로 보지 않는다. */
	UPROPERTY(EditDefaultsOnly, Category="Knight|Footstep", meta=(ClampMin=0, Units="cm"))
	float FootstepMinLift = 2.f;

	/** 같은 발이 다시 소리 내기까지 최소 간격. */
	UPROPERTY(EditDefaultsOnly, Category="Knight|Footstep", meta=(ClampMin=0, Units="s"))
	float FootstepCooldown = 0.2f;

	/** 달리기(IsRunning) 중 발이 땅에 닿을 때 발밑에서 흙먼지 이펙트를 피어오르게 할지 여부. */
	UPROPERTY(EditDefaultsOnly, Category="Knight|Footstep")
	bool bSpawnFootstepRunDust = true;

	/** 달리기 발걸음 흙먼지 이펙트 (비우면 NS_Monster_RunDust). */
	UPROPERTY(EditDefaultsOnly, Category="Knight|Footstep", meta=(EditCondition="bSpawnFootstepRunDust"))
	TObjectPtr<UNiagaraSystem> FootstepRunDustEffect;

	/** 달리기 발걸음 흙먼지 크기 배율. */
	UPROPERTY(EditDefaultsOnly, Category="Knight|Footstep", meta=(ClampMin=0.1, EditCondition="bSpawnFootstepRunDust"))
	float FootstepRunDustScale = 0.65f;

private:
	/** (2026-10-22) 발소리 검사. 게임 스레드(NativeUpdateAnimation)에서 부른다. */
	void UpdateFootsteps(float DeltaSeconds, const ACharacter& Character);

	struct FFootState
	{
		float MinHeight = UE_BIG_NUMBER;
		float MaxHeight = -UE_BIG_NUMBER;
		bool bLifted = false;
		double LastStepTime = -UE_BIG_NUMBER;
	};
	TArray<FFootState> FootStates;

	/** (2026-10-28) 변주 배열에서 직전과 다른 것을 고른다. 비었으면 nullptr. */
	static USoundBase* PickVariation(const TArray<TObjectPtr<USoundBase>>& Sounds, int32& LastIndex);
	int32 LastWalkIndex = INDEX_NONE;
	int32 LastRunIndex = INDEX_NONE;
	int32 LastWaterIndex = INDEX_NONE;
	int32 LastDeepWaterIndex = INDEX_NONE;

	/** (2026-10-28) 서보 구동 루프. 게임 월드에서만 켠다. */
	void UpdateServoLoop(float DeltaSeconds);
	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> ServoLoopAudio;
	float ServoLoopCurrentVolume = 0.f;

	// Game thread copies read by the thread-safe update.
	FVector CachedVelocity = FVector::ZeroVector;
	bool bCachedIsFalling = false;
	bool bCachedHasAcceleration = false;
	bool bCachedPlayingRootMotion = false;
	bool bAbilitySystemBound = false;
	TWeakObjectPtr<UAbilitySystemComponent> BoundASC;
};
