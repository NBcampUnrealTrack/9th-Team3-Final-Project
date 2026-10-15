#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "knightAnimNotify_GroundImpact.generated.h"

class UCameraShakeBase;
class UNiagaraSystem;

/**
 * 큰 몬스터가 땅을 내려찍는 순간(발이나 몸이 바닥에 닿는 프레임)에 두는 땅울림. 연출 전용이고 피해와는 관계없다.
 *
 * Bone 아래 바닥을 찾아 그 자리에 Dust를 터뜨리고, 가까운 플레이어의 화면을 흔든다. 흔들림은 InnerRadius 안에서
 * ShakeScale 그대로, OuterRadius에서 0이 되며 플레이어의 화면 흔들림 설정(CameraShakeScale)을 따른다.
 * 얕은 물 아래 바닥이면 수면에 WaterSplash를 터뜨리고 물결(UknightWaterRippleSubsystem)을 낸다.
 */
UCLASS(meta=(DisplayName="Knight Ground Impact"))
class KNIGHT_API UknightAnimNotify_GroundImpact : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
	virtual FString GetNotifyName_Implementation() const override;

protected:
	/** 바닥에 닿는 뼈(앞발, 가슴). None이면 메시 위치를 쓴다. */
	UPROPERTY(EditAnywhere, Category="Ground Impact")
	FName Bone;

	UPROPERTY(EditAnywhere, Category="Ground Impact|Shake")
	TSubclassOf<UCameraShakeBase> Shake;

	UPROPERTY(EditAnywhere, Category="Ground Impact|Shake", meta=(ClampMin=0))
	float ShakeScale = 1.f;

	UPROPERTY(EditAnywhere, Category="Ground Impact|Shake", meta=(ClampMin=0, Units="cm"))
	float InnerRadius = 1500.f;

	UPROPERTY(EditAnywhere, Category="Ground Impact|Shake", meta=(ClampMin=0, Units="cm"))
	float OuterRadius = 7000.f;

	UPROPERTY(EditAnywhere, Category="Ground Impact|Dust")
	TObjectPtr<UNiagaraSystem> Dust;

	UPROPERTY(EditAnywhere, Category="Ground Impact|Dust", meta=(ClampMin=0))
	float DustScale = 1.f;

	/** 뼈에서 이 거리 안에 바닥이 없으면(공중, 낭떠러지) 먼지를 만들지 않는다. */
	UPROPERTY(EditAnywhere, Category="Ground Impact|Dust", meta=(ClampMin=0, Units="cm"))
	float MaxGroundDistance = 1500.f;

	/** 물 위를 내려찍으면 Dust 대신 수면에 터뜨린다. 비어 있으면 Dust를 수면에 터뜨린다. */
	UPROPERTY(EditAnywhere, Category="Ground Impact|Water")
	TObjectPtr<UNiagaraSystem> WaterSplash;

	UPROPERTY(EditAnywhere, Category="Ground Impact|Water", meta=(ClampMin=0))
	float WaterSplashScale = 1.f;

	/** 물 위를 내려찍을 때 수면을 누르는 반지름. 물결이 켜진 레벨에서만 쓴다. */
	UPROPERTY(EditAnywhere, Category="Ground Impact|Water", meta=(ClampMin=0, Units="cm"))
	float RippleRadius = 600.f;

	UPROPERTY(EditAnywhere, Category="Ground Impact|Water", meta=(ClampMin=0, Units="cm"))
	float RippleDepth = 40.f;
};
