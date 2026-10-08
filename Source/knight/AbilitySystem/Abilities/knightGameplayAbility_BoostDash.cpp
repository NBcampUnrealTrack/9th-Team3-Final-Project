#include "AbilitySystem/Abilities/knightGameplayAbility_BoostDash.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystem/Attributes/knightAttributeSet.h"
#include "Abilities/Tasks/AbilityTask_ApplyRootMotionConstantForce.h"
#include "Character/knightMovementComponent.h"
#include "Engine/World.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Abilities/Tasks/AbilityTask_WaitInputPress.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/RootMotionSource.h"
#include "Input/knightInputBufferComponent.h"
#include "Perception/AISense_Hearing.h"
#include "System/knightGameplayTags.h"

UknightGameplayAbility_BoostDash::UknightGameplayAbility_BoostDash(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ActivationPolicy = EknightAbilityActivationPolicy::OnInputTriggered;
	ActivationGroup = EknightAbilityActivationGroup::Exclusive_Replaceable;
	SetAssetTags(FGameplayTagContainer(knightGameplayTags::Ability_Boost_Dash));
	ActivationOwnedTags.AddTag(knightGameplayTags::State_Boosting);
	ActivationBlockedTags.AddTag(knightGameplayTags::State_Dead);
	ActivationBlockedTags.AddTag(knightGameplayTags::State_HitReact);
	ActivationBlockedTags.AddTag(knightGameplayTags::State_Overheated);
	DashCueTag = knightGameplayTags::GameplayCue_Boost_Dash;
}

float UknightGameplayAbility_BoostDash::GetMechAttributeValue(const FGameplayAttribute& Attribute, float Fallback) const
{
	const UAbilitySystemComponent* const ASC = GetAbilitySystemComponentFromActorInfo();
	return ASC && ASC->HasAttributeSetForAttribute(Attribute) ? ASC->GetNumericAttribute(Attribute) : Fallback;
}

bool UknightGameplayAbility_BoostDash::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	// 퀵스텝 쿨타임 중이면 발동하지 않는다.
	const UWorld* const World = GetWorld();
	if (World && World->GetTimeSeconds() < CooldownEndTime)
	{
		return false;
	}
	return Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags);
}

void UknightGameplayAbility_BoostDash::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	ACharacter* const Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	if (!Character || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	StartDash(*Character);
}

void UknightGameplayAbility_BoostDash::StartDash(ACharacter& Character)
{
	// 경직 중 이어지는 대시면, 이전 대시의 무적·몽타주 태스크를 먼저 정리한다.
	if (InvulnerableTask.IsValid())
	{
		InvulnerableTask->EndTask();
	}
	if (MontageTask.IsValid())
	{
		MontageTask->EndTask();
	}
	RemoveInvulnerability();
	bCooldownPending = true;

	FVector Direction = Character.GetCharacterMovement()->GetLastInputVector().GetSafeNormal2D();
	const bool bHasInput = !Direction.IsNearlyZero();
	if (!bHasInput)
	{
		Direction = -Character.GetActorForwardVector().GetSafeNormal2D();
	}

	const UAbilitySystemComponent* const OwnerASC = GetAbilitySystemComponentFromActorInfo();
	const bool bLockedOn = OwnerASC && OwnerASC->HasMatchingGameplayTag(knightGameplayTags::State_LockedOn);
	if (bFaceDashDirectionWhenFree && bHasInput && !bLockedOn)
	{
		Character.SetActorRotation(FRotator(0.f, Direction.Rotation().Yaw, 0.f));
	}
	UAnimMontage* const Montage = SelectDashMontage(Character, Direction);

	if (InvulnerableDuration > 0.f)
	{
		if (UAbilitySystemComponent* const ASC = GetAbilitySystemComponentFromActorInfo())
		{
			ASC->AddLooseGameplayTag(knightGameplayTags::State_Invulnerable);
			bInvulnerableApplied = true;
		}
		UAbilityTask_WaitDelay* const Task = UAbilityTask_WaitDelay::WaitDelay(this, InvulnerableDuration);
		Task->OnFinish.AddDynamic(this, &ThisClass::OnInvulnerabilityEnded);
		Task->ReadyForActivation();
		InvulnerableTask = Task;
	}

	if (Montage)
	{
		UAbilityTask_PlayMontageAndWait* const Task = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
			this, NAME_None, Montage, GetMontagePlayRate(Montage), NAME_None, true, 0.f);
		Task->ReadyForActivation();
		MontageTask = Task;
	}

	// 거리 = 기본 거리 x 퀵스텝 속도 배율 x 중량 계수. 시간은 고정이라 거리가 늘면 속도도 빨라진다.
	const UknightMovementComponent* const KnightMovement = Cast<UknightMovementComponent>(Character.GetCharacterMovement());
	const float WeightFactor = KnightMovement ? KnightMovement->GetWeightSpeedFactor() : 1.f;
	const float Distance = DashDistance * GetMechAttributeValue(UknightAttributeSet::GetDodgeSpeedAttribute(), 1.f) * WeightFactor;
	const float Strength = Distance / DashDuration;
	UAbilityTask_ApplyRootMotionConstantForce* const DashTask = UAbilityTask_ApplyRootMotionConstantForce::ApplyRootMotionConstantForce(
		this, TEXT("BoostDash"), Direction, Strength, DashDuration, false, StrengthOverTime,
		ERootMotionFinishVelocityMode::ClampVelocity, FVector::ZeroVector, Character.GetCharacterMovement()->GetMaxSpeed(), false);
	DashTask->OnFinish.AddDynamic(this, &ThisClass::OnDashFinished);
	DashTask->ReadyForActivation();

	UAbilitySystemComponent* const CueASC = DashCueTag.IsValid() ? GetAbilitySystemComponentFromActorInfo() : nullptr;
	if (UAbilitySystemComponent* const ASC = CueASC)
	{
		FGameplayCueParameters CueParams;
		CueParams.Location = Character.GetActorLocation();
		CueParams.Normal = Direction;
		CueParams.Instigator = &Character;
		ASC->ExecuteGameplayCue(DashCueTag, CueParams);
	}

	// 추진기는 시끄럽다: 아직 나이트를 못 본 몬스터도 소리를 들을 수 있다 (AI 청각).
	UAISense_Hearing::ReportNoiseEvent(&Character, Character.GetActorLocation(), NoiseLoudness, &Character, NoiseRange, TEXT("Boost"));
}

