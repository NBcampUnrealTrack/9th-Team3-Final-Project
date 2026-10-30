#include "Animation/knightAnimNode_HitFlinch.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimSequence.h"
#include "AnimationRuntime.h"
#include "Components/SkeletalMeshComponent.h"
#include "HAL/IConsoleManager.h"
#include "Monster/knightMonsterCharacterBase.h"

namespace knightHitFlinch
{
	static TAutoConsoleVariable<int32> CVarEnabled(TEXT("knight.Anim.HitFlinch"), 1,
		TEXT("1 = monsters flinch toward the hit side on every hit (knight Hit Flinch anim node), 0 = off."));
}

void FAnimNode_knightHitFlinch::Initialize_AnyThread(const FAnimationInitializeContext& Context)
{
	FAnimNode_Base::Initialize_AnyThread(Context);
	Source.Initialize(Context);
	Sequence = nullptr;
	Alpha = 0.f;
}

void FAnimNode_knightHitFlinch::CacheBones_AnyThread(const FAnimationCacheBonesContext& Context)
{
	Source.CacheBones(Context);
}

void FAnimNode_knightHitFlinch::PreUpdate(const UAnimInstance* InAnimInstance)
{
	const USkeletalMeshComponent* const Mesh = InAnimInstance ? InAnimInstance->GetSkelMeshComponent() : nullptr;
	const AknightMonsterCharacterBase* const Monster = Mesh ? Cast<AknightMonsterCharacterBase>(Mesh->GetOwner()) : nullptr;
	if (!Monster)
	{
		return;
	}
	const FknightHitFlinchState& State = Monster->GetHitFlinch();
	if (State.Count == SeenCount)
	{
		return;
	}
	SeenCount = State.Count;
	Sequence = State.Sequence;
	Strength = State.Strength;
	PlayRate = State.PlayRate;
	BlendIn = State.BlendIn;
	BlendOut = State.BlendOut;
	Time = 0.f;
}

void FAnimNode_knightHitFlinch::Update_AnyThread(const FAnimationUpdateContext& Context)
{
	Source.Update(Context);
	Alpha = 0.f;
	if (!Sequence)
	{
		return;
	}
	Time += Context.GetDeltaTime() * PlayRate;
	const float Length = Sequence->GetPlayLength();
	if (Time >= Length)
	{
		Sequence = nullptr;
		return;
	}
	const float In = BlendIn > 0.f ? FMath::Min(1.f, Time / BlendIn) : 1.f;
	const float Out = BlendOut > 0.f ? FMath::Min(1.f, (Length - Time) / BlendOut) : 1.f;
	Alpha = Strength * In * Out;
}

void FAnimNode_knightHitFlinch::Evaluate_AnyThread(FPoseContext& Output)
{
	Source.Evaluate(Output);
	if (!Sequence || Alpha <= UE_KINDA_SMALL_NUMBER || knightHitFlinch::CVarEnabled.GetValueOnAnyThread() == 0)
	{
		return;
	}
	FPoseContext HitPose(Output);
	FPoseContext BasePose(Output);
	FAnimationPoseData HitData(HitPose);
	FAnimationPoseData BaseData(BasePose);
	Sequence->GetAnimationPose(HitData, FAnimExtractContext(static_cast<double>(Time)));
	Sequence->GetAnimationPose(BaseData, FAnimExtractContext(0.0));

	// 루트(이동)는 그대로 두고, 피격 애니메이션의 커브(발 고정 등)도 더하지 않는다. 몸의 움찔만 얹는다.
	const FCompactPoseBoneIndex RootIndex(0);
	HitPose.Pose[RootIndex] = BasePose.Pose[RootIndex];
	FAnimationRuntime::ConvertPoseToAdditive(HitPose.Pose, BasePose.Pose);
	HitPose.Curve.Empty();
	HitPose.CustomAttributes.Empty();

	FAnimationPoseData OutputData(Output);
	FAnimationRuntime::AccumulateLocalSpaceAdditivePose(OutputData, HitData, Alpha);
	Output.Pose.NormalizeRotations();
}

void FAnimNode_knightHitFlinch::GatherDebugData(FNodeDebugData& DebugData)
{
	DebugData.AddDebugItem(FString::Printf(TEXT("HitFlinch(%s, %.2f s, alpha %.2f)"), *GetNameSafe(Sequence), Time, Alpha));
	Source.GatherDebugData(DebugData);
}
