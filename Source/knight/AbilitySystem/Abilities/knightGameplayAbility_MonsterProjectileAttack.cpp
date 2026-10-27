#include "AbilitySystem/Abilities/knightGameplayAbility_MonsterProjectileAttack.h"

#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_Repeat.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Monster/knightMonsterCharacterBase.h"
#include "Monster/knightMonsterMovementComponent.h"
#include "Monster/knightMonsterSonicProjectile.h"
#include "Monster/knightMonsterTargetingComponent.h"
#include "Sound/SoundBase.h"
#include "System/knightGameplayTags.h"
#include "knight.h"

namespace knightMonsterProjectile
{
	constexpr float TickInterval = 1.f / 30.f;
	constexpr int32 MaxTicks = 100000;
}

UknightGameplayAbility_MonsterProjectileAttack::UknightGameplayAbility_MonsterProjectileAttack(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Selection.MinRange = 800.f;
	Selection.MaxRange = 3000.f;
	Selection.MaxAngle = 180.f;
}

void UknightGameplayAbility_MonsterProjectileAttack::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	const UAbilitySystemComponent* const ASC = GetAbilitySystemComponentFromActorInfo();
	const bool bEnraged = ASC && ASC->HasMatchingGameplayTag(knightGameplayTags::State_Monster_Enraged);
	ShotsLeft = FMath::Max(1, bEnraged ? EnragedShotCount : ShotCount);
	ShotEffect = bEnraged && EnragedProjectileEffect ? EnragedProjectileEffect.Get() : ProjectileEffect.Get();
	Elapsed = 0.f;
	NextShotTime = FireTime;
	EndTime = FireTime + (ShotsLeft - 1) * ShotInterval + RecoveryTime;

	if (Montage)
	{
		UAbilityTask_PlayMontageAndWait* const MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, Montage);
		MontageTask->ReadyForActivation();
	}
	UAbilityTask_Repeat* const Task = UAbilityTask_Repeat::RepeatAction(this, knightMonsterProjectile::TickInterval, knightMonsterProjectile::MaxTicks);
	Task->OnPerformAction.AddDynamic(this, &ThisClass::HandleTick);
	TickTask = Task;
	Task->ReadyForActivation();
}

void UknightGameplayAbility_MonsterProjectileAttack::HandleTick(int32 ActionNumber)
{
	Elapsed += knightMonsterProjectile::TickInterval;
	const AknightMonsterCharacterBase* const Monster = Cast<AknightMonsterCharacterBase>(GetAvatarActorFromActorInfo());
	const UknightMonsterTargetingComponent* const Targeting = Monster ? Monster->GetTargetingComponent() : nullptr;
	const AActor* const Target = Targeting ? Targeting->GetCurrentTarget() : nullptr;
	if (ShotsLeft > 0 && Target)
	{
		if (UknightMonsterMovementComponent* const Movement = Monster->GetMonsterMovementComponent())
		{
			Movement->TurnTowards(Target->GetActorLocation(), knightMonsterProjectile::TickInterval, AimTurnRate, 2.f);
		}
	}
	while (ShotsLeft > 0 && Elapsed >= NextShotTime)
	{
		Fire();
		--ShotsLeft;
		NextShotTime += ShotInterval;
	}
	if (Elapsed >= EndTime)
	{
		if (TickTask.IsValid())
		{
			TickTask->EndTask();
		}
		EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, false);
	}
}

void UknightGameplayAbility_MonsterProjectileAttack::Fire()
{
	AknightMonsterCharacterBase* const Monster = Cast<AknightMonsterCharacterBase>(GetAvatarActorFromActorInfo());
	UWorld* const World = GetWorld();
	if (!Monster || !World)
	{
		return;
	}
	USkeletalMeshComponent* const Mesh = Monster->GetMesh();
	const FVector Anchor = Mesh && Mesh->DoesSocketExist(MuzzleSocket) ? Mesh->GetSocketLocation(MuzzleSocket) : Monster->GetActorLocation();
	const FVector Muzzle = Anchor + Monster->GetActorQuat().RotateVector(MuzzleOffset);

	// 쏘는 순간의 타깃 몸통(캡슐 중심)을 겨눈다. 타깃이 없으면 정면 아래로 쏜다.
	const UknightMonsterTargetingComponent* const Targeting = Monster->GetTargetingComponent();
	const AActor* const Target = Targeting ? Targeting->GetCurrentTarget() : nullptr;
	const FVector Aim = Target ? Target->GetActorLocation() : Muzzle + Monster->GetActorForwardVector() * 1000.f - FVector(0.f, 0.f, 500.f);

	FActorSpawnParameters Params;
	Params.Owner = Monster;
	Params.Instigator = Monster;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AknightMonsterSonicProjectile* const Projectile = World->SpawnActor<AknightMonsterSonicProjectile>(Muzzle, (Aim - Muzzle).Rotation(), Params);
	if (Projectile)
	{
		Projectile->SetDeafenTime(DeafenTime);
		Projectile->SetImpactEffects(ImpactEffects);
		Projectile->Launch(Monster, HitParams, Aim - Muzzle, ProjectileSpeed, ProjectileRadius, MaxDistance, ShotEffect);
	}
	if (FireSound && Mesh)
	{
		UGameplayStatics::SpawnSoundAttached(FireSound, Mesh, MuzzleSocket);
	}
	UE_LOG(LogknightAI, Log, TEXT("%s: %s fires (%.0f cm to target)."), *GetNameSafe(Monster), *GetName(), FVector::Dist(Muzzle, Aim));
}