float UknightGameplayAbility_BoostDash::GetMontagePlayRate(const UAnimMontage* Montage) const
{
	// 지금 규칙: 고정 배속(MontagePlayRate). 헤더 주석 참고 — 바뀔 수 있는 항목.
	return MontagePlayRate;
}

UAnimMontage* UknightGameplayAbility_BoostDash::SelectDashMontage(const ACharacter& Character, const FVector& Direction) const
{
	const FVector Forward = Character.GetActorForwardVector().GetSafeNormal2D();
	const FVector Right = Character.GetActorRightVector().GetSafeNormal2D();
	const float ForwardDot = FVector::DotProduct(Direction, Forward);
	const float RightDot = FVector::DotProduct(Direction, Right);

	UAnimMontage* Picked = DashMontage;
	if (FMath::Abs(ForwardDot) >= FMath::Abs(RightDot))
	{
		if (ForwardDot < 0.f && DashMontageBack)
		{
			Picked = DashMontageBack;
		}
	}
	else if (RightDot > 0.f)
	{
		Picked = DashMontageRight ? DashMontageRight.Get() : DashMontage.Get();
	}
	else
	{
		Picked = DashMontageLeft ? DashMontageLeft.Get() : DashMontage.Get();
	}
	return Picked;
}

void UknightGameplayAbility_BoostDash::OnDashFinished()
{
	// 쿨타임은 이동이 끝난 시점부터 센다. 그래야 경직 중에 쿨타임이 끝나 대시로 캔슬할 수 있다.
	StartCooldown();
	if (RecoveryDuration <= 0.f)
	{
		EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, false);
		return;
	}

	// 경직: 공격·가드가 대시를 대신할 수 있게 캔슬 창을 열고, 이동 입력으로는 가속하지 못하게 한다(미끄러지며 정지).
	bInRecovery = true;
	if (UAbilitySystemComponent* const ASC = GetAbilitySystemComponentFromActorInfo())
	{
		ASC->AddLooseGameplayTag(knightGameplayTags::Window_Cancel);
		bCancelWindowApplied = true;
	}
	if (const ACharacter* const Character = Cast<ACharacter>(GetAvatarActorFromActorInfo()))
	{
		if (UknightMovementComponent* const KnightMovement = Cast<UknightMovementComponent>(Character->GetCharacterMovement()))
		{
			KnightMovement->SetMoveInputLocked(true);
			bMoveLockApplied = true;
		}
	}

	UAbilityTask_WaitDelay* const Task = UAbilityTask_WaitDelay::WaitDelay(this, RecoveryDuration);
	Task->OnFinish.AddDynamic(this, &ThisClass::OnRecoveryFinished);
	Task->ReadyForActivation();
	RecoveryTask = Task;

	// 대시 중에 미리 눌러 둔 대시(버퍼)가 있으면 바로 잇고, 없으면 경직 중 입력을 기다린다.
	if (!TryChainDash(true))
	{
		WaitForChainInput();
	}
}

