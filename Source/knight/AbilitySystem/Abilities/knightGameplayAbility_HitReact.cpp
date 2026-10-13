#include "AbilitySystem/Abilities/knightGameplayAbility_HitReact.h"

#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_ApplyRootMotionConstantForce.h"
#include "Camera/CameraShakeBase.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/RootMotionSource.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "Animation/AnimMontage.h"
#include "GameFramework/Actor.h"
#include "GameplayEffect.h"
#include "System/knightGameplayTags.h"
#include "knight.h"

UAnimMontage* FknightDirectionalMontages::Pick(const FVector& AvatarForward, const FVector& AvatarRight, const FVector& ToInstigator) const
{
	const FVector Direction = ToInstigator.GetSafeNormal2D();
	const float ForwardDot = FVector::DotProduct(AvatarForward.GetSafeNormal2D(), Direction);
	const float RightDot = FVector::DotProduct(AvatarRight.GetSafeNormal2D(), Direction);

	UAnimMontage* Chosen = nullptr;
	if (FMath::Abs(ForwardDot) >= FMath::Abs(RightDot))
	{
		Chosen = ForwardDot >= 0.f ? Front.Get() : Back.Get();
	}
	else
	{
		Chosen = RightDot >= 0.f ? Right.Get() : Left.Get();
	}
	return Chosen ? Chosen : Front.Get();
}

UknightGameplayAbility_HitReact::UknightGameplayAbility_HitReact(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ActivationGroup = EknightAbilityActivationGroup::Independent;
	bRetriggerInstancedAbility = true;

	SetAssetTags(FGameplayTagContainer(knightGameplayTags::Ability_HitReact));
	ActivationBlockedTags.AddTag(knightGameplayTags::State_Dead);

	FGameplayTagContainer Interrupted;
	Interrupted.AddTag(knightGameplayTags::Ability_Attack);
	Interrupted.AddTag(knightGameplayTags::Ability_Guard);
	Interrupted.AddTag(knightGameplayTags::Ability_Boost_Dash);
	Interrupted.AddTag(knightGameplayTags::Ability_Boost_Flight);
	CancelAbilitiesWithTag.AppendTags(Interrupted);
	BlockAbilitiesWithTag.AppendTags(Interrupted);

	for (const FGameplayTag& EventTag : { knightGameplayTags::Event_Damaged.GetTag(), knightGameplayTags::Event_PoiseBroken.GetTag(), knightGameplayTags::Event_Parried.GetTag() })
	{
		FAbilityTriggerData Trigger;
		Trigger.TriggerTag = EventTag;
		Trigger.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;
		AbilityTriggers.Add(Trigger);
	}
}

bool UknightGameplayAbility_HitReact::ShouldAbilityRespondToEvent(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayEventData* Payload) const
{
	if (!Super::ShouldAbilityRespondToEvent(ActorInfo, Payload) || !Payload || !ActorInfo)
	{
		return false;
	}
	if (Payload->EventTag != knightGameplayTags::Event_Damaged)
	{
		return true;
	}
	// Flinch filters. Stagger, knockdown and parry recoil always play.
	const UAbilitySystemComponent* const ASC = ActorInfo->AbilitySystemComponent.Get();
	if (!ASC || Payload->EventMagnitude < MinFlinchDamage)
	{
		return false;
	}
	return !ASC->HasMatchingGameplayTag(knightGameplayTags::State_SuperArmor)
		&& !ASC->HasMatchingGameplayTag(knightGameplayTags::State_HitReact_Stagger)
		&& !ASC->HasMatchingGameplayTag(knightGameplayTags::State_HitReact_Knockdown);
}

EknightHitReactType UknightGameplayAbility_HitReact::ResolveType(const FGameplayEventData& Payload) const
{
	if (Payload.EventTag == knightGameplayTags::Event_PoiseBroken)
	{
		return Payload.EventMagnitude >= KnockdownPoiseDamage ? EknightHitReactType::Knockdown : EknightHitReactType::Stagger;
	}
	if (Payload.EventTag == knightGameplayTags::Event_Parried)
	{
		return EknightHitReactType::Stagger;
	}
	return EknightHitReactType::Flinch;
}

const FknightDirectionalMontages& UknightGameplayAbility_HitReact::GetMontages(EknightHitReactType Type) const
{
	switch (Type)
	{
	case EknightHitReactType::Stagger:
		return StaggerMontages;
	case EknightHitReactType::Knockdown:
		return KnockdownMontages;
	default:
		return FlinchMontages;
	}
}

FGameplayTag UknightGameplayAbility_HitReact::GetStateTag(EknightHitReactType Type)
{
	switch (Type)
	{
	case EknightHitReactType::Stagger:
		return knightGameplayTags::State_HitReact_Stagger;
	case EknightHitReactType::Knockdown:
		return knightGameplayTags::State_HitReact_Knockdown;
	default:
		return knightGameplayTags::State_HitReact_Flinch;
	}
}

