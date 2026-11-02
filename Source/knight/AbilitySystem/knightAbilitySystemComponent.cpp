#include "AbilitySystem/knightAbilitySystemComponent.h"

#include "AbilitySystem/knightAbilityTagRelationshipMapping.h"
#include "AbilitySystem/Attributes/knightAttributeSet.h"
#include "AbilitySystem/Attributes/knightCoreAttributeSet.h"
#include "Character/knightMovementComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "System/knightGameplayTags.h"
#include "HAL/IConsoleManager.h"
#include "Camera/CameraShakeBase.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Particles/ParticleSystem.h"
#include "Particles/ParticleSystemComponent.h"
#include "Sound/SoundBase.h"
#include "knight.h"

namespace
{
	// (2026-10-20) 스탠스별 공격 차이 비교용. 0이면 한손·양손 수치가 같다(모션만 다름).
	TAutoConsoleVariable<int32> CVarStanceModifiers(
		TEXT("knight.Player.StanceModifiers"), 1,
		TEXT("1: one/two-handed stance attack modifiers on (damage, poise, attack speed, stamina). 0: off."),
		ECVF_Cheat);
}

// ---- 그로기 자연 회복: 순수 계산 (테스트 대상) ----

namespace knightPoiseRecovery
{
	float Step(float CurrentPoise, float MaxPoise, float RecoveryRate, float DeltaSeconds,
		float SecondsSinceLastHit, float RecoveryDelay)
	{
		if (RecoveryRate <= 0.f || CurrentPoise <= 0.f || CurrentPoise >= MaxPoise || SecondsSinceLastHit < RecoveryDelay)
		{
			return CurrentPoise;
		}
		return FMath::Min(MaxPoise, CurrentPoise + RecoveryRate * FMath::Max(0.f, DeltaSeconds));
	}
}

// ---- 행동 에너지 자연 회복: 순수 계산 (테스트 대상, 2026-10-09) ----

namespace knightBoostRecovery
{
	float Step(float CurrentGauge, float MaxGauge, float RecoveryRate, float DeltaSeconds,
		float SecondsSinceLastSpend, float RecoveryDelay, bool bRecoveryPaused)
	{
		if (bRecoveryPaused || RecoveryRate <= 0.f || CurrentGauge >= MaxGauge || SecondsSinceLastSpend < RecoveryDelay)
		{
			return CurrentGauge;
		}
		return FMath::Min(MaxGauge, CurrentGauge + RecoveryRate * FMath::Max(0.f, DeltaSeconds));
	}
}

UknightAbilitySystemComponent::UknightAbilitySystemComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	FMemory::Memzero(ActivationGroupCounts, sizeof(ActivationGroupCounts));
}

void UknightAbilitySystemComponent::InitAbilityActorInfo(AActor* InOwnerActor, AActor* InAvatarActor)
{
	const FGameplayAbilityActorInfo* const ActorInfo = AbilityActorInfo.Get();
	const bool bHasNewAvatar = InAvatarActor && (!ActorInfo || InAvatarActor != ActorInfo->AvatarActor.Get());

	Super::InitAbilityActorInfo(InOwnerActor, InAvatarActor);

	if (bHasNewAvatar)
	{
		TryActivateAbilitiesOnSpawn();
	}
	StartPoiseRecovery();
	StartBoostRecovery();

	// (2026-10-14) 공격 후딜 길이를 재려고 캔슬 창이 열린 시각을 기록한다(IsAttackRecoveryDashOnly).
	if (!CancelWindowHandle.IsValid())
	{
		CancelWindowHandle = RegisterGameplayTagEvent(knightGameplayTags::Window_Cancel, EGameplayTagEventType::AnyCountChange)
			.AddUObject(this, &ThisClass::HandleCancelWindowChanged);
	}
}

void UknightAbilitySystemComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (const UWorld* const World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PoiseRecoveryTimer);
		World->GetTimerManager().ClearTimer(BoostRecoveryTimer);
	}
	if (CancelWindowHandle.IsValid())
	{
		UnregisterGameplayTagEvent(CancelWindowHandle, knightGameplayTags::Window_Cancel, EGameplayTagEventType::AnyCountChange);
		CancelWindowHandle.Reset();
	}
	Super::EndPlay(EndPlayReason);
}

void UknightAbilitySystemComponent::HandleCancelWindowChanged(const FGameplayTag Tag, int32 NewCount)
{
	// 수가 늘면 새 창이 열린 것. 콤보로 이어질 때 앞 창이 닫히기 전에 다음 창이 열려도 새로 센다.
	if (NewCount > CancelWindowCount)
	{
		const UWorld* const World = GetWorld();
		CancelWindowOpenedTime = World ? World->GetTimeSeconds() : 0.0;
	}
	CancelWindowCount = NewCount;
}

