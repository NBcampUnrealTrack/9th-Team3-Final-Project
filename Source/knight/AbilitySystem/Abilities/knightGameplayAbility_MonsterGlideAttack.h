#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/knightGameplayAbility_MonsterAttack.h"
#include "Combat/knightCombatTypes.h"
#include "knightGameplayAbility_MonsterGlideAttack.generated.h"

class UAbilityTask_Repeat;
class UAnimMontage;

/**
 * 공중 턴의 날짐승이 포효한 뒤 낮게 활강하며 지나가면서 꼬리로 찌르는 공격(잔향 활강 꼬리 찌르기).
 *
 *   포효: RoarMontage를 RoarTime 동안 재생하며 타깃 쪽으로 돈다.
 *   찌르기: 방향·거리를 고정하고 StrikeMontage를 재생한다(가까우면 MinGlideSpeed를 지키려고 앞부분을 건너뛴다). StabTime에 꼬리 끝(몸 앞 TailReach)이 고정한 타깃 위치에
 *           오도록 일정한 속도로 날고, 같은 동안 메시가 바닥 높이로 내려온다. 찌른 뒤에도 PassTime 동안 같은 속도로 지나간다.
 *           HitStartTime ~ HitEndTime 동안 HitSocket(꼬리 끝) 구체에 닿은 대상을 한 번씩 때린다.
 *   연속: 분노 중이면 EnragedStrikeCount번. 중간 찌르기는 타깃 너머 RepassDistance까지 지나간 뒤 TurnTime 동안 멈춰 타깃 쪽으로 돈다.
 *   탈진 착지: 공중 턴을 끝내고 LandMontage를 LandHoldTime 동안, 이어서 GetUpMontage를 재생한다(공격 없는 딜 타임).
 *             그동안 State.Monster.Collapsed를 붙인다(날개로 플레이어를 밀어내지 않음).
 *             일어나는 동안(GetUpMontage)은 State.Monster.GettingUp도 붙인다(잔향 가슴 약점 노출).
 *             끝나면 지상 턴이 처음부터 시작된다.
 *             공중 턴을 스스로 끝내므로 Selection.RequiredOwnerTags에 State.Monster.LastAirAttack을 넣어 마지막 공격으로만 쓴다.
 *
 * bDarkenView면 포효부터 착지까지 플레이어 시야를 어둠으로 덮고, 첫 찌르기는 어둠이 덮인 뒤 DarknessHoldTime이 지나야 시작한다.
 * 그리고 찌르기 SilhouetteLeadTime 전부터 찌르는 순간까지 몸 윤곽선만 보인다.
 *
 * 찌르는 동안 State.Monster.NoFlinch를 붙여 패링 경직이 활강을 끊지 않는다. 그로기 게이지가 다 차면 다운이 끊는다(격추).
 * 디버그: knight.Monster.DrawAttacks 1 이면 판정 구체를 그린다.
 */
