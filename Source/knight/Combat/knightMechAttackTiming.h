// (2026-10-20) 기계식 공격 타이밍: 사람처럼 부드럽게 가속·감속하는 공격을 '준비에서 뚝 멈춤(락) → 모터가 확 돌며 휘두름(버스트)
// → 판정 끝에서 뚝 멈춤(스톱)' 으로 바꾼다. 애니메이션은 그대로 두고 공격 몽타주의 재생 속도만 구간별로 바꾼다.
//
// 구간은 몽타주의 첫 근접 판정(Knight Melee Trace)에서 자동으로 잡는다. 몽타주마다 노티파이를 따로 넣지 않는다.
//   준비(Windup)  : 몽타주 시작 ~ 판정 LockLeadTime 전      → 배속 x WindupRate (조금 느리게, 무게감)
//   락(Lock)      : 위 지점에서 LockTime(실제 초) 동안       → 배속 x LockRate (거의 정지, 모터가 힘을 모음)
//   버스트(Burst) : 락이 끝난 뒤 ~ 판정 끝                    → 배속 x BurstRate (빠르게) + 추진기 분사·작은 흔들림
//   스톱(Stop)    : 판정 끝에서 StopTime(실제 초) 동안        → 배속 x StopRate (뚝 멈춤) + 증기 배출
//   후딜(Recovery): 그 뒤                                     → 원래 배속
// 원래 배속(어빌리티가 정한 PlayRate x 공격 배속 x 스탠스)을 기준으로 곱하므로 스탠스·공격 배속 설정과 같이 쓴다.
// 판정·콤보·캔슬 창은 몽타주 노티파이라 배속을 따라 함께 움직인다.
//
// 조절: BP_Knight → AbilitySystemComponent → Knight|Attack Feel → MechTiming. 비교: 콘솔 knight.Player.MechTiming 0/1

#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Templates/SubclassOf.h"
#include "knightMechAttackTiming.generated.h"

class UAnimInstance;
class UAnimMontage;
class UCameraShakeBase;
class UNiagaraComponent;
class UNiagaraSystem;
class USoundBase;
class USkeletalMeshComponent;

USTRUCT(BlueprintType)
struct KNIGHT_API FknightMechTimingSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category="Mech Timing")
	bool bEnabled = true;

	/** 준비 동작 배속(원래 배속에 곱함). 1보다 작으면 무겁게. */
	UPROPERTY(EditAnywhere, Category="Mech Timing", meta=(ClampMin=0.1, ClampMax=2))
	float WindupRate = 0.9f;

	/** 첫 판정 몇 초 전(몽타주 시간)에 락을 거는지. 칼이 가장 뒤로 젖혀진 지점 근처. */
	UPROPERTY(EditAnywhere, Category="Mech Timing", meta=(ClampMin=0, Units="s"))
	float LockLeadTime = 0.12f;

	/** 락 길이(실제 초). 한손 기준, 양손은 TwoHandedLockScale을 곱한다. */
	UPROPERTY(EditAnywhere, Category="Mech Timing", meta=(ClampMin=0, Units="s"))
	float LockTime = 0.07f;

	UPROPERTY(EditAnywhere, Category="Mech Timing", meta=(ClampMin=0.01, ClampMax=1))
	float LockRate = 0.06f;

	/** 휘두르는 구간 배속. 한손 기준, 양손은 TwoHandedBurstScale을 곱한다. */
	UPROPERTY(EditAnywhere, Category="Mech Timing", meta=(ClampMin=0.5, ClampMax=3))
	float BurstRate = 1.45f;

	/** 판정 끝 '뚝 멈춤' 길이(실제 초)와 배속. */
	UPROPERTY(EditAnywhere, Category="Mech Timing", meta=(ClampMin=0, Units="s"))
	float StopTime = 0.06f;

	UPROPERTY(EditAnywhere, Category="Mech Timing", meta=(ClampMin=0.01, ClampMax=1))
	float StopRate = 0.12f;

	/** 양손 스탠스: 락을 더 길게(더 무겁게), 휘두르기는 조금 덜 빠르게. */
	UPROPERTY(EditAnywhere, Category="Mech Timing", meta=(ClampMin=0.1, ClampMax=4))
	float TwoHandedLockScale = 1.5f;

	UPROPERTY(EditAnywhere, Category="Mech Timing", meta=(ClampMin=0.1, ClampMax=2))
	float TwoHandedBurstScale = 0.92f;

	/** 버스트 순간 추진기 분사. 비우면 없음. 예: NS_Knight_BackBoost. */
	UPROPERTY(EditAnywhere, Category="Mech Timing|Effects")
	TObjectPtr<UNiagaraSystem> BurstEffect;

	/**
	 * 분사를 붙일 뼈·소켓(한손). (2026-10-20 사용자 제안) 팔을 움직여 칼을 휘두르니 등이 아니라 팔꿈치(lowerarm_r 시작점).
	 * 비우면 BurstEffect를 쓰지 않는다.
	 */
	UPROPERTY(EditAnywhere, Category="Mech Timing|Effects")
	TArray<FName> BurstSockets = { TEXT("lowerarm_r") };

	/** 양손 스탠스 분사 위치(두 팔꿈치). 비우면 BurstSockets를 쓴다. */
	UPROPERTY(EditAnywhere, Category="Mech Timing|Effects")
	TArray<FName> TwoHandedBurstSockets = { TEXT("lowerarm_r"), TEXT("lowerarm_l") };

	UPROPERTY(EditAnywhere, Category="Mech Timing|Effects", meta=(ClampMin=0))
	float BurstEffectScale = 0.35f;

	/** 버스트 순간 작은 카메라 흔들림(모터 킥). 비우면 없음. */
	UPROPERTY(EditAnywhere, Category="Mech Timing|Effects")
	TSubclassOf<UCameraShakeBase> BurstShake;

	UPROPERTY(EditAnywhere, Category="Mech Timing|Effects", meta=(ClampMin=0))
	float BurstShakeScale = 0.25f;

	/** 판정 끝에서 관절 증기 배출. 비우면 없음. */
	UPROPERTY(EditAnywhere, Category="Mech Timing|Effects")
	TObjectPtr<UNiagaraSystem> StopEffect;

	UPROPERTY(EditAnywhere, Category="Mech Timing|Effects")
	TArray<FName> StopSockets = { TEXT("upperarm_r"), TEXT("upperarm_l") };

	UPROPERTY(EditAnywhere, Category="Mech Timing|Effects", meta=(ClampMin=0))
	float StopEffectScale = 0.5f;

	/** 소리(서보 모음 / 모터 휘두름 / 유압 정지). 비우면 없음. VARCO 사운드가 나오면 연결. */
	UPROPERTY(EditAnywhere, Category="Mech Timing|Sounds")
	TObjectPtr<USoundBase> LockSound;

	UPROPERTY(EditAnywhere, Category="Mech Timing|Sounds")
	TObjectPtr<USoundBase> BurstSound;

	UPROPERTY(EditAnywhere, Category="Mech Timing|Sounds")
	TObjectPtr<USoundBase> StopSound;
};

