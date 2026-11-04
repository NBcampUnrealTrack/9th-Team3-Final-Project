#include "AbilitySystem/Abilities/knightGameplayAbility_Guard.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "AbilitySystem/Attributes/knightAttributeSet.h"
#include "AbilitySystem/Attributes/knightCoreAttributeSet.h"
#include "AbilitySystem/knightAbilitySystemComponent.h"
#include "AbilitySystem/Effects/knightGameplayEffect_Damage.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayTag.h"
#include "Abilities/Tasks/AbilityTask_WaitInputRelease.h"
#include "Character/knightMovementComponent.h"
#include "Combat/knightCombatLibrary.h"
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
	// 공중(점프·낙하·호버링)에서는 가드를 올리지 않는다. 가드 중에 공중에 뜨면 내린다(OnLeftGround).
	ActivationBlockedTags.AddTag(knightGameplayTags::State_Airborne);
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

bool UknightGameplayAbility_Guard::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}
	// (2026-10-26) 퍼펙트 가드 없이 내린 직후 ReguardLockoutTime 동안은 다시 못 올린다(가드 연타 막기).
	// 여기서 막아야 누르고 있는 동안 bRetryPressWhileHeld가 잠금이 풀리는 순간 올려 준다.
	const UWorld* const World = GetWorld();
	return !World || LastGuardEndTime < 0.0 || World->GetTimeSeconds() - LastGuardEndTime >= ReguardLockoutTime;
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
	bGuardRaised = true;
	bParriedThisGuard = false;

	// 퍼펙트 가드 판정 시간은 누를 때 한 번 읽는다. 가드 도중 스탯이 바뀌면 다음 가드부터 반영된다. (2026-10-08)
	// (2026-10-15 사용자 결정) 후딜 중 미리 눌러 두었다가 나중에 올라간 가드도 올라간 순간부터 판정 시간 전부를 준다.
	// 가드를 올린 순간 맞았는데 퍼펙트가 안 되면 불쾌하기 때문(누른 순간부터 세는 방식은 버림).
	float PerfectGuardWindow = GetPerfectGuardWindow();

	// (2026-10-26) 퍼펙트 가드 없이 내린 뒤 PerfectGuardRearmTime 안에 다시 올렸으면 판정을 줄인다: 처음 x RearmPerfectGuardScale, 연속 두 번째부터 0.
	const UWorld* const World = GetWorld();
	const float SinceLastGuard = World && LastGuardEndTime >= 0.0 ? static_cast<float>(World->GetTimeSeconds() - LastGuardEndTime) : -1.f;
	QuickReguardCount = SinceLastGuard >= 0.f && SinceLastGuard < PerfectGuardRearmTime ? QuickReguardCount + 1 : 0;
	if (QuickReguardCount > 0)
	{
		const float FullWindow = PerfectGuardWindow;
		PerfectGuardWindow = QuickReguardCount == 1 ? PerfectGuardWindow * RearmPerfectGuardScale : 0.f;
		UE_LOG(LogknightCombat, Log, TEXT("%s: guard raised %.2fs after the last one (quick re-guard %d) -> perfect guard window %.3f -> %.3f"),
			*GetNameSafe(GetAvatarActorFromActorInfo()), SinceLastGuard, QuickReguardCount, FullWindow, PerfectGuardWindow);
	}

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

	// 가드 중에 공중에 뜨면(점프·낭떠러지) 가드를 내린다. 키를 누르고 있으면 착지해서 다시 올릴 수 있게 되는 순간 올라간다(bRetryPressWhileHeld).
	UAbilityTask_WaitGameplayTagAdded* const AirborneTask = UAbilityTask_WaitGameplayTagAdded::WaitGameplayTagAdd(this, knightGameplayTags::State_Airborne, nullptr, true);
	AirborneTask->Added.AddDynamic(this, &ThisClass::OnLeftGround);
	AirborneTask->ReadyForActivation();
}

