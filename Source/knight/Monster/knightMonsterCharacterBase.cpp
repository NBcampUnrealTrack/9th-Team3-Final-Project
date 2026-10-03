#include "Monster/knightMonsterCharacterBase.h"

#include "AIController.h"
#include "BrainComponent.h"
#include "AbilitySystem/Attributes/knightCoreAttributeSet.h"
#include "AbilitySystem/Attributes/knightMonsterAttributeSet.h"
#include "AbilitySystem/knightAbilitySystemComponent.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "Combat/knightHitReactionComponent.h"
#include "Combat/knightHitStopComponent.h"
#include "Combat/knightMeleeTraceComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameplayEffect.h"
#include "Monster/knightMonsterDefinition.h"
#include "Monster/knightMonsterMovementComponent.h"
#include "Monster/knightMonsterPartComponent.h"
#include "Monster/knightMonsterTargetingComponent.h"
#include "MotionWarpingComponent.h"
#include "Perception/AIPerceptionComponent.h"
#include "System/knightGameplayTags.h"
#include "System/knightTeams.h"
#include "knight.h"
#include "knightPerfCounters.h"

AknightMonsterCharacterBase::AknightMonsterCharacterBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UknightMonsterMovementComponent>(ACharacter::CharacterMovementComponentName))
{
	// Movement-only capsule. Size it to the monster's footprint in the Blueprint, not its whole body.
	GetCapsuleComponent()->InitCapsuleSize(120.f, 200.f);

	bUseControllerRotationYaw = false;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	// Weapon traces hit the physics asset bodies, not the capsule.
	GetMesh()->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	GetMesh()->SetCollisionObjectType(ECC_PhysicsBody);
	GetMesh()->SetGenerateOverlapEvents(false);

	AbilitySystemComponent = CreateDefaultSubobject<UknightAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	CoreAttributeSet = CreateDefaultSubobject<UknightCoreAttributeSet>(TEXT("CoreAttributeSet"));
	MonsterAttributeSet = CreateDefaultSubobject<UknightMonsterAttributeSet>(TEXT("MonsterAttributeSet"));

	PartComponent = CreateDefaultSubobject<UknightMonsterPartComponent>(TEXT("PartComponent"));
	TargetingComponent = CreateDefaultSubobject<UknightMonsterTargetingComponent>(TEXT("TargetingComponent"));
	MeleeTrace = CreateDefaultSubobject<UknightMeleeTraceComponent>(TEXT("MeleeTrace"));
	HitStop = CreateDefaultSubobject<UknightHitStopComponent>(TEXT("HitStop"));
	HitReaction = CreateDefaultSubobject<UknightHitReactionComponent>(TEXT("HitReaction"));
	MotionWarping = CreateDefaultSubobject<UMotionWarpingComponent>(TEXT("MotionWarping"));
}

UAbilitySystemComponent* AknightMonsterCharacterBase::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

UknightMonsterMovementComponent* AknightMonsterCharacterBase::GetMonsterMovementComponent() const
{
	return Cast<UknightMonsterMovementComponent>(GetCharacterMovement());
}

void AknightMonsterCharacterBase::BeginPlay()
{
	Super::BeginPlay();

	CoreAttributeSet->OnOutOfHealth.AddUObject(this, &ThisClass::HandleOutOfHealth);
	MonsterAttributeSet->OnRageMaxed.AddUObject(this, &ThisClass::HandleRageMaxed);
	MonsterAttributeSet->OnStaminaDepleted.AddUObject(this, &ThisClass::HandleStaminaDepleted);

	// Placed monsters may begin play before possession; parts and threat work either way.
	InitializeAbilitySystem();
	SetCountedAlive(true);
}

void AknightMonsterCharacterBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	SetCountedAlive(false);
	Super::EndPlay(EndPlayReason);
}

void AknightMonsterCharacterBase::SetCountedAlive(bool bAlive)
{
	static int32 AliveMonsters = 0;
	if (bCountedAlive == bAlive)
	{
		return;
	}
	bCountedAlive = bAlive;
	AliveMonsters += bAlive ? 1 : -1;
	KNIGHT_PERF_GAUGE("Monster.Alive", AliveMonsters);
}

void AknightMonsterCharacterBase::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	InitializeAbilitySystem();
}

void AknightMonsterCharacterBase::InitializeAbilitySystem()
{
	AbilitySystemComponent->InitAbilityActorInfo(this, this);
	if (bAbilitySystemInitialized)
	{
		return;
	}
	bAbilitySystemInitialized = true;

	PartComponent->InitializeParts(MonsterDefinition ? MonsterDefinition->PartData.Get() : nullptr, AbilitySystemComponent);
	TargetingComponent->InitializeTargeting(AbilitySystemComponent);

	if (!MonsterDefinition)
	{
		UE_LOG(LogknightAI, Warning, TEXT("%s has no MonsterDefinition."), *GetNameSafe(this));
		return;
	}
	AbilitySystemComponent->SetTagRelationshipMapping(MonsterDefinition->TagRelationshipMapping);
	for (const UknightAbilitySet* const AbilitySet : MonsterDefinition->AbilitySets)
	{
		if (AbilitySet)
		{
			AbilitySet->GiveToAbilitySystem(AbilitySystemComponent, &GrantedHandles, this);
		}
	}
}