void UknightAbilitySystemComponent::StartBoostRecovery()
{
	UWorld* const World = GetWorld();
	// 에디터의 BP 미리보기 같은 게임 밖 월드에서는 켜지 않는다.
	if (!World || !World->IsGameWorld() || World->GetTimerManager().IsTimerActive(BoostRecoveryTimer))
	{
		return;
	}
	World->GetTimerManager().SetTimer(BoostRecoveryTimer, this, &ThisClass::TickBoostRecovery, BoostRecoveryInterval, true);
}

void UknightAbilitySystemComponent::TickBoostRecovery()
{
	// 행동 에너지는 나이트(메카 속성 세트)만 있다. 몬스터는 여기서 끝난다.
	const UknightAttributeSet* const MechSet = GetSet<UknightAttributeSet>();
	const UWorld* const World = GetWorld();
	if (!MechSet || !World || HasMatchingGameplayTag(knightGameplayTags::State_Dead))
	{
		return;
	}

	const FGameplayAttribute GaugeAttribute = UknightAttributeSet::GetBoostGaugeAttribute();
	const float CurrentGauge = GetNumericAttributeBase(GaugeAttribute);
	// 달리는 중이거나 공격이 아직 끝나지 않았으면(콤보 창·후딜) 멈춘다. 멈춘 동안에도 대기 시간은 흘러서,
	// 대기 시간이 이미 지났으면 행동이 끝나는 순간 바로 차기 시작한다. (2026-10-16)
	const bool bPaused = (bPauseBoostRecoveryWhileRunning && IsAvatarRunning())
		|| (bHoldBoostRecoveryDuringAction && IsActionHoldingBoostRecovery());
	// (2026-10-26) 가드를 올리고 있으면 회복 속도를 줄인다(GuardBoostRecoveryScale).
	const float RecoveryRate = MechSet->GetBoostRecoveryRate()
		* (HasMatchingGameplayTag(knightGameplayTags::State_Guarding) ? GuardBoostRecoveryScale : 1.f);
	const float NewGauge = knightBoostRecovery::Step(CurrentGauge, MechSet->GetMaxBoostGauge(), RecoveryRate,
		BoostRecoveryInterval, World->GetTimeSeconds() - MechSet->GetLastBoostSpendTime(), MechSet->GetBoostRecoveryDelay(), bPaused);
	if (NewGauge != CurrentGauge)
	{
		SetNumericAttributeBase(GaugeAttribute, NewGauge);
	}
}

bool UknightAbilitySystemComponent::IsAvatarRunning() const
{
	const ACharacter* const Character = Cast<ACharacter>(GetAvatarActor());
	const UknightMovementComponent* const Movement = Character ? Cast<UknightMovementComponent>(Character->GetCharacterMovement()) : nullptr;
	return Movement && Movement->IsRunning();
}

void UknightAbilitySystemComponent::StartPoiseRecovery()
{
	UWorld* const World = GetWorld();
	// 에디터의 BP 미리보기 같은 게임 밖 월드에서는 켜지 않는다.
	if (!World || !World->IsGameWorld() || World->GetTimerManager().IsTimerActive(PoiseRecoveryTimer))
	{
		return;
	}
	World->GetTimerManager().SetTimer(PoiseRecoveryTimer, this, &ThisClass::TickPoiseRecovery, PoiseRecoveryInterval, true);
}

void UknightAbilitySystemComponent::TickPoiseRecovery()
{
	const UknightCoreAttributeSet* const CoreSet = GetSet<UknightCoreAttributeSet>();
	const UWorld* const World = GetWorld();
	if (!CoreSet || !World || HasMatchingGameplayTag(knightGameplayTags::State_Dead))
	{
		return;
	}

	const FGameplayAttribute PoiseAttribute = UknightCoreAttributeSet::GetPoiseAttribute();
	const float CurrentPoise = GetNumericAttributeBase(PoiseAttribute);
	const float NewPoise = knightPoiseRecovery::Step(CurrentPoise, CoreSet->GetMaxPoise(), CoreSet->GetPoiseRecoveryRate(),
		PoiseRecoveryInterval, World->GetTimeSeconds() - CoreSet->GetLastHitTime(), PoiseRecoveryDelay);
	if (NewPoise != CurrentPoise)
	{
		// 몬스터 반응(UknightGameplayAbility_MonsterReaction)과 같은 방식으로 기본값을 바꾼다.
		SetNumericAttributeBase(PoiseAttribute, NewPoise);
	}
}

