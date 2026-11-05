#include "Monster/knightMonsterCharacterBase.h"

#include "AIController.h"
#include "BrainComponent.h"
#include "AbilitySystem/Abilities/knightGameplayAbility_MonsterAttack.h"
#include "AbilitySystem/Attributes/knightCoreAttributeSet.h"
#include "AbilitySystem/Effects/knightGameplayEffect_Burn.h"
#include "AbilitySystemGlobals.h"
#include "AbilitySystem/Attributes/knightMonsterAttributeSet.h"
#include "AbilitySystem/knightAbilitySystemComponent.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "Combat/knightHitReactionComponent.h"
#include "Combat/knightMeleeTraceComponent.h"
#include "Components/AudioComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "EngineUtils.h"
#include "GameplayEffect.h"
#include "Kismet/GameplayStatics.h"
#include "Monster/knightGameplayEffect_ParriedPoiseDamage.h"
#include "Monster/knightGameplayEffect_RageGain.h"
#include "Monster/knightMonsterBodyBlockerComponent.h"
#include "Monster/knightMonsterDefinition.h"
#include "Monster/knightMonsterMovementComponent.h"
#include "Monster/knightMonsterPartComponent.h"
#include "Monster/knightMonsterPartShellComponent.h"
#include "Monster/knightMonsterAirTurnComponent.h"
#include "Monster/knightMonsterGuardPushComponent.h"
#include "Monster/knightMonsterHitStopComponent.h"
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
	PartShell = CreateDefaultSubobject<UknightMonsterPartShellComponent>(TEXT("PartShell"));
	TargetingComponent = CreateDefaultSubobject<UknightMonsterTargetingComponent>(TEXT("TargetingComponent"));
	MeleeTrace = CreateDefaultSubobject<UknightMeleeTraceComponent>(TEXT("MeleeTrace"));
	HitStop = CreateDefaultSubobject<UknightMonsterHitStopComponent>(TEXT("HitStop"));
	HitReaction = CreateDefaultSubobject<UknightHitReactionComponent>(TEXT("HitReaction"));
	MotionWarping = CreateDefaultSubobject<UMotionWarpingComponent>(TEXT("MotionWarping"));
	PushOut = CreateDefaultSubobject<UknightMonsterPushOutComponent>(TEXT("PushOut"));
	GuardPush = CreateDefaultSubobject<UknightMonsterGuardPushComponent>(TEXT("GuardPush"));
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
	AbilitySystemComponent->RegisterGameplayTagEvent(knightGameplayTags::State_Monster_Enraged, EGameplayTagEventType::NewOrRemoved)
		.AddUObject(this, &ThisClass::HandleEnragedChanged);
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
	StartLoopSounds(false);
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

namespace
{
	/** 줄여 둔 루프 소리의 볼륨. 0으로 틀면 엔진이 소리를 바로 끝내 버려 다시 키울 수 없으므로 들리지 않을 만큼만 남긴다. */
	constexpr float MutedLoopVolume = 0.001f;
}

void AknightMonsterCharacterBase::StartLoopSounds(bool bEngaged)
{
	UWorld* const World = GetWorld();
	if (!MonsterDefinition || !World || !World->IsGameWorld())
	{
		return;
	}
	const TArray<FknightMonsterLoopSound>& Loops = MonsterDefinition->LoopSounds;
	LoopAudios.SetNum(Loops.Num());
	LoopResting.SetNum(Loops.Num());
	LoopBurstTimers.SetNum(Loops.Num());
	for (int32 Index = 0; Index < Loops.Num(); ++Index)
	{
		const FknightMonsterLoopSound& Loop = Loops[Index];
		if (!Loop.Sound || Loop.bWaitForEngage != bEngaged || LoopAudios[Index])
		{
			continue;
		}
		UAudioComponent* const Audio = UGameplayStatics::SpawnSoundAttached(Loop.Sound, GetMesh(), Loop.AttachName, FVector::ZeroVector,
			EAttachLocation::KeepRelativeOffset, true, 1.f, 1.f, 0.f, nullptr, nullptr, false);
		if (!Audio)
		{
			continue;
		}
		LoopAudios[Index] = Audio;
		// 가끔 들리는 소리는 쉬는 상태로 시작해 첫 RestTime 뒤에 처음 들린다.
		LoopResting[Index] = Loop.BurstTime > 0.f;
		if (LoopResting[Index])
		{
			World->GetTimerManager().SetTimer(LoopBurstTimers[Index], FTimerDelegate::CreateUObject(this, &ThisClass::ToggleLoopBurst, Index),
				FMath::RandRange(Loop.RestTime.Min, Loop.RestTime.Max), false);
		}
		Audio->FadeIn(Loop.FadeTime, GetLoopVolume(Index));
		for (const FGameplayTag& Tag : Loop.MuteTags)
		{
			AbilitySystemComponent->RegisterGameplayTagEvent(Tag, EGameplayTagEventType::NewOrRemoved)
				.AddUObject(this, &ThisClass::HandleLoopSoundMuteChanged);
		}
	}
}

