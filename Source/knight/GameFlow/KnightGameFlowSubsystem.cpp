#include "KnightGameFlowSubsystem.h"
#include "Quest/KnightProgressionIds.h"

#include "KnightFlowSettings.h"
#include "Engine/Engine.h"
#include "Engine/StreamableManager.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/PackageName.h"
#include "Progression/knightProgressionSubsystem.h"
#include "Progression/knightSaveSubsystem.h"
#include "Quest/KnightQuestData.h"
#include "Raid/knightRaidDefinition.h"
#include "Raid/knightRaidSubsystem.h"
#include "System/knightAssetManager.h"
#include "UObject/UObjectGlobals.h"
#include "knight.h"

#define LOCTEXT_NAMESPACE "KnightGameFlow"

void UKnightGameFlowSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	PostLoadMapHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &ThisClass::HandlePostLoadMap);
	if (GEngine)
	{
		TravelFailureHandle = GEngine->OnTravelFailure().AddUObject(this, &ThisClass::HandleTravelFailure);
	}
}

void UKnightGameFlowSubsystem::Deinitialize()
{
	FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(PostLoadMapHandle);
	if (GEngine && TravelFailureHandle.IsValid())
	{
		GEngine->OnTravelFailure().Remove(TravelFailureHandle);
	}
	ResetPendingTravel();
	RegisteredWorld.Reset();
	Super::Deinitialize();
}

bool UKnightGameFlowSubsystem::RequestOpenMainMenu()
{
	return RequestOpenMenu(EKnightFlowState::MainMenu);
}

bool UKnightGameFlowSubsystem::RequestOpenTitle()
{
	if ((CurrentFlowState == EKnightFlowState::MainMenu || CurrentFlowState == EKnightFlowState::Gameplay)
		&& !bTravelPending)
	{
		if (UknightSaveSubsystem* const Save = GetGameInstance()->GetSubsystem<UknightSaveSubsystem>())
		{
			Save->RequestSave();
		}
	}
	return RequestOpenMenu(EKnightFlowState::Title);
}

const UKnightStageCatalog* UKnightGameFlowSubsystem::GetStageCatalog() const
{
	const TSoftObjectPtr<UKnightStageCatalog>& Catalog = GetDefault<UKnightFlowSettings>()->StageCatalog;
	if (Catalog.IsNull())
	{
		UE_LOG(Logknight, Warning, TEXT("Knight Flow Settings has no StageCatalog."));
		return nullptr;
	}
	const UKnightStageCatalog* const Loaded = Catalog.LoadSynchronous();
	if (!Loaded)
	{
		UE_LOG(Logknight, Error, TEXT("Cannot load StageCatalog %s."), *Catalog.ToString());
	}
	return Loaded;
}

bool UKnightGameFlowSubsystem::GetStageDefinitions(TArray<FKnightStageDefinition>& OutStages) const
{
	OutStages.Reset();
	const UKnightStageCatalog* const Catalog = GetStageCatalog();
	if (!Catalog)
	{
		return false;
	}
	if (Catalog->Stages.IsEmpty())
	{
		UE_LOG(Logknight, Warning, TEXT("StageCatalog %s has no stages."), *Catalog->GetPathName());
		return false;
	}
	TMap<FName, int32> IdCounts;
	for (const FKnightStageDefinition& Stage : Catalog->Stages)
	{
		if (!Stage.StageId.IsNone())
		{
			IdCounts.FindOrAdd(KnightProgressionIds::Stage(Stage.StageId))++;
		}
	}
	for (const FKnightStageDefinition& Stage : Catalog->Stages)
	{
		if (Stage.StageId.IsNone())
		{
			UE_LOG(Logknight, Error, TEXT("StageCatalog %s has a stage with an empty StageId."), *Catalog->GetPathName());
		}
		else if (IdCounts.FindChecked(KnightProgressionIds::Stage(Stage.StageId)) != 1)
		{
			UE_LOG(Logknight, Error, TEXT("StageCatalog %s has duplicate StageId %s; all matching entries are excluded."),
				*Catalog->GetPathName(), *Stage.StageId.ToString());
		}
		else
		{
			if (Stage.bDevelopmentOnly)
			{
				bool bEditorPIE = false;
#if WITH_EDITOR
				const UWorld* const World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
				bEditorPIE = World && World->WorldType == EWorldType::PIE;
#endif
				if (!bEditorPIE) continue;
			}
			OutStages.Add(Stage);
			OutStages.Last().StageId = KnightProgressionIds::Stage(Stage.StageId);
		}
	}
	return !OutStages.IsEmpty();
}

