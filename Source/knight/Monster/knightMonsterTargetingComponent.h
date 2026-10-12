#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "knightMonsterTargetingComponent.generated.h"

class UAbilitySystemComponent;
struct FGameplayEffectSpec;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FknightMonsterTargetChanged, AActor*, OldTarget, AActor*, NewTarget);

/**
 * 몬스터가 싸울 대상을 고른다. 전통적인 위협도 테이블을 대신한다. 솔로 레이드에서 후보는
 * 플레이어와, 나중에 추가되는 목표물(발전소)이나 아군이다.
 *
 * 후보는 AI Perception(UknightBossAIController가 시야/청각/피해를 전달)과 AddObjective에서 들어온다.
 * 점수 = Priority / (1 + 거리 / DistanceFalloff) + 최근 피해 위협도.
 * 피해 위협도는 시간이 지나며 줄어들므로, 몬스터를 때리면 한동안 어그로가 끌린다. 더 이상 감지되지
 * 않는 후보도 LoseTargetDelay초 동안은 유효하다. CloseAwarenessRadius 안에 있는 후보는 시야 밖(등 뒤)이어도 감지한 것으로 본다.
 */
UCLASS(ClassGroup=(Knight), meta=(BlueprintSpawnableComponent))
class KNIGHT_API UknightMonsterTargetingComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UknightMonsterTargetingComponent();

	/** 피해 위협도를 소유자의 Core AttributeSet에 바인딩한다. 몬스터가 ASC 준비를 마친 뒤 호출한다. */
	void InitializeTargeting(UAbilitySystemComponent* InASC);

	/** AI 컨트롤러에서 오는 Perception 갱신. */
	void NotifyPerceived(AActor* Actor, bool bSensed);

	/** 몬스터가 감지하지 않아도 고려해야 할 타깃(방어 목표물). */
	UFUNCTION(BlueprintCallable, Category="Knight|Monster")
	void AddObjective(AActor* Objective, float Priority = 0.5f);

	UFUNCTION(BlueprintCallable, Category="Knight|Monster")
	void RemoveCandidate(AActor* Actor);

	/** 다음 평가를 기다리지 않고 지금 후보 점수를 다시 매긴다. */
	UFUNCTION(BlueprintCallable, Category="Knight|Monster")
	void EvaluateTarget();

	/** 평가를 멈추고 모든 후보를 잊는다(사망 시). */
	void StopTargeting();

	UFUNCTION(BlueprintPure, Category="Knight|Monster")
	AActor* GetCurrentTarget() const { return CurrentTarget.Get(); }

	/** 현재 타깃이 등 뒤(정면에서 BehindAngle 이상)에 계속 머문 시간(초). 평가 간격(EvaluationInterval)마다 갱신한다. */
	float GetTargetBehindTime() const;

	UPROPERTY(BlueprintAssignable, Category="Knight|Monster")
	FknightMonsterTargetChanged OnTargetChanged;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** 자동 평가 간격(초). */
	UPROPERTY(EditDefaultsOnly, Category="Knight|Monster", meta=(ClampMin=0.05, Units="s"))
	float EvaluationInterval = 0.5f;

	/** 감지된 적대 폰의 우선순위. 목표물은 AddObjective에 넘긴 우선순위를 쓴다. */
	UPROPERTY(EditDefaultsOnly, Category="Knight|Monster", meta=(ClampMin=0))
	float PerceivedPriority = 1.f;

	UPROPERTY(EditDefaultsOnly, Category="Knight|Monster", meta=(ClampMin=1, Units="cm"))
	float DistanceFalloff = 3000.f;

	/** 후보에게 받은 피해 1당 더해지는 위협도. */
	UPROPERTY(EditDefaultsOnly, Category="Knight|Monster", meta=(ClampMin=0))
	float ThreatPerDamage = 0.01f;

	/** 초당 줄어드는 위협도 비율. */
	UPROPERTY(EditDefaultsOnly, Category="Knight|Monster", meta=(ClampMin=0, ClampMax=1))
	float ThreatDecayPerSecond = 0.1f;

	/** 새 타깃은 현재 점수보다 이 배율 이상 높아야 한다. 몬스터가 타깃을 이리저리 바꾸지 않게 한다. */
	UPROPERTY(EditDefaultsOnly, Category="Knight|Monster", meta=(ClampMin=1))
	float SwitchHysteresis = 1.25f;

	/**
	 * 마지막으로 보거나 듣거나 맞은 뒤 후보가 유효하게 남는 유지 시간(초). 보스는 등 뒤로 돌아간 플레이어를
	 * 시야각을 벗어나는 즉시 놓치지 않고 계속 쫓는다.
	 */
	UPROPERTY(EditDefaultsOnly, Category="Knight|Monster", meta=(ClampMin=0, Units="s"))
	float LoseTargetDelay = 8.f;

	/**
	 * 이 수평 거리(몬스터 중심 기준) 안에 있는 후보는 시야 밖이어도 계속 감지한 것으로 본다. 0이면 끈다.
	 * 시야는 앞쪽만 보므로, 공격하지 않고 등 뒤에 머무는 플레이어를 LoseTargetDelay 뒤에 잊어버리는 것을 막는다.
	 * 이미 한 번 감지한 후보에만 적용된다(처음 발견은 여전히 시야·청각·피해로).
	 */
	UPROPERTY(EditDefaultsOnly, Category="Knight|Monster", meta=(ClampMin=0, Units="cm"))
	float CloseAwarenessRadius = 2500.f;

	/** 타깃이 정면에서 이 각도 이상 벗어나 있으면 등 뒤로 본다(GetTargetBehindTime, 뒤쪽 공격의 MinTargetBehindTime). */
	UPROPERTY(EditDefaultsOnly, Category="Knight|Monster", meta=(ClampMin=0, ClampMax=180, Units="deg"))
	float BehindAngle = 120.f;

private:
	struct FCandidate
	{
		TWeakObjectPtr<AActor> Actor;
		float Priority = 1.f;
		float Threat = 0.f;
		/** 마지막으로 감지되었거나 몬스터를 때린 월드 시간. */
		double LastSensedTime = -1.e9;
		bool bPerceived = false;
		bool bObjective = false;
	};

	void HandleDamaged(AActor* DamageInstigator, AActor* DamageCauser, const FGameplayEffectSpec* EffectSpec,
		float Magnitude, float OldValue, float NewValue);
	FCandidate* FindCandidate(const AActor* Actor);
	float ScoreCandidate(const FCandidate& Candidate) const;
	bool IsCandidateValid(const FCandidate& Candidate, double Now) const;
	void SetCurrentTarget(AActor* NewTarget);
	void UpdateTargetBehind(double Now);

	TArray<FCandidate> Candidates;
	TWeakObjectPtr<AActor> CurrentTarget;
	TWeakObjectPtr<UAbilitySystemComponent> BoundASC;
	FTimerHandle EvaluationTimer;
	double LastEvaluationTime = 0.0;
	/** 현재 타깃이 등 뒤로 들어간 월드 시간. 등 뒤가 아니면 음수. */
	double TargetBehindSince = -1.0;
	bool bStopped = false;
};
