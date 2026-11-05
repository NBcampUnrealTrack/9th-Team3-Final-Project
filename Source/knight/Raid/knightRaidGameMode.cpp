#include "Raid/knightRaidGameMode.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "ContentStreaming.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Kismet/GameplayStatics.h"
#include "PipelineStateCache.h"
#include "ShaderPipelineCache.h"
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
#include "Raid/knightRaidFinishPresentation.h"
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
	if (IsMapTestMode())
	{
		if (UknightRaidSubsystem* const Session = GetGameInstance()->GetSubsystem<UknightRaidSubsystem>())
		{
			Session->EndRun();
		}
		if (AknightRaidGameState* const RaidState = GetRaidGameState())
		{
			RaidState->SetRaidDefinition(RaidDefinition);
		}
		// Release the raid loading gate without starting an encounter or its clock.
		SetPhase(EknightRaidPhase::Explore);
		return;
	}
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
		// Combat music starts mid-raid; loading it then would stall the game thread.
		TArray<FSoftObjectPath> MusicPaths;
		for (const TSoftObjectPtr<USoundBase>* const Track : { &RaidDefinition->CombatMusic, &RaidDefinition->CombatMusicLowHealth })
		{
			if (!Track->IsNull())
			{
				MusicPaths.Add(Track->ToSoftObjectPath());
			}
		}
		if (!MusicPaths.IsEmpty())
		{
			CombatMusicHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(MusicPaths);
		}
	}

	// 월드가 안정될 때까지 로딩 화면(단계 None)과 일시정지를 유지한 뒤 출격한다.
	if (bHoldLoadingUntilReady)
	{
		LoadingHoldStartTime = FPlatformTime::Seconds();
		if (AknightRaidGameState* const RaidState = GetRaidGameState())
		{
			RaidState->SetLoadingStatus(NSLOCTEXT("knight", "RaidLoadingPreparing", "레이드 준비 중"));
		}
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
	const uint32 BundledPSOs = FShaderPipelineCache::NumPrecompilesRemaining();
	const uint32 PrecacheRequests = PipelineStateCache::NumActivePrecacheRequests();
	const int32 ActivePSOTasks = PipelineStateCache::GetNumActivePipelinePrecompileTasks();
	int32 ShaderJobs = 0;
#if WITH_EDITOR
	ShaderJobs = GShaderCompilingManager ? GShaderCompilingManager->GetNumRemainingJobs() : 0;
#endif
	// Any load still in flight (preloaded cues, camera screen effects, combat music) would be waited for by the first
	// synchronous load after the raid starts, so it stalls a frame the player sees.
	const int32 AsyncPackages = GetNumAsyncPackages();
	const bool bReady = Elapsed >= MinLoadingHoldTime && StreamingResources <= MaxStreamingResources
		&& ShaderJobs == 0 && BundledPSOs == 0 && PrecacheRequests == 0 && ActivePSOTasks == 0 && AsyncPackages == 0;
	if (!bReady)
	{
		if (AknightRaidGameState* const RaidState = GetRaidGameState())
		{
			// These counters can describe the same compile, so do not add them into a misleading job count.
			const FText PSOStatus = (BundledPSOs > 0 || PrecacheRequests > 0 || ActivePSOTasks > 0)
				? NSLOCTEXT("knight", "RaidLoadingPSOActive", "일반 PSO 준비 중")
				: NSLOCTEXT("knight", "RaidLoadingPSOReady", "일반 PSO 준비 완료");
			RaidState->SetLoadingStatus(FText::Format(NSLOCTEXT("knight", "RaidLoadingProgress", "레이드 준비 중 · 스트리밍 {0} · 셰이더 {1} · {2} · 에셋 {3}"),
				FText::AsNumber(StreamingResources), FText::AsNumber(ShaderJobs), PSOStatus, FText::AsNumber(AsyncPackages)));
		}
		if (Elapsed < FMath::Max<double>(MaxLoadingHoldTime, MinLoadingHoldTime))
		{
			return true;
		}
	}
	if (bReady)
	{
		UE_LOG(Logknight, Log, TEXT("%s: raid loading hold %.2fs (tracked tasks ready, streaming %d, shader jobs %d, bundled PSOs %u, precache requests %u, active PSO tasks %d, async packages %d; RTPSOs untracked)."),
			*GetNameSafe(this), Elapsed, StreamingResources, ShaderJobs, BundledPSOs, PrecacheRequests, ActivePSOTasks, AsyncPackages);
	}
	else
	{
		UE_LOG(Logknight, Error, TEXT("%s: raid loading hold %.2fs (tracked preparation timed out, streaming %d, shader jobs %d, bundled PSOs %u, precache requests %u, active PSO tasks %d, async packages %d; RTPSOs untracked)."),
			*GetNameSafe(this), Elapsed, StreamingResources, ShaderJobs, BundledPSOs, PrecacheRequests, ActivePSOTasks, AsyncPackages);
	}
	LoadingHoldTicker.Reset();
	if (bPausedForLoading)
	{
		bPausedForLoading = false;
		UGameplayStatics::SetGamePaused(this, false);
	}
	if (bReady)
	{
		BeginDeploy();
	}
	else
	{
		// Never begin the raid while tracked preparation is incomplete. Keep phase None visible until travel starts.
		Super::ReturnToMainMenu();
	}
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
	if (!Monster || IsMapTestMode())
	{
		return false;
	}
	return !RaidDefinition || !RaidDefinition->TargetMonster || Monster->GetMonsterDefinition() == RaidDefinition->TargetMonster;
}

