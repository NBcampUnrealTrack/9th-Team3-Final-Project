#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/knightGameplayAbility_MonsterAttack.h"
#include "Combat/knightCombatTypes.h"
#include "knightGameplayAbility_MonsterSpikeRain.generated.h"

class UAnimMontage;
class UMaterialInterface;
class UNiagaraSystem;
class UStaticMesh;

/** 가시 비가 떨어지는 모양. */
UENUM()
enum class EknightSpikeRainPattern : uint8
{
	/** 타깃 주변에 흩뿌린다. */
	Scatter,
	/** 타깃 발밑에 차례로 떨어진다. 가시마다 경고가 뜨는 순간 타깃이 있던 자리를 노리므로, 멈춰 있으면 맞고 움직이면 뒤를 쫓아온다. */
	Track
};

/**
 * 등의 가시를 하늘로 쏘아 올려 타깃 주변에 떨어뜨리는 몬스터 공격(가시 비). 몽타주의 FireTime에 등에서 불꽃(LaunchEffect)과 함께
 * 가시 SpikeCount개를 위로 곧게 쏘아 올리고(화면 밖으로 사라지면 삭제), 착탄 FallTime 전에 착탄점 위 FallHeight에서 새 가시를
 * 만들어 수직으로 꽂는다(AknightMonsterSpikeProjectile). 첫 가시는 타깃이 있던 자리, 나머지는 그 주변 ScatterRadius 안에
 * 타깃 쪽으로 모이게(CenterBias) 흩어지고, LandInterval씩 늦게(0이면 거의 동시에, LandJitter만큼 흔들어) 떨어진다. 착탄 WarningTime 전부터 바닥에 원을 그려 피할 수 있게 하고, 한 번의 가시 비에서
 * 대상마다 한 번만 맞는다. 가시는 어빌리티와 따로 떨어지므로(월드 타이머) 몽타주가 끝나면 어빌리티도 끝난다.
 *
 * Pattern이 Track이면 흩뿌리지 않고 SpikeCount개를 LandInterval마다 하나씩 떨어뜨린다. 각 가시는 경고가 뜨는 순간(착탄 WarningTime 전)
 * 타깃 발밑(TrackJitter 안에서 조금 흔듦)을 노리므로, WarningTime이 곧 피할 수 있는 여유다.
 */
