#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/knightGameplayAbility.h"
#include "Character/knightThrusterComponent.h"
#include "knightGameplayAbility_BoostDash.generated.h"

class ACharacter;
class UAnimMontage;
class UCurveFloat;
class UMaterialInterface;
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
 * (2026-10-29 사용자 결정) 퀵스텝 → 구르기: 몽타주를 구르기(양손 2H-A_Roll, 한손 1H-B_Roll)로 바꾸고 1배속으로 끝까지 재생한다.
 *   한손 스탠스는 OneHandedDashMontage 칸들을 쓴다. 잔상은 끄고 부스터 소리는 남겼다(GA_Knight_BoostDash).
 *   부스터는 구르기를 시작하는 순간 구르는 반대쪽으로 짧게 뿜는다(나이트의 부스터 컴포넌트 노즐).
 *   등(RollBackThrust)과 발(RollFootThrust)을 따로 켜고 끄며, 기본은 발만 살짝 켠다.
 *   PIE에서는 knight.Player.RollBackThrust / RollFootThrust / RollThrustScale로 바꿔 볼 수 있다.
 *   락온 중 대각선은 가장 가까운 구르기에 맞춰 몸을 최대 45° 돌려 구르고(bAlignRollToDirectionWhenLockedOn), 뒤는 왼쪽 BL·오른쪽 BR.
 *   몽타주 배속은 MontageMoveEndTime 기준으로 구르는 부분 = DashDuration, 일어나는 부분 = RecoveryDuration에 맞춘다.
 *   땅에서만 구른다. 구르는 내내 방향키로 몸이 돌지 않고(이동 잠금), 다 일어나는 순간 방향키를 누르고 있으면 바로 그 속도로 출발한다.
 *   State.Boosting 대신 State.Rolling을 단다: Boost 카메라(축소)로 바뀌지 않고, 카메라가 뒤따라오는 거리도 그대로 둔다.
 *   등이 바닥에 닿는 구간(ContactSlowStartTime ~ ContactSlowEndTime)은 ContactPlayRateScale배로 느리게 재생해 무게감을 준다.
 *   닿는 순간의 흙먼지·쿵 소리·화면 흔들림은 구르기 몽타주의 노티파이(Knight Ground Impact, Play Sound)가 낸다.
 *   공격·가드는 구르기 이동이 끝나는 순간부터 끊을 수 있다(경직의 Window.Cancel).
 *   다음 구르기는 몸이 다 일어선 시점부터 나간다. 구르는 도중 누른 구르기 키는 기억했다가 그 시점에 바로 다음 구르기로 잇는다(bQueueRollDuringRoll).
 *
 * 스탯 연결 (2026-10-06):
 *   대시 거리 = DashDistance x DodgeSpeed(메카 속성, 배율) x 중량 계수(무브먼트 컴포넌트)
 *   대시 시간(DashDuration)은 그대로 두어 몽타주 타이밍이 어긋나지 않게 한다. 그래서 거리가 늘면 속도도 빨라진다.
 *   쿨타임 = 대시 이동이 끝난 뒤 DodgeCooldown(메카 속성, 초) 동안 다시 대시할 수 없다. 기본 0.5초.
 *   (2026-10-28 사용자 결정) 기본 0초: 스테미너(StaminaCost, 0보다 많으면 가불)가 허용하는 한 연속으로 쓴다. 파츠로 늘리면 다시 걸린다.
 *   (2026-10-29 사용자 결정) 기본 0.3초: 연속 퀵스텝 사이 후딜. 그동안 누른 키는 입력 버퍼에 남았다가 쿨타임이 끝나면 나간다(대시 간격 0.65초).
 *
 * 경직과 캔슬 (2026-10-07):
 *   흐름: 대시 이동(DashDuration) → 경직(RecoveryDuration) → 종료. 몽타주는 경직까지 이어서 재생된다.
 *   경직 중에는 이동 입력으로 가속하지 않고(미끄러지며 정지) Window.Cancel을 켠다.
 *   단, 대시가 끝날 때 대시 방향 쪽으로 방향키를 누르고 있으면 이동을 잠그지 않는다(관성 이어가기, 2026-10-09, CarryMomentumMaxAngle).
 *     - 공격·가드: Window.Cancel 덕분에 활성화 그룹 규칙대로 대시를 대신한다. 대시 중 미리 누른 입력은 버퍼에서 나간다.
 *     - 대시: 실행 중에 퀵스텝 키(InputTag.Boost)를 다시 누르면 입력 처리(knightAbilitySystemComponent)가 재발동을 시도한다.
 *       경직 중이고 쿨타임이 끝났을 때만 받아들여 지금 대시를 끝내고 새 대시로 다시 시작한다(bRetriggerInstancedAbility, 비용 다시 냄).
 *       대시 이동 도중의 요청은 거절해 이동이 끊기지 않는다(그 누름은 입력 버퍼에 남아 경직이 시작되면 다시 시도된다).
 *       (2026-10-28) 쿨타임이 0이면 콤보처럼 이어진다: 이동 중에 미리 누른 키(선입력, 버퍼 0.5초)나 경직 중에 누른 키로 경직을 끊고 다음 대시.
 *       (2026-10-29) 쿨타임 0.3초가 경직(0.1초)보다 길어서, 다음 대시는 경직 캔슬이 아니라 쿨타임이 끝날 때 버퍼에서 나간다.
 *       (2026-10-29 구르기) 경직이 0.48초로 길어져, 다음 구르기는 이동이 끝나고 0.3초 뒤(경직 도중)에 경직을 끊고 나간다(간격 0.9초).
 *       같은 날 이동 0.72초·경직 0.3초로 바꿔, 쿨타임과 경직이 같이 끝난다(간격 1.02초 = 구르기 전체 길이).
 *       구르기 쿨타임은 기본 0이고, 대신 몸이 다 일어선 시점부터 다음 구르기를 받는다(CanActivateAbility의 bStoodUp).
 *
 * 입력 (2026-10-08): 퀵스텝 키(기본 LeftControl, IA_Boost → InputTag.Boost)를 한 번 누르면 대시. 달리기는 LeftShift(InputTag.Run)로 분리했다.
 * (전에는 Shift를 누르고 있으면 달리기, 두 번 탭하면 knightGameplayAbility_Sprint가 태그로 대시를 발동했다)
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
	/** 구르는 도중 구르기 키를 다시 누르면 기억해 둔다(bQueueRollDuringRoll). */
	virtual void InputPressed(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo) override;

