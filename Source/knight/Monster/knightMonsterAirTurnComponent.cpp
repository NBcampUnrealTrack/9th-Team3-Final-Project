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
		if (GroundTime >= GetGroundTurnTime() && CanChangeTurn())
		{
			TakeOff();
		}
		return;
	}

	AirTime += DeltaTime;
	// 분노 마무리 공격(물 회오리)을 기다리는 동안은 내리지 않는다. 그 공격이 공중 턴을 끝낸다.
	if ((AirAttacksUsed >= GetAirAttacksPerTurn() || AirTime >= GetAirTurnMaxTime()) && CanChangeTurn()
		&& !ASC->HasMatchingGameplayTag(knightGameplayTags::State_Monster_EnrageFinale))
	{
		Land();
		return;
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
	UE_LOG(LogknightAI, Log, TEXT("%s: take off after %.1f s on the ground."), *GetNameSafe(GetOwner()), GroundTime);
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
		if (bAirborne && Attack->CountsAsAttack())
		{
			++AirAttacksUsed;
			UpdateLastAttackTag();
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
	if (ASC.IsValid())
	{
		const bool bLast = bAirborne && AirAttacksUsed == GetAirAttacksPerTurn() - 1;
		ASC->SetLooseGameplayTagCount(knightGameplayTags::State_Monster_LastAirAttack, bLast ? 1 : 0);
	}
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

void UknightMonsterAirTurnComponent::SendEvent(const FGameplayTag& EventTag) const
{
	FGameplayEventData Payload;
	Payload.EventTag = EventTag;
	Payload.Instigator = GetOwner();
	Payload.Target = GetOwner();
	ASC->HandleGameplayEvent(EventTag, &Payload);
}