bool UKnightGameFlowSubsystem::FindStageDefinition(FName StageId, FKnightStageDefinition& OutStage) const
{
	StageId = KnightProgressionIds::Stage(StageId);
	OutStage = FKnightStageDefinition();
	if (StageId.IsNone())
	{
		UE_LOG(Logknight, Warning, TEXT("A stage lookup used an empty StageId."));
		return false;
	}
	const UKnightStageCatalog* const Catalog = GetStageCatalog();
	if (!Catalog)
	{
		return false;
	}
	const FKnightStageDefinition* Found = nullptr;
	for (const FKnightStageDefinition& Stage : Catalog->Stages)
	{
		if (KnightProgressionIds::Stage(Stage.StageId) == StageId)
		{
			if (Found)
			{
				UE_LOG(Logknight, Error, TEXT("StageCatalog %s has duplicate StageId %s."), *Catalog->GetPathName(), *StageId.ToString());
				return false;
			}
			Found = &Stage;
		}
	}
	if (!Found)
	{
		UE_LOG(Logknight, Warning, TEXT("StageId %s was not found in StageCatalog %s."), *StageId.ToString(), *Catalog->GetPathName());
		return false;
	}
	OutStage = *Found;
	OutStage.StageId = StageId;
	return true;
}

bool UKnightGameFlowSubsystem::CanLaunchStage(FName StageId, FText& OutReason) const
{
	FKnightStageDefinition Stage;
	if (!FindStageDefinition(StageId, Stage))
	{
		OutReason = LOCTEXT("InvalidStage", "스테이지 정보를 확인할 수 없습니다.");
		return false;
	}
	if (Stage.GameplayMap.IsNull())
	{
		OutReason = LOCTEXT("StageMapNotConfigured", "이 스테이지의 맵이 아직 지정되지 않았습니다.");
		return false;
	}
	FString MapPackage;
	if (!ResolveMapPackage(Stage.GameplayMap, MapPackage))
	{
		UE_LOG(Logknight, Error, TEXT("Stage %s has an invalid GameplayMap: %s."), *StageId.ToString(), *Stage.GameplayMap.ToString());
		OutReason = LOCTEXT("StageMapInvalid", "스테이지 맵을 열 수 없습니다.");
		return false;
	}
	const UknightProgressionSubsystem* const Progress = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UknightProgressionSubsystem>() : nullptr;
	if (!Progress || !Progress->IsStageUnlocked(StageId))
	{
		OutReason = LOCTEXT("StageLocked", "This raid is not unlocked or the profile could not be loaded.");
		return false;
	}
	if (!Stage.RaidDefinition.IsNull()
		&& !FPackageName::DoesPackageExist(Stage.RaidDefinition.ToSoftObjectPath().GetLongPackageName()))
	{
		OutReason = LOCTEXT("RaidDefinitionMissing", "The raid definition is missing.");
		return false;
	}
	const UknightRaidDefinition* const Raid = Stage.RaidDefinition.LoadSynchronous();
	if (Raid && Raid->IsWaitingForTargetMonster())
	{
		OutReason = LOCTEXT("RaidTargetNotReady", "이 레이드의 몬스터가 아직 준비되지 않았습니다.");
		return false;
	}
	OutReason = FText::GetEmpty();
	return true;
}