void UknightGameplayAbility_BoostDash::OnRecoveryFinished()
{
	EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, false);
}

void UknightGameplayAbility_BoostDash::WaitForChainInput()
{
	// 대시가 실행 중이라 대시 키는 어빌리티 발동 대신 이 태스크로 들어온다 (UknightAbilitySystemComponent::AbilitySpecInputPressed).
	UAbilityTask_WaitInputPress* const Task = UAbilityTask_WaitInputPress::WaitInputPress(this);
	Task->OnPress.AddDynamic(this, &ThisClass::OnRecoveryInputPressed);
	Task->ReadyForActivation();
	ChainInputTask = Task;
}

void UknightGameplayAbility_BoostDash::OnRecoveryInputPressed(float TimeWaited)
{
	// 쿨타임 중이라 못 이었으면 다음 누름을 다시 기다린다.
	if (!TryChainDash(false))
	{
		WaitForChainInput();
	}
}

bool UknightGameplayAbility_BoostDash::TryChainDash(bool bRequireBufferedInput)
{
	if (!bInRecovery)
	{
		return false;
	}
	UAbilitySystemComponent* const ASC = GetAbilitySystemComponentFromActorInfo();
	ACharacter* const Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	const UWorld* const World = GetWorld();
	if (!ASC || !Character || !World)
	{
		return false;
	}

	UknightInputBufferComponent* const InputBuffer = Character->FindComponentByClass<UknightInputBufferComponent>();
	const FGameplayTag InputTag = GetInputTag();
	if (bRequireBufferedInput && !(InputBuffer && InputBuffer->HasBufferedInput(InputTag)))
	{
		return false;
	}

	// 첫 발동과 같은 기준: 쿨타임, 발동 차단 태그(사망·피격·과열), 게이지 비용.
	if (World->GetTimeSeconds() < CooldownEndTime || ASC->HasAnyMatchingGameplayTags(ActivationBlockedTags))
	{
		return false;
	}
	if (!CommitAbilityCost(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo()))
	{
		return false;
	}

	// 이 누름으로 이어졌으니 버퍼에서 지운다. 안 지우면 다음 경직 시작 때 같은 누름으로 한 번 더 이어질 수 있다.
	if (InputBuffer)
	{
		InputBuffer->ConsumeBufferedInput(InputTag);
	}
	if (RecoveryTask.IsValid())
	{
		RecoveryTask->EndTask();
	}
	if (ChainInputTask.IsValid())
	{
		ChainInputTask->EndTask();
	}
	ReleaseRecoveryState();
	StartDash(*Character);
	return true;
}

void UknightGameplayAbility_BoostDash::ReleaseRecoveryState()
{
	bInRecovery = false;
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
		if (UknightMovementComponent* const KnightMovement = Character ? Cast<UknightMovementComponent>(Character->GetCharacterMovement()) : nullptr)
		{
			KnightMovement->SetMoveInputLocked(false);
		}
	}
}

void UknightGameplayAbility_BoostDash::StartCooldown()
{
	if (!bCooldownPending)
	{
		return;
	}
	bCooldownPending = false;
	const float Cooldown = GetMechAttributeValue(UknightAttributeSet::GetDodgeCooldownAttribute(), 0.f);
	if (const UWorld* const World = GetWorld(); World && Cooldown > 0.f)
	{
		CooldownEndTime = World->GetTimeSeconds() + Cooldown;
	}
}

FGameplayTag UknightGameplayAbility_BoostDash::GetInputTag() const
{
	// 동적 소스 태그에는 InputTag만 들어 있다 (UknightAbilitySet 참고).
	const FGameplayAbilitySpec* const Spec = GetCurrentAbilitySpec();
	return Spec ? Spec->GetDynamicSpecSourceTags().First() : FGameplayTag();
}

void UknightGameplayAbility_BoostDash::OnInvulnerabilityEnded()
{
	RemoveInvulnerability();
}

void UknightGameplayAbility_BoostDash::RemoveInvulnerability()
{
	if (bInvulnerableApplied)
	{
		bInvulnerableApplied = false;
		if (UAbilitySystemComponent* const ASC = GetAbilitySystemComponentFromActorInfo())
		{
			ASC->RemoveLooseGameplayTag(knightGameplayTags::State_Invulnerable);
		}
	}
}

void UknightGameplayAbility_BoostDash::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	RemoveInvulnerability();
	ReleaseRecoveryState();

	// 이동이 끝나기 전에 끊긴 대시(피격 등)도 쿨타임은 건다. 비용 부족 등으로 시작도 못 했으면 걸지 않는다.
	StartCooldown();

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