void UknightAbilitySystemComponent::TryActivateAbilitiesOnSpawn()
{
	ABILITYLIST_SCOPE_LOCK();
	for (const FGameplayAbilitySpec& Spec : ActivatableAbilities.Items)
	{
		if (const UknightGameplayAbility* const AbilityCDO = Cast<UknightGameplayAbility>(Spec.Ability))
		{
			AbilityCDO->TryActivateAbilityOnSpawn(AbilityActorInfo.Get(), Spec);
		}
	}
}

void UknightAbilitySystemComponent::AbilityInputTagPressed(const FGameplayTag& InputTag)
{
	if (!InputTag.IsValid())
	{
		return;
	}
	for (const FGameplayAbilitySpec& Spec : ActivatableAbilities.Items)
	{
		if (Spec.Ability && Spec.GetDynamicSpecSourceTags().HasTagExact(InputTag))
		{
			InputPressedSpecHandles.AddUnique(Spec.Handle);
			InputHeldSpecHandles.AddUnique(Spec.Handle);
			// 누른 순간 막혀 있으면 누르고 있는 동안 다시 시도한다(가드). 발동하면 NotifyAbilityActivated에서 빠진다. (2026-10-15)
			const UknightGameplayAbility* const AbilityCDO = Cast<UknightGameplayAbility>(Spec.Ability);
			if (AbilityCDO && AbilityCDO->ShouldRetryPressWhileHeld() && !Spec.IsActive())
			{
				PendingHeldPressHandles.AddUnique(Spec.Handle);
			}
		}
	}
}

void UknightAbilitySystemComponent::AbilityInputTagReleased(const FGameplayTag& InputTag)
{
	if (!InputTag.IsValid())
	{
		return;
	}
	for (const FGameplayAbilitySpec& Spec : ActivatableAbilities.Items)
	{
		if (Spec.Ability && Spec.GetDynamicSpecSourceTags().HasTagExact(InputTag))
		{
			InputReleasedSpecHandles.AddUnique(Spec.Handle);
			InputHeldSpecHandles.Remove(Spec.Handle);
			PendingHeldPressHandles.Remove(Spec.Handle);
		}
	}
}

void UknightAbilitySystemComponent::ProcessAbilityInput(float DeltaTime, bool bGamePaused)
{
	if (HasMatchingGameplayTag(knightGameplayTags::State_InputBlocked))
	{
		ClearAbilityInput();
		return;
	}

	RecentlyActivatedInputTags.Reset();
	TArray<FGameplayAbilitySpecHandle, TInlineAllocator<8>> AbilitiesToActivate;

	// 누르고 있는 입력: WhileInputActive 어빌리티는 발동할 때까지 매 프레임 다시 시도한다.
	for (const FGameplayAbilitySpecHandle& Handle : InputHeldSpecHandles)
	{
		const FGameplayAbilitySpec* const Spec = FindAbilitySpecFromHandle(Handle);
		if (Spec && Spec->Ability && !Spec->IsActive())
		{
			const UknightGameplayAbility* const AbilityCDO = Cast<UknightGameplayAbility>(Spec->Ability);
			// (2026-10-15) 누른 순간 막혔던 bRetryPressWhileHeld 어빌리티(가드)도 누르고 있는 동안 다시 시도한다.
			if (AbilityCDO && (AbilityCDO->GetActivationPolicy() == EknightAbilityActivationPolicy::WhileInputActive
				|| PendingHeldPressHandles.Contains(Handle)))
			{
				AbilitiesToActivate.AddUnique(Spec->Handle);
			}
		}
	}

	// 이번 프레임에 누른 입력: 실행 중인 어빌리티는 누름을 전달받고, 대기 중인 OnInputTriggered 어빌리티는 발동한다.
	for (const FGameplayAbilitySpecHandle& Handle : InputPressedSpecHandles)
	{
		FGameplayAbilitySpec* const Spec = FindAbilitySpecFromHandle(Handle);
		if (!Spec || !Spec->Ability)
		{
			continue;
		}
		Spec->InputPressed = true;
		const UknightGameplayAbility* const AbilityCDO = Cast<UknightGameplayAbility>(Spec->Ability);
		const bool bTriggeredByPress = AbilityCDO && AbilityCDO->GetActivationPolicy() == EknightAbilityActivationPolicy::OnInputTriggered;
		if (Spec->IsActive())
		{
			AbilitySpecInputPressed(*Spec);
			// 재발동을 허용하는 어빌리티(퀵스텝)는 실행 중에도 발동을 시도한다. 받아들일지는 어빌리티의 CanActivateAbility가 정한다. (2026-10-08)
			if (bTriggeredByPress && AbilityCDO->CanRetriggerWhileActive())
			{
				AbilitiesToActivate.AddUnique(Spec->Handle);
			}
		}
		else if (bTriggeredByPress)
		{
			AbilitiesToActivate.AddUnique(Spec->Handle);
		}
	}

	// 누름 처리를 끝낸 뒤 발동해야, 한 번의 누름으로 발동한 어빌리티가 같은 누름 이벤트를 또 받지 않는다.
	// 같은 입력 태그에 묶인 어빌리티는 한 번에 하나만 발동한다 (입력 버퍼의 TryActivateAbilityByInputTag와 같은 규칙).
	// 콤보처럼 단계들이 같은 키를 쓰면, 2단계가 발동하며 생긴 태그로 3단계까지 같은 프레임에 연달아 발동해 2단계를 건너뛸 수 있다.
	// (앞 단계 몽타주의 Window.Combo는 노티파이가 끝나는 다음 애니메이션 업데이트까지 남아 있다)
	FGameplayTagContainer ActivatedInputTags;
	for (const FGameplayAbilitySpecHandle& Handle : AbilitiesToActivate)
	{
		const FGameplayAbilitySpec* const Spec = FindAbilitySpecFromHandle(Handle);
		if (!Spec)
		{
			continue;
		}
		// 동적 소스 태그에는 InputTag만 들어 있다 (UknightAbilitySet 참고).
		const FGameplayTagContainer InputTags = Spec->GetDynamicSpecSourceTags();
		if (InputTags.HasAnyExact(ActivatedInputTags))
		{
			continue;
		}
		if (TryActivateAbility(Handle))
		{
			ActivatedInputTags.AppendTags(InputTags);
		}
	}

	// 이번 프레임에 뗀 입력: 실행 중인 어빌리티가 뗌을 전달받는다 (차지 공격은 여기서 차지를 끝낸다).
	for (const FGameplayAbilitySpecHandle& Handle : InputReleasedSpecHandles)
	{
		FGameplayAbilitySpec* const Spec = FindAbilitySpecFromHandle(Handle);
		if (!Spec || !Spec->Ability)
		{
			continue;
		}
		Spec->InputPressed = false;
		if (Spec->IsActive())
		{
			AbilitySpecInputReleased(*Spec);
		}
	}

	InputPressedSpecHandles.Reset();
	InputReleasedSpecHandles.Reset();
}

