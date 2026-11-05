#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "NativeGameplayTags.h"
#include "Templates/SubclassOf.h"
#include "knightCombatTypes.generated.h"

class UGameplayEffect;

namespace knightCombat
{
	/**
	 * (2026-11-04) bKnockdown 공격에 그냥 맞았을 때(가드·패링·회피가 아님) 맞은 쪽에 보내는 이벤트.
	 * 플레이어 피격(GA_Knight_HitReact)은 이걸 받으면 그로기와 상관없이 다운된다.
	 */
	KNIGHT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_KnockedDown);
}

/** Weapon trace shape for one swing. Sampled along the line between two sockets. */
USTRUCT(BlueprintType)
struct KNIGHT_API FknightMeleeTraceParams
{
	GENERATED_BODY()

	/** Component tag of the mesh that owns the sockets. None uses the character's main mesh. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Trace")
	FName MeshComponentTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Trace")
	FName StartSocket = TEXT("weapon_start");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Trace")
	FName EndSocket = TEXT("weapon_end");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Trace", meta=(ClampMin=1, Units="cm"))
	float Radius = 30.f;

	/** Points along the blade that are swept each frame. More points catch thin targets on long blades. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Trace", meta=(ClampMin=2, ClampMax=16))
	int32 BladeSamples = 4;

	/**
	 * 0보다 크면 이 판정 구간의 칼자국(SwingTrail) 꼬리가 남는 시간(초)을 이 값으로 덮어쓴다. 0이면 판정 컴포넌트의 SwingTrailLife(또는 이펙트 기본값).
	 * 대쉬공격처럼 크게 미끄러지며 찌르는 공격은 꼬리가 제자리에 길게 남아 칼을 따라오지 않는 것처럼 보여서 짧게 준다. 바뀔 수 있는 값.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Trace", meta=(ClampMin=0, Units="s"))
	float SwingTrailLife = 0.f;
};

/** How one resolved melee hit turned out. */
UENUM(BlueprintType)
enum class EknightHitOutcome : uint8
{
	/** Target cannot be hit (dead, friendly, no ASC). */
	Ignored,
	/** Target was invulnerable (evasion frames). */
	Evaded,
	Parried,
	Guarded,
	Hit
};

/** Per-attack damage values. Attack abilities fill this from their defaults. */
USTRUCT(BlueprintType)
struct KNIGHT_API FknightMeleeHitParams
{
	GENERATED_BODY()

	/** Instant effect whose execution is UknightDamageExecution (or a Blueprint subclass). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Damage")
	TSubclassOf<UGameplayEffect> DamageEffect;

	/** Flat damage added before the attack power scaling. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Damage")
	float BaseDamage = 0.f;

	/** Percentage of attack power this attack deals (Monster Hunter style motion value). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Damage")
	float MotionValue = 30.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Damage")
	float PoiseDamage = 10.f;

	/**
	 * (2026-11-04) 넘어뜨리는 공격(몸통 낙하 등). 켜면 그냥 맞았을 때(가드·패링·회피가 아님) 맞은 쪽에 Event.KnockedDown을 보내
	 * 그로기와 상관없이 다운시킨다. 지금은 플레이어 피격만 이 이벤트를 받는다. 기본은 꺼짐(지금과 같음).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Damage")
	bool bKnockdown = false;

	/** False for attacks too big to deflect (breath, slams, body checks). A parry attempt then counts as a guard. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Defense")
	bool bCanBeParried = true;

	/**
	 * 몬스터 공격용. false면 퍼펙트 가드당해도 공격한 몬스터의 경직도가 깎이지 않는다(패링 보상만 있는 원거리 공격).
	 * Event.Parried에 Event.Parried.NoPoise 태그(InstigatorTags)로 실려 간다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Defense", meta=(EditCondition="bCanBeParried"))
	bool bParryBuildsPoise = true;

	/** Part multipliers at or above this play the weak point cue instead of the normal hit cue. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Feedback")
	float WeakpointMultiplierThreshold = 1.45f;

	/** Seconds both actors freeze on a clean hit. Zero disables hit stop. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Feedback", meta=(ClampMin=0, Units="s"))
	float HitStopDuration = 0.08f;

	/** Seconds both actors freeze on a parry. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Feedback", meta=(ClampMin=0, Units="s"))
	float ParryHitStopDuration = 0.25f;
};
