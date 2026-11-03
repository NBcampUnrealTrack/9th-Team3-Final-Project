#include "Monster/knightMonsterAirTurnComponent.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystem/Abilities/knightGameplayAbility_MonsterAttack.h"
#include "Components/AudioComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/RootMotionSource.h"
#include "Monster/knightMonsterMovementComponent.h"
#include "Monster/knightMonsterCharacterBase.h"
#include "Monster/knightMonsterHoverComponent.h"
#include "Monster/knightMonsterTargetingComponent.h"
#include "System/knightGameplayTags.h"
#include "knight.h"

UknightMonsterAirTurnComponent::UknightMonsterAirTurnComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UknightMonsterAirTurnComponent::BeginPlay()
{
	Super::BeginPlay();

	const AknightMonsterCharacterBase* const Monster = Cast<AknightMonsterCharacterBase>(GetOwner());
	ASC = Monster ? Monster->GetAbilitySystemComponent() : nullptr;
	Hover = Monster ? Monster->FindComponentByClass<UknightMonsterHoverComponent>() : nullptr;
	if (GroundTurnTime <= 0.f || !ASC.IsValid() || !Hover.IsValid())
	{
		return;
	}
	AbilityEndedHandle = ASC->OnAbilityEnded.AddUObject(this, &ThisClass::HandleAbilityEnded);
	KnockdownHandle = ASC->RegisterGameplayTagEvent(knightGameplayTags::State_HitReact_Knockdown, EGameplayTagEventType::NewOrRemoved)
		.AddUObject(this, &ThisClass::HandleKnockdownChanged);
	SetComponentTickEnabled(true);
}

void UknightMonsterAirTurnComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (ASC.IsValid())
	{
		ASC->OnAbilityEnded.Remove(AbilityEndedHandle);
		ASC->RegisterGameplayTagEvent(knightGameplayTags::State_HitReact_Knockdown, EGameplayTagEventType::NewOrRemoved).Remove(KnockdownHandle);
	}
	Super::EndPlay(EndPlayReason);
}

void UknightMonsterAirTurnComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const AknightMonsterCharacterBase* const Monster = Cast<AknightMonsterCharacterBase>(GetOwner());
	if (!Monster || !ASC.IsValid())
	{
		return;
	}
	if (Monster->IsDead())
	{
		if (bAirborne)
		{
			EndAirTurn(FallBlendTime, false);
		}
		UpdateHoverSound(false, DeltaTime);
		SetComponentTickEnabled(false);
		return;
	}

	const bool bReacting = ASC->HasMatchingGameplayTag(knightGameplayTags::State_Monster_Reacting);
	// 이륙·착지 애니메이션과 공중 공격은 자기 날갯짓 소리가 있다.
	UpdateHoverSound(bAirborne && !bReacting && !IsAttacking(), DeltaTime);
	if (!bAirborne)
	{
		const UknightMonsterTargetingComponent* const Targeting = Monster->GetTargetingComponent();
		if (Targeting && Targeting->GetCurrentTarget() && !bReacting && !ASC->HasMatchingGameplayTag(knightGameplayTags::State_Monster_Exhausted))
		{
			GroundTime += DeltaTime;
		}
		const int32 GroundAttacks = GetGroundAttacksPerTurn();
		const bool bAttacksDone = GroundAttacks > 0 && GroundAttacksUsed >= GroundAttacks;
		if ((bAttacksDone || GroundTime >= GetGroundTurnTime()) && CanChangeTurn())
		{
			TakeOff();
		}
		return;
	}

	AirTime += DeltaTime;
	UpdateLastAttackTag();
	// 분노 마무리 공격(물 회오리)을 기다리는 동안은 내리지 않는다. 그 공격이 공중 턴을 끝낸다.
	if (CanChangeTurn() && !ASC->HasMatchingGameplayTag(knightGameplayTags::State_Monster_EnrageFinale))
	{
		const float MaxTime = GetAirTurnMaxTime();
		const bool bWaitFinisher = AirTime < MaxTime + FinisherWaitTime && HasAirAttackSoon(EAirAttackFilter::FinishersOnly, 0.f);
		const bool bTurnDone = AirAttacksUsed >= GetAirAttacksPerTurn() || (AirTime >= MaxTime && !bWaitFinisher);
		// 쓸 공중 공격이 모두 쿨다운이면 빈 비행으로 최대 시간을 채우지 않고 내려온다.
		if (bTurnDone || (EarlyLandWaitTime >= 0.f && !HasAirAttackSoon(EAirAttackFilter::All, EarlyLandWaitTime)))
		{
			if (!bTurnDone)
			{
				UE_LOG(LogknightAI, Log, TEXT("%s: no air attack ready within %.1f s, landing early."), *GetNameSafe(GetOwner()), EarlyLandWaitTime);
			}
			Land();
			return;
		}
	}
	UpdateRetreat(DeltaTime);
}

