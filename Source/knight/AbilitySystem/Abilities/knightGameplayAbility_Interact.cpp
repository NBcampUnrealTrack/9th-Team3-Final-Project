#include "AbilitySystem/Abilities/knightGameplayAbility_Interact.h"

#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Abilities/Tasks/AbilityTask_WaitInputRelease.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "Interfaces/InteractableInterface.h"
#include "NativeGameplayTags.h"
#include "System/knightGameplayTags.h"
#include "knight.h"

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Ability_Interact, "Ability.Interact");

UknightGameplayAbility_Interact::UknightGameplayAbility_Interact(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ActivationPolicy = EknightAbilityActivationPolicy::OnInputTriggered;
	ActivationGroup = EknightAbilityActivationGroup::Independent;
	SetAssetTags(FGameplayTagContainer(TAG_Ability_Interact));
	ActivationBlockedTags.AddTag(knightGameplayTags::State_Dead);
	ActivationBlockedTags.AddTag(knightGameplayTags::State_HitReact);
}

AActor* UknightGameplayAbility_Interact::FindBestInteractable(AActor* Interactor, float Radius, float MaxAngle)
{
	UWorld* const World = Interactor ? Interactor->GetWorld() : nullptr;
	if (!World)
	{
		return nullptr;
	}
	const FVector Origin = Interactor->GetActorLocation();
	const FVector Forward = Interactor->GetActorForwardVector().GetSafeNormal2D();
	const float CosMax = FMath::Cos(FMath::DegreesToRadians(MaxAngle));

	TArray<FOverlapResult> Overlaps;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(knightInteract), false, Interactor);
	FCollisionObjectQueryParams ObjectParams(FCollisionObjectQueryParams::AllObjects);
	World->OverlapMultiByObjectType(Overlaps, Origin, FQuat::Identity, ObjectParams, FCollisionShape::MakeSphere(Radius), Params);

	AActor* Best = nullptr;
	float BestScore = TNumericLimits<float>::Max();
	TSet<AActor*> Seen;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* const Actor = Overlap.GetActor();
		if (!Actor || Seen.Contains(Actor) || !Actor->GetClass()->ImplementsInterface(UInteractableInterface::StaticClass()))
		{
			continue;
		}
		Seen.Add(Actor);
		if (!IInteractableInterface::Execute_CanInteract(Actor, Interactor))
		{
			continue;
		}
		FVector To = Actor->GetActorLocation() - Origin;
		To.Z = 0.f;
		const float Distance = To.Size();
		const float Cos = Distance > KINDA_SMALL_NUMBER ? FVector::DotProduct(To / Distance, Forward) : 1.f;
		if (Cos < CosMax && Distance > 60.f)
		{
			continue;
		}
		// 가까울수록, 정면일수록 우선.
		const float Score = Distance * (2.f - Cos);
		if (Score < BestScore)
		{
			BestScore = Score;
			Best = Actor;
		}
	}
	return Best;
}

void UknightGameplayAbility_Interact::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	bDone = false;
	AActor* const Avatar = GetAvatarActorFromActorInfo();
	AActor* const Found = FindBestInteractable(Avatar, InteractRadius, MaxInteractAngle);
	if (!Found || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	Target = Found;

	const float Duration = IInteractableInterface::Execute_GetInteractionDuration(Found);
	if (Duration <= 0.f)
	{
		OnHoldFinished();
		return;
	}
	UE_LOG(LogknightAbility, Log, TEXT("Interact: hold %.1fs for %s"), Duration, *GetNameSafe(Found));
	UAbilityTask_WaitInputRelease* const Release = UAbilityTask_WaitInputRelease::WaitInputRelease(this, true);
	Release->OnRelease.AddDynamic(this, &ThisClass::OnHoldReleased);
	Release->ReadyForActivation();
	UAbilityTask_WaitDelay* const Hold = UAbilityTask_WaitDelay::WaitDelay(this, Duration);
	Hold->OnFinish.AddDynamic(this, &ThisClass::OnHoldFinished);
	Hold->ReadyForActivation();
}

void UknightGameplayAbility_Interact::OnHoldFinished()
{
	if (bDone)
	{
		return;
	}
	AActor* const Found = Target.Get();
	AActor* const Avatar = GetAvatarActorFromActorInfo();
	if (Found && Avatar && IInteractableInterface::Execute_CanInteract(Found, Avatar))
	{
		UE_LOG(LogknightAbility, Log, TEXT("Interact: %s"), *GetNameSafe(Found));
		IInteractableInterface::Execute_Interact(Found, Avatar);
		Finish(false);
		return;
	}
	Finish(true);
}

void UknightGameplayAbility_Interact::OnHoldReleased(float TimeHeld)
{
	if (!bDone)
	{
		Finish(true);
	}
}

void UknightGameplayAbility_Interact::Finish(bool bWasCancelled)
{
	bDone = true;
	if (IsActive())
	{
		EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, bWasCancelled);
	}
}
