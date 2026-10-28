#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/knightGameplayAbility.h"
#include "knightGameplayAbility_BoostDash.generated.h"

class ACharacter;
class UAnimMontage;
class UCurveFloat;
class UMaterialInterface;
class UNiagaraComponent;
class UNiagaraSystem;
class USoundBase;

/**
 * 부스트 대시 / 회피 부스트(퀵스텝). 현재 이동 입력 방향으로(입력이 없으면 뒤로) 일정한 힘의
 * 루트 모션 소스로 나이트를 움직이고, 처음 InvulnerableDuration초 동안 State.Invulnerable을 건다.
 *
 * 게이지 비용은 BP의 StaminaCost(기본 클래스 UknightGameplayAbility, 모든 행동이 같은 칸)로 낸다. (2026-10-09, 전에는 Cost Gameplay Effect GE_Knight_BoostCost)
 * (2026-10-26) 다른 행동처럼 게이지가 0보다 많으면 나가고, 모자란 만큼 음수가 된다(가불). 0 이하면 Ability.ActivateFail.Cost로 실패한다.
 *
 * 방향과 자세: 락온이 아니면 이동 입력이 있는 대시는 먼저 나이트를 대시 방향으로 돌리고 DashMontage(정면)를 재생한다.
 * 락온 중(State.LockedOn)이거나 입력 없는 백스텝이면 바라보는 방향을 유지하고, 그 방향 기준의 대시 방향으로
 * 몽타주를 고른다(앞 / 뒤 / 왼쪽 / 오른쪽). 지정하지 않은 방향 몽타주는 DashMontage를 쓴다.
 *
 * 스탯 연결 (2026-10-06):
 *   대시 거리 = DashDistance x DodgeSpeed(메카 속성, 배율) x 중량 계수(무브먼트 컴포넌트)
 *   대시 시간(DashDuration)은 그대로 두어 몽타주 타이밍이 어긋나지 않게 한다. 그래서 거리가 늘면 속도도 빨라진다.
 *   쿨타임 = 대시 이동이 끝난 뒤 DodgeCooldown(메카 속성, 초) 동안 다시 대시할 수 없다. 기본 0.5초.
 *
 * 경직과 캔슬 (2026-10-07):
 *   흐름: 대시 이동(DashDuration) → 경직(RecoveryDuration) → 종료. 몽타주는 경직까지 이어서 재생된다.
 *   경직 중에는 이동 입력으로 가속하지 않고(미끄러지며 정지) Window.Cancel을 켠다.
 *   단, 대시가 끝날 때 대시 방향 쪽으로 방향키를 누르고 있으면 이동을 잠그지 않는다(관성 이어가기, 2026-10-09, CarryMomentumMaxAngle).
 *     - 공격·가드: Window.Cancel 덕분에 활성화 그룹 규칙대로 대시를 대신한다. 대시 중 미리 누른 입력은 버퍼에서 나간다.
 *     - 대시: 실행 중에 퀵스텝 키(InputTag.Boost)를 다시 누르면 입력 처리(knightAbilitySystemComponent)가 재발동을 시도한다.
 *       경직 중이고 쿨타임이 끝났을 때만 받아들여 지금 대시를 끝내고 새 대시로 다시 시작한다(bRetriggerInstancedAbility, 비용 다시 냄).
 *       대시 이동 도중의 요청은 거절해 이동이 끊기지 않는다(그 누름은 입력 버퍼에 남아 경직이 시작되면 다시 시도된다).
 *
 * 입력 (2026-10-08): 퀵스텝 키(기본 LeftControl, IA_Boost → InputTag.Boost)를 한 번 누르면 대시. 달리기는 LeftShift(InputTag.Run)로 분리했다.
 * (전에는 Shift를 누르고 있으면 달리기, 두 번 탭하면 knightGameplayAbility_Sprint가 태그로 대시를 발동했다)
 *
 * 퀵스텝 공격 (2026-10-28): 대시하는 동안과 끝난 뒤 FollowUpGraceTime초 동안 FollowUpTags(State.QuickstepFollowUp)를 붙인다.
 * 그동안 공격 키는 1타 대신 GA_Knight_QuickstepAttack(일반 근접 공격 BP)이 나간다.
 */