void UknightMonsterAirTurnComponent::UpdateRetreat(float DeltaTime)
{
	AknightMonsterCharacterBase* const Monster = Cast<AknightMonsterCharacterBase>(GetOwner());
	const UknightMonsterTargetingComponent* const Targeting = Monster ? Monster->GetTargetingComponent() : nullptr;
	const AActor* const Target = Targeting ? Targeting->GetCurrentTarget() : nullptr;
	if (AirKeepDistance <= 0.f || !Target || !CanChangeTurn())
	{
		return;
	}
	const double Now = GetWorld()->GetTimeSeconds();
	if (Now < RetreatEndTime)
	{
		// 물러나는 동안 타깃을 본다.
		if (UknightMonsterMovementComponent* const Movement = Monster->GetMonsterMovementComponent())
		{
			Movement->TurnTowards(Target->GetActorLocation(), DeltaTime, 180.f, 5.f);
		}
		return;
	}
	const FVector Away = Monster->GetActorLocation() - Target->GetActorLocation();
	const float Gap = AirKeepDistance - static_cast<float>(Away.Size2D());
	UCharacterMovementComponent* const Movement = Monster->GetCharacterMovement();
	if (Gap < 100.f || !Movement)
	{
		return;
	}
	const float Duration = FMath::Clamp(Gap / RetreatSpeed, 0.3f, 2.f);
	const TSharedPtr<FRootMotionSource_ConstantForce> Force = MakeShared<FRootMotionSource_ConstantForce>();
	Force->InstanceName = TEXT("AirRetreat");
	Force->AccumulateMode = ERootMotionAccumulateMode::Override;
	Force->Priority = 5;
	Force->Force = Away.GetSafeNormal2D(UE_SMALL_NUMBER, -Monster->GetActorForwardVector()) * (Gap / Duration);
	Force->Duration = Duration;
	Force->FinishVelocityParams.Mode = ERootMotionFinishVelocityMode::SetVelocity;
	Force->FinishVelocityParams.SetVelocity = FVector::ZeroVector;
	Movement->ApplyRootMotionSource(Force);
	RetreatEndTime = Now + Duration;
}

void UknightMonsterAirTurnComponent::UpdateHoverSound(bool bHovering, float DeltaTime)
{
	HoverAudios.RemoveAll([](const UAudioComponent* Audio) { return !IsValid(Audio) || !Audio->IsPlaying(); });
	if (!bHovering)
	{
		for (UAudioComponent* const Audio : HoverAudios)
		{
			Audio->FadeOut(HoverSoundFadeOut, 0.f);
		}
		HoverAudios.Reset();
		// 다시 떠 있기 시작하면 바로 소리를 낸다.
		HoverSoundTime = 0.f;
		return;
	}
	HoverSoundTime -= DeltaTime;
	const ACharacter* const Character = Cast<ACharacter>(GetOwner());
	if (HoverSoundTime > 0.f || !HoverSound || !Character)
	{
		return;
	}
	HoverSoundTime = HoverSoundInterval;
	if (UAudioComponent* const Audio = UGameplayStatics::SpawnSoundAttached(HoverSound, Character->GetMesh(), HoverSoundBone))
	{
		HoverAudios.Add(Audio);
	}
}

void UknightMonsterAirTurnComponent::TakeOff()
{
	if (bAirborne || !ASC.IsValid() || !Hover.IsValid())
	{
		return;
	}
	bAirborne = true;
	AirTime = 0.f;
	AirAttacksUsed = 0;
	ASC->AddLooseGameplayTag(knightGameplayTags::State_Monster_Airborne);
	UpdateLastAttackTag();
	Hover->SetAirborne(true, TakeOffBlendTime);
	SetPawnBlocking(false);
	SendEvent(knightGameplayTags::Event_Monster_TakeOff);
	UE_LOG(LogknightAI, Log, TEXT("%s: take off after %.1f s on the ground, %d ground attack(s)."), *GetNameSafe(GetOwner()), GroundTime, GroundAttacksUsed);
}