void UknightAbilitySystemComponent::ClearAbilityInput()
{
	InputPressedSpecHandles.Reset();
	InputReleasedSpecHandles.Reset();
	InputHeldSpecHandles.Reset();
	PendingHeldPressHandles.Reset();
}

bool UknightAbilitySystemComponent::TryActivateAbilityByInputTag(const FGameplayTag& InputTag)
{
	if (!InputTag.IsValid() || HasMatchingGameplayTag(knightGameplayTags::State_InputBlocked))
	{
		return false;
	}
	TArray<FGameplayAbilitySpecHandle, TInlineAllocator<4>> Candidates;
	for (const FGameplayAbilitySpec& Spec : ActivatableAbilities.Items)
	{
		// 실행 중인 어빌리티는 재발동을 허용할 때만 후보로 둔다 (ProcessAbilityInput과 같은 규칙).
		const UknightGameplayAbility* const AbilityCDO = Cast<UknightGameplayAbility>(Spec.Ability);
		const bool bCanTry = !Spec.IsActive() || (AbilityCDO && AbilityCDO->CanRetriggerWhileActive());
		if (Spec.Ability && bCanTry && Spec.GetDynamicSpecSourceTags().HasTagExact(InputTag))
		{
			Candidates.Add(Spec.Handle);
		}
	}
	for (const FGameplayAbilitySpecHandle& Handle : Candidates)
	{
		if (TryActivateAbility(Handle))
		{
			return true;
		}
	}
	return false;
}

void UknightAbilitySystemComponent::AbilitySpecInputPressed(FGameplayAbilitySpec& Spec)
{
	Super::AbilitySpecInputPressed(Spec);
	// UAbilityTask_WaitInputPress를 깨운다. 프로젝트의 모든 어빌리티는 인스턴스형이라 기본 인스턴스가 키를 가지고 있다.
	if (Spec.IsActive())
	{
		if (const UGameplayAbility* const Instance = Spec.GetPrimaryInstance())
		{
			InvokeReplicatedEvent(EAbilityGenericReplicatedEvent::InputPressed, Spec.Handle,
				Instance->GetCurrentActivationInfo().GetActivationPredictionKey());
		}
	}
}

