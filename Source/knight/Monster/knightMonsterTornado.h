#pragma once

#include "CoreMinimal.h"
#include "Combat/knightCombatTypes.h"
#include "GameFramework/Actor.h"
#include "knightMonsterTornado.generated.h"

class UAudioComponent;
class UCameraShakeBase;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UNiagaraComponent;
class UNiagaraSystem;
class USoundBase;
class UStaticMesh;
class UStaticMeshComponent;

/** 회오리 하나의 크기·움직임. 몬스터 어빌리티가 정해 AknightMonsterTornado::Launch에 넘긴다. */
USTRUCT(BlueprintType)
struct KNIGHT_API FknightMonsterTornadoParams
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tornado", meta=(ClampMin=1, Units="cm"))
	float Radius = 300.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tornado", meta=(ClampMin=1, Units="cm"))
	float Height = 1200.f;

	/**
	 * 생긴 자리에서 다 만들어질 때까지. 그동안 움직이지 않는다. 깔때기가 있으면 수면 소용돌이(FormEffect) 뒤 RiseStart부터 기둥이
	 * 아래에서 위로 차오르고(판정 높이도 같이), 없으면 작은 크기에서 커진다. 다 만들어지는 순간 BurstEffect가 터진다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tornado", meta=(ClampMin=0, Units="s"))
	float GrowTime = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tornado", meta=(ClampMin=0, Units="s"))
	float ChaseTime = 5.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tornado", meta=(ClampMin=0, Units="cm/s"))
	float Speed = 800.f;

	/** 타깃 쪽으로 진행 방향을 꺾는 속도. 작을수록 옆으로 피하기 쉽다. 0이면 처음 방향으로 일직선으로 지나간다.
	 * 잔향 20: 달리기만으로는 따라잡히고, 달리다 퀵 대시를 섞으면 피한다(25면 대시로도 판정 경계에 걸린다). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tornado", meta=(ClampMin=0, Units="deg"))
	float TurnRate = 90.f;

	/** 쫓기가 끝난 뒤 사라지는 시간. 판정은 없다. 깔때기가 있으면 기둥이 아래부터 풀려 올라가며 물이 쏟아지고(CollapseEffect), 없으면 줄어든다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tornado", meta=(ClampMin=0, Units="s"))
	float FadeTime = 0.5f;

	/** 밑동(수면 또는 바닥)에 붙는 입자. 커지는 크기를 따르고 EffectSpinRate로 돈다. 깔때기도 이것도 없으면 거꾸로 선 기본 원뿔이 보인다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tornado|Visual")
	TObjectPtr<UNiagaraSystem> Effect;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tornado|Visual", meta=(Units="deg"))
	float EffectSpinRate = 200.f;

	/** 깔때기 메시. 피벗이 아래 가운데이고 +Z로 100cm, 반지름 100cm가 판정 반지름이 되게 만든다(Radius·Height에 맞춰 늘린다). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tornado|Visual")
	TObjectPtr<UStaticMesh> FunnelMesh;

	/** 스칼라 파라미터 Fade(0~1)로 나타나고 사라진다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tornado|Visual")
	TObjectPtr<UMaterialInterface> FunnelMaterial;

	/** 깔때기 바깥을 나선으로 빠르게 감아 오르는 물줄기. 깔때기 메시를 StreamScale배 굵게 겹쳐 그린다. 비우면 없음. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tornado|Visual")
	TObjectPtr<UMaterialInterface> StreamMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tornado|Visual", meta=(ClampMin=1))
	float StreamScale = 1.05f;

	/** 만들어지는 동안 밑동 수면에서 도는 물보라 고리와 빨려 드는 물안개. 다 만들어지면 끈다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tornado|Form")
	TObjectPtr<UNiagaraSystem> FormEffect;

	/** 만들어지는 시간(GrowTime) 중 기둥이 차오르기 시작하는 비율. 그 전은 수면 소용돌이만 보인다. 이때 Effect도 켠다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tornado|Form", meta=(ClampMin=0, ClampMax=0.95))
	float RiseStart = 0.3f;

	/** 다 만들어지는 순간 밑동에서 한 번 터지는 물. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tornado|Form")
	TObjectPtr<UNiagaraSystem> BurstEffect;

	/** 다 만들어지는 순간 수면을 크게 한 번 누른다. 0이면 없음. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tornado|Form", meta=(ClampMin=0, Units="cm"))
	float BurstRippleRadius = 600.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tornado|Form", meta=(ClampMin=0, Units="cm"))
	float BurstRippleDepth = 25.f;

	/** 다 만들어지는 순간 가까운 플레이어 화면을 흔든다. BurstShakeInnerRadius 안은 그대로, OuterRadius까지 줄어든다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tornado|Form")
	TSubclassOf<UCameraShakeBase> BurstShake;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tornado|Form", meta=(ClampMin=0))
	float BurstShakeScale = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tornado|Form", meta=(ClampMin=0, Units="cm"))
	float BurstShakeInnerRadius = 1500.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tornado|Form", meta=(ClampMin=0, Units="cm"))
	float BurstShakeOuterRadius = 4000.f;

	/** 사라지기 시작할 때 기둥에서 쏟아져 내리는 물(한 번). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tornado|Visual")
	TObjectPtr<UNiagaraSystem> CollapseEffect;

	/** 깔때기 아래 끝을 밑동보다 이만큼 낮춰 수면·바닥에서 솟은 것처럼 보이게 한다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tornado|Visual", meta=(ClampMin=0, Units="cm"))
	float FunnelSink = 40.f;

	/** 수면 위를 이 간격마다 눌러 지나간 자리에 골을 판다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tornado|Water", meta=(ClampMin=1, Units="cm"))
	float WaterRippleSpacing = 50.f;

	/** 0이면 수면을 누르지 않는다. 커지는 크기를 따른다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tornado|Water", meta=(ClampMin=0, Units="cm"))
	float WaterRippleRadius = 250.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tornado|Water", meta=(ClampMin=0, Units="cm"))
	float WaterRippleDepth = 8.f;

	/** 제자리에서 커지는 동안 이 간격마다 밑동 수면을 휘저어 물결 고리를 낸다. 0이면 끈다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tornado|Water", meta=(ClampMin=0, Units="s"))
	float WaterChurnInterval = 0.1f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tornado|Water", meta=(ClampMin=0, Units="cm"))
	float WaterChurnDepth = 4.f;

	/** 수면 위에 있는 동안(과 그 뒤 2초) 물결 높이 상한. 0이면 물결 설정의 MaxHeight 그대로. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tornado|Water", meta=(ClampMin=0, Units="cm"))
	float WaterMaxHeight = 30.f;

	/** 지나간 자리에 남기는 물보라. 비우면 물결 설정의 SplashHeavySystem. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tornado|Water")
	TObjectPtr<UNiagaraSystem> WaterSplash;

	/** 이만큼 갈 때마다 물보라를 하나 낸다. 0이면 물보라 없음. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tornado|Water", meta=(ClampMin=0, Units="cm"))
	float WaterSplashSpacing = 600.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tornado|Water", meta=(ClampMin=0))
	float WaterSplashScale = 0.6f;

	/** 생기는 순간 밑동에서 한 번 재생한다(소용돌이가 일어 기둥이 차오르는 소리). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tornado|Sound")
	TObjectPtr<USoundBase> SpawnSound;

	/** 다 만들어져 쫓기 시작할 때부터 회오리를 따라 반복 재생하고, 사라지는 동안(FadeTime) 줄인다. 반복(Looping) 소리여야 한다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tornado|Sound")
	TObjectPtr<USoundBase> LoopSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tornado|Sound", meta=(ClampMin=0, Units="s"))
	float LoopFadeIn = 0.3f;
};

/**
 * 몬스터가 바닥에 만드는 회오리(잔향 공중 4). 생긴 자리에서 GrowTime 동안 커진 뒤 ChaseTime 동안 Speed로 바닥을 따라 타깃을 쫓고,
 * FadeTime 동안 줄어들며 사라진다. 원기둥(Radius·Height) 안에 들어온 적대 대상을 근접 타격과 같은 규칙(UknightCombatLibrary::ApplyMeleeHit)으로
 * 때린다. 맞은 대상은 이 회오리에 다시 맞지 않는다. 회피(무적)로 피한 대상은 무적이 끝난 뒤 아직 안에 있으면 맞는다.
 * 막을 수 없게 하려면 가드 범위가 0인 피해 실행(UknightDamageExecution::GuardArcDegrees)을 쓴다.
 * 공격한 몬스터가 죽으면 바로 사라지기 시작한다.
 * 보이는 부분은 깔때기 메시(FunnelMesh)와 밑동 입자(Effect). 수면 위에서는 밑동이 수면에 붙고, 커지는 동안 수면을 휘젓고,
 * 지나간 자리에 골과 물보라를 남긴다.
 * 디버그: knight.Monster.DrawAttacks 1 이면 판정 원기둥을 그린다.
 */
