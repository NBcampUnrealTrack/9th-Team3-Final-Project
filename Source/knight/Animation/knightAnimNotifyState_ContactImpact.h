#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "knightAnimNotifyState_ContactImpact.generated.h"

class UknightAnimNotify_GroundImpact;

USTRUCT(BlueprintType)
struct FknightContactImpactStep
{
	GENERATED_BODY()

	/** 몸이 닿은 뒤 이만큼 지나서 터뜨린다(솟구친 물이 쏟아지는 것처럼 뒤따르는 연출). */
	UPROPERTY(EditAnywhere, Category="Contact Impact", meta=(ClampMin=0, Units="s"))
	float Delay = 0.f;

	UPROPERTY(EditAnywhere, Instanced, Category="Contact Impact")
	TObjectPtr<UknightAnimNotify_GroundImpact> Impact;
};

/**
 * 구간 동안 몸이 실제로 수면(물이 없으면 바닥)에 닿는 순간을 찾아 Steps의 땅울림을 터뜨린다. 연출 전용이다.
 *
 * 떨어지는 높이가 그때그때 달라(탈진 착지, 패링당해 떨어지는 다이브) 정해진 시각에 두면 물보라가 이르거나 늦는다.
 * 매 프레임 루트를 뺀 뼈 중 가장 낮은 뼈가 수면에서 ContactHeight 안으로 내려오면 닿은 것으로 본다.
 * 구간이 끝날 때까지 닿지 않았으면 끝나는 순간에 터뜨린다.
 */
UCLASS(meta=(DisplayName="Knight Contact Impact"))
class KNIGHT_API UknightAnimNotifyState_ContactImpact : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration,
		const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float FrameDeltaTime,
		const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
	virtual FString GetNotifyName_Implementation() const override;

protected:
	/** 가장 낮은 뼈가 수면·바닥에서 이 높이(cm) 안으로 내려오면 닿은 것으로 본다. 뼈는 살 안쪽에 있으니 몸 두께만큼 준다. */
	UPROPERTY(EditAnywhere, Category="Contact Impact", meta=(ClampMin=0, Units="cm"))
	float ContactHeight = 30.f;

	/** 이 거리 안에 바닥이 없으면(높이 떠 있음) 아직 닿지 않은 것으로 본다. */
	UPROPERTY(EditAnywhere, Category="Contact Impact", meta=(ClampMin=0, Units="cm"))
	float MaxGroundDistance = 3000.f;

	UPROPERTY(EditAnywhere, Category="Contact Impact")
	TArray<FknightContactImpactStep> Steps;

private:
	bool IsTouching(const USkeletalMeshComponent* MeshComp) const;
	void Trigger(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation);

	/** 같은 몽타주를 여러 몬스터가 함께 쓰므로 메시마다 이번 구간에 이미 터뜨렸는지 기억한다. */
	TMap<TWeakObjectPtr<USkeletalMeshComponent>, bool> Triggered;
};
