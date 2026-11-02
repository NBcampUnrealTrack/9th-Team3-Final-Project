#include "AbilitySystem/Abilities/knightGameplayAbility_MonsterSwingAttack.h"

#include "Abilities/Tasks/AbilityTask_Repeat.h"
#include "Monster/knightMonsterCharacterBase.h"
#include "Monster/knightMonsterMovementComponent.h"
#include "Monster/knightMonsterTargetingComponent.h"
#include "MotionWarpingComponent.h"
#include "RootMotionModifier_SkewWarp.h"
#include "knight.h"

namespace knightMonsterSwing
{
	const FName WarpTargetName(TEXT("MonsterSwing"));
	constexpr float WindupInterval = 1.f / 60.f;
	constexpr float WindupTolerance = 2.f;
}

void UknightGameplayAbility_MonsterSwingAttack::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	if (!IsActive())
	{
		return;
	}

	AknightMonsterCharacterBase* const Monster = Cast<AknightMonsterCharacterBase>(GetAvatarActorFromActorInfo());
	const UknightMonsterTargetingComponent* const Targeting = Monster ? Monster->GetTargetingComponent() : nullptr;
	const AActor* const Target = Targeting ? Targeting->GetCurrentTarget() : nullptr;
	UMotionWarpingComponent* const MotionWarping = Monster ? Monster->FindComponentByClass<UMotionWarpingComponent>() : nullptr;
	if (!Monster || !MotionWarping)
	{
		return;
	}

	// 예비 동작: 휘두르는 반대쪽으로 돈다. 타깃이 휘두르는 쪽 뒤(WindupTargetAngle)에 오는 만큼 돌되, 범위를 벗어나면 최소만 돈다.
	const float WindupSign = SwingYaw < 0.f ? 1.f : -1.f;
	const float StartYaw = Monster->GetActorRotation().Yaw;
	float WindupYaw = WindupMinYaw;
	if (Target)
	{
		const FVector ToTarget = (Target->GetActorLocation() - Monster->GetActorLocation()).GetSafeNormal2D();
		// 휘두르는 쪽을 양수로 둔 타깃 각도. 예비 동작으로 R만큼 돌면 이 값이 R만큼 줄어든다.
		const float TargetAngle = WindupSign * FRotator::NormalizeAxis(ToTarget.Rotation().Yaw - StartYaw);
		const float Needed = FRotator::ClampAxis(TargetAngle - WindupTargetAngle);
		if (Needed >= WindupMinYaw && Needed <= WindupMaxYaw)
		{
			WindupYaw = Needed;
		}
	}
	WindupTargetYaw = StartYaw + WindupSign * WindupYaw;

	if (WindupMaxTime > 0.f && WindupYaw > 0.f)
	{
		const int32 Steps = FMath::Max(1, FMath::CeilToInt(WindupMaxTime / knightMonsterSwing::WindupInterval));
		UAbilityTask_Repeat* const Task = UAbilityTask_Repeat::RepeatAction(this, knightMonsterSwing::WindupInterval, Steps);
		Task->OnPerformAction.AddDynamic(this, &ThisClass::HandleWindupTick);
		WindupTask = Task;
		Task->ReadyForActivation();
	}

	// 휘두르기: 몽타주의 회전 구간을 예비 동작이 끝난 방향에서 SwingYaw만큼 돌도록 늘린다.
	URootMotionModifier_SkewWarp::AddRootMotionModifierSkewWarp(MotionWarping, Montage, SwingWarpStartTime, SwingWarpEndTime,
		knightMonsterSwing::WarpTargetName, EWarpPointAnimProvider::None, FTransform::Identity, NAME_None,
		/*bInWarpTranslation*/ false, /*bInIgnoreZAxis*/ true, /*bInWarpRotation*/ true);
	MotionWarping->AddOrUpdateWarpTargetFromLocationAndRotation(knightMonsterSwing::WarpTargetName, Monster->GetActorLocation(),
		FRotator(0.f, WindupTargetYaw + SwingYaw, 0.f));
	UE_LOG(LogknightAbility, Verbose, TEXT("%s: windup %.0f deg, swing %.0f deg."), *GetName(), WindupSign * WindupYaw, SwingYaw);
}

void UknightGameplayAbility_MonsterSwingAttack::HandleWindupTick(int32 ActionNumber)
{
	const AknightMonsterCharacterBase* const Monster = Cast<AknightMonsterCharacterBase>(GetAvatarActorFromActorInfo());
	UknightMonsterMovementComponent* const Movement = Monster ? Monster->GetMonsterMovementComponent() : nullptr;
	bool bDone = !Movement;
	if (!bDone)
	{
		const FVector Facing = Monster->GetActorLocation() + FRotator(0.f, WindupTargetYaw, 0.f).Vector() * 100.f;
		bDone = Movement->TurnTowards(Facing, GetMonsterTimeStep(knightMonsterSwing::WindupInterval), WindupTurnRate, knightMonsterSwing::WindupTolerance);
	}
	if (bDone && WindupTask.IsValid())
	{
		WindupTask->EndTask();
	}
}