void UknightGameplayAbility_HitReact::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	AActor* const Avatar = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	UAbilitySystemComponent* const ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	if (!TriggerEventData || !Avatar || !ASC)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	ActiveType = ResolveType(*TriggerEventData);
	ASC->AddLooseGameplayTag(GetStateTag(ActiveType));
	bStateTagApplied = true;

	const AActor* const EventInstigator = TriggerEventData->Instigator.Get();
	ApplyImpactFeel(*Avatar, EventInstigator);
	const FVector ToInstigator = EventInstigator ? EventInstigator->GetActorLocation() - Avatar->GetActorLocation() : Avatar->GetActorForwardVector();
	UAnimMontage* const Montage = GetMontages(ActiveType).Pick(Avatar->GetActorForwardVector(), Avatar->GetActorRightVector(), ToInstigator);
	if (!Montage)
	{
		UE_LOG(LogknightAbility, Verbose, TEXT("%s: no montage for hit reaction %d."), *GetName(), static_cast<int32>(ActiveType));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	UAbilityTask_PlayMontageAndWait* const Task = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, Montage);
	Task->OnCompleted.AddDynamic(this, &ThisClass::OnMontageFinished);
	Task->OnBlendOut.AddDynamic(this, &ThisClass::OnMontageFinished);
	Task->OnInterrupted.AddDynamic(this, &ThisClass::OnMontageFinished);
	Task->OnCancelled.AddDynamic(this, &ThisClass::OnMontageFinished);
	Task->ReadyForActivation();
}

const FknightHitImpactFeel& UknightGameplayAbility_HitReact::GetImpactFeel(EknightHitReactType Type) const
{
	switch (Type)
	{
	case EknightHitReactType::Stagger:
		return StaggerImpact;
	case EknightHitReactType::Knockdown:
		return KnockdownImpact;
	default:
		return FlinchImpact;
	}
}

void UknightGameplayAbility_HitReact::ApplyImpactFeel(AActor& Avatar, const AActor* EventInstigator)
{
	const FknightHitImpactFeel& Feel = GetImpactFeel(ActiveType);

	// 넉백: 때린 쪽에서 멀어지는 방향. 몽타주 루트 모션에 더해지도록 더하기(Additive)로 건다.
	if (Feel.KnockbackDistance > 0.f && EventInstigator)
	{
		const FVector Away = (Avatar.GetActorLocation() - EventInstigator->GetActorLocation()).GetSafeNormal2D();
		if (!Away.IsNearlyZero())
		{
			const float Time = FMath::Max(Feel.KnockbackTime, 0.02f);
			UAbilityTask_ApplyRootMotionConstantForce* const Task = UAbilityTask_ApplyRootMotionConstantForce::ApplyRootMotionConstantForce(
				this, TEXT("HitKnockback"), Away, Feel.KnockbackDistance / Time, Time, true, nullptr,
				ERootMotionFinishVelocityMode::SetVelocity, FVector::ZeroVector, 0.f, true);
			Task->ReadyForActivation();
		}
	}

	// 흔들림·슬로는 플레이어가 맞았을 때만.
	const APawn* const Pawn = Cast<APawn>(&Avatar);
	if (!Pawn || !Pawn->IsPlayerControlled())
	{
		return;
	}
	if (Feel.CameraShake && Feel.CameraShakeScale > 0.f)
	{
		if (APlayerController* const PC = Cast<APlayerController>(Pawn->GetController()))
		{
			PC->ClientStartCameraShake(Feel.CameraShake, Feel.CameraShakeScale);
		}
	}
	UWorld* const World = Avatar.GetWorld();
	if (World && Feel.SlowMoDuration > 0.f && Feel.SlowMoDilation < 1.f)
	{
		UGameplayStatics::SetGlobalTimeDilation(World, Feel.SlowMoDilation);
		// 타이머도 느려진 시간으로 세므로, 실제 시간 SlowMoDuration에 맞추려고 배율을 곱한다.
		FTimerHandle RestoreTimer;
		World->GetTimerManager().SetTimer(RestoreTimer, FTimerDelegate::CreateWeakLambda(World, [World]()
		{
			UGameplayStatics::SetGlobalTimeDilation(World, 1.f);
		}), Feel.SlowMoDuration * Feel.SlowMoDilation, false);
	}
}

void UknightGameplayAbility_HitReact::OnMontageFinished()
{
	EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, false);
}

void UknightGameplayAbility_HitReact::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (bStateTagApplied)
	{
		bStateTagApplied = false;
		if (UAbilitySystemComponent* const ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr)
		{
			ASC->RemoveLooseGameplayTag(GetStateTag(ActiveType));
			if (ActiveType != EknightHitReactType::Flinch && PoiseRecoveryEffect && !ASC->HasMatchingGameplayTag(knightGameplayTags::State_Dead))
			{
				ASC->ApplyGameplayEffectToSelf(PoiseRecoveryEffect->GetDefaultObject<UGameplayEffect>(), 1.f, ASC->MakeEffectContext());
			}
		}
	}
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
