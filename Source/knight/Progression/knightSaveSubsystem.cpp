#include "Progression/knightSaveSubsystem.h"

#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "GameFlow/KnightGameFlowSubsystem.h"
#include "HAL/PlatformTime.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Progression/knightSaveGame.h"
#include "Raid/knightRaidSubsystem.h"
#include "knight.h"

const FString UknightSaveSubsystem::SlotName = TEXT("KnightProgress");

void UknightSaveSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	MakeSlotName(ActiveSlotId, ActiveSlotName);
	LoadSlot(ActiveSlotName);
	LastPlayTimeSample = FPlatformTime::Seconds();
	PlayTimeTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateUObject(this, &ThisClass::TickPlayTime), 0.25f);
}

void UknightSaveSubsystem::Deinitialize()
{
	FTSTicker::GetCoreTicker().RemoveTicker(PlayTimeTickerHandle);
	if (SaveRetryTickerHandle.IsValid()) FTSTicker::GetCoreTicker().RemoveTicker(SaveRetryTickerHandle);
	Super::Deinitialize();
}

FString UknightSaveSubsystem::FixedSlotId(int32 SlotNumber)
{
	switch (SlotNumber)
	{
	case 1: return TEXT("Default");
	case 2: return TEXT("Slot2");
	case 3: return TEXT("Slot3");
	default: return FString();
	}
}

bool UknightSaveSubsystem::SelectFixedSlot(int32 SlotNumber)
{
	return SelectSlot(FixedSlotId(SlotNumber));
}

int32 UknightSaveSubsystem::GetActiveFixedSlotNumber() const
{
	for (int32 Number = 1; Number <= 3; ++Number)
	{
		if (ActiveSlotId == FixedSlotId(Number)) return Number;
	}
	return 0;
}

bool UknightSaveSubsystem::GetFixedSlotInfo(int32 SlotNumber, bool& bExists, bool& bReadable, float& PlayTimeSeconds) const
{
	bExists = false;
	bReadable = false;
	PlayTimeSeconds = 0.f;
	FString Name;
	if (!MakeSlotName(FixedSlotId(SlotNumber), Name)) return false;
	bExists = UGameplayStatics::DoesSaveGameExist(Name, UserIndex);
	if (!bExists)
	{
		bReadable = true;
		return true;
	}
	if (UknightSaveGame* const Loaded = ReadExistingSlot(Name))
	{
		bReadable = true;
		PlayTimeSeconds = Loaded->TotalPlayTime;
	}
	return true;
}

bool UknightSaveSubsystem::CanCountPlayTime() const
{
	if (!bSlotSelected || !SaveGame || bLoadFailed) return false;
	const UWorld* const World = GetWorld();
	const UKnightGameFlowSubsystem* const Flow = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UKnightGameFlowSubsystem>() : nullptr;
	return World && World->IsGameWorld() && !World->IsPaused() && Flow && !Flow->IsTravelPending()
		&& (Flow->GetCurrentFlowState() == EKnightFlowState::MainMenu
			|| Flow->GetCurrentFlowState() == EKnightFlowState::Gameplay);
}

void UknightSaveSubsystem::AccumulatePlayTime()
{
	const double Now = FPlatformTime::Seconds();
	const bool bCounting = CanCountPlayTime();
	if (bCounting && bWasCountingPlayTime && LastPlayTimeSample > 0.0)
	{
		const double Elapsed = FMath::Max(0.0, Now - LastPlayTimeSample);
		SaveGame->TotalPlayTime += static_cast<float>(Elapsed);
		SecondsSincePeriodicSave += Elapsed;
	}
	LastPlayTimeSample = Now;
	bWasCountingPlayTime = bCounting;
}

bool UknightSaveSubsystem::TickPlayTime(float DeltaTime)
{
	AccumulatePlayTime();
	if (SecondsSincePeriodicSave >= 60.0 && !bSaveInFlight && CanCountPlayTime())
	{
		SecondsSincePeriodicSave = 0.0;
		RequestSave();
	}
	return true;
}