void UknightAbilitySystemComponent::AbilitySpecInputReleased(FGameplayAbilitySpec& Spec)
{
	Super::AbilitySpecInputReleased(Spec);
	// UAbilityTask_WaitInputRelease를 깨운다.
	if (Spec.IsActive())
	{
		if (const UGameplayAbility* const Instance = Spec.GetPrimaryInstance())
		{
			InvokeReplicatedEvent(EAbilityGenericReplicatedEvent::InputReleased, Spec.Handle,
				Instance->GetCurrentActivationInfo().GetActivationPredictionKey());
		}
	}
}

bool UknightAbilitySystemComponent::IsActivationGroupBlocked(EknightAbilityActivationGroup Group, const FGameplayTagContainer& AbilityTags,
	const bool bIsComboStep) const
{
	const int32 BlockingCount = ActivationGroupCounts[static_cast<uint8>(EknightAbilityActivationGroup::Exclusive_Blocking)];
	const int32 ReplaceableCount = ActivationGroupCounts[static_cast<uint8>(EknightAbilityActivationGroup::Exclusive_Replaceable)];

	switch (Group)
	{
	case EknightAbilityActivationGroup::Independent:
		return false;
	case EknightAbilityActivationGroup::Exclusive_Blocking:
		return BlockingCount > 0;
	case EknightAbilityActivationGroup::Exclusive_Replaceable:
		if (BlockingCount > 0)
		{
			return true;
		}
		if (ReplaceableCount == 0)
		{
			return false;
		}
		// (2026-10-14) 공격 후딜은 퀵스텝만 끊는다. 콤보 창이 같이 열려 있으면 아래 규칙으로 다음 공격은 이어진다.
		// (2026-10-23) bAttacksCancelAttackRecovery면 후딜에도 다음 공격을 허용한다(콤보 창이 없는 공격용).
		// (2026-10-29) 꺼져 있으면 후딜 동안에는 같은 콤보의 다음 단계(bIsComboStep)만 나간다.
		if (HasMatchingGameplayTag(knightGameplayTags::Window_Cancel)
			&& (!IsAttackRecoveryDashOnly() || AbilityTags.HasTag(knightGameplayTags::Ability_Boost_Dash)
				|| (bAttacksCancelAttackRecovery && AbilityTags.HasTag(knightGameplayTags::Ability_Attack))
				|| (bIsComboStep && AbilityTags.HasTag(knightGameplayTags::Ability_Attack))))
		{
			return false;
		}
		// 콤보 창: 공격만 이어서 쓸 수 있다. 나머지는 캔슬 창을 기다린다.
		// (2026-10-29 사용자 결정) bOnlyComboStepsInComboWindow면 같은 콤보의 다음 단계만. 다른 공격은 후딜이 끝난 뒤.
		return !(HasMatchingGameplayTag(knightGameplayTags::Window_Combo) && AbilityTags.HasTag(knightGameplayTags::Ability_Attack)
			&& (bIsComboStep || !bOnlyComboStepsInComboWindow));
	default:
		checkf(false, TEXT("Invalid activation group %d"), static_cast<int32>(Group));
		return true;
	}
}

const FknightStanceModifiers& UknightAbilitySystemComponent::GetStanceModifiers() const
{
	static const FknightStanceModifiers Neutral;
	// 행동 에너지 속성(메카 속성 세트)이 있는 나이트만. 몬스터도 이 ASC를 쓰지만 스탠스가 없다.
	if (!bApplyStanceModifiers || CVarStanceModifiers.GetValueOnGameThread() == 0
		|| !HasAttributeSetForAttribute(UknightAttributeSet::GetBoostGaugeAttribute()))
	{
		return Neutral;
	}
	return HasMatchingGameplayTag(knightGameplayTags::State_Stance_TwoHanded) ? TwoHandedModifiers : OneHandedModifiers;
}

void UknightAbilitySystemComponent::PlayAttackImpactShake(const float PoiseDamage, const bool bGuarded) const
{
	if (!AttackImpactShake || AttackShakeMaxScale <= 0.f || AttackShakeFullPoise <= AttackShakeMinPoise)
	{
		return;
	}
	const APawn* const Pawn = Cast<APawn>(GetAvatarActor());
	const APlayerController* const PC = Pawn && Pawn->IsLocallyControlled() ? Pawn->GetController<APlayerController>() : nullptr;
	if (!PC || !PC->PlayerCameraManager)
	{
		return;
	}
	const float Alpha = FMath::Clamp((PoiseDamage - AttackShakeMinPoise) / (AttackShakeFullPoise - AttackShakeMinPoise), 0.f, 1.f);
	const float Scale = Alpha * AttackShakeMaxScale * (bGuarded ? AttackShakeGuardedRatio : 1.f);
	const UWorld* const World = GetWorld();
	const double Now = World ? World->GetRealTimeSeconds() : 0.0;
	if (Scale > KINDA_SMALL_NUMBER && Now - LastAttackShakeTime >= AttackShakeCooldown)
	{
		LastAttackShakeTime = Now;
		// 화면 흔들림 설정(CameraShakeScale)은 AknightPlayerCameraManager::StartCameraShake가 곱한다.
		PC->PlayerCameraManager->StartCameraShake(AttackImpactShake, Scale);
	}
}

