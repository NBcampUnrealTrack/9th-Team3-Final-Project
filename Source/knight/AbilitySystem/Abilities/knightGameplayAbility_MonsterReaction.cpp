#include "AbilitySystem/Abilities/knightGameplayAbility_MonsterReaction.h"

#include "AIController.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "AbilitySystem/Attributes/knightCoreAttributeSet.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Monster/knightMonsterCharacterBase.h"
#include "Monster/knightMonsterDefinition.h"
#include "System/knightGameplayTags.h"
#include "knight.h"
#include "knightPerfCounters.h"

UknightGameplayAbility_MonsterReaction::UknightGameplayAbility_MonsterReaction(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ActivationGroup = EknightAbilityActivationGroup::Independent;
	// 다운 중에 Poise가 다시 깨져도 다운을 처음부터 다시 시작하지 않는다.
	bRetriggerInstancedAbility = false;

	SetAssetTags(FGameplayTagContainer(knightGameplayTags::Ability_HitReact));
	ActivationOwnedTags.AddTag(knightGameplayTags::State_Monster_Reacting);
	ActivationBlockedTags.AddTag(knightGameplayTags::State_Dead);
	CancelAbilitiesWithTag.AddTag(knightGameplayTags::Ability_Attack);
	BlockAbilitiesWithTag.AddTag(knightGameplayTags::Ability_Attack);

	StateTag = knightGameplayTags::State_HitReact_Knockdown;

	// 기본값은 Poise가 깨질 때의 다운이다. 다른 반응은 Blueprint 하위 클래스에서 Triggers를 바꾼다.
	FAbilityTriggerData Trigger;
	Trigger.TriggerTag = knightGameplayTags::Event_PoiseBroken;
	Trigger.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;
	AbilityTriggers.Add(Trigger);
}