void AknightMonsterCharacterBase::StopLoopSounds()
{
	for (int32 Index = 0; Index < LoopAudios.Num(); ++Index)
	{
		if (UAudioComponent* const Audio = LoopAudios[Index])
		{
			const float FadeTime = MonsterDefinition && MonsterDefinition->LoopSounds.IsValidIndex(Index) ? MonsterDefinition->LoopSounds[Index].FadeTime : 0.5f;
			Audio->bAutoDestroy = true;
			Audio->FadeOut(FadeTime, 0.f);
		}
	}
	LoopAudios.Reset();
	if (UWorld* const World = GetWorld())
	{
		for (FTimerHandle& Timer : LoopBurstTimers)
		{
			World->GetTimerManager().ClearTimer(Timer);
		}
	}
	LoopBurstTimers.Reset();
	LoopResting.Reset();
}

void AknightMonsterCharacterBase::HandleLoopSoundMuteChanged(const FGameplayTag Tag, int32 NewCount)
{
	if (!MonsterDefinition || IsDead())
	{
		return;
	}
	for (int32 Index = 0; Index < LoopAudios.Num(); ++Index)
	{
		const FknightMonsterLoopSound& Loop = MonsterDefinition->LoopSounds[Index];
		if (UAudioComponent* const Audio = LoopAudios[Index]; Audio && Loop.MuteTags.HasTagExact(Tag))
		{
			// 같은 소리에 걸린 다른 태그가 남아 있으면 계속 줄여 둔다.
			Audio->AdjustVolume(Loop.FadeTime, GetLoopVolume(Index));
		}
	}
}

float AknightMonsterCharacterBase::GetLoopVolume(int32 Index) const
{
	const FknightMonsterLoopSound& Loop = MonsterDefinition->LoopSounds[Index];
	const bool bResting = LoopResting.IsValidIndex(Index) && LoopResting[Index];
	return bResting || AbilitySystemComponent->HasAnyMatchingGameplayTags(Loop.MuteTags) ? MutedLoopVolume : 1.f;
}

void AknightMonsterCharacterBase::ToggleLoopBurst(int32 Index)
{
	UWorld* const World = GetWorld();
	if (!MonsterDefinition || !World || !LoopAudios.IsValidIndex(Index) || !LoopAudios[Index] || IsDead())
	{
		return;
	}
	const FknightMonsterLoopSound& Loop = MonsterDefinition->LoopSounds[Index];
	LoopResting[Index] = !LoopResting[Index];
	LoopAudios[Index]->AdjustVolume(Loop.FadeTime, GetLoopVolume(Index));
	UE_LOG(LogknightAI, Verbose, TEXT("%s: loop sound %s %s."), *GetName(), *GetNameSafe(Loop.Sound), LoopResting[Index] ? TEXT("rests") : TEXT("plays"));
	const float Delay = LoopResting[Index] ? FMath::RandRange(Loop.RestTime.Min, Loop.RestTime.Max) : Loop.BurstTime;
	World->GetTimerManager().SetTimer(LoopBurstTimers[Index], FTimerDelegate::CreateUObject(this, &ThisClass::ToggleLoopBurst, Index), Delay, false);
}

void AknightMonsterCharacterBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopLoopSounds();
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
	StopLoopSounds();

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
	RemoveLingeringEffectsOnOthers();
	// 날던 몬스터는 나는 자세에서 바닥 사망 자세로 섞이며 순식간에 내려온다. 중력으로 떨어지듯 받친다(바닥이면 그대로).
	if (Hover)
	{
		Hover->FallToGround(-GetCharacterMovement()->GetGravityZ());
	}

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
	// 공중에서 죽으면(도약 중 등) 그 자리에서 이동을 끄면 시체가 떠 있다. 떨어뜨리고 땅에 닿으면(Landed) 끈다.
	UCharacterMovementComponent* const Movement = GetCharacterMovement();
	if (Movement->IsMovingOnGround() || Movement->MovementMode == MOVE_None)
	{
		Movement->StopMovementImmediately();
		Movement->DisableMovement();
	}
	else
	{
		bFallingToDeath = true;
		Movement->Velocity = FVector(0.f, 0.f, FMath::Min(Movement->Velocity.Z, 0.f));
		Movement->SetMovementMode(MOVE_Falling);
	}

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

void AknightMonsterCharacterBase::Landed(const FHitResult& Hit)
{
	Super::Landed(Hit);
	if (bFallingToDeath)
	{
		bFallingToDeath = false;
		GetCharacterMovement()->StopMovementImmediately();
		GetCharacterMovement()->DisableMovement();
	}
}

void AknightMonsterCharacterBase::RemoveLingeringEffectsOnOthers()
{
	// 이 몬스터가 건 화상(지속 피해)은 처치 뒤에도 남아 틱마다 피해를 준다. 처치 연출 중에 플레이어가 다치지 않게 지운다.
	FGameplayEffectQuery Query;
	const UAbilitySystemComponent* const Source = AbilitySystemComponent;
	Query.CustomMatchDelegate.BindLambda([Source](const FActiveGameplayEffect& Effect)
	{
		return Effect.Spec.Def && Effect.Spec.Def->IsA<UknightGameplayEffect_Burn>()
			&& Effect.Spec.GetEffectContext().GetInstigatorAbilitySystemComponent() == Source;
	});
	for (TActorIterator<APawn> It(GetWorld()); It; ++It)
	{
		UAbilitySystemComponent* const ASC = *It != this ? UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(*It) : nullptr;
		if (ASC && ASC->GetActiveEffects(Query).Num() > 0)
		{
			ASC->RemoveActiveEffects(Query);
			UE_LOG(LogknightAI, Log, TEXT("%s: removed its burn from %s."), *GetName(), *It->GetName());
		}
	}
}

void AknightMonsterCharacterBase::HandleRageMaxed(AActor* EffectInstigator, AActor* EffectCauser, const FGameplayEffectSpec* EffectSpec,
	float Magnitude, float OldValue, float NewValue)
{
	if (IsDead() || !MonsterDefinition || !MonsterDefinition->EnrageEffect || EnrageStart != EEnrageStart::None)
	{
		return;
	}
	EnrageStart = EEnrageStart::Waiting;
	UpdateEnrageStart();
	if (EnrageStart != EEnrageStart::None)
	{
		GetWorldTimerManager().SetTimer(EnrageStartTimer, this, &ThisClass::UpdateEnrageStart, 0.1f, true);
	}
}