void UknightMonsterAirTurnComponent::Land()
{
	if (bAirborne)
	{
		UE_LOG(LogknightAI, Log, TEXT("%s: land after %.1f s in the air, %d air attack(s)."), *GetNameSafe(GetOwner()), AirTime, AirAttacksUsed);
		// 마지막 공중 공격이 땅에서 끝났으면(다이브) 착지 애니메이션 없이 그대로 지상 턴을 시작한다.
		const bool bAlreadyLow = Hover.IsValid() && Hover->GetAirAlpha() < 0.5f;
		EndAirTurn(bAlreadyLow ? 0.3f : LandBlendTime, !bAlreadyLow);
	}
}

void UknightMonsterAirTurnComponent::EndAirTurn(float BlendTime, bool bSendLandEvent)
{
	bAirborne = false;
	GroundTime = 0.f;
	GroundAttacksUsed = 0;
	if (ASC.IsValid())
	{
		ASC->RemoveLooseGameplayTag(knightGameplayTags::State_Monster_Airborne);
	}
	UpdateLastAttackTag();
	if (Hover.IsValid())
	{
		Hover->SetAirborne(false, BlendTime);
	}
	SetPawnBlocking(true);
	if (bSendLandEvent)
	{
		SendEvent(knightGameplayTags::Event_Monster_Land);
	}
}

void UknightMonsterAirTurnComponent::HandleAbilityEnded(const FAbilityEndedData& Data)
{
	if (const UknightGameplayAbility_MonsterAttack* const Attack = Cast<UknightGameplayAbility_MonsterAttack>(Data.AbilityThatEnded))
	{
		LastAttackEndTime = GetWorld()->GetTimeSeconds();
		// 공격 사이의 탐색 비행 같은 이동 행동은 공중 공격 횟수에 넣지 않는다.
		if (Attack->CountsAsAttack())
		{
			if (bAirborne)
			{
				++AirAttacksUsed;
				UpdateLastAttackTag();
			}
			// 공중 공격이 땅에서 끝나도(글라이드·회오리 착지) 지상 공격으로 세지 않는다. 그 공격이 지상 턴을 새로 시작한다.
			else if (!Attack->GetSelection().RequiredOwnerTags.HasTagExact(knightGameplayTags::State_Monster_Airborne))
			{
				++GroundAttacksUsed;
			}
		}
	}
}

void UknightMonsterAirTurnComponent::HandleKnockdownChanged(const FGameplayTag Tag, int32 NewCount)
{
	// 격추: 그로기 게이지가 다 차서 다운되면 공중에서 바로 떨어진다. 다운 애니메이션은 바닥 높이에서 재생된다.
	if (NewCount > 0 && bAirborne)
	{
		UE_LOG(LogknightAI, Log, TEXT("%s: shot down after %.1f s in the air."), *GetNameSafe(GetOwner()), AirTime);
		EndAirTurn(FallBlendTime, false);
	}
}

void UknightMonsterAirTurnComponent::SetPawnBlocking(bool bBlock)
{
	const ACharacter* const Character = Cast<ACharacter>(GetOwner());
	UCapsuleComponent* const Capsule = Character ? Character->GetCapsuleComponent() : nullptr;
	if (!Capsule)
	{
		return;
	}
	if (!bBlock)
	{
		SavedPawnResponse = Capsule->GetCollisionResponseToChannel(ECC_Pawn);
		Capsule->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	}
	else
	{
		Capsule->SetCollisionResponseToChannel(ECC_Pawn, SavedPawnResponse);
	}
}

void UknightMonsterAirTurnComponent::UpdateLastAttackTag()
{
	if (!ASC.IsValid())
	{
		return;
	}
	// 마무리 공격은 턴의 마지막 공격으로 연다: 공격 수를 거의 채웠거나, 시간이 얼마 안 남았거나, 다른 공중 공격이 없을 때.
	// 한 번 열면 턴이 끝날 때까지 둔다(다른 공격의 쿨다운이 끝나도 다시 닫지 않는다).
	bool bOpen = false;
	if (bAirborne)
	{
		bOpen = bLastAirAttackOpen || AirAttacksUsed >= GetAirAttacksPerTurn() - 1
			|| GetAirTurnMaxTime() - AirTime <= FinisherOpenTime
			|| !HasAirAttackSoon(EAirAttackFilter::NoFinishers, FMath::Max(EarlyLandWaitTime, 0.f));
	}
	if (bOpen && !bLastAirAttackOpen)
	{
		UE_LOG(LogknightAI, Log, TEXT("%s: finisher air attacks open (%.1f s in the air, %d air attack(s))."), *GetNameSafe(GetOwner()), AirTime, AirAttacksUsed);
	}
	bLastAirAttackOpen = bOpen;
	ASC->SetLooseGameplayTagCount(knightGameplayTags::State_Monster_LastAirAttack, bOpen ? 1 : 0);
}

