#include "AbilitySystem/Abilities/knightGameplayAbility_HeavyAttack.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystem/knightAbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Abilities/Tasks/AbilityTask_WaitInputRelease.h"
#include "Animation/AnimMontage.h"
#include "Combat/knightCombatLibrary.h"
#include "Combat/knightGroggyFeedbackComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/Controller.h"
#include "TimerManager.h"
#include "System/knightGameplayTags.h"
#include "knight.h"

namespace
{
	// 락온 상태가 아닐 때 공격하면 화면 중앙(카메라/컨트롤러 수평 방향)을 바라보게 한다.
	void FaceScreenCenterForHeavyAttack(ACharacter* Character, const UAbilitySystemComponent* ASC)
	{
		if (!Character || (ASC && ASC->HasMatchingGameplayTag(knightGameplayTags::State_LockedOn)))
		{
			return;
		}
		if (const AController* const Controller = Character->GetController())
		{
			Character->SetActorRotation(FRotator(0.f, Controller->GetControlRotation().Yaw, 0.f));
		}
	}
}

UknightGameplayAbility_HeavyAttack::UknightGameplayAbility_HeavyAttack(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ActivationPolicy = EknightAbilityActivationPolicy::OnInputTriggered;
	// Independent while deciding, so the heavy step it starts is not blocked by this ability itself.
	ActivationGroup = EknightAbilityActivationGroup::Independent;
	SetAssetTags(FGameplayTagContainer(knightGameplayTags::Ability_Attack_Charge));
	ActivationBlockedTags.AddTag(knightGameplayTags::State_Dead);
	ActivationBlockedTags.AddTag(knightGameplayTags::State_HitReact);
	LoopHitParams.MotionValue = 15.f;
	LoopHitParams.PoiseDamage = 5.f;
	ReleaseHitParams.MotionValue = 80.f;
	ReleaseHitParams.PoiseDamage = 40.f;
	// 차지를 시작할 때 쓰는 행동 에너지. 탭 강공격 콤보는 각 강공격 단계(일반 공격 BP)의 값을 쓴다. 2026-10-09 임시값.
	StaminaCost = 20.f;
}

void UknightGameplayAbility_HeavyAttack::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	Phase = EPhase::Deciding;
	bReleased = false;

	UAbilityTask_WaitInputRelease* const Release = UAbilityTask_WaitInputRelease::WaitInputRelease(this, true);
	Release->OnRelease.AddDynamic(this, &ThisClass::OnReleased);
	Release->ReadyForActivation();

	UAbilityTask_WaitDelay* const Hold = UAbilityTask_WaitDelay::WaitDelay(this, HoldThreshold);
	Hold->OnFinish.AddDynamic(this, &ThisClass::OnHoldThreshold);
	Hold->ReadyForActivation();
}

void UknightGameplayAbility_HeavyAttack::OnReleased(float TimeHeld)
{
	bReleased = true;
	if (Phase == EPhase::Deciding)
	{
		// Tap: heavy combo step, retried for a short while if the current swing cannot be replaced yet.
		Phase = EPhase::TapBuffer;
		const UWorld* const World = GetWorld();
		TapDeadline = (World ? World->GetTimeSeconds() : 0.0) + TapBufferTime;
		TryHeavyStep();
	}
	else if (Phase == EPhase::Charging)
	{
		Phase = EPhase::Releasing;
		const UWorld* const World = GetWorld();
		const double Held = (World ? World->GetTimeSeconds() : ChargeStartTime) - ChargeStartTime;
		const float Alpha = FMath::Clamp(static_cast<float>(Held) / MaxChargeTime, 0.f, 1.f);
		const float Scale = FMath::Lerp(1.f, MaxChargeMultiplier, Alpha);
		ActiveHitParams = ReleaseHitParams;
		ActiveHitParams.MotionValue *= Scale;
		ActiveHitParams.PoiseDamage *= Scale;
		UE_LOG(LogknightAbility, Log, TEXT("%s: released after %.2fs charge, x%.2f (MV %.0f)"), *GetName(), Held, Scale, ActiveHitParams.MotionValue);
		if (ChargeReleaseMontage)
		{
			FaceScreenCenterForHeavyAttack(Cast<ACharacter>(GetAvatarActorFromActorInfo()), GetAbilitySystemComponentFromActorInfo());
			FVector WarpLocation;
			FRotator WarpRotation;
			float WarpTravel = 0.f;
			const bool bWarp = UknightCombatLibrary::SetAttackWarpTarget(GetAvatarActorFromActorInfo(), WarpTargetName, MaxWarpDistance,
				WarpStopDistance, bUseSoftTarget, SoftTargetRange, SoftTargetAngle)
				&& UknightCombatLibrary::ComputeAttackWarp(GetAvatarActorFromActorInfo(), MaxWarpDistance, WarpStopDistance,
					bUseSoftTarget, SoftTargetRange, SoftTargetAngle, WarpLocation, WarpRotation, WarpTravel);
			PlayMontage(ChargeReleaseMontage, TEXT("ChargeRelease"), false);
			if (bWarp)
			{
				// 발사 몽타주와 같은 배속(행동 속도)을 넘겨야 돌진이 모션과 같이 끝난다. (2026-10-08)
				UknightCombatLibrary::StartAttackLunge(this, ChargeReleaseMontage, GetAttackSpeed(), WarpLocation, WarpTravel, 0.25f);
			}
		}
		else
		{
			Finish(false);
		}
	}
}

