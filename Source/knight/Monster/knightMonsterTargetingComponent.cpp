#include "Monster/knightMonsterTargetingComponent.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystem/Attributes/knightCoreAttributeSet.h"
#include "Engine/World.h"
#include "GenericTeamAgentInterface.h"
#include "Interfaces/CombatInterface.h"
#include "TimerManager.h"
#include "knight.h"
#include "knightPerfCounters.h"

UknightMonsterTargetingComponent::UknightMonsterTargetingComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UknightMonsterTargetingComponent::BeginPlay()
{
	Super::BeginPlay();
	LastEvaluationTime = GetWorld()->GetTimeSeconds();
	GetWorld()->GetTimerManager().SetTimer(EvaluationTimer, this, &ThisClass::EvaluateTarget, EvaluationInterval, true);
}

void UknightMonsterTargetingComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* const World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(EvaluationTimer);
	}
	if (BoundASC.IsValid())
	{
		if (const UknightCoreAttributeSet* const CoreSet = BoundASC->GetSet<UknightCoreAttributeSet>())
		{
			CoreSet->OnDamaged.RemoveAll(this);
		}
	}
	Super::EndPlay(EndPlayReason);
}

void UknightMonsterTargetingComponent::InitializeTargeting(UAbilitySystemComponent* InASC)
{
	BoundASC = InASC;
	if (const UknightCoreAttributeSet* const CoreSet = InASC ? InASC->GetSet<UknightCoreAttributeSet>() : nullptr)
	{
		CoreSet->OnDamaged.AddUObject(this, &ThisClass::HandleDamaged);
	}
}

void UknightMonsterTargetingComponent::NotifyPerceived(AActor* Actor, bool bSensed)
{
	if (!Actor || Actor == GetOwner() || bStopped)
	{
		return;
	}
	FCandidate* Candidate = FindCandidate(Actor);
	if (!Candidate)
	{
		if (!bSensed)
		{
			return;
		}
		Candidate = &Candidates.AddDefaulted_GetRef();
		Candidate->Actor = Actor;
		Candidate->Priority = PerceivedPriority;
	}
	// Perception은 변화만 알려 주므로, 계속 보이는 타깃은 보이는 동안 갱신이 오지 않는다.
	// 따라서 놓친 순간이 마지막으로 감지된 시점이고, 유지 시간도 그때부터 센다.
	Candidate->bPerceived = bSensed;
	Candidate->LastSensedTime = GetWorld()->GetTimeSeconds();
	// 첫 접촉은 다음 타이머 틱을 기다리지 않고 바로 반응한다.
	if (bSensed && !CurrentTarget.IsValid())
	{
		EvaluateTarget();
	}
}

void UknightMonsterTargetingComponent::AddObjective(AActor* Objective, float Priority)
{
	if (!Objective)
	{
		return;
	}
	FCandidate* Candidate = FindCandidate(Objective);
	if (!Candidate)
	{
		Candidate = &Candidates.AddDefaulted_GetRef();
		Candidate->Actor = Objective;
	}
	Candidate->Priority = Priority;
	Candidate->bObjective = true;
}

void UknightMonsterTargetingComponent::RemoveCandidate(AActor* Actor)
{
	Candidates.RemoveAll([Actor](const FCandidate& Candidate) { return Candidate.Actor.Get() == Actor; });
	if (CurrentTarget.Get() == Actor)
	{
		EvaluateTarget();
	}
}

void UknightMonsterTargetingComponent::HandleDamaged(AActor* DamageInstigator, AActor* DamageCauser, const FGameplayEffectSpec* EffectSpec,
	float Magnitude, float OldValue, float NewValue)
{
	if (!DamageInstigator || DamageInstigator == GetOwner() || bStopped)
	{
		return;
	}
	FCandidate* Candidate = FindCandidate(DamageInstigator);
	if (!Candidate)
	{
		// 맞으면 시야 범위 밖이라도 공격자를 감지한 것으로 친다.
		Candidate = &Candidates.AddDefaulted_GetRef();
		Candidate->Actor = DamageInstigator;
		Candidate->Priority = PerceivedPriority;
		Candidate->bPerceived = true;
	}
	Candidate->LastSensedTime = GetWorld()->GetTimeSeconds();
	Candidate->Threat += Magnitude * ThreatPerDamage;
	if (!CurrentTarget.IsValid())
	{
		EvaluateTarget();
	}
}

