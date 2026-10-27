#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/knightGameplayAbility_MonsterAttack.h"
#include "Combat/knightCombatTypes.h"
#include "knightGameplayAbility_MonsterFlybyAttack.generated.h"

class UAbilityTask_Repeat;
class UAnimMontage;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UNiagaraComponent;
class UNiagaraSystem;
class UAudioComponent;
class USoundBase;
class UStaticMesh;
class UStaticMeshComponent;

/**
 * 지나가며 바닥을 때리는 공격의 보이는 부분. 경고 동안 가슴(BeamBone)에 모이는 빛과 소리, 아래 바닥에서 빨려 오르는 먼지(수면이면
 * 물안개와 떨리는 잔물결), 지나가는 동안 가슴에서 바닥까지 이어진 기둥,
 * 기둥이 바닥(또는 수면)에 닿는 자리의 바람과 물결. 공명해방(State.Monster.Enraged) 중에는 Enraged 쪽을 쓰고, 비어 있으면 기본을 쓴다.
 * 모두 비우면 아무것도 보이지 않는다(판정은 그대로).
 */
USTRUCT(BlueprintType)
struct FknightFlybyBeamEffects
{
	GENERATED_BODY()

	/** 경고 동안 BeamBone에 붙는다. 지나가기가 시작되거나 공격이 끊기면 바로 끈다. */
	UPROPERTY(EditDefaultsOnly, Category="Effects")
	TObjectPtr<UNiagaraSystem> Warning;

	UPROPERTY(EditDefaultsOnly, Category="Effects")
	TObjectPtr<UNiagaraSystem> EnragedWarning;

	/**
	 * 경고 동안 가슴 바로 아래 바닥에 붙어, 바닥의 먼지가 가슴으로 빨려 올라간다. 바닥에서 위(+Z)로 WarningInflowHeight까지 오르도록
	 * 만들고, 실제 가슴 높이에 맞춰 Z로 늘인다. 지나가기가 시작되면 새 입자만 멈춘다.
	 */
	UPROPERTY(EditDefaultsOnly, Category="Effects|Warning")
	TObjectPtr<UNiagaraSystem> WarningInflow;

	/** 가슴 아래가 수면이면 WarningInflow 대신 쓴다(물안개와 물방울). 비우면 WarningInflow. */
	UPROPERTY(EditDefaultsOnly, Category="Effects|Warning")
	TObjectPtr<UNiagaraSystem> WarningWaterInflow;

	UPROPERTY(EditDefaultsOnly, Category="Effects|Warning", meta=(ClampMin=1, Units="cm"))
	float WarningInflowHeight = 1000.f;

	/** 경고 동안 가슴에서 울리는 소리(공명음). 지나가기가 시작되는 순간 WarningSoundFadeOut만에 끊는다. 비우면 소리 없음. */
	UPROPERTY(EditDefaultsOnly, Category="Effects|Warning")
	TObjectPtr<USoundBase> WarningSound;

	/** 경고 시작과 끝의 음높이 배율. 사이는 선형으로 올린다. 소리 자체에 음높이 변화가 있으면 둘 다 1로 둔다. */
	UPROPERTY(EditDefaultsOnly, Category="Effects|Warning")
	FVector2D WarningSoundPitch = FVector2D(1.0, 1.4);

	UPROPERTY(EditDefaultsOnly, Category="Effects|Warning", meta=(ClampMin=0, Units="s"))
	float WarningSoundFadeOut = 0.05f;

	/** 경고 동안 가슴 아래 수면을 이 간격마다 같은 자리에서 눌러 잔물결 고리가 이어 퍼지게 한다. 0이면 끈다. */
	UPROPERTY(EditDefaultsOnly, Category="Effects|Warning", meta=(ClampMin=0, Units="s"))
	float WarningRippleInterval = 0.12f;

	UPROPERTY(EditDefaultsOnly, Category="Effects|Warning", meta=(ClampMin=0, Units="cm"))
	float WarningRippleRadius = 250.f;

	/** 경고 시작과 끝의 누르는 깊이(cm). 사이는 선형으로 깊어진다. */
	UPROPERTY(EditDefaultsOnly, Category="Effects|Warning")
	FVector2D WarningRippleDepth = FVector2D(2.0, 8.0);

