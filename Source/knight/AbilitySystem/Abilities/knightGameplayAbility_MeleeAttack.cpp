#include "AbilitySystem/Abilities/knightGameplayAbility_MeleeAttack.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "AbilitySystemComponent.h"
#include "Combat/knightCombatLibrary.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "Input/knightInputBufferComponent.h"
#include "TimerManager.h"
#include "System/knightGameplayTags.h"
#include "knight.h"

namespace
{
	// 이동 캔슬 검사 간격. 짧게 뗐다 누르는 입력도 놓치지 않게 짧게 둔다. (2026-10-07 0.05 → 0.02)
	constexpr float MoveCancelCheckInterval = 0.02f;

	// 누른 채로 방향이 이 각도보다 많이 바뀌면 새 누름으로 본다. W → W+D가 45도라 그보다 작게 둔다.
	constexpr float FreshMovePressAngleDegrees = 30.f;
}

UknightGameplayAbility_MeleeAttack::UknightGameplayAbility_MeleeAttack(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ActivationPolicy = EknightAbilityActivationPolicy::OnInputTriggered;
	ActivationGroup = EknightAbilityActivationGroup::Exclusive_Replaceable;
	SetAssetTags(FGameplayTagContainer(knightGameplayTags::Ability_Attack));
	ActivationBlockedTags.AddTag(knightGameplayTags::State_Dead);
	ActivationBlockedTags.AddTag(knightGameplayTags::State_HitReact);
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
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	bAttackStarted = false;
	if (DrawMontage && !IsWeaponInHand())
	{
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
	const float Rate = PlayRate * GetActionSpeed();
	UAbilityTask_PlayMontageAndWait* const MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, AttackMontage, Rate, StartSection);
	MontageTask->OnCompleted.AddDynamic(this, &ThisClass::OnMontageFinished);
	MontageTask->OnBlendOut.AddDynamic(this, &ThisClass::OnMontageFinished);
	MontageTask->OnInterrupted.AddDynamic(this, &ThisClass::OnMontageFinished);
	MontageTask->OnCancelled.AddDynamic(this, &ThisClass::OnMontageFinished);
	MontageTask->ReadyForActivation();

	if (bLunge)
	{
		UknightCombatLibrary::StartAttackLunge(this, AttackMontage, Rate, WarpLocation, WarpTravel, MaxLungeTime);
	}

	if (bCancelRecoveryOnMove)
	{
		// 공격 시작 때 누르고 있던 방향은 새 누름이 아니다.
		bFreshMovePress = false;
		const ACharacter* const Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
		MoveInputBaseline = Character ? GetLocalMoveInput(*Character) : FVector2D::ZeroVector;
		if (UWorld* const World = GetWorld())
		{
			World->GetTimerManager().SetTimer(MoveCancelTimer, this, &ThisClass::CheckMoveCancel, MoveCancelCheckInterval, true);
		}
	}
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
	UpdateFreshMovePress(*Character);
	if (!ASC->HasMatchingGameplayTag(knightGameplayTags::Window_Cancel))
	{
		return;
	}
	if (!bMoveCancelInComboWindow && ASC->HasMatchingGameplayTag(knightGameplayTags::Window_Combo))
	{
		return;
	}
	// 공격 전부터 누르고 있던 방향키로는 캔슬하지 않는다. 새로 누른 이동 입력만 캔슬한다.
	if (!bFreshMovePress || Character->GetLastMovementInputVector().IsNearlyZero())
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

FVector2D UknightGameplayAbility_MeleeAttack::GetLocalMoveInput(const ACharacter& Character)
{
	// 월드 기준 입력을 컨트롤 회전(yaw) 기준으로 되돌린다. 공격 중 카메라를 돌려도 같은 키는 같은 방향으로 읽힌다.
	const FRotator YawRotation(0.f, Character.GetControlRotation().Yaw, 0.f);
	const FVector LocalInput = YawRotation.UnrotateVector(Character.GetLastMovementInputVector());
	return FVector2D(LocalInput.X, LocalInput.Y);
}

void UknightGameplayAbility_MeleeAttack::UpdateFreshMovePress(const ACharacter& Character)
{
	const FVector2D Input = GetLocalMoveInput(Character);
	if (Input.IsNearlyZero())
	{
		// 키를 뗐다 → 다음에 들어오는 입력은 새 누름.
		MoveInputBaseline = FVector2D::ZeroVector;
		return;
	}
	if (MoveInputBaseline.IsNearlyZero())
	{
		// 뗐다가 다시 누름 (또는 공격 시작 때 안 누르고 있다가 누름).
		bFreshMovePress = true;
		MoveInputBaseline = Input;
		return;
	}
	// 누른 채로 다른 키를 더 누르거나 바꿈(W → W+D, W → D 등): 처음 방향에서 크게 바뀌면 새 누름.
	const float MinDot = FMath::Cos(FMath::DegreesToRadians(FreshMovePressAngleDegrees));
	if (FVector2D::DotProduct(Input.GetSafeNormal(), MoveInputBaseline.GetSafeNormal()) < MinDot)
	{
		bFreshMovePress = true;
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
