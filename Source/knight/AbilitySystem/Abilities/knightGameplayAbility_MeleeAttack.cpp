#include "AbilitySystem/Abilities/knightGameplayAbility_MeleeAttack.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "AbilitySystem/knightAbilitySystemComponent.h"
#include "AbilitySystem/Abilities/knightGameplayAbility_StanceSwitch.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Combat/knightCombatLibrary.h"
#include "Combat/knightGroggyFeedbackComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "Input/knightInputBufferComponent.h"
#include "MotionWarpingComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "TimerManager.h"
#include "System/knightGameplayTags.h"
#include "knight.h"

namespace
{
	// 이동 캔슬 검사 간격. 짧게 뗐다 누르는 입력도 놓치지 않게 짧게 둔다. (2026-10-07 0.05 → 0.02)
	constexpr float MoveCancelCheckInterval = 0.02f;

	// 락온 상태가 아닐 때 공격하면 화면 중앙(카메라/컨트롤러 수평 방향)을 바라보게 한다.
	// bPreferMoveDirection이면 이동 입력 방향을 먼저, 입력이 없으면 이동 속도 방향을 보고, 둘 다 없을 때만 화면 중앙을 본다.
	void FaceScreenCenterIfUnlocked(ACharacter* Character, const UAbilitySystemComponent* ASC, const bool bPreferMoveDirection = false)
	{
		if (!Character || (ASC && ASC->HasMatchingGameplayTag(knightGameplayTags::State_LockedOn)))
		{
			return;
		}
		if (bPreferMoveDirection)
		{
			const FVector Input = Character->GetLastMovementInputVector();
			const FVector Velocity = Character->GetVelocity();
			// 입력은 0~1 크기, 속도는 cm/s. 아주 작은 값(스틱 떨림, 멈추기 직전)은 방향으로 보지 않는다.
			const FVector Move = Input.SizeSquared2D() > 0.01f ? Input : (Velocity.SizeSquared2D() > 100.f ? Velocity : FVector::ZeroVector);
			if (!Move.IsNearlyZero())
			{
				Character->SetActorRotation(FRotator(0.f, Move.Rotation().Yaw, 0.f));
				return;
			}
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
	const bool bComboLink = !ComboLinkFromTags.IsEmpty() && OwnedTags.HasAny(ComboLinkFromTags);
	// (2026-10-22) 칼이 등에 있으면 DrawIgnoredBlockedTags(먼 락온 등)로는 막지 않는다. CDO에서 불릴 수 있어 ASC의 아바타로 본다.
	const bool bDrawIgnore = DrawMontage && !DrawIgnoredBlockedTags.IsEmpty() && OwnedTags.HasAny(DrawIgnoredBlockedTags)
		&& !IsWeaponInHandFor(AbilitySystemComponent.GetAvatarActor(), WeaponHandSocket, WeaponComponentTag);
	if (!bComboLink && !bDrawIgnore)
	{
		return Super::DoesAbilitySatisfyTagRequirements(AbilitySystemComponent, SourceTags, TargetTags, OptionalRelevantTags);
	}

	// 엔진 검사(UGameplayAbility::DoesAbilitySatisfyTagRequirements)와 같되 ActivationBlockedTags를 바꿔 본다.
	// 콤보 이어받기면 ComboLinkBlockedTags(2026-10-15), 칼이 등에 있으면 DrawIgnoredBlockedTags를 뺀다(2026-10-22).
	FGameplayTagContainer EffectiveBlockedTags = bComboLink ? ComboLinkBlockedTags : ActivationBlockedTags;
	if (bDrawIgnore)
	{
		EffectiveBlockedTags.RemoveTags(DrawIgnoredBlockedTags);
	}
	const bool bBlocked = OwnedTags.HasAny(EffectiveBlockedTags)
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

	bAttackStarted = false;
	bStartupDelayDone = false;
	// 무작위 몽타주는 칼을 뽑아 스탠스가 정해진 뒤 처음 쓸 때 고른다(GetStanceAttackMontage).
	bRandomMontageChosen = false;
	ChosenRandomMontage.Reset();
	bHitCuePlayed = false;
	if (DrawMontage && !IsWeaponInHand())
	{
		// 락온 중이 아니면 화면 중앙(카메라 방향)을 보고 뽑는다. 공격은 StartAttack에서 한 번 더 맞춘다(뽑는 동안 카메라가 돌 수 있음).
		// (2026-10-20) 뽑기 없이 바로 공격하면 StartAttack만 돌린다(중복 호출 정리).
		FaceScreenCenterIfUnlocked(Cast<ACharacter>(GetAvatarActorFromActorInfo()), GetAbilitySystemComponentFromActorInfo(), bFaceMoveDirectionIfUnlocked);
		// (2026-10-15) 뽑을 때는 항상 한손 스탠스로 시작한다.
		UknightGameplayAbility_StanceSwitch::ResetToOneHanded(GetAbilitySystemComponentFromActorInfo());
		// (2026-10-22) 뽑는 동안에는 걷지 않는다. 뽑기만 하면 칼이 손에 들어올 때(CheckDrawMoveCancel),
		// 뽑고 바로 공격하면 공격이 시작할 때(StartAttack), 중간에 끊기면 EndAbility에서 푼다.
		if (bLockMovementWhileDrawing)
		{
			SetDrawMovementLocked(true);
		}
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

void UknightGameplayAbility_MeleeAttack::SetDrawMovementLocked(const bool bLock)
{
	if (bDrawMovementLocked == bLock)
	{
		return;
	}
	const ACharacter* const Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	AController* const Controller = Character ? Character->GetController() : nullptr;
	if (!Controller)
	{
		return;
	}
	// SetIgnoreMoveInput은 횟수를 센다. 다른 곳에서 건 잠금은 건드리지 않고 내가 건 만큼만 푼다.
	Controller->SetIgnoreMoveInput(bLock);
	bDrawMovementLocked = bLock;
}

void UknightGameplayAbility_MeleeAttack::CheckDrawMoveCancel()
{
	const ACharacter* const Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	if (!IsActive() || bAttackStarted || !Character || !IsWeaponInHand())
	{
		return;
	}
	// (2026-10-22) 칼이 손에 들어왔다: 이동 잠금을 푼다. 다음 검사부터 이동 입력이 들어오면 발도를 끝낸다.
	if (bDrawMovementLocked)
	{
		SetDrawMovementLocked(false);
	}
	if (Character->GetLastMovementInputVector().IsNearlyZero())
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
	return IsWeaponInHandFor(GetAvatarActorFromActorInfo(), WeaponHandSocket, WeaponComponentTag);
}

bool UknightGameplayAbility_MeleeAttack::IsWeaponInHandFor(const AActor* const Avatar, const FName HandSocket, const FName WeaponTag)
{
	const ACharacter* const Character = Cast<ACharacter>(Avatar);
	const USkeletalMeshComponent* const CharacterMesh = Character ? Character->GetMesh() : nullptr;
	if (!CharacterMesh)
	{
		return true;
	}
	TArray<UMeshComponent*> Meshes;
	Character->GetComponents<UMeshComponent>(Meshes);
	for (const UMeshComponent* const Mesh : Meshes)
	{
		if (Mesh != CharacterMesh && Mesh->ComponentHasTag(WeaponTag))
		{
			return Mesh->GetAttachSocketName() == HandSocket;
		}
	}
	return true;
}

AActor* UknightGameplayAbility_MeleeAttack::GetParriedWarpTarget() const
{
	const UknightAbilitySystemComponent* const ASC = Cast<UknightAbilitySystemComponent>(GetAbilitySystemComponentFromActorInfo());
	if (!bWarpToParriedAttacker || !ASC || ASC->HasMatchingGameplayTag(knightGameplayTags::State_LockedOn))
	{
		return nullptr;
	}
	AActor* const Target = ASC->GetParryCounterTarget();
	const UAbilitySystemComponent* const TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Target);
	return Target && !(TargetASC && TargetASC->HasMatchingGameplayTag(knightGameplayTags::State_Dead)) ? Target : nullptr;
}

void UknightGameplayAbility_MeleeAttack::UpdateTrackedWarpTarget()
{
	AActor* const Avatar = GetAvatarActorFromActorInfo();
	const ACharacter* const Character = Cast<ACharacter>(Avatar);
	UAnimInstance* const Anim = Character && Character->GetMesh() ? Character->GetMesh()->GetAnimInstance() : nullptr;
	UAnimMontage* const Montage = TrackWarpMontage.Get();
	UMotionWarpingComponent* const MotionWarping = Avatar ? Avatar->FindComponentByClass<UMotionWarpingComponent>() : nullptr;
	// 휘두르기 시작(워프 구간 끝)을 지나면 더 따라가지 않는다.
	if (!IsActive() || !Anim || !Montage || !MotionWarping || !Anim->Montage_IsPlaying(Montage)
		|| Anim->Montage_GetPosition(Montage) >= TrackWarpEndTime)
	{
		if (UWorld* const World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(TrackWarpTimer);
		}
		return;
	}
	// 지금 자리에서 대상 앞 지점을 구한 뒤(거리 제한 없이), 공격을 시작한 자리에서 MaxWarpDistance 안으로 자른다.
	const float Unlimited = MaxWarpDistance + 10000.f;
	FVector Location;
	FRotator Rotation;
	float Travel = 0.f;
	AActor* const ParriedTarget = GetParriedWarpTarget();
	const bool bHasTarget = ParriedTarget
		? UknightCombatLibrary::ComputeAttackWarpToActor(Avatar, ParriedTarget, Unlimited, WarpStopDistance, Location, Rotation, Travel)
		: UknightCombatLibrary::ComputeAttackWarp(Avatar, Unlimited, WarpStopDistance, bUseSoftTarget, SoftTargetRange, SoftTargetAngle,
			Location, Rotation, Travel);
	if (!bHasTarget)
	{
		return;
	}
	FVector FromStart = Location - TrackWarpStart;
	FromStart.Z = 0.f;
	if (FromStart.Size() > MaxWarpDistance)
	{
		const FVector Clamped = TrackWarpStart + FromStart.GetSafeNormal() * MaxWarpDistance;
		Location = FVector(Clamped.X, Clamped.Y, Location.Z);
	}
	MotionWarping->AddOrUpdateWarpTargetFromLocationAndRotation(WarpTargetName, Location, Rotation);
}

void UknightGameplayAbility_MeleeAttack::StartAttack()
{
	// (2026-10-29 사용자 요청) 선딜레이: 락온 중이면 LockOnStartupDelay, 양손이면 스탠스 감속분(bStanceSlowdownAsStartupDelay)만큼
	// 첫 자세로 멈춰 예열한 뒤 다시 여기로 온다.
	if (!bStartupDelayDone && GetWorld())
	{
		const UAbilitySystemComponent* const ASC = GetAbilitySystemComponentFromActorInfo();
		const float LockOnDelay = LockOnStartupDelay > 0.f && ASC && ASC->HasMatchingGameplayTag(knightGameplayTags::State_LockedOn)
			? LockOnStartupDelay : 0.f;
		const float Delay = BaseStartupDelay + LockOnDelay + GetStanceStartupDelay();
		if (Delay > 0.f)
		{
			BeginStartupDelay(Delay);
			return;
		}
	}
	// 예열에서 멈춰 둔 몽타주는 아래에서 같은 몽타주를 처음부터 다시 틀며 넘어간다. 이제부터는 EndAbility가 내릴 필요가 없다.
	StartupDelayMontage.Reset();
	bAttackStarted = true;
	SetDrawMovementLocked(false);
	if (const AActor* const Avatar = GetAvatarActorFromActorInfo())
	{
		if (UknightThrusterComponent* const Thrusters = Avatar->FindComponentByClass<UknightThrusterComponent>())
		{
			StopThrust(AttackStartThrustId);
			AttackStartThrustId = Thrusters->StartThrust(AttackStartThrust, Avatar->GetActorForwardVector());
		}
	}
	if (UWorld* const World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DrawTimer);
	}
	FVector WarpLocation;
	FRotator WarpRotation;
	float WarpTravel = 0.f;
	bool bLunge = false;
	if (AActor* const ParriedTarget = GetParriedWarpTarget())
	{
		// (2026-10-20) 패링 반격: 락온이 아니어도 패링한 적을 보고 그쪽으로 돌진한다.
		bLunge = UknightCombatLibrary::SetAttackWarpTargetToActor(GetAvatarActorFromActorInfo(), WarpTargetName, ParriedTarget,
			MaxWarpDistance, WarpStopDistance, WarpLocation, WarpRotation, WarpTravel);
		if (bLunge)
		{
			GetAvatarActorFromActorInfo()->SetActorRotation(FRotator(0.f, WarpRotation.Yaw, 0.f));
		}
	}
	else if (ForwardLungeDistance > 0.f)
	{
		// (2026-10-29 사용자 결정) 락온 대상으로 끌려가지 않고 앞으로 ForwardLungeDistance만큼(앞에 적·벽이 있으면 그 앞까지).
		FaceScreenCenterIfUnlocked(Cast<ACharacter>(GetAvatarActorFromActorInfo()), GetAbilitySystemComponentFromActorInfo(), bFaceMoveDirectionIfUnlocked);
		bLunge = SetForwardLungeWarpTarget(WarpLocation, WarpRotation, WarpTravel);
	}
	else
	{
		FaceScreenCenterIfUnlocked(Cast<ACharacter>(GetAvatarActorFromActorInfo()), GetAbilitySystemComponentFromActorInfo(), bFaceMoveDirectionIfUnlocked);
		const bool bWarp = UknightCombatLibrary::SetAttackWarpTarget(GetAvatarActorFromActorInfo(), WarpTargetName, MaxWarpDistance,
			WarpStopDistance, bUseSoftTarget, SoftTargetRange, SoftTargetAngle);
		bLunge = bWarp && UknightCombatLibrary::ComputeAttackWarp(GetAvatarActorFromActorInfo(), MaxWarpDistance, WarpStopDistance,
			bUseSoftTarget, SoftTargetRange, SoftTargetAngle, WarpLocation, WarpRotation, WarpTravel);
	}

	UAbilityTask_WaitGameplayEvent* const HitTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, knightGameplayTags::Event_Melee_Hit, nullptr, false, true);
	HitTask->EventReceived.AddDynamic(this, &ThisClass::OnMeleeHitEvent);
	HitTask->ReadyForActivation();

	// 실제 재생 속도 = PlayRate x 행동 속도. 돌진도 같은 배속을 넘겨야 모션과 같이 끝난다. (2026-10-08)
	// (2026-10-14) x 공격 배속(ASC AttackAnimationSpeed, 시험 치트 knight.Player.AttackSpeed).
	// (2026-10-29) 양손 감속을 선딜로 바꾼 공격은 감속을 빼고 재생한다(GetStanceRateCompensation).
	const float Rate = PlayRate * GetAttackSpeed() * GetStanceRateCompensation();
	UAnimMontage* const Montage = GetStanceAttackMontage();
	UAbilityTask_PlayMontageAndWait* const MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, Montage, Rate, StartSection);
	MontageTask->OnCompleted.AddDynamic(this, &ThisClass::OnMontageFinished);
	MontageTask->OnBlendOut.AddDynamic(this, &ThisClass::OnMontageFinished);
	MontageTask->OnInterrupted.AddDynamic(this, &ThisClass::OnMontageFinished);
	MontageTask->OnCancelled.AddDynamic(this, &ThisClass::OnMontageFinished);
	MontageTask->ReadyForActivation();
	// (2026-10-20) 기계식 타이밍: 준비 끝 락 → 모터 버스트 → 판정 끝 스톱.
	StartMechTiming(Montage, Rate);
	if (AActor* const Avatar = GetAvatarActorFromActorInfo())
	{
		if (UknightGroggyFeedbackComponent* const Feedback = Avatar->FindComponentByClass<UknightGroggyFeedbackComponent>())
		{
			Feedback->WatchAttack(this, Montage, GetStanceScaledHitParams(HitParams));
		}
	}

