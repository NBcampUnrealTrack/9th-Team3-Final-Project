#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "knightAnimNotify_MeleeArea.generated.h"

/**
 * 몽타주의 이 프레임에 몬스터 앞쪽 구체 안의 대상을 한 번 친다. 양발 내려찍기처럼 착지 순간 넓게 덮는 타격에 쓴다.
 *
 * Knight Melee Trace와 같은 UknightMeleeTraceComponent를 거쳐 Event.Melee.Hit로 보내므로, 공격 어빌리티
 * (UknightGameplayAbility_MonsterMeleeAttack)가 같은 HitParams로 피해·가드·패링을 처리한다. 한 몽타주 안에서
 * 칼날 판정(Melee Trace)과 섞어 쓸 수 있고, 타이밍이 몽타주 프레임에 붙어 재생 속도가 바뀌어도 어긋나지 않는다.
 * 범위는 UknightGameplayAbility_MonsterAreaAttack과 같은 방식(몬스터 로컬 Offset, 반지름 Radius)이다.
 *
 * 디버그: knight.Combat.DrawMeleeTraces 1
 */
UCLASS(meta=(DisplayName="Knight Melee Area"))
class KNIGHT_API UknightAnimNotify_MeleeArea : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
	virtual FString GetNotifyName_Implementation() const override;

protected:
	/** 몬스터 로컬 공간 기준 구체 중심(X가 정면). */
	UPROPERTY(EditAnywhere, Category="Area")
	FVector Offset = FVector(350.f, 0.f, 0.f);

	UPROPERTY(EditAnywhere, Category="Area", meta=(ClampMin=1, Units="cm"))
	float Radius = 300.f;
};
