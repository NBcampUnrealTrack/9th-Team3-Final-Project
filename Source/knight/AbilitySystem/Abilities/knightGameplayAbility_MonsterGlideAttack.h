#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/knightGameplayAbility_MonsterAttack.h"
#include "Combat/knightCombatTypes.h"
#include "knightGameplayAbility_MonsterGlideAttack.generated.h"

class AknightMonsterCharacterBase;
class UAbilityTask_Repeat;
class UAnimMontage;
class USoundBase;

/** 활강 찌르기 한 번. */
USTRUCT(BlueprintType)
struct FknightMonsterGlideStrike
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TObjectPtr<UAnimMontage> Montage;

	/** 판정 부위가 가장 앞으로 나오는 시각(Montage 초). 이때 판정 부위가 겨눈 곳에 온다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin=0.05, Units="s"))
	float StabTime = 0.92f;

	/** StabTime에 몸(캡슐) 중심에서 판정 부위까지의 수평 거리. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin=0, Units="cm"))
	float Reach = 800.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName HitSocket = TEXT("tail_08");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin=1, Units="cm"))
	float HitRadius = 250.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin=0, Units="s"))
	float HitStartTime = 0.74f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin=0, Units="s"))
	float HitEndTime = 1.1f;

	/** 어둠 속이면 이 찌르기 전에 타깃 등 뒤(BehindDistance)로 돌아간다. 끄면 있던 자리에서 바로 날아든다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	bool bFromBehind = true;
};

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
 *   등 뒤로 돌기: BehindDistance > 0이면 CircleStartTime부터 첫 찌르기까지 타깃 둘레를 돌아 카메라 반대쪽(등 뒤)으로 간다.
 *                 그동안 CircleFlapSound를 되풀이해 소리로 위치를 짐작하게 한다.
 *   추격: 찌르는 동안 StabTime - HomingLockTime까지 HomingTurnRate로 타깃 쪽으로 방향을 꺾고, HomingMaxSpeed가 있으면 속도도 다시 맞춘다.
 *         타깃이 움직이던 방향으로 고정 시간만큼 앞을 겨눈다. 그 뒤로는 고정이라 그 순간 구르거나 패링해야 피한다.
 *   어둠이 없으면 두 동작 모두 하지 않는다(정면에서 일직선).
 *
 * Strikes가 있으면 찌르기마다 몽타주·판정을 따로 쓰고, 그 수만큼 찌른다(분노 연속 횟수는 쓰지 않음). 비어 있으면 위 값으로 StrikeCount번,
 * 첫 찌르기만 등 뒤에서 한다. 찌르기 사이에 등 뒤로 돌 때는 공중 높이로 올라가 TurnTime 동안 어둠 밖을 돈다(잔향 어둠 기습).
 * bLand를 끄면 마지막 찌르기 뒤 착지하지 않고 RiseTime 동안 공중 높이로 돌아가 공중 턴을 이어 간다.
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

	/** 비어 있지 않으면 StrikeMontage·StabTime·TailReach·Hit 값 대신 이 순서대로 찌른다. */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Glide")
	TArray<FknightMonsterGlideStrike> Strikes;

	/** 마지막 찌르기 뒤 탈진 착지로 공중 턴을 끝낸다. 끄면 다시 날아올라 공중 턴을 이어 간다. */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Land")
	bool bLand = true;

	/** bLand가 꺼져 있을 때 공중 높이로 돌아가는 시간. */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Land", meta=(EditCondition="!bLand", ClampMin=0.1, Units="s"))
	float RiseTime = 1.2f;

	/** bLand가 꺼져 있을 때 날아오르며 재생한다(선택). */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Land", meta=(EditCondition="!bLand"))
	TObjectPtr<UAnimMontage> RiseMontage;

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

	/**
	 * 어둠 속에서 날아드는 동안 몬스터 몸에 붙는 검붉은 빛무리 경고의 시작 세기(0~1). 날아들기 시작할 때 이 세기로 켜서
	 * 방향을 고정하는 순간(StabTime - HomingLockTime) 1이 되고, 찌르는 순간 끈다. 0이면 경고 없음.
	 */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Darkness", meta=(EditCondition="bDarkenView", ClampMin=0, ClampMax=1))
	float WarningStartStrength = 0.2f;

	/** 빛무리 중심이 되는 뼈. */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Darkness", meta=(EditCondition="bDarkenView"))
	FName WarningBone = TEXT("spine_02");

	/** 어둠 속 첫 찌르기를 시작할 위치: 타깃에서 카메라 반대쪽으로 이 거리. 벽이 있으면 벽 앞까지. 0이면 돌지 않는다. */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Darkness", meta=(EditCondition="bDarkenView", ClampMin=0, Units="cm"))
	float BehindDistance = 2200.f;

	/** 어둠이 덮이고 이 시간이 지나면 등 뒤로 돌기 시작한다(어둠이 다 덮이기 전에 움직이는 게 보이지 않게). */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Darkness", meta=(EditCondition="bDarkenView", ClampMin=0, Units="s"))
	float CircleStartTime = 0.8f;

	UPROPERTY(EditDefaultsOnly, Category="Attack|Darkness", meta=(EditCondition="bDarkenView", ClampMin=0, Units="cm/s"))
	float CircleMaxSpeed = 3000.f;

	/** 등 뒤로 도는 동안 CircleFlapInterval마다 몸에 붙여 재생한다(위치 단서). */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Darkness", meta=(EditCondition="bDarkenView"))
	TObjectPtr<USoundBase> CircleFlapSound;

	UPROPERTY(EditDefaultsOnly, Category="Attack|Darkness", meta=(EditCondition="bDarkenView", ClampMin=0.05, Units="s"))
	float CircleFlapInterval = 0.5f;

	/** 어둠 속 찌르기가 타깃 쪽으로 방향을 꺾는 최대 속도. 0이면 일직선. */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Darkness", meta=(EditCondition="bDarkenView", ClampMin=0, Units="deg"))
	float HomingTurnRate = 90.f;

	/**
	 * 찌르는 순간(StabTime) 이 시간 전부터는 방향과 속도를 고정한다. 마지막 순간 구르기로 피할 틈.
	 * 고정하기 전에는 타깃이 움직이던 속도로 이 시간만큼 앞을 겨누므로, 같은 방향으로 계속 달리면 맞는다.
	 */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Darkness", meta=(EditCondition="bDarkenView", ClampMin=0, Units="s"))
	float HomingLockTime = 0.3f;

	/**
	 * 어둠 속 찌르기의 속도 범위. 고정하기 전까지 매 틱 남은 시간으로 다시 정해 StabTime에 꼬리 끝이 겨눈 곳에 오게 한다.
	 * HomingMaxSpeed가 0이면 속도는 시작할 때 그대로다(방향만 꺾음).
	 */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Darkness", meta=(EditCondition="bDarkenView", ClampMin=0, Units="cm/s"))
	float HomingMinSpeed = 600.f;

	UPROPERTY(EditDefaultsOnly, Category="Attack|Darkness", meta=(EditCondition="bDarkenView", ClampMin=0, Units="cm/s"))
	float HomingMaxSpeed = 0.f;

