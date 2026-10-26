#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Combat/knightCombatTypes.h"
#include "Engine/EngineTypes.h"
#include "knightMeleeTraceComponent.generated.h"

class UMeshComponent;
class UNiagaraComponent;
class UNiagaraSystem;

DECLARE_MULTICAST_DELEGATE_OneParam(FknightMeleeHitEvent, const FHitResult& /*Hit*/);

/**
 * Sweeps the weapon between frames while a UknightAnimNotifyState_MeleeTrace is active.
 *
 * Each frame, several points along the blade are swept from last frame's position to this frame's,
 * so fast swings do not tunnel. Overlapping windows (both claws at once) each sweep their own shape and
 * share one hit list, so each actor is hit at most once until the last overlapping window ends. Valid hits
 * (ICombatInterface::CanReceiveHit and hostile team) are sent to the owner's ASC as Event.Melee.Hit
 * with the hit result as target data; the running attack ability turns them into damage.
 *
 * Debug: knight.Combat.DrawMeleeTraces 1
 */
UCLASS(ClassGroup=(Knight), meta=(BlueprintSpawnableComponent))
class KNIGHT_API UknightMeleeTraceComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UknightMeleeTraceComponent();

	void BeginTrace(const FknightMeleeTraceParams& Params);
	/** Ends the window that began with the same shape. */
	void EndTrace(const FknightMeleeTraceParams& Params);

	/**
	 * One overlap of a sphere (UknightAnimNotify_MeleeArea), sent through the same Event.Melee.Hit path as the sweeps.
	 * If a trace window is open at the same time, its hit list is shared so an actor is still hit only once.
	 */
	void HitArea(const FVector& Center, float Radius);

	UFUNCTION(BlueprintPure, Category="Knight|Combat")
	bool IsTracing() const { return !ActiveTraces.IsEmpty(); }

	/** Native listeners (debugger, tests). Gameplay reacts to Event.Melee.Hit instead. */
	FknightMeleeHitEvent OnMeleeHit;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Object types the weapon can hit. Monster part bodies are usually PhysicsBody or Pawn. */
	UPROPERTY(EditDefaultsOnly, Category="Knight|Combat")
	TArray<TEnumAsByte<EObjectTypeQuery>> HitObjectTypes;

	/**
	 * (2026-10-22) 휘두를 때 칼날을 따라 남는 칼자국(리본 트레일). 판정 구간(Knight Melee Trace)마다 하나씩 켜고 끝나면 끈다.
	 * 판정 소켓 두 개(StartSocket~EndSocket) 가운데에 붙이고, 이펙트의 로컬 Z축을 칼날 방향으로 맞춘다.
	 * SlashHitVFX의 NS_Trail_* 처럼 User.Width(리본 폭), User.AxisOffset(로컬 -Z)을 쓰는 이펙트용. 비우면 없음(몬스터 기본값).
	 */
	UPROPERTY(EditDefaultsOnly, Category="Knight|Combat|Swing Trail")
	TObjectPtr<UNiagaraSystem> SwingTrail;

	/** 리본 폭 = 두 소켓 사이 길이 x 이 값. 칼끝·손잡이까지 덮거나 줄일 때. */
	UPROPERTY(EditDefaultsOnly, Category="Knight|Combat|Swing Trail", meta=(ClampMin=0.1, EditCondition="SwingTrail"))
	float SwingTrailWidthScale = 1.f;

	/** 칼날 방향으로 이펙트 중심을 옮기는 거리(cm). +면 칼끝 쪽. */
	UPROPERTY(EditDefaultsOnly, Category="Knight|Combat|Swing Trail", meta=(EditCondition="SwingTrail", Units="cm"))
	float SwingTrailCenterOffset = 0.f;

	/** 0보다 크면 이펙트의 User.Life(꼬리가 남는 시간, 초)를 덮어쓴다. */
	UPROPERTY(EditDefaultsOnly, Category="Knight|Combat|Swing Trail", meta=(ClampMin=0, Units="s", EditCondition="SwingTrail"))
	float SwingTrailLife = 0.f;

	/** 켜면 아래 색으로 이펙트 색(User.ColorHead/ColorTail/ColorDark)을 덮어쓴다. 원본은 파란 판타지 색이라 기본으로 켠다. */
	UPROPERTY(EditDefaultsOnly, Category="Knight|Combat|Swing Trail", meta=(EditCondition="SwingTrail"))
	bool bOverrideSwingTrailColor = true;

	/** 칼날 바로 뒤(밝은 쪽). 금속 칼날 느낌의 옅은 흰색. HDR 값이라 1보다 크면 빛난다. */
	UPROPERTY(EditDefaultsOnly, Category="Knight|Combat|Swing Trail", meta=(EditCondition="SwingTrail && bOverrideSwingTrailColor"))
	FLinearColor SwingTrailColorHead = FLinearColor(1.2f, 1.15f, 1.05f, 1.f);

	/** 꼬리 쪽(사라지는 쪽). */
	UPROPERTY(EditDefaultsOnly, Category="Knight|Combat|Swing Trail", meta=(EditCondition="SwingTrail && bOverrideSwingTrailColor"))
	FLinearColor SwingTrailColorTail = FLinearColor(0.25f, 0.25f, 0.27f, 1.f);

	/** 이펙트의 점광원(User.LightOn). 끄면 주변을 비추지 않는다. */
	UPROPERTY(EditDefaultsOnly, Category="Knight|Combat|Swing Trail", meta=(EditCondition="SwingTrail"))
	bool bSwingTrailLight = false;

	/** 켜면 스탠스(한손 / 양손 State.Stance.TwoHanded)에 따라 궤적 색·두께·광원을 자동으로 다르게 연출한다. */
	UPROPERTY(EditDefaultsOnly, Category="Knight|Combat|Swing Trail", meta=(EditCondition="SwingTrail"))
	bool bUseStanceTrailVariation = true;

	/** 한손 스탠스일 때 궤적 폭 배율. 날렵하고 예리한 청백색 궤적. */
	UPROPERTY(EditDefaultsOnly, Category="Knight|Combat|Swing Trail", meta=(ClampMin=0.1, EditCondition="SwingTrail && bUseStanceTrailVariation"))
	float OneHandedTrailWidthMultiplier = 0.95f;

	UPROPERTY(EditDefaultsOnly, Category="Knight|Combat|Swing Trail", meta=(EditCondition="SwingTrail && bUseStanceTrailVariation"))
	FLinearColor OneHandedTrailColorHead = FLinearColor(0.35f, 2.2f, 4.5f, 1.f);

	UPROPERTY(EditDefaultsOnly, Category="Knight|Combat|Swing Trail", meta=(EditCondition="SwingTrail && bUseStanceTrailVariation"))
	FLinearColor OneHandedTrailColorTail = FLinearColor(0.05f, 0.3f, 1.2f, 1.f);

	/** 양손 스탠스일 때 덮어쓸 나이아가라 에셋(비우면 기본 SwingTrail 사용). */
	UPROPERTY(EditDefaultsOnly, Category="Knight|Combat|Swing Trail", meta=(EditCondition="SwingTrail && bUseStanceTrailVariation"))
	TObjectPtr<UNiagaraSystem> TwoHandedSwingTrail;

	/** 양손 스탠스일 때 궤적 폭 배율. 넓고 묵직한 진홍/화염 궤적. */
	UPROPERTY(EditDefaultsOnly, Category="Knight|Combat|Swing Trail", meta=(ClampMin=0.1, EditCondition="SwingTrail && bUseStanceTrailVariation"))
	float TwoHandedTrailWidthMultiplier = 1.45f;

	UPROPERTY(EditDefaultsOnly, Category="Knight|Combat|Swing Trail", meta=(EditCondition="SwingTrail && bUseStanceTrailVariation"))
	FLinearColor TwoHandedTrailColorHead = FLinearColor(4.5f, 0.25f, 0.08f, 1.f);

	UPROPERTY(EditDefaultsOnly, Category="Knight|Combat|Swing Trail", meta=(EditCondition="SwingTrail && bUseStanceTrailVariation"))
	FLinearColor TwoHandedTrailColorTail = FLinearColor(1.2f, 0.02f, 0.0f, 1.f);

	/** 양손 스탠스일 때 궤적 점광원 활성화 여부. */
	UPROPERTY(EditDefaultsOnly, Category="Knight|Combat|Swing Trail", meta=(EditCondition="SwingTrail && bUseStanceTrailVariation"))
	bool bTwoHandedTrailLight = true;

	/** 검을 휘두를 때 대기를 가르는 아지랑이(굴절) 트레일을 함께 켤지 여부. */
	UPROPERTY(EditDefaultsOnly, Category="Knight|Combat|Swing Trail", meta=(EditCondition="SwingTrail"))
	bool bEnableRefractionTrail = true;

	/** 대기 굴절 트레일 나이아가라 에셋 (기본: NS_Trail_refraction). */
	UPROPERTY(EditDefaultsOnly, Category="Knight|Combat|Swing Trail", meta=(EditCondition="SwingTrail && bEnableRefractionTrail"))
	TObjectPtr<UNiagaraSystem> RefractionTrail;

private:
	struct FActiveTrace
	{
		FknightMeleeTraceParams Params;
		TWeakObjectPtr<UMeshComponent> Mesh;
		TArray<FVector> PreviousPoints;
		/** (2026-10-22) 이 판정 구간의 칼자국. */
		TWeakObjectPtr<UNiagaraComponent> TrailFX;
		/** 대기 굴절(아지랑이) 트레일. */
		TWeakObjectPtr<UNiagaraComponent> RefractionFX;
	};

	void StartSwingTrail(FActiveTrace& Trace) const;
	static void StopSwingTrail(FActiveTrace& Trace);

	UMeshComponent* FindTraceMesh(FName ComponentTag) const;
	static void SampleBlade(const FActiveTrace& Trace, TArray<FVector>& OutPoints);
	void SweepFrame();
	void HandleHit(const FHitResult& Hit);
	bool IsValidTarget(AActor* Target) const;
	void StopAllTraces();

	TArray<FActiveTrace> ActiveTraces;
	/** Shared by overlapping windows; cleared when the last one ends. */
	TSet<TWeakObjectPtr<AActor>> HitActors;
};