void UknightAbilitySystemComponent::PlayAttackImpactEffects(const FHitResult& Hit, const bool bGuarded, const bool bWeakpoint,
	const float PoiseDamage, const float PartMultiplier) const
{
	const float WeakScale = bWeakpoint && !bGuarded ? AttackImpactWeakpointScale : 1.f;
	UWorld* const World = GetWorld();
	if (!World)
	{
		return;
	}
	const FVector Location = Hit.ImpactPoint;
	const float EffectScale = AttackImpactEffectScale * WeakScale * (bGuarded ? AttackImpactGuardedEffectRatio : 1.f);
	if (EffectScale > KINDA_SMALL_NUMBER)
	{
		const FVector Normal = Hit.ImpactNormal.IsNearlyZero() ? FVector::UpVector : Hit.ImpactNormal;
		const FRotator Facing = bAttackImpactEffectUpAlongNormal ? FRotationMatrix::MakeFromZ(Normal).Rotator() : Normal.Rotation();
		for (UFXSystemAsset* const Effect : AttackImpactEffects)
		{
			if (UNiagaraSystem* const Niagara = Cast<UNiagaraSystem>(Effect))
			{
				UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, Niagara, Location, Facing, FVector(EffectScale),
					true, true, ENCPoolMethod::AutoRelease);
			}
			else if (UParticleSystem* const Cascade = Cast<UParticleSystem>(Effect))
			{
				UGameplayStatics::SpawnEmitterAtLocation(World, Cascade, Location, Facing, FVector(EffectScale), true,
					EPSCPoolMethod::AutoRelease);
			}
		}
	}
	// 공용 큐를 대신하면 큐의 기본 흔들림도 여기서 낸다(막혔을 때는 가드 큐가 따로 있다).
	if (!bGuarded && ShouldReplaceSharedHitCue() && AttackImpactShake && AttackImpactBaseShakeScale > 0.f)
	{
		const APawn* const Pawn = Cast<APawn>(GetAvatarActor());
		const APlayerController* const PC = Pawn && Pawn->IsLocallyControlled() ? Pawn->GetController<APlayerController>() : nullptr;
		if (PC && PC->PlayerCameraManager)
		{
			PC->PlayerCameraManager->StartCameraShake(AttackImpactShake, AttackImpactBaseShakeScale * WeakScale);
		}
	}
	const double Now = World->GetRealTimeSeconds();
	// (2026-10-29) 타격 종류별 소리: 약점 > 장갑 부위 > 무거운 타격 > 기본. 비어 있는 종류는 기본(AttackImpactSounds)을 쓴다.
	const TArray<TObjectPtr<USoundBase>>* SoundPool = &AttackImpactSounds;
	if (!bGuarded)
	{
		if (bWeakpoint && !AttackImpactWeakpointSounds.IsEmpty())
		{
			SoundPool = &AttackImpactWeakpointSounds;
		}
		else if (PartMultiplier < AttackImpactArmorPartMultiplier && !AttackImpactArmorSounds.IsEmpty())
		{
			SoundPool = &AttackImpactArmorSounds;
		}
		else if (PoiseDamage >= AttackImpactHeavyPoiseDamage && !AttackImpactHeavySounds.IsEmpty())
		{
			SoundPool = &AttackImpactHeavySounds;
		}
	}
	if (!SoundPool->IsEmpty() && AttackImpactSoundVolume > 0.f && Now - LastAttackImpactSoundTime >= AttackImpactSoundCooldown)
	{
		int32 Index = FMath::RandRange(0, SoundPool->Num() - 1);
		if (SoundPool->Num() > 1 && Index == LastAttackImpactSoundIndex)
		{
			Index = (Index + 1) % SoundPool->Num();
		}
		LastAttackImpactSoundIndex = Index;
		if (USoundBase* const Sound = (*SoundPool)[Index])
		{
			LastAttackImpactSoundTime = Now;
			const float WeakVolume = SoundPool == &AttackImpactWeakpointSounds ? 1.f : FMath::Sqrt(WeakScale);
			UGameplayStatics::PlaySoundAtLocation(World, Sound, Location, AttackImpactSoundVolume * (bGuarded ? 0.7f : 1.f) * WeakVolume);
		}
	}
}

