// Editor-only smoke hooks for an isolated -KnightSaveTestNamespace. Never accept a production save.
#include "AbilitySystem/Attributes/knightCoreAttributeSet.h"
#include "AbilitySystem/Attributes/knightAttributeSet.h"
#include "Blueprint/WidgetTree.h"
#include "CommonButtonBase.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Components/WidgetSwitcher.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Item/knightEquipmentData.h"
#include "Item/knightEquipmentStats.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Menu/UI/KnightInventoryWidget.h"
#include "Menu/UI/KnightMaintenanceWidget.h"
#include "Progression/knightProgressionSubsystem.h"
#include "Progression/knightSaveGame.h"
#include "Progression/knightSaveSubsystem.h"
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
		if (Args[0] == TEXT("MaterialWidgetCases"))
		{
			int32 Failures = 0;
			auto Check = [&Failures](bool bPassed, const TCHAR* Case)
			{
				if (!bPassed) ++Failures;
				UE_LOG(LogknightDev, Log, TEXT("Phase6 MaterialWidgetCases %s: %s"), bPassed ? TEXT("PASS") : TEXT("FAIL"), Case);
			};
			UknightSaveSubsystem* const Saves = World->GetGameInstance()->GetSubsystem<UknightSaveSubsystem>();
			const UknightMaterialItemDefinition* const Material = LoadObject<UknightMaterialItemDefinition>(nullptr,
				TEXT("/Game/Knight/Items/Materials/Janyeol/DA_Material_Janyeol_Hide.DA_Material_Janyeol_Hide"));
			APlayerController* const Player = UGameplayStatics::GetPlayerController(World, 0);
			if (!Saves || !Material || !Player || !Saves->SelectSlot(TEXT("Phase6Widget")))
			{
				UE_LOG(LogknightDev, Error, TEXT("Phase6 MaterialWidgetCases setup failed."));
				return;
			}
			UknightSaveGame* const Save = Saves->GetSaveGame();
			if (!Save) { UE_LOG(LogknightDev, Error, TEXT("Phase6 MaterialWidgetCases save unavailable.")); return; }
			Save->ItemCounts.Add(Material->GetPrimaryAssetId(), 3);
			Save->Money = 123;
			UKnightMaintenanceWidget* const Widget = CreateWidget<UKnightMaintenanceWidget>(
				Player, UKnightMaintenanceWidget::GetSharedWidgetClass());
			if (!Widget || !Widget->WidgetTree)
			{
				UE_LOG(LogknightDev, Error, TEXT("Phase6 MaterialWidgetCases WBP failed to construct."));
				return;
			}
			Widget->Open(EKnightMaintenanceDestination::Crafting);
			UCommonButtonBase* const MaterialsTab = Cast<UCommonButtonBase>(Widget->WidgetTree->FindWidget(TEXT("Btn_Armor")));
			UCommonButtonBase* const SellButton = Cast<UCommonButtonBase>(Widget->WidgetTree->FindWidget(TEXT("Btn_Execute")));
			UCommonButtonBase* const CancelButton = Cast<UCommonButtonBase>(Widget->WidgetTree->FindWidget(TEXT("Btn_ConfirmCancel")));
			UTextBlock* const DetailName = Cast<UTextBlock>(Widget->WidgetTree->FindWidget(TEXT("Txt_PartName")));
			UScrollBox* const List = Cast<UScrollBox>(Widget->WidgetTree->FindWidget(TEXT("List_Items")));
			UWidgetSwitcher* const Switcher = Cast<UWidgetSwitcher>(Widget->WidgetTree->FindWidget(TEXT("Switcher_Info")));
			UWidget* const MaterialPage = Widget->WidgetTree->FindWidget(TEXT("Phase6_MaterialSalePage"));
			if (!MaterialsTab || !SellButton || !CancelButton || !DetailName || !List || !Switcher || !MaterialPage)
			{
				UE_LOG(LogknightDev, Error, TEXT("Phase6 MaterialWidgetCases required controls missing."));
				return;
			}
			MaterialsTab->OnClicked().Broadcast();
			Check(Switcher->GetActiveWidget() == MaterialPage, TEXT("Materials uses separate detail page"));
			auto FindMaterialRow = [List, Material]() -> UKnightInventoryEntryWidget*
			{
				for (UWidget* const Child : List->GetAllChildren())
					if (UKnightInventoryEntryWidget* const Row = Cast<UKnightInventoryEntryWidget>(Child))
						if (Row->GetItem() == Material) return Row;
				return nullptr;
			};
			UKnightInventoryEntryWidget* Row = FindMaterialRow();
			Check(Row != nullptr, TEXT("owned material appears in Materials list"));
			if (Row) Row->OnSelected.Broadcast(Material);
			Check(DetailName->GetText().EqualTo(Material->DisplayName), TEXT("selection resolves material display name"));
			Progression->OnProgressionChanged.Broadcast();
			Check(DetailName->GetText().EqualTo(Material->DisplayName) && FindMaterialRow() != nullptr,
				TEXT("unrelated refresh preserves selected material ID"));
			SellButton->OnClicked().Broadcast();
			CancelButton->OnClicked().Broadcast();
			Check(Save->ItemCounts.FindRef(Material->GetPrimaryAssetId()) == 3 && Save->Money == 123,
				TEXT("confirmation cancellation does not mutate inventory or currency"));
			SellButton->OnClicked().Broadcast();
			Check(Saves->SelectSlot(TEXT("Phase6WidgetOther")), TEXT("active profile changes while confirmation is open"));
			Check(DetailName->GetText().ToString().Contains(TEXT("선택"))
				&& Progression->GetItemCount(Material) == 0,
				TEXT("profile change clears stale material selection and does not sell"));
			Widget->Close();
			if (Failures)
			{
				UE_LOG(LogknightDev, Error, TEXT("Phase6 MaterialWidgetCases SUMMARY failures=%d"), Failures);
			}
			else
			{
				UE_LOG(LogknightDev, Log, TEXT("Phase6 MaterialWidgetCases SUMMARY failures=0"));
			}
			return;
		}
		if (Args[0] == TEXT("MaterialSaleCases"))
		{
			const bool bFinalStack = Args.Num() > 1 && Args[1] == TEXT("Final");
			UknightSaveSubsystem* const Saves = World->GetGameInstance()->GetSubsystem<UknightSaveSubsystem>();
			const UknightMaterialItemDefinition* const Material = LoadObject<UknightMaterialItemDefinition>(nullptr,
				TEXT("/Game/Knight/Items/Materials/Janyeol/DA_Material_Janyeol_Hide.DA_Material_Janyeol_Hide"));
			const UknightMaterialItemDefinition* const Rare = LoadObject<UknightMaterialItemDefinition>(nullptr,
				TEXT("/Game/Knight/Items/Materials/Janyeol/DA_Material_Janyeol_Essence.DA_Material_Janyeol_Essence"));
			const UknightMaterialItemDefinition* const Unsellable = LoadObject<UknightMaterialItemDefinition>(nullptr,
				TEXT("/Game/Knight/Items/Dev/DA_CoreTestMaterial.DA_CoreTestMaterial"));
			int32 Failures = 0;
			auto Check = [&Failures](bool bPassed, const TCHAR* Case)
			{
				if (!bPassed) ++Failures;
				UE_LOG(LogknightDev, Log, TEXT("Phase6 MaterialSaleCases %s: %s"), bPassed ? TEXT("PASS") : TEXT("FAIL"), Case);
			};
			if (!Saves || !Material || !Rare || !Unsellable || !Material->bSellable || Material->SellPrice <= 0
				|| Unsellable->bSellable || Unsellable->SellPrice > 0)
			{
				UE_LOG(LogknightDev, Error, TEXT("Phase6 MaterialSaleCases setup failed: save or material definitions unavailable."));
				return;
			}
			const FString Slot = bFinalStack ? TEXT("Phase6Final") : TEXT("Phase6Partial");
			Check(Saves->SelectSlot(Slot), TEXT("isolated test profile selected"));
			UknightSaveGame* Save = Saves->GetSaveGame();
			if (!Save) { UE_LOG(LogknightDev, Error, TEXT("Phase6 MaterialSaleCases save unavailable.")); return; }
			const FPrimaryAssetId Id = Material->GetPrimaryAssetId();
			const FPrimaryAssetId RareId = Rare->GetPrimaryAssetId();
			const FPrimaryAssetId UnsellableId = Unsellable->GetPrimaryAssetId();
			const int32 InitialCount = bFinalStack ? 1 : 3;
			const int32 SaleCount = bFinalStack ? 1 : 2;
			Save->ItemCounts.Add(Id, InitialCount);
			Save->ItemCounts.Add(RareId, 2);
			Save->ItemCounts.Add(UnsellableId, 1);
			Save->Money = 123;
			TArray<FknightItemStack> Owned;
			Check(Progression->GetOwnedMaterials(Owned), TEXT("owned material query complete"));
			Check(Owned.Num() == 3, TEXT("positive owned materials include non-sellable item"));
			bool bSorted = true;
			for (int32 Index = 1; Index < Owned.Num(); ++Index)
			{
				if (Owned[Index - 1].Item->Rarity < Owned[Index].Item->Rarity
					|| (Owned[Index - 1].Item->Rarity == Owned[Index].Item->Rarity
						&& Owned[Index - 1].Item->GetPrimaryAssetId().ToString() > Owned[Index].Item->GetPrimaryAssetId().ToString()))
					bSorted = false;
			}
			Check(bSorted, TEXT("rarity descending and stable item-ID order"));
			FKnightMaterialSaleReceipt Quote;
			Check(!Progression->GetMaterialSaleQuote(Id, 0, Quote)
				&& !Progression->GetMaterialSaleQuote(Id, -1, Quote)
				&& !Progression->GetMaterialSaleQuote(Id, InitialCount + 1, Quote),
				TEXT("zero negative and excess quantities rejected"));
			Check(!Progression->GetMaterialSaleQuote(UnsellableId, 1, Quote), TEXT("non-sellable definition rejected"));
			Check(Progression->GetMaterialSaleQuote(Id, SaleCount, Quote)
				&& Quote.UnitPrice == Material->SellPrice
				&& Quote.TotalMoney == static_cast<int64>(Material->SellPrice) * SaleCount,
				TEXT("quote uses asset price and exact total"));
			Check(Save->ItemCounts.FindRef(Id) == InitialCount && Save->Money == 123,
				TEXT("quote and rejected requests leave inventory and money unchanged"));
			Save->Money = MAX_int64;
			Check(!Progression->GetMaterialSaleQuote(Id, 1, Quote), TEXT("currency overflow rejected"));
			Save->Money = 123;
			Check(Saves->SelectSlot(Slot + TEXT("Other")), TEXT("other isolated profile selected"));
			FKnightMaterialSaleReceipt StaleReceipt;
			Check(!Progression->SellMaterial(Id, SaleCount, StaleReceipt)
				&& Progression->GetItemCount(Material) == 0,
				TEXT("old profile material request rejected in new profile"));
			Check(Saves->SelectSlot(Slot), TEXT("original isolated profile reselected"));
			Save = Saves->GetSaveGame();
			if (!Save) { UE_LOG(LogknightDev, Error, TEXT("Phase6 MaterialSaleCases profile reload failed.")); return; }
			Save->ItemCounts.Add(Id, InitialCount);
			Save->ItemCounts.Add(RareId, 2);
			Save->ItemCounts.Add(UnsellableId, 1);
			Save->Money = 123;
			FKnightMaterialSaleReceipt Receipt;
			Check(Progression->SellMaterial(Id, SaleCount, Receipt), TEXT("valid sale accepted"));
			Check(Save->ItemCounts.FindRef(Id) == InitialCount - SaleCount
				&& Save->Money == 123 + static_cast<int64>(Material->SellPrice) * SaleCount,
				TEXT("one transaction changes material and money exactly once"));
			Check(!Progression->SellMaterial(Id, SaleCount, Receipt), TEXT("immediate duplicate request rejected"));
			Owned.Reset();
			Check(Progression->GetOwnedMaterials(Owned), TEXT("post-sale owned query complete"));
			const FknightItemStack* Remaining = Owned.FindByPredicate([Id](const FknightItemStack& Stack)
			{
				return Stack.Item && Stack.Item->GetPrimaryAssetId() == Id;
			});
			Check(bFinalStack ? Remaining == nullptr : Remaining && Remaining->Count == 1,
				bFinalStack ? TEXT("final stack removed from list") : TEXT("partial sale retains item with updated count"));
			if (Failures)
			{
				UE_LOG(LogknightDev, Error, TEXT("Phase6 MaterialSaleCases SUMMARY mode=%s failures=%d"),
					bFinalStack ? TEXT("Final") : TEXT("Partial"), Failures);
			}
			else
			{
				UE_LOG(LogknightDev, Log, TEXT("Phase6 MaterialSaleCases SUMMARY mode=%s failures=0"),
					bFinalStack ? TEXT("Final") : TEXT("Partial"));
			}
			return;
		}
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
					// Janyeol values follow the equipment/stat pass on main: the four-piece adds AttackPower, not Defense.
					constexpr float JanyeolDefense[] = { 5.f, 23.f, 28.f, 34.f, 34.f };
					constexpr float JanyeolAttack[] = { 0.f, 0.f, 10.f, 22.f, 142.f };
					const float ExpectedDefense = bJanyeol ? JanyeolDefense[Index]
						: 5.f * (Index + 1 - (Index == 4 ? 1 : 0));
					const float ExpectedAttack = bJanyeol ? JanyeolAttack[Index]
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
							- Before.Values.FindRef(UknightCoreAttributeSet::GetDefenseAttribute()), bJanyeol ? 2.f : 1.f),
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
								- Before.Values.FindRef(UknightCoreAttributeSet::GetDefenseAttribute()), 3.f),
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
				const FGameplayAttribute BonusAttribute = UknightCoreAttributeSet::GetAttackPowerAttribute();
				const float FourBonus = Four.Values.FindRef(BonusAttribute) - BaseAttack;
				const float ThreeBonus = Three.Values.FindRef(BonusAttribute) - BaseAttack;
				const float OneBonus = One.Values.FindRef(BonusAttribute) - BaseAttack;
				Check(Four.SetCounts.FindRef(SetTag) == 4 && Three.SetCounts.FindRef(SetTag) == 3
				&& One.SetCounts.FindRef(SetTag) == 1
				&& FMath::IsNearlyEqual(FourBonus, bJanyeol ? 22.f : 30.f)
				&& FMath::IsNearlyEqual(ThreeBonus, 10.f) && FMath::IsNearlyZero(OneBonus)
				&& (!bJanyeol || (FMath::IsNearlyEqual(Four.Values.FindRef(UknightCoreAttributeSet::GetDefenseAttribute()) - BaseDefense, 34.f)
					&& FMath::IsNearlyEqual(Three.Values.FindRef(UknightCoreAttributeSet::GetDefenseAttribute()) - BaseDefense, 28.f)
					&& FMath::IsNearlyEqual(One.Values.FindRef(UknightCoreAttributeSet::GetDefenseAttribute()) - BaseDefense, 5.f))),
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
				const bool bValidCosts = Recipe->Costs.Num() == 2
					&& Recipe->Costs[0].Item && Recipe->Costs[1].Item
					&& Recipe->Costs[0].Item != Recipe->Costs[1].Item
					&& Recipe->Costs[0].Count > 0 && Recipe->Costs[1].Count > 0
					&& Recipe->Costs[0].Item->IsA<UknightMaterialItemDefinition>()
					&& Recipe->Costs[1].Item->IsA<UknightMaterialItemDefinition>();
				Check(bValidCosts && Recipe->RequiredClearedStageId.IsNone(),
					FString::Printf(TEXT("two distinct positive material costs %s"), *Recipe->GetName()));
				if (!Recipe->Result || !bValidCosts) continue;
				const int32 Before = Progression->GetItemCount(Recipe->Result);
				if (Before > 0)
				{
					Check(!Progression->CanCraftPart(Recipe) && !Progression->CraftPart(Recipe),
						FString::Printf(TEXT("already owned craft blocked %s"), *Recipe->GetName()));
					continue;
				}
				for (const FknightItemCost& Cost : Recipe->Costs)
				{
					const int32 Missing = FMath::Max(0, Cost.Count - Progression->GetItemCount(Cost.Item));
					if (Missing > 0) Progression->AddItem(Cost.Item, Missing);
				}
				const int32 MaterialA = Progression->GetItemCount(Recipe->Costs[0].Item);
				const int32 MaterialB = Progression->GetItemCount(Recipe->Costs[1].Item);
				Check(Progression->CanCraftPart(Recipe) && Progression->CraftPart(Recipe)
					&& Progression->GetItemCount(Recipe->Result) == Before + 1
					&& Progression->GetItemCount(Recipe->Costs[0].Item) == MaterialA - Recipe->Costs[0].Count
					&& Progression->GetItemCount(Recipe->Costs[1].Item) == MaterialB - Recipe->Costs[1].Count
					&& !Progression->CanCraftPart(Recipe) && !Progression->CraftPart(Recipe),
					FString::Printf(TEXT("craft spends exactly two costs and blocks duplicates %s"), *Recipe->GetName()));
			}
			Check(NewRecipeCount == 21, FString::Printf(TEXT("catalog has %d/21 formal recipes"), NewRecipeCount));
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
		TEXT("knight.MaintenanceTest"), TEXT("Grant <asset> <count> | Snapshot | PureCases | AssetCases | MaterialSaleCases Partial|Final | MaterialWidgetCases; requires editor test save namespace."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&MaintenanceTest), ECVF_Cheat);
}
#endif