private:
	UFUNCTION()
	void HandleTick(int32 ActionNumber);

	UFUNCTION()
	void HandleGetUpFinished();

	void StartStrike();
	void StartLand();
	void StartRise();
	/** Index번째 찌르기. Strikes가 비어 있으면 StrikeMontage 등 단일 값으로 만든다. */
	FknightMonsterGlideStrike GetStrike(int32 Index) const;
	/** 다음 찌르기를 등 뒤에서 하려고 어둠 속을 돌고 있어야 하면 true. */
	bool ShouldCircleForNextStrike() const;
	void ApplyHits();
	void SetNoFlinch(bool bOn);
	void SetDarkness(bool bOn);
	void SetCollapsed(bool bOn);
	void SetSilhouette(bool bOn);
	/** 어둠 속 몬스터 몸에 Strength만큼 검붉은 빛무리를 띄운다. */
	void SetWarning(float Strength);
	void SetGettingUp(bool bOn);
	void UpdateCircle(AknightMonsterCharacterBase& Monster, const AActor& Target, float HoldTime);
	FVector GetBehindLocation(const AknightMonsterCharacterBase& Monster, const AActor& Target) const;
	void StartGlideForce(const FVector& Velocity, float Duration);
	void SetGlideVelocity(const FVector& Velocity);
	void StopGlideForce();

	enum class EPhase : uint8 { Roar, Strike, Turn, Land, GetUp, Rise };

	TWeakObjectPtr<UAbilityTask_Repeat> TickTask;
	TSet<TWeakObjectPtr<AActor>> HitActors;
	EPhase Phase = EPhase::Roar;
	float PhaseTime = 0.f;
	int32 StrikesLeft = 0;
	/** 지금(Strike) 또는 다음(Roar·Turn) 찌르기 번호. */
	int32 StrikeIndex = 0;
	FknightMonsterGlideStrike CurrentStrike;
	/** 이번 찌르기를 시작한 몽타주 위치(앞부분을 건너뛰면 0보다 크다). */
	float StrikeStartTime = 0.f;
	/** 이번 찌르기가 끝나는 시각(StrikeMontage 초). */
	float StrikeEndTime = 0.f;
	/** 지금 쓰는 이동 루트 모션 소스(등 뒤로 돌기, 활강). 0이면 없음. */
	uint16 GlideForceID = 0;
	FVector GlideDirection = FVector::ForwardVector;
	float GlideSpeed = 0.f;
	float CircleFlapTime = 0.f;
	bool bCircling = false;
	bool bNoFlinchApplied = false;
	bool bLanded = false;
	bool bDarknessOn = false;
	bool bCollapsedApplied = false;
	bool bSilhouetteOn = false;
	bool bGettingUpApplied = false;
};
