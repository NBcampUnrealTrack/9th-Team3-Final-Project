#include "Raid/knightRaidGameMode.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFlow/KnightGameFlowSubsystem.h"
#include "GameFlow/KnightStageCatalog.h"
#include "GameFramework/Pawn.h"
#include "Monster/knightMonsterCharacterBase.h"
#include "Monster/knightMonsterPartComponent.h"
#include "Monster/knightMonsterTargetingComponent.h"
#include "Raid/knightRaidDefinition.h"
#include "Raid/knightRaidGameState.h"
#include "Raid/knightRaidSubsystem.h"
#include "System/knightAssetManager.h"
#include "TimerManager.h"
#include "knight.h"

AknightRaidGameMode::AknightRaidGameMode()
{
	GameStateClass = AknightRaidGameState::StaticClass();
}

void AknightRaidGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);
	ResolveRaidDefinition();
}

void AknightRaidGameMode::ResolveRaidDefinition()
{
	RaidDefinition = nullptr;
	const UGameInstance* const GameInstance = GetGameInstance();
	const UKnightGameFlowSubsystem* const Flow = GameInstance ? GameInstance->GetSubsystem<UKnightGameFlowSubsystem>() : nullptr;
	FKnightStageDefinition Stage;
	if (Flow && !Flow->GetActiveStageId().IsNone() && Flow->FindStageDefinition(Flow->GetActiveStageId(), Stage) && !Stage.RaidDefinition.IsNull())
	{
		RaidDefinition = UknightAssetManager::GetAsset(Stage.RaidDefinition);
	}
	if (!RaidDefinition)
	{
		RaidDefinition = DefaultRaidDefinition;
	}
	if (!RaidDefinition)
	{
		UE_LOG(Logknight, Warning, TEXT("%s: no raid definition (stage has none and DefaultRaidDefinition is empty). The raid has no time limit or target."), *GetNameSafe(this));
	}
}

void AknightRaidGameMode::StartPlay()
{
	Super::StartPlay();

	if (AknightRaidGameState* const RaidState = GetRaidGameState())
	{
		RaidState->SetRaidDefinition(RaidDefinition);
	}
	BindPlacedTargetMonsters();

	SetPhase(EknightRaidPhase::Deploy);
	const float DeployDuration = RaidDefinition ? RaidDefinition->DeployDuration : 0.f;
	if (DeployDuration > 0.f)
	{
		GetWorldTimerManager().SetTimer(PhaseTimer, this, &ThisClass::BeginExploration, DeployDuration, false);
	}
	else
	{
		BeginExploration();
	}
}

void AknightRaidGameMode::BindPlacedTargetMonsters()
{
	for (TActorIterator<AknightMonsterCharacterBase> It(GetWorld()); It; ++It)
	{
		if (IsTargetMonster(*It))
		{
			RegisterTargetMonster(*It);
		}
	}
	if (TargetMonsters.IsEmpty())
	{
		UE_LOG(Logknight, Warning, TEXT("%s: no target monster found in the map; the raid cannot be cleared."), *GetNameSafe(this));
	}
}

bool AknightRaidGameMode::IsTargetMonster(const AknightMonsterCharacterBase* Monster) const
{
	if (!Monster)
	{
		return false;
	}
	return !RaidDefinition || !RaidDefinition->TargetMonster || Monster->GetMonsterDefinition() == RaidDefinition->TargetMonster;
}

void AknightRaidGameMode::RegisterTargetMonster(AknightMonsterCharacterBase* Monster)
{
	if (!Monster || Monster->IsDead() || TargetMonsters.Contains(Monster))
	{
		return;
	}
	TargetMonsters.Add(Monster);
	Monster->OnMonsterDefeated.AddUniqueDynamic(this, &ThisClass::HandleMonsterDefeated);
	if (UknightMonsterTargetingComponent* const Targeting = Monster->GetTargetingComponent())
	{
		Targeting->OnTargetChanged.AddUniqueDynamic(this, &ThisClass::HandleMonsterTargetChanged);
	}
	if (UknightMonsterPartComponent* const Parts = Monster->GetPartComponent())
	{
		Parts->OnPartBroken.AddUniqueDynamic(this, &ThisClass::HandlePartBroken);
	}
}

void AknightRaidGameMode::BeginExploration()
{
	if (AknightRaidGameState* const RaidState = GetRaidGameState())
	{
		RaidState->StartRaidClock();
	}
	SetPhase(MonstersFightingPlayer.IsEmpty() ? EknightRaidPhase::Explore : EknightRaidPhase::Combat);

	const float TimeLimit = RaidDefinition ? RaidDefinition->TimeLimit : 0.f;
	if (TimeLimit > 0.f)
	{
		GetWorldTimerManager().SetTimer(TimeLimitTimer, this, &ThisClass::HandleTimeUp, TimeLimit, false);
	}
}

