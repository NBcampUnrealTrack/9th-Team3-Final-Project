#include "Progression/knightSaveSubsystem.h"

#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "Progression/knightSaveGame.h"
#include "knight.h"

const FString UknightSaveSubsystem::SlotName = TEXT("KnightProgress");

void UknightSaveSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	LoadSlot(ActiveSlotName);
}

bool UknightSaveSubsystem::LoadSlot(const FString& Slot)
{
	UknightSaveGame* Loaded = nullptr;
	if (UGameplayStatics::DoesSaveGameExist(Slot, UserIndex))
	{
		TArray<uint8> RawSave;
		const bool bReadable = UGameplayStatics::LoadDataFromSlot(RawSave, Slot, UserIndex);
		// UE's legacy/no-header path tries to deserialize arbitrary bytes and can assert on an invalid FName.
		// This project only writes the versioned GVAS format used by UE 5.8.
		const bool bValidHeader = bReadable && RawSave.Num() >= 64 && RawSave.Num() <= 32 * 1024 * 1024
			&& RawSave[0] == 'G' && RawSave[1] == 'V' && RawSave[2] == 'A' && RawSave[3] == 'S';
		if (bValidHeader)
		{
			Loaded = Cast<UknightSaveGame>(UGameplayStatics::LoadGameFromMemory(RawSave));
		}
		if (!Loaded || !MigrateSave(*Loaded))
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
	if (NewSlot == ActiveSlotName) return !bLoadFailed && SaveGame != nullptr;
	ActiveSlotId = SlotId;
	ActiveSlotName = NewSlot;
	bSavePending = false;
	return LoadSlot(NewSlot);
}

void UknightSaveSubsystem::RequestSave()
{
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
	InFlightSlotName = ActiveSlotName;
	SaveSnapshot = DuplicateObject<UknightSaveGame>(SaveGame, this);
	if (!SaveSnapshot)
	{
		bSaveInFlight = false;
		InFlightSlotName.Reset();
		OnSaveFinished.Broadcast(false);
		return;
	}
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
	bSaveInFlight = false;
	SaveSnapshot = nullptr;
	InFlightSlotName.Reset();
	if (!bSuccess)
	{
		UE_LOG(Logknight, Error, TEXT("Saving slot %s failed."), *CompletedSlot);
	}
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
	if (bSaveInFlight || (UGameplayStatics::DoesSaveGameExist(ActiveSlotName, UserIndex)
		&& !UGameplayStatics::DeleteGameInSlot(ActiveSlotName, UserIndex)))
	{
		OnSaveFinished.Broadcast(false);
		return;
	}
	bLoadFailed = false;
	SaveGame = Cast<UknightSaveGame>(UGameplayStatics::CreateSaveGameObject(UknightSaveGame::StaticClass()));
	RequestSave();
}