void UknightGameplayAbility_Guard::OnLeftGround()
{
	EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, true);
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
	// (2026-10-27 팀 결정) 패링은 그로기를 깎지 않는다. 예전 규칙은 knight.Combat.ParryBuildsPoise 1.
	if (UknightCombatLibrary::DoesParryBuildPoise())
	{
		ApplyPerfectGuardPoiseDamage(Payload.Instigator.Get());
	}

	if (UAbilitySystemComponent* const ASC = GetAbilitySystemComponentFromActorInfo())
	{
		// (2026-10-20 사용자 결정) 패링 반격이 락온 없이도 패링한 적에게 돌진하도록 기억해 둔다.
		// 지금 투사체(가시 비)는 쏜 몬스터를 공격자로 넘기지만, 투사체 자신이 오면 쏜 쪽(Instigator, 없으면 Owner)으로 바꾼다.
		if (UknightAbilitySystemComponent* const KnightASC = Cast<UknightAbilitySystemComponent>(ASC))
		{
			AActor* CounterTarget = const_cast<AActor*>(Payload.Instigator.Get());
			if (CounterTarget && !CounterTarget->IsA<APawn>())
			{
				AActor* const Shooter = CounterTarget->GetInstigator() ? CounterTarget->GetInstigator() : CounterTarget->GetOwner();
				CounterTarget = Shooter ? Shooter : CounterTarget;
			}
			KnightASC->SetParryCounterTarget(CounterTarget);
		}
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
	// (2026-10-26) 패링으로 내려간 가드는 연타 막기(다시 올리기 잠금·판정 감소)를 적용하지 않는다(EndAbility).
	bParriedThisGuard = true;
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
	StopStepTurn(false);
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
		if (FaceLockedTarget(GuardLockOnTurnRate * GuardMoveCancelCheckInterval))
		{
			StopStepTurn(true);
		}
		else if (bStepTurnWhenUnlocked)
		{
			UpdateStepTurn(*Character);
		}
		else
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
	// (2026-10-26) 이동 입력으로 가드를 내리는 건 bMoveInputCancelsGuard를 켰을 때만.
	if (!bMoveInputCancelsGuard)
	{
		return;
	}
	MovePress.Update(*Character);
	if (MovePress.HasFreshPress() && !Character->GetLastMovementInputVector().IsNearlyZero())
	{
		EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, false);
	}
}

void UknightGameplayAbility_Guard::UpdateStepTurn(ACharacter& Character)
{
	UAnimInstance* const Anim = Character.GetMesh() ? Character.GetMesh()->GetAnimInstance() : nullptr;
	if (!Anim)
	{
		return;
	}
	// 도는 동안에는 몽타주의 루트 회전이 몸을 돌린다(여기서 회전을 건드리면 축발이 미끄러진다).
	// 돌기 몽타주는 자동 블렌드 아웃을 꺼 두어 끝 프레임에 멈춘다(재생은 끝났는데 아직 내려가지 않은 상태) → 다 돌았으니 가드 자세로 넘긴다.
	// 끝 프레임 = 가드 대기 자세라 이어 붙인 티가 나지 않는다.
	if (bStepTurning)
	{
		UAnimMontage* const Montage = ActiveStepTurnMontage.Get();
		// 일시정지(재생 멈춤)와 헷갈리지 않게 위치가 끝에 닿았는지도 본다.
		if (Montage && Anim->GetActiveInstanceForMontage(Montage) && !Anim->Montage_IsPlaying(Montage)
			&& Anim->Montage_GetPosition(Montage) >= Montage->GetPlayLength() - 0.02f)
		{
			bStepTurning = false;
			ActiveStepTurnMontage.Reset();
			PlayGuardPoseMontage();
		}
		return;
	}
	const AController* const Controller = Character.GetController();
	if (!Controller)
	{
		return;
	}
	const float Delta = FMath::FindDeltaAngleDegrees(Character.GetActorRotation().Yaw, Controller->GetControlRotation().Yaw);
	if (FMath::Abs(Delta) <= StepTurnAngle)
	{
		return;
	}
	// 가드 자세가 아닌 다른 몽타주(가드 피격·패링 주춤 등) 중이면 돌지 않는다.
	const UAnimMontage* const Current = Anim->GetCurrentActiveMontage();
	if (Current && Current != GuardMontage)
	{
		return;
	}
	// 요(Yaw)가 커지면 오른쪽으로 도는 것이다.
	const bool bRight = Delta > 0.f;
	UAnimMontage* const TurnMontage = FMath::Abs(Delta) > Turn180Angle
		? (bRight ? TurnRight180Montage.Get() : TurnLeft180Montage.Get())
		: (bRight ? TurnRight90Montage.Get() : TurnLeft90Montage.Get());
	if (!TurnMontage || Anim->Montage_Play(TurnMontage, StepTurnPlayRate * GetActionSpeed()) <= 0.f)
	{
		return;
	}
	bStepTurning = true;
	ActiveStepTurnMontage = TurnMontage;
	FOnMontageBlendingOutStarted BlendingOut;
	BlendingOut.BindUObject(this, &ThisClass::OnStepTurnBlendingOut);
	Anim->Montage_SetBlendingOutDelegate(BlendingOut, TurnMontage);
}