	// (2026-10-14) 루트 모션 공격은 애니 + 모션 워핑이 이동을 맡는다. 코드 돌진은 제자리 애니일 때만.
	if (bLunge && bLungeWithCode)
	{
		UknightCombatLibrary::StartAttackLunge(this, Montage, Rate, WarpLocation, WarpTravel, MaxLungeTime);
	}
	// (2026-10-26) 돌진 잔상(대쉬공격): 워프 구간이 끝날 때(첫 판정 직전)까지. 코드 돌진·루트 모션 워프 둘 다 이 구간에 움직인다.
	// 멈출 지점(WarpLocation)까지 1m(InRangeDistance) 안이면 기본 공격이 닿는 거리라 남기지 않는다.
	if (bLunge && bLungeGhostTrail)
	{
		AknightGhostTrailActor::StartGhostTrailForAbility(this, LungeGhostTrailSettings,
			UknightCombatLibrary::GetWarpWindowEnd(Montage) / FMath::Max(Rate, 0.01f), WarpLocation);
	}
	// (2026-10-20) 패링 반격은 휘두르기 전(워프 구간 끝)까지 대상을 따라간다.
	TrackWarpEndTime = bWarpToParriedAttacker && bLunge ? UknightCombatLibrary::GetWarpWindowEnd(Montage) : -1.f;
	if (TrackWarpEndTime > 0.f)
	{
		TrackWarpMontage = Montage;
		TrackWarpStart = GetAvatarActorFromActorInfo()->GetActorLocation();
		if (UWorld* const World = GetWorld())
		{
			World->GetTimerManager().SetTimer(TrackWarpTimer, this, &ThisClass::UpdateTrackedWarpTarget, MoveCancelCheckInterval, true);
		}
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

bool UknightGameplayAbility_MeleeAttack::SetForwardLungeWarpTarget(FVector& OutLocation, FRotator& OutRotation, float& OutTravel) const
{
	ACharacter* const Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	UMotionWarpingComponent* const MotionWarping = Character ? Character->FindComponentByClass<UMotionWarpingComponent>() : nullptr;
	UWorld* const World = GetWorld();
	if (!Character || !MotionWarping || !World)
	{
		return false;
	}
	const FVector Forward = Character->GetActorForwardVector().GetSafeNormal2D();
	const FVector Start = Character->GetActorLocation();
	const float Radius = Character->GetCapsuleComponent() ? Character->GetCapsuleComponent()->GetScaledCapsuleRadius() : 40.f;
	float Travel = ForwardLungeDistance;
	// 앞을 캡슐 반지름 구로 훑는다. 적(폰)에 닿으면 그 표면에서 WarpStopDistance 앞, 벽 같은 다른 것이면 닿기 직전까지만 간다.
	// 닿는 순간 캐릭터 중심은 대상 표면에서 Radius 앞이다.
	FHitResult Hit;
	const FCollisionQueryParams Params(SCENE_QUERY_STAT(knightForwardLunge), false, Character);
	if (World->SweepSingleByChannel(Hit, Start, Start + Forward * (ForwardLungeDistance + WarpStopDistance), FQuat::Identity, ECC_Pawn,
		FCollisionShape::MakeSphere(Radius), Params))
	{
		const float StopGap = Hit.GetActor() && Hit.GetActor()->IsA<APawn>() ? WarpStopDistance - Radius : 0.f;
		Travel = FMath::Clamp(Hit.Distance - StopGap, 0.f, ForwardLungeDistance);
	}
	OutTravel = Travel;
	OutLocation = Start + Forward * Travel;
	OutRotation = Forward.Rotation();
	MotionWarping->AddOrUpdateWarpTargetFromLocationAndRotation(WarpTargetName, OutLocation, OutRotation);
	UE_LOG(LogknightAbility, Log, TEXT("%s: forward lunge %.0fcm (max %.0f, blocked by %s)"), *GetName(), Travel, ForwardLungeDistance,
		Hit.bBlockingHit ? *GetNameSafe(Hit.GetActor()) : TEXT("nothing"));
	return true;
}

float UknightGameplayAbility_MeleeAttack::GetStanceRateCompensation() const
{
	const UknightAbilitySystemComponent* const ASC = GetKnightAbilitySystemComponentFromActorInfo();
	const float StanceScale = ASC ? ASC->GetStanceModifiers().AttackSpeedScale : 1.f;
	return bStanceSlowdownAsStartupDelay && StanceScale > KINDA_SMALL_NUMBER && StanceScale < 1.f ? 1.f / StanceScale : 1.f;
}

float UknightGameplayAbility_MeleeAttack::GetStanceStartupDelay() const
{
	const float Compensation = GetStanceRateCompensation();
	const UAnimMontage* const Montage = GetStanceAttackMontage();
	if (Compensation <= 1.f || !Montage)
	{
		return 0.f;
	}
	// 느린 배속으로 몽타주 전체를 재생할 때와 감속 없이 재생할 때의 차이. 공격 전체 시간은 예전과 같아진다.
	const float SlowRate = FMath::Max(PlayRate * GetAttackSpeed(), 0.01f);
	return Montage->GetPlayLength() / SlowRate * (1.f - 1.f / Compensation);
}

void UknightGameplayAbility_MeleeAttack::BeginStartupDelay(const float Delay)
{
	bStartupDelayDone = true;
	// 발도 몽타주가 이 멈춘 공격 몽타주에 끊겨도(OnDrawInterrupted) 공격이 끝나지 않게, 발도 뒤 공격이 두 번 시작되지 않게.
	bAttackStarted = true;
	ACharacter* const Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	UWorld* const World = GetWorld();
	if (!Character || !World)
	{
		StartAttack();
		return;
	}
	// 락온 중이면 대상 쪽으로 돌고, 그 자리에 선다(달리던 속도는 버린다). 이동 입력은 예열이 끝나 StartAttack이 시작될 때 푼다.
	FVector WarpLocation;
	FRotator WarpRotation;
	float WarpTravel = 0.f;
	if (UknightCombatLibrary::ComputeAttackWarp(Character, MaxWarpDistance, WarpStopDistance, bUseSoftTarget, SoftTargetRange,
		SoftTargetAngle, WarpLocation, WarpRotation, WarpTravel))
	{
		Character->SetActorRotation(FRotator(0.f, WarpRotation.Yaw, 0.f));
	}
	if (UCharacterMovementComponent* const Movement = Character->GetCharacterMovement())
	{
		Movement->StopMovementImmediately();
	}
	SetDrawMovementLocked(true);
	// 자세 몽타주(StartupDelayPoseMontage)가 있으면 선딜레이 동안 끝까지 재생하고, 없으면 공격 몽타주의 첫 자세로 멈춰 둔다.
	UAnimInstance* const Anim = Character->GetMesh() ? Character->GetMesh()->GetAnimInstance() : nullptr;
	// 한손 스탠스면 한손 자세 몽타주(비어 있으면 양손 것).
	UAnimMontage* const PoseMontage = OneHandedStartupDelayPoseMontage
		&& !UknightGameplayAbility_StanceSwitch::IsTwoHanded(GetAbilitySystemComponentFromActorInfo())
		? OneHandedStartupDelayPoseMontage.Get() : StartupDelayPoseMontage.Get();
	if (Anim && PoseMontage)
	{
		const float PoseRate = StartupDelayPoseRate > 0.f ? StartupDelayPoseRate
			: FMath::Clamp(PoseMontage->GetPlayLength() / FMath::Max(Delay, 0.01f), 0.5f, 3.f);
		if (StartupDelayPoseBlendIn > 0.f)
		{
			// (2026-10-29) 달리던 자세에서 버티는 자세로 천천히 들어간다(몽타주 기본 0.1초 직선 블렌드는 툭 바뀌었다).
			FAlphaBlendArgs BlendIn(StartupDelayPoseBlendIn);
			BlendIn.BlendOption = EAlphaBlendOption::HermiteCubic;
			Anim->Montage_PlayWithBlendIn(PoseMontage, BlendIn, PoseRate);
		}
		else
		{
			Anim->Montage_Play(PoseMontage, PoseRate);
		}
		StartupDelayMontage = PoseMontage;
	}
	else if (UAnimMontage* const Montage = GetStanceAttackMontage(); Anim && Montage)
	{
		Anim->Montage_Play(Montage, PlayRate * GetAttackSpeed());
		Anim->Montage_Pause(Montage);
		StartupDelayMontage = Montage;
	}
	if (StartupDelayEffect && Character->GetMesh())
	{
		for (const FName& Socket : StartupDelayEffectSockets)
		{
			if (UNiagaraComponent* const Effect = UNiagaraFunctionLibrary::SpawnSystemAttached(StartupDelayEffect, Character->GetMesh(), Socket,
				FVector::ZeroVector, FRotator::ZeroRotator, EAttachLocation::SnapToTarget, true, true))
			{
				StartupDelayEffects.Add(Effect);
			}
		}
	}
	if (UknightThrusterComponent* const Thrusters = Character->FindComponentByClass<UknightThrusterComponent>())
	{
		StopThrust(StartupDelayThrustId);
		StartupDelayThrustId = Thrusters->StartThrust(StartupDelayThrust, Character->GetActorForwardVector());
	}
	World->GetTimerManager().SetTimer(StartupDelayTimer, this, &ThisClass::OnStartupDelayFinished, Delay, false);
	UE_LOG(LogknightAbility, Log, TEXT("%s: startup delay %.2fs (stance part %.2fs)"), *GetName(), Delay, GetStanceStartupDelay());
}

void UknightGameplayAbility_MeleeAttack::OnStartupDelayFinished()
{
	StopStartupDelay();
	if (IsActive())
	{
		StartAttack();
	}
}

void UknightGameplayAbility_MeleeAttack::StopStartupDelay()
{
	if (UWorld* const World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(StartupDelayTimer);
	}
	for (const TWeakObjectPtr<UNiagaraComponent>& Effect : StartupDelayEffects)
	{
		if (UNiagaraComponent* const Component = Effect.Get())
		{
			Component->Deactivate();
		}
	}
	StartupDelayEffects.Reset();
	StopThrust(StartupDelayThrustId);
}

void UknightGameplayAbility_MeleeAttack::StopThrust(int32& ThrustId) const
{
	if (ThrustId == 0)
	{
		return;
	}
	const AActor* const Avatar = GetAvatarActorFromActorInfo();
	if (UknightThrusterComponent* const Thrusters = Avatar ? Avatar->FindComponentByClass<UknightThrusterComponent>() : nullptr)
	{
		Thrusters->StopThrust(ThrustId);
	}
	ThrustId = 0;
}

UAnimMontage* UknightGameplayAbility_MeleeAttack::GetStanceAttackMontage() const
{
	const bool bOneHanded = !UknightGameplayAbility_StanceSwitch::IsTwoHanded(GetAbilitySystemComponentFromActorInfo());
	// 무작위 목록이 있으면 이번 발동에서 한 번만 고르고(바로 앞에 나온 것은 빼고), 끝날 때까지 같은 몽타주를 준다.
	if (!bRandomMontageChosen)
	{
		bRandomMontageChosen = true;
		const TArray<TObjectPtr<UAnimMontage>>& Pool = bOneHanded && !OneHandedRandomAttackMontages.IsEmpty()
			? OneHandedRandomAttackMontages : RandomAttackMontages;
		TArray<UAnimMontage*> Choices;
		for (const TObjectPtr<UAnimMontage>& Candidate : Pool)
		{
			if (Candidate)
			{
				Choices.AddUnique(Candidate.Get());
			}
		}
		if (Choices.Num() > 1)
		{
			Choices.Remove(LastRandomMontage.Get());
		}
		if (!Choices.IsEmpty())
		{
			ChosenRandomMontage = Choices[FMath::RandRange(0, Choices.Num() - 1)];
			LastRandomMontage = ChosenRandomMontage;
			UE_LOG(LogknightAbility, Log, TEXT("%s: random attack montage %s (%d choices, %s)"), *GetName(), *GetNameSafe(ChosenRandomMontage.Get()),
				Choices.Num(), bOneHanded ? TEXT("one-handed") : TEXT("two-handed"));
		}
	}
	if (UAnimMontage* const Chosen = ChosenRandomMontage.Get())
	{
		return Chosen;
	}
	// (2026-10-15) 한손 스탠스면 한손 몽타주. 비어 있으면 양손 몽타주를 그대로 쓴다.
	if (OneHandedAttackMontage && bOneHanded)
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
	if (AActor* const Avatar = GetAvatarActorFromActorInfo())
	{
		if (UknightGroggyFeedbackComponent* const Feedback = Avatar->FindComponentByClass<UknightGroggyFeedbackComponent>())
		{
			Feedback->EndPredictionForAttack(this);
		}
	}
	if (UWorld* const World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(MoveCancelTimer);
		World->GetTimerManager().ClearTimer(DrawTimer);
		World->GetTimerManager().ClearTimer(TrackWarpTimer);
	}
	// (2026-10-22) 뽑는 도중 끝나도(피격·사망·다른 어빌리티로 교체) 이동 잠금은 반드시 푼다.
	SetDrawMovementLocked(false);
	// (2026-10-29) 예열(락온 선딜레이) 도중 끝나면 이펙트를 끄고, 멈춰 둔 몽타주를 내린다(공격 몽타주가 시작됐으면 비어 있다).
	StopStartupDelay();
	// 공격 시작 부스터는 보통 정한 시간에 스스로 꺼진다. 공격이 먼저 끝나면(교체·피격 등) 여기서 끈다.
	StopThrust(AttackStartThrustId);
	if (UAnimMontage* const PausedMontage = StartupDelayMontage.Get())
	{
		const ACharacter* const Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
		if (UAnimInstance* const Anim = Character && Character->GetMesh() ? Character->GetMesh()->GetAnimInstance() : nullptr)
		{
			Anim->Montage_Stop(0.2f, PausedMontage);
		}
	}
	StartupDelayMontage.Reset();
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
	const EknightHitOutcome Outcome = UknightCombatLibrary::ApplyMeleeHit(GetAvatarActorFromActorInfo(), *Hit, GetStanceScaledHitParams(HitParams), this);
	// 실제로 맞혔을 때만 추가 타격 큐(HitCueTag)를 맞은 곳에서 한 번 실행한다.
	if (Outcome == EknightHitOutcome::Hit && HitCueTag.IsValid() && !bHitCuePlayed)
	{
		if (UAbilitySystemComponent* const ASC = GetAbilitySystemComponentFromActorInfo())
		{
			bHitCuePlayed = true;
			FGameplayCueParameters CueParams;
			CueParams.Location = Hit->ImpactPoint;
			CueParams.Normal = Hit->ImpactNormal;
			CueParams.Instigator = GetAvatarActorFromActorInfo();
			CueParams.EffectCauser = GetAvatarActorFromActorInfo();
			CueParams.PhysicalMaterial = Hit->PhysMaterial;
			ASC->ExecuteGameplayCue(HitCueTag, CueParams);
		}
	}
	OnHitResolved(*Hit, Outcome);
}
