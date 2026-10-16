#include "AbilitySystem/Abilities/knightGameplayAbility_MeleeAttack.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "AbilitySystem/knightAbilitySystemComponent.h"
#include "AbilitySystem/Abilities/knightGameplayAbility_StanceSwitch.h"
#include "Combat/knightCombatLibrary.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/Controller.h"
#include "Input/knightInputBufferComponent.h"
#include "TimerManager.h"
#include "System/knightGameplayTags.h"
#include "knight.h"

namespace
{
	// 이동 캔슬 검사 간격. 짧게 뗐다 누르는 입력도 놓치지 않게 짧게 둔다. (2026-10-07 0.05 → 0.02)
	constexpr float MoveCancelCheckInterval = 0.02f;

	// 락온 상태가 아닐 때 공격하면 화면 중앙(카메라/컨트롤러 수평 방향)을 바라보게 한다.
	void FaceScreenCenterIfUnlocked(ACharacter* Character, const UAbilitySystemComponent* ASC)
	{
		if (!Character || (ASC && ASC->HasMatchingGameplayTag(knightGameplayTags::State_LockedOn)))
		{
			return;
		}
		if (const AController* const Controller = Character->GetController())
		{
			Character->SetActorRotation(FRotator(0.f, Controller->GetControlRotation().Yaw, 0.f));
		}
	}
}

UknightGameplayAbility_MeleeAttack::UknightGameplayAbility_MeleeAttack(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ActivationPolicy = EknightAbilityActivationPolicy::OnInputTriggered;
	ActivationGroup = EknightAbilityActivationGroup::Exclusive_Replaceable;
	SetAssetTags(FGameplayTagContainer(knightGameplayTags::Ability_Attack));
	ActivationBlockedTags.AddTag(knightGameplayTags::State_Dead);
	ActivationBlockedTags.AddTag(knightGameplayTags::State_HitReact);
	// 한 번 휘두를 때 쓰는 행동 에너지. 2026-10-09 임시값(최대 100, 회복 초당 25 기준), 공격 BP마다 조절.
	StaminaCost = 10.f;
}

void UknightGameplayAbility_MeleeAttack::ApplyCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo) const
{
	// 검을 뽑기만 하는 누름(bAttackAfterDraw가 꺼져 있을 때)은 행동 에너지를 쓰지 않는다. (2026-10-09)
	if (!bDrawOnlyActivation)
	{
		Super::ApplyCost(Handle, ActorInfo, ActivationInfo);
	}
}

bool UknightGameplayAbility_MeleeAttack::DoesAbilitySatisfyTagRequirements(const UAbilitySystemComponent& AbilitySystemComponent,
	const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	const FGameplayTagContainer& OwnedTags = AbilitySystemComponent.GetOwnedGameplayTags();
	if (ComboLinkFromTags.IsEmpty() || !OwnedTags.HasAny(ComboLinkFromTags))
	{
		return Super::DoesAbilitySatisfyTagRequirements(AbilitySystemComponent, SourceTags, TargetTags, OptionalRelevantTags);
	}

	// 콤보 이어받기: 엔진 검사(UGameplayAbility::DoesAbilitySatisfyTagRequirements)와 같되 ActivationBlockedTags 자리에 ComboLinkBlockedTags. (2026-10-15)
	const bool bBlocked = OwnedTags.HasAny(ComboLinkBlockedTags)
		|| GetAssetTags().HasAny(AbilitySystemComponent.GetBlockedAbilityTags())
		|| (SourceTags && SourceTags->HasAny(SourceBlockedTags))
		|| (TargetTags && TargetTags->HasAny(TargetBlockedTags))
		|| AbilitySystemComponent.AreAbilityTagsBlocked(GetAssetTags());
	const bool bMissing = !OwnedTags.HasAll(ActivationRequiredTags)
		|| (SourceTags && !SourceTags->HasAll(SourceRequiredTags))
		|| (TargetTags && !TargetTags->HasAll(TargetRequiredTags));
	if (bBlocked || bMissing)
	{
		if (OptionalRelevantTags)
		{
			const UAbilitySystemGlobals& Globals = UAbilitySystemGlobals::Get();
			OptionalRelevantTags->AddTag(bBlocked ? Globals.ActivateFailTagsBlockedTag : Globals.ActivateFailTagsMissingTag);
		}
		return false;
	}
	return SatisfiesTagRelationshipRequirements(AbilitySystemComponent, OptionalRelevantTags);
}