void UknightGameplayAbility_Guard::OnStepTurnBlendingOut(UAnimMontage* Montage, bool bInterrupted)
{
	if (Montage != ActiveStepTurnMontage.Get())
	{
		return;
	}
	bStepTurning = false;
	ActiveStepTurnMontage.Reset();
	// 다른 몽타주가 끼어들었으면 그 몽타주에 맡긴다. 끊긴 게 아니면 몽타주의 자동 블렌드 아웃이 켜져 있는 것(그러면 몇 도 덜 돈다) → 가드 자세로 이어 간다.
	if (!bInterrupted && IsActive())
	{
		PlayGuardPoseMontage();
	}
}

void UknightGameplayAbility_Guard::StopStepTurn(bool bResumeGuardPose)
{
	bStepTurning = false;
	UAnimMontage* const Montage = ActiveStepTurnMontage.Get();
	// 먼저 비워서 아래에서 몽타주를 멈출 때 오는 알림(OnStepTurnBlendingOut)을 무시하게 한다.
	ActiveStepTurnMontage.Reset();
	const ACharacter* const Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	UAnimInstance* const Anim = Character && Character->GetMesh() ? Character->GetMesh()->GetAnimInstance() : nullptr;
	// 끝 프레임에 멈춰 있는 돌기 몽타주(재생은 끝났지만 아직 내려가지 않음)도 내려야 하므로 Montage_IsPlaying이 아니라 활성 여부로 본다.
	if (!Montage || !Anim || !Anim->GetActiveInstanceForMontage(Montage))
	{
		return;
	}
	if (bResumeGuardPose && IsActive())
	{
		// 가드 자세 몽타주가 같은 슬롯의 돌기 몽타주를 밀어낸다.
		PlayGuardPoseMontage();
	}
	else
	{
		Anim->Montage_Stop(0.2f, Montage);
	}
}

void UknightGameplayAbility_Guard::PlayGuardPoseMontage()
{
	if (!GuardMontage)
	{
		return;
	}
	UAbilityTask_PlayMontageAndWait* const MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, GuardMontage, GetActionSpeed());
	MontageTask->ReadyForActivation();
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
	// (2026-10-26) 가드 연타 막기: 퍼펙트 가드 없이 내린 시각을 기록한다. 패링했으면 처음부터 다시 센다.
	if (bGuardRaised)
	{
		bGuardRaised = false;
		const UWorld* const World = GetWorld();
		LastGuardEndTime = bParriedThisGuard || !World ? -1.0 : World->GetTimeSeconds();
		if (bParriedThisGuard)
		{
			QuickReguardCount = 0;
		}
	}
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
