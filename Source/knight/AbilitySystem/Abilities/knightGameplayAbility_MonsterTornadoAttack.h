#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/knightGameplayAbility_MonsterAttack.h"
#include "Combat/knightCombatTypes.h"
#include "Monster/knightMonsterTornado.h"
#include "knightGameplayAbility_MonsterTornadoAttack.generated.h"

class UAbilityTask_Repeat;
class UAnimMontage;

/**
 * 공중 턴의 날짐승이 날갯짓으로 바닥에 회오리(AknightMonsterTornado)를 만들어 타깃을 쫓게 한 뒤 지쳐 내려앉는 공격(잔향 회오리).
 *
 *   날갯짓: SummonMontage를 재생하며 타깃 쪽으로 돈다. SpawnTime에 몸 아래 바닥에 회오리를 만들고,
 *           분노 중이면 EnragedTornadoCount개를 SpawnInterval 간격으로 만든다. 여러 개면 좌우로 SpawnSpread씩 벌려 놓고
 *           첫 진행 방향도 SpreadAngle씩 벌려 양옆에서 조여 오게 한다. 0이면 각자 자리에서 타깃 쪽으로 나가 타깃 근처에서 엇갈린다.
 *           bEnragedVolley면 분노 중에는 벌리지 않고 한 자리에서 연달아 쏘고, 마지막 하나는 EnragedLastScale배 크기로 만든다.
 *   지침: ExhaustTime부터 TiredTime 동안 날갯짓(메시 애니메이션 전체)이 TiredAnimRate까지 점점 느려지고, 높이도 TiredAirAlpha까지
 *         내려온다. 떨어질 것을 미리 보여 플레이어가 다가갈 틈을 준다. 착지 때는 남은 높이를 LandDescendTime 동안 내려온다.
 *   탈진 착지: 지침이 끝나면 공중 턴을 끝내고 LandMontage를 LandHoldTime 동안, 이어서 GetUpMontage를 재생한다(공격 없는 딜 타임).
 *             공중이 아니면 착지 없이 끝난다. 일어난 뒤부터 지상 턴이 처음부터 시작된다.
 *             착지부터 끝날 때까지 State.Monster.Collapsed를 붙인다(엎드린 몸·날개로 플레이어를 밀어내지 않음).
 *             일어나는 동안(GetUpMontage)은 State.Monster.GettingUp도 붙인다(잔향 가슴 약점 노출).
 *
 * 공중 턴을 스스로 끝내므로 Selection.RequiredOwnerTags에 State.Monster.LastAirAttack을 넣어 공중 턴의 마지막 공격으로만 쓴다.
 *
 * 회오리는 어빌리티와 따로 움직이므로 착지하는 동안에도 타깃을 쫓는다. 회피만 되게 하려면 HitParams.DamageEffect에
 * 가드 범위 0인 피해 실행을 쓰고 bCanBeParried를 끈다.
 */
UCLASS()
class KNIGHT_API UknightGameplayAbility_MonsterTornadoAttack : public UknightGameplayAbility_MonsterAttack
{
	GENERATED_BODY()

public:
	UknightGameplayAbility_MonsterTornadoAttack(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	UPROPERTY(EditDefaultsOnly, Category="Attack")
	TObjectPtr<UAnimMontage> SummonMontage;

	UPROPERTY(EditDefaultsOnly, Category="Attack")
	FknightMeleeHitParams HitParams;

	UPROPERTY(EditDefaultsOnly, Category="Attack|Tornado")
	FknightMonsterTornadoParams Tornado;

	/** 발동부터 첫 회오리가 생기기까지(SummonMontage 초). */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Tornado", meta=(ClampMin=0, Units="s"))
	float SpawnTime = 1.f;

	UPROPERTY(EditDefaultsOnly, Category="Attack|Tornado", meta=(ClampMin=1))
	int32 TornadoCount = 1;

	/** State.Monster.Enraged일 때 만드는 수(잔향 공명해방 2개). */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Tornado", meta=(ClampMin=1))
	int32 EnragedTornadoCount = 2;

	/** 분노 중에는 좌우로 벌리지 않고 몸 아래 한 자리에서 SpawnInterval 간격으로 연달아 타깃을 향해 쏜다(잔향 공명 3연사). */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Tornado")
	bool bEnragedVolley = false;

	/** 분노 중 마지막 회오리의 크기 배율. 판정(Radius·Height)과 이펙트, 물결이 함께 커진다. */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Tornado", meta=(ClampMin=0.1))
	float EnragedLastScale = 1.f;

	UPROPERTY(EditDefaultsOnly, Category="Attack|Tornado", meta=(ClampMin=0, Units="s"))
	float SpawnInterval = 0.6f;

	/** 여러 개일 때 이웃한 회오리 사이의 거리(몸 오른쪽 방향). */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Tornado", meta=(ClampMin=0, Units="cm"))
	float SpawnSpread = 800.f;

	/** 여러 개일 때 이웃한 회오리의 첫 진행 방향 차이. */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Tornado", meta=(ClampMin=0, ClampMax=180, Units="deg"))
	float SpreadAngle = 70.f;

	UPROPERTY(EditDefaultsOnly, Category="Attack|Tornado", meta=(ClampMin=0, Units="deg"))
	float TurnRate = 180.f;

	/** 발동부터 지치기 시작하기까지. 마지막 회오리가 생긴 뒤여야 한다. */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Land", meta=(ClampMin=0, Units="s"))
	float ExhaustTime = 2.5f;

	/** 공중에서 날갯짓이 느려지는 시간. 끝나면 탈진 착지한다. 0이면 바로 떨어진다. */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Land", meta=(ClampMin=0, Units="s"))
	float TiredTime = 3.f;

	/** 지침이 끝날 때의 애니메이션 속도 배율. */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Land", meta=(ClampMin=0.05, ClampMax=1))
	float TiredAnimRate = 0.35f;

	/** 지치는 동안 내려오는 높이(0 = 지상 높이, 1 = 공중 높이). 처음엔 천천히, 끝으로 갈수록 빨리 내려온다. */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Land", meta=(ClampMin=0, ClampMax=1))
	float TiredAirAlpha = 0.35f;

	/** 지침이 끝난 높이에서 바닥까지 내려오는 시간. 착지(LandMontage)와 함께 시작한다. */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Land", meta=(ClampMin=0, Units="s"))
	float LandDescendTime = 0.8f;

	/** 탈진 착지(바닥에 떨어져 엎드림). 비어 있으면 착지 없이 끝난다. */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Land")
	TObjectPtr<UAnimMontage> LandMontage;

	UPROPERTY(EditDefaultsOnly, Category="Attack|Land", meta=(ClampMin=0, Units="s"))
	float LandHoldTime = 4.f;

	UPROPERTY(EditDefaultsOnly, Category="Attack|Land")
	TObjectPtr<UAnimMontage> GetUpMontage;

private:
	UFUNCTION()
	void HandleTick(int32 ActionNumber);

	UFUNCTION()
	void HandleGetUpFinished();

	void SpawnTornado(int32 Index, int32 Count);
	void StartLand();
	/** 메시 애니메이션 속도 배율(지침). */
	void SetAnimRate(float Rate);
	void SetGettingUp(bool bOn);

	enum class EPhase : uint8 { Summon, Tired, Land, GetUp };

	TWeakObjectPtr<UAbilityTask_Repeat> TickTask;
	EPhase Phase = EPhase::Summon;
	float PhaseTime = 0.f;
	int32 SpawnCount = 0;
	int32 Spawned = 0;
	bool bEnraged = false;
	bool bLanded = false;
	bool bGettingUpApplied = false;
};
