#include "Combat/knightGroggyFeedbackComponent.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Combat/knightHitReactionComponent.h"
#include "Combat/knightHitStopComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "System/knightGameplayTags.h"
#include "TimerManager.h"
#include "knight.h"

namespace
{
	/** 슬로모션을 갱신하는 간격(초, 월드 시간). 60fps 한 프레임 정도. */
	constexpr float ReleaseTickInterval = 1.f / 60.f;

	bool IsInHitStop(const AActor* Actor)
	{
		const UknightHitStopComponent* const HitStop = Actor ? Actor->FindComponentByClass<UknightHitStopComponent>() : nullptr;
		return HitStop && HitStop->IsInHitStop();
	}
}

UknightGroggyFeedbackComponent::UknightGroggyFeedbackComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	GroggyCueTag = knightGameplayTags::GameplayCue_Hit_Groggy;
	ReleaseCueTag = knightGameplayTags::GameplayCue_Hit_Groggy_Release;
}

void UknightGroggyFeedbackComponent::BeginPlay()
{
	Super::BeginPlay();
	PoiseBrokenHandle = UknightHitReactionComponent::OnAnyPoiseBroken.AddUObject(this, &ThisClass::HandleAnyPoiseBroken);
}

void UknightGroggyFeedbackComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UknightHitReactionComponent::OnAnyPoiseBroken.Remove(PoiseBrokenHandle);
	PoiseBrokenHandle.Reset();
	if (UWorld* const World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ReleaseDelayTimer);
	}
	FinishRelease();
	Super::EndPlay(EndPlayReason);
}

void UknightGroggyFeedbackComponent::HandleAnyPoiseBroken(AActor* InVictim, AActor* Instigator, const FGameplayEffectContextHandle& Context)
{
	AActor* const Owner = GetOwner();
	UWorld* const World = GetWorld();
	if (!Owner || !World || !InVictim || Instigator != Owner || InVictim == Owner)
	{
		return;
	}
	// 앞 그로기의 슬로모션이 아직 진행 중이면 정리하고 새로 시작한다.
	World->GetTimerManager().ClearTimer(ReleaseDelayTimer);
	FinishRelease();
	Victim = InVictim;

	// 1. 멈춤: 히트스탑 컴포넌트로 양쪽을 거의 정지(같은 순간의 일반 타격·패링 히트스탑보다 길면 늘어남).
	for (AActor* const Actor : { Owner, InVictim })
	{
		if (UknightHitStopComponent* const HitStop = Actor->FindComponentByClass<UknightHitStopComponent>())
		{
			HitStop->ApplyHitStop(FreezeTime);
		}
	}

	// 타격 큐 위치: 타격 지점이 있으면 거기(일반 공격·스킬), 없으면(패링 그로기) 상대 몸에서 나를 향한 표면 쪽.
	CueParams = FGameplayCueParameters();
	CueParams.Instigator = Owner;
	CueParams.EffectCauser = Owner;
	if (const FHitResult* const Hit = Context.GetHitResult())
	{
		CueParams.Location = Hit->ImpactPoint;
		CueParams.Normal = Hit->ImpactNormal;
	}
	else
	{
		float Radius = 0.f;
		float HalfHeight = 0.f;
		InVictim->GetSimpleCollisionCylinder(Radius, HalfHeight);
		const FVector ToOwner = (Owner->GetActorLocation() - InVictim->GetActorLocation()).GetSafeNormal2D();
		CueParams.Location = InVictim->GetActorLocation() + ToOwner * Radius;
		CueParams.Normal = ToOwner;
	}
	UAbilitySystemComponent* const VictimASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(InVictim);
	if (VictimASC && GroggyCueTag.IsValid())
	{
		VictimASC->ExecuteGameplayCue(GroggyCueTag, CueParams);
	}

	// 2·3. FreezeTime 뒤 '쾅'과 슬로모션. 타이머는 월드 시간이라 액터의 느려짐과 상관없이 정확하다.
	if (FreezeTime > 0.f)
	{
		World->GetTimerManager().SetTimer(ReleaseDelayTimer, this, &ThisClass::StartRelease, FreezeTime, false);
	}
	else
	{
		StartRelease();
	}

	UE_LOG(LogknightCombat, Log, TEXT("%s: groggy feedback on %s (freeze %.2fs + slow release %.2fs from x%.2f, %s)."), *GetNameSafe(Owner), *GetNameSafe(InVictim),
		FreezeTime, SlowReleaseTime, SlowReleaseStartSpeed, Context.GetHitResult() ? TEXT("hit point") : TEXT("no hit point (parry)"));
}