UknightSaveGame* UknightSaveSubsystem::ReadExistingSlot(const FString& Slot) const
{
	TArray<uint8> RawSave;
	const bool bReadable = UGameplayStatics::LoadDataFromSlot(RawSave, Slot, UserIndex);
	// UE's legacy/no-header path tries to deserialize arbitrary bytes and can assert on an invalid FName.
	// This project only writes the versioned GVAS format used by UE 5.8.
	const bool bValidHeader = bReadable && RawSave.Num() >= 64 && RawSave.Num() <= 32 * 1024 * 1024
		&& RawSave[0] == 'G' && RawSave[1] == 'V' && RawSave[2] == 'A' && RawSave[3] == 'S';
	UknightSaveGame* const Loaded = bValidHeader
		? Cast<UknightSaveGame>(UGameplayStatics::LoadGameFromMemory(RawSave)) : nullptr;
	return Loaded && MigrateSave(*Loaded) ? Loaded : nullptr;
}

bool UknightSaveSubsystem::LoadSlot(const FString& Slot)
{
	UknightSaveGame* Loaded = nullptr;
	if (UGameplayStatics::DoesSaveGameExist(Slot, UserIndex))
	{
		Loaded = ReadExistingSlot(Slot);
		if (!Loaded)
		{
			UE_LOG(Logknight, Error, TEXT("Save slot %s exists but is unreadable or invalid; writes are blocked."), *Slot);
			bLoadFailed = true;
			SaveGame = nullptr;
			return false;
		}
	}
	if (!Loaded)
	{
		Loaded = Cast<UknightSaveGame>(UGameplayStatics::CreateSaveGameObject(UknightSaveGame::StaticClass()));
	}
	SaveGame = Loaded;
	bLoadFailed = false;
	return SaveGame != nullptr;
}

bool UknightSaveSubsystem::MigrateSave(UknightSaveGame& Save) const
{
	if (Save.SaveVersion < 1 || Save.SaveVersion > UknightSaveGame::CurrentVersion)
	{
		UE_LOG(Logknight, Error, TEXT("Unsupported save version %d (current %d)."), Save.SaveVersion, UknightSaveGame::CurrentVersion);
		return false;
	}
	// One block per version bump, oldest first.
	if (Save.SaveVersion < 2)
	{
		// Version 1 keyed items by bare FName. Accept "Item:Name" strings and plain asset names.
		auto ToItemId = [](FName Name)
		{
			const FPrimaryAssetId Parsed = FPrimaryAssetId::FromString(Name.ToString());
			return Parsed.IsValid() ? Parsed : FPrimaryAssetId(TEXT("Item"), Name);
		};
		for (const TPair<FName, int32>& Entry : Save.Materials)
		{
			Save.ItemCounts.FindOrAdd(ToItemId(Entry.Key)) += Entry.Value;
		}
		for (const TPair<FName, int32>& Entry : Save.UpgradeLevels)
		{
			Save.ItemUpgradeLevels.Add(ToItemId(Entry.Key), Entry.Value);
		}
		Save.Materials.Reset();
		Save.UpgradeLevels.Reset();
		UE_LOG(Logknight, Log, TEXT("Migrated save from version 1 to 2."));
		Save.SaveVersion = 2;
	}
	for (const TPair<FPrimaryAssetId, int32>& Entry : Save.ItemCounts)
	{
		if (!Entry.Key.IsValid() || Entry.Value < 0) return false;
	}
	for (const TPair<FPrimaryAssetId, int32>& Entry : Save.ItemUpgradeLevels)
	{
		if (!Entry.Key.IsValid() || Entry.Value < 0) return false;
	}
	for (const TPair<FPrimaryAssetId, int32>& Entry : Save.WeaponUpgradeFailures)
	{
		if (!Entry.Key.IsValid() || Entry.Value < 0) return false;
	}
	Save.CompletedQuestIds.Remove(NAME_None);
	Save.FirstDefeatedBossIds.Remove(NAME_None);
	if (Save.Money < 0 || Save.UnlockedSharedSocketSlots < 0) return false;
	if (Save.EquippedSocketItems.Num() > Save.UnlockedSharedSocketSlots) return false;
	TMap<FPrimaryAssetId, int32> EquippedSocketCounts;
	for (const FPrimaryAssetId& Id : Save.EquippedSocketItems)
	{
		if (!Id.IsValid()) continue; // Empty slot.
		const int32 Used = ++EquippedSocketCounts.FindOrAdd(Id);
		if (Save.ItemCounts.FindRef(Id) < Used) return false;
	}
	if (!FMath::IsFinite(Save.TotalPlayTime) || Save.TotalPlayTime < 0.f) return false;
	Save.SaveVersion = UknightSaveGame::CurrentVersion;
	return true;
}