void UknightGameplayAbility_MonsterReaction::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	UAbilitySystemComponent* const ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	ACharacter* const Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	if (!ASC || !Character || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	KNIGHT_PERF_COUNT("Monster.Reactions", 1);

	ActiveHoldMontage = HoldMontage;
	const AActor* const Instigator = TriggerEventData ? TriggerEventData->Instigator.Get() : nullptr;
	if (Instigator && Instigator != Character)
	{
		const EknightHitSide Side = AknightMonsterCharacterBase::GetHitSide(*Character, Instigator->GetActorLocation());
		UAnimMontage* const SideMontage = Side == EknightHitSide::Back ? HoldMontageBack.Get()
			: Side == EknightHitSide::Left ? HoldMontageLeft.Get()
			: Side == EknightHitSide::Right ? HoldMontageRight.Get() : nullptr;
		if (SideMontage)
		{
			ActiveHoldMontage = SideMontage;
		}
	}

	// 다른 반응(다운 등) 중에 켜졌으면 그 몽타주를 덮지 않고 물러났다가, 그 반응이 끝난 뒤 EndAbility의 다시 실행 경로로 켜진다.
	// Reacting은 자기 ActivationOwnedTags로도 하나 붙어 있으므로 1보다 많아야 다른 반응이 있는 것이다.
	if (bReplayIfInterrupted && ASC->GetTagCount(knightGameplayTags::State_Monster_Reacting) > 1)
	{
		UE_LOG(LogknightAI, Log, TEXT("%s: %s waits for the current reaction."), *GetNameSafe(Character), *GetName());
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (bCancelOtherReactions)
	{
		// 취소된 반응의 몽타주는 자기 블렌드 아웃으로 빠지고, 아래 유지 몽타주가 블렌드 인으로 이어받는다.
		const FGameplayTagContainer ReactionTags(knightGameplayTags::Ability_HitReact);
		ASC->CancelAbilities(&ReactionTags, nullptr, this);
	}

	if (StateTag.IsValid())
	{
		ASC->AddLooseGameplayTag(StateTag);
		bStateTagApplied = true;
	}

	// 현재 이동 경로를 멈춘다. 새 경로가 시작되지 않게 막는 것은 StateTree의 반응 상태가 맡는다.
	if (AAIController* const Controller = Cast<AAIController>(Character->GetController()))
	{
		Controller->StopMovement();
	}
	Character->GetCharacterMovement()->StopMovementImmediately();

	bHoldStarted = false;
	if (RewindTime > 0.f)
	{
		// 한 프레임 뒤에 되감는다. 패링으로 Poise가 깨지면 다운이 같은 프레임에 켜져 이 반응을 취소하는데, 그 전에 되감기가
		// 블렌드 없이 시작되면 공격 몽타주가 즉시 사라져 다운이 대기 자세에서 블렌드를 시작한다(자세가 튐).
		UAbilityTask_WaitDelay* const RewindDelay = UAbilityTask_WaitDelay::WaitDelay(this, 0.f);
		RewindDelay->OnFinish.AddDynamic(this, &ThisClass::HandleRewindDelayFinished);
		RewindDelay->ReadyForActivation();
	}
	else
	{
		StartHold();
	}

	UE_LOG(LogknightAI, Log, TEXT("%s: %s started (%s)."), *GetNameSafe(Character), *GetName(),
		TriggerEventData ? *TriggerEventData->EventTag.ToString() : TEXT("tag trigger"));
}

bool UknightGameplayAbility_MonsterReaction::TryStartRewind(ACharacter& Character)
{
	UAnimInstance* const AnimInstance = Character.GetMesh() ? Character.GetMesh()->GetAnimInstance() : nullptr;
	if (!AnimInstance)
	{
		return false;
	}
	// 공격 어빌리티는 이 어빌리티가 켜지면서 취소되어(CancelAbilitiesWithTag) 그 몽타주가 막 블렌드 아웃을 시작했다.
	// 아직 목록에 남아 있는 가장 최근 몽타주가 끊긴 공격이다.
	const FAnimMontageInstance* Interrupted = nullptr;
	for (const FAnimMontageInstance* const Instance : AnimInstance->MontageInstances)
	{
		if (Instance && Instance->Montage && Instance->Montage != ActiveHoldMontage && Instance->Montage != RecoverMontage)
		{
			Interrupted = Instance;
		}
	}
	if (!Interrupted)
	{
		return false;
	}
	const UAnimMontage* const Montage = Interrupted->Montage;
	const float Position = Interrupted->GetPosition();
	for (const FSlotAnimationTrack& Track : Montage->SlotAnimTracks)
	{
		const FAnimSegment* const Segment = Track.AnimTrack.GetSegmentAtTime(Position);
		float AnimPosition = 0.f;
		UAnimSequenceBase* const Sequence = Segment ? Segment->GetAnimationData(Position, AnimPosition) : nullptr;
		if (!Sequence)
		{
			continue;
		}
		const float Start = FMath::Max(0.f, AnimPosition - RewindTime);
		if (AnimPosition - Start <= UE_KINDA_SMALL_NUMBER)
		{
			return false;
		}
		// 같은 자세에서 이어지므로 블렌드 인 없이 시작하고, 끝(되감긴 자세)에 도착하면 유지 몽타주의 블렌드 인과 같은 시간으로 넘긴다.
		// 블렌드 아웃 시점을 0으로 둬야 짧은 되감기 도중에 블렌드 아웃이 먼저 시작되지 않는다.
		const float BlendOutTime = ActiveHoldMontage ? ActiveHoldMontage->BlendIn.GetBlendTime() : 0.25f;
		UAnimMontage* const Rewind = UAnimMontage::CreateSlotAnimationAsDynamicMontage(Sequence, Track.SlotName, 0.f, BlendOutTime, 1.f, 1, 0.f);
		if (!Rewind || Rewind->SlotAnimTracks.IsEmpty() || Rewind->SlotAnimTracks[0].AnimTrack.AnimSegments.IsEmpty())
		{
			return false;
		}
		FAnimSegment& RewindSegment = Rewind->SlotAnimTracks[0].AnimTrack.AnimSegments[0];
		RewindSegment.AnimStartTime = Start;
		RewindSegment.AnimEndTime = AnimPosition;
		const float Length = RewindSegment.GetLength();
		Rewind->SetCompositeLength(Length);

		// 끝(지금 자세)에서 시작해 거꾸로 재생한다.
		UAbilityTask_PlayMontageAndWait* const RewindTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
			this, TEXT("Rewind"), Rewind, -RewindRate, NAME_None, true, 1.f, Length);
		RewindTask->OnCompleted.AddDynamic(this, &ThisClass::HandleRewindFinished);
		RewindTask->OnBlendOut.AddDynamic(this, &ThisClass::HandleRewindFinished);
		RewindTask->OnInterrupted.AddDynamic(this, &ThisClass::HandleRewindFinished);
		RewindTask->OnCancelled.AddDynamic(this, &ThisClass::HandleRewindFinished);
		RewindTask->ReadyForActivation();
		UE_LOG(LogknightAI, Verbose, TEXT("%s: rewinding %s %.2f -> %.2f s."), *GetNameSafe(&Character), *GetNameSafe(Sequence), AnimPosition, Start);
		return true;
	}
	return false;
}

