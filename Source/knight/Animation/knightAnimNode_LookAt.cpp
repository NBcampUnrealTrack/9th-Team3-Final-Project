#include "Animation/knightAnimNode_LookAt.h"

#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbility.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "Components/SkeletalMeshComponent.h"
#include "HAL/IConsoleManager.h"
#include "Monster/knightMonsterCharacterBase.h"
#include "Monster/knightMonsterTargetingComponent.h"
#include "System/knightGameplayTags.h"

namespace knightLookAt
{
	static TAutoConsoleVariable<int32> CVarEnabled(TEXT("knight.Anim.LookAt"), 1,
		TEXT("1 = monsters turn their neck and head toward the target while idle or moving (knight Look At anim node), 0 = off."));
}

void FAnimNode_knightLookAt::InitializeBoneReferences(const FBoneContainer& RequiredBones)
{
	for (FknightLookAtBone& Entry : Bones)
	{
		Entry.Bone.Initialize(RequiredBones);
	}
}

bool FAnimNode_knightLookAt::IsValidToEvaluate(const USkeleton* Skeleton, const FBoneContainer& RequiredBones)
{
	// 엔진은 여기서 거짓이면 UpdateInternal도 부르지 않는다. 세기만 보면 0에서 올라가지 못하므로 볼 생각이 있을 때도 참으로 둔다.
	if (knightLookAt::CVarEnabled.GetValueOnAnyThread() == 0 || (!bWantLook && Strength <= UE_KINDA_SMALL_NUMBER))
	{
		return false;
	}
	return Bones.ContainsByPredicate([&RequiredBones](const FknightLookAtBone& Entry)
	{
		return Entry.Weight > 0.f && Entry.Bone.IsValidToEvaluate(RequiredBones);
	});
}

bool FAnimNode_knightLookAt::WantsToLook(const AknightMonsterCharacterBase& Monster, bool bDisableWhenAirborne)
{
	if (Monster.IsDead())
	{
		return false;
	}
	const UknightMonsterTargetingComponent* const Targeting = Monster.GetTargetingComponent();
	if (!Targeting || !Targeting->GetCurrentTarget())
	{
		return false;
	}
	const UAbilitySystemComponent* const ASC = Monster.GetAbilitySystemComponent();
	if (!ASC)
	{
		return true;
	}
	if (ASC->HasMatchingGameplayTag(knightGameplayTags::State_Monster_Reacting)
		|| (bDisableWhenAirborne && ASC->HasMatchingGameplayTag(knightGameplayTags::State_Monster_Airborne)))
	{
		return false;
	}
	// 공격 중에는 공격 모션의 머리 움직임을 그대로 둔다. 이동 행동(옆걸음, 물러나기)은 공격 태그도 갖지만 타깃을 보며 움직인다.
	for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
	{
		if (Spec.IsActive() && Spec.Ability)
		{
			const FGameplayTagContainer& Tags = Spec.Ability->GetAssetTags();
			if (Tags.HasTag(knightGameplayTags::Ability_Attack) && !Tags.HasTag(knightGameplayTags::Ability_Move))
			{
				return false;
			}
		}
	}
	return true;
}

void FAnimNode_knightLookAt::PreUpdate(const UAnimInstance* InAnimInstance)
{
	bWantLook = false;
	const USkeletalMeshComponent* const Mesh = InAnimInstance ? InAnimInstance->GetSkelMeshComponent() : nullptr;
	const AknightMonsterCharacterBase* const Monster = Mesh ? Cast<AknightMonsterCharacterBase>(Mesh->GetOwner()) : nullptr;
	if (!Monster || Bones.IsEmpty() || knightLookAt::CVarEnabled.GetValueOnGameThread() == 0)
	{
		return;
	}
	ActorToComponent = Mesh->GetComponentQuat().Inverse() * Monster->GetActorQuat();
	// 공격이 머리 방향을 직접 정하면(브레스 쓸기) 공격 중이어도 그 각도로 돌린다. 위아래는 공격 모션 그대로 둔다.
	float AimYaw = 0.f;
	if (!Monster->IsDead() && Monster->GetHeadAimYaw(AimYaw))
	{
		DesiredYaw = FMath::Clamp(AimYaw, -MaxYaw, MaxYaw);
		DesiredPitch = 0.f;
		bWantLook = true;
		return;
	}
	if (!WantsToLook(*Monster, bDisableWhenAirborne))
	{
		return;
	}
	const AActor* const Target = Monster->GetTargetingComponent()->GetCurrentTarget();
	// 마지막 뼈(머리)에서 본다. 지난 프레임 포즈지만 방향을 정하는 데는 충분하다.
	const FName HeadName = Bones.Last().Bone.BoneName;
	const FVector Eye = Mesh->GetBoneIndex(HeadName) != INDEX_NONE ? Mesh->GetSocketLocation(HeadName) : Monster->GetActorLocation();
	const FVector ToTargetLocal = Monster->GetActorQuat().UnrotateVector(Target->GetActorLocation() - Eye);
	if (ToTargetLocal.IsNearlyZero())
	{
		return;
	}
	DesiredYaw = FMath::Clamp(FMath::RadiansToDegrees(FMath::Atan2(ToTargetLocal.Y, ToTargetLocal.X)), -MaxYaw, MaxYaw);
	DesiredPitch = FMath::Clamp(FMath::RadiansToDegrees(FMath::Atan2(ToTargetLocal.Z, ToTargetLocal.Size2D())), -MaxPitch, MaxPitch);
	bWantLook = true;
}