void AknightRaidGameMode::HandleMonsterTargetChanged(AActor* OldTarget, AActor* NewTarget)
{
	// The targeting component does not say which monster changed, so re-check every target monster.
	MonstersFightingPlayer.Reset();
	for (const TWeakObjectPtr<AknightMonsterCharacterBase>& Monster : TargetMonsters)
	{
		const UknightMonsterTargetingComponent* const Targeting = Monster.IsValid() ? Monster->GetTargetingComponent() : nullptr;
		const APawn* const Target = Targeting ? Cast<APawn>(Targeting->GetCurrentTarget()) : nullptr;
		if (Target && Target->IsPlayerControlled() && !Monster->IsDead())
		{
			MonstersFightingPlayer.Add(Monster.Get());
		}
	}

	const AknightRaidGameState* const RaidState = GetRaidGameState();
	const EknightRaidPhase Phase = RaidState ? RaidState->GetRaidPhase() : EknightRaidPhase::None;
	if (Phase == EknightRaidPhase::Explore || Phase == EknightRaidPhase::Combat)
	{
		SetPhase(MonstersFightingPlayer.IsEmpty() ? EknightRaidPhase::Explore : EknightRaidPhase::Combat);
	}
}

void AknightRaidGameMode::HandlePartBroken(FGameplayTag PartTag, AActor* BreakInstigator)
{
	BrokenParts.AddTag(PartTag);
}

void AknightRaidGameMode::HandleMonsterDefeated(AknightMonsterCharacterBase* Monster, AActor* Killer)
{
	MonstersFightingPlayer.Remove(Monster);
	for (const TWeakObjectPtr<AknightMonsterCharacterBase>& Target : TargetMonsters)
	{
		if (Target.IsValid() && !Target->IsDead())
		{
			return;
		}
	}
	FinishRaid(true, EknightRaidFailReason::None);
}

void AknightRaidGameMode::HandlePlayerDeath(AController* PlayerController)
{
	// Replaces the base behaviour: the raid result decides the return delay.
	FinishRaid(false, EknightRaidFailReason::KnightDestroyed);
}

void AknightRaidGameMode::HandleTimeUp()
{
	FinishRaid(false, EknightRaidFailReason::TimeUp);
}

void AknightRaidGameMode::AbandonRaid()
{
	FinishRaid(false, EknightRaidFailReason::Abandoned);
}

void AknightRaidGameMode::FinishRaid(bool bSuccess, EknightRaidFailReason FailReason)
{
	AknightRaidGameState* const RaidState = GetRaidGameState();
	const EknightRaidPhase Phase = RaidState ? RaidState->GetRaidPhase() : EknightRaidPhase::None;
	if (Phase == EknightRaidPhase::Success || Phase == EknightRaidPhase::Failed || Phase == EknightRaidPhase::Returning)
	{
		return;
	}

	GetWorldTimerManager().ClearTimer(PhaseTimer);
	GetWorldTimerManager().ClearTimer(TimeLimitTimer);
	if (RaidState)
	{
		RaidState->StopRaidClock();
	}

	FknightRaidResult Result;
	const UGameInstance* const GameInstance = GetGameInstance();
	if (const UKnightGameFlowSubsystem* const Flow = GameInstance ? GameInstance->GetSubsystem<UKnightGameFlowSubsystem>() : nullptr)
	{
		Result.StageId = Flow->GetActiveStageId();
	}
	Result.bSuccess = bSuccess;
	Result.FailReason = FailReason;
	Result.ElapsedTime = RaidState ? RaidState->GetElapsedRaidTime() : 0.f;
	Result.BrokenParts = BrokenParts;
	if (UknightRaidSubsystem* const RaidSubsystem = GameInstance ? GameInstance->GetSubsystem<UknightRaidSubsystem>() : nullptr)
	{
		RaidSubsystem->RecordResult(Result);
	}

	SetPhase(bSuccess ? EknightRaidPhase::Success : EknightRaidPhase::Failed);

	float Delay = bSuccess ? 20.f : 5.f;
	if (RaidDefinition)
	{
		Delay = bSuccess ? RaidDefinition->SuccessReturnDelay : RaidDefinition->FailureReturnDelay;
	}
	if (FailReason == EknightRaidFailReason::Abandoned)
	{
		Delay = 0.f;
	}
	if (Delay > 0.f)
	{
		GetWorldTimerManager().SetTimer(PhaseTimer, this, &ThisClass::ReturnToMainMenu, Delay, false);
	}
	else
	{
		ReturnToMainMenu();
	}
}

void AknightRaidGameMode::ReturnToMainMenu()
{
	SetPhase(EknightRaidPhase::Returning);
	Super::ReturnToMainMenu();
}

void AknightRaidGameMode::SetPhase(EknightRaidPhase NewPhase)
{
	if (AknightRaidGameState* const RaidState = GetRaidGameState())
	{
		RaidState->SetRaidPhase(NewPhase);
	}
}

AknightRaidGameState* AknightRaidGameMode::GetRaidGameState() const
{
	return GetGameState<AknightRaidGameState>();
}