void UknightGameplayAbility_MonsterReaction::HandleRewindDelayFinished()
{
	ACharacter* const Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	if (IsActive() && !(Character && TryStartRewind(*Character)))
	{
		StartHold();
	}
}

void UknightGameplayAbility_MonsterReaction::HandleRewindFinished()
{
	if (IsActive())
	{
		StartHold();
	}
}

void UknightGameplayAbility_MonsterReaction::StartHold()
{
	if (bHoldStarted)
	{
		return;
	}
	bHoldStarted = true;
	if (ActiveHoldMontage)
	{
		UAbilityTask_PlayMontageAndWait* const HoldTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, TEXT("Hold"), ActiveHoldMontage);
		HoldTask->ReadyForActivation();
	}
	UAbilityTask_WaitDelay* const HoldDelay = UAbilityTask_WaitDelay::WaitDelay(this, HoldTime);
	HoldDelay->OnFinish.AddDynamic(this, &ThisClass::HandleHoldFinished);
	HoldDelay->ReadyForActivation();
}

void UknightGameplayAbility_MonsterReaction::HandleHoldFinished()
{
	UAbilitySystemComponent* const ASC = GetAbilitySystemComponentFromActorInfo();
	if (ASC && !RecoverTags.IsEmpty() && !bRecoverTagsApplied)
	{
		ASC->AddLooseGameplayTags(RecoverTags);
		bRecoverTagsApplied = true;
	}
	if (RecoverMontage)
	{
		UAbilityTask_PlayMontageAndWait* const RecoverTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, TEXT("Recover"), RecoverMontage);
		RecoverTask->OnCompleted.AddDynamic(this, &ThisClass::HandleRecoverFinished);
		RecoverTask->OnBlendOut.AddDynamic(this, &ThisClass::HandleRecoverFinished);
		RecoverTask->OnInterrupted.AddDynamic(this, &ThisClass::HandleRecoverFinished);
		RecoverTask->OnCancelled.AddDynamic(this, &ThisClass::HandleRecoverFinished);
		RecoverTask->ReadyForActivation();
		return;
	}
	UAbilityTask_WaitDelay* const RecoverDelay = UAbilityTask_WaitDelay::WaitDelay(this, RecoverTime);
	RecoverDelay->OnFinish.AddDynamic(this, &ThisClass::HandleRecoverFinished);
	RecoverDelay->ReadyForActivation();
}

void UknightGameplayAbility_MonsterReaction::HandleRecoverFinished()
{
	EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, false);
}