bool UknightAbilitySystemComponent::IsAttackRecoveryDashOnly() const
{
	if (!bOnlyDashCancelsAttackRecovery || !HasMatchingGameplayTag(knightGameplayTags::Window_Cancel))
	{
		return false;
	}
	// 후딜 길이가 지났으면(검을 들어 올리는 꼬리 동작) 예전처럼 무엇이든 끊는다.
	const UWorld* const World = GetWorld();
	if (!World || World->GetTimeSeconds() - CancelWindowOpenedTime >= AttackRecoveryDashOnlyTime)
	{
		return false;
	}
	// 캔슬 창을 연 게 공격인지 본다. 가드·퀵스텝 경직도 캔슬 창을 열지만 그건 지금처럼 무엇이든 끊을 수 있다.
	return IsExclusiveAttackActive();
}

bool UknightAbilitySystemComponent::IsExclusiveAttackActive() const
{
	for (const FGameplayAbilitySpec& Spec : ActivatableAbilities.Items)
	{
		if (!Spec.IsActive())
		{
			continue;
		}
		for (const UGameplayAbility* const Instance : Spec.GetAbilityInstances())
		{
			const UknightGameplayAbility* const KnightAbility = Cast<UknightGameplayAbility>(Instance);
			if (KnightAbility && KnightAbility->IsActive()
				&& KnightAbility->GetActivationGroup() == EknightAbilityActivationGroup::Exclusive_Replaceable
				&& KnightAbility->GetAssetTags().HasTag(knightGameplayTags::Ability_Attack))
			{
				return true;
			}
		}
	}
	return false;
}

bool UknightAbilitySystemComponent::IsActionHoldingBoostRecovery() const
{
	// 다음 콤보를 이어 갈 수 있는 동안(콤보 창)과 후딜(퀵스텝만 되는 시간)에는 회복을 미룬다. (2026-10-16)
	if (HasMatchingGameplayTag(knightGameplayTags::Window_Combo) || IsAttackRecoveryDashOnly())
	{
		return true;
	}
	// (2026-10-26 사용자 결정) 차지를 모으는 동안(State.Charging, 해방 전이라 아직 독립 그룹)도 회복을 미룬다.
	// 해방한 뒤에는 교체 가능 배타 공격이 되어 아래 일반 공격 규칙을 받는다.
	if (HasMatchingGameplayTag(knightGameplayTags::State_Charging) && !IsExclusiveAttackActive())
	{
		return true;
	}
	// 캔슬 창이 열리기 전 = 아직 휘두르는 중. 캔슬 창이 열려 있으면(후딜 뒤 꼬리 동작) 이동으로 끊을 수 있으니 회복한다.
	return !HasMatchingGameplayTag(knightGameplayTags::Window_Cancel) && IsExclusiveAttackActive();
}

void UknightAbilitySystemComponent::CancelActivationGroupAbilities(EknightAbilityActivationGroup Group, const UknightGameplayAbility* IgnoreAbility)
{
	ABILITYLIST_SCOPE_LOCK();
	for (FGameplayAbilitySpec& Spec : ActivatableAbilities.Items)
	{
		if (!Spec.IsActive())
		{
			continue;
		}
		for (UGameplayAbility* const Instance : Spec.GetAbilityInstances())
		{
			UknightGameplayAbility* const KnightAbility = Cast<UknightGameplayAbility>(Instance);
			if (KnightAbility && KnightAbility != IgnoreAbility && KnightAbility->IsActive()
				&& KnightAbility->GetActivationGroup() == Group && KnightAbility->CanBeCanceled())
			{
				KnightAbility->CancelAbility(Spec.Handle, AbilityActorInfo.Get(), KnightAbility->GetCurrentActivationInfo(), true);
			}
		}
	}
}

void UknightAbilitySystemComponent::NotifyActivationGroupChanged(const UknightGameplayAbility* Ability, EknightAbilityActivationGroup OldGroup,
	EknightAbilityActivationGroup NewGroup)
{
	if (!Ability || OldGroup == NewGroup)
	{
		return;
	}
	int32& OldCount = ActivationGroupCounts[static_cast<uint8>(OldGroup)];
	if (ensure(OldCount > 0))
	{
		OldCount--;
	}
	ActivationGroupCounts[static_cast<uint8>(NewGroup)]++;
	if (NewGroup != EknightAbilityActivationGroup::Independent)
	{
		CancelActivationGroupAbilities(EknightAbilityActivationGroup::Exclusive_Replaceable, Ability);
	}
}