void AknightMonsterCharacterBase::UpdateEnrageStart()
{
	if (IsDead() || !MonsterDefinition || !MonsterDefinition->EnrageEffect)
	{
		GetWorldTimerManager().ClearTimer(EnrageStartTimer);
		EnrageStart = EEnrageStart::None;
		return;
	}
	// 다운·탈진·다른 반응(이륙 포함)이 끝날 때까지 기다린다. Rage는 가득 찬 채로 남는다.
	// 반응 중에 분노 포효가 끼어들면 다운·탈진이 끊겨 플레이어가 공격할 시간을 잃는다.
	FGameplayTagContainer WaitTags;
	WaitTags.AddTag(knightGameplayTags::State_Monster_Reacting);
	WaitTags.AddTag(knightGameplayTags::State_Monster_Exhausted);
	WaitTags.AddTag(knightGameplayTags::State_Monster_Collapsed);
	if (AbilitySystemComponent->HasAnyMatchingGameplayTags(WaitTags))
	{
		return;
	}
	if (EnrageStart == EEnrageStart::Waiting && MonsterDefinition->bEnrageTakeOffFirst && AirTurn && !AirTurn->IsAirborne())
	{
		// 이륙 반응(Event.Monster.TakeOff)이 Reacting을 붙이므로 다음 확인에서 이륙이 끝나기를 기다린다.
		EnrageStart = EEnrageStart::TakingOff;
		AirTurn->TakeOff();
		UE_LOG(LogknightAI, Log, TEXT("%s: take off before enrage."), *GetName());
		return;
	}
	GetWorldTimerManager().ClearTimer(EnrageStartTimer);
	EnrageStart = EEnrageStart::None;
	AbilitySystemComponent->ApplyGameplayEffectToSelf(MonsterDefinition->EnrageEffect->GetDefaultObject<UGameplayEffect>(), 1.f,
		AbilitySystemComponent->MakeEffectContext());
}

