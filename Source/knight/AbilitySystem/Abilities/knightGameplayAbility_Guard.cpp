#include "AbilitySystem/Abilities/knightGameplayAbility_Guard.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "AbilitySystem/Attributes/knightAttributeSet.h"
#include "AbilitySystem/Attributes/knightCoreAttributeSet.h"
#include "AbilitySystem/Effects/knightGameplayEffect_Damage.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Abilities/Tasks/AbilityTask_WaitInputRelease.h"
#include "Character/knightMovementComponent.h"
#include "Combat/knightTargetLockComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/Controller.h"
#include "GameFramework/RootMotionSource.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameplayEffect.h"
#include "TimerManager.h"
#include "System/knightGameplayTags.h"
#include "knight.h"

namespace
{
	// 가드 중 새 이동 입력 검사 간격. 공격 후딜 이동 캔슬과 같은 값. (2026-10-08)
	constexpr float GuardMoveCancelCheckInterval = 0.02f;
}

UknightGameplayAbility_Guard::UknightGameplayAbility_Guard(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ActivationPolicy = EknightAbilityActivationPolicy::OnInputTriggered;
	ActivationGroup = EknightAbilityActivationGroup::Exclusive_Replaceable;
	SetAssetTags(FGameplayTagContainer(knightGameplayTags::Ability_Guard));
	ActivationOwnedTags.AddTag(knightGameplayTags::State_Guarding);
	ActivationBlockedTags.AddTag(knightGameplayTags::State_Dead);
	ActivationBlockedTags.AddTag(knightGameplayTags::State_HitReact);
	PerfectGuardDamageEffect = UknightGameplayEffect_Damage::StaticClass();
	// 가드는 행동 에너지를 쓰지 않는다(StaminaCost 0, 2026-10-09 사용자 결정). 막은 피격은 가드 효율에 따라 체력·그로기가 줄어든다(피해 실행).
	// (2026-10-15 사용자 결정) 공격 후딜 등으로 누른 순간 막혀 있었으면, 키를 누르고 있는 동안 가드할 수 있게 되는 순간 바로 올린다.
	bRetryPressWhileHeld = true;
}

float UknightGameplayAbility_Guard::GetPerfectGuardWindow() const
{
	const UAbilitySystemComponent* const ASC = GetAbilitySystemComponentFromActorInfo();
	const FGameplayAttribute Attribute = UknightAttributeSet::GetPerfectGuardWindowAttribute();
	return ASC && ASC->HasAttributeSetForAttribute(Attribute) ? ASC->GetNumericAttribute(Attribute) : ParryWindow;
}

void UknightGameplayAbility_Guard::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	UAbilitySystemComponent* const ASC = GetAbilitySystemComponentFromActorInfo();
	const bool bLockedOn = ASC && ASC->HasMatchingGameplayTag(knightGameplayTags::State_LockedOn);
	if (bLockedOn)
	{
		FaceLockedTarget(-1.f);
	}
	else if (ACharacter* const Character = Cast<ACharacter>(GetAvatarActorFromActorInfo()))
	{
		if (const AController* const Controller = Character->GetController())
		{
			Character->SetActorRotation(FRotator(0.f, Controller->GetControlRotation().Yaw, 0.f));
		}
	}
	EnterGuardStance();

	// 퍼펙트 가드 판정 시간은 누를 때 한 번 읽는다. 가드 도중 스탯이 바뀌면 다음 가드부터 반영된다. (2026-10-08)
	// (2026-10-15 사용자 결정) 후딜 중 미리 눌러 두었다가 나중에 올라간 가드도 올라간 순간부터 판정 시간 전부를 준다.
	// 가드를 올린 순간 맞았는데 퍼펙트가 안 되면 불쾌하기 때문(누른 순간부터 세는 방식은 버림).
	const float PerfectGuardWindow = GetPerfectGuardWindow();

	if (PerfectGuardWindow > 0.f && ParrySucceededEventTag.IsValid())
	{
		UAbilityTask_WaitGameplayEvent* const CounterTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, ParrySucceededEventTag, nullptr, true, true);
		CounterTask->EventReceived.AddDynamic(this, &ThisClass::OnParrySucceeded);
		CounterTask->ReadyForActivation();
	}

	if (PerfectGuardWindow > 0.f)
	{
		if (ASC)
		{
			ASC->AddLooseGameplayTag(knightGameplayTags::State_Parrying);
			bParryTagApplied = true;
		}
		UAbilityTask_WaitDelay* const ParryTask = UAbilityTask_WaitDelay::WaitDelay(this, PerfectGuardWindow);
		ParryTask->OnFinish.AddDynamic(this, &ThisClass::OnParryWindowEnded);
		ParryTask->ReadyForActivation();
	}

	if (GuardMontage)
	{
		// 가드 모션은 행동 속도 배속으로 재생한다. 퍼펙트 가드 판정 시간은 난이도가 바뀌지 않게 배속과 무관하게 둔다. (2026-10-08)
		UAbilityTask_PlayMontageAndWait* const MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, GuardMontage, GetActionSpeed());
		MontageTask->ReadyForActivation();
	}

	// 키를 떼면 퍼펙트 가드 판정 시간 중이라도 바로 가드를 내린다(퍼펙트 가드도 같이 끝남). (2026-10-08 사용자 결정)
	// 전에는 판정 시간이 끝날 때 뗌을 확인해서, 짧게 톡 눌러도 판정 시간 내내 패리가 유지되는 대신 그동안 가드를 내릴 수 없었다.
	// 입력 버퍼로 늦게 발동했는데 이미 키를 뗀 상태면 바로 끝난다.
	WaitForRelease();
}

