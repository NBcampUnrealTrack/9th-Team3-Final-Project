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
 *   탈진 착지: ExhaustTime에 공중 턴을 끝내고 LandMontage를 LandHoldTime 동안, 이어서 GetUpMontage를 재생한다(공격 없는 딜 타임).
 *             공중이 아니면 착지 없이 끝난다. 일어난 뒤부터 지상 턴이 처음부터 시작된다.
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

	/** 발동부터 탈진 착지를 시작하기까지. 마지막 회오리가 생긴 뒤여야 한다. */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Land", meta=(ClampMin=0, Units="s"))
	float ExhaustTime = 2.5f;

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

	enum class EPhase : uint8 { Summon, Land, GetUp };

	TWeakObjectPtr<UAbilityTask_Repeat> TickTask;
	EPhase Phase = EPhase::Summon;
	float PhaseTime = 0.f;
	int32 SpawnCount = 0;
	int32 Spawned = 0;
	bool bLanded = false;
};