bool UknightSaveSubsystem::MakeSlotName(const FString& SlotId, FString& OutSlotName)
{
	OutSlotName.Reset();
	if (SlotId.IsEmpty() || SlotId.Len() > 48) return false;
	for (const TCHAR Character : SlotId)
	{
		if (!FChar::IsAlnum(Character) && Character != TEXT('_') && Character != TEXT('-')) return false;
	}
	OutSlotName = SlotId == TEXT("Default") ? SlotName : TEXT("KnightProgress_") + SlotId;
#if WITH_EDITOR
	FString TestNamespace;
	if (FParse::Value(FCommandLine::Get(), TEXT("KnightSaveTestNamespace="), TestNamespace))
	{
		if (TestNamespace.IsEmpty() || TestNamespace.Len() > 32) return false;
		for (const TCHAR Character : TestNamespace)
		{
			if (!FChar::IsAlnum(Character) && Character != TEXT('_') && Character != TEXT('-')) return false;
		}
		OutSlotName = TEXT("KnightProgress_Test_") + TestNamespace + TEXT("_") + SlotId;
	}
#endif
	return true;
}

bool UknightSaveSubsystem::DoesSlotExist(const FString& SlotId) const
{
	FString Name;
	return MakeSlotName(SlotId, Name) && UGameplayStatics::DoesSaveGameExist(Name, UserIndex);
}

bool UknightSaveSubsystem::SelectSlot(const FString& SlotId)
{
	FString NewSlot;
	if (bSaveInFlight || !MakeSlotName(SlotId, NewSlot)) return false;
	if (bSaveRecoveryPending && NewSlot != ActiveSlotName)
	{
		UE_LOG(Logknight, Error, TEXT("Cannot leave slot %s while its latest state is unsaved; retry RequestSave."), *ActiveSlotName);
		return false;
	}
	// A quest result must be credited to the same profile that accepted the launch.
	if (NewSlot != ActiveSlotName)
	{
		const UknightRaidSubsystem* const Raid = GetGameInstance()
			? GetGameInstance()->GetSubsystem<UknightRaidSubsystem>() : nullptr;
		if (Raid && Raid->GetCurrentRunId().IsValid()) return false;
	}
	if (NewSlot == ActiveSlotName)
	{
		const bool bWasSelected = bSlotSelected;
		if (!bSlotSelected || bLoadFailed) LoadSlot(NewSlot);
		bSlotSelected = !bLoadFailed && SaveGame != nullptr;
		if (bSlotSelected != bWasSelected) OnActiveSlotChanged.Broadcast(ActiveSlotId);
		return bSlotSelected;
	}
	AccumulatePlayTime();
	bWasCountingPlayTime = false;
	bSlotSelected = false;
	ActiveSlotId = SlotId;
	ActiveSlotName = NewSlot;
	bSavePending = false;
	bSlotSelected = LoadSlot(NewSlot);
	SecondsSincePeriodicSave = 0.0;
	OnActiveSlotChanged.Broadcast(ActiveSlotId);
	return bSlotSelected;
}

void UknightSaveSubsystem::RequestSave()
{
	AccumulatePlayTime();
	if (!SaveGame || bLoadFailed)
	{
		OnSaveFinished.Broadcast(false);
		return;
	}
	if (bSaveInFlight)
	{
		bSavePending = true;
		return;
	}
	StartAsyncSave();
}

