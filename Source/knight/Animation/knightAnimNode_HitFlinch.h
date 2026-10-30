#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNodeBase.h"
#include "knightAnimNode_HitFlinch.generated.h"

class UAnimSequence;

/**
 * 맞을 때마다 지금 동작(이동, 공격) 위에 피격 애니메이션을 덧셈으로 살짝 얹어 몸이 맞은 쪽에서 움찔하게 한다.
 * 몽타주가 아니라서 공격을 끊지 않는다. 피격 애니메이션의 첫 프레임과의 차이만 더하고, 루트는 건드리지 않는다.
 *
 * 무엇을 얼마나 재생할지는 몬스터(AknightMonsterCharacterBase::GetHitFlinch)가 피해를 받을 때 정한다. 이 노드는 새 피격이
 * 들어오면 처음부터 다시 재생하고, BlendIn/BlendOut으로 세기를 올렸다 내린다. 시간은 애니메이션 시간이라 히트스톱 동안은 멈춘다.
 *
 * 콘솔: knight.Anim.HitFlinch 0 으로 끈다.
 */
USTRUCT(BlueprintInternalUseOnly)
struct KNIGHT_API FAnimNode_knightHitFlinch : public FAnimNode_Base
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category="Links")
	FPoseLink Source;

	virtual void Initialize_AnyThread(const FAnimationInitializeContext& Context) override;
	virtual void CacheBones_AnyThread(const FAnimationCacheBonesContext& Context) override;
	virtual void Update_AnyThread(const FAnimationUpdateContext& Context) override;
	virtual void Evaluate_AnyThread(FPoseContext& Output) override;
	virtual void GatherDebugData(FNodeDebugData& DebugData) override;
	virtual bool HasPreUpdate() const override { return true; }
	virtual void PreUpdate(const UAnimInstance* InAnimInstance) override;

private:
	/** 지금 얹는 피격 애니메이션. 몬스터 정의 에셋이 붙잡고 있다. */
	const UAnimSequence* Sequence = nullptr;
	float Time = 0.f;
	float PlayRate = 1.f;
	float Strength = 0.f;
	float BlendIn = 0.05f;
	float BlendOut = 0.25f;
	/** 마지막으로 받은 피격 번호. 바뀌면 처음부터 다시 재생한다. */
	int32 SeenCount = 0;
	/** 지금 세기(BlendIn/BlendOut 적용). Evaluate가 쓴다. */
	float Alpha = 0.f;
};