	/** 기둥 메시. 피벗이 위 가운데이고 -Z로 100cm, 아래 끝 반지름 100cm로 만든다(높이·반지름에 맞춰 늘린다). */
	UPROPERTY(EditDefaultsOnly, Category="Effects|Pillar")
	TObjectPtr<UStaticMesh> PillarMesh;

	/** 스칼라 파라미터 Fade(0~1)로 나타나고 사라진다. */
	UPROPERTY(EditDefaultsOnly, Category="Effects|Pillar")
	TObjectPtr<UMaterialInterface> PillarMaterial;

	UPROPERTY(EditDefaultsOnly, Category="Effects|Pillar")
	TObjectPtr<UMaterialInterface> EnragedPillarMaterial;

	/** 기둥 아래 끝 반지름. 0이면 HalfWidth. */
	UPROPERTY(EditDefaultsOnly, Category="Effects|Pillar", meta=(ClampMin=0, Units="cm"))
	float PillarRadius = 0.f;

	UPROPERTY(EditDefaultsOnly, Category="Effects|Pillar", meta=(ClampMin=0, Units="s"))
	float FadeTime = 0.15f;

	/** 기둥을 따라 내려가는 바람결. 가슴에서 아래(-Z)로 PillarStreakLength만큼 가도록 만들고, 실제 기둥 높이에 맞춰 Z로 늘인다. */
	UPROPERTY(EditDefaultsOnly, Category="Effects|Pillar")
	TObjectPtr<UNiagaraSystem> PillarStreaks;

	UPROPERTY(EditDefaultsOnly, Category="Effects|Pillar")
	TObjectPtr<UNiagaraSystem> EnragedPillarStreaks;

	UPROPERTY(EditDefaultsOnly, Category="Effects|Pillar", meta=(ClampMin=1, Units="cm"))
	float PillarStreakLength = 1000.f;

	/** 지나가는 동안 BeamBone에서 나는 소리(브레스). 지나가기가 끝나거나 공격이 끊기면 BeamSoundFadeOut만에 줄인다. 비우면 소리 없음. */
	UPROPERTY(EditDefaultsOnly, Category="Effects|Pillar")
	TObjectPtr<USoundBase> BeamSound;

	UPROPERTY(EditDefaultsOnly, Category="Effects|Pillar", meta=(ClampMin=0, Units="s"))
	float BeamSoundFadeOut = 0.4f;

	/** 기둥이 바닥(또는 수면)에 닿는 자리. 바닥을 따라 바깥으로 퍼지는 바람. */
	UPROPERTY(EditDefaultsOnly, Category="Effects|Ground")
	TObjectPtr<UNiagaraSystem> Ground;

	UPROPERTY(EditDefaultsOnly, Category="Effects|Ground")
	TObjectPtr<UNiagaraSystem> EnragedGround;

	/** 지나간 자리에 남기는 일회성 연기. 바닥(또는 수면)에 GroundTrailSpacing마다 스폰한다. */
	UPROPERTY(EditDefaultsOnly, Category="Effects|Ground")
	TObjectPtr<UNiagaraSystem> GroundTrail;

	UPROPERTY(EditDefaultsOnly, Category="Effects|Ground")
	TObjectPtr<UNiagaraSystem> EnragedGroundTrail;

	UPROPERTY(EditDefaultsOnly, Category="Effects|Ground", meta=(ClampMin=1, Units="cm"))
	float GroundTrailSpacing = 500.f;

	/** 기둥이 닿은 수면을 이 간격마다 눌러 지나간 자리에 골을 판다. 촘촘히 이어 누르면 양옆으로 V자 물결이 퍼진다. */
	UPROPERTY(EditDefaultsOnly, Category="Effects|Water", meta=(ClampMin=1, Units="cm"))
	float WaterRippleSpacing = 50.f;

	UPROPERTY(EditDefaultsOnly, Category="Effects|Water", meta=(ClampMin=0, Units="cm"))
	float WaterRippleRadius = 300.f;

	UPROPERTY(EditDefaultsOnly, Category="Effects|Water", meta=(ClampMin=0, Units="cm"))
	float WaterRippleDepth = 10.f;

