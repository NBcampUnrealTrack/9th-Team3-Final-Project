#include "Progression/knightSaveSubsystem.h"

#include "Kismet/GameplayStatics.h"
#include "Progression/knightSaveGame.h"
#include "knight.h"

const FString UknightSaveSubsystem::SlotName = TEXT("KnightProgress");

void UknightSaveSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	LoadOrCreate();
}

void UknightSaveSubsystem::LoadOrCreate()
{
	SaveGame = nullptr;
	if (UGameplayStatics::DoesSaveGameExist(SlotName, UserIndex))
	{
		SaveGame = Cast<UknightSaveGame>(UGameplayStatics::LoadGameFromSlot(SlotName, UserIndex));
		if (!SaveGame)
		{
			UE_LOG(Logknight, Error, TEXT("Save slot %s exists but could not be read; starting a fresh save. The unreadable file is overwritten by the next save."), *SlotName);
		}
	}
	if (SaveGame)
	{
		MigrateSave(*SaveGame);
	}
	else
	{
		SaveGame = Cast<UknightSaveGame>(UGameplayStatics::CreateSaveGameObject(UknightSaveGame::StaticClass()));
	}
}

void UknightSaveSubsystem::MigrateSave(UknightSaveGame& Save) const
{
	if (Save.SaveVersion > UknightSaveGame::CurrentVersion)
	{
		UE_LOG(Logknight, Warning, TEXT("Save version %d is newer than this build (%d)."), Save.SaveVersion, UknightSaveGame::CurrentVersion);
		return;
	}
	// Add one block per version bump, oldest first:
	// if (Save.SaveVersion < 2) { ...convert...; Save.SaveVersion = 2; }
	Save.SaveVersion = UknightSaveGame::CurrentVersion;
}

void UknightSaveSubsystem::RequestSave()
{
	if (!SaveGame)
	{
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
	UGameplayStatics::AsyncSaveGameToSlot(SaveGame, SlotName, UserIndex,
		FAsyncSaveGameToSlotDelegate::CreateUObject(this, &ThisClass::HandleAsyncSaveFinished));
}

void UknightSaveSubsystem::HandleAsyncSaveFinished(const FString& Slot, int32 User, bool bSuccess)
{
	bSaveInFlight = false;
	if (!bSuccess)
	{
		UE_LOG(Logknight, Error, TEXT("Saving slot %s failed."), *Slot);
	}
	OnSaveFinished.Broadcast(bSuccess);
	if (bSavePending)
	{
		StartAsyncSave();
	}
}

void UknightSaveSubsystem::ResetSave()
{
	UGameplayStatics::DeleteGameInSlot(SlotName, UserIndex);
	SaveGame = Cast<UknightSaveGame>(UGameplayStatics::CreateSaveGameObject(UknightSaveGame::StaticClass()));
	RequestSave();
}
