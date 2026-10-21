#include "Monster/knightMonsterAirTurnComponent.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystem/Abilities/knightGameplayAbility_MonsterAttack.h"
#include "Components/CapsuleComponent.h"
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
		SetComponentTickEnabled(false);
		return;
	}

	const bool bReacting = ASC->HasMatchingGameplayTag(knightGameplayTags::State_Monster_Reacting);
	if (!bAirborne)
	{
		const UknightMonsterTargetingComponent* const Targeting = Monster->GetTargetingComponent();
		if (Targeting && Targeting->GetCurrentTarget() && !bReacting && !ASC->HasMatchingGameplayTag(knightGameplayTags::State_Monster_Exhausted))
		{
			GroundTime += DeltaTime;
		}
		if (GroundTime >= GroundTurnTime && CanChangeTurn())
		{
			TakeOff();
		}
		return;
	}

	AirTime += DeltaTime;
	if ((AirAttacksUsed >= AirAttacksPerTurn || AirTime >= AirTurnMaxTime) && CanChangeTurn())
	{
		Land();
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
		EndAirTurn(LandBlendTime, true);
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
	if (bAirborne && Cast<UknightGameplayAbility_MonsterAttack>(Data.AbilityThatEnded))
	{
		++AirAttacksUsed;
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

bool UknightMonsterAirTurnComponent::CanChangeTurn() const
{
	// 공격과 반응(포효·다운)이 끝난 뒤에 바꾼다. 공격 도중에 뜨거나 내리면 판정 위치가 어긋난다.
	return !IsAttacking() && !ASC->HasMatchingGameplayTag(knightGameplayTags::State_Monster_Reacting);
}

void UknightMonsterAirTurnComponent::SendEvent(const FGameplayTag& EventTag) const
{
	FGameplayEventData Payload;
	Payload.EventTag = EventTag;
	Payload.Instigator = GetOwner();
	Payload.Target = GetOwner();
	ASC->HandleGameplayEvent(EventTag, &Payload);
}