UCLASS()
class KNIGHT_API UknightGameplayAbility_MonsterSpikeRain : public UknightGameplayAbility_MonsterAttack
{
	GENERATED_BODY()

public:
	UknightGameplayAbility_MonsterSpikeRain(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

protected:
	UPROPERTY(EditDefaultsOnly, Category="Attack")
	TObjectPtr<UAnimMontage> Montage;

	UPROPERTY(EditDefaultsOnly, Category="Attack")
	FknightMeleeHitParams HitParams;

	/** 가시를 쏘는 시각(몽타주 초). 등을 내리꽂는 순간. */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Spike", meta=(ClampMin=0, Units="s"))
	float FireTime = 1.3f;

	UPROPERTY(EditDefaultsOnly, Category="Attack|Spike")
	EknightSpikeRainPattern Pattern = EknightSpikeRainPattern::Scatter;

	/** 떨어뜨릴 가시 수. */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Spike", meta=(ClampMin=1))
	int32 SpikeCount = 36;

	/** 첫 가시를 뺀 나머지가 떨어지는 범위(타깃 위치 기준 반지름). */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Spike", meta=(ClampMin=0, Units="cm"))
	float ScatterRadius = 1800.f;

	/**
	 * 클수록 타깃 쪽으로 모인다. 거리 = ScatterRadius x 난수^CenterBias. 0.5면 넓이에 고르게, 1.5면 약 63%가 반경 절반 안에 떨어진다.
	 */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Spike", meta=(ClampMin=0.1))
	float CenterBias = 1.5f;

	/** Track: 노린 자리를 이 반지름 안에서 흔든다. 가시 줄이 너무 반듯해 보이지 않게 한다. */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Spike", meta=(ClampMin=0, Units="cm", EditCondition="Pattern == EknightSpikeRainPattern::Track"))
	float TrackJitter = 25.f;

	/**
	 * 한 번의 가시 비에서 같은 대상이 다시 맞기까지의 시간. 0보다 작으면 대상마다 한 번만 맞는다.
	 * 연달아 맞게 할 때는 피격 경직보다 조금 길게 두어, 경직에 묶인 채 가시를 전부 맞지 않게 한다.
	 */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Spike", meta=(Units="s"))
	float RehitInterval = -1.f;

	/** 가시를 쏘는 본. 그 위치에서 LaunchHeight만큼 위, 수평으로 LaunchSpread 안에서 출발한다. */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Launch")
	FName LaunchSocket = TEXT("spine_02");

	UPROPERTY(EditDefaultsOnly, Category="Attack|Launch", meta=(Units="cm"))
	float LaunchHeight = 250.f;

	UPROPERTY(EditDefaultsOnly, Category="Attack|Launch", meta=(ClampMin=0, Units="cm"))
	float LaunchSpread = 250.f;

	/** 올라가는 가시의 속도와 사라질 때까지의 시간. */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Launch", meta=(ClampMin=0, Units="cm/s"))
	float AscendSpeed = 9000.f;

	UPROPERTY(EditDefaultsOnly, Category="Attack|Launch", meta=(ClampMin=0.01, Units="s"))
	float AscendTime = 0.3f;

	/** 발사할 때 LaunchSocket에 터뜨리는 효과(등의 불꽃). */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Launch")
	TObjectPtr<UNiagaraSystem> LaunchEffect;

	/** 발사부터 첫 가시가 꽂힐 때까지. 다음 가시는 LandInterval씩 늦다. */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Fall", meta=(ClampMin=0.1, Units="s"))
	float FirstLandTime = 1.6f;

	UPROPERTY(EditDefaultsOnly, Category="Attack|Fall", meta=(ClampMin=0, Units="s"))
	float LandInterval = 0.f;

	/** 가시마다 착탄을 0 ~ 이 시간만큼 무작위로 늦춘다. 동시에 꽂히는 것처럼 보이면서 생성이 몇 프레임에 나뉜다. */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Fall", meta=(ClampMin=0, Units="s"))
	float LandJitter = 0.05f;

	/** 떨어지는 가시가 생기는 높이(착탄점 위)와 꽂힐 때까지의 시간. */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Fall", meta=(ClampMin=0, Units="cm"))
	float FallHeight = 3000.f;

	UPROPERTY(EditDefaultsOnly, Category="Attack|Fall", meta=(ClampMin=0.01, Units="s"))
	float FallTime = 0.2f;

	UPROPERTY(EditDefaultsOnly, Category="Attack|Fall", meta=(ClampMin=1, Units="cm"))
	float HitRadius = 200.f;

	/** 착탄점에서 위로 이 높이까지(낙하 경로)에 있는 대상이 맞는다. 가시는 대상을 통과해 땅에 박힌다. */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Fall", meta=(ClampMin=1, Units="cm"))
	float PathHeight = 400.f;

	/**
	 * 착탄 이 시간 전부터 바닥에 원을 그린다. 원 반지름은 HitRadius다. 데칼은 이 시간 동안 페이드인하므로
	 * 머티리얼이 DecalLifetimeOpacity를 쓰면 착탄이 가까울수록 진해지게(그림자처럼) 만들 수 있다.
	 */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Fall", meta=(ClampMin=0, Units="s"))
	float WarningTime = 0.8f;

	UPROPERTY(EditDefaultsOnly, Category="Attack|Fall")
	TObjectPtr<UMaterialInterface> WarningDecal;

	/** 착탄 지점에 터뜨리는 효과(충격파, 먼지). */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Fall")
	TArray<TObjectPtr<UNiagaraSystem>> ImpactEffects;

	UPROPERTY(EditDefaultsOnly, Category="Attack|Fall", meta=(ClampMin=0, Units="s"))
	float StuckTime = 1.5f;

	/** 가시 외형. 끝이 +Z인 원뿔 두 개를 밑면끼리 붙인다. SpikeScale은 원뿔 하나의 스케일. */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Look")
	TObjectPtr<UStaticMesh> SpikeMesh;

	UPROPERTY(EditDefaultsOnly, Category="Attack|Look")
	TObjectPtr<UMaterialInterface> SpikeMaterial;

	UPROPERTY(EditDefaultsOnly, Category="Attack|Look")
	FVector SpikeScale = FVector(0.24f, 0.24f, 0.6f);

	/** 떨어지는 가시에 붙는 효과(뜨거운 가시의 불길). 꽂혀 있는 StuckTime 동안 함께 남는다. */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Look")
	TObjectPtr<UNiagaraSystem> SpikeEffect;

private:
	UFUNCTION()
	void HandleFire();

	UFUNCTION()
	void HandleMontageEnded();
};