UCLASS()
class KNIGHT_API UknightGameplayAbility_MonsterGlideAttack : public UknightGameplayAbility_MonsterAttack
{
	GENERATED_BODY()

public:
	UknightGameplayAbility_MonsterGlideAttack(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	/** 선택 사항. 활강 전에 재생한다. */
	UPROPERTY(EditDefaultsOnly, Category="Attack")
	TObjectPtr<UAnimMontage> RoarMontage;

	UPROPERTY(EditDefaultsOnly, Category="Attack", meta=(ClampMin=0, Units="s"))
	float RoarTime = 1.5f;

	UPROPERTY(EditDefaultsOnly, Category="Attack")
	TObjectPtr<UAnimMontage> StrikeMontage;

	UPROPERTY(EditDefaultsOnly, Category="Attack")
	FknightMeleeHitParams HitParams;

	/** 꼬리 끝이 가장 앞으로 나오는 시각(StrikeMontage 초). 이때 꼬리 끝이 고정한 타깃 위치에 온다. */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Glide", meta=(ClampMin=0.05, Units="s"))
	float StabTime = 0.92f;

	/** StabTime에 몸(캡슐) 중심에서 꼬리 끝까지의 수평 거리. */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Glide", meta=(ClampMin=0, Units="cm"))
	float TailReach = 800.f;

	/**
	 * 활강 속도의 최솟값. 타깃이 가까워 이보다 느려지면 StrikeMontage를 앞부분을 건너뛰고 시작해, 이 속도로 날아도
	 * StabTime에 꼬리 끝이 타깃에 오게 한다.
	 */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Glide", meta=(ClampMin=0, Units="cm/s"))
	float MinGlideSpeed = 1500.f;

	/** 마지막 찌르기 뒤 같은 속도로 지나가는 시간(관성). 짧게 지나가 타깃 근처에 내려앉는다. */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Glide", meta=(ClampMin=0, Units="s"))
	float PassTime = 0.6f;

	/** 연속 찌르기 중간에는 타깃 너머 이 거리(몸 기준)까지 관성으로 지나간 뒤 돌아선다. 다음 활강에 쓸 거리를 남긴다. */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Glide", meta=(ClampMin=0, Units="cm"))
	float RepassDistance = 1300.f;

	UPROPERTY(EditDefaultsOnly, Category="Attack|Glide", meta=(ClampMin=1))
	int32 StrikeCount = 1;

	/** State.Monster.Enraged일 때의 연속 횟수(잔향 공명해방 3연격). */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Glide", meta=(ClampMin=1))
	int32 EnragedStrikeCount = 3;

	/** 연속 찌르기 사이에 멈춰서 타깃 쪽으로 도는 시간. */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Glide", meta=(ClampMin=0, Units="s"))
	float TurnTime = 0.9f;

	UPROPERTY(EditDefaultsOnly, Category="Attack|Glide", meta=(ClampMin=0, Units="deg"))
	float TurnRate = 240.f;

	UPROPERTY(EditDefaultsOnly, Category="Attack|Hit")
	FName HitSocket = TEXT("tail_08");

	UPROPERTY(EditDefaultsOnly, Category="Attack|Hit", meta=(ClampMin=1, Units="cm"))
	float HitRadius = 250.f;

	UPROPERTY(EditDefaultsOnly, Category="Attack|Hit", meta=(ClampMin=0, Units="s"))
	float HitStartTime = 0.74f;

	UPROPERTY(EditDefaultsOnly, Category="Attack|Hit", meta=(ClampMin=0, Units="s"))
	float HitEndTime = 1.1f;

	/** 탈진 착지(바닥에 떨어져 엎드림). 비어 있으면 착지 없이 끝난다. */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Land")
	TObjectPtr<UAnimMontage> LandMontage;

	UPROPERTY(EditDefaultsOnly, Category="Attack|Land", meta=(ClampMin=0, Units="s"))
	float LandHoldTime = 4.f;

	UPROPERTY(EditDefaultsOnly, Category="Attack|Land")
	TObjectPtr<UAnimMontage> GetUpMontage;

	/** 포효부터 착지(또는 끊길 때)까지 플레이어 시야를 어둠으로 덮는다. 소리는 그대로다. */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Darkness")
	bool bDarkenView = false;

	/** 어둠이 덮이고(포효 시작) 첫 찌르기까지 기다리는 시간. RoarTime보다 짧으면 RoarTime을 쓴다. 그동안 타깃 쪽으로 돈다. */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Darkness", meta=(EditCondition="bDarkenView", ClampMin=0, Units="s"))
	float DarknessHoldTime = 3.f;

	/** 찌르는 순간(StabTime) 이 시간 전부터 어둠 속에 몸 윤곽선이 희미하게 보인다. 0이면 보이지 않는다. */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Darkness", meta=(EditCondition="bDarkenView", ClampMin=0, Units="s"))
	float SilhouetteLeadTime = 0.5f;

private:
	UFUNCTION()
	void HandleTick(int32 ActionNumber);

	UFUNCTION()
	void HandleGetUpFinished();

	void StartStrike();
	void StartLand();
	void ApplyHits();
	void SetNoFlinch(bool bOn);
	void SetDarkness(bool bOn);
	void SetCollapsed(bool bOn);
	void SetSilhouette(bool bOn);
	void SetGettingUp(bool bOn);

	enum class EPhase : uint8 { Roar, Strike, Turn, Land, GetUp };

	TWeakObjectPtr<UAbilityTask_Repeat> TickTask;
	TSet<TWeakObjectPtr<AActor>> HitActors;
	EPhase Phase = EPhase::Roar;
	float PhaseTime = 0.f;
	int32 StrikesLeft = 0;
	/** 이번 찌르기가 끝나는 시각(StrikeMontage 초). */
	float StrikeEndTime = 0.f;
	bool bNoFlinchApplied = false;
	bool bLanded = false;
	bool bDarknessOn = false;
	bool bCollapsedApplied = false;
	bool bSilhouetteOn = false;
	bool bGettingUpApplied = false;
};