void UknightGameplayAbility_Guard::WaitForRelease()
{
	UAbilityTask_WaitInputRelease* const ReleaseTask = UAbilityTask_WaitInputRelease::WaitInputRelease(this, true);
	ReleaseTask->OnRelease.AddDynamic(this, &ThisClass::OnGuardReleased);
	ReleaseTask->ReadyForActivation();
}

void UknightGameplayAbility_Guard::ApplyPerfectGuardPoiseDamage(const AActor* Attacker) const
{
	UAbilitySystemComponent* const ASC = GetAbilitySystemComponentFromActorInfo();
	UAbilitySystemComponent* const AttackerASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Attacker);
	// 피해 실행은 양쪽 ASC의 Core 속성 세트를 읽는다. 세트가 없는 쪽이 있으면 GAS가 크래시하므로 건너뛴다.
	const FGameplayAttribute AttackPowerAttribute = UknightCoreAttributeSet::GetAttackPowerAttribute();
	if (!ASC || !AttackerASC || !PerfectGuardDamageEffect
		|| !ASC->HasAttributeSetForAttribute(AttackPowerAttribute) || !AttackerASC->HasAttributeSetForAttribute(AttackPowerAttribute))
	{
		return;
	}
	// 피해 공식(기본 피해 + 공격력 x 모션 계수 / 100)과 같은 모양으로 그로기를 만든다. 경직치(StaggerPower)는 피해 실행이 곱한다.
	const float AttackPower = ASC->GetNumericAttribute(AttackPowerAttribute);
	const float BasePoiseDamage = PerfectGuardPoiseDamage + AttackPower * PerfectGuardPoiseMotionValue / 100.f;
	const FGameplayEffectSpecHandle SpecHandle = MakeOutgoingGameplayEffectSpec(PerfectGuardDamageEffect);
	if (!SpecHandle.IsValid())
	{
		return;
	}
	SpecHandle.Data->SetSetByCallerMagnitude(knightGameplayTags::SetByCaller_Damage, 0.f);
	SpecHandle.Data->SetSetByCallerMagnitude(knightGameplayTags::SetByCaller_MotionValue, 0.f);
	SpecHandle.Data->SetSetByCallerMagnitude(knightGameplayTags::SetByCaller_PoiseDamage, BasePoiseDamage);
	ASC->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data, AttackerASC);
	UE_LOG(LogknightCombat, Log, TEXT("%s perfect-guarded %s: poise damage %.1f (base %.1f + attack power %.1f x %.0f / 100), before stagger power."),
		*GetNameSafe(GetAvatarActorFromActorInfo()), *GetNameSafe(Attacker), BasePoiseDamage, PerfectGuardPoiseDamage, AttackPower, PerfectGuardPoiseMotionValue);
}