void UknightAbilitySystemComponent::NotifyAbilityActivated(const FGameplayAbilitySpecHandle Handle, UGameplayAbility* Ability)
{
	Super::NotifyAbilityActivated(Handle, Ability);

	// 그 누름으로 발동했으니 더 다시 시도하지 않는다(다른 행동 뒤 자동으로 다시 나가지 않게). (2026-10-15)
	PendingHeldPressHandles.Remove(Handle);

	const UknightGameplayAbility* const KnightAbility = Cast<UknightGameplayAbility>(Ability);
	if (!KnightAbility)
	{
		return;
	}
	if (const FGameplayAbilitySpec* const Spec = FindAbilitySpecFromHandle(Handle))
	{
		// 동적 소스 태그에는 InputTag만 들어 있다 (UknightAbilitySet 참고).
		RecentlyActivatedInputTags.AppendTags(Spec->GetDynamicSpecSourceTags());
	}

	const EknightAbilityActivationGroup Group = KnightAbility->GetActivationGroup();
	ActivationGroupCounts[static_cast<uint8>(Group)]++;

	// 새 배타 어빌리티는 실행 중이던 교체 가능한 배타 어빌리티를 대신한다.
	if (Group != EknightAbilityActivationGroup::Independent)
	{
		CancelActivationGroupAbilities(EknightAbilityActivationGroup::Exclusive_Replaceable, KnightAbility);
		// (2026-10-26) 차지를 모으는 중(State.Charging, 해방 전이라 독립 그룹)에 가드·퀵스텝 같은 배타 어빌리티가 나가면 차지를 취소한다.
		// 그대로 두면 차지가 끝나지 않고 새 행동과 같이 돌았다. 해방한 뒤의 차지는 배타 그룹이라 위에서 이미 끊긴다.
		if (HasMatchingGameplayTag(knightGameplayTags::State_Charging))
		{
			const FGameplayTagContainer ChargeTags(knightGameplayTags::Ability_Attack_Charge);
			CancelAbilities(&ChargeTags, nullptr, Ability);
		}
	}

	const int32 ExclusiveCount = ActivationGroupCounts[static_cast<uint8>(EknightAbilityActivationGroup::Exclusive_Replaceable)]
		+ ActivationGroupCounts[static_cast<uint8>(EknightAbilityActivationGroup::Exclusive_Blocking)];
	if (ExclusiveCount > 1)
	{
		UE_LOG(LogknightAbility, Error, TEXT("%d exclusive abilities are active at once (latest: %s)."), ExclusiveCount, *KnightAbility->GetName());
	}
}

void UknightAbilitySystemComponent::NotifyAbilityEnded(FGameplayAbilitySpecHandle Handle, UGameplayAbility* Ability, bool bWasCancelled)
{
	Super::NotifyAbilityEnded(Handle, Ability, bWasCancelled);

	if (const UknightGameplayAbility* const KnightAbility = Cast<UknightGameplayAbility>(Ability))
	{
		int32& Count = ActivationGroupCounts[static_cast<uint8>(KnightAbility->GetActivationGroup())];
		if (ensure(Count > 0))
		{
			Count--;
		}
	}
}

void UknightAbilitySystemComponent::ApplyAbilityBlockAndCancelTags(const FGameplayTagContainer& AbilityTags, UGameplayAbility* RequestingAbility,
	bool bEnableBlockTags, const FGameplayTagContainer& BlockTags, bool bExecuteCancelTags, const FGameplayTagContainer& CancelTags)
{
	FGameplayTagContainer ModifiedBlockTags = BlockTags;
	FGameplayTagContainer ModifiedCancelTags = CancelTags;
	if (TagRelationshipMapping)
	{
		TagRelationshipMapping->GetAbilityTagsToBlockAndCancel(AbilityTags, &ModifiedBlockTags, &ModifiedCancelTags);
	}
	Super::ApplyAbilityBlockAndCancelTags(AbilityTags, RequestingAbility, bEnableBlockTags, ModifiedBlockTags, bExecuteCancelTags, ModifiedCancelTags);
}

void UknightAbilitySystemComponent::SetTagRelationshipMapping(UknightAbilityTagRelationshipMapping* NewMapping)
{
	TagRelationshipMapping = NewMapping;
}

void UknightAbilitySystemComponent::GetAdditionalActivationTagRequirements(const FGameplayTagContainer& AbilityTags,
	FGameplayTagContainer& OutActivationRequired, FGameplayTagContainer& OutActivationBlocked) const
{
	if (TagRelationshipMapping)
	{
		TagRelationshipMapping->GetRequiredAndBlockedActivationTags(AbilityTags, &OutActivationRequired, &OutActivationBlocked);
	}
}