	/** 수면을 지나는 동안(과 그 뒤 2초) 물결 높이 상한. 0이면 물결 설정의 MaxHeight 그대로. */
	UPROPERTY(EditDefaultsOnly, Category="Effects|Water", meta=(ClampMin=0, Units="cm"))
	float WaterMaxHeight = 35.f;

	/** 수면에 닿은 자리에서 양옆으로 갈라져 튀는 물보라 막과 물안개. 수면 위에 있는 동안만 켠다. */
	UPROPERTY(EditDefaultsOnly, Category="Effects|Water")
	TObjectPtr<UNiagaraSystem> WaterSpray;

	/** 수면을 지날 때 물보라. 비우면 물결 설정의 SplashHeavySystem. */
	UPROPERTY(EditDefaultsOnly, Category="Effects|Water")
	TObjectPtr<UNiagaraSystem> WaterSplash;

	/** 이만큼 갈 때마다 물보라를 하나 낸다. 0이면 물보라 없음. */
	UPROPERTY(EditDefaultsOnly, Category="Effects|Water", meta=(ClampMin=0, Units="cm"))
	float WaterSplashSpacing = 700.f;

	UPROPERTY(EditDefaultsOnly, Category="Effects|Water", meta=(ClampMin=0))
	float WaterSplashScale = 0.7f;

	/** 플레이어가 띠(HalfWidth)에서 이만큼 더 바깥까지 있으면 화면이 자글거린다. 음수면 화면 연출 없음. */
	UPROPERTY(EditDefaultsOnly, Category="Effects|Screen", meta=(Units="cm"))
	float JitterMargin = 150.f;
};

/**
 * 공중 턴의 날짐승이 타깃 머리 위를 일자로 빠르게 지나가며 아래 바닥을 띠 모양으로 때리는 공격(잔향 초음파 브레스).
 *
 *   경고: WarningMontage를 재생하며 WarningTime 동안 타깃 쪽으로 돈다. 그동안 몬스터에 State.Monster.Warning이 붙는다.
 *   지나가기: 경고가 끝나는 순간의 타깃 쪽으로 방향을 고정하고, 타깃 너머 PassOvershoot까지 PassSpeed로 날아간다.
 *             지나가는 동안 BeamBone(가슴) 아래 바닥에서 반지름 HalfWidth 구체로 때린다(폭 HalfWidth*2 띠). 대상마다 한 번.
 *   회복: PassPoseEndTime > 0이면 지나가는 동안 쏘는 자세를 유지하고, 지나간 뒤 WarningMontage의 남은 부분(회복 동작)을 재생하고 끝난다.
 *
 * 회피만 되게 하려면 HitParams.DamageEffect에 가드 범위 0인 피해 실행을 쓰고 bCanBeParried를 끈다.
 * 보이는 부분은 Effects(경고 빛, 가슴에서 바닥까지 이어진 기둥, 바닥·수면에 닿는 바람과 물결). 기둥 아래의 플레이어는 화면이 자글거린다.
 * 디버그: knight.Monster.DrawAttacks 1 이면 경고 중 지나갈 띠와 판정 구체를 그린다.
 */