UCLASS()
class KNIGHT_API AknightMonsterTornado : public AActor
{
	GENERATED_BODY()

public:
	AknightMonsterTornado();

	/** Direction은 쫓기 시작할 때의 진행 방향(수평). */
	void Launch(AActor* InAttacker, AActor* InTarget, const FknightMeleeHitParams& InHitParams, const FknightMonsterTornadoParams& InParams,
		const FVector& Direction);

	virtual void Tick(float DeltaSeconds) override;

private:
	/** 판정 원기둥 반지름·높이 비율. */
	void ApplyHits(float RadiusScale, float HeightScale);
	/**
	 * Scale은 깔때기가 없을 때 원뿔·입자 크기. Rise는 기둥이 차오른 높이 비율, Lift는 아래부터 풀려 사라진 높이 비율,
	 * Opacity는 전체 짙기(머티리얼 파라미터 Rise, Lift, Fade).
	 */
	void UpdateVisual(float Scale, float Rise, float Lift, float Opacity, float DeltaSeconds);
	/** 다 만들어지는 순간: 물 터짐, 큰 물결, 화면 흔들림. */
	void Burst();
	/** 사라지기 시작할 때: 입자 생성을 멈추고 쏟아지는 물을 낸다. */
	void StartCollapse();
	UNiagaraComponent* SpawnAtBase(UNiagaraSystem* System, bool bAttached);
	/** 밑동 아래 수면을 찾아 BaseOffset을 맞추고, 수면이면 휘젓기·골·물보라를 낸다. */
	void UpdateWater(float Scale, bool bMoving, float DeltaSeconds);
	/** 발밑 바닥 높이로 붙인다. 바닥을 못 찾으면 그대로 둔다. */
	void SnapToFloor(FVector& Location) const;

