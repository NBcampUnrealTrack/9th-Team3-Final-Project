#include "Raid/knightRaidGameMode.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "ContentStreaming.h"
#include "Kismet/GameplayStatics.h"
#if WITH_EDITOR
#include "ShaderCompiler.h"
#endif
#include "AbilitySystem/knightGameplayCueManager.h"
#include "AbilitySystem/Attributes/knightCoreAttributeSet.h"
#include "Engine/GameInstance.h"
#include "knightAudioSubsystem.h"
#include "Sound/SoundBase.h"
#include "System/knightGameplayTags.h"
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
	if (UknightRaidSubsystem* const Session = GetGameInstance()->GetSubsystem<UknightRaidSubsystem>())
	{
		if (!Session->GetCurrentRunId().IsValid())
		{
			const UKnightGameFlowSubsystem* const Flow = GetGameInstance()->GetSubsystem<UKnightGameFlowSubsystem>();
			Session->BeginRun(Flow ? Flow->GetActiveStageId() : NAME_None);
		}
	}

	if (AknightRaidGameState* const RaidState = GetRaidGameState())
	{
		RaidState->SetRaidDefinition(RaidDefinition);
	}
	BindPlacedTargetMonsters();

	if (RaidDefinition)
	{
		if (UknightGameplayCueManager* const CueManager = UknightGameplayCueManager::Get())
		{
			CueManager->PreloadCues(RaidDefinition->PreloadGameplayCues);
		}
		if (UknightAudioSubsystem* const Audio = GetGameInstance()->GetSubsystem<UknightAudioSubsystem>())
		{
			Audio->SetMusicState(RaidDefinition->MusicStates.Explore);
			Audio->PlayMusic(UknightAssetManager::GetAsset(RaidDefinition->Music));
		}
	}

	// (2026-10-19) 월드가 안정될 때까지 로딩 화면(단계 None)과 일시정지를 유지한 뒤 출격한다.
	if (bHoldLoadingUntilReady)
	{
		LoadingHoldStartTime = FPlatformTime::Seconds();
		bPausedForLoading = !UGameplayStatics::IsGamePaused(this) && UGameplayStatics::SetGamePaused(this, true);
		LoadingHoldTicker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &ThisClass::TickLoadingHold), 0.1f);
	}
	else
	{
		BeginDeploy();
	}
}

bool AknightRaidGameMode::TickLoadingHold(float DeltaTime)
{
	const double Elapsed = FPlatformTime::Seconds() - LoadingHoldStartTime;
	const int32 StreamingResources = IStreamingManager::Get().GetNumWantingResources();
	int32 ShaderJobs = 0;
#if WITH_EDITOR
	ShaderJobs = GShaderCompilingManager ? GShaderCompilingManager->GetNumRemainingJobs() : 0;
#endif
	const bool bReady = Elapsed >= MinLoadingHoldTime && StreamingResources <= MaxStreamingResources && ShaderJobs == 0;
	if (!bReady && Elapsed < MaxLoadingHoldTime)
	{
		return true;
	}
	UE_LOG(Logknight, Log, TEXT("%s: raid loading hold %.2fs (%s, streaming %d, shader jobs %d)."), *GetNameSafe(this), Elapsed,
		bReady ? TEXT("ready") : TEXT("timed out"), StreamingResources, ShaderJobs);
	LoadingHoldTicker.Reset();
	if (bPausedForLoading)
	{
		bPausedForLoading = false;
		UGameplayStatics::SetGamePaused(this, false);
	}
	BeginDeploy();
	return false;
}

void AknightRaidGameMode::BeginDeploy()
{
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
	if (UAbilitySystemComponent* const ASC = Monster->GetAbilitySystemComponent())
	{
		ASC->RegisterGameplayTagEvent(knightGameplayTags::State_Monster_Enraged, EGameplayTagEventType::NewOrRemoved)
			.AddUObject(this, &ThisClass::HandleMonsterEnragedChanged);
		ASC->GetGameplayAttributeValueChangeDelegate(UknightCoreAttributeSet::GetHealthAttribute())
			.AddUObject(this, &ThisClass::HandleMonsterHealthChanged);
	}
}

void AknightRaidGameMode::HandleMonsterHealthChanged(const FOnAttributeChangeData& Data)
{
	if (bCombatMusicLowHealth || !RaidDefinition)
	{
		return;
	}
	// The change data does not say whose health changed, so check every target monster.
	for (const TWeakObjectPtr<AknightMonsterCharacterBase>& Monster : TargetMonsters)
	{
		const UAbilitySystemComponent* const MonsterASC = Monster.IsValid() ? Monster->GetAbilitySystemComponent() : nullptr;
		if (!MonsterASC)
		{
			continue;
		}
		bool bFound = false;
		const float MaxHealth = MonsterASC->GetGameplayAttributeValue(UknightCoreAttributeSet::GetMaxHealthAttribute(), bFound);
		const float Health = MonsterASC->GetGameplayAttributeValue(UknightCoreAttributeSet::GetHealthAttribute(), bFound);
		if (MaxHealth > 0.f && Health / MaxHealth < RaidDefinition->CombatMusicLowHealthRatio)
		{
			bCombatMusicLowHealth = true;
			UpdateMusic();
			return;
		}
	}
}

void AknightRaidGameMode::HandleMonsterEnragedChanged(const FGameplayTag Tag, int32 NewCount)
{
	UpdateMusic();
}

