#include "AbilitySystem/Abilities/knightGameplayAbility_SkillArea.h"

#include "Camera/CameraShakeBase.h"
#include "Combat/knightMeleeTraceComponent.h"
#include "Components/CapsuleComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "TimerManager.h"

UknightGameplayAbility_SkillArea::UknightGameplayAbility_SkillArea(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UknightGameplayAbility_SkillArea::StartSkill()
{
	UWorld* const World = GetWorld();
	if (!SkillMontage || !World)
	{
		EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, true);
		return;
	}
	ListenForMeleeHits(bHitOncePerActor);
	const float Rate = FMath::Max(PlayRate * GetAttackSpeed(), 0.01f);
	PulseTimers.SetNum(Pulses.Num());
	for (int32 Index = 0; Index < Pulses.Num(); ++Index)
	{
		const float Delay = Pulses[Index].Time / Rate;
		if (Delay <= 0.f)
		{
			FirePulse(Index);
			continue;
		}
		World->GetTimerManager().SetTimer(PulseTimers[Index], FTimerDelegate::CreateUObject(this, &ThisClass::FirePulse, Index), Delay, false);
	}
	PlaySkillMontage(SkillMontage, PlayRate);
}

void UknightGameplayAbility_SkillArea::FirePulse(int32 Index)
{
	ACharacter* const Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	UWorld* const World = GetWorld();
	if (!IsActive() || !Character || !World || !Pulses.IsValidIndex(Index))
	{
		return;
	}
	const FknightSkillAreaPulse& Pulse = Pulses[Index];
	const FVector Center = Character->GetActorTransform().TransformPosition(Pulse.Offset);
	if (UknightMeleeTraceComponent* const Trace = Character->FindComponentByClass<UknightMeleeTraceComponent>())
	{
		Trace->HitArea(Center, Pulse.Radius);
	}
	if (bDrawPulses)
	{
		DrawDebugSphere(World, Center, Pulse.Radius, 16, FColor::Orange, false, 0.5f);
	}
	if (!Pulse.bPlayEffect)
	{
		return;
	}
	if (PulseEffect)
	{
		// 바닥(캡슐 아래)에 낸다.
		const float HalfHeight = Character->GetCapsuleComponent() ? Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 90.f;
		const FVector Ground(Center.X, Center.Y, Character->GetActorLocation().Z - HalfHeight);
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, PulseEffect, Ground, Character->GetActorRotation(), FVector(PulseEffectScale),
			true, true, ENCPoolMethod::AutoRelease);
	}
	if (PulseShake)
	{
		if (APlayerController* const PC = Cast<APlayerController>(Character->GetController()))
		{
			PC->ClientStartCameraShake(PulseShake, PulseShakeScale);
		}
	}
}

void UknightGameplayAbility_SkillArea::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (UWorld* const World = GetWorld())
	{
		for (FTimerHandle& Timer : PulseTimers)
		{
			World->GetTimerManager().ClearTimer(Timer);
		}
	}
	PulseTimers.Reset();
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
