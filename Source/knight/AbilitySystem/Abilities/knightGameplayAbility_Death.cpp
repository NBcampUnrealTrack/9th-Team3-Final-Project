#include "AbilitySystem/Abilities/knightGameplayAbility_Death.h"

#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Animation/AnimMontage.h"
#include "Camera/CameraShakeBase.h"
#include "Components/MeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "System/knightGameplayTags.h"
#include "knight.h"

UknightGameplayAbility_Death::UknightGameplayAbility_Death(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ActivationGroup = EknightAbilityActivationGroup::Independent;
	SetAssetTags(FGameplayTagContainer(knightGameplayTags::Ability_Death));

	FAbilityTriggerData Trigger;
	Trigger.TriggerTag = knightGameplayTags::Event_Death;
	Trigger.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;
	AbilityTriggers.Add(Trigger);
}

void UknightGameplayAbility_Death::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	// The hit reaction is still running here: the character cancels it only after Event.Death is handled.
	const UAbilitySystemComponent* const ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	const bool bKnockedDown = ASC && ASC->HasMatchingGameplayTag(knightGameplayTags::State_HitReact_Knockdown);
	UAnimMontage* const Montage = bKnockedDown && KnockdownDeathMontage ? KnockdownDeathMontage.Get() : DeathMontage.Get();
	if (Montage)
	{
		// Not stopped when the ability ends, so the last frame holds even if the ability is cancelled later.
		UAbilityTask_PlayMontageAndWait* const Task = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
			this, NAME_None, Montage, 1.f, NAME_None, /*bStopWhenAbilityEnds*/ false);
		Task->ReadyForActivation();
		UE_LOG(LogknightAbility, Log, TEXT("%s: playing %s%s."), *GetName(), *GetNameSafe(Montage), bKnockedDown ? TEXT(" (knocked down)") : TEXT(""));
	}
	else if (!bExplodeOnDeath)
	{
		UE_LOG(LogknightAbility, Warning, TEXT("%s: no death montage; the Knight stays in its current pose."), *GetName());
	}

	// (2026-10-19) 쓰러지기 시작한 뒤 잠깐 있다가 폭발하며 사라진다.
	if (bExplodeOnDeath)
	{
		if (ExplodeDelay > 0.f)
		{
			UAbilityTask_WaitDelay* const Delay = UAbilityTask_WaitDelay::WaitDelay(this, ExplodeDelay);
			Delay->OnFinish.AddDynamic(this, &ThisClass::Explode);
			Delay->ReadyForActivation();
		}
		else
		{
			Explode();
		}
	}
}

void UknightGameplayAbility_Death::Explode()
{
	ACharacter* const Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	UWorld* const World = GetWorld();
	if (!Character || !World)
	{
		return;
	}
	const USkeletalMeshComponent* const Mesh = Character->GetMesh();
	const FVector Location = Mesh && Mesh->DoesSocketExist(ExplosionSocket) ? Mesh->GetSocketLocation(ExplosionSocket) : Character->GetActorLocation();

	for (UNiagaraSystem* const Effect : ExplosionEffects)
	{
		if (Effect)
		{
			UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, Effect, Location, Character->GetActorRotation(), FVector(ExplosionScale), true, true);
		}
	}
	if (ExplosionSound)
	{
		UGameplayStatics::PlaySoundAtLocation(World, ExplosionSound, Location);
	}
	if (ExplosionShake && ExplosionShakeScale > 0.f)
	{
		if (APlayerController* const PC = Cast<APlayerController>(Character->GetController()))
		{
			PC->ClientStartCameraShake(ExplosionShake, ExplosionShakeScale);
		}
	}

	// 메카를 숨긴다: 본체·부위 메시·파츠·무기까지 액터의 모든 메시. 캡슐·카메라는 그대로 둔다.
	TArray<UMeshComponent*> Meshes;
	Character->GetComponents<UMeshComponent>(Meshes);
	for (UMeshComponent* const MeshComponent : Meshes)
	{
		MeshComponent->SetHiddenInGame(true, true);
	}
	UE_LOG(LogknightAbility, Log, TEXT("%s: exploded at %s (%d effects, %d meshes hidden)."), *GetNameSafe(Character), *Location.ToCompactString(),
		ExplosionEffects.Num(), Meshes.Num());
}