bool UKnightGameFlowSubsystem::ResolveMapPackage(const TSoftObjectPtr<UWorld>& Map, FString& OutPackage) const
{
	OutPackage.Reset();
	if (Map.IsNull())
	{
		return false;
	}
	const FSoftObjectPath MapPath = Map.ToSoftObjectPath();
	const FString PackageName = MapPath.GetLongPackageName();
	if (!FPackageName::IsValidLongPackageName(PackageName, true)
		|| MapPath.GetAssetName() != FPackageName::GetShortName(PackageName)
		|| !FPackageName::DoesPackageExist(PackageName))
	{
		return false;
	}
	OutPackage = PackageName;
	return true;
}

bool UKnightGameFlowSubsystem::RequestStartStage(FName StageId)
{
	FText Reason;
	return RequestStartStageInternal(StageId, nullptr, Reason);
}

bool UKnightGameFlowSubsystem::RequestStartQuest(FName QuestId, FText& OutReason)
{
	if (bTravelPending)
	{
		OutReason = LOCTEXT("QuestTravelPending", "이미 출격 준비 중입니다.");
		return RejectRequest(EKnightFlowState::Gameplay, OutReason);
	}
	if (CurrentFlowState != EKnightFlowState::MainMenu)
	{
		OutReason = LOCTEXT("QuestNotInMenu", "메인메뉴에서만 출격할 수 있습니다.");
		return RejectRequest(EKnightFlowState::Gameplay, OutReason);
	}
	const UknightSaveSubsystem* const Saves = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UknightSaveSubsystem>() : nullptr;
	if (!Saves || !Saves->HasSelectedSlot())
	{
		OutReason = LOCTEXT("QuestNoSaveSlot", "저장 슬롯을 먼저 선택해야 합니다.");
		return RejectRequest(EKnightFlowState::Gameplay, OutReason);
	}
	const UKnightQuestCatalog* const Catalog = UKnightQuestSettings::LoadCatalog();
	const UKnightQuestDefinition* const Quest = Catalog ? Catalog->FindQuest(QuestId) : nullptr;
	if (!Catalog || !Quest)
	{
		UE_LOG(Logknight, Error, TEXT("Quest launch rejected: missing catalog or QuestId %s."), *QuestId.ToString());
		OutReason = LOCTEXT("QuestMissing", "퀘스트 정보를 확인할 수 없습니다.");
		return RejectRequest(EKnightFlowState::Gameplay, OutReason);
	}
	if (!Catalog->CanSelectQuest(QuestId, GetGameInstance(), OutReason))
	{
		UE_LOG(Logknight, Warning, TEXT("Quest %s cannot launch: %s"), *QuestId.ToString(), *OutReason.ToString());
		return RejectRequest(EKnightFlowState::Gameplay, OutReason);
	}
	return RequestStartStageInternal(Quest->StageId, Quest, OutReason);
}