protected:
	/**
	 * (2026-10-29 사용자 결정) 퀵스텝 비활성화: 꺼져 있으면 퀵스텝 키(Ctrl)를 눌러도 나가지 않는다(CanActivateAbility가 거절).
	 * 어빌리티 세트·입력 매핑은 그대로라, GA_Knight_BoostDash에서 이 칸만 켜면 다시 쓸 수 있다. 바뀔 수 있는 값.
	 * (2026-10-29 사용자 결정, 같은 날) 퀵스텝을 구르기로 바꾸고 다시 켬.
	 */
	UPROPERTY(EditDefaultsOnly, Category="Dash")
	bool bQuickstepEnabled = true;

	/**
	 * 배율·중량 계수가 1일 때의 구르기 거리. 4.5m: 무거운 기체가 한 번에 크게 빠지는 느낌. 바뀔 수 있는 값.
	 */
	UPROPERTY(EditDefaultsOnly, Category="Dash", meta=(ClampMin=0, Units="cm"))
	float DashDistance = 450.f;

	/**
	 * 구르기 이동 시간. 몽타주에서 몸이 실제로 나가는 구간(0 ~ MontageMoveEndTime)을 이 시간에 맞춰 재생한다.
	 * 0.8초: 등이 닿는 구간을 느리게 재생하는 몫까지 포함한 시간. 바뀔 수 있는 값. 몽타주를 바꾸면 RecoveryDuration과 같이 맞춘다.
	 */
	UPROPERTY(EditDefaultsOnly, Category="Dash", meta=(ClampMin=0.05, Units="s"))
	float DashDuration = 0.8f;

	/**
	 * 구르기 이동이 끝난 뒤의 경직(일어나는 동작) 시간. 이 동안 공격·가드로 캔슬할 수 있다. 0이면 경직 없이 바로 끝난다.
	 * 몽타주의 나머지(일어나는 동작, 구르기 몽타주 0.48초)를 이 시간에 맞춰 재생한다. 0.4초 = 약 1.2배속.
	 * 더 줄이면 일어나는 동작이 빨리 감긴 것처럼 보인다. 바뀔 수 있는 값.
	 */
	UPROPERTY(EditDefaultsOnly, Category="Dash", meta=(ClampMin=0, Units="s"))
	float RecoveryDuration = 0.4f;

	/**
	 * 몽타주에서 등이 바닥에 닿아 있는 구간의 시작·끝(몽타주 기준 초). 이 구간은 배속 x ContactPlayRateScale로 느리게 재생하고,
	 * 그만큼 나머지 이동 구간을 빠르게 해서 이동 구간 전체는 DashDuration 그대로다.
	 * 구르기 10방향 모두 가슴이 바닥 45cm 아래로 내려오는 구간이 약 0.33 ~ 0.47초라 그 구간으로 둔다. 바뀔 수 있는 값.
	 */
	UPROPERTY(EditDefaultsOnly, Category="Dash|Contact", meta=(ClampMin=0, Units="s"))
	float ContactSlowStartTime = 0.33f;

	UPROPERTY(EditDefaultsOnly, Category="Dash|Contact", meta=(ClampMin=0, Units="s"))
	float ContactSlowEndTime = 0.47f;

	/** 등이 닿는 구간의 재생 배율. 0.5면 그 구간만 절반 속도. 1이면 느리게 하지 않는다. 바뀔 수 있는 값. */
	UPROPERTY(EditDefaultsOnly, Category="Dash|Contact", meta=(ClampMin=0.1, ClampMax=1))
	float ContactPlayRateScale = 0.5f;

	/**
	 * 켜면 구르는 도중(다 일어서기 전)에 누른 구르기 키를 기억했다가, 몸이 다 일어선 순간 바로 다음 구르기로 잇는다.
	 * 입력 버퍼(0.5초)는 구르기 한 번(약 1초)보다 짧아 일찍 누른 키가 사라지므로 따로 기억한다. 끄면 입력 버퍼만 쓴다.
	 */
	UPROPERTY(EditDefaultsOnly, Category="Dash")
	bool bQueueRollDuringRoll = true;

	/**
	 * (2026-10-29) 몽타주에서 몸이 실제로 나가는 구간이 끝나는 시각(몽타주 기준 초). 구르기 몽타주는 0.6초(그 뒤는 일어나는 동작).
	 * 0보다 크면 배속을 시간에 맞춘다: 몽타주 0 ~ 이 시각을 DashDuration 동안, 나머지(일어나기)를 RecoveryDuration 동안 재생한다.
	 * 그래서 BP에서 DashDuration·RecoveryDuration만 바꿔도 몽타주가 그 시간에 맞춰 끝까지 재생된다. 0이면 MontagePlayRate 고정.
	 * 바뀔 수 있는 값. 몽타주를 바꾸면 새 몽타주의 이동이 끝나는 시각으로 맞춘다.
	 */
	UPROPERTY(EditDefaultsOnly, Category="Dash", meta=(ClampMin=0, Units="s"))
	float MontageMoveEndTime = 0.6f;

	/**
	 * (2026-10-29 사용자 결정) 몽타주에서 몸이 다 일어선 시각(몽타주 기준 초). 그 뒤의 정리 동작은 방향키를 누르면 건너뛴다:
	 * 경직 중 이 시각이 지나고 방향키를 누르고 있으면 경직을 끝내고 바로 출발한다(일어나는 동작은 항상 끝까지 보인다).
	 * 방향키를 안 누르면 몽타주 끝까지 재생한다. 0이면 건너뛰지 않는다.
	 * 다음 구르기도 이 시각부터 받는다(0이면 경직이 시작될 때부터).
	 * MontageStandUpTimes에 없는 몽타주가 이 값을 쓴다(앞 구르기 기준 0.9초). 바뀔 수 있는 값.
	 */
	UPROPERTY(EditDefaultsOnly, Category="Dash", meta=(ClampMin=0, Units="s"))
	float MontageStandUpTime = 0.9f;

	/**
	 * (2026-10-29) 몽타주별 다 일어선 시각(골반 높이가 서 있을 때의 95%에 닿는 시각으로 잼).
	 * 양손 앞 0.9·왼쪽 0.81·오른쪽 0.81·뒤 0.69·오른쪽 뒤 0.72, 한손 앞 0.87·왼쪽 0.78·오른쪽 0.75·뒤 0.69·오른쪽 뒤 0.69. 바뀔 수 있는 값.
	 */
	UPROPERTY(EditDefaultsOnly, Category="Dash")
	TMap<TObjectPtr<UAnimMontage>, float> MontageStandUpTimes;

	/**
	 * 관성 이어가기 각도 (2026-10-09 사용자 결정, 바뀔 수 있는 값). 대시가 끝날 때 대시 방향에서 이 각도 안쪽으로 방향키를 누르고 있으면
	 * 경직 동안 이동을 잠그지 않아, 걷기·달리기 속도 그대로 이어서 움직인다. 방향키를 안 누르거나 이 각도 밖이면 전처럼 미끄러지며 버틴다.
	 * (출발 Easing을 넣은 뒤 '퀵스텝 → 경직에서 거의 정지 → 느린 출발'이 어색해서 넣음. 캔슬 창은 어느 쪽이든 그대로)
	 * 0이면 항상 잠근다.
	 * (2026-10-29 사용자 결정, 구르기) 90 → 0: 방향키와 상관없이 일어나는 동작을 끝까지 보인다(움직이면 일어나며 미끄러져 보임).
	 */
	UPROPERTY(EditDefaultsOnly, Category="Dash", meta=(ClampMin=0, ClampMax=180, Units="Degrees"))
	float CarryMomentumMaxAngle = 0.f;

	/**
	 * 대시 몽타주 재생 속도(고정). 어빌리티가 끝나면(대시 + 경직) 그 지점에서 블렌드 아웃된다.
	 * 1.5배속, 대시 0.35초 + 경직 0.1초면 몽타주(1.33초) 약 51% 지점에서 끝난다.
	 * (2026-10-07: 대시+경직 시간에 맞춘 자동 배속(약 2.4~3배)은 대시 중에 동작이 너무 빨리 지나가서 고정 배속으로 바꿈.
	 *  1.0배속도 시험해 본 뒤 1.5배속으로 정함)
	 * (2026-10-29 사용자 결정, 구르기) 1.5 → 1.0: 구르기를 원래 속도로 끝까지 재생한다(대시 0.6초 + 경직 0.48초 = 1.08초).
	 * (2026-10-29) MontageMoveEndTime이 0보다 크면 이 값은 쓰지 않는다(시간에 맞춘 배속).
	 */
	UPROPERTY(EditDefaultsOnly, Category="Dash", meta=(ClampMin=0.1))
	float MontagePlayRate = 1.f;

	/**
	 * 선택 사항. 대시 동안의 0..1 커브. 거리는 크게 바꾸지 않고 순간 가속 모양만 바꾼다.
	 * 가로축은 대시 이동 시간을 0~1로 본 값이고, 값이 속도 배율이다(평균이 1이면 거리가 DashDistance 그대로).
	 * (2026-10-29) GA_Knight_BoostDash = CF_Knight_Roll_Speed: 0.06초 만에 붙고, 0.33초까지 일정, 일어나며 부드럽게 감속.
	 */
	UPROPERTY(EditDefaultsOnly, Category="Dash")
	TObjectPtr<UCurveFloat> StrengthOverTime;

	/** 대시 시작부터의 회피(무적) 시간. (2026-10-29 사용자 결정, 구르기가 느려진 만큼 0.2 → 0.3, 바뀔 수 있는 값) */
	UPROPERTY(EditDefaultsOnly, Category="Dash", meta=(ClampMin=0, Units="s"))
	float InvulnerableDuration = 0.3f;

	/**
	 * 선택 사항. 대시 자세(정면, 2026-10-29부터 구르기). 몽타주의 루트 모션은 무시하고 루트 모션 소스가 이동을 맡는다.
	 * 그래서 몽타주에 쓰는 시퀀스는 루트 모션을 끄고 루트를 고정한다(Force Root Lock).
	 * 이 4칸(DashMontage~Right)은 양손 스탠스용이고, 한손 스탠스는 아래 OneHanded 칸을 쓴다.
	 */
	UPROPERTY(EditDefaultsOnly, Category="Dash")
	TObjectPtr<UAnimMontage> DashMontage;

	/** 바라보는 방향 기준으로 뒤로 움직일 때의 대시 자세 (락온, 또는 입력 없는 백스텝). DashMontageBackRight가 있으면 왼쪽 뒤용. */
	UPROPERTY(EditDefaultsOnly, Category="Dash|Directional")
	TObjectPtr<UAnimMontage> DashMontageBack;

	/** (2026-10-29) 오른쪽 뒤로 움직일 때의 뒤 구르기(BR: 오른쪽으로 몸을 틀며 뒤로). 비우면 DashMontageBack을 쓴다. */
	UPROPERTY(EditDefaultsOnly, Category="Dash|Directional")
	TObjectPtr<UAnimMontage> DashMontageBackRight;

	UPROPERTY(EditDefaultsOnly, Category="Dash|Directional")
	TObjectPtr<UAnimMontage> DashMontageLeft;

	UPROPERTY(EditDefaultsOnly, Category="Dash|Directional")
	TObjectPtr<UAnimMontage> DashMontageRight;

	/**
	 * (2026-10-29 사용자 결정) 락온 중 대각선 구르기: 누른 방향에 가장 가까운 구르기(앞·왼쪽·오른쪽·뒤)를 고르고,
	 * 그 구르기 방향이 실제 이동 방향과 맞도록 몸을 최대 45° 돌린다. 구르는 동안은 락온 회전을 멈추고, 끝나면 다시 대상을 본다.
	 * 정확한 대각선(45°)은 앞·뒤 구르기로 처리한다. 끄면 몸은 대상을 본 채 가장 가까운 방향 몽타주만 고른다.
	 */
	UPROPERTY(EditDefaultsOnly, Category="Dash|Directional")
	bool bAlignRollToDirectionWhenLockedOn = true;

	/**
	 * (2026-10-29) 한손 스탠스(State.Stance.TwoHanded가 없을 때)의 정면 대시 자세. 비우면 한손에서도 위의 양손 4칸을 쓴다.
	 * 한손 방향 칸(Back·Left·Right)이 비어 있으면 그 방향은 이 칸을 쓴다(양손 칸과 같은 규칙).
	 */
	UPROPERTY(EditDefaultsOnly, Category="Dash|One Handed")
	TObjectPtr<UAnimMontage> OneHandedDashMontage;

	UPROPERTY(EditDefaultsOnly, Category="Dash|One Handed")
	TObjectPtr<UAnimMontage> OneHandedDashMontageBack;

	UPROPERTY(EditDefaultsOnly, Category="Dash|One Handed")
	TObjectPtr<UAnimMontage> OneHandedDashMontageBackRight;

	UPROPERTY(EditDefaultsOnly, Category="Dash|One Handed")
	TObjectPtr<UAnimMontage> OneHandedDashMontageLeft;

	UPROPERTY(EditDefaultsOnly, Category="Dash|One Handed")
	TObjectPtr<UAnimMontage> OneHandedDashMontageRight;

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
	 * 구르기를 시작할 때 등 부스터를 뿜을지. 기본은 끔(발만 켠다). 바뀔 수 있는 값.
	 * 등·발 부스터 노즐은 나이트의 부스터 컴포넌트(BP_Knight → Thrusters)에서 이름으로 고른다.
	 * 둘 다 구르는 반대쪽으로 조금 아래로 짧게 뿜고(구르는 쪽으로 밀어 주는 느낌), 구르기 이동이 끝나면 시간이 남아 있어도 끈다.
	 * 이펙트나 노즐을 비우면 뿜지 않는다.
	 */
	UPROPERTY(EditDefaultsOnly, Category="Dash|Booster", meta=(InlineEditConditionToggle))
	bool bUseRollBackThrust = false;

	/** 등 부스터. 기본: 등 가운데 한 줄(Back_C), 크기 0.9, 0.2초. 바뀔 수 있는 값. */
	UPROPERTY(EditDefaultsOnly, Category="Dash|Booster", meta=(EditCondition="bUseRollBackThrust"))
	FknightThrustSettings RollBackThrust;

	/** 구르기를 시작할 때 두 발 부스터를 뿜을지. 기본은 켬. 바뀔 수 있는 값. */
	UPROPERTY(EditDefaultsOnly, Category="Dash|Booster", meta=(InlineEditConditionToggle))
	bool bUseRollFootThrust = true;

	/** 발 부스터. 기본: 두 발 앞꿈치(Foot_L/R), 크기 0.45(살짝), 0.2초. 바뀔 수 있는 값. */
	UPROPERTY(EditDefaultsOnly, Category="Dash|Booster", meta=(EditCondition="bUseRollFootThrust"))
	FknightThrustSettings RollFootThrust;

	/** 대시 소리의 AI 청각 크기. */
	UPROPERTY(EditDefaultsOnly, Category="Dash|Noise", meta=(ClampMin=0))
	float NoiseLoudness = 1.f;

	/** 대시 소리가 들리는 최대 거리. 0이면 듣는 쪽의 청각 범위를 쓴다. */
	UPROPERTY(EditDefaultsOnly, Category="Dash|Noise", meta=(ClampMin=0, Units="cm"))
	float NoiseRange = 0.f;

	/** 퀵스텝 대시 이동 동안 메카 홀로그램 잔상(애프터이미지)을 남길지 여부. (2026-10-29 사용자 결정, 구르기로 바꾸며 끔) */
	UPROPERTY(EditDefaultsOnly, Category="Dash|Ghost Trail")
	bool bSpawnGhostTrail = false;

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
	/**
	 * 바라보는 방향 기준 대시 방향으로 몽타주를 고른다. OutMontageYaw에는 고른 몽타주가 구르는 방향
	 * (바라보는 방향 기준 각도, 오른쪽 +: 앞 0, 오른쪽 90, 왼쪽 -90, 뒤 ±180)을 돌려준다.
	 */
	UAnimMontage* SelectDashMontage(const ACharacter& Character, const FVector& Direction, float* OutMontageYaw = nullptr) const;

	/** 대시 한 번(방향·몽타주·무적·루트 모션). */
	void StartDash(ACharacter& Character);

	/**
	 * 몽타주 재생 속도. 지금 규칙: MontagePlayRate 고정.
	 * 바뀔 수 있는 항목이다(예: 대시 + 경직 시간에 맞춘 자동 배속, 대시 부분만 맞추기). 바꿀 때는 이 함수만 고치면 된다.
	 */
	float GetMontagePlayRate(const UAnimMontage* Montage) const;

	/** 이동 구간(0 ~ MontageMoveEndTime) 안에서 느리게 재생하는 등이 닿는 구간의 길이(몽타주 기준 초). 느리게 하지 않으면 0. */
	float GetContactSlowLength() const;

	/** 구르는 동안 몽타주 위치를 보고, 등이 닿는 구간에 들어가면 느리게, 나오면 원래 배속으로 바꾼다. */
	void UpdateContactPlayRate();

	/** 메카 속성 값을 읽는다. 해당 속성 세트가 없으면 Fallback. */
	float GetMechAttributeValue(const FGameplayAttribute& Attribute, float Fallback) const;

	UFUNCTION()
	void OnDashFinished();

	UFUNCTION()
	void OnInvulnerabilityEnded();

	UFUNCTION()
	void OnRecoveryFinished();

	/**
	 * 몸이 다 일어선 뒤 경직 동안 매 프레임 부른다. 처음 부를 때 다음 구르기를 받기 시작하고, 기억해 둔 구르기 키가 있으면 바로 다음 구르기로 잇는다.
	 * 그 밖에는 방향키를 누르고 있으면 경직을 끝내고 바로 출발한다.
	 */
	void CheckStandUpMove();

	/** 기억해 둔 구르기 키가 있으면 지우고 다음 구르기(재발동)를 시도한다. 나갔으면 true(이 구르기는 이미 끝나고 새로 시작함). */
	bool TryQueuedRoll();

	/** 경직 상태(Window.Cancel, 이동 잠금)를 푼다. */
	void ReleaseRecoveryState();

	void StartCooldown();
	void RemoveInvulnerability();

	/** 구르기 부스터(등·발)를 끈다. */
	void StopRollThrust();
	void SpawnGhostStep();
	void StopGhostTrail();

	/** 지금 뿜고 있는 구르기 등·발 부스터(부스터 컴포넌트 번호). 0이면 없음. */
	int32 RollBackThrustId = 0;
	int32 RollFootThrustId = 0;

	FTimerHandle GhostTrailTimer;
	FTimerHandle StandUpMoveTimer;
	FTimerHandle ContactRateTimer;

	/** 이동 구간의 기본 배속(등이 닿는 구간 밖). */
	float DashBasePlayRate = 1.f;

	/** 지금 등이 닿는 구간의 느린 배속을 걸어 두었는지. */
	bool bContactSlowApplied = false;

	/** 몸이 다 일어선 시점이 지났는지. 이때부터 다음 구르기를 받는다. */
	bool bStoodUp = false;

	/** 구르는 도중 누른 구르기 키가 있는지(bQueueRollDuringRoll). */
	bool bRollQueued = false;

	bool bInvulnerableApplied = false;
	bool bCancelWindowApplied = false;
	bool bMoveLockApplied = false;
	bool bInRecovery = false;

	/**
	 * (2026-10-29) 구르는 동안 방향키로 몸이 돌지 않게(비락온 이동 방향 회전·락온 회전 모두) 건 이동 잠금.
	 * 락온 여부와 상관없이 구르기 시작부터 건다. 어빌리티가 끝날 때 푼다.
	 */
	bool bFacingLockApplied = false;

	/** 지금 재생 중인 대시 몽타주. 경직이 시작될 때 일어나는 동작의 배속을 바꾸는 데 쓴다. */
	TWeakObjectPtr<UAnimMontage> PlayingDashMontage;

	/** 이번 대시의 방향(수평 단위 벡터). 경직에서 관성 이어가기 여부를 정할 때 쓴다. */
	FVector DashDirection = FVector::ZeroVector;

	/** 이 월드 시각(초)이 지나기 전에는 다시 발동할 수 없다. 어빌리티가 액터별 인스턴스라 액터마다 따로 기록된다. */
	float CooldownEndTime = 0.f;

	/** 대시를 시작했지만 아직 쿨타임을 걸지 않았는지. 이동이 끝나거나, 그 전에 어빌리티가 끊기면 건다. */
	bool bCooldownPending = false;
};