void UknightGameplayAbility_MonsterReaction::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	// 몽타주 블렌드 아웃과 완료가 둘 다 호출될 수 있으므로 처음 종료될 때만 Poise를 회복한다.
	if (!IsEndAbilityValid(Handle, ActorInfo))
	{
		return;
	}
	UAbilitySystemComponent* const ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	if (ASC && bStateTagApplied)
	{
		ASC->RemoveLooseGameplayTag(StateTag);
	}
	bStateTagApplied = false;
	if (ASC && bRecoverTagsApplied)
	{
		ASC->RemoveLooseGameplayTags(RecoverTags);
	}
	bRecoverTagsApplied = false;

	// 사망하면 반응이 취소되며, 시체는 깨진 Poise를 그대로 둔다.
	if (!bWasCancelled && bRestorePoise)
	{
		RestorePoise();
	}
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);

	// 자기 Reacting 태그가 빠진 뒤에도 남아 있으면 다른 반응이 이어받은 것이다. 그 밖의 취소(사망 등)는 다시 실행하지 않는다.
	if (bWasCancelled && bReplayIfInterrupted && ASC && !ReplayDelegateHandle.IsValid()
		&& !ASC->HasMatchingGameplayTag(knightGameplayTags::State_Dead)
		&& ASC->HasMatchingGameplayTag(knightGameplayTags::State_Monster_Reacting))
	{
		ReplayASC = ASC;
		ReplayHandle = Handle;
		ReplayDelegateHandle = ASC->RegisterGameplayTagEvent(knightGameplayTags::State_Monster_Reacting, EGameplayTagEventType::NewOrRemoved)
			.AddUObject(this, &ThisClass::HandleReactingChangedForReplay);
		UE_LOG(LogknightAI, Log, TEXT("%s: %s interrupted, replays after the current reaction."), *GetNameSafe(ASC->GetAvatarActor()), *GetName());
	}
}

void UknightGameplayAbility_MonsterReaction::HandleReactingChangedForReplay(const FGameplayTag Tag, int32 NewCount)
{
	UAbilitySystemComponent* const ASC = ReplayASC.Get();
	if (NewCount > 0 || !ASC)
	{
		return;
	}
	ASC->RegisterGameplayTagEvent(knightGameplayTags::State_Monster_Reacting, EGameplayTagEventType::NewOrRemoved).Remove(ReplayDelegateHandle);
	ReplayDelegateHandle.Reset();
	if (!ASC->HasMatchingGameplayTag(knightGameplayTags::State_Dead))
	{
		ASC->TryActivateAbility(ReplayHandle);
	}
}

void UknightGameplayAbility_MonsterReaction::RestorePoise() const
{
	UAbilitySystemComponent* const ASC = GetAbilitySystemComponentFromActorInfo();
	if (!ASC || !ASC->GetSet<UknightCoreAttributeSet>())
	{
		return;
	}
	// 기본값(Base)을 바꾸므로 변경이 영구적이다. Poise가 0보다 커지면 Poise 붕괴 이벤트도 다시 발생할 수 있게 된다.
	const float CurrentMaxPoise = ASC->GetNumericAttributeBase(UknightCoreAttributeSet::GetMaxPoiseAttribute());
	if (InitialMaxPoise < 0.f)
	{
		InitialMaxPoise = CurrentMaxPoise;
	}
	float NewMaxPoise = CurrentMaxPoise * (1.f + MaxPoiseGrowth);
	// 처음 MaxPoise의 MaxPoiseGrowthLimit배에서 멈춘다(이미 넘었으면 줄이지 않는다).
	if (MaxPoiseGrowthLimit > 0.f)
	{
		NewMaxPoise = FMath::Max(CurrentMaxPoise, FMath::Min(NewMaxPoise, InitialMaxPoise * MaxPoiseGrowthLimit));
	}
	ASC->SetNumericAttributeBase(UknightCoreAttributeSet::GetMaxPoiseAttribute(), NewMaxPoise);
	ASC->SetNumericAttributeBase(UknightCoreAttributeSet::GetPoiseAttribute(), NewMaxPoise);
}