void UknightGameplayAbility_MeleeAttack::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	if (!AttackMontage)
	{
		UE_LOG(LogknightAbility, Error, TEXT("%s has no AttackMontage."), *GetName());
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	bDrawOnlyActivation = DrawMontage && !bAttackAfterDraw && !IsWeaponInHand();
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// 락온 중이 아니면 화면 중앙(카메라 방향)을 바로 보고 공격한다.
	FaceScreenCenterIfUnlocked(Cast<ACharacter>(GetAvatarActorFromActorInfo()), GetAbilitySystemComponentFromActorInfo());

	bAttackStarted = false;
	if (DrawMontage && !IsWeaponInHand())
	{
		// (2026-10-15) 뽑을 때는 항상 한손 스탠스로 시작한다.
		UknightGameplayAbility_StanceSwitch::ResetToOneHanded(GetAbilitySystemComponentFromActorInfo());
		// 발도도 행동 속도를 따른다. 공격으로 넘어가는 시점(DrawAttackStartTime)도 같은 배속으로 맞춘다. (2026-10-08)
		const float DrawRate = DrawPlayRate * GetActionSpeed();
		UAbilityTask_PlayMontageAndWait* const DrawTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
			this, TEXT("Draw"), DrawMontage, DrawRate);
		DrawTask->OnBlendOut.AddDynamic(this, &ThisClass::OnDrawFinished);
		DrawTask->OnCompleted.AddDynamic(this, &ThisClass::OnDrawFinished);
		DrawTask->OnInterrupted.AddDynamic(this, &ThisClass::OnDrawInterrupted);
		DrawTask->OnCancelled.AddDynamic(this, &ThisClass::OnDrawInterrupted);
		DrawTask->ReadyForActivation();
		if (UWorld* const World = GetWorld())
		{
			if (!bAttackAfterDraw)
			{
				World->GetTimerManager().SetTimer(DrawTimer, this, &ThisClass::CheckDrawMoveCancel, 0.05f, true);
			}
			else if (DrawAttackStartTime > 0.f)
			{
				World->GetTimerManager().SetTimer(DrawTimer, this, &ThisClass::OnDrawFinished, DrawAttackStartTime / DrawRate, false);
			}
		}
		return;
	}
	StartAttack();
}

void UknightGameplayAbility_MeleeAttack::OnDrawFinished()
{
	if (!IsActive() || bAttackStarted)
	{
		return;
	}
	if (bAttackAfterDraw)
	{
		StartAttack();
	}
	else
	{
		// Draw only: the next press attacks with the sword in hand.
		EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, false);
	}
}

void UknightGameplayAbility_MeleeAttack::CheckDrawMoveCancel()
{
	const ACharacter* const Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	if (!IsActive() || bAttackStarted || !Character || !IsWeaponInHand() || Character->GetLastMovementInputVector().IsNearlyZero())
	{
		return;
	}
	if (UWorld* const World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DrawTimer);
	}
	MontageStop(RecoveryCancelBlendOut);
	if (IsActive())
	{
		EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, false);
	}
}

void UknightGameplayAbility_MeleeAttack::OnDrawInterrupted()
{
	// Starting the attack montage interrupts the draw; anything else (a hit, death) ends the attack.
	if (IsActive() && !bAttackStarted)
	{
		EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, true);
	}
}

bool UknightGameplayAbility_MeleeAttack::IsWeaponInHand() const
{
	const ACharacter* const Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	const USkeletalMeshComponent* const CharacterMesh = Character ? Character->GetMesh() : nullptr;
	if (!CharacterMesh)
	{
		return true;
	}
	TArray<UMeshComponent*> Meshes;
	Character->GetComponents<UMeshComponent>(Meshes);
	for (const UMeshComponent* const Mesh : Meshes)
	{
		if (Mesh != CharacterMesh && Mesh->ComponentHasTag(WeaponComponentTag))
		{
			return Mesh->GetAttachSocketName() == WeaponHandSocket;
		}
	}
	return true;
}