bool UKnightGameFlowSubsystem::RequestStartStageInternal(FName StageId, const UKnightQuestDefinition* Quest, FText& OutReason)
{
	StageId = KnightProgressionIds::Stage(StageId);
	if (bTravelPending)
	{
		OutReason = LOCTEXT("TravelAlreadyPending", "A level transition is already in progress.");
		return RejectRequest(EKnightFlowState::Gameplay, OutReason);
	}
	if (CurrentFlowState != EKnightFlowState::MainMenu)
	{
		OutReason = LOCTEXT("NotInMainMenu", "Stages can only launch from the main menu.");
		return RejectRequest(EKnightFlowState::Gameplay, OutReason);
	}
	if (!CanLaunchStage(StageId, OutReason))
	{
		return RejectRequest(EKnightFlowState::Gameplay, OutReason);
	}
	FKnightStageDefinition Stage;
	if (!FindStageDefinition(StageId, Stage))
	{
		OutReason = LOCTEXT("StageChanged", "Stage definition is no longer available.");
		return RejectRequest(EKnightFlowState::Gameplay, OutReason);
	}
	UGameInstance* const GameInstance = GetGameInstance();
	UWorld* const World = GameInstance ? GameInstance->GetWorld() : nullptr;
	if (!GEngine || !World || !World->IsGameWorld() || World->GetGameInstance() != GameInstance || RegisteredWorld.Get() != World)
	{
		OutReason = LOCTEXT("NoMenuWorld", "No registered main menu world is available for stage travel.");
		return RejectRequest(EKnightFlowState::Gameplay, OutReason);
	}
	FString MapPackage;
	if (!ResolveMapPackage(Stage.GameplayMap, MapPackage))
	{
		OutReason = LOCTEXT("StageMapChanged", "Stage map is no longer available.");
		return RejectRequest(EKnightFlowState::Gameplay, OutReason);
	}
	if (Stage.RaidDefinition.IsNull())
	{
		PendingQuestContext = FKnightQuestRunContext();
		return BeginTravel(EKnightFlowState::Gameplay, MapPackage, World, StageId);
	}
	PendingQuestContext = FKnightQuestRunContext();
	if (Quest)
	{
		PendingQuestContext.QuestId = KnightProgressionIds::Quest(Quest->QuestId);
		PendingQuestContext.StageId = StageId;
		PendingQuestContext.BossId = Quest->BossId;
		PendingQuestContext.Difficulty = Quest->Difficulty;
		if (const UknightSaveSubsystem* const Saves = GameInstance->GetSubsystem<UknightSaveSubsystem>())
		{
			PendingQuestContext.SlotId = Saves->GetActiveSlotId();
		}
	}
	PendingDestination = EKnightFlowState::Gameplay;
	PendingMapPackage = MapPackage;
	PendingSourceWorld = World;
	PendingStageId = StageId;
	PendingRaidDefinitionPath = Stage.RaidDefinition.ToSoftObjectPath();
	SetTravelPending(true);
	PendingLoadHandle = UknightAssetManager::LoadRaidDefinitionAsync(
		Stage.RaidDefinition, FStreamableDelegate::CreateUObject(this, &ThisClass::HandleRaidDefinitionLoaded));
	if (!PendingLoadHandle)
	{
		ResetPendingTravel();
		OutReason = LOCTEXT("RaidLoadCouldNotStart", "Could not start loading the raid definition.");
		return RejectRequest(EKnightFlowState::Gameplay, OutReason);
	}
	OutReason = FText::GetEmpty();
	return true;
}

void UKnightGameFlowSubsystem::HandleRaidDefinitionLoaded()
{
	if (!bTravelPending || PendingDestination != EKnightFlowState::Gameplay) return;
	const FSoftObjectPath DefinitionPath = PendingRaidDefinitionPath;
	UWorld* const Source = PendingSourceWorld.Get();
	const FString MapPackage = PendingMapPackage;
	const FName StageId = PendingStageId;
	PendingLoadHandle.Reset();
	PendingRaidDefinitionPath.Reset();
	bool bInjectedFailure = false;
#if WITH_EDITOR
	bInjectedFailure = bDebugFailNextRaidLoad;
	bDebugFailNextRaidLoad = false;
#endif
	if (bInjectedFailure || !Cast<UknightRaidDefinition>(DefinitionPath.ResolveObject()) || !Source
		|| Source != GetGameInstance()->GetWorld())
	{
		ResetPendingTravel();
		OnTravelFailed.Broadcast(EKnightFlowState::Gameplay, LOCTEXT("RaidLoadFailed", "The raid definition could not be loaded."));
		return;
	}
	BeginTravel(EKnightFlowState::Gameplay, MapPackage, Source, StageId);
}

