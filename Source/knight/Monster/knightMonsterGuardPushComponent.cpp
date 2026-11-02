#include "Monster/knightMonsterGuardPushComponent.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "knight.h"

namespace knightMonsterGuardPush
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_GuardPushed, "Event.GuardPushed", "A guarded monster hit pushed the defender back (magnitude 1 light, 2 heavy).");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_GuardBroken, "Event.GuardBroken", "A guarded monster hit broke the defender's guard.");
}

UknightMonsterGuardPushComponent::UknightMonsterGuardPushComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

bool UknightMonsterGuardPushComponent::HandleHit(const AActor* Monster, AActor& Target, EknightHitOutcome Outcome,
	EknightMonsterGuardReaction Reaction, float ExistingPushDistance)
{
	if (Outcome != EknightHitOutcome::Guarded || !Monster)
	{
		return false;
	}
	UknightMonsterGuardPushComponent* const Component = Monster->FindComponentByClass<UknightMonsterGuardPushComponent>();
	return Component && Component->OnGuarded(Target, Reaction, ExistingPushDistance);
}

float UknightMonsterGuardPushComponent::GetPushDistance(EknightMonsterGuardReaction Reaction) const
{
	switch (Reaction)
	{
	case EknightMonsterGuardReaction::Sustained: return SustainedDistance;
	case EknightMonsterGuardReaction::Light: return LightDistance;
	case EknightMonsterGuardReaction::Medium: return MediumDistance;
	case EknightMonsterGuardReaction::Heavy: return HeavyDistance;
	case EknightMonsterGuardReaction::Break: return BreakDistance;
	default: return 0.f;
	}
}

bool UknightMonsterGuardPushComponent::OnGuarded(AActor& Target, EknightMonsterGuardReaction Reaction, float ExistingPushDistance)
{
	AActor* const Monster = GetOwner();
	if (UAbilitySystemComponent* const TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(&Target))
	{
		FGameplayEventData Payload;
		Payload.Instigator = Monster;
		Payload.Target = &Target;
		if (Reaction == EknightMonsterGuardReaction::Break)
		{
			Payload.EventTag = knightMonsterGuardPush::Event_GuardBroken;
		}
		else
		{
			Payload.EventTag = knightMonsterGuardPush::Event_GuardPushed;
			Payload.EventMagnitude = Reaction <= EknightMonsterGuardReaction::Light ? 1.f : 2.f;
		}
		TargetASC->HandleGameplayEvent(Payload.EventTag, &Payload);
	}

	const float Distance = GetPushDistance(Reaction);
	const bool bPush = Monster && Distance > 0.f && Distance > ExistingPushDistance;
	UE_LOG(LogknightAI, Verbose, TEXT("%s: guarded by %s, reaction %d, push %.0f cm (attack knockback %.0f cm)%s."), *GetNameSafe(Monster), *Target.GetName(),
		static_cast<int32>(Reaction), bPush ? Distance : 0.f, ExistingPushDistance, Reaction == EknightMonsterGuardReaction::Break ? TEXT(", guard broken") : TEXT(""));
	if (!bPush)
	{
		return false;
	}
	FPush* Push = Pushes.FindByPredicate([&Target](const FPush& Entry) { return Entry.Target.Get() == &Target; });
	if (!Push)
	{
		Push = &Pushes.AddDefaulted_GetRef();
		Push->Target = &Target;
	}
	Push->Knockback.StartAway(*Monster, Target, Distance / PushTime, PushTime);
	Push->Remaining = PushTime;
	SetComponentTickEnabled(true);
	return true;
}

void UknightMonsterGuardPushComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// DeltaTime은 몬스터 시간이다. 밀림은 대상 시간으로 센다.
	const float WorldDelta = GetWorld() ? GetWorld()->GetDeltaSeconds() : DeltaTime;
	for (int32 Index = Pushes.Num() - 1; Index >= 0; --Index)
	{
		FPush& Push = Pushes[Index];
		const AActor* const Target = Push.Target.Get();
		if (Target)
		{
			const float Step = WorldDelta * FMath::Max(Target->CustomTimeDilation, 0.f);
			Push.Knockback.Tick(Step);
			Push.Remaining -= Step;
		}
		if (!Target || Push.Remaining <= 0.f)
		{
			Pushes.RemoveAtSwap(Index);
		}
	}
	if (Pushes.IsEmpty())
	{
		SetComponentTickEnabled(false);
	}
}
