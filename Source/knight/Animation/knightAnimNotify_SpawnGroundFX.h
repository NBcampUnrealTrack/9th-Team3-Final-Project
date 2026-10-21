#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "knightAnimNotify_SpawnGroundFX.generated.h"

class UNiagaraSystem;

/**
 * 몬스터 앞 바닥에 이펙트를 남기는 노티파이(브레스가 지나간 자리의 불길과 잔열 자국). 연출 전용이다.
 *
 * 액터 기준 Offset(앞 X, 오른쪽 Y) 위치에서 아래로 바닥을 찾아, 액터와 같은 방향으로 월드에 스폰한다. 몸에 붙이지 않으므로
 * 몬스터가 움직여도 자국은 그 자리에 남는다. 이펙트는 스스로 끝나는 일회성이어야 한다(무한 루프면 남아 있다).
 */
UCLASS(meta=(DisplayName="Knight Spawn Ground FX"))
class KNIGHT_API UknightAnimNotify_SpawnGroundFX : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
	virtual FString GetNotifyName_Implementation() const override;

protected:
	UPROPERTY(EditAnywhere, Category="Ground FX")
	TObjectPtr<UNiagaraSystem> Template;

	/** 액터 기준 위치(앞 X, 오른쪽 Y, cm). 높이는 바닥을 찾아 정한다. */
	UPROPERTY(EditAnywhere, Category="Ground FX")
	FVector2D Offset = FVector2D(1500.0, 0.0);

	UPROPERTY(EditAnywhere, Category="Ground FX", meta=(ClampMin=0))
	float Scale = 1.f;

	/** 액터 높이에서 이 거리 안에 바닥이 없으면(공중, 낭떠러지) 만들지 않는다. */
	UPROPERTY(EditAnywhere, Category="Ground FX", meta=(ClampMin=0, Units="cm"))
	float MaxGroundDistance = 3000.f;
};
