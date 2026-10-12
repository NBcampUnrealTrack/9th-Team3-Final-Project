// Editor-only smoke hooks for an isolated -KnightSaveTestNamespace. Never accept a production save.
#include "AbilitySystem/Attributes/knightCoreAttributeSet.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Item/knightEquipmentData.h"
#include "Item/knightEquipmentStats.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Progression/knightProgressionSubsystem.h"
#include "knightDev.h"

#if WITH_EDITOR
namespace
{
	void MaintenanceTest(const TArray<FString>& Args, UWorld* World)
	{
		FString Namespace;
		if (!FParse::Value(FCommandLine::Get(), TEXT("KnightSaveTestNamespace="), Namespace) || Namespace.IsEmpty())
		{
			UE_LOG(LogknightDev, Error, TEXT("knight.MaintenanceTest requires -KnightSaveTestNamespace."));
			return;
		}
		UknightProgressionSubsystem* Progression = World && World->GetGameInstance()
			? World->GetGameInstance()->GetSubsystem<UknightProgressionSubsystem>() : nullptr;
		if (!Progression || Args.IsEmpty()) return;
		if (Args[0] == TEXT("Grant") && Args.Num() >= 3)
		{
			const UknightItemDefinition* Item = LoadObject<UknightItemDefinition>(nullptr, *Args[1]);
			const int32 Count = FCString::Atoi(*Args[2]);
			const int32 Before = Progression->GetItemCount(Item);
			Progression->AddItem(Item, Count);
			UE_LOG(LogknightDev, Log, TEXT("MaintenanceTest Grant %s %d -> %d"), *Args[1], Before, Progression->GetItemCount(Item));
			return;
		}
		if (Args[0] == TEXT("Snapshot"))
		{
			const UknightMaintenanceCatalog* Catalog = UknightEquipmentSettings::LoadCatalog();
			const auto Loadout = knightEquipmentStats::ReadLoadout(Progression, Catalog);
			const FknightEquipmentStats Stats = knightEquipmentStats::Calculate(Loadout, Catalog, Progression);
			UE_LOG(LogknightDev, Log, TEXT("MaintenanceTest Snapshot complete=%d HP=%.1f ATK=%.1f DEF=%.1f sets=%d"),
				Stats.bComplete, Stats.Values.FindRef(UknightCoreAttributeSet::GetMaxHealthAttribute()),
				Stats.Values.FindRef(UknightCoreAttributeSet::GetAttackPowerAttribute()),
				Stats.Values.FindRef(UknightCoreAttributeSet::GetDefenseAttribute()), Stats.SetCounts.Num());
			for (const auto& Entry : Stats.SetCounts)
				UE_LOG(LogknightDev, Log, TEXT("MaintenanceTest Set %s count=%d"), *Entry.Key.ToString(), Entry.Value);
			return;
		}
		if (Args[0] == TEXT("PureCases"))
		{
			const UknightMaintenanceCatalog* Catalog = UknightEquipmentSettings::LoadCatalog();
			const TCHAR* Names[] = { TEXT("Head"), TEXT("Torso"), TEXT("Legs"), TEXT("Arms"), TEXT("Weapon") };
			knightEquipmentStats::FLoadout Loadout;
			TArray<const UknightPartItemDefinition*> Parts;
			for (const TCHAR* Name : Names)
			{
				const FString Path = TEXT("/Game/Knight/Items/Dev/DA_Weapon_TestSet.DA_Weapon_TestSet");
				const FString PartPath = FCString::Strcmp(Name, TEXT("Weapon")) == 0 ? Path
					: FString::Printf(TEXT("/Game/Knight/Items/Parts/Test/DA_Part_Test_%s.DA_Part_Test_%s"), Name, Name);
				Parts.Add(LoadObject<UknightPartItemDefinition>(nullptr, *PartPath));
			}
			for (int32 Index = 0; Index < Parts.Num(); ++Index)
			{
				if (!Parts[Index]) return;
				Loadout.Add(Parts[Index]->SlotTag, Parts[Index]);
				const FknightEquipmentStats Stats = knightEquipmentStats::Calculate(Loadout, Catalog, nullptr);
				UE_LOG(LogknightDev, Log, TEXT("MaintenanceTest PureCases count=%d set=%d HP=%.1f ATK=%.1f DEF=%.1f"),
					Index + 1, Stats.SetCounts.FindRef(Parts[Index]->SetTag),
					Stats.Values.FindRef(UknightCoreAttributeSet::GetMaxHealthAttribute()),
					Stats.Values.FindRef(UknightCoreAttributeSet::GetAttackPowerAttribute()),
					Stats.Values.FindRef(UknightCoreAttributeSet::GetDefenseAttribute()));
			}
			Loadout.Remove(Parts[1]->SlotTag);
			Loadout.Remove(Parts[2]->SlotTag);
			const FknightEquipmentStats Three = knightEquipmentStats::Calculate(Loadout, Catalog, nullptr);
			UE_LOG(LogknightDev, Log, TEXT("MaintenanceTest PureCases count=3 set=%d ATK=%.1f DEF=%.1f"),
				Three.SetCounts.FindRef(Parts[0]->SetTag),
				Three.Values.FindRef(UknightCoreAttributeSet::GetAttackPowerAttribute()),
				Three.Values.FindRef(UknightCoreAttributeSet::GetDefenseAttribute()));
		}
	}

	FAutoConsoleCommandWithWorldAndArgs MaintenanceTestCommand(
		TEXT("knight.MaintenanceTest"), TEXT("Grant <asset> <count> | Snapshot | PureCases; requires editor test save namespace."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&MaintenanceTest), ECVF_Cheat);
}
#endif
