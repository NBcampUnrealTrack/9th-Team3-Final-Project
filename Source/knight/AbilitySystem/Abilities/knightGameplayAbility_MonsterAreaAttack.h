#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/knightGameplayAbility_MonsterAttack.h"
#include "Combat/knightCombatTypes.h"
#include "Monster/knightMonsterKnockback.h"
#include "knightGameplayAbility_MonsterAreaAttack.generated.h"

class UAnimMontage;
class UGameplayEffect;
class UAudioComponent;
class UNiagaraComponent;
class UNiagaraSystem;
class USoundBase;

/** 내려찍기처럼 몸의 한 부분이 땅에 닿는 순간마다 따로 때리는 판정. 그 순간 소켓 위치에 구체를 놓는다. */
USTRUCT(BlueprintType)
struct FknightMonsterAreaImpact
{
	GENERATED_BODY()

	/** 발동부터 판정까지의 시간. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Impact", meta=(ClampMin=0, Units="s"))
	float Time = 0.8f;

	/** 판정 구체의 중심이 되는 소켓(본). 판정 순간의 위치를 쓴다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Impact")
	FName Socket;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Impact", meta=(ClampMin=1, Units="cm"))
	float Radius = 300.f;

	/**
	 * 0보다 크면 판정 순간 소켓이 땅(이동 캡슐 바닥)에서 이 높이보다 높을 때는 때리지 않는다.
	 * 몽타주가 끊기거나 늦어져 발이 아직 공중에 있는데 맞는 일을 막는다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Impact", meta=(ClampMin=0, Units="cm"))
	float MaxHeight = 0.f;
};

/**
 * 몽타주 대신 어빌리티가 타이밍을 잡는 몬스터 공격. 예고 → 몬스터 앞쪽 구체 오버랩(내려찍기, 발구르기는 한 번,
 * 브레스처럼 이어지는 공격은 ActiveTime 동안 계속) → 후딜 순서로 진행한다. 대상마다 한 번만 맞는다.
 *
 * 구체 안의 적대적인 ICombatInterface 액터마다 UknightCombatLibrary::ApplyMeleeHit을 한 번씩 거치므로
 * 가드, 패링, 회피, 피해가 무기 타격과 같은 규칙을 따른다. 스켈레톤 없이도 동작하므로 테스트 맵의 큐브
 * 대역에 쓸 수 있고, 실제 몬스터도 범위 공격에 쓸 수 있다. 이때 Montage는 연출용으로만 재생하고 타이밍은 여기서 잡는다.
 *
 * ConeHalfAngle을 주면 구체 안에서도 원뿔(ConeOrigin에서 정면으로 좌우 ConeHalfAngle) 안의 대상만 맞는다.
 *
 * LungeMaxDistance를 주면 LungeStartTime ~ LungeEndTime 동안 타깃 쪽으로 몸을 던진다(애니메이션에 이동이 없는 몸통 박치기).
 * 방향과 거리는 돌진을 시작할 때 정하고 그 뒤로는 따라가지 않으므로, 몸을 끌어올리는 예고를 보고 옆으로 피할 수 있다.
 *
 * Impacts를 주면 몬스터 앞쪽 구체 하나 대신 판정마다 정한 시각에 소켓(앞발 등) 위치를 때린다. 두 앞발이 떨어져 닿는
 * 내려찍기를 구체 하나로 덮으면 발이 닿지 않는 앞쪽까지 맞는다.
 *
 * KnockbackSpeed를 주면 맞은 대상을 몬스터 진행 경로 옆으로 날려 보낸다(몸통 박치기에 맞고 몸 앞에 붙어 끌려가지 않게).
 * bKnockbackAway면 몬스터 중심에서 바깥으로 곧장 날린다(돌풍·충격파).
 *
 * 시각(WindupTime, Impacts, LungeStartTime 등)은 몬스터 시간(CustomTimeDilation을 곱한 시간)으로 잰다. 히트스톱으로 몽타주가
 * 멈추는 동안 판정 시계도 같이 멈추므로, 예비 동작 중에 맞아도 판정이 애니메이션보다 먼저 나오지 않는다.
 *
 * 디버그: knight.Monster.DrawAttacks 1 이면 예고(주황)와 타격 지점(빨강)을 그린다.
 */
UCLASS()
class KNIGHT_API UknightGameplayAbility_MonsterAreaAttack : public UknightGameplayAbility_MonsterAttack
{
	GENERATED_BODY()

public:
	UknightGameplayAbility_MonsterAreaAttack(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

protected:
	/** 오버랩을 실행하고 타격을 적용한다. 타격 순간에 효과를 더하려면 오버라이드한다. */
	virtual void OnImpact();

	/** 발동부터 타격까지의 시간. 플레이어가 공격을 읽고 피할 수 있는 구간이다. */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Timing", meta=(ClampMin=0, Units="s"))
	float WindupTime = 0.8f;

	/** 타격 순간부터 판정이 유지되는 시간. 0이면 타격 순간 한 번만 판정한다. 유지되는 동안 구체에 들어온 대상도 맞는다. */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Timing", meta=(ClampMin=0, Units="s"))
	float ActiveTime = 0.f;

	/** 판정이 끝난 뒤 어빌리티가 끝나기까지의 시간. 플레이어가 반격할 수 있는 구간이다. */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Timing", meta=(ClampMin=0, Units="s"))
	float RecoveryTime = 1.f;

	/** 몬스터 로컬 공간 기준 구체 중심(X가 정면). AreaSocket이 있으면 그 소켓 위치에서 몬스터 방향 기준으로 더하는 오프셋이다. */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Area")
	FVector AreaOffset = FVector(350.f, 0.f, 0.f);

	/**
	 * 있으면 구체가 이 소켓(본)을 따라간다. 머리를 휘두르는 몸통 박치기처럼 치는 부위가 판정 동안 크게 움직이는 공격에 쓴다.
	 * ActiveTime 동안 판정할 때마다 그 순간의 소켓 위치를 쓴다.
	 */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Area")
	FName AreaSocket;

	UPROPERTY(EditDefaultsOnly, Category="Attack|Area", meta=(ClampMin=1, Units="cm"))
	float AreaRadius = 300.f;

	/**
	 * 비어 있지 않으면 WindupTime의 AreaOffset 구체 대신 이 판정들을 쓴다(시간 순서대로 둔다). 마지막 판정 뒤 바로 후딜로 넘어가며
	 * ActiveTime은 쓰지 않는다. 예고 큐와 디버그 예고 표시는 그대로 AreaOffset 위치에 나온다.
	 */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Area")
	TArray<FknightMonsterAreaImpact> Impacts;

	/**
	 * 0보다 크면 구체 안이라도 ConeOrigin에서 정면 기준 좌우 이 각도 안에 있는 대상만 맞는다(브레스처럼 입에서 뻗는 공격).
	 * 0이면 구체 전체를 판정한다.
	 */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Area", meta=(ClampMin=0, ClampMax=90, Units="deg"))
	float ConeHalfAngle = 0.f;

	/** 몬스터 로컬 공간 기준 원뿔 꼭짓점(입 위치). ConeHalfAngle이 0보다 클 때만 쓴다. */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Area", meta=(EditCondition="ConeHalfAngle > 0"))
	FVector ConeOrigin = FVector::ZeroVector;

	/**
	 * 켜면 판정이 유지되는 동안(WindupTime부터 ActiveTime 동안) 몸은 그대로 두고 고개만 타깃 쪽으로 천천히 돌려 쓴다(브레스).
	 * 몸 정면에서 시작해 초당 AimSweepRate도로 따라가고 좌우 AimSweepMaxYaw를 넘지 않는다. 범위·원뿔·바닥 이펙트가 이 방향을
	 * 따르고, 머리는 Look At 노드가 같은 각도로 돌린다(입에 붙은 이펙트가 함께 돈다). Impacts를 쓰는 공격에는 쓰지 않는다.
	 */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Aim Sweep")
	bool bAimSweep = false;

	/** 고개가 타깃을 따라가는 속도. 플레이어가 옆으로 달리면 벗어나고, 가만히 있으면 맞을 만큼. */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Aim Sweep", meta=(ClampMin=0, Units="deg", EditCondition="bAimSweep"))
	float AimSweepRate = 20.f;

	/** 몸 정면 기준 최대 각도. Look At 노드의 MaxYaw(잔열 45도)보다 크면 머리는 거기서 멈춘다. */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Aim Sweep", meta=(ClampMin=0, ClampMax=90, Units="deg", EditCondition="bAimSweep"))
	float AimSweepMaxYaw = 45.f;

	/** 선택 사항. 고개가 AimSweepGroundFXStep도 돌 때마다 그 방향 바닥에 남기는 이펙트(브레스 그을림). 처음 방향은 몽타주 노티파이가 남긴다. */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Aim Sweep", meta=(EditCondition="bAimSweep"))
	TObjectPtr<UNiagaraSystem> AimSweepGroundFX;

	UPROPERTY(EditDefaultsOnly, Category="Attack|Aim Sweep", meta=(ClampMin=1, Units="deg", EditCondition="bAimSweep"))
	float AimSweepGroundFXStep = 15.f;

	/** 몸 위치에서 고개 방향으로 이만큼 앞 바닥에 남긴다(월드 cm). */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Aim Sweep", meta=(ClampMin=0, Units="cm", EditCondition="bAimSweep"))
	float AimSweepGroundFXDistance = 1500.f;

	/**
	 * 0보다 크면 돌진한다. 구체 중심(AreaOffset.X 앞)이 타깃에 오도록 타깃까지 거리 - AreaOffset.X만큼 가되 이 거리를 넘지 않는다.
	 * 가까우면 제자리에서 친다.
	 */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Lunge", meta=(ClampMin=0, Units="cm"))
	float LungeMaxDistance = 0.f;

	/** 돌진 구간(발동부터 초). 끝나는 시각은 WindupTime 근처로 두어 몸이 닿는 순간 판정이 난다. */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Lunge", meta=(ClampMin=0, Units="s", EditCondition="LungeMaxDistance > 0"))
	float LungeStartTime = 0.4f;

	UPROPERTY(EditDefaultsOnly, Category="Attack|Lunge", meta=(ClampMin=0, Units="s", EditCondition="LungeMaxDistance > 0"))
	float LungeEndTime = 0.8f;

	UPROPERTY(EditDefaultsOnly, Category="Attack")
	FknightMeleeHitParams HitParams;

	/** 맞은 대상을 날려 보내는 수평 속도(몬스터 정면에서 대상 쪽 옆으로 약 63도). 0이면 날리지 않는다. */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Knockback", meta=(ClampMin=0, Units="cm/s"))
	float KnockbackSpeed = 0.f;

	/** 날려 보내는 시간. 이동 거리는 KnockbackSpeed x KnockbackTime이다. */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Knockback", meta=(ClampMin=0, Units="s", EditCondition="KnockbackSpeed > 0"))
	float KnockbackTime = 0.4f;

	/** 가드로 막았을 때 날려 보내는 세기 배율. */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Knockback", meta=(ClampMin=0, ClampMax=1, EditCondition="KnockbackSpeed > 0"))
	float GuardKnockbackScale = 0.5f;

	/** 켜면 몬스터 중심에서 대상 쪽 바깥으로 곧장 날린다(돌풍·충격파). 끄면 진행 경로 옆으로 날린다(몸통 박치기). */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Knockback", meta=(EditCondition="KnockbackSpeed > 0"))
	bool bKnockbackAway = false;

	/** 0보다 크면 실제로 맞은 플레이어가 이 시간 동안 멍해진다(초음파 피격과 같은 소리 줄임·화면 울림). */
	UPROPERTY(EditDefaultsOnly, Category="Attack", meta=(ClampMin=0, Units="s"))
	float HitDeafenTime = 0.f;

	/** 선택 사항. 실제로 맞은 대상(가드·패링·회피 제외)에게 추가로 거는 효과(브레스 화상 등). */
	UPROPERTY(EditDefaultsOnly, Category="Attack")
	TSubclassOf<UGameplayEffect> HitTargetEffect;

	/** 선택 사항. 연출용으로만 재생하며 어빌리티가 끝나면 멈춘다. */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Presentation")
	TObjectPtr<UAnimMontage> Montage;

	/** 선택 사항. WindupEffectStartTime부터 첫 타격까지 WindupEffectSocket에 붙여 재생한다(기 모으기). 공격이 끊겨도 끈다. */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Presentation")
	TObjectPtr<UNiagaraSystem> WindupEffect;

	UPROPERTY(EditDefaultsOnly, Category="Attack|Presentation")
	FName WindupEffectSocket;

	UPROPERTY(EditDefaultsOnly, Category="Attack|Presentation", meta=(ClampMin=0, Units="s"))
	float WindupEffectStartTime = 0.f;

	UPROPERTY(EditDefaultsOnly, Category="Attack|Presentation", meta=(ClampMin=0.01))
	float WindupEffectScale = 1.f;

	/** 선택 사항. WindupEffect와 같은 때 같은 자리에서 재생하고 첫 타격에 짧게 줄여 끊는다. */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Presentation")
	TObjectPtr<USoundBase> WindupSound;

	/** 선택 사항. 첫 타격 순간 범위 중심 아래 바닥에서 몬스터가 보는 방향으로 재생한다(충격파 고리, 돌풍). */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Presentation")
	TArray<TObjectPtr<UNiagaraSystem>> HitAreaEffects;

	/** HitAreaEffects 크기 배율. 범위만큼 퍼지게 맞춘다. */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Presentation", meta=(ClampMin=0.01))
	float HitAreaEffectScale = 1.f;

	UPROPERTY(EditDefaultsOnly, Category="Attack|Presentation")
	TObjectPtr<USoundBase> HitAreaSound;

	/** 선택 사항. 예고가 시작될 때 범위 중심에서 실행하는 큐(바닥 데칼, 포효 등). */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Presentation", meta=(Categories="GameplayCue"))
	FGameplayTag TelegraphCue;

private:
	/** 몬스터 시간을 한 칸 진행하고, 그 사이에 지난 돌진·판정·후딜 시각을 처리한다. */
	UFUNCTION()
	void HandleClockTick(int32 ActionNumber);

	void StartLunge();

	UFUNCTION()
	void HandleKnockbackTick(int32 ActionNumber);

	void ApplyImpact(const FknightMonsterAreaImpact& Impact);
	void HitArea(const FVector& Center, float Radius);
	void PlayHitAreaEffects(const FVector& Center) const;
	void StartWindupEffect();
	void StopWindupEffect();

	FVector GetAreaCenter() const;
	bool IsInsideCone(const AActor& Avatar, const FVector& Point) const;
	/** 공격 방향 기준 변환: 액터 변환에 고개 쓸기 각도(AimYaw)를 더한다. 쓸지 않으면 액터 변환 그대로다. */
	FTransform GetAimTransform(const AActor& Avatar) const;
	/** 고개 쓸기 각도를 몬스터 시간 DeltaTime만큼 타깃 쪽으로 돌리고, 머리와 바닥 이펙트에 반영한다. */
	void UpdateAimSweep(float DeltaTime);
	void StopAimSweep();
	void SpawnAimGroundFX() const;

	/** 이번 발동에서 이미 맞은 대상. 판정이 유지되는 동안 같은 대상을 다시 때리지 않는다. 앞발 판정이 여러 번이어도 한 번만 맞는다. */
	TSet<TWeakObjectPtr<AActor>> HitActors;
	int32 NextImpact = 0;
	FknightMonsterKnockback Knockback;

	/** 발동부터 지난 몬스터 시간. */
	float Elapsed = 0.f;
	/** 판정이 모두 끝나 후딜이 시작된 몬스터 시간. 아직이면 음수. */
	float RecoveryStart = -1.f;
	/** ActiveTime 동안 다음 오버랩을 할 몬스터 시간. */
	float NextActiveCheck = 0.f;
	bool bLungeStarted = false;
	bool bWindupDone = false;
	bool bHitAreaEffectsPlayed = false;
	bool bWindupEffectStarted = false;
	/** 고개 쓸기 각도(몸 정면 기준, 오른쪽 +). */
	float AimYaw = 0.f;
	/** 마지막으로 바닥 이펙트를 남긴 고개 각도. */
	float LastGroundFXYaw = 0.f;
	bool bAimSweeping = false;
	TWeakObjectPtr<UNiagaraComponent> WindupEffectComponent;
	TWeakObjectPtr<UAudioComponent> WindupSoundComponent;
};