void UknightGameplayAbility_HeavyAttack::TryHeavyStep()
{
	UAbilitySystemComponent* const ASC = GetAbilitySystemComponentFromActorInfo();
	UWorld* const World = GetWorld();
	if (!ASC || !World || !IsActive())
	{
		return;
	}
	if (HeavyAbilityTag.IsValid() && ASC->TryActivateAbilitiesByTag(FGameplayTagContainer(HeavyAbilityTag)))
	{
		Finish(false);
		return;
	}
	if (World->GetTimeSeconds() >= TapDeadline)
	{
		Finish(false);
		return;
	}
	World->GetTimerManager().SetTimer(TapTimer, this, &ThisClass::TryHeavyStep, 0.05f, false);
}

bool UknightGameplayAbility_HeavyAttack::CanStartCharge() const
{
	const UAbilitySystemComponent* const ASC = GetAbilitySystemComponentFromActorInfo();
	if (!ASC || !ChargeLoopMontage)
	{
		return false;
	}
	// (2026-10-14) 공격 후딜은 퀵스텝만 끊는다 → 그때는 캔슬 창이어도 차지를 시작하지 않는다(아래에서 실행 중인 공격에 걸림).
	const UknightAbilitySystemComponent* const KnightASC = GetKnightAbilitySystemComponentFromActorInfo();
	if (ASC->HasMatchingGameplayTag(knightGameplayTags::Window_Cancel) && !(KnightASC && KnightASC->IsAttackRecoveryDashOnly()))
	{
		return true;
	}
	FGameplayTagContainer Busy;
	Busy.AddTag(knightGameplayTags::Ability_Guard);
	Busy.AddTag(knightGameplayTags::Ability_Boost_Dash);
	for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
	{
		if (!Spec.IsActive() || !Spec.Ability || Spec.Ability == GetClass()->GetDefaultObject())
		{
			continue;
		}
		const FGameplayTagContainer& Tags = Spec.Ability->GetAssetTags();
		const bool bAttack = Tags.HasTag(knightGameplayTags::Ability_Attack) && !Tags.HasTagExact(knightGameplayTags::Ability_Attack_Charge);
		if (bAttack || Tags.HasAny(Busy))
		{
			return false;
		}
	}
	return true;
}

void UknightGameplayAbility_HeavyAttack::OnHoldThreshold()
{
	if (Phase != EPhase::Deciding || bReleased)
	{
		return;
	}
	if (!CanStartCharge())
	{
		Finish(false);
		return;
	}
	StartCharge();
}

void UknightGameplayAbility_HeavyAttack::StartCharge()
{
	UAbilitySystemComponent* const ASC = GetAbilitySystemComponentFromActorInfo();
	if (!ASC)
	{
		Finish(true);
		return;
	}
	// 차지를 시작할 때 비용(행동 에너지 StaminaCost)을 낸다. 탭으로 끝나면 이 어빌리티는 비용을 내지 않는다. (2026-10-09)
	if (!CommitAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo()))
	{
		Finish(true);
		return;
	}
	Phase = EPhase::Charging;
	// 락온 중이 아니면 화면 중앙(카메라 방향)을 바로 본다. 탭 콤보는 일반 공격 어빌리티가 같은 처리를 한다.
	FaceScreenCenterForHeavyAttack(Cast<ACharacter>(GetAvatarActorFromActorInfo()), ASC);
	const UWorld* const World = GetWorld();
	ChargeStartTime = World ? World->GetTimeSeconds() : 0.0;
	ActiveHitParams = LoopHitParams;

	FGameplayTagContainer Interrupt;
	Interrupt.AddTag(knightGameplayTags::Ability_Attack);
	Interrupt.AddTag(knightGameplayTags::Ability_Guard);
	Interrupt.AddTag(knightGameplayTags::Ability_Boost_Dash);
	ASC->CancelAbilities(&Interrupt, nullptr, this);
	FGameplayTagContainer Blocked(knightGameplayTags::Ability_Attack);
	ASC->BlockAbilitiesWithTags(Blocked);
	ASC->AddLooseGameplayTag(knightGameplayTags::State_Charging);
	bChargeStateApplied = true;

	UAbilityTask_WaitGameplayEvent* const HitTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, knightGameplayTags::Event_Melee_Hit, nullptr, false, true);
	HitTask->EventReceived.AddDynamic(this, &ThisClass::OnMeleeHitEvent);
	HitTask->ReadyForActivation();

	if (ChargeStartMontage)
	{
		PlayMontage(ChargeStartMontage, TEXT("ChargeStart"), false);
	}
	else
	{
		PlayLoop();
	}
}

