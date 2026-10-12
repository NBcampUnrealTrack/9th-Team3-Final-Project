// Editor-only smoke hooks for an isolated -KnightSaveTestNamespace. Never accept a production save.
#include "AbilitySystem/Attributes/knightCoreAttributeSet.h"
#include "AbilitySystem/Attributes/knightAttributeSet.h"
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
		if (Args[0] == TEXT("AssetCases"))
		{
			const UknightMaintenanceCatalog* Catalog = UknightEquipmentSettings::LoadCatalog();
			if (!Catalog) { UE_LOG(LogknightDev, Error, TEXT("MaintenanceTest AssetCases: catalog missing")); return; }
			int32 Failures = 0;
			auto Check = [&Failures](bool bPassed, const FString& Case)
			{
				if (!bPassed) ++Failures;
				UE_LOG(LogknightDev, Log, TEXT("MaintenanceTest AssetCases %s: %s"), bPassed ? TEXT("PASS") : TEXT("FAIL"), *Case);
			};
			auto LoadPart = [](const TCHAR* SetName, const TCHAR* PartName)
			{
				const FString AssetName = FString::Printf(TEXT("DA_Equipment_%s_%s"), SetName, PartName);
				const FString Path = FString::Printf(TEXT("/Game/Knight/Data/Equipment/%s/%s.%s"), SetName, *AssetName, *AssetName);
				return LoadObject<UknightPartItemDefinition>(nullptr, *Path);
			};
			const FknightEquipmentStats Empty = knightEquipmentStats::Calculate({}, Catalog, nullptr);
			const float BaseDefense = Empty.Values.FindRef(UknightCoreAttributeSet::GetDefenseAttribute());
			const float BaseAttack = Empty.Values.FindRef(UknightCoreAttributeSet::GetAttackPowerAttribute());
			for (const TCHAR* SetName : { TEXT("Janyeol"), TEXT("Janhyang") })
			{
				const bool bJanyeol = FCString::Strcmp(SetName, TEXT("Janyeol")) == 0;
				const TCHAR* Names[] = { TEXT("HeadA"), bJanyeol ? TEXT("BodyA") : TEXT("Body"), TEXT("Arms"), TEXT("Legs"), TEXT("Weapon") };
				knightEquipmentStats::FLoadout Loadout;
				for (int32 Index = 0; Index < 5; ++Index)
				{
					const UknightPartItemDefinition* Part = LoadPart(SetName, Names[Index]);
					Check(Part != nullptr, FString::Printf(TEXT("%s %s exists"), SetName, Names[Index]));
					if (!Part) continue;
					Loadout.Add(Part->SlotTag, Part);
					const FknightEquipmentStats Stats = knightEquipmentStats::Calculate(Loadout, Catalog, nullptr);
					const int32 Count = Stats.SetCounts.FindRef(Part->SetTag);
					const float Defense = Stats.Values.FindRef(UknightCoreAttributeSet::GetDefenseAttribute()) - BaseDefense;
					const float Attack = Stats.Values.FindRef(UknightCoreAttributeSet::GetAttackPowerAttribute()) - BaseAttack;
					const float ExpectedDefense = bJanyeol ? 5.f * (Index + 1 - (Index == 4 ? 1 : 0)) + (Index >= 3 ? 30.f : Index >= 1 ? 10.f : 0.f)
						: 5.f * (Index + 1 - (Index == 4 ? 1 : 0));
					const float ExpectedAttack = bJanyeol ? (Index == 4 ? 10.f : 0.f)
						: (Index >= 3 ? 30.f : Index >= 1 ? 10.f : 0.f) + (Index == 4 ? 10.f : 0.f);
					Check(Count == Index + 1 && FMath::IsNearlyEqual(Defense, ExpectedDefense)
						&& FMath::IsNearlyEqual(Attack, ExpectedAttack),
						FString::Printf(TEXT("%s count=%d DEF=%.1f/%.1f ATK=%.1f/%.1f"), SetName, Count,
							Defense, ExpectedDefense, Attack, ExpectedAttack));
				}
				const UknightPartItemDefinition* HeadB = LoadPart(SetName, TEXT("HeadB"));
				Check(HeadB != nullptr, FString::Printf(TEXT("%s HeadB exists"), SetName));
				if (HeadB)
				{
					const FknightEquipmentStats Before = knightEquipmentStats::Calculate(Loadout, Catalog, nullptr);
					const UknightPartItemDefinition* Previous = Loadout.FindRef(HeadB->SlotTag);
					auto Preview = Loadout;
					Preview.Add(HeadB->SlotTag, HeadB);
					const FknightEquipmentStats After = knightEquipmentStats::Calculate(Preview, Catalog, nullptr);
					Check(After.SetCounts.FindRef(HeadB->SetTag) == 5 && Loadout.FindRef(HeadB->SlotTag) == Previous
						&& FMath::IsNearlyEqual(After.Values.FindRef(UknightCoreAttributeSet::GetDefenseAttribute())
							- Before.Values.FindRef(UknightCoreAttributeSet::GetDefenseAttribute()), 1.f),
						FString::Printf(TEXT("%s HeadA/B preview replaces one slot without changing loadout"), SetName));
				}
				if (bJanyeol)
				{
					const UknightPartItemDefinition* BodyB = LoadPart(SetName, TEXT("BodyB"));
					Check(BodyB != nullptr, TEXT("Janyeol BodyB exists"));
					if (BodyB)
					{
						const FknightEquipmentStats Before = knightEquipmentStats::Calculate(Loadout, Catalog, nullptr);
						const UknightPartItemDefinition* Previous = Loadout.FindRef(BodyB->SlotTag);
						auto Preview = Loadout;
						Preview.Add(BodyB->SlotTag, BodyB);
						const FknightEquipmentStats After = knightEquipmentStats::Calculate(Preview, Catalog, nullptr);
						Check(After.SetCounts.FindRef(BodyB->SetTag) == 5 && Loadout.FindRef(BodyB->SlotTag) == Previous
							&& FMath::IsNearlyEqual(After.Values.FindRef(UknightCoreAttributeSet::GetDefenseAttribute())
								- Before.Values.FindRef(UknightCoreAttributeSet::GetDefenseAttribute()), 1.f),
							TEXT("Janyeol BodyA/B preview replaces one slot without changing loadout"));
					}
				}
			const UknightPartItemDefinition* Weapon = LoadPart(SetName, TEXT("Weapon"));
			const UknightPartItemDefinition* Legs = LoadPart(SetName, TEXT("Legs"));
			const UknightPartItemDefinition* Arms = LoadPart(SetName, TEXT("Arms"));
			const UknightPartItemDefinition* Body = LoadPart(SetName, bJanyeol ? TEXT("BodyA") : TEXT("Body"));
			if (Weapon && Legs && Arms && Body)
			{
				Loadout.Remove(Weapon->SlotTag);
				const FknightEquipmentStats Four = knightEquipmentStats::Calculate(Loadout, Catalog, nullptr);
				Loadout.Remove(Legs->SlotTag);
				const FknightEquipmentStats Three = knightEquipmentStats::Calculate(Loadout, Catalog, nullptr);
				Loadout.Remove(Arms->SlotTag);
				Loadout.Remove(Body->SlotTag);
				const FknightEquipmentStats One = knightEquipmentStats::Calculate(Loadout, Catalog, nullptr);
				const FGameplayTag SetTag = Body->SetTag;
				const FGameplayAttribute BonusAttribute = bJanyeol
					? UknightCoreAttributeSet::GetDefenseAttribute() : UknightCoreAttributeSet::GetAttackPowerAttribute();
				const float FourBonus = Four.Values.FindRef(BonusAttribute) - (bJanyeol ? BaseDefense + 20.f : BaseAttack);
				const float ThreeBonus = Three.Values.FindRef(BonusAttribute) - (bJanyeol ? BaseDefense + 15.f : BaseAttack);
				const float OneBonus = One.Values.FindRef(BonusAttribute) - (bJanyeol ? BaseDefense + 5.f : BaseAttack);
				Check(Four.SetCounts.FindRef(SetTag) == 4 && Three.SetCounts.FindRef(SetTag) == 3
					&& One.SetCounts.FindRef(SetTag) == 1 && FMath::IsNearlyEqual(FourBonus, 30.f)
					&& FMath::IsNearlyEqual(ThreeBonus, 10.f) && FMath::IsNearlyZero(OneBonus),
					FString::Printf(TEXT("%s unequip removes tier effects: 4=%.1f 3=%.1f 1=%.1f"),
						SetName, FourBonus, ThreeBonus, OneBonus));
			}
			}
			const UknightPartItemDefinition* HeukgwanHead = LoadPart(TEXT("Heukgwan"), TEXT("Helmet"));
			const UknightPartItemDefinition* HeukgwanBody = LoadPart(TEXT("Heukgwan"), TEXT("Body"));
			if (HeukgwanHead && HeukgwanBody)
			{
				knightEquipmentStats::FLoadout Loadout;
				Loadout.Add(HeukgwanHead->SlotTag, HeukgwanHead);
				Loadout.Add(HeukgwanBody->SlotTag, HeukgwanBody);
				const FknightEquipmentStats Stats = knightEquipmentStats::Calculate(Loadout, Catalog, nullptr);
				const float WalkDelta = Stats.Values.FindRef(UknightAttributeSet::GetWalkSpeedAttribute())
					- Empty.Values.FindRef(UknightAttributeSet::GetWalkSpeedAttribute());
				const float RunDelta = Stats.Values.FindRef(UknightAttributeSet::GetRunSpeedAttribute())
					- Empty.Values.FindRef(UknightAttributeSet::GetRunSpeedAttribute());
				// Subtracting the 1.0 speed multiplier loses a few low float bits.
				Check(Stats.SetCounts.FindRef(HeukgwanHead->SetTag) == 2
					&& FMath::IsNearlyEqual(WalkDelta, 0.05f, 1.e-5f)
					&& FMath::IsNearlyEqual(RunDelta, 30.f / 900.f, 1.e-5f),
					FString::Printf(TEXT("Heukgwan two-piece count=%d walk=%.9f/0.050000000 run=%.9f/0.033333333"),
						Stats.SetCounts.FindRef(HeukgwanHead->SetTag), WalkDelta, RunDelta));
			}
			else Check(false, TEXT("Heukgwan parts exist"));
			const UknightPartItemDefinition* MixedHead = LoadPart(TEXT("Janyeol"), TEXT("HeadA"));
			const UknightPartItemDefinition* MixedBody = LoadPart(TEXT("Janhyang"), TEXT("Body"));
			if (MixedHead && MixedBody)
			{
				knightEquipmentStats::FLoadout Mixed;
				Mixed.Add(MixedHead->SlotTag, MixedHead);
				Mixed.Add(MixedBody->SlotTag, MixedBody);
				const FknightEquipmentStats Stats = knightEquipmentStats::Calculate(Mixed, Catalog, nullptr);
				Check(Stats.SetCounts.FindRef(MixedHead->SetTag) == 1 && Stats.SetCounts.FindRef(MixedBody->SetTag) == 1
					&& FMath::IsNearlyEqual(Stats.Values.FindRef(UknightCoreAttributeSet::GetDefenseAttribute()) - BaseDefense, 10.f)
					&& FMath::IsNearlyEqual(Stats.Values.FindRef(UknightCoreAttributeSet::GetAttackPowerAttribute()) - BaseAttack, 0.f),
					TEXT("mixed sets remain separate"));
			}
			else Check(false, TEXT("mixed set parts exist"));
			int32 NewRecipeCount = 0;
			TSet<FPrimaryAssetId> UniqueIds;
			for (const UknightCraftRecipeDefinition* Recipe : Catalog->Recipes)
			{
				if (!Recipe || !Recipe->GetPathName().StartsWith(TEXT("/Game/Knight/Data/Equipment/"))) continue;
				++NewRecipeCount;
				const int32 Before = Progression->GetItemCount(Recipe->Result);
				if (Recipe->Result)
				{
					const FPrimaryAssetId Id = Recipe->Result->GetPrimaryAssetId();
					Check(Id.IsValid() && !UniqueIds.Contains(Id) && Recipe->Result->SlotTag.IsValid()
						&& Recipe->Result->SetTag.IsValid()
						&& Recipe->Result->SlotTag.GetTagName().ToString().StartsWith(TEXT("Equipment.Slot."))
						&& Recipe->Result->SetTag.GetTagName().ToString().StartsWith(TEXT("Equipment.Set."))
						&& Recipe->Result->IsA<UknightWeaponItemDefinition>() ==
							(Recipe->Result->SlotTag.GetTagName() == TEXT("Equipment.Slot.Weapon")),
						FString::Printf(TEXT("unique ID, class, slot and set %s"), *Recipe->Result->GetName()));
					UniqueIds.Add(Id);
				}
				Check(Recipe->Result && Recipe->Costs.IsEmpty() && Recipe->RequiredClearedStageId.IsNone()
					&& Progression->CanCraftPart(Recipe) && Progression->CraftPart(Recipe)
					&& Progression->GetItemCount(Recipe->Result) == Before + 1,
					FString::Printf(TEXT("zero-cost craft %s"), *Recipe->GetName()));
			}
			Check(NewRecipeCount == 15, FString::Printf(TEXT("catalog has %d/15 new recipes"), NewRecipeCount));
			UE_LOG(LogknightDev, Log, TEXT("MaintenanceTest AssetCases result: %s (%d failures)"), Failures ? TEXT("FAIL") : TEXT("PASS"), Failures);
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
		TEXT("knight.MaintenanceTest"), TEXT("Grant <asset> <count> | Snapshot | PureCases | AssetCases; requires editor test save namespace."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&MaintenanceTest), ECVF_Cheat);
}
#endif