UCLASS()
class KNIGHT_API UknightGameplayAbility_BoostDash : public UknightGameplayAbility
{
	GENERATED_BODY()

public:
	UknightGameplayAbility_BoostDash(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const override;
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	/** 배율·중량 계수가 1일 때의 대시 거리. (2026-10-07 900 → 750, 2026-10-16 750 → 375 절반으로 조정) */
	UPROPERTY(EditDefaultsOnly, Category="Dash", meta=(ClampMin=0, Units="cm"))
	float DashDistance = 375.f;

	UPROPERTY(EditDefaultsOnly, Category="Dash", meta=(ClampMin=0.05, Units="s"))
	float DashDuration = 0.35f;

	/**
	 * 대시 이동이 끝난 뒤의 경직 시간. 이 동안 공격·가드·(쿨타임이 끝난) 대시로 캔슬할 수 있다. 0이면 경직 없이 바로 끝난다.
	 * (2026-10-07 0.25 → 0.1, 플레이해 보니 경직이 길게 느껴져 줄임)
	 */
	UPROPERTY(EditDefaultsOnly, Category="Dash", meta=(ClampMin=0, Units="s"))
	float RecoveryDuration = 0.1f;

	/**
	 * 관성 이어가기 각도 (2026-10-09 사용자 결정, 바뀔 수 있는 값). 대시가 끝날 때 대시 방향에서 이 각도 안쪽으로 방향키를 누르고 있으면
	 * 경직 동안 이동을 잠그지 않아, 걷기·달리기 속도 그대로 이어서 움직인다. 방향키를 안 누르거나 이 각도 밖이면 전처럼 미끄러지며 버틴다.
	 * (출발 Easing을 넣은 뒤 '퀵스텝 → 경직에서 거의 정지 → 느린 출발'이 어색해서 넣음. 캔슬 창은 어느 쪽이든 그대로)
	 * 0이면 항상 잠근다.
	 */
	UPROPERTY(EditDefaultsOnly, Category="Dash", meta=(ClampMin=0, ClampMax=180, Units="Degrees"))
	float CarryMomentumMaxAngle = 90.f;

	/**
	 * 대시 몽타주 재생 속도(고정). 어빌리티가 끝나면(대시 + 경직) 그 지점에서 블렌드 아웃된다.
	 * 1.5배속, 대시 0.35초 + 경직 0.1초면 몽타주(1.33초) 약 51% 지점에서 끝난다.
	 * (2026-10-07: 대시+경직 시간에 맞춘 자동 배속(약 2.4~3배)은 대시 중에 동작이 너무 빨리 지나가서 고정 배속으로 바꿈.
	 *  1.0배속도 시험해 본 뒤 1.5배속으로 정함)
	 */
	UPROPERTY(EditDefaultsOnly, Category="Dash", meta=(ClampMin=0.1))
	float MontagePlayRate = 1.5f;

	/** 선택 사항. 대시 동안의 0..1 커브. 거리는 크게 바꾸지 않고 순간 가속 모양만 바꾼다. */
	UPROPERTY(EditDefaultsOnly, Category="Dash")
	TObjectPtr<UCurveFloat> StrengthOverTime;

	/** 대시 시작부터의 회피(무적) 시간. */
	UPROPERTY(EditDefaultsOnly, Category="Dash", meta=(ClampMin=0, Units="s"))
	float InvulnerableDuration = 0.2f;

	/** 선택 사항. 추진기 자세(정면). 몽타주의 루트 모션은 무시하고 루트 모션 소스가 이동을 맡는다. */
	UPROPERTY(EditDefaultsOnly, Category="Dash")
	TObjectPtr<UAnimMontage> DashMontage;

	/** 바라보는 방향 기준으로 뒤로 움직일 때의 대시 자세 (락온, 또는 입력 없는 백스텝). */
	UPROPERTY(EditDefaultsOnly, Category="Dash|Directional")
	TObjectPtr<UAnimMontage> DashMontageBack;

	UPROPERTY(EditDefaultsOnly, Category="Dash|Directional")
	TObjectPtr<UAnimMontage> DashMontageLeft;

	UPROPERTY(EditDefaultsOnly, Category="Dash|Directional")
	TObjectPtr<UAnimMontage> DashMontageRight;

	/**
	 * (2026-10-15 사용자 결정) 방향키를 누르고 있을 때만 퀵스텝이 나간다. 끄면 예전처럼 입력 없이 누르면 뒤로(백스텝) 나간다.
	 */
	UPROPERTY(EditDefaultsOnly, Category="Dash")
	bool bRequireMoveInput = true;

	/** 락온이 아니면 시작할 때 나이트를 대시 방향으로 돌려서 정면 자세와 맞춘다. */
	UPROPERTY(EditDefaultsOnly, Category="Dash|Directional")
	bool bFaceDashDirectionWhenFree = true;

	/**
	 * 대시마다 실행하는 게임플레이 큐(추진기 이펙트). 비우면 실행하지 않는다.
	 * (2026-10-08 추가: 같은 이동 로직을 쓰는 구르기 회피(knightGameplayAbility_Dodge)는 추진기 큐를 쓰지 않는다)
	 */
	UPROPERTY(EditDefaultsOnly, Category="Dash", meta=(Categories="GameplayCue"))
	FGameplayTag DashCueTag;

	/** (2026-10-22) 퀵스텝을 시작할 때 나는 부스터 소리. 비우면 나지 않는다(DashCueTag에 연결된 큐 에셋이 없어 여기서 낸다). */
	UPROPERTY(EditDefaultsOnly, Category="Dash|Effects")
	TObjectPtr<USoundBase> DashSound;

	UPROPERTY(EditDefaultsOnly, Category="Dash|Effects", meta=(ClampMin=0))
	float DashSoundVolume = 1.f;

	/**
	 * (2026-10-28 사용자 요청 '퀵스텝 공격') 퀵스텝하는 동안과 끝난 뒤 FollowUpGraceTime초 동안 붙이는 태그. 기본 State.QuickstepFollowUp.
	 * 퀵스텝 공격(GA_Knight_QuickstepAttack)이 이 태그를 필요로 하고, 1타·대쉬공격은 이 태그에 막혀서 그동안 공격 키는 퀵스텝 공격이 된다.
	 * 퀵스텝 중에 미리 누른 공격은 입력 버퍼에 있다가 경직(캔슬 창)이 시작될 때 나간다.
	 */
	UPROPERTY(EditDefaultsOnly, Category="Dash|Follow Up")
	FGameplayTagContainer FollowUpTags;

	/** (2026-10-28) 퀵스텝이 끝난 뒤에도 FollowUpTags를 남기는 시간(초). 이 안에 누른 공격도 퀵스텝 공격이 된다. 바뀔 수 있는 값. */
	UPROPERTY(EditDefaultsOnly, Category="Dash|Follow Up", meta=(ClampMin=0, Units="s"))
	float FollowUpGraceTime = 0.2f;

	/**
	 * 등 부스터 (2026-10-12). 대시 이동 동안 등에서 대시 반대 방향으로 뿜는다. 이펙트는 로컬 -Z로 뿜는 시스템(NS_Knight_Thruster).
	 * 위치는 BackThrusterBone 위치에 BackThrusterOffsets(액터 기준: X 앞, Y 오른쪽, Z 위)를 더한 곳이고 매 프레임 따라간다.
	 * 비워 두면 나오지 않는다.
	 */
	UPROPERTY(EditDefaultsOnly, Category="Dash|Back Booster")
	TObjectPtr<UNiagaraSystem> BackThrusterEffect;

	UPROPERTY(EditDefaultsOnly, Category="Dash|Back Booster")
	FName BackThrusterBone = TEXT("spine_03");

	UPROPERTY(EditDefaultsOnly, Category="Dash|Back Booster")
	TArray<FVector> BackThrusterOffsets;

	UPROPERTY(EditDefaultsOnly, Category="Dash|Back Booster", meta=(ClampMin=0.1))
	float BackThrusterScale = 1.4f;

	/** 뿜는 방향을 아래로 기울이는 각도. 0이면 대시 반대 방향 수평. */
	UPROPERTY(EditDefaultsOnly, Category="Dash|Back Booster", meta=(ClampMin=0, ClampMax=80, Units="deg"))
	float BackThrusterDownAngle = 20.f;

	/** 대시 이동이 끝난 뒤 분사를 이만큼 더 유지한다. */
	UPROPERTY(EditDefaultsOnly, Category="Dash|Back Booster", meta=(ClampMin=0, Units="s"))
	float BackThrusterLinger = 0.05f;

	/** 디버그: 분사 위치(노란 구)와 뿜는 방향(빨간 선)을 그리고 로그를 남긴다. */
	UPROPERTY(EditDefaultsOnly, Category="Dash|Back Booster")
	bool bDebugBackThrusters = false;

	/** 대시 소리의 AI 청각 크기. */
	UPROPERTY(EditDefaultsOnly, Category="Dash|Noise", meta=(ClampMin=0))
	float NoiseLoudness = 1.f;

	/** 대시 소리가 들리는 최대 거리. 0이면 듣는 쪽의 청각 범위를 쓴다. */
	UPROPERTY(EditDefaultsOnly, Category="Dash|Noise", meta=(ClampMin=0, Units="cm"))
	float NoiseRange = 0.f;

	/** 퀵스텝 대시 이동 동안 메카 홀로그램 잔상(애프터이미지)을 남길지 여부. */
	UPROPERTY(EditDefaultsOnly, Category="Dash|Ghost Trail")
	bool bSpawnGhostTrail = true;

	/** 잔상에 덮어쓸 반투명/애디티브 머티리얼 (비우면 M_Knight_GhostTrail). */
	UPROPERTY(EditDefaultsOnly, Category="Dash|Ghost Trail", meta=(EditCondition="bSpawnGhostTrail"))
	TObjectPtr<UMaterialInterface> GhostMaterial;

	/** 대시 중 잔상을 찍는 간격(초). */
	UPROPERTY(EditDefaultsOnly, Category="Dash|Ghost Trail", meta=(ClampMin=0.02, Units="s", EditCondition="bSpawnGhostTrail"))
	float GhostSpawnInterval = 0.065f;

	/** 잔상 하나가 서서히 사라지기까지 걸리는 시간(초). */
	UPROPERTY(EditDefaultsOnly, Category="Dash|Ghost Trail", meta=(ClampMin=0.05, Units="s", EditCondition="bSpawnGhostTrail"))
	float GhostLifetime = 0.26f;

	/** 잔상 생성 직후 초기 불투명도 (GhostOpacity). */
	UPROPERTY(EditDefaultsOnly, Category="Dash|Ghost Trail", meta=(ClampMin=0.05, ClampMax=1.0, EditCondition="bSpawnGhostTrail"))
	float GhostInitialOpacity = 0.65f;

	/** 한손 스탠스 대시 잔상 색상 (푸른 사이버 홀로그램 HDR). */
	UPROPERTY(EditDefaultsOnly, Category="Dash|Ghost Trail", meta=(EditCondition="bSpawnGhostTrail"))
	FLinearColor OneHandedGhostColor = FLinearColor(0.15f, 1.1f, 3.2f, 1.f);

	/** 양손 스탠스 대시 잔상 색상 (진홍 오버드라이브 홀로그램 HDR). */
	UPROPERTY(EditDefaultsOnly, Category="Dash|Ghost Trail", meta=(EditCondition="bSpawnGhostTrail"))
	FLinearColor TwoHandedGhostColor = FLinearColor(3.2f, 0.22f, 0.08f, 1.f);

private:
	UAnimMontage* SelectDashMontage(const ACharacter& Character, const FVector& Direction) const;

	/** 대시 한 번(방향·몽타주·무적·루트 모션). */
	void StartDash(ACharacter& Character);

	/**
	 * 몽타주 재생 속도. 지금 규칙: MontagePlayRate 고정.
	 * 바뀔 수 있는 항목이다(예: 대시 + 경직 시간에 맞춘 자동 배속, 대시 부분만 맞추기). 바꿀 때는 이 함수만 고치면 된다.
	 */
	float GetMontagePlayRate(const UAnimMontage* Montage) const;

	/** 메카 속성 값을 읽는다. 해당 속성 세트가 없으면 Fallback. */
	float GetMechAttributeValue(const FGameplayAttribute& Attribute, float Fallback) const;

	UFUNCTION()
	void OnDashFinished();

	UFUNCTION()
	void OnInvulnerabilityEnded();

	UFUNCTION()
	void OnRecoveryFinished();

	/** 경직 상태(Window.Cancel, 이동 잠금)를 푼다. */
	void ReleaseRecoveryState();

	void StartCooldown();
	void RemoveInvulnerability();

	void StartBackThrusters(const ACharacter& Character);
	void UpdateBackThrusters();
	void StopBackThrusters();
	void SpawnGhostStep();
	void StopGhostTrail();

	UPROPERTY(Transient)
	TArray<TObjectPtr<UNiagaraComponent>> BackThrusters;

	FTimerHandle BackThrusterTimer;
	FTimerHandle BackThrusterStopTimer;
	FTimerHandle GhostTrailTimer;

	/** (2026-10-28) 이번 대시가 붙인 FollowUpTags를 FollowUpGraceTime 뒤에 뗀다(붙이지 않았으면 아무것도 안 함). EndAbility에서 부른다. */
	void ReleaseFollowUpTags();

	bool bInvulnerableApplied = false;
	bool bFollowUpApplied = false;
	bool bCancelWindowApplied = false;
	bool bMoveLockApplied = false;
	bool bInRecovery = false;

	/** 이번 대시의 방향(수평 단위 벡터). 경직에서 관성 이어가기 여부를 정할 때 쓴다. */
	FVector DashDirection = FVector::ZeroVector;

	/** 이 월드 시각(초)이 지나기 전에는 다시 발동할 수 없다. 어빌리티가 액터별 인스턴스라 액터마다 따로 기록된다. */
	float CooldownEndTime = 0.f;

	/** 대시를 시작했지만 아직 쿨타임을 걸지 않았는지. 이동이 끝나거나, 그 전에 어빌리티가 끊기면 건다. */
	bool bCooldownPending = false;
};