void UknightGameplayAbility_MeleeAttack::StartAttack()
{
	bAttackStarted = true;
	if (UWorld* const World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DrawTimer);
	}
	FaceScreenCenterIfUnlocked(Cast<ACharacter>(GetAvatarActorFromActorInfo()), GetAbilitySystemComponentFromActorInfo());
	const bool bWarp = UknightCombatLibrary::SetAttackWarpTarget(GetAvatarActorFromActorInfo(), WarpTargetName, MaxWarpDistance,
		WarpStopDistance, bUseSoftTarget, SoftTargetRange, SoftTargetAngle);
	FVector WarpLocation;
	FRotator WarpRotation;
	float WarpTravel = 0.f;
	const bool bLunge = bWarp && UknightCombatLibrary::ComputeAttackWarp(GetAvatarActorFromActorInfo(), MaxWarpDistance, WarpStopDistance,
		bUseSoftTarget, SoftTargetRange, SoftTargetAngle, WarpLocation, WarpRotation, WarpTravel);

	UAbilityTask_WaitGameplayEvent* const HitTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, knightGameplayTags::Event_Melee_Hit, nullptr, false, true);
	HitTask->EventReceived.AddDynamic(this, &ThisClass::OnMeleeHitEvent);
	HitTask->ReadyForActivation();

	// 실제 재생 속도 = PlayRate x 행동 속도. 돌진도 같은 배속을 넘겨야 모션과 같이 끝난다. (2026-10-08)
	// (2026-10-14) x 공격 배속(ASC AttackAnimationSpeed, 시험 치트 knight.Player.AttackSpeed).
	const float Rate = PlayRate * GetAttackSpeed();
	UAnimMontage* const Montage = GetStanceAttackMontage();
	UAbilityTask_PlayMontageAndWait* const MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, Montage, Rate, StartSection);
	MontageTask->OnCompleted.AddDynamic(this, &ThisClass::OnMontageFinished);
	MontageTask->OnBlendOut.AddDynamic(this, &ThisClass::OnMontageFinished);
	MontageTask->OnInterrupted.AddDynamic(this, &ThisClass::OnMontageFinished);
	MontageTask->OnCancelled.AddDynamic(this, &ThisClass::OnMontageFinished);
	MontageTask->ReadyForActivation();

	// (2026-10-14) 루트 모션 공격은 애니 + 모션 워핑이 이동을 맡는다. 코드 돌진은 제자리 애니일 때만.
	if (bLunge && bLungeWithCode)
	{
		UknightCombatLibrary::StartAttackLunge(this, Montage, Rate, WarpLocation, WarpTravel, MaxLungeTime);
	}
	if (ACharacter* const Character = Cast<ACharacter>(GetAvatarActorFromActorInfo()))
	{
		Character->SetAnimRootMotionTranslationScale(RootMotionTranslationScale);
		bRootMotionScaleApplied = true;
	}

	if (bCancelRecoveryOnMove)
	{
		// 공격 시작 때 누르고 있던 방향은 새 누름이 아니다.
		if (const ACharacter* const Character = Cast<ACharacter>(GetAvatarActorFromActorInfo()))
		{
			MovePress.Reset(*Character);
		}
		if (UWorld* const World = GetWorld())
		{
			World->GetTimerManager().SetTimer(MoveCancelTimer, this, &ThisClass::CheckMoveCancel, MoveCancelCheckInterval, true);
		}
	}
}

UAnimMontage* UknightGameplayAbility_MeleeAttack::GetStanceAttackMontage() const
{
	// (2026-10-15) 한손 스탠스면 한손 몽타주. 비어 있으면 양손 몽타주를 그대로 쓴다.
	if (OneHandedAttackMontage && !UknightGameplayAbility_StanceSwitch::IsTwoHanded(GetAbilitySystemComponentFromActorInfo()))
	{
		return OneHandedAttackMontage;
	}
	return AttackMontage;
}