bool AknightMonsterCharacterBase::CanReceiveHit(const AActor* Attacker) const
{
	return !IsDead() && Attacker != this;
}

FGameplayTag AknightMonsterCharacterBase::ResolveHitPart(const FHitResult& Hit) const
{
	return PartComponent->ResolvePart(Hit);
}

float AknightMonsterCharacterBase::GetPartDamageMultiplier(const FGameplayTag& PartTag) const
{
	return PartComponent->GetDamageMultiplier(PartTag);
}

bool AknightMonsterCharacterBase::IsDead() const
{
	return bIsDying || AbilitySystemComponent->HasMatchingGameplayTag(knightGameplayTags::State_Dead);
}

bool AknightMonsterCharacterBase::IsTargetable() const
{
	return !IsDead();
}

void AknightMonsterCharacterBase::GetTargetPoints(TArray<FknightTargetPoint>& OutPoints) const
{
	const int32 Before = OutPoints.Num();
	PartComponent->GetTargetPoints(OutPoints);
	if (OutPoints.Num() == Before)
	{
		FknightTargetPoint& Point = OutPoints.AddDefaulted_GetRef();
		Point.Component = GetMesh();
		Point.SocketName = MonsterDefinition ? MonsterDefinition->FallbackLockOnSocket : NAME_None;
	}
}

float AknightMonsterCharacterBase::GetTargetRadius() const
{
	return MonsterDefinition ? MonsterDefinition->TargetRadius : GetCapsuleComponent()->GetScaledCapsuleRadius();
}

FGenericTeamId AknightMonsterCharacterBase::GetGenericTeamId() const
{
	return knightTeams::ToTeamId(EknightTeam::Monster);
}

void AknightMonsterCharacterBase::HandleOutOfHealth(AActor* DamageInstigator, AActor* DamageCauser, const FGameplayEffectSpec* EffectSpec,
	float Magnitude, float OldValue, float NewValue)
{
	if (bIsDying)
	{
		return;
	}
	bIsDying = true;
	SetCountedAlive(false);

	AbilitySystemComponent->AddLooseGameplayTag(knightGameplayTags::State_Dead);

	FGameplayEventData Payload;
	Payload.EventTag = knightGameplayTags::Event_Death;
	Payload.Instigator = DamageInstigator;
	Payload.Target = this;
	Payload.EventMagnitude = Magnitude;
	if (EffectSpec)
	{
		Payload.ContextHandle = EffectSpec->GetEffectContext();
	}
	AbilitySystemComponent->HandleGameplayEvent(Payload.EventTag, &Payload);

	const FGameplayTagContainer KeepAbilities(knightGameplayTags::Ability_Death);
	AbilitySystemComponent->CancelAbilities(nullptr, &KeepAbilities);

	if (AAIController* const AIController = Cast<AAIController>(GetController()))
	{
		if (UBrainComponent* const Brain = AIController->GetBrainComponent())
		{
			Brain->StopLogic(TEXT("Monster defeated"));
		}
		AIController->StopMovement();
		// A corpse no longer needs sight and hearing queries.
		if (UAIPerceptionComponent* const Perception = AIController->GetAIPerceptionComponent())
		{
			Perception->Deactivate();
		}
	}
	TargetingComponent->StopTargeting();
	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->DisableMovement();

	// The Knight walks over or through the corpse to reach carve points; part bodies stay for presentation.
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);

	UE_LOG(LogknightAI, Log, TEXT("%s defeated by %s."), *GetNameSafe(this), *GetNameSafe(DamageInstigator));
	OnMonsterDefeated.Broadcast(this, DamageInstigator);
}

void AknightMonsterCharacterBase::HandleRageMaxed(AActor* EffectInstigator, AActor* EffectCauser, const FGameplayEffectSpec* EffectSpec,
	float Magnitude, float OldValue, float NewValue)
{
	if (!IsDead() && MonsterDefinition && MonsterDefinition->EnrageEffect)
	{
		AbilitySystemComponent->ApplyGameplayEffectToSelf(MonsterDefinition->EnrageEffect->GetDefaultObject<UGameplayEffect>(), 1.f,
			AbilitySystemComponent->MakeEffectContext());
	}
}

void AknightMonsterCharacterBase::HandleStaminaDepleted(AActor* EffectInstigator, AActor* EffectCauser, const FGameplayEffectSpec* EffectSpec,
	float Magnitude, float OldValue, float NewValue)
{
	if (!IsDead() && MonsterDefinition && MonsterDefinition->ExhaustEffect)
	{
		AbilitySystemComponent->ApplyGameplayEffectToSelf(MonsterDefinition->ExhaustEffect->GetDefaultObject<UGameplayEffect>(), 1.f,
			AbilitySystemComponent->MakeEffectContext());
	}
}