void AknightRaidGameMode::RegisterTargetMonster(AknightMonsterCharacterBase* Monster)
{
	if (IsMapTestMode() || !Monster || Monster->IsDead() || TargetMonsters.Contains(Monster))
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
		Parts->OnPartBrokenNative.AddUObject(this, &ThisClass::HandlePartBroken);
	}
	if (UAbilitySystemComponent* const ASC = Monster->GetAbilitySystemComponent())
	{
		ASC->RegisterGameplayTagEvent(knightGameplayTags::State_Monster_Enraged, EGameplayTagEventType::NewOrRemoved)
			.AddUObject(this, &ThisClass::HandleMonsterEnragedChanged);
		// 보스 BGM은 2페이즈에 들어가도 Enrage 곡으로 바뀐다(bCombatMusicDynamic).
		ASC->RegisterGameplayTagEvent(knightGameplayTags::State_Monster_Phase2, EGameplayTagEventType::NewOrRemoved)
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
		if (PlayingCombatTrack && Phase == EknightRaidPhase::Success && RaidDefinition->bCombatMusicDynamic)
		{
			// 처치 연출 중에는 몸이 땅에 닿을 때(HandleFinishImpact)까지 지금 곡을 이어 간다.
			const AknightRaidGameState* const RaidState = GetRaidGameState();
			if (RaidState && RaidState->IsFinishPresentationPlaying() && RaidState->GetFinishStage() < EknightRaidFinishStage::Impact)
			{
				return;
			}
			// 끄지 않고 지금 상태(Normal/Enrage)의 엔딩으로 넘어간다. 엔딩이 끝나면 MetaSound가 스스로 끝난다.
			Audio->SetMusicFloatParameter(TEXT("Ending"), 1.f);
			Audio->TriggerMusicParameter(TEXT("PlayEnding"));
		}
		else if (PlayingCombatTrack)
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
	if (Track && Track != PlayingCombatTrack)
	{
		// Starts with the default fade, switches tracks with the longer crossfade.
		Audio->PlayMusic(Track, PlayingCombatTrack ? RaidDefinition->CombatMusicSwitchFadeTime : -1.f, RaidDefinition->CombatMusicLoopGap);
		PlayingCombatTrack = Track;
		CombatMusicIntensity = 0.f;  // 새 MetaSound는 Normal에서 시작한다.
	}
	if (PlayingCombatTrack && RaidDefinition->bCombatMusicDynamic)
	{
		UpdateCombatMusicIntensity(*Audio);
	}
}