	UPROPERTY(VisibleAnywhere, Category="Tornado")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, Category="Tornado")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere, Category="Tornado")
	TObjectPtr<UNiagaraComponent> EffectComponent;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> FunnelMaterialInstance;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> StreamMesh;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> StreamMaterialInstance;

	UPROPERTY(Transient)
	TObjectPtr<UNiagaraComponent> FormComponent;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> LoopAudio;

	TWeakObjectPtr<AActor> Attacker;
	TWeakObjectPtr<AActor> Target;
	FknightMeleeHitParams HitParams;
	FknightMonsterTornadoParams Params;
	TSet<TWeakObjectPtr<AActor>> HitActors;
	FVector MoveDirection = FVector::ForwardVector;
	float Age = 0.f;
	/** 사라지기 시작한 나이. 음수면 아직 쫓는 중이다. */
	float FadeStartAge = -1.f;
	float Spin = 0.f;
	float EffectSpin = 0.f;
	/** 액터 위치(바닥)에서 밑동(수면 또는 바닥)까지의 높이. */
	float BaseOffset = 0.f;
	/** 수면 위를 지난 거리(골·물보라 간격용)와 마지막 휘젓기 뒤 지난 시간. */
	float WaterTravel = 0.f;
	float WaterSplashTravel = 0.f;
	float ChurnTime = 0.f;
	FVector LastWaterPoint = FVector::ZeroVector;
	bool bOverWater = false;
	bool bFunnel = false;
	bool bBurst = false;
};