void FAnimNode_knightLookAt::UpdateInternal(const FAnimationUpdateContext& Context)
{
	FAnimNode_SkeletalControlBase::UpdateInternal(Context);
	const float DeltaTime = Context.GetDeltaTime();
	Strength = FMath::FInterpConstantTo(Strength, bWantLook ? 1.f : 0.f, DeltaTime, 1.f / BlendTime);
	// 풀리는 동안에는 마지막 방향을 유지한 채 세기만 줄인다. 정면으로 돌아오면서 동시에 풀리면 머리가 두 번 움직여 보인다.
	if (bWantLook)
	{
		const float Step = TurnSpeed * DeltaTime;
		CurrentYaw = FMath::FixedTurn(CurrentYaw, DesiredYaw, Step);
		CurrentPitch = FMath::FixedTurn(CurrentPitch, DesiredPitch, Step);
	}
	else if (Strength <= UE_KINDA_SMALL_NUMBER)
	{
		CurrentYaw = 0.f;
		CurrentPitch = 0.f;
	}
}

void FAnimNode_knightLookAt::EvaluateSkeletalControl_AnyThread(FComponentSpacePoseContext& Output, TArray<FBoneTransform>& OutBoneTransforms)
{
	const FBoneContainer& BoneContainer = Output.Pose.GetPose().GetBoneContainer();
	const FVector ForwardCS = ActorToComponent.RotateVector(FVector::ForwardVector);
	const FVector LookCS = ActorToComponent.RotateVector(FRotator(CurrentPitch, CurrentYaw, 0.f).Vector());
	const FQuat FullTurn = FQuat::FindBetweenNormals(ForwardCS, LookCS);

	// 뿌리부터 돌린다. 앞 뼈를 돌린 만큼(Accumulated) 다음 뼈도 따라 움직인 위치에서 자기 몫을 더 돌린다.
	TArray<TPair<FCompactPoseBoneIndex, float>, TInlineAllocator<8>> Chain;
	for (const FknightLookAtBone& Entry : Bones)
	{
		if (Entry.Weight > 0.f && Entry.Bone.IsValidToEvaluate(BoneContainer))
		{
			Chain.Emplace(Entry.Bone.GetCompactPoseIndex(BoneContainer), Entry.Weight);
		}
	}
	Chain.Sort([](const TPair<FCompactPoseBoneIndex, float>& A, const TPair<FCompactPoseBoneIndex, float>& B) { return A.Key < B.Key; });

	FTransform Accumulated = FTransform::Identity;
	for (const TPair<FCompactPoseBoneIndex, float>& Link : Chain)
	{
		const FTransform Original = Output.Pose.GetComponentSpaceTransform(Link.Key);
		const FVector Pivot = (Original * Accumulated).GetLocation();
		const FQuat Share = FQuat::Slerp(FQuat::Identity, FullTurn, Link.Value * Strength);
		Accumulated = Accumulated * FTransform(-Pivot) * FTransform(Share) * FTransform(Pivot);
		FTransform Result = Original * Accumulated;
		Result.NormalizeRotation();
		OutBoneTransforms.Emplace(Link.Key, Result);
	}
}

void FAnimNode_knightLookAt::GatherDebugData(FNodeDebugData& DebugData)
{
	DebugData.AddDebugItem(FString::Printf(TEXT("%s (Yaw %.1f, Pitch %.1f, Strength %.2f)"),
		*DebugData.GetNodeName(this), CurrentYaw, CurrentPitch, Strength));
	ComponentPose.GatherDebugData(DebugData);
}