void AknightMonsterCharacterBase::HandleDamaged(AActor* DamageInstigator, AActor* DamageCauser, const FGameplayEffectSpec* EffectSpec,
	float Magnitude, float OldValue, float NewValue)
{
	// 이 타격으로 죽으면 분노·2페이즈를 쌓지 않는다. 사망 처리(IsDead)는 이 알림 뒤라서, 큰 한 방이 분노를 채우면
	// 쓰러지는 몸으로 분노 포효(잔향 공명해방)가 시작됐다.
	const bool bLethal = NewValue <= 0.f;
	if (!bLethal)
	{
		AddRageFromDamage(Magnitude, DamageInstigator);
	}
	TriggerHitFlinch(DamageInstigator, EffectSpec, Magnitude);

	const float Ratio = MonsterDefinition ? MonsterDefinition->Phase2HealthRatio : 0.f;
	if (bLethal || bInPhase2 || bPhase2Pending || IsDead() || Ratio <= 0.f
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

EknightHitSide AknightMonsterCharacterBase::GetHitSide(const AActor& Victim, const FVector& From)
{
	const FVector ToFrom = (From - Victim.GetActorLocation()).GetSafeNormal2D();
	const float Forward = FVector::DotProduct(ToFrom, Victim.GetActorForwardVector());
	const float Right = FVector::DotProduct(ToFrom, Victim.GetActorRightVector());
	if (FMath::Abs(Forward) >= FMath::Abs(Right))
	{
		return Forward >= 0.f ? EknightHitSide::Front : EknightHitSide::Back;
	}
	return Right >= 0.f ? EknightHitSide::Right : EknightHitSide::Left;
}

void AknightMonsterCharacterBase::TriggerHitFlinch(AActor* DamageInstigator, const FGameplayEffectSpec* EffectSpec, float Damage)
{
	if (!MonsterDefinition || IsDead() || Damage <= 0.f)
	{
		return;
	}
	const FknightMonsterHitFlinch& Settings = MonsterDefinition->HitFlinch;
	// 맞은 자리(무기 판정의 충돌 지점)가 있으면 그쪽, 없으면 공격한 쪽.
	const FHitResult* const Hit = EffectSpec ? EffectSpec->GetContext().GetHitResult() : nullptr;
	const AActor* const Source = DamageInstigator ? DamageInstigator : (EffectSpec ? EffectSpec->GetContext().GetInstigator() : nullptr);
	if (!Hit && !Source)
	{
		return;
	}
	const FVector From = Hit ? FVector(Hit->ImpactPoint) : Source->GetActorLocation();
	const EknightHitSide Side = GetHitSide(*this, From);
	const UAnimSequence* const Sequence = Side == EknightHitSide::Front ? Settings.Front.Get()
		: Side == EknightHitSide::Back ? Settings.Back.Get()
		: Side == EknightHitSide::Left ? Settings.Left.Get() : Settings.Right.Get();
	if (!Sequence)
	{
		return;
	}
	const UGameplayAbility* const SourceAbility = EffectSpec ? EffectSpec->GetContext().GetAbility() : nullptr;
	const bool bHeavy = SourceAbility && !Settings.HeavyAttackTags.IsEmpty() && SourceAbility->GetAssetTags().HasAny(Settings.HeavyAttackTags);
	float Strength = FMath::Lerp(Settings.MinStrength, Settings.MaxStrength, FMath::Clamp(Damage / Settings.FullDamage, 0.f, 1.f));
	float PlayRate = Settings.PlayRate;
	if (bHeavy)
	{
		Strength = FMath::Min(Strength * Settings.HeavyStrengthScale, 1.f);
		PlayRate *= Settings.HeavyPlayRateScale;
	}
	const bool bWeak = !Settings.WeakTags.IsEmpty() && AbilitySystemComponent->HasAnyMatchingGameplayTags(Settings.WeakTags);
	if (bWeak)
	{
		Strength *= Settings.WeakStrengthScale;
	}
	const double Now = GetWorld()->GetTimeSeconds();
	// 강공격은 항상 새로 움찔한다. 강공격 움찔은 길게 이어져야 하므로, 끝나기 전에 들어온 약한 타격이
	// 처음부터 다시 틀어 짧게 끊지 못하게 한다.
	const double KeepUntil = FMath::Max(LastHitFlinchTime + Settings.MinInterval, HeavyFlinchEndTime);
	if (!bHeavy && Now < KeepUntil && Strength <= HitFlinchState.Strength)
	{
		return;
	}
	LastHitFlinchTime = Now;
	HeavyFlinchEndTime = bHeavy ? Now + Sequence->GetPlayLength() / PlayRate : -1.e9;
	HitFlinchState.Sequence = Sequence;
	HitFlinchState.Strength = Strength;
	HitFlinchState.PlayRate = PlayRate;
	HitFlinchState.BlendIn = Settings.BlendIn;
	HitFlinchState.BlendOut = Settings.BlendOut;
	++HitFlinchState.Count;
	UE_LOG(LogknightAI, Verbose, TEXT("%s: hit flinch %s (%s, %s%s, damage %.0f, strength %.2f, %.2fs)."), *GetName(), *GetNameSafe(Sequence),
		*StaticEnum<EknightHitSide>()->GetNameStringByValue(static_cast<int64>(Side)), bHeavy ? TEXT("heavy") : TEXT("light"),
		bWeak ? TEXT(" weak") : TEXT(""), Damage, Strength, Sequence->GetPlayLength() / PlayRate);
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
	StartLoopSounds(true);

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

void AknightMonsterCharacterBase::HandleEnragedChanged(const FGameplayTag Tag, int32 NewCount)
{
	// 죽으면서 효과가 지워져 떨어질 때는 시체의 외형을 바꾸지 않는다.
	if (NewCount <= 0 && IsDead())
	{
		return;
	}
	if (NewCount > 0 && EnrageFinale == EEnrageFinale::None && MonsterDefinition && MonsterDefinition->EnrageFinaleAttack)
	{
		EnrageFinale = EEnrageFinale::Waiting;
		GetWorldTimerManager().SetTimer(EnrageFinaleTimer, this, &ThisClass::UpdateEnrageFinale, 0.25f, true);
	}
	else if (NewCount <= 0 && EnrageFinale == EEnrageFinale::Waiting)
	{
		EndEnrageFinale();
	}
	OnEnragedChanged(NewCount > 0);
}

void AknightMonsterCharacterBase::UpdateEnrageFinale()
{
	const TSubclassOf<UknightGameplayAbility_MonsterAttack> FinaleClass = MonsterDefinition ? MonsterDefinition->EnrageFinaleAttack : nullptr;
	FGameplayAbilitySpec* const Spec = FinaleClass ? AbilitySystemComponent->FindAbilitySpecFromClass(FinaleClass) : nullptr;
	if (IsDead() || !Spec)
	{
		EndEnrageFinale();
		return;
	}
	const double Now = GetWorld()->GetTimeSeconds();
	switch (EnrageFinale)
	{
	case EEnrageFinale::Waiting:
	{
		FGameplayEffectQuery Query;
		Query.EffectDefinition = MonsterDefinition->EnrageEffect;
		float Remaining = 0.f;
		for (const float Time : AbilitySystemComponent->GetActiveEffectsTimeRemaining(Query))
		{
			Remaining = FMath::Max(Remaining, Time);
		}
		if (Remaining > MonsterDefinition->EnrageFinaleLeadTime)
		{
			return;
		}
		// 효과가 끝나도 분노 태그가 떨어지지 않게 직접 붙여 두고, 마무리 공격이 끝나면 뗀다.
		AbilitySystemComponent->AddLooseGameplayTag(knightGameplayTags::State_Monster_Enraged);
		AbilitySystemComponent->AddLooseGameplayTag(knightGameplayTags::State_Monster_EnrageFinale);
		EnrageFinale = Spec->IsActive() ? EEnrageFinale::Running : EEnrageFinale::Pending;
		EnrageFinaleGiveUpTime = Now + 20.0;
		UE_LOG(LogknightAI, Log, TEXT("%s: enrage ends in %.1f s, finale %s next."), *GetName(), Remaining, *GetNameSafe(FinaleClass));
		return;
	}
	case EEnrageFinale::Pending:
	{
		if (Now > EnrageFinaleGiveUpTime)
		{
			UE_LOG(LogknightAI, Warning, TEXT("%s: gave up the enrage finale (still blocked after 20 s)."), *GetName());
			EndEnrageFinale();
			return;
		}
		// 지금 공격이나 반응(다운·포효)이 끝난 뒤에 쓴다.
		for (const FGameplayAbilitySpec& Other : AbilitySystemComponent->GetActivatableAbilities())
		{
			if (Other.IsActive() && Cast<UknightGameplayAbility_MonsterAttack>(Other.GetPrimaryInstance()))
			{
				return;
			}
		}
		if (AbilitySystemComponent->HasMatchingGameplayTag(knightGameplayTags::State_Monster_Reacting))
		{
			return;
		}
		// 공중 공격이면 먼저 뜨고, 지상 공격이면 먼저 내린다.
		const FknightMonsterAttackSelection& Selection = GetDefault<UknightGameplayAbility_MonsterAttack>(FinaleClass)->GetSelection();
		const bool bAirAttack = Selection.RequiredOwnerTags.HasTagExact(knightGameplayTags::State_Monster_Airborne);
		const bool bGroundAttack = Selection.BlockedOwnerTags.HasTagExact(knightGameplayTags::State_Monster_Airborne);
		if (AirTurn && (bAirAttack || bGroundAttack) && AirTurn->IsAirborne() != bAirAttack)
		{
			bAirAttack ? AirTurn->TakeOff() : AirTurn->Land();
			return;
		}
		if (AbilitySystemComponent->TryActivateAbility(Spec->Handle))
		{
			EnrageFinale = EEnrageFinale::Running;
			UE_LOG(LogknightAI, Log, TEXT("%s: enrage finale %s started."), *GetName(), *GetNameSafe(FinaleClass));
		}
		return;
	}
	case EEnrageFinale::Running:
		// 지쳐 내려앉는 순간(State.Monster.Collapsed) 분노를 푼다. 그 뒤의 딜 타임은 평소 상태다.
		if (!Spec->IsActive() || AbilitySystemComponent->HasMatchingGameplayTag(knightGameplayTags::State_Monster_Collapsed))
		{
			UE_LOG(LogknightAI, Log, TEXT("%s: enrage finale ended."), *GetName());
			EndEnrageFinale();
		}
		return;
	default:
		EndEnrageFinale();
	}
}

void AknightMonsterCharacterBase::EndEnrageFinale()
{
	GetWorldTimerManager().ClearTimer(EnrageFinaleTimer);
	const bool bTagsAdded = EnrageFinale == EEnrageFinale::Pending || EnrageFinale == EEnrageFinale::Running;
	EnrageFinale = EEnrageFinale::None;
	if (bTagsAdded)
	{
		AbilitySystemComponent->RemoveLooseGameplayTag(knightGameplayTags::State_Monster_EnrageFinale);
		AbilitySystemComponent->RemoveLooseGameplayTag(knightGameplayTags::State_Monster_Enraged);
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