bool UKnightGameFlowSubsystem::RequestOpenMenu(EKnightFlowState Destination)
{
	if (bTravelPending)
	{
		return RejectRequest(Destination, LOCTEXT("MenuTravelAlreadyPending", "A menu level transition is already in progress."));
	}
	if (CurrentFlowState == Destination)
	{
		return RejectRequest(Destination, LOCTEXT("AlreadyAtDestination", "This menu level is already active."));
	}

	UGameInstance* const GameInstance = GetGameInstance();
	UWorld* const World = GameInstance ? GameInstance->GetWorld() : nullptr;
	if (!GEngine || !World || !World->IsGameWorld() || World->GetGameInstance() != GameInstance)
	{
		return RejectRequest(Destination, LOCTEXT("NoGameWorld", "No active game world is available for menu travel."));
	}

	const UKnightFlowSettings* const Settings = GetDefault<UKnightFlowSettings>();
	const TSoftObjectPtr<UWorld>& TargetMap = Destination == EKnightFlowState::Title ? Settings->TitleMap : Settings->MainMenuMap;
	if (TargetMap.IsNull())
	{
		return RejectRequest(Destination, LOCTEXT("MapNotConfigured", "The destination menu map is not configured."));
	}

	// A valid soft map reference need not already be loaded into memory.
	FString PackageName;
	if (!ResolveMapPackage(TargetMap, PackageName))
	{
		return RejectRequest(Destination, FText::Format(
			LOCTEXT("InvalidMap", "The destination menu map is invalid or missing: {0}"),
			FText::FromString(TargetMap.ToString())));
	}
	return BeginTravel(Destination, PackageName, World);
}

bool UKnightGameFlowSubsystem::BeginTravel(EKnightFlowState Destination, const FString& MapPackage, UWorld* SourceWorld, FName StageId)
{
	PendingDestination = Destination;
	PendingMapPackage = MapPackage;
	PendingSourceWorld = SourceWorld;
	PendingStageId = StageId;
	if (Destination == EKnightFlowState::Gameplay)
	{
		if (UknightRaidSubsystem* const Session = GetGameInstance()->GetSubsystem<UknightRaidSubsystem>())
		{
			PendingRunId = PendingQuestContext.QuestId.IsNone()
				? Session->BeginRun(StageId)
				: Session->BeginRun(StageId, PendingQuestContext.QuestId, PendingQuestContext.BossId,
					PendingQuestContext.Difficulty, PendingQuestContext.SlotId);
		}
	}
	SetTravelPending(true);
	UGameplayStatics::OpenLevel(SourceWorld, FName(*MapPackage));
	return true;
}

bool UKnightGameFlowSubsystem::RejectRequest(EKnightFlowState Destination, const FText& Reason)
{
	UE_LOG(Logknight, Warning, TEXT("Menu travel to %d rejected: %s"), static_cast<int32>(Destination), *Reason.ToString());
	OnTravelRejected.Broadcast(Destination, Reason);
	return false;
}

void UKnightGameFlowSubsystem::RegisterEnteredMenu(EKnightFlowState MenuState, UWorld* EnteredWorld)
{
	UGameInstance* const GameInstance = GetGameInstance();
	if (!GameInstance || !EnteredWorld || !EnteredWorld->IsGameWorld()
		|| EnteredWorld->GetGameInstance() != GameInstance
		|| GameInstance->GetWorld() != EnteredWorld)
	{
		return;
	}
	if (MenuState != EKnightFlowState::Title && MenuState != EKnightFlowState::MainMenu)
	{
		UE_LOG(Logknight, Error, TEXT("A menu GameMode attempted to register invalid flow state %d."), static_cast<int32>(MenuState));
		return;
	}

	const bool bWasPending = bTravelPending;
	const EKnightFlowState RequestedState = PendingDestination;
	const bool bMatchesRequest = !bWasPending
		|| (MenuState == PendingDestination
			&& UWorld::RemovePIEPrefix(EnteredWorld->GetOutermost()->GetName()) == PendingMapPackage);

	RegisteredWorld = EnteredWorld;
	ActiveStageId = NAME_None;
	ConfirmEnteredWorld(MenuState);
	if (bWasPending && !bMatchesRequest)
	{
		const FText Reason = LOCTEXT("UnexpectedMenu", "The loaded level did not register the requested menu type or map.");
		UE_LOG(Logknight, Error, TEXT("Menu travel entered an unexpected menu level."));
		OnTravelFailed.Broadcast(RequestedState, Reason);
	}
}