void AknightRaidGameMode::UpdateCombatMusicIntensity(UknightAudioSubsystem& Audio)
{
	// 싸우는 중인지가 아니라 살아 있는 대상 몬스터를 본다. 잠깐 어그로가 풀려도 2페이즈 곡은 그대로 간다.
	bool bIntense = false;
	bool bAnyAlive = false;
	for (const TWeakObjectPtr<AknightMonsterCharacterBase>& Monster : TargetMonsters)
	{
		const UAbilitySystemComponent* const ASC = Monster.IsValid() && !Monster->IsDead() ? Monster->GetAbilitySystemComponent() : nullptr;
		bAnyAlive |= ASC != nullptr;
		bIntense |= ASC && (ASC->HasMatchingGameplayTag(knightGameplayTags::State_Monster_Phase2)
			|| ASC->HasMatchingGameplayTag(knightGameplayTags::State_Monster_Enraged));
	}
	// 마지막 대상이 죽으면 그때 곡을 유지한다. 사망으로 분노가 풀려 Normal로 돌아가면 엔딩도 Normal 쪽이 골라진다.
	if (!bAnyAlive)
	{
		return;
	}
	const float Wanted = bIntense ? 1.f : 0.f;
	if (Wanted == CombatMusicIntensity)
	{
		return;
	}
	// 페이드 시간을 먼저 바꿔야 이번 전환에 쓰인다.
	Audio.SetMusicFloatParameter(TEXT("IntensityFadeTime"), bIntense ? RaidDefinition->CombatMusicEnrageFadeTime : RaidDefinition->CombatMusicCalmFadeTime);
	Audio.SetMusicFloatParameter(TEXT("Intensity"), Wanted);
	CombatMusicIntensity = Wanted;
	UE_LOG(Logknight, Log, TEXT("Combat music intensity -> %.0f"), Wanted);
}

void AknightRaidGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (FinishPresentation)
	{
		FinishPresentation->Stop();
		FinishPresentation = nullptr;
	}
	if (LoadingHoldTicker.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(LoadingHoldTicker);
		LoadingHoldTicker.Reset();
	}
	if (bPausedForLoading)
	{
		bPausedForLoading = false;
		UGameplayStatics::SetGamePaused(this, false);
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
	if (IsMapTestMode()) return false;
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

bool AknightRaidGameMode::RecordBrokenPart(FGuid RunId, FName BossId,
	AknightMonsterCharacterBase* Monster, FGameplayTag PartTag)
{
	const UGameInstance* const GameInstance = GetGameInstance();
	const UknightRaidSubsystem* const Raid = GameInstance ? GameInstance->GetSubsystem<UknightRaidSubsystem>() : nullptr;
	const AknightRaidGameState* const State = GetRaidGameState();
	const EknightRaidPhase Phase = State ? State->GetRaidPhase() : EknightRaidPhase::None;
	if (!Raid || !RunId.IsValid() || Raid->GetCurrentRunId() != RunId || !Monster
		|| !TargetMonsters.Contains(Monster) || !IsTargetMonster(Monster)
		|| (Phase != EknightRaidPhase::Explore && Phase != EknightRaidPhase::Combat)
		|| !PartTag.IsValid() || !PartTag.MatchesTag(knightGameplayTags::Monster_Part)
		|| BrokenParts.HasTagExact(PartTag))
	{
		return false;
	}
	FKnightQuestRunContext Context;
	const bool bQuestRun = Raid->GetActiveQuestRunContext(Context);
	if ((bQuestRun && BossId != Context.BossId) || (!bQuestRun && !BossId.IsNone())) return false;
	const UknightMonsterPartComponent* const Parts = Monster->GetPartComponent();
	if (!Parts || !Parts->IsPartBroken(PartTag)) return false;
	BrokenParts.AddTag(PartTag);
	return true;
}

void AknightRaidGameMode::HandlePartBroken(FGameplayTag PartTag, AActor* BreakInstigator, AActor* BrokenMonster)
{
	const UGameInstance* const GameInstance = GetGameInstance();
	const UknightRaidSubsystem* const Raid = GameInstance ? GameInstance->GetSubsystem<UknightRaidSubsystem>() : nullptr;
	if (!Raid) return;
	FKnightQuestRunContext Context;
	const FName BossId = Raid->GetActiveQuestRunContext(Context) ? Context.BossId : NAME_None;
	RecordBrokenPart(Raid->GetCurrentRunId(), BossId, Cast<AknightMonsterCharacterBase>(BrokenMonster), PartTag);
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
	if (Monster)
	{
		StartFinishPresentation(*Monster);
	}
	FinishRaid(true, EknightRaidFailReason::None);
}

void AknightRaidGameMode::StartFinishPresentation(AknightMonsterCharacterBase& Monster)
{
	AknightRaidGameState* const RaidState = GetRaidGameState();
	APlayerController* const PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	const EknightRaidPhase Phase = RaidState ? RaidState->GetRaidPhase() : EknightRaidPhase::None;
	if (!RaidDefinition || !RaidState || !PC || IsMapTestMode() || FinishPresentation
		|| Phase == EknightRaidPhase::Success || Phase == EknightRaidPhase::Failed || Phase == EknightRaidPhase::Returning)
	{
		return;
	}
	UknightRaidFinishPresentation* const Presentation = NewObject<UknightRaidFinishPresentation>(this);
	Presentation->OnImpact = [this]() { HandleFinishImpact(); };
	Presentation->OnDone = [this]() { HandleFinishDone(); };
	if (Presentation->Start(Monster, *PC, *RaidState, RaidDefinition->FinishPresentation))
	{
		FinishPresentation = Presentation;
	}
}

bool AknightRaidGameMode::IsFinishPresentationPlaying() const
{
	const AknightRaidGameState* const RaidState = GetRaidGameState();
	return RaidState && RaidState->IsFinishPresentationPlaying();
}

void AknightRaidGameMode::HandleFinishImpact()
{
	// 쓰러져 땅에 닿는 순간 보스 BGM 엔딩으로 넘어간다.
	UpdateCombatMusic(EknightRaidPhase::Success);
}

void AknightRaidGameMode::HandleFinishDone()
{
	if (PendingReturnDelay > 0.f)
	{
		GetWorldTimerManager().SetTimer(PhaseTimer, this, &ThisClass::ReturnToMainMenu, PendingReturnDelay, false);
	}
	else
	{
		ReturnToMainMenu();
	}
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
	if (IsMapTestMode())
	{
		ReturnToMainMenu();
		return;
	}
	FinishRaid(false, EknightRaidFailReason::Abandoned);
}

bool AknightRaidGameMode::IsMapTestMode() const
{
	return RaidDefinition && RaidDefinition->IsWaitingForTargetMonster();
}

void AknightRaidGameMode::FinishRaid(bool bSuccess, EknightRaidFailReason FailReason)
{
	if (IsMapTestMode()) return;
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
	FKnightQuestRunContext QuestContext;
	const UknightRaidSubsystem* const ActiveRun = GameInstance ? GameInstance->GetSubsystem<UknightRaidSubsystem>() : nullptr;
	const bool bQuestRun = ActiveRun && ActiveRun->GetActiveQuestRunContext(QuestContext)
		&& QuestContext.RunId == Result.RunId && QuestContext.StageId == Result.StageId;
	if (bSuccess && RaidDefinition && !bQuestRun)
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
	if (bSuccess && IsFinishPresentationPlaying())
	{
		// 귀환 대기는 처치 연출이 끝나고 결과 화면이 열릴 때부터 센다.
		PendingReturnDelay = Delay;
		return;
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