void UknightMonsterTargetingComponent::EvaluateTarget()
{
	if (bStopped)
	{
		return;
	}
	KNIGHT_PERF_SCOPE("Monster.Targeting");
	const double Now = GetWorld()->GetTimeSeconds();
	const float Elapsed = static_cast<float>(Now - LastEvaluationTime);
	LastEvaluationTime = Now;
	const float Decay = FMath::Clamp(1.f - ThreatDecayPerSecond * Elapsed, 0.f, 1.f);

	Candidates.RemoveAll([](const FCandidate& Candidate) { return !Candidate.Actor.IsValid(); });

	AActor* Best = nullptr;
	float BestScore = 0.f;
	float CurrentScore = 0.f;
	const FVector OwnerLocation = GetOwner()->GetActorLocation();
	for (FCandidate& Candidate : Candidates)
	{
		Candidate.Threat *= Decay;
		if (CloseAwarenessRadius > 0.f && !Candidate.bObjective
			&& FVector::DistSquared2D(OwnerLocation, Candidate.Actor->GetActorLocation()) <= FMath::Square(CloseAwarenessRadius))
		{
			Candidate.LastSensedTime = Now;
		}
		if (!IsCandidateValid(Candidate, Now))
		{
			continue;
		}
		const float Score = ScoreCandidate(Candidate);
		if (Candidate.Actor == CurrentTarget)
		{
			CurrentScore = Score;
		}
		if (Score > BestScore)
		{
			BestScore = Score;
			Best = Candidate.Actor.Get();
		}
	}

	if (Best && CurrentTarget.IsValid() && Best != CurrentTarget.Get() && CurrentScore > 0.f && BestScore < CurrentScore * SwitchHysteresis)
	{
		return;
	}
	SetCurrentTarget(Best);
}

void UknightMonsterTargetingComponent::StopTargeting()
{
	// 이번 프레임에 이미 쌓인 Perception 이벤트는 사망 후에도 도착할 수 있으니 무시한다.
	bStopped = true;
	if (UWorld* const World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(EvaluationTimer);
	}
	Candidates.Reset();
	SetCurrentTarget(nullptr);
}

UknightMonsterTargetingComponent::FCandidate* UknightMonsterTargetingComponent::FindCandidate(const AActor* Actor)
{
	return Candidates.FindByPredicate([Actor](const FCandidate& Candidate) { return Candidate.Actor.Get() == Actor; });
}

float UknightMonsterTargetingComponent::ScoreCandidate(const FCandidate& Candidate) const
{
	const float Distance = FVector::Dist(GetOwner()->GetActorLocation(), Candidate.Actor->GetActorLocation());
	return Candidate.Priority / (1.f + Distance / DistanceFalloff) + Candidate.Threat;
}

bool UknightMonsterTargetingComponent::IsCandidateValid(const FCandidate& Candidate, double Now) const
{
	AActor* const Actor = Candidate.Actor.Get();
	if (!Actor)
	{
		return false;
	}
	const bool bRemembered = Now - Candidate.LastSensedTime <= LoseTargetDelay;
	if (!Candidate.bPerceived && !Candidate.bObjective && !bRemembered)
	{
		return false;
	}
	if (Actor->GetClass()->ImplementsInterface(UCombatInterface::StaticClass()) && ICombatInterface::Execute_IsDeadBP(Actor))
	{
		return false;
	}
	return FGenericTeamId::GetAttitude(GetOwner(), Actor) == ETeamAttitude::Hostile;
}

void UknightMonsterTargetingComponent::SetCurrentTarget(AActor* NewTarget)
{
	AActor* const OldTarget = CurrentTarget.Get();
	if (OldTarget == NewTarget)
	{
		return;
	}
	CurrentTarget = NewTarget;
	KNIGHT_PERF_COUNT("Monster.TargetChanges", 1);
	UE_LOG(LogknightAI, Log, TEXT("%s target: %s -> %s"), *GetNameSafe(GetOwner()), *GetNameSafe(OldTarget), *GetNameSafe(NewTarget));
	OnTargetChanged.Broadcast(OldTarget, NewTarget);
}