UCLASS()
class KNIGHT_API UknightGameplayAbility_MonsterFlybyAttack : public UknightGameplayAbility_MonsterAttack
{
	GENERATED_BODY()

public:
	UknightGameplayAbility_MonsterFlybyAttack(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	/** 선택 사항. 경고 동안 재생하고 지나가기 시작할 때 멈춘다. */
	UPROPERTY(EditDefaultsOnly, Category="Attack")
	TObjectPtr<UAnimMontage> WarningMontage;

	UPROPERTY(EditDefaultsOnly, Category="Attack")
	FknightMeleeHitParams HitParams;

	UPROPERTY(EditDefaultsOnly, Category="Attack|Warning", meta=(ClampMin=0, Units="s"))
	float WarningTime = 1.5f;

	/**
	 * WarningMontage에서 쏘는 자세를 유지하는 구간이 끝나는 시각(몽타주 초). 0보다 크면 지나가는 동안 몽타주를 멈추지 않고
	 * 지나가기가 끝날 때 이 시각에 닿도록 재생 속도를 바꾼다. 지나간 뒤에는 이 시각부터 끝까지(회복 동작) 제 속도로 재생하고 끝난다.
	 * 0이면 지나가기가 시작될 때 몽타주를 멈춘다.
	 */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Warning", meta=(ClampMin=0, Units="s"))
	float PassPoseEndTime = 0.f;

	UPROPERTY(EditDefaultsOnly, Category="Attack|Warning", meta=(ClampMin=0, Units="deg"))
	float TurnRate = 240.f;

	UPROPERTY(EditDefaultsOnly, Category="Attack|Pass", meta=(ClampMin=1, Units="cm/s"))
	float PassSpeed = 2500.f;

	/** 타깃 너머 이만큼 더 지나간 뒤 멈춘다(몸 기준). */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Pass", meta=(ClampMin=0, Units="cm"))
	float PassOvershoot = 1500.f;

	/** 바닥 판정이 따라가는 뼈(가슴). 이 뼈 바로 아래 바닥을 때린다. */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Hit")
	FName BeamBone = TEXT("spine_02");

	/** 띠의 절반 폭. 바닥 위 같은 높이까지 닿는다. */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Hit", meta=(ClampMin=1, Units="cm"))
	float HalfWidth = 300.f;

	UPROPERTY(EditDefaultsOnly, Category="Attack|Effects")
	FknightFlybyBeamEffects Effects;

private:
	UFUNCTION()
	void HandleTick(int32 ActionNumber);

	void StartPass();
	void StartRecover();
	void ApplyHits();
	void SetWarning(bool bOn);

	/** 가슴 아래 판정 중심(바닥 높이)과 가슴 위치. */
	void GetBeam(FVector& OutChest, FVector& OutGround) const;
	/** 가슴 아래 수면 높이. 수면이 바닥 근처나 그보다 위(물에 잠긴 바닥)이고 가슴보다 아래일 때만 true. */
	bool FindBeamWater(const FVector& Chest, const FVector& Ground, float& OutSurfaceZ) const;
	void UpdateWarningEffects();
	/** 경고 소리를 끊고 빨려 오르는 입자의 생성을 멈춘다(남은 입자는 수명대로). */
	void StopWarningEffects();
	void StartBeamEffects();
	void UpdateBeamEffects();
	void UpdateWater(const FVector& Chest, const FVector& Ground, float& InOutBottomZ);
	void StopEffects();

	TWeakObjectPtr<UAbilityTask_Repeat> TickTask;
	TWeakObjectPtr<UAnimMontage> PlayingWarning;
	TSet<TWeakObjectPtr<AActor>> HitActors;

	UPROPERTY(Transient)
	TObjectPtr<UNiagaraComponent> WarningComponent;

	UPROPERTY(Transient)
	TObjectPtr<UNiagaraComponent> InflowComponent;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> WarningAudio;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> BeamAudio;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> PillarComponent;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> PillarMaterialInstance;

	UPROPERTY(Transient)
	TObjectPtr<UNiagaraComponent> StreakComponent;

	UPROPERTY(Transient)
	TObjectPtr<UNiagaraComponent> GroundComponent;

	UPROPERTY(Transient)
	TObjectPtr<UNiagaraComponent> SprayComponent;

	bool bEnraged = false;
	/** 경고 중 마지막으로 수면을 누른 뒤 지난 시간. */
	float WarningRippleTime = 0.f;
	/** 수면 위를 지난 거리(물결·물보라 간격용). 수면을 벗어나면 다시 센다. */
	float WaterTravel = 0.f;
	float WaterSplashTravel = 0.f;
	FVector LastWaterPoint = FVector::ZeroVector;
	/** 마지막 연기 자국을 남긴 자리. */
	FVector LastTrailPoint = FVector::ZeroVector;
	bool bTrailStarted = false;
	bool bOverWater = false;
	bool bPassing = false;
	/** 지나간 뒤 회복 동작을 재생하는 중. */
	bool bRecovering = false;
	float RecoverDuration = 0.f;
	bool bWarningApplied = false;
	float PhaseTime = 0.f;
	float PassDuration = 0.f;
};
