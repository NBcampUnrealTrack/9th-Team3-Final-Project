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
#include "Monster/knightGameplayEffect_ParriedPoiseDamage.h"
#include "Monster/knightGameplayEffect_RageGain.h"
#include "Monster/knightMonsterBodyBlockerComponent.h"
#include "Monster/knightMonsterDefinition.h"
#include "Monster/knightMonsterMovementComponent.h"
#include "Monster/knightMonsterPartComponent.h"
#include "Monster/knightMonsterAirTurnComponent.h"
#include "Monster/knightMonsterHoverComponent.h"
#include "Monster/knightMonsterPushOutComponent.h"
#include "Monster/knightMonsterTargetingComponent.h"
#include "MotionWarpingComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "Perception/AIPerceptionComponent.h"
#include "System/knightGameplayTags.h"
#include "System/knightTeams.h"
#include "TimerManager.h"
#include "knight.h"
#include "knightPerfCounters.h"

AknightMonsterCharacterBase::AknightMonsterCharacterBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UknightMonsterMovementComponent>(ACharacter::CharacterMovementComponentName))
{
	// 이동 전용 캡슐. Blueprint에서 몸 전체가 아니라 몬스터가 발 딛는 면적에 맞춰 크기를 정한다.
	GetCapsuleComponent()->InitCapsuleSize(120.f, 200.f);

	bUseControllerRotationYaw = false;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	// 무기 트레이스는 캡슐이 아니라 피직스 에셋 바디에 맞는다.
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
	PushOut = CreateDefaultSubobject<UknightMonsterPushOutComponent>(TEXT("PushOut"));
	Hover = CreateDefaultSubobject<UknightMonsterHoverComponent>(TEXT("Hover"));
	AirTurn = CreateDefaultSubobject<UknightMonsterAirTurnComponent>(TEXT("AirTurn"));
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

	// 바닥 데칼(잔열 자국 등)이 위에 선 몸과 무기에 덧씌워지지 않게 한다.
	TInlineComponentArray<UPrimitiveComponent*> Primitives(this);
	for (UPrimitiveComponent* const Primitive : Primitives)
	{
		Primitive->SetReceivesDecals(false);
	}

	CoreAttributeSet->OnOutOfHealth.AddUObject(this, &ThisClass::HandleOutOfHealth);
	CoreAttributeSet->OnDamaged.AddUObject(this, &ThisClass::HandleDamaged);
	MonsterAttributeSet->OnRageMaxed.AddUObject(this, &ThisClass::HandleRageMaxed);
	MonsterAttributeSet->OnStaminaDepleted.AddUObject(this, &ThisClass::HandleStaminaDepleted);
	TargetingComponent->OnTargetChanged.AddUniqueDynamic(this, &ThisClass::HandleTargetChanged);
	AbilitySystemComponent->GenericGameplayEventCallbacks.FindOrAdd(knightGameplayTags::Event_Parried).AddUObject(this, &ThisClass::HandleParried);
	if (MonsterDefinition)
	{
		for (const FGameplayTag& BlockedTag : MonsterDefinition->ParriedPoiseBlockedTags)
		{
			AbilitySystemComponent->RegisterGameplayTagEvent(BlockedTag, EGameplayTagEventType::NewOrRemoved)
				.AddUObject(this, &ThisClass::HandleParriedPoiseBlockedChanged);
		}
	}

	// 배치된 몬스터는 빙의 전에 BeginPlay가 불릴 수 있다. 부위와 위협도는 어느 쪽이든 동작한다.
	InitializeAbilitySystem();
	SetCountedAlive(true);
	PrewarmEffects();
}

