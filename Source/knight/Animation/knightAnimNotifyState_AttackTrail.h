#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "knightAnimNotifyState_AttackTrail.generated.h"

class UknightAttackTrailComponent;
class UMaterialInterface;

/**
 * 구간 동안 뼈 두 개(BaseSocket → TipSocket)가 지나간 자리에 공격 궤적을 남긴다.
 * 판정(Knight Melee Trace)과 같은 뼈·구간을 쓰면 궤적이 판정 범위와 겹친다.
 * 구간이 끝나도 남은 궤적은 Lifetime초 동안 사라지며 남는다.
 */
UCLASS(meta=(DisplayName="Knight Attack Trail"))
class KNIGHT_API UknightAnimNotifyState_AttackTrail : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	UknightAnimNotifyState_AttackTrail();

	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
	virtual FString GetNotifyName_Implementation() const override;

protected:
	UPROPERTY(EditAnywhere, Category="Trail")
	FName BaseSocket;

	UPROPERTY(EditAnywhere, Category="Trail")
	FName TipSocket;

	/** 머티리얼은 UV.x(나이), UV.y(Base 0 → Tip 1), 정점 색 A(세기)를 받는다. */
	UPROPERTY(EditAnywhere, Category="Trail")
	TObjectPtr<UMaterialInterface> Material;

	/** Base에서 Tip 쪽으로 이만큼 올라간 곳부터 그린다(0 = Base부터). */
	UPROPERTY(EditAnywhere, Category="Trail", meta=(ClampMin=0, ClampMax=1))
	float BaseRatio = 0.f;

	/** Tip 바깥으로 더 늘이는 길이. 메시 스케일과 관계없이 월드 cm다. */
	UPROPERTY(EditAnywhere, Category="Trail", meta=(Units="cm"))
	float TipExtension = 0.f;

	/** 지나간 자리가 사라지기까지 걸리는 시간. */
	UPROPERTY(EditAnywhere, Category="Trail", meta=(ClampMin=0.01, Units="s"))
	float Lifetime = 0.25f;

	UPROPERTY(EditAnywhere, Category="Trail", meta=(ClampMin=0, Units="s"))
	float FadeIn = 0.04f;

	/** Tip이 MinSpeed보다 느린 곳은 그리지 않고 FullSpeed부터 다 그린다(월드 cm/s). 둘 다 0이면 속도와 관계없이 그린다. */
	UPROPERTY(EditAnywhere, Category="Trail", meta=(ClampMin=0, Units="cm/s"))
	float MinSpeed = 0.f;

	UPROPERTY(EditAnywhere, Category="Trail", meta=(ClampMin=0, Units="cm/s"))
	float FullSpeed = 0.f;

private:
	/** 노티파이 객체는 같은 몽타주를 재생하는 모든 메시가 함께 쓰므로 메시마다 궤적을 따로 둔다. */
	TMap<TWeakObjectPtr<USkeletalMeshComponent>, TWeakObjectPtr<UknightAttackTrailComponent>> ActiveTrails;
};