void UknightSaveSubsystem::StartAsyncSave()
{
	bSaveInFlight = true;
	bSavePending = false;
	SaveRetryCount = 0;
	InFlightSlotName = ActiveSlotName;
	SaveSnapshot = DuplicateObject<UknightSaveGame>(SaveGame, this);
	if (!SaveSnapshot)
	{
		bSaveInFlight = false;
		InFlightSlotName.Reset();
		OnSaveFinished.Broadcast(false);
		return;
	}
	LaunchSaveAttempt();
}

void UknightSaveSubsystem::LaunchSaveAttempt()
{
	#if WITH_EDITOR
	if (DebugFailuresRemaining > 0)
	{
		--DebugFailuresRemaining;
		if (UWorld* const World = GetWorld())
		{
			World->GetTimerManager().SetTimer(DebugFailureTimer, this, &ThisClass::HandleDebugSaveFailure, 0.25f, false);
			return;
		}
		HandleAsyncSaveFinished(InFlightSlotName, UserIndex, false);
		return;
	}
	#endif
	UGameplayStatics::AsyncSaveGameToSlot(SaveSnapshot, InFlightSlotName, UserIndex,
		FAsyncSaveGameToSlotDelegate::CreateUObject(this, &ThisClass::HandleAsyncSaveFinished));
}

bool UknightSaveSubsystem::RetryFailedSave(float DeltaTime)
{
	SaveRetryTickerHandle.Reset();
	if (bSaveInFlight && SaveSnapshot && !InFlightSlotName.IsEmpty()) LaunchSaveAttempt();
	return false;
}

#if WITH_EDITOR
void UknightSaveSubsystem::HandleDebugSaveFailure()
{
	HandleAsyncSaveFinished(InFlightSlotName, UserIndex, false);
}
#endif

void UknightSaveSubsystem::HandleAsyncSaveFinished(const FString& Slot, int32 User, bool bSuccess)
{
	const FString CompletedSlot = Slot;
	if (!bSaveInFlight || CompletedSlot != InFlightSlotName || User != UserIndex)
	{
		UE_LOG(Logknight, Error, TEXT("Ignoring stale save completion for slot %s."), *CompletedSlot);
		return;
	}
	if (!bSuccess)
	{
		if (SaveSnapshot && SaveRetryCount < 2)
		{
			++SaveRetryCount;
			UE_LOG(Logknight, Warning, TEXT("Saving slot %s failed; retry %d uses the same snapshot."),
				*CompletedSlot, SaveRetryCount);
			SaveRetryTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
				FTickerDelegate::CreateUObject(this, &ThisClass::RetryFailedSave), 0.5f);
			return;
		}
		UE_LOG(Logknight, Error, TEXT("Saving slot %s failed after retries; the in-memory state remains available for RequestSave."),
			*CompletedSlot);
		bSaveRecoveryPending = true;
	}
	else bSaveRecoveryPending = false;
	bSaveInFlight = false;
	SaveSnapshot = nullptr;
	InFlightSlotName.Reset();
	UE_LOG(Logknight, Log, TEXT("Save completion slot=%s success=%d followUp=%d"), *CompletedSlot, bSuccess, bSavePending);
	if (bSavePending)
	{
		StartAsyncSave();
		return;
	}
	OnSaveFinished.Broadcast(bSuccess);
}

void UknightSaveSubsystem::ResetSave()
{
	if (bSaveInFlight || bSaveRecoveryPending || (UGameplayStatics::DoesSaveGameExist(ActiveSlotName, UserIndex)
		&& !UGameplayStatics::DeleteGameInSlot(ActiveSlotName, UserIndex)))
	{
		OnSaveFinished.Broadcast(false);
		return;
	}
	bLoadFailed = false;
	SaveGame = Cast<UknightSaveGame>(UGameplayStatics::CreateSaveGameObject(UknightSaveGame::StaticClass()));
	OnActiveSlotChanged.Broadcast(ActiveSlotId);
	RequestSave();
}