void AknightMonsterCharacterBase::PrewarmEffects()
{
	UWorld* const World = GetWorld();
	if (!MonsterDefinition || !World || !World->IsGameWorld())
	{
		return;
	}
	// 레이드 로딩 화면 동안은 월드를 그리지 않아 실제로 그려서 데우는 방법은 통하지 않는다. 대신 숨긴 채 켜면 Niagara가
	// 초기화하면서 PSO 준비를 비동기로 걸어 두고, 실제 공격 때는 준비된 PSO를 쓴다.
	TArray<TWeakObjectPtr<UNiagaraComponent>> Spawned;
	for (UNiagaraSystem* const System : MonsterDefinition->PrewarmEffects)
	{
		UNiagaraComponent* const Effect = System ? UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, System, GetActorLocation(),
			FRotator::ZeroRotator, FVector::OneVector, false, false) : nullptr;
		if (Effect)
		{
			Effect->SetHiddenInGame(true);
			Effect->Activate(true);
			Spawned.Add(Effect);
		}
	}
	if (Spawned.IsEmpty())
	{
		return;
	}
	FTimerHandle Handle;
	World->GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(this, [Spawned]()
	{
		for (const TWeakObjectPtr<UNiagaraComponent>& Effect : Spawned)
		{
			if (Effect.IsValid())
			{
				Effect->DestroyComponent();
			}
		}
	}), 2.f, false);
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
		// 시체는 더 이상 시야와 청각 쿼리가 필요 없다.
		if (UAIPerceptionComponent* const Perception = AIController->GetAIPerceptionComponent())
		{
			Perception->Deactivate();
		}
	}
	TargetingComponent->StopTargeting();
	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->DisableMovement();

	// 플레이어가 갈무리 지점에 닿도록 시체를 밟거나 통과할 수 있게 한다. 부위 바디는 연출용으로 남긴다.
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	// 큰 몬스터의 몸 아래 차단 상자도 같은 이유로 끈다.
	TArray<UknightMonsterBodyBlockerComponent*> BodyBlockers;
	GetComponents<UknightMonsterBodyBlockerComponent>(BodyBlockers);
	for (UknightMonsterBodyBlockerComponent* const BodyBlocker : BodyBlockers)
	{
		BodyBlocker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

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

void AknightMonsterCharacterBase::HandleDamaged(AActor* DamageInstigator, AActor* DamageCauser, const FGameplayEffectSpec* EffectSpec,
	float Magnitude, float OldValue, float NewValue)
{
	AddRageFromDamage(Magnitude, DamageInstigator);

	const float Ratio = MonsterDefinition ? MonsterDefinition->Phase2HealthRatio : 0.f;
	if (bInPhase2 || bPhase2Pending || IsDead() || Ratio <= 0.f
		|| CoreAttributeSet->GetHealth() >= CoreAttributeSet->GetMaxHealth() * Ratio)
	{
		return;
	}

	// 다운 중에 넘기면 기상 후에 포효한다. 전환 포효가 다운 중 반격 기회를 끊지 않게.
	if (AbilitySystemComponent->HasMatchingGameplayTag(knightGameplayTags::State_Monster_Reacting))
	{
		bPhase2Pending = true;
		ReactingChangedHandle = AbilitySystemComponent->RegisterGameplayTagEvent(knightGameplayTags::State_Monster_Reacting,
			EGameplayTagEventType::NewOrRemoved).AddUObject(this, &ThisClass::HandleReactingChanged);
		return;
	}
	EnterPhase2();
}

void AknightMonsterCharacterBase::HandleReactingChanged(const FGameplayTag Tag, int32 NewCount)
{
	if (NewCount > 0)
	{
		return;
	}
	AbilitySystemComponent->RegisterGameplayTagEvent(knightGameplayTags::State_Monster_Reacting, EGameplayTagEventType::NewOrRemoved)
		.Remove(ReactingChangedHandle);
	ReactingChangedHandle.Reset();
	bPhase2Pending = false;
	if (!IsDead())
	{
		EnterPhase2();
	}
}

void AknightMonsterCharacterBase::EnterPhase2()
{
	bInPhase2 = true;
	AbilitySystemComponent->AddLooseGameplayTag(knightGameplayTags::State_Monster_Phase2);
	UE_LOG(LogknightAI, Log, TEXT("%s: phase 2 at health %.0f / %.0f."), *GetNameSafe(this),
		CoreAttributeSet->GetHealth(), CoreAttributeSet->GetMaxHealth());
}

void AknightMonsterCharacterBase::AddRageFromDamage(float Damage, AActor* DamageInstigator)
{
	const float Ratio = MonsterDefinition ? MonsterDefinition->RageFullDamageRatio : 0.f;
	const float FullDamage = CoreAttributeSet->GetMaxHealth() * Ratio;
	// 분노 중에 쌓으면 분노가 끝나 Rage가 0이 된 직후 남은 값으로 다시 분노할 수 있다.
	if (IsDead() || Damage <= 0.f || FullDamage <= 0.f
		|| AbilitySystemComponent->HasMatchingGameplayTag(knightGameplayTags::State_Monster_Enraged))
	{
		return;
	}
	FGameplayEffectContextHandle Context = AbilitySystemComponent->MakeEffectContext();
	if (DamageInstigator)
	{
		Context.AddInstigator(DamageInstigator, DamageInstigator);
	}
	FGameplayEffectSpec Spec(GetDefault<UknightGameplayEffect_RageGain>(), Context, 1.f);
	Spec.SetSetByCallerMagnitude(knightGameplayTags::SetByCaller_Rage, MonsterAttributeSet->GetMaxRage() * Damage / FullDamage);
	AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(Spec);
	UE_LOG(LogknightAI, Verbose, TEXT("%s: rage %.1f / %.1f after %.0f damage."), *GetNameSafe(this),
		MonsterAttributeSet->GetRage(), MonsterAttributeSet->GetMaxRage(), Damage);
}

void AknightMonsterCharacterBase::HandleTargetChanged(AActor* OldTarget, AActor* NewTarget)
{
	// 타깃을 잃었다 다시 찾는 것은 첫 조우가 아니다. 포효는 한 생에 한 번만.
	if (bHasEngaged || !NewTarget || IsDead())
	{
		return;
	}
	bHasEngaged = true;

	FGameplayEventData Payload;
	Payload.EventTag = knightGameplayTags::Event_Monster_Engaged;
	Payload.Instigator = NewTarget;
	Payload.Target = this;
	AbilitySystemComponent->HandleGameplayEvent(Payload.EventTag, &Payload);
}

void AknightMonsterCharacterBase::HandleParried(const FGameplayEventData* Payload)
{
	const float Ratio = MonsterDefinition ? MonsterDefinition->ParriedPoiseDamageRatio : 0.f;
	if (IsDead() || Ratio <= 0.f)
	{
		return;
	}
	if (AbilitySystemComponent->HasAnyMatchingGameplayTags(MonsterDefinition->ParriedPoiseBlockedTags))
	{
		UE_LOG(LogknightAI, Log, TEXT("%s: parried, poise blocked by state."), *GetNameSafe(this));
		return;
	}
	// 경직 반응이 같은 이벤트로 먼저 발동해 공격을 취소하므로, 공격 쪽 설정은 이벤트에 실려 온 태그로 본다.
	if (Payload && Payload->InstigatorTags.HasTagExact(knightGameplayTags::Event_Parried_NoPoise))
	{
		UE_LOG(LogknightAI, Log, TEXT("%s: parried, the attack does not build poise."), *GetNameSafe(this));
		return;
	}
	const float PoiseDamage = CoreAttributeSet->GetMaxPoise() * Ratio;
	AActor* const Defender = Payload ? const_cast<AActor*>(Payload->Instigator.Get()) : nullptr;
	FGameplayEffectContextHandle Context = AbilitySystemComponent->MakeEffectContext();
	if (Defender)
	{
		Context.AddInstigator(Defender, Defender);
	}
	FGameplayEffectSpec Spec(GetDefault<UknightGameplayEffect_ParriedPoiseDamage>(), Context, 1.f);
	Spec.SetSetByCallerMagnitude(knightGameplayTags::SetByCaller_PoiseDamage, PoiseDamage);
	AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(Spec);
	UE_LOG(LogknightAI, Log, TEXT("%s: parried by %s, poise -%.0f -> %.0f / %.0f."), *GetNameSafe(this), *GetNameSafe(Defender),
		PoiseDamage, CoreAttributeSet->GetPoise(), CoreAttributeSet->GetMaxPoise());
}

void AknightMonsterCharacterBase::HandleParriedPoiseBlockedChanged(const FGameplayTag Tag, int32 NewCount)
{
	if (NewCount <= 0 || IsDead())
	{
		return;
	}
	const float MaxPoise = AbilitySystemComponent->GetNumericAttributeBase(UknightCoreAttributeSet::GetMaxPoiseAttribute());
	AbilitySystemComponent->SetNumericAttributeBase(UknightCoreAttributeSet::GetPoiseAttribute(), MaxPoise);
	UE_LOG(LogknightAI, Log, TEXT("%s: %s added, poise reset to %.0f."), *GetNameSafe(this), *Tag.ToString(), MaxPoise);
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