/**
 * 한 번의 공격에 대한 기계식 타이밍 실행기. 공격 어빌리티가 몽타주를 재생한 직후 Start로 만들고, 끝날 때 Stop한다.
 * 실제 시간 티커로 돌며 몽타주 재생 속도를 구간별로 바꾼다. 다른 몽타주로 바뀌었으면 손대지 않고 끝난다.
 */
class KNIGHT_API FknightMechAttackTiming : public TSharedFromThis<FknightMechAttackTiming>
{
public:
	/** 몽타주에 근접 판정이 없거나 꺼져 있으면 nullptr. */
	static TSharedPtr<FknightMechAttackTiming> Start(UAnimInstance* AnimInstance, UAnimMontage* Montage, float BaseRate,
		const FknightMechTimingSettings& Settings, bool bTwoHanded);

	~FknightMechAttackTiming();

	/** 재생 속도를 원래대로 돌리고(아직 같은 몽타주를 재생 중이면) 끝낸다. */
	void Stop();

private:
	enum class EPhase : uint8 { Windup, Lock, Burst, StopHold, Done };

	bool Tick(float DeltaRealTime);
	void SetRate(float Factor) const;
	void EnterPhase(EPhase NewPhase);
	/** 붙인 이펙트를 OutSpawned에 담는다(반복 이펙트도 있어서 끝낼 때 꺼야 한다). */
	void SpawnEffects(UNiagaraSystem* Effect, const TArray<FName>& Sockets, float Scale, TArray<TWeakObjectPtr<UNiagaraComponent>>& OutSpawned) const;
	/** (2026-10-20) 버스트 분사 끄기. NS_Knight_BackBoost 같은 추진기 이펙트는 반복이라 안 끄면 등에 계속 남았다. */
	void StopEffects(TArray<TWeakObjectPtr<UNiagaraComponent>>& Spawned) const;
	void PlaySound(USoundBase* Sound) const;
	bool IsStillPlaying() const;

	TWeakObjectPtr<UAnimInstance> Anim;
	TWeakObjectPtr<UAnimMontage> Montage;
	TWeakObjectPtr<USkeletalMeshComponent> Mesh;
	TArray<TWeakObjectPtr<UNiagaraComponent>> BurstFx;
	TArray<TWeakObjectPtr<UNiagaraComponent>> StopFx;
	FknightMechTimingSettings Settings;
	FTSTicker::FDelegateHandle Ticker;
	float BaseRate = 1.f;
	float LockStart = 0.f;
	float TraceStart = 0.f;
	float TraceEnd = 0.f;
	float LockTime = 0.f;
	float BurstRate = 1.f;
	float PhaseElapsed = 0.f;
	bool bTwoHandedStance = false;
	EPhase Phase = EPhase::Windup;
};
