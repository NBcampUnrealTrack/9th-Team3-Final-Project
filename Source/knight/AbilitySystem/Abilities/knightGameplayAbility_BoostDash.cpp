#include "AbilitySystem/Abilities/knightGameplayAbility_BoostDash.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystem/Attributes/knightAttributeSet.h"
#include "Abilities/Tasks/AbilityTask_ApplyRootMotionConstantForce.h"
#include "Character/knightMovementComponent.h"
#include "Engine/World.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/RootMotionSource.h"
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
	bDashStarted = true;

	FVector Direction = Character->GetCharacterMovement()->GetLastInputVector().GetSafeNormal2D();
	const bool bHasInput = !Direction.IsNearlyZero();
	if (!bHasInput)
	{
		Direction = -Character->GetActorForwardVector().GetSafeNormal2D();
	}

	const UAbilitySystemComponent* const OwnerASC = GetAbilitySystemComponentFromActorInfo();
	const bool bLockedOn = OwnerASC && OwnerASC->HasMatchingGameplayTag(knightGameplayTags::State_LockedOn);
	if (bFaceDashDirectionWhenFree && bHasInput && !bLockedOn)
	{
		Character->SetActorRotation(FRotator(0.f, Direction.Rotation().Yaw, 0.f));
	}
	UAnimMontage* const Montage = SelectDashMontage(*Character, Direction);

	if (InvulnerableDuration > 0.f)
	{
		if (UAbilitySystemComponent* const ASC = GetAbilitySystemComponentFromActorInfo())
		{
			ASC->AddLooseGameplayTag(knightGameplayTags::State_Invulnerable);
			bInvulnerableApplied = true;
		}
		UAbilityTask_WaitDelay* const InvulnerableTask = UAbilityTask_WaitDelay::WaitDelay(this, InvulnerableDuration);
		InvulnerableTask->OnFinish.AddDynamic(this, &ThisClass::OnInvulnerabilityEnded);
		InvulnerableTask->ReadyForActivation();
	}

	if (Montage)
	{
		UAbilityTask_PlayMontageAndWait* const MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
			this, NAME_None, Montage, 1.f, NAME_None, true, 0.f);
		MontageTask->ReadyForActivation();
	}

	// 거리 = 기본 거리 x 퀵스텝 속도 배율 x 중량 계수. 시간은 고정이라 거리가 늘면 속도도 빨라진다.
	const UknightMovementComponent* const KnightMovement = Cast<UknightMovementComponent>(Character->GetCharacterMovement());
	const float WeightFactor = KnightMovement ? KnightMovement->GetWeightSpeedFactor() : 1.f;
	const float Distance = DashDistance * GetMechAttributeValue(UknightAttributeSet::GetDodgeSpeedAttribute(), 1.f) * WeightFactor;
	const float Strength = Distance / DashDuration;
	UAbilityTask_ApplyRootMotionConstantForce* const DashTask = UAbilityTask_ApplyRootMotionConstantForce::ApplyRootMotionConstantForce(
		this, TEXT("BoostDash"), Direction, Strength, DashDuration, false, StrengthOverTime,
		ERootMotionFinishVelocityMode::ClampVelocity, FVector::ZeroVector, Character->GetCharacterMovement()->GetMaxSpeed(), false);
	DashTask->OnFinish.AddDynamic(this, &ThisClass::OnDashFinished);
	DashTask->ReadyForActivation();

	if (UAbilitySystemComponent* const ASC = GetAbilitySystemComponentFromActorInfo())
	{
		FGameplayCueParameters CueParams;
		CueParams.Location = Character->GetActorLocation();
		CueParams.Normal = Direction;
		CueParams.Instigator = Character;
		ASC->ExecuteGameplayCue(knightGameplayTags::GameplayCue_Boost_Dash, CueParams);
	}

	// 추진기는 시끄럽다: 아직 나이트를 못 본 몬스터도 소리를 들을 수 있다 (AI 청각).
	UAISense_Hearing::ReportNoiseEvent(Character, Character->GetActorLocation(), NoiseLoudness, Character, NoiseRange, TEXT("Boost"));
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
	EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, false);
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

	// 실제로 대시한 경우에만, 끝난 시점부터 쿨타임을 센다 (피격 등으로 중간에 끊겨도 적용).
	if (bDashStarted)
	{
		bDashStarted = false;
		const float Cooldown = GetMechAttributeValue(UknightAttributeSet::GetDodgeCooldownAttribute(), 0.f);
		if (const UWorld* const World = GetWorld(); World && Cooldown > 0.f)
		{
			CooldownEndTime = World->GetTimeSeconds() + Cooldown;
		}
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