void UknightGameplayAbility_Guard::OnParrySucceeded(FGameplayEventData Payload)
{
	// 퍼펙트 가드: 공격한 적에게 그로기 피해. ApplyMeleeHit이 Instigator에 공격자를 담아 보낸다. (2026-10-08)
	ApplyPerfectGuardPoiseDamage(Payload.Instigator.Get());

	if (UAbilitySystemComponent* const ASC = GetAbilitySystemComponentFromActorInfo())
	{
		UWorld* const World = GetWorld();
		if (ParryCounterTag.IsValid() && ParryCounterWindow > 0.f && World)
		{
			// The window outlives this ability, so the timer belongs to the ASC, not to the guard.
			ASC->AddLooseGameplayTag(ParryCounterTag);
			const FGameplayTag Tag = ParryCounterTag;
			FTimerHandle Handle;
			World->GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(ASC, [ASC, Tag]()
			{
				ASC->RemoveLooseGameplayTag(Tag);
			}), ParryCounterWindow, false);
		}
		if (ParryCounterEffect)
		{
			ASC->ApplyGameplayEffectToSelf(ParryCounterEffect->GetDefaultObject<UGameplayEffect>(), 1.f, ASC->MakeEffectContext());
		}
	}
	// End the guard so the attack key can start the counter right away.
	ACharacter* const Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	const AActor* const Attacker = Payload.Instigator.Get();
	EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, false);
	// 가드 모션이 멈춘 뒤에 주춤 모션을 건다. 반격 모션이 같은 슬롯에서 이것을 끊는다. (2026-10-13)
	PlayParryRecoil(Character, Attacker);
}

void UknightGameplayAbility_Guard::PlayParryRecoil(ACharacter* Character, const AActor* Attacker) const
{
	if (!Character)
	{
		return;
	}
	if (ParryRecoilDistance > 0.f)
	{
		FVector Away = Attacker ? (Character->GetActorLocation() - Attacker->GetActorLocation()).GetSafeNormal2D() : FVector::ZeroVector;
		if (Away.IsNearlyZero())
		{
			Away = -Character->GetActorForwardVector().GetSafeNormal2D();
		}
		if (UCharacterMovementComponent* const Movement = Character->GetCharacterMovement())
		{
			// 어빌리티가 끝나도 남도록 무브먼트에 직접 건다. 반격 모션의 루트 모션을 덮지 않게 더하기.
			const float Time = FMath::Max(ParryRecoilTime, 0.02f);
			const TSharedPtr<FRootMotionSource_ConstantForce> Force = MakeShared<FRootMotionSource_ConstantForce>();
			Force->InstanceName = TEXT("ParryRecoil");
			Force->AccumulateMode = ERootMotionAccumulateMode::Additive;
			Force->Priority = 5;
			Force->Force = Away * (ParryRecoilDistance / Time);
			Force->Duration = Time;
			Force->FinishVelocityParams.Mode = ERootMotionFinishVelocityMode::SetVelocity;
			Force->FinishVelocityParams.SetVelocity = FVector::ZeroVector;
			Movement->ApplyRootMotionSource(Force);
		}
	}
	if (ParryRecoilMontage)
	{
		if (UAnimInstance* const AnimInstance = Character->GetMesh() ? Character->GetMesh()->GetAnimInstance() : nullptr)
		{
			AnimInstance->Montage_Play(ParryRecoilMontage, ParryRecoilPlayRate);
		}
	}
}

void UknightGameplayAbility_Guard::OnParryWindowEnded()
{
	// 판정 시간이 끝나면 일반 가드로 바뀐다. 가드 해제는 키를 뗄 때(WaitForRelease)만 한다.
	RemoveParryTag();
}

void UknightGameplayAbility_Guard::OnGuardReleased(float TimeHeld)
{
	EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, false);
}

void UknightGameplayAbility_Guard::RemoveParryTag()
{
	if (bParryTagApplied)
	{
		bParryTagApplied = false;
		if (UAbilitySystemComponent* const ASC = GetAbilitySystemComponentFromActorInfo())
		{
			ASC->RemoveLooseGameplayTag(knightGameplayTags::State_Parrying);
		}
	}
}