void AknightRaidGameMode::UpdateMusic()
{
	UknightAudioSubsystem* const Audio = GetGameInstance() ? GetGameInstance()->GetSubsystem<UknightAudioSubsystem>() : nullptr;
	const AknightRaidGameState* const RaidState = GetRaidGameState();
	if (!Audio || !RaidDefinition || !RaidState)
	{
		return;
	}
	UpdateCombatMusic(RaidState->GetRaidPhase());
	const FknightRaidMusicStates& States = RaidDefinition->MusicStates;
	switch (RaidState->GetRaidPhase())
	{
	case EknightRaidPhase::Combat:
	{
		bool bEnraged = false;
		for (const TWeakObjectPtr<AActor>& Fighting : MonstersFightingPlayer)
		{
			const UAbilitySystemComponent* const ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Fighting.Get());
			bEnraged |= ASC && ASC->HasMatchingGameplayTag(knightGameplayTags::State_Monster_Enraged);
		}
		Audio->SetMusicState(bEnraged ? States.Enraged : States.Combat);
		break;
	}
	case EknightRaidPhase::Success:
		Audio->SetMusicState(States.Success);
		break;
	case EknightRaidPhase::Returning:
		// Keep the result music playing until the map changes.
		break;
	case EknightRaidPhase::Failed:
		Audio->SetMusicState(States.Failed);
		break;
	default:
		Audio->SetMusicState(States.Explore);
		break;
	}
}

void AknightRaidGameMode::UpdateCombatMusic(EknightRaidPhase Phase)
{
	UknightAudioSubsystem* const Audio = GetGameInstance() ? GetGameInstance()->GetSubsystem<UknightAudioSubsystem>() : nullptr;
	if (!Audio || !RaidDefinition || RaidDefinition->CombatMusic.IsNull() || bCombatMusicEnded)
	{
		return;
	}
	if (Phase == EknightRaidPhase::Success || Phase == EknightRaidPhase::Failed || Phase == EknightRaidPhase::Returning)
	{
		if (PlayingCombatTrack)
		{
			Audio->StopMusic(RaidDefinition->CombatMusicSwitchFadeTime);
		}
		PlayingCombatTrack = nullptr;
		bCombatMusicEnded = true;
		return;
	}
	// The first fight starts the music; after that it carries on through exploring.
	if (Phase != EknightRaidPhase::Combat && !PlayingCombatTrack)
	{
		return;
	}
	const TSoftObjectPtr<USoundBase>& Wanted = bCombatMusicLowHealth && !RaidDefinition->CombatMusicLowHealth.IsNull()
		? RaidDefinition->CombatMusicLowHealth : RaidDefinition->CombatMusic;
	USoundBase* const Track = UknightAssetManager::GetAsset(Wanted);
	if (!Track || Track == PlayingCombatTrack)
	{
		return;
	}
	// Starts with the default fade, switches tracks with the longer crossfade.
	Audio->PlayMusic(Track, PlayingCombatTrack ? RaidDefinition->CombatMusicSwitchFadeTime : -1.f, RaidDefinition->CombatMusicLoopGap);
	PlayingCombatTrack = Track;
}

void AknightRaidGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (LoadingHoldTicker.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(LoadingHoldTicker);
		LoadingHoldTicker.Reset();
	}
	if (UknightGameplayCueManager* const CueManager = UknightGameplayCueManager::Get())
	{
		CueManager->ReleasePreloadedCues();
	}
	if (UknightAudioSubsystem* const Audio = GetGameInstance() ? GetGameInstance()->GetSubsystem<UknightAudioSubsystem>() : nullptr)
	{
		Audio->StopMusic(0.5f);
	}
	Super::EndPlay(EndPlayReason);
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

bool AknightRaidGameMode::RequestStartRaid()
{
	const AknightRaidGameState* const State = GetRaidGameState();
	if (!State || State->GetRaidPhase() != EknightRaidPhase::Deploy) return false;
	GetWorldTimerManager().ClearTimer(PhaseTimer);
	BeginExploration();
	return true;
}

void AknightRaidGameMode::NotifyObjectiveCompleted()
{
	FinishRaid(true, EknightRaidFailReason::None);
}

void AknightRaidGameMode::NotifyPlayerDefeated()
{
	FinishRaid(false, EknightRaidFailReason::KnightDestroyed);
}

void AknightRaidGameMode::RequestReturnToLobby()
{
	const AknightRaidGameState* const State = GetRaidGameState();
	if (State && (State->GetRaidPhase() == EknightRaidPhase::Success
		|| State->GetRaidPhase() == EknightRaidPhase::Failed)) ReturnToMainMenu();
	else AbandonRaid();
}

void AknightRaidGameMode::DebugFinishRaid(bool bSuccess)
{
	if (bSuccess) NotifyObjectiveCompleted();
	else NotifyPlayerDefeated();
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
	NotifyPlayerDefeated();
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
	if (const UknightRaidSubsystem* const Session = GameInstance ? GameInstance->GetSubsystem<UknightRaidSubsystem>() : nullptr)
	{
		Result.RunId = Session->GetCurrentRunId();
	}
	if (const UKnightGameFlowSubsystem* const Flow = GameInstance ? GameInstance->GetSubsystem<UKnightGameFlowSubsystem>() : nullptr)
	{
		Result.StageId = Flow->GetActiveStageId();
	}
	Result.bSuccess = bSuccess;
	Result.FailReason = FailReason;
	Result.ElapsedTime = RaidState ? RaidState->GetElapsedRaidTime() : 0.f;
	Result.BrokenParts = BrokenParts;
	if (bSuccess && RaidDefinition)
	{
		RaidDefinition->RollRewards(BrokenParts, Result.Rewards);
	}
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
	// Also runs when the phase is unchanged: which monsters are fighting may have changed.
	UpdateMusic();
}

AknightRaidGameState* AknightRaidGameMode::GetRaidGameState() const
{
	return GetGameState<AknightRaidGameState>();
}