bool UknightMonsterAirTurnComponent::IsAttacking() const
{
	for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
	{
		if (Spec.IsActive() && Cast<UknightGameplayAbility_MonsterAttack>(Spec.GetPrimaryInstance()))
		{
			return true;
		}
	}
	return false;
}

bool UknightMonsterAirTurnComponent::IsEnraged() const
{
	return ASC.IsValid() && ASC->HasMatchingGameplayTag(knightGameplayTags::State_Monster_Enraged);
}

float UknightMonsterAirTurnComponent::GetGroundTurnTime() const
{
	return IsEnraged() && EnragedGroundTurnTime > 0.f ? EnragedGroundTurnTime : GroundTurnTime;
}

int32 UknightMonsterAirTurnComponent::GetGroundAttacksPerTurn() const
{
	return IsEnraged() && EnragedGroundAttacksPerTurn > 0 ? EnragedGroundAttacksPerTurn : GroundAttacksPerTurn;
}

int32 UknightMonsterAirTurnComponent::GetAirAttacksPerTurn() const
{
	return IsEnraged() && EnragedAirAttacksPerTurn > 0 ? EnragedAirAttacksPerTurn : AirAttacksPerTurn;
}

float UknightMonsterAirTurnComponent::GetAirTurnMaxTime() const
{
	return IsEnraged() && EnragedAirTurnMaxTime > 0.f ? EnragedAirTurnMaxTime : AirTurnMaxTime;
}

bool UknightMonsterAirTurnComponent::CanChangeTurn() const
{
	// 공격과 반응(포효·다운)이 끝난 뒤에 바꾼다. 공격 도중에 뜨거나 내리면 판정 위치가 어긋난다.
	// 공격이 막 끝났으면 조금 기다린다. 그 사이에 연계 공격(FollowUpAttacks)이 이어질 수 있다.
	return !IsAttacking() && !ASC->HasMatchingGameplayTag(knightGameplayTags::State_Monster_Reacting)
		&& GetWorld()->GetTimeSeconds() - LastAttackEndTime >= FollowUpGraceTime;
}

bool UknightMonsterAirTurnComponent::HasAirAttackSoon(EAirAttackFilter Filter, float Within) const
{
	const double Now = GetWorld()->GetTimeSeconds();
	for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
	{
		const UknightGameplayAbility_MonsterAttack* const Attack = Cast<UknightGameplayAbility_MonsterAttack>(Spec.GetPrimaryInstance());
		if (!Attack || !Attack->CountsAsAttack())
		{
			continue;
		}
		const FknightMonsterAttackSelection& Selection = Attack->GetSelection();
		const bool bFinisher = Selection.RequiredOwnerTags.HasTagExact(knightGameplayTags::State_Monster_LastAirAttack);
		if ((Filter == EAirAttackFilter::NoFinishers && bFinisher) || (Filter == EAirAttackFilter::FinishersOnly && !bFinisher))
		{
			continue;
		}
		// 지상 공격은 Airborne 차단 태그로, 마무리 공격은 마무리가 열리기 전까지 LastAirAttack 요구 태그로 걸러진다.
		if (Selection.Weight <= 0.f || Selection.bOnlyAsFollowUp || !ASC->HasAllMatchingGameplayTags(Selection.RequiredOwnerTags)
			|| ASC->HasAnyMatchingGameplayTags(Selection.BlockedOwnerTags))
		{
			continue;
		}
		if (Attack->GetCooldownRemaining(Now) <= Within)
		{
			return true;
		}
	}
	return false;
}

void UknightMonsterAirTurnComponent::SendEvent(const FGameplayTag& EventTag) const
{
	FGameplayEventData Payload;
	Payload.EventTag = EventTag;
	Payload.Instigator = GetOwner();
	Payload.Target = GetOwner();
	ASC->HandleGameplayEvent(EventTag, &Payload);
}