void UknightGameplayAbility_Guard::EnterGuardStance()
{
	// 가드 중에는 캔슬 창을 열어 두어, 공격·강공격·퀵스텝이 활성화 그룹 규칙대로 바로 가드를 대신하게 한다.
	if (UAbilitySystemComponent* const ASC = GetAbilitySystemComponentFromActorInfo())
	{
		ASC->AddLooseGameplayTag(knightGameplayTags::Window_Cancel);
		bCancelWindowApplied = true;
	}
	ACharacter* const Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	if (!Character)
	{
		return;
	}
	// 가드 중에는 제자리에 서고 몸도 돌지 않는다(가속 0).
	if (UknightMovementComponent* const Movement = Cast<UknightMovementComponent>(Character->GetCharacterMovement()))
	{
		Movement->SetMoveInputLocked(true);
		bMoveLockApplied = true;
	}
	MovePress.Reset(*Character);
	if (UWorld* const World = GetWorld())
	{
		World->GetTimerManager().SetTimer(MoveCancelTimer, this, &ThisClass::CheckMoveCancel, GuardMoveCancelCheckInterval, true);
	}
}

void UknightGameplayAbility_Guard::ExitGuardStance()
{
	if (UWorld* const World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(MoveCancelTimer);
	}
	if (bCancelWindowApplied)
	{
		bCancelWindowApplied = false;
		if (UAbilitySystemComponent* const ASC = GetAbilitySystemComponentFromActorInfo())
		{
			ASC->RemoveLooseGameplayTag(knightGameplayTags::Window_Cancel);
		}
	}
	if (bMoveLockApplied)
	{
		bMoveLockApplied = false;
		const ACharacter* const Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
		if (UknightMovementComponent* const Movement = Character ? Cast<UknightMovementComponent>(Character->GetCharacterMovement()) : nullptr)
		{
			Movement->SetMoveInputLocked(false);
		}
	}
}

void UknightGameplayAbility_Guard::CheckMoveCancel()
{
	ACharacter* const Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	if (!IsActive() || !Character)
	{
		return;
	}
	// 락온 중에는 가드하는 동안에도 대상을 따라 몸을 돌린다(초당 GuardLockOnTurnRate도). (2026-10-12)
	// 비락온 중에는 가드하는 동안 카메라 시선(화면 중앙)을 따라 몸을 돌린다.
	if (GuardLockOnTurnRate > 0.f)
	{
		if (!FaceLockedTarget(GuardLockOnTurnRate * GuardMoveCancelCheckInterval))
		{
			if (const AController* const Controller = Character->GetController())
			{
				const float CurrentYaw = Character->GetActorRotation().Yaw;
				const float TargetYaw = Controller->GetControlRotation().Yaw;
				const float TurnDelta = GuardLockOnTurnRate * GuardMoveCancelCheckInterval;
				const float NewYaw = FMath::FixedTurn(CurrentYaw, TargetYaw, TurnDelta);
				Character->SetActorRotation(FRotator(0.f, NewYaw, 0.f));
			}
		}
	}
	MovePress.Update(*Character);
	if (MovePress.HasFreshPress() && !Character->GetLastMovementInputVector().IsNearlyZero())
	{
		EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, false);
	}
}

bool UknightGameplayAbility_Guard::FaceLockedTarget(float MaxDeltaYaw) const
{
	ACharacter* const Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	const UknightTargetLockComponent* const TargetLock = Character ? Character->FindComponentByClass<UknightTargetLockComponent>() : nullptr;
	FVector TargetLocation;
	if (!TargetLock || !TargetLock->GetLockedTargetLocation(TargetLocation))
	{
		return false;
	}
	const FVector ToTarget = (TargetLocation - Character->GetActorLocation()).GetSafeNormal2D();
	if (ToTarget.IsNearlyZero())
	{
		return false;
	}
	const float CurrentYaw = Character->GetActorRotation().Yaw;
	const float TargetYaw = ToTarget.Rotation().Yaw;
	const float NewYaw = MaxDeltaYaw < 0.f
		? TargetYaw
		: CurrentYaw + FMath::Clamp(FMath::FindDeltaAngleDegrees(CurrentYaw, TargetYaw), -MaxDeltaYaw, MaxDeltaYaw);
	Character->SetActorRotation(FRotator(0.f, NewYaw, 0.f));
	return true;
}

void UknightGameplayAbility_Guard::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	RemoveParryTag();
	ExitGuardStance();
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