void UknightGameplayAbility_MeleeAttack::CheckMoveCancel()
{
	const UAbilitySystemComponent* const ASC = GetAbilitySystemComponentFromActorInfo();
	const ACharacter* const Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	if (!ASC || !Character)
	{
		return;
	}
	// 캔슬 창 밖에서도 새 누름은 계속 추적한다 → 창이 열리기 전에 다시 눌러 둔 것도 창이 열리면 캔슬한다. (2026-10-07)
	MovePress.Update(*Character);
	if (!ASC->HasMatchingGameplayTag(knightGameplayTags::Window_Cancel))
	{
		return;
	}
	// (2026-10-14) 공격 후딜은 퀵스텝만 끊는다(ASC bOnlyDashCancelsAttackRecovery) → 이동으로는 끊지 않는다.
	if (const UknightAbilitySystemComponent* const KnightASC = GetKnightAbilitySystemComponentFromActorInfo())
	{
		if (KnightASC->IsAttackRecoveryDashOnly())
		{
			return;
		}
	}
	if (!bMoveCancelInComboWindow && ASC->HasMatchingGameplayTag(knightGameplayTags::Window_Combo))
	{
		return;
	}
	// 공격 전부터 누르고 있던 방향키로는 캔슬하지 않는다. 새로 누른 이동 입력만 캔슬한다.
	if (!MovePress.HasFreshPress() || Character->GetLastMovementInputVector().IsNearlyZero())
	{
		return;
	}
	// 공격·가드·대시 입력이 버퍼에 남아 있으면 이동 캔슬보다 그 입력을 우선한다. (2026-10-07)
	// 방향키를 누른 채 연타하면, 다음 공격이 나가기 전에 이동이 공격을 끊어 1타만 빠르게 반복되던 문제.
	// 연타를 멈추면(버퍼가 비면) 지금처럼 이동으로 바로 후딜을 캔슬할 수 있다.
	if (const UknightInputBufferComponent* const InputBuffer = Character->FindComponentByClass<UknightInputBufferComponent>())
	{
		if (InputBuffer->HasBufferedInput(InputBuffer->GetBufferedInputTag()))
		{
			return;
		}
	}
	if (UWorld* const World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(MoveCancelTimer);
	}
	MontageStop(RecoveryCancelBlendOut);
	if (IsActive())
	{
		EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, false);
	}
}

void UknightGameplayAbility_MeleeAttack::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (UWorld* const World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(MoveCancelTimer);
		World->GetTimerManager().ClearTimer(DrawTimer);
	}
	if (bRootMotionScaleApplied)
	{
		bRootMotionScaleApplied = false;
		if (ACharacter* const Character = Cast<ACharacter>(GetAvatarActorFromActorInfo()))
		{
			Character->SetAnimRootMotionTranslationScale(1.f);
		}
	}
	// 다음 단계로 교체되거나 피격 등으로 캔슬된 경우가 아니라, 정상적으로 끝난 공격만 콤보 유예를 준다.
	if (!bWasCancelled && IsActive())
	{
		KeepComboStepTags();
	}
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UknightGameplayAbility_MeleeAttack::KeepComboStepTags()
{
	UAbilitySystemComponent* const ASC = GetAbilitySystemComponentFromActorInfo();
	UWorld* const World = GetWorld();
	if (!ASC || !World || ComboGraceTime <= 0.f || ActivationOwnedTags.IsEmpty())
	{
		return;
	}
	// Super::EndAbility가 소유 태그를 빼기 전에 한 번 더 붙여 두고, 유예 시간이 지나면 뺀다.
	// 그동안 다음 단계(이 태그가 필요)는 발동할 수 있고, 1타(Combo 계열이 있으면 막힘)는 나가지 않는다.
	const FGameplayTagContainer Tags = ActivationOwnedTags;
	ASC->AddLooseGameplayTags(Tags);
	FTimerHandle GraceTimer;
	World->GetTimerManager().SetTimer(GraceTimer, FTimerDelegate::CreateWeakLambda(ASC, [ASC, Tags]()
	{
		ASC->RemoveLooseGameplayTags(Tags);
	}), ComboGraceTime, false);
}

void UknightGameplayAbility_MeleeAttack::OnMontageFinished()
{
	EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, false);
}

void UknightGameplayAbility_MeleeAttack::OnMeleeHitEvent(FGameplayEventData Payload)
{
	const FHitResult* const Hit = Payload.TargetData.Num() > 0 && Payload.TargetData.Get(0) ? Payload.TargetData.Get(0)->GetHitResult() : nullptr;
	if (!Hit)
	{
		return;
	}
	const EknightHitOutcome Outcome = UknightCombatLibrary::ApplyMeleeHit(GetAvatarActorFromActorInfo(), *Hit, HitParams);
	OnHitResolved(*Hit, Outcome);
}