void UKnightGameFlowSubsystem::RegisterEnteredGameplay(UWorld* EnteredWorld)
{
	UGameInstance* const GameInstance = GetGameInstance();
	if (!GameInstance || !EnteredWorld || !EnteredWorld->IsGameWorld()
		|| EnteredWorld->GetGameInstance() != GameInstance
		|| GameInstance->GetWorld() != EnteredWorld)
	{
		return;
	}
	const bool bWasPending = bTravelPending;
	const EKnightFlowState RequestedState = PendingDestination;
	if (bWasPending && (RequestedState != EKnightFlowState::Gameplay || PendingStageId.IsNone()
		|| UWorld::RemovePIEPrefix(EnteredWorld->GetOutermost()->GetName()) != PendingMapPackage))
	{
		ResetPendingTravel();
		RegisteredWorld.Reset();
		ActiveStageId = NAME_None;
		SetCurrentFlowState(EKnightFlowState::Uninitialized);
		const FText Reason = LOCTEXT("UnexpectedGameplay", "The loaded Gameplay level did not match the requested stage and map.");
		UE_LOG(Logknight, Error, TEXT("Gameplay registration did not match pending stage travel in %s."), *EnteredWorld->GetPathName());
		OnTravelFailed.Broadcast(RequestedState, Reason);
		return;
	}
	RegisteredWorld = EnteredWorld;
	ActiveStageId = bWasPending ? PendingStageId : NAME_None;
	if (!bWasPending)
	{
		// Direct PIE/OpenLevel must not inherit a selected quest or another world's run.
		if (UknightRaidSubsystem* const Session = GameInstance->GetSubsystem<UknightRaidSubsystem>())
		{
			Session->EndRun();
		}
		const FString MapPackage = UWorld::RemovePIEPrefix(EnteredWorld->GetOutermost()->GetName());
		if (const UKnightStageCatalog* const Catalog = GetStageCatalog())
		{
			for (const FKnightStageDefinition& Stage : Catalog->Stages)
			{
				if (Stage.StageId.IsNone() || Stage.GameplayMap.ToSoftObjectPath().GetLongPackageName() != MapPackage) continue;
				if (!ActiveStageId.IsNone())
				{
					// Shared test maps are ambiguous; keep their GameMode's existing default-definition fallback.
					ActiveStageId = NAME_None;
					break;
				}
				ActiveStageId = KnightProgressionIds::Stage(Stage.StageId);
			}
		}
	}
	ConfirmEnteredWorld(EKnightFlowState::Gameplay);
	if (bWasPending)
	{
		FKnightQuestRunContext Context;
		const UknightRaidSubsystem* const Session = GameInstance->GetSubsystem<UknightRaidSubsystem>();
		if (Session && Session->GetActiveQuestRunContext(Context))
		{
			UE_LOG(Logknight, Log, TEXT("Quest run entered gameplay quest=%s stage=%s boss=%s difficulty=%d run=%s world=%s"),
				*Context.QuestId.ToString(), *Context.StageId.ToString(), *Context.BossId.ToString(),
				static_cast<int32>(Context.Difficulty), *Context.RunId.ToString(), *EnteredWorld->GetPathName());
		}
	}
}

void UKnightGameFlowSubsystem::HandleTravelFailure(UWorld* FailedWorld, ETravelFailure::Type FailureType, const FString& ErrorString)
{
	if (!bTravelPending || !FailedWorld || FailedWorld->GetGameInstance() != GetGameInstance())
	{
		return;
	}
	if (FailedWorld != PendingSourceWorld.Get() && FailedWorld != GetGameInstance()->GetWorld())
	{
		return;
	}

	const EKnightFlowState RequestedState = PendingDestination;
	ResetPendingTravel();
	if (RegisteredWorld.Get() != GetGameInstance()->GetWorld())
	{
		RegisteredWorld.Reset();
		ActiveStageId = NAME_None;
		SetCurrentFlowState(EKnightFlowState::Uninitialized);
	}
	const FText Reason = ErrorString.IsEmpty()
		? LOCTEXT("EngineTravelFailure", "The engine could not open the destination menu level.")
		: FText::FromString(ErrorString);
	UE_LOG(Logknight, Error, TEXT("Menu travel failed (%d): %s"), static_cast<int32>(FailureType), *Reason.ToString());
	OnTravelFailed.Broadcast(RequestedState, Reason);
}