void UknightGroggyFeedbackComponent::StartRelease()
{
	AActor* const Owner = GetOwner();
	AActor* const VictimActor = Victim.Get();
	UWorld* const World = GetWorld();
	if (!Owner || !World)
	{
		return;
	}

	if (UAbilitySystemComponent* const VictimASC = VictimActor ? UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(VictimActor) : nullptr)
	{
		if (ReleaseCueTag.IsValid())
		{
			VictimASC->ExecuteGameplayCue(ReleaseCueTag, CueParams);
		}
	}

	// 원래 속도: 히트스탑이 아직 걸려 있으면(같은 순간에 끝나는 타이머) 그쪽이 1배속으로 돌려놓으므로 1로 본다.
	OwnerBaseSpeed = IsInHitStop(Owner) ? 1.f : Owner->CustomTimeDilation;
	VictimBaseSpeed = !VictimActor || IsInHitStop(VictimActor) ? 1.f : VictimActor->CustomTimeDilation;
	ReleaseStartTime = World->GetTimeSeconds();
	bReleasing = true;
	World->GetTimerManager().SetTimer(ReleaseTickTimer, this, &ThisClass::TickRelease, ReleaseTickInterval, true);
	TickRelease();
}

void UknightGroggyFeedbackComponent::TickRelease()
{
	const UWorld* const World = GetWorld();
	if (!bReleasing || !World)
	{
		return;
	}
	// 처음엔 천천히, 끝에 빨리 정상 속도로(제곱 곡선).
	const float Alpha = SlowReleaseTime > 0.f ? FMath::Clamp(static_cast<float>(World->GetTimeSeconds() - ReleaseStartTime) / SlowReleaseTime, 0.f, 1.f) : 1.f;
	const float Speed = FMath::Lerp(SlowReleaseStartSpeed, 1.f, Alpha * Alpha);

	bool bWaitingForHitStop = false;
	auto ApplySpeed = [Speed, &bWaitingForHitStop](AActor* Actor, float BaseSpeed)
	{
		if (!Actor)
		{
			return;
		}
		// 다른 타격의 히트스탑이 걸려 있으면 그 동안은 히트스탑 컴포넌트에 맡긴다(끝나면 다시 이어서 바꾼다).
		if (IsInHitStop(Actor))
		{
			bWaitingForHitStop = true;
			return;
		}
		Actor->CustomTimeDilation = BaseSpeed * Speed;
	};
	ApplySpeed(GetOwner(), OwnerBaseSpeed);
	ApplySpeed(Victim.Get(), VictimBaseSpeed);
	if (Alpha >= 1.f && !bWaitingForHitStop)
	{
		FinishRelease();
	}
}

void UknightGroggyFeedbackComponent::FinishRelease()
{
	if (!bReleasing)
	{
		return;
	}
	bReleasing = false;
	if (UWorld* const World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ReleaseTickTimer);
	}
	// 원래 속도로 돌려놓는다. 다른 히트스탑이 걸려 있는 액터는 그쪽이 끝날 때 원래 속도로 돌려놓는다.
	if (AActor* const Owner = GetOwner(); Owner && !IsInHitStop(Owner))
	{
		Owner->CustomTimeDilation = OwnerBaseSpeed;
	}
	if (AActor* const VictimActor = Victim.Get(); VictimActor && !IsInHitStop(VictimActor))
	{
		VictimActor->CustomTimeDilation = VictimBaseSpeed;
	}
}