void UknightGameplayAbility_HeavyAttack::PlayLoop()
{
	if (Phase == EPhase::Charging)
	{
		PlayMontage(ChargeLoopMontage, TEXT("ChargeLoop"), true);
	}
}

void UknightGameplayAbility_HeavyAttack::PlayMontage(UAnimMontage* Montage, FName TaskName, bool bLoopOnEnd)
{
	// 차지 시작·반복·발사 몽타주 모두 행동 속도 배속으로 재생한다. (2026-10-08)
	UAbilityTask_PlayMontageAndWait* const Task = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, TaskName, Montage, GetAttackSpeed());
	if (Phase == EPhase::Releasing)
	{
		Task->OnCompleted.AddDynamic(this, &ThisClass::OnReleaseFinished);
		Task->OnBlendOut.AddDynamic(this, &ThisClass::OnReleaseFinished);
		Task->OnInterrupted.AddDynamic(this, &ThisClass::OnReleaseFinished);
		Task->OnCancelled.AddDynamic(this, &ThisClass::OnReleaseFinished);
	}
	else if (bLoopOnEnd)
	{
		Task->OnCompleted.AddDynamic(this, &ThisClass::OnChargeLoopFinished);
		Task->OnBlendOut.AddDynamic(this, &ThisClass::OnChargeLoopFinished);
	}
	else
	{
		Task->OnCompleted.AddDynamic(this, &ThisClass::OnChargeStartFinished);
		Task->OnBlendOut.AddDynamic(this, &ThisClass::OnChargeStartFinished);
	}
	Task->ReadyForActivation();
	if (AActor* const Avatar = GetAvatarActorFromActorInfo())
	{
		if (UknightGroggyFeedbackComponent* const Feedback = Avatar->FindComponentByClass<UknightGroggyFeedbackComponent>())
		{
			Feedback->WatchAttack(this, Montage, ActiveHitParams);
		}
	}
}

void UknightGameplayAbility_HeavyAttack::OnChargeStartFinished()
{
	PlayLoop();
}

void UknightGameplayAbility_HeavyAttack::OnChargeLoopFinished()
{
	PlayLoop();
}

void UknightGameplayAbility_HeavyAttack::OnReleaseFinished()
{
	Finish(false);
}

void UknightGameplayAbility_HeavyAttack::OnMeleeHitEvent(FGameplayEventData Payload)
{
	const FHitResult* const Hit = Payload.TargetData.Num() > 0 && Payload.TargetData.Get(0) ? Payload.TargetData.Get(0)->GetHitResult() : nullptr;
	if (Hit && (Phase == EPhase::Charging || Phase == EPhase::Releasing))
	{
		UknightCombatLibrary::ApplyMeleeHit(GetAvatarActorFromActorInfo(), *Hit, ActiveHitParams, this);
	}
}

void UknightGameplayAbility_HeavyAttack::Finish(bool bWasCancelled)
{
	if (IsActive())
	{
		EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, bWasCancelled);
	}
}

void UknightGameplayAbility_HeavyAttack::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (AActor* const Avatar = GetAvatarActorFromActorInfo())
	{
		if (UknightGroggyFeedbackComponent* const Feedback = Avatar->FindComponentByClass<UknightGroggyFeedbackComponent>())
		{
			Feedback->EndPredictionForAttack(this);
		}
	}
	if (UWorld* const World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(TapTimer);
	}
	if (bChargeStateApplied)
	{
		bChargeStateApplied = false;
		if (UAbilitySystemComponent* const ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr)
		{
			ASC->RemoveLooseGameplayTag(knightGameplayTags::State_Charging);
			ASC->UnBlockAbilitiesWithTags(FGameplayTagContainer(knightGameplayTags::Ability_Attack));
		}
		if (bWasCancelled)
		{
			MontageStop(0.2f);
		}
	}
	Phase = EPhase::Deciding;
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