void UKnightGameFlowSubsystem::HandlePostLoadMap(UWorld* LoadedWorld)
{
	if (!LoadedWorld || !LoadedWorld->IsGameWorld()
		|| LoadedWorld->GetGameInstance() != GetGameInstance()
		|| GetGameInstance()->GetWorld() != LoadedWorld
		|| RegisteredWorld.Get() == LoadedWorld)
	{
		return;
	}

	// InitGame has already run. A level without our menu GameMode cannot confirm a menu request.
	const bool bWasPending = bTravelPending;
	const EKnightFlowState RequestedState = PendingDestination;
	ResetPendingTravel();
	if (UknightRaidSubsystem* const Session = GetGameInstance()->GetSubsystem<UknightRaidSubsystem>())
	{
		Session->EndRun();
	}
	RegisteredWorld.Reset();
	ActiveStageId = NAME_None;
	SetCurrentFlowState(EKnightFlowState::Uninitialized);
	if (bWasPending)
	{
		const FText Reason = LOCTEXT("MissingMenuMode", "The loaded level did not initialize a Knight menu GameMode.");
		UE_LOG(Logknight, Error, TEXT("Menu travel loaded a level without a valid Knight menu GameMode."));
		OnTravelFailed.Broadcast(RequestedState, Reason);
	}
}

void UKnightGameFlowSubsystem::SetCurrentFlowState(EKnightFlowState NewState)
{
	if (CurrentFlowState != NewState)
	{
		const EKnightFlowState PreviousState = CurrentFlowState;
		CurrentFlowState = NewState;
		OnFlowStateChanged.Broadcast(PreviousState, NewState);
	}
}

void UKnightGameFlowSubsystem::SetTravelPending(bool bNewPending)
{
	if (bTravelPending != bNewPending)
	{
		bTravelPending = bNewPending;
		OnTravelPendingChanged.Broadcast(bTravelPending);
	}
}

void UKnightGameFlowSubsystem::ConfirmEnteredWorld(EKnightFlowState NewState)
{
	// Publish an internally complete arrival before either delegate runs.
	const EKnightFlowState PreviousState = CurrentFlowState;
	if (NewState != EKnightFlowState::Gameplay)
	{
		if (UknightRaidSubsystem* const Session = GetGameInstance()->GetSubsystem<UknightRaidSubsystem>())
		{
			Session->EndRun();
		}
	}
	const bool bWasPending = bTravelPending;
	PendingDestination = EKnightFlowState::Uninitialized;
	PendingMapPackage.Reset();
	PendingSourceWorld.Reset();
	PendingStageId = NAME_None;
	PendingRaidDefinitionPath.Reset();
	PendingLoadHandle.Reset();
	PendingRunId.Invalidate();
	PendingQuestContext = FKnightQuestRunContext();
	bTravelPending = false;
	CurrentFlowState = NewState;
	if (bWasPending)
	{
		OnTravelPendingChanged.Broadcast(false);
	}
	if (PreviousState != NewState)
	{
		OnFlowStateChanged.Broadcast(PreviousState, NewState);
	}
}

void UKnightGameFlowSubsystem::ResetPendingTravel()
{
	if (PendingLoadHandle) PendingLoadHandle->CancelHandle();
	PendingLoadHandle.Reset();
	PendingRaidDefinitionPath.Reset();
	if (PendingRunId.IsValid())
	{
		if (UknightRaidSubsystem* const Session = GetGameInstance()->GetSubsystem<UknightRaidSubsystem>())
		{
			Session->CancelRun(PendingRunId);
		}
		PendingRunId.Invalidate();
	}
	PendingDestination = EKnightFlowState::Uninitialized;
	PendingMapPackage.Reset();
	PendingSourceWorld.Reset();
	PendingStageId = NAME_None;
	PendingQuestContext = FKnightQuestRunContext();
	SetTravelPending(false);
}

#undef LOCTEXT_NAMESPACE
