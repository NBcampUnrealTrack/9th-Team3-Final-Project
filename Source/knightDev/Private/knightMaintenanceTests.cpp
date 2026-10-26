// Editor-only smoke hooks for an isolated -KnightSaveTestNamespace. Never accept a production save.
#include "AbilitySystem/Attributes/knightCoreAttributeSet.h"
#include "AbilitySystem/Attributes/knightAttributeSet.h"
#include "Blueprint/WidgetTree.h"
#include "CommonButtonBase.h"
#include "CommonTextBlock.h"
#include "Menu/UI/KnightMaintenanceEntryWidget.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Components/WidgetSwitcher.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "knightCharacter.h"
#include "Item/knightEquipmentData.h"
#include "Item/knightEquipmentStats.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Menu/UI/KnightInventoryWidget.h"
#include "Menu/UI/KnightMainMenuWidget.h"
#include "Menu/UI/KnightMaintenanceWidget.h"
#include "Progression/knightProgressionSubsystem.h"
#include "Progression/knightSaveGame.h"
#include "Progression/knightSaveSubsystem.h"
#include "System/knightGameplayTags.h"
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
		if (Args[0] == TEXT("IntegrationCases"))
		{
			const FString Mode = Args.Num() > 1 ? Args[1] : TEXT("Navigation");
			int32 Failures = 0;
			auto Check = [&Failures, &Mode](bool bPassed, const TCHAR* Case)
			{
				if (!bPassed) ++Failures;
				UE_LOG(LogknightDev, Log, TEXT("Phase9 IntegrationCases %s %s: %s"), *Mode,
					bPassed ? TEXT("PASS") : TEXT("FAIL"), Case);
			};
			UknightSaveSubsystem* const Saves = World->GetGameInstance()->GetSubsystem<UknightSaveSubsystem>();
			const FString Slot = Mode == TEXT("EquipmentUnequip") ? TEXT("Phase9EquipmentEquip")
				: Mode == TEXT("RestoreCraft") ? TEXT("Phase9Craft")
				: Mode == TEXT("RestoreState") ? TEXT("Phase9SaveState")
				: Mode == TEXT("Legacy") ? TEXT("Legacy") : TEXT("Phase9") + Mode;
			if (!Saves || !Saves->SelectSlot(Slot) || !Saves->GetSaveGame())
			{
				UE_LOG(LogknightDev, Error, TEXT("Phase9 IntegrationCases setup failed mode=%s"), *Mode);
				return;
			}
			UknightSaveGame* const Save = Saves->GetSaveGame();
			const UknightMaterialItemDefinition* const Hide = LoadObject<UknightMaterialItemDefinition>(nullptr,
				TEXT("/Game/Knight/Items/Materials/Janyeol/DA_Material_Janyeol_Hide.DA_Material_Janyeol_Hide"));
			const UknightWeaponItemDefinition* const Weapon = LoadObject<UknightWeaponItemDefinition>(nullptr,
				TEXT("/Game/Knight/Data/Equipment/Janyeol/DA_Equipment_Janyeol_Weapon.DA_Equipment_Janyeol_Weapon"));
			const UknightSocketItemDefinition* const Socket = LoadObject<UknightSocketItemDefinition>(nullptr,
				TEXT("/Game/Knight/Items/Sockets/DA_Socket_RepairExpansion.DA_Socket_RepairExpansion"));
			const UknightCraftRecipeDefinition* const Recipe = LoadObject<UknightCraftRecipeDefinition>(nullptr,
				TEXT("/Game/Knight/Data/Equipment/Janyeol/Recipes/DA_Recipe_Equipment_Janyeol_HeadB.DA_Recipe_Equipment_Janyeol_HeadB"));
			if (!Hide || !Weapon || !Socket || !Recipe || !Recipe->Result || Recipe->Costs.Num() != 2)
			{
				UE_LOG(LogknightDev, Error, TEXT("Phase9 IntegrationCases definitions missing mode=%s"), *Mode);
				return;
			}
			if (Mode == TEXT("Navigation"))
			{
				APlayerController* const Player = UGameplayStatics::GetPlayerController(World, 0);
				const TSubclassOf<UKnightMainMenuWidget> MenuClass = LoadClass<UKnightMainMenuWidget>(nullptr,
					TEXT("/Game/Knight/UI/Menu/WBP_KnightMainMenu.WBP_KnightMainMenu_C"));
				UKnightMainMenuWidget* const Menu = Player && MenuClass ? CreateWidget<UKnightMainMenuWidget>(Player, MenuClass) : nullptr;
				Check(Menu && Menu->WidgetTree, TEXT("real main-menu WBP constructed"));
				if (Menu && Menu->WidgetTree)
				{
					Check(Menu->ShowPage(EKnightMainMenuPage::Inventory), TEXT("serialized legacy page redirects to Materials"));
					UWidgetSwitcher* const Switcher = Cast<UWidgetSwitcher>(Menu->WidgetTree->FindWidget(TEXT("Switcher_MaintenanceRuntime")));
					UKnightMaintenanceWidget* const Maintenance = Switcher ? Cast<UKnightMaintenanceWidget>(Switcher->GetActiveWidget()) : nullptr;
					Check(Menu->GetCurrentPage() == EKnightMainMenuPage::Maintenance && Maintenance
						&& Maintenance->IsMaterialsViewOpen(), TEXT("shortcut uses real maintenance Materials view"));
					Check(!Menu->WidgetTree->FindWidget(TEXT("Panel_InventoryRuntime")), TEXT("legacy inventory panel is not constructed"));
					Check(Menu->ToggleMaterialsShortcut() && Menu->GetCurrentPage() == EKnightMainMenuPage::Home,
						TEXT("Materials shortcut returns directly Home"));
					Check(Menu->ToggleMaterialsShortcut() && Switcher && Switcher->GetActiveWidget() == Maintenance
						&& Maintenance->IsMaterialsViewOpen(), TEXT("repeat entry reuses one widget with fresh Materials state"));
					Check(Menu->ShowPage(EKnightMainMenuPage::Home), TEXT("normal page exit works"));
					Check(Menu->ShowPage(EKnightMainMenuPage::Maintenance) && Menu->GetCurrentPage() == EKnightMainMenuPage::Home,
						TEXT("maintenance submenu retains Home page"));
					UWidget* const Submenu = Menu->GetWidgetFromName(TEXT("Panel_MaintenanceSubmenu"));
					Check(Submenu && (Submenu->GetVisibility() == ESlateVisibility::Visible) && Submenu->GetParent() == Menu->GetWidgetFromName(TEXT("Panel_Home"))
						&& !Menu->GetWidgetFromName(TEXT("Panel_MaintenanceSelectionRuntime")), TEXT("adjacent submenu belongs to Home; centered chooser absent"));
					Check(Menu->RequestBack() && Menu->GetCurrentPage() == EKnightMainMenuPage::Home && Submenu && !(Submenu->GetVisibility() == ESlateVisibility::Visible),
						TEXT("Back collapses submenu without opening Settings"));
					UCommonButtonBase* const MaintenanceItem = Cast<UCommonButtonBase>(Menu->GetWidgetFromName(TEXT("Button_Maintenance")));
					if (MaintenanceItem) MaintenanceItem->OnClicked().Broadcast();
					Check(Submenu && (Submenu->GetVisibility() == ESlateVisibility::Visible), TEXT("maintenance item opens submenu"));
					if (MaintenanceItem) MaintenanceItem->OnClicked().Broadcast();
					Check(Submenu && !(Submenu->GetVisibility() == ESlateVisibility::Visible), TEXT("same maintenance item toggles submenu closed"));
					for (const TCHAR* ChoiceName : { TEXT("MaintenanceChoiceCrafting"), TEXT("MaintenanceChoiceEnhancement"), TEXT("MaintenanceChoiceSockets") })
					{
						Menu->ShowPage(EKnightMainMenuPage::Maintenance);
						UCommonButtonBase* const Choice = Cast<UCommonButtonBase>(Menu->GetWidgetFromName(ChoiceName));
						Check(Choice != nullptr, ChoiceName);
						if (Choice) Choice->OnClicked().Broadcast();
						Check(Menu->GetCurrentPage() == EKnightMainMenuPage::Maintenance && Maintenance
							&& !Maintenance->IsMaterialsViewOpen() && Submenu && !(Submenu->GetVisibility() == ESlateVisibility::Visible), TEXT("choice opens destination and hides submenu"));

						Check(Menu->RequestBack() && Menu->GetCurrentPage() == EKnightMainMenuPage::Home && Submenu && (Submenu->GetVisibility() == ESlateVisibility::Visible),
							TEXT("destination Back restores Home with submenu expanded"));
					}
					Check(Menu->ShowPage(EKnightMainMenuPage::Codex) && Submenu && !(Submenu->GetVisibility() == ESlateVisibility::Visible), TEXT("other Home destination dismisses submenu"));
					Menu->ShowPage(EKnightMainMenuPage::Home);
					Check(Menu->ToggleMaterialsShortcut() && Menu->RequestBack() && Menu->GetCurrentPage() == EKnightMainMenuPage::Home
						&& Submenu && !(Submenu->GetVisibility() == ESlateVisibility::Visible), TEXT("Materials shortcut Back returns directly Home without submenu"));
				}
			}
			else if (Mode == TEXT("EquipmentEquip") || Mode == TEXT("EquipmentUnequip"))
			{
				const UknightPartItemDefinition* const HeadA = LoadObject<UknightPartItemDefinition>(nullptr,
					TEXT("/Game/Knight/Data/Equipment/Janyeol/DA_Equipment_Janyeol_HeadA.DA_Equipment_Janyeol_HeadA"));
				const UknightPartItemDefinition* const HeadB = LoadObject<UknightPartItemDefinition>(nullptr,
					TEXT("/Game/Knight/Data/Equipment/Janyeol/DA_Equipment_Janyeol_HeadB.DA_Equipment_Janyeol_HeadB"));
				APlayerController* const Player = UGameplayStatics::GetPlayerController(World, 0);
				UKnightMaintenanceWidget* const Widget = Player ? CreateWidget<UKnightMaintenanceWidget>(
					Player, UKnightMaintenanceWidget::GetSharedWidgetClass()) : nullptr;
				if (!HeadA || !HeadB || !Widget || !Widget->WidgetTree)
				{
					UE_LOG(LogknightDev, Error, TEXT("Phase9 equipment fixture or WBP missing"));
					return;
				}
				const FGameplayTag HeadTag = knightGameplayTags::Equipment_Slot_Head;
				const FPrimaryAssetId HeadAId = HeadA->GetPrimaryAssetId();
				const FPrimaryAssetId HeadBId = HeadB->GetPrimaryAssetId();
				if (Mode == TEXT("EquipmentEquip"))
				{
					Save->ItemCounts.Reset();
					Save->EquippedItems.Reset();
					Save->ExplicitlyEmptySlots.Reset();
					Save->ItemCounts.Add(HeadAId, 1);
					Save->ItemCounts.Add(HeadBId, 1);
					Save->EquippedItems.Add(HeadTag, HeadAId);
					Save->Money = 4321;
				}
				Widget->Open(EKnightMaintenanceDestination::Crafting);
				UCommonButtonBase* const EquipmentTab = Cast<UCommonButtonBase>(Widget->WidgetTree->FindWidget(TEXT("Btn_Weapon")));
				UCommonButtonBase* const Execute = Cast<UCommonButtonBase>(Widget->WidgetTree->FindWidget(TEXT("Btn_Execute")));
				UCommonButtonBase* const Unequip = Cast<UCommonButtonBase>(Widget->WidgetTree->FindWidget(TEXT("Btn_Unequip")));
				UCommonButtonBase* const Confirm = Cast<UCommonButtonBase>(Widget->WidgetTree->FindWidget(TEXT("Btn_ConfirmOk")));
				UCommonButtonBase* const Cancel = Cast<UCommonButtonBase>(Widget->WidgetTree->FindWidget(TEXT("Btn_ConfirmCancel")));
				UScrollBox* const List = Cast<UScrollBox>(Widget->WidgetTree->FindWidget(TEXT("List_Items")));
				UScrollBox* const Picker = Cast<UScrollBox>(Widget->WidgetTree->FindWidget(TEXT("RuntimeEquipmentPickerList")));
				UTextBlock* const Name = Cast<UTextBlock>(Widget->WidgetTree->FindWidget(TEXT("Txt_PartName")));
				if (!EquipmentTab || !Execute || !Unequip || !Confirm || !Cancel || !List || !Picker || !Name)
				{
					UE_LOG(LogknightDev, Error, TEXT("Phase9 equipment UI controls missing"));
					return;
				}
				EquipmentTab->OnClicked().Broadcast();
				Check(List->GetChildrenCount() == 5, TEXT("equipment tab shows exactly five fixed slots"));
				auto HeadRow = [List]() -> UCommonButtonBase*
				{
					return List->GetChildrenCount() > 1 ? Cast<UCommonButtonBase>(List->GetChildAt(1)) : nullptr;
				};
				if (Mode == TEXT("EquipmentUnequip"))
				{
					Check(Progression->GetEquippedPart(HeadTag) == HeadB, TEXT("new process restores confirmed replacement"));
					if (HeadRow()) HeadRow()->OnClicked().Broadcast();
					Unequip->OnClicked().Broadcast();
					Check(!Progression->GetEquippedPart(HeadTag) && Save->ExplicitlyEmptySlots.Contains(HeadTag)
						&& Progression->GetItemCount(HeadB) == 1,
						TEXT("unequip records explicit empty slot without deleting owned equipment"));
				}
				else
				{
					Check(Name->GetText().ToString().Contains(TEXT("현재 장비")), TEXT("unselected slot displays overall stats"));
					if (HeadRow()) HeadRow()->OnClicked().Broadcast();
					Check(Name->GetText().EqualTo(HeadA->DisplayName), TEXT("selected slot displays individual equipment"));
					if (HeadRow()) HeadRow()->OnClicked().Broadcast();
					Check(Name->GetText().ToString().Contains(TEXT("현재 장비")), TEXT("same-slot click restores overall view"));
					if (HeadRow()) HeadRow()->OnClicked().Broadcast();
					Execute->OnClicked().Broadcast();
					Check(Picker->GetChildrenCount() == 2, TEXT("replacement picker contains only owned compatible heads"));
					Cancel->OnClicked().Broadcast();
					Check(Progression->GetEquippedPart(HeadTag) == HeadA && Save->Money == 4321,
						TEXT("picker cancellation does not mutate equipment or currency"));
					Execute->OnClicked().Broadcast();
					if (Picker->GetChildrenCount() > 1)
						if (UCommonButtonBase* const Candidate = Cast<UCommonButtonBase>(Picker->GetChildAt(1))) Candidate->OnClicked().Broadcast();
					Check(Saves->SelectSlot(TEXT("Phase9EquipmentOther")) && Progression->GetItemCount(HeadB) == 0,
						TEXT("profile switch closes stale equipment picker and isolates ownership"));
					Confirm->OnClicked().Broadcast();
					Check(Progression->GetEquippedPart(HeadTag) != HeadB, TEXT("stale confirmation cannot equip previous profile item"));
					Check(Saves->SelectSlot(TEXT("Phase9EquipmentEquip")), TEXT("return to isolated equipment profile"));
					UknightSaveGame* const Active = Saves->GetSaveGame();
					if (Active)
					{
						Active->ItemCounts.Add(HeadAId, 1);
						Active->ItemCounts.Add(HeadBId, 1);
						Active->EquippedItems.Add(HeadTag, HeadAId);
						Active->Money = 4321;
						Progression->OnProgressionChanged.Broadcast();
						if (HeadRow()) HeadRow()->OnClicked().Broadcast();
						Execute->OnClicked().Broadcast();
						if (Picker->GetChildrenCount() > 1)
							if (UCommonButtonBase* const Candidate = Cast<UCommonButtonBase>(Picker->GetChildAt(1))) Candidate->OnClicked().Broadcast();
						Active->ItemCounts.Remove(HeadBId);
						Confirm->OnClicked().Broadcast();
						Check(Progression->GetEquippedPart(HeadTag) == HeadA,
							TEXT("confirmation revalidates ownership before replacement"));
						Active->ItemCounts.Add(HeadBId, 1);
						Progression->OnProgressionChanged.Broadcast();
						if (Picker->GetChildrenCount() > 1)
							if (UCommonButtonBase* const Candidate = Cast<UCommonButtonBase>(Picker->GetChildAt(1))) Candidate->OnClicked().Broadcast();
						Confirm->OnClicked().Broadcast();
						Check(Progression->GetEquippedPart(HeadTag) == HeadB && Active->Money == 4321,
							TEXT("confirmed replacement updates equipped ID without consuming ownership or currency"));
						Progression->OnProgressionChanged.Broadcast();
						Check(Name->GetText().EqualTo(HeadB->DisplayName), TEXT("refresh preserves valid selected slot"));
					}
				}
				Widget->Close();
			}
			else if (Mode == TEXT("Craft") || Mode == TEXT("RestoreCraft"))
			{
				const FPrimaryAssetId ResultId = Recipe->Result->GetPrimaryAssetId();
				if (Mode == TEXT("Craft"))
				{
					Save->ItemCounts.Reset();
					Save->EquippedItems.Reset();
					Save->ExplicitlyEmptySlots.Reset();
					Save->Money = 913;
					for (const FknightItemCost& Cost : Recipe->Costs)
						Save->ItemCounts.Add(Cost.Item->GetPrimaryAssetId(), Cost.Count + 2);
					const UknightPartItemDefinition* const EquippedBefore = Progression->GetEquippedPart(Recipe->Result->SlotTag);
					Check(Progression->CanCraftPart(Recipe), TEXT("two-material recipe is craftable in isolated fixture"));
					Saves->DebugFailNextSaves(1);
					Check(Progression->CraftPart(Recipe), TEXT("craft transaction accepted"));
					Check(Progression->GetItemCount(Recipe->Result) == 1 && !Progression->CanCraftPart(Recipe)
						&& Progression->GetEquippedPart(Recipe->Result->SlotTag) == EquippedBefore
						&& !Save->EquippedItems.Contains(Recipe->Result->SlotTag) && Save->Money == 913,
						TEXT("craft grants one owned part, blocks duplicate, does not autoequip or spend currency"));
				}
				else Check(Progression->GetItemCount(Recipe->Result) == 1 && Save->Money == 913
					&& !Save->EquippedItems.Contains(Recipe->Result->SlotTag),
					TEXT("new process restores crafted ownership without autoequip or extra currency change"));
				for (const FknightItemCost& Cost : Recipe->Costs)
					Check(Progression->GetItemCount(Cost.Item) == 2, TEXT("recipe material quantity restored after one deduction"));
				Check(ResultId.IsValid(), TEXT("crafted asset has stable item ID"));
			}
			else if (Mode == TEXT("SaveState") || Mode == TEXT("RestoreState"))
			{
				const FPrimaryAssetId HideId = Hide->GetPrimaryAssetId();
				const FPrimaryAssetId WeaponId = Weapon->GetPrimaryAssetId();
				const FPrimaryAssetId SocketId = Socket->GetPrimaryAssetId();
				if (Mode == TEXT("SaveState"))
				{
					Save->ItemCounts.Reset();
					Save->EquippedItems.Reset();
					Save->ItemUpgradeLevels.Reset();
					Save->WeaponUpgradeFailures.Reset();
					Save->ItemCounts.Add(HideId, 7);
					Save->ItemCounts.Add(WeaponId, 1);
					Save->ItemCounts.Add(SocketId, 1);
					Save->Money = 2468;
					Save->EquippedItems.Add(knightGameplayTags::Equipment_Slot_Weapon, WeaponId);
					Save->ItemUpgradeLevels.Add(WeaponId, 4);
					Save->WeaponUpgradeFailures.Add(WeaponId, 3);
					Save->UnlockedSharedSocketSlots = 1;
					Save->EquippedSocketItems = { SocketId };
					Save->FirstDefeatedBossIds.Add(TEXT("Janyeol"));
					Saves->RequestSave();
				}
				Check(Progression->GetMoney() == 2468 && Progression->GetItemCount(Hide) == 7
					&& Progression->GetItemCount(Weapon) == 1 && Progression->GetItemCount(Socket) == 1,
					TEXT("materials, owned weapon/socket and currency agree"));
				Check(Progression->GetEquippedPart(knightGameplayTags::Equipment_Slot_Weapon) == Weapon
					&& Progression->GetUpgradeLevel(Weapon) == 4 && Progression->GetWeaponUpgradeFailures(Weapon) == 3,
					TEXT("equipped ID, upgrade level and failure compensation agree"));
				Check(Progression->GetUnlockedSharedSocketSlots() == 1 && Progression->GetEquippedSocketItem(0) == Socket,
					TEXT("socket unlock and assignment agree"));
				if (Mode == TEXT("RestoreState"))
				{
					TArray<FKnightMainEquipmentSlot> Slots;
					Check(Progression->GetMainEquipmentSlots(Slots) && Slots.Num() == 5,
						TEXT("five main equipment slots resolve after reload"));
					Check(Saves->SelectSlot(TEXT("Phase9Other")) && Progression->GetMoney() == 0
						&& Progression->GetItemCount(Hide) == 0 && Progression->GetUnlockedSharedSocketSlots() == 0,
						TEXT("other isolated profile does not inherit ownership, money or unlocks"));
					Check(Saves->SelectSlot(TEXT("Phase9SaveState")) && Progression->GetMoney() == 2468
						&& Progression->GetEquippedSocketItem(0) == Socket,
						TEXT("original profile restores after active-profile change"));
				}
			}
			else if (Mode == TEXT("Legacy"))
			{
				Check(Saves->HasSelectedSlot() && Save->SaveVersion <= UknightSaveGame::CurrentVersion,
					TEXT("copied older-format fixture loads into current save class"));
				TArray<FKnightMainEquipmentSlot> Slots;
				Check(Progression->GetMainEquipmentSlots(Slots) && Slots.Num() == 5,
					TEXT("legacy fixture resolves current five-slot query"));
			}
			if (Failures > 0)
			{
				UE_LOG(LogknightDev, Error, TEXT("Phase9 IntegrationCases SUMMARY mode=%s failures=%d"), *Mode, Failures);
			}
			else
			{
				UE_LOG(LogknightDev, Log, TEXT("Phase9 IntegrationCases SUMMARY mode=%s failures=%d"), *Mode, Failures);
			}
			return;
		}
		if (Args[0] == TEXT("SocketCases"))
		{
			const FString Mode = Args.Num() > 1 ? Args[1] : TEXT("Widget");
			int32 Failures = 0;
			auto Check = [&Failures, &Mode](bool bPassed, const TCHAR* Case)
			{
				if (!bPassed) ++Failures;
				UE_LOG(LogknightDev, Log, TEXT("Phase8 SocketCases %s %s: %s"), *Mode,
					bPassed ? TEXT("PASS") : TEXT("FAIL"), Case);
			};
			UknightSaveSubsystem* const Saves = World->GetGameInstance()->GetSubsystem<UknightSaveSubsystem>();
			const UknightSocketItemDefinition* const Repair = LoadObject<UknightSocketItemDefinition>(nullptr,
				TEXT("/Game/Knight/Items/Sockets/DA_Socket_RepairExpansion.DA_Socket_RepairExpansion"));
			const UknightSocketItemDefinition* const Attack = LoadObject<UknightSocketItemDefinition>(nullptr,
				TEXT("/Game/Knight/Items/Sockets/DA_Socket_AttackEfficiency.DA_Socket_AttackEfficiency"));
			const UknightSocketItemDefinition* const Charge = LoadObject<UknightSocketItemDefinition>(nullptr,
				TEXT("/Game/Knight/Items/Sockets/DA_Socket_ChargeAssist.DA_Socket_ChargeAssist"));
			const FString TestSlot = Mode == TEXT("RestoreEquip") ? TEXT("Phase8Equip") : TEXT("Phase8") + Mode;
			if (!Saves || !Repair || !Attack || !Charge || !Saves->SelectSlot(TestSlot))
			{
				UE_LOG(LogknightDev, Error, TEXT("Phase8 SocketCases setup failed mode=%s"), *Mode);
				return;
			}
			UknightSaveGame* const Save = Saves->GetSaveGame();
			if (!Save) { UE_LOG(LogknightDev, Error, TEXT("Phase8 SocketCases save unavailable")); return; }
			const FPrimaryAssetId RepairId = Repair->GetPrimaryAssetId();
			const FPrimaryAssetId AttackId = Attack->GetPrimaryAssetId();
			const FPrimaryAssetId ChargeId = Charge->GetPrimaryAssetId();
			if (Mode == TEXT("RestoreEquip"))
			{
				Check(Save->UnlockedSharedSocketSlots == 1 && Save->EquippedSocketItems.IsValidIndex(0)
					&& Save->EquippedSocketItems[0] == RepairId && Save->ItemCounts.FindRef(RepairId) == 1,
					TEXT("new process restores saved unlock, assignment and ownership"));
				UE_LOG(LogknightDev, Log, TEXT("Phase8 SocketCases SUMMARY mode=%s failures=%d"), *Mode, Failures);
				return;
			}
			Save->ItemCounts.Reset();
			Save->EquippedSocketItems.Reset();
			Save->UnlockedSharedSocketSlots = Mode == TEXT("Zero") ? 0 : Mode == TEXT("Effect") ? 3
				: Mode == TEXT("Overflow") ? 4 : Mode == TEXT("Reject") ? 2 : 1;
			Save->ItemCounts.Add(RepairId, Mode == TEXT("Reject") ? 2 : 1);
			if (Mode == TEXT("Replace") || Mode == TEXT("Unequip") || Mode == TEXT("Reject"))
			{
				Save->EquippedSocketItems.SetNum(Save->UnlockedSharedSocketSlots);
				Save->EquippedSocketItems[0] = RepairId;
			}
			if (Mode == TEXT("Replace") || Mode == TEXT("Overflow") || Mode == TEXT("Effect"))
				Save->ItemCounts.Add(AttackId, 1);
			if (Mode == TEXT("Overflow"))
			{
				Save->EquippedSocketItems.SetNum(4);
				Save->EquippedSocketItems[3] = RepairId;
			}
			if (Mode == TEXT("Effect"))
			{
				Save->ItemCounts.Add(ChargeId, 1);
				Save->EquippedSocketItems = { RepairId, AttackId, ChargeId };
			}
			APlayerController* const Player = UGameplayStatics::GetPlayerController(World, 0);
			UKnightMaintenanceWidget* const Widget = Player ? CreateWidget<UKnightMaintenanceWidget>(
				Player, UKnightMaintenanceWidget::GetSharedWidgetClass()) : nullptr;
			if (!Widget || !Widget->WidgetTree)
			{
				UE_LOG(LogknightDev, Error, TEXT("Phase8 SocketCases WBP construction failed mode=%s"), *Mode);
				return;
			}
			Widget->Open(EKnightMaintenanceDestination::Sockets);
			auto SlotButton = [Widget](int32 Index) -> UCommonButtonBase*
			{
				return Cast<UCommonButtonBase>(Widget->WidgetTree->FindWidget(
					FName(*FString::Printf(TEXT("Phase8_SocketSlot%d"), Index))));
			};
			auto CandidateButton = [Widget](int32 Index) -> UCommonButtonBase*
			{
				return Cast<UCommonButtonBase>(Widget->WidgetTree->FindWidget(
					FName(*FString::Printf(TEXT("Phase8_SocketCandidate%d"), Index))));
			};
			UTextBlock* const Name = Cast<UTextBlock>(Widget->WidgetTree->FindWidget(TEXT("SocketDetailTitle")));
			UTextBlock* const State = Cast<UTextBlock>(Widget->WidgetTree->FindWidget(TEXT("SocketAssignmentSummary")));
			UTextBlock* const Bonus = Cast<UTextBlock>(Widget->WidgetTree->FindWidget(TEXT("SocketEffectsText")));
			UCommonButtonBase* const Equip = Cast<UCommonButtonBase>(Widget->WidgetTree->FindWidget(TEXT("SocketEquipAction")));
			UCommonButtonBase* const Unequip = Cast<UCommonButtonBase>(Widget->WidgetTree->FindWidget(TEXT("SocketUnequipAction")));
			Check(SlotButton(0) && SlotButton(1) && SlotButton(2) && Name && State && Bonus && Equip && Unequip,
				TEXT("exactly three visible slot controls and required detail controls exist"));
			if (!SlotButton(0) || !SlotButton(1) || !SlotButton(2) || !Name || !State || !Bonus || !Equip || !Unequip)
				return;
			Check(Name->GetText().ToString().Contains(TEXT("활성")), TEXT("fresh entry selects no slot and shows combined effects"));
			Check(!SlotButton(0)->GetSelected() && !SlotButton(1)->GetSelected() && !SlotButton(2)->GetSelected()
				&& CandidateButton(0) && !CandidateButton(0)->GetSelected() && !SlotButton(3), TEXT("three positions and owned list start with no selection"));
			if (Mode == TEXT("Zero"))
			{
				SlotButton(0)->OnClicked().Broadcast();
				Check(Name->GetText().ToString().Contains(TEXT("활성")) && Save->EquippedSocketItems.IsEmpty(),
					TEXT("zero unlocks shows locked controls and rejects slot selection"));
			}
			else if (Mode == TEXT("Widget"))
			{
				CandidateButton(0)->OnClicked().Broadcast();
				Check(Name->GetText().ToString().Contains(TEXT("활성")) && Save->EquippedSocketItems.IsEmpty(),
					TEXT("candidate can be selected before slot without changing combined view"));
				SlotButton(2)->OnClicked().Broadcast();
				Check(Name->GetText().ToString().Contains(TEXT("활성")), TEXT("locked slot click cannot select"));
				SlotButton(0)->OnClicked().Broadcast();
				Check(Name->GetText().ToString().Contains(TEXT("빈 소켓")) && CandidateButton(0),
					TEXT("unlocked empty slot reveals owned candidate without equipping"));
				if (CandidateButton(0)) CandidateButton(0)->OnClicked().Broadcast();
				Check(Name->GetText().ToString().Contains(TEXT("빈 소켓")) && Save->EquippedSocketItems.IsEmpty(),
					TEXT("candidate selection does not appear equipped or mutate save"));
				Check(!Widget->HandleConfirmKey() && Save->EquippedSocketItems.IsEmpty(),
					TEXT("shared confirm route cannot equip a candidate; explicit action owns the transaction"));
				SlotButton(0)->OnClicked().Broadcast();
				Check(Name->GetText().ToString().Contains(TEXT("활성")) && Save->EquippedSocketItems.IsEmpty(),
					TEXT("same slot click deselects without unequipping"));
				SlotButton(0)->OnClicked().Broadcast();
				UCommonButtonBase* OldSlot = SlotButton(0);
				UCommonButtonBase* OldCandidate = CandidateButton(0);
				Check(Saves->SelectSlot(TEXT("Phase8WidgetOther")), TEXT("profile change accepted"));
				OldSlot->OnClicked().Broadcast(); OldCandidate->OnClicked().Broadcast();
				Equip->OnClicked().Broadcast(); Saves->OnSaveFinished.Broadcast(true);
				Check(Name->GetText().ToString().Contains(TEXT("활성")) && State->GetText().ToString().Contains(TEXT("해금 0"))
					&& Save->EquippedSocketItems.IsEmpty(), TEXT("profile change clears slot and candidate without old-profile mutation"));
			}
			else if (Mode == TEXT("Effect"))
			{
				const AknightCharacter* const Defaults = GetDefault<AknightCharacter>();
				const int32 Base = Defaults ? Defaults->GetBaseRepairKitGuardsPerKit() : 0;
				Check(Base > 0 && Progression->GetSocketGaugeGuardsPerKit(Base) == FMath::Max(1, Base - 1)
					&& Progression->GetSocketEffectCount(EKnightSocketEffect::SocketGaugeCharge) == 1,
					TEXT("gauge uses character baseline and guards-per-kit query, not effect-value multiplier"));
				Check(Progression->GetRepairKitCapacityBonus() == FMath::RoundToInt(Repair->EffectValue)
					&& FMath::IsNearlyEqual(Progression->GetAttackStaminaCostMultiplier(), 1.f - Attack->EffectValue),
					TEXT("combined effects match gameplay query semantics"));
				SlotButton(1)->OnClicked().Broadcast();
				Check(Name->GetText().EqualTo(Attack->DisplayName) && Bonus->GetText().ToString().Contains(TEXT("공격")),
					TEXT("selected equipped slot shows its actual individual effect"));
				SlotButton(1)->OnClicked().Broadcast();
				Check(Name->GetText().ToString().Contains(TEXT("활성")), TEXT("reselection restores combined effects"));
				Check(Save->EquippedSocketItems[1] == AttackId, TEXT("same equipped slot click only deselects and never unequips"));
				SlotButton(2)->OnClicked().Broadcast();
				Check(Bonus->GetText().ToString().Contains(TEXT("기준")) && !Bonus->GetText().ToString().Contains(TEXT("%")),
					TEXT("charge detail uses guards-per-kit context rather than percentage"));
			}
			else if (Mode == TEXT("Overflow"))
			{
				Check(State->GetText().ToString().Contains(TEXT("읽기 전용"))
					&& Bonus->GetText().ToString().Contains(TEXT("추가 저장 슬롯"))
					&& Progression->GetRepairKitCapacityBonus() == FMath::RoundToInt(Repair->EffectValue),
					TEXT("overflow assignment remains in combined effect and is explicitly read-only"));
				SlotButton(0)->OnClicked().Broadcast();
				if (CandidateButton(0)) CandidateButton(0)->OnClicked().Broadcast();
				Equip->OnClicked().Broadcast();
				Check(Save->EquippedSocketItems.Num() == 4 && Save->EquippedSocketItems[3] == RepairId
					&& !Save->EquippedSocketItems[0].IsValid(), TEXT("overflow UI cannot truncate or mutate saved assignments"));
			}
			else if (Mode == TEXT("Reject"))
			{
				Check(!Repair->bAllowDuplicateEquip, TEXT("fixture socket prohibits duplicate equipment"));
				SlotButton(1)->OnClicked().Broadcast();
				if (CandidateButton(0)) CandidateButton(0)->OnClicked().Broadcast();
				Equip->OnClicked().Broadcast();
				Check(Save->EquippedSocketItems[0] == RepairId && !Save->EquippedSocketItems[1].IsValid(),
					TEXT("prohibited duplicate rejected without mutation"));
				Check(!Progression->EquipSocketItem(2, Repair) && !Progression->EquipSocketItem(1, Charge),
					TEXT("backend rejects locked slot and unowned socket"));
			}
			else if (Mode == TEXT("Equip") || Mode == TEXT("Replace") || Mode == TEXT("Unequip"))
			{
				SlotButton(0)->OnClicked().Broadcast();
				if (Mode == TEXT("Unequip")) Unequip->OnClicked().Broadcast();
				else
				{
					const int32 CandidateIndex = Mode == TEXT("Equip") || Attack->DisplayName.ToString() < Repair->DisplayName.ToString() ? 0 : 1;
					if (CandidateButton(CandidateIndex)) CandidateButton(CandidateIndex)->OnClicked().Broadcast();
					if (UFunction* Click = Equip->FindFunction(TEXT("HandleButtonClicked"))) Equip->ProcessEvent(Click, nullptr);
				}
				const FPrimaryAssetId Expected = Mode == TEXT("Equip") ? RepairId : Mode == TEXT("Replace") ? AttackId : FPrimaryAssetId();
				Check(Save->EquippedSocketItems.IsValidIndex(0) && Save->EquippedSocketItems[0] == Expected
					&& Save->ItemCounts.FindRef(RepairId) == 1 && (Mode != TEXT("Replace") || Save->ItemCounts.FindRef(AttackId) == 1),
					TEXT("explicit action atomically changes assignment without consuming ownership"));
				Check(Mode == TEXT("Unequip") ? Name->GetText().ToString().Contains(TEXT("빈 소켓"))
					: Name->GetText().EqualTo(Mode == TEXT("Replace") ? Attack->DisplayName : Repair->DisplayName),
					TEXT("valid selected slot remains selected and shows actual post-action state"));
				Saves->OnSaveFinished.Broadcast(false);
				Saves->OnSaveFinished.Broadcast(true);
				Check(Save->EquippedSocketItems[0] == Expected && Save->ItemCounts.FindRef(RepairId) == 1,
					TEXT("save completion callback never replays socket mutation"));
			}
			Widget->Close();
			UE_LOG(LogknightDev, Log, TEXT("Phase8 SocketCases SUMMARY mode=%s failures=%d"), *Mode, Failures);
			return;
		}
		if (Args[0] == TEXT("UpgradeCases"))
		{
			const FString Mode = Args.Num() > 1 ? Args[1] : TEXT("Reject");
			int32 Failures = 0;
			auto Check = [&Failures, &Mode](bool bPassed, const TCHAR* Case)
			{
				if (!bPassed) ++Failures;
				UE_LOG(LogknightDev, Log, TEXT("Phase7 UpgradeCases %s %s: %s"), *Mode,
					bPassed ? TEXT("PASS") : TEXT("FAIL"), Case);
			};
			UknightSaveSubsystem* const Saves = World->GetGameInstance()->GetSubsystem<UknightSaveSubsystem>();
			const UknightWeaponItemDefinition* const Weapon = LoadObject<UknightWeaponItemDefinition>(nullptr,
				TEXT("/Game/Knight/Data/Equipment/Janyeol/DA_Equipment_Janyeol_Weapon.DA_Equipment_Janyeol_Weapon"));
			const UknightWeaponItemDefinition* const Other = LoadObject<UknightWeaponItemDefinition>(nullptr,
				TEXT("/Game/Knight/Data/Equipment/Janhyang/DA_Equipment_Janhyang_Weapon.DA_Equipment_Janhyang_Weapon"));
			if (!Saves || !Weapon || !Other || Weapon->UpgradeSteps.Num() != 15 || !Saves->SelectSlot(TEXT("Phase7") + Mode))
			{
				UE_LOG(LogknightDev, Error, TEXT("Phase7 UpgradeCases setup failed mode=%s"), *Mode);
				return;
			}
			UknightSaveGame* const Save = Saves->GetSaveGame();
			if (!Save) { UE_LOG(LogknightDev, Error, TEXT("Phase7 UpgradeCases save unavailable")); return; }
			const FPrimaryAssetId WeaponId = Weapon->GetPrimaryAssetId();
			const FPrimaryAssetId OtherId = Other->GetPrimaryAssetId();
			Save->ItemCounts.Reset();
			Save->EquippedItems.Reset();
			Save->ItemUpgradeLevels.Reset();
			Save->WeaponUpgradeFailures.Reset();
			Save->Money = 10000;
			Save->ItemCounts.Add(WeaponId, 1);
			Save->ItemCounts.Add(OtherId, 1);
			const int32 Level = Mode == TEXT("SuccessUnequipped") ? 0 : Mode == TEXT("SuccessFinal") ? 14 : 9;
			Save->ItemUpgradeLevels.Add(WeaponId, Level);
			Save->WeaponUpgradeFailures.Add(WeaponId, 2);
			Save->EquippedItems.Add(knightGameplayTags::Equipment_Slot_Weapon,
				Mode == TEXT("SuccessUnequipped") ? OtherId : WeaponId);
			const FknightUpgradeStep& Step = Weapon->UpgradeSteps[Level];
			const UknightMaterialItemDefinition* const Gate = Step.Costs.IsEmpty() ? nullptr
				: Cast<UknightMaterialItemDefinition>(Step.Costs[0].Item.Get());
			if (Gate) Save->ItemCounts.Add(Gate->GetPrimaryAssetId(), Step.Costs[0].Count);
			FKnightWeaponUpgradeQuote Quote;
			Progression->GetWeaponUpgradeQuote(Weapon, Quote);
			const double ExpectedChance = FMath::Clamp(static_cast<double>(Step.BaseSuccessChance)
				+ 2.0 * Step.FailureBonusPerAttempt, 0.0, 1.0);
			Check(Quote.Availability == EKnightWeaponUpgradeAvailability::Ready
				&& Quote.CurrentLevel == Level && Quote.TargetLevel == Level + 1
				&& Quote.MoneyCost == Step.MoneyCost, TEXT("owned weapon quote matches step and cost"));
			Check(FMath::IsNearlyEqual(Quote.EffectiveSuccessChance, ExpectedChance)
				&& FMath::IsNearlyEqual(Quote.NextWeaponAttackIncrease, Step.AttackIncrease),
				TEXT("effective chance and next weapon attack come from transaction rules"));
			if (Mode == TEXT("Widget") || Mode == TEXT("WidgetAttempt"))
			{
				APlayerController* const Player = UGameplayStatics::GetPlayerController(World, 0);
				UKnightMaintenanceWidget* const Widget = Player ? CreateWidget<UKnightMaintenanceWidget>(
					Player, UKnightMaintenanceWidget::GetSharedWidgetClass()) : nullptr;
				if (!Widget || !Widget->WidgetTree)
				{
					UE_LOG(LogknightDev, Error, TEXT("Phase7 UpgradeCases Widget WBP construction failed"));
					return;
				}
				Widget->Open(EKnightMaintenanceDestination::Enhancement);
				UScrollBox* List = Cast<UScrollBox>(Widget->GetWidgetFromName(TEXT("EnhancementOwnedWeapons")));
				UCommonTextBlock* Name = Cast<UCommonTextBlock>(Widget->GetWidgetFromName(TEXT("EnhancementWeaponName")));
				UCommonTextBlock* Chance = Cast<UCommonTextBlock>(Widget->GetWidgetFromName(TEXT("EnhancementChance")));
				UCommonTextBlock* Gain = Cast<UCommonTextBlock>(Widget->GetWidgetFromName(TEXT("EnhancementAttackIncrease")));
				UCommonButtonBase* Execute = Cast<UCommonButtonBase>(Widget->GetWidgetFromName(TEXT("Phase7_UpgradeButton")));
				if (!List || !Name || !Chance || !Gain || !Execute) { Check(false, TEXT("enhancement controls exist")); return; }
				Check(List->GetChildrenCount() == 2 && Name->GetText().ToString().Contains(TEXT("선택")),
					TEXT("fresh entry lists owned weapons with no selected target"));
				auto FindRow = [List](FPrimaryAssetId Id) -> UKnightMaintenanceEntryWidget*
				{
					for (UWidget* Child : List->GetAllChildren())
						if (auto* Row = Cast<UKnightMaintenanceEntryWidget>(Child))
							if (Row->GetPart() && Row->GetPart()->GetPrimaryAssetId() == Id) return Row;
					return nullptr;
				};
				UKnightMaintenanceEntryWidget* First = FindRow(WeaponId);
				UKnightMaintenanceEntryWidget* Second = FindRow(OtherId);
				Check(First && Second && !First->GetSelected() && !Second->GetSelected(), TEXT("owned rows initially unselected"));
				if (First) First->OnClicked().Broadcast();
				FNumberFormattingOptions Percent; Percent.MinimumFractionalDigits = 1; Percent.MaximumFractionalDigits = 1;
				Check(Name->GetText().EqualTo(Weapon->DisplayName)
					&& Chance->GetText().EqualTo(FText::AsPercent(Quote.EffectiveSuccessChance, &Percent))
					&& Gain->GetText().ToString().Contains(FText::AsNumber(Quote.NextWeaponAttackIncrease).ToString()),
					TEXT("target details use actual effective probability and successful stat gain"));
				if (Second) Second->OnClicked().Broadcast();
				Check(First && Second && !First->GetSelected() && Second->GetSelected(), TEXT("selection clears previous row highlight"));
				if (First) First->OnClicked().Broadcast();
				Check(Save->Money == 10000 && Save->ItemUpgradeLevels.FindRef(WeaponId) == Level
					&& Save->EquippedItems.FindRef(knightGameplayTags::Equipment_Slot_Weapon) == WeaponId,
					TEXT("target selection does not equip or mutate progression"));
				Progression->OnProgressionChanged.Broadcast();
				Check(Name->GetText().EqualTo(Weapon->DisplayName) && FindRow(WeaponId) && FindRow(WeaponId)->GetSelected(),
					TEXT("refresh preserves target by ID and only its selection"));
				Saves->OnSaveFinished.Broadcast(true);
				Check(Save->Money == 10000 && Save->ItemUpgradeLevels.FindRef(WeaponId) == Level
					&& Name->GetText().EqualTo(Weapon->DisplayName), TEXT("save completion preserves selection without replaying attempt"));
				// A quote rendered for +9 must not silently buy a different level after an external change.
				Save->ItemUpgradeLevels.Add(WeaponId, Level + 1);
				Execute->OnClicked().Broadcast();
				Check(Save->Money == 10000 && Save->ItemUpgradeLevels.FindRef(WeaponId) == Level + 1,
					TEXT("execution rejects stale target level before transaction"));
				Save->ItemUpgradeLevels.Add(WeaponId, Level);
				if (Mode == TEXT("WidgetAttempt"))
				{
					Progression->OnProgressionChanged.Broadcast();
					Progression->SetDebugNextWeaponUpgradeOutcome(1);
					UFunction* Click = Execute->FindFunction(TEXT("HandleButtonClicked"));
					if (Click) Execute->ProcessEvent(Click, nullptr);
					Check(Click && Save->Money == 10000 - Quote.MoneyCost && Save->ItemUpgradeLevels.FindRef(WeaponId) == Level + 1,
						TEXT("one real button-handler activation performs exactly one existing transaction"));
					Check(!Gate || Save->ItemCounts.FindRef(Gate->GetPrimaryAssetId()) == 0,
						TEXT("successful UI attempt consumes gate once"));
					const FString LevelText = Cast<UCommonTextBlock>(Widget->GetWidgetFromName(TEXT("EnhancementLevels")))->GetText().ToString();
					Check(LevelText.Contains(FString::Printf(TEXT("+%d"), Level + 1)) && Name->GetText().EqualTo(Weapon->DisplayName),
						TEXT("attempt refreshes current level and preserves selected weapon"));
					Saves->OnSaveFinished.Broadcast(false);
					Saves->OnSaveFinished.Broadcast(true);
					Check(Save->Money == 10000 - Quote.MoneyCost && Save->ItemUpgradeLevels.FindRef(WeaponId) == Level + 1,
						TEXT("save failure and completion notifications never replay UI attempt"));
				}
				else
				{
					Check(Saves->SelectSlot(TEXT("Phase7WidgetOther")), TEXT("profile switch succeeds"));
					if (First) First->OnClicked().Broadcast();
					Saves->OnSaveFinished.Broadcast(true);
					Check(Name->GetText().ToString().Contains(TEXT("선택")) && List->GetChildrenCount() == 0
						&& Save->ItemUpgradeLevels.FindRef(WeaponId) == Level,
						TEXT("profile change clears target and rejects stale row and save callbacks"));
				}
				Widget->Close();
			}
			else if (Mode == TEXT("Reject"))
			{
				Save->Money = 0;
				Progression->GetWeaponUpgradeQuote(Weapon, Quote);
				FKnightWeaponUpgradeReceipt Receipt;
				Check(Quote.Availability == EKnightWeaponUpgradeAvailability::InsufficientMoney
					&& Progression->TryUpgradeWeapon(Weapon, Receipt) == EKnightWeaponUpgradeOutcome::Rejected
					&& Save->ItemUpgradeLevels.FindRef(WeaponId) == Level && Save->WeaponUpgradeFailures.FindRef(WeaponId) == 2,
					TEXT("insufficient currency rejects without mutation"));
				Save->Money = 10000;
				if (Gate) Save->ItemCounts.Remove(Gate->GetPrimaryAssetId());
				Progression->GetWeaponUpgradeQuote(Weapon, Quote);
				Check(Gate && Quote.Availability == EKnightWeaponUpgradeAvailability::MissingMaterial
					&& Progression->TryUpgradeWeapon(Weapon, Receipt) == EKnightWeaponUpgradeOutcome::Rejected
					&& Save->Money == 10000, TEXT("gateway material shortage rejects without cost"));
				Save->ItemUpgradeLevels.Add(WeaponId, 15);
				Progression->GetWeaponUpgradeQuote(Weapon, Quote);
				Check(Quote.Availability == EKnightWeaponUpgradeAvailability::MaxLevel && Quote.TargetLevel == 0
					&& Progression->TryUpgradeWeapon(Weapon, Receipt) == EKnightWeaponUpgradeOutcome::Rejected,
					TEXT("maximum level is inspectable but cannot upgrade"));
				Save->ItemCounts.Remove(OtherId);
				Progression->GetWeaponUpgradeQuote(Other, Quote);
				Check(Quote.Availability == EKnightWeaponUpgradeAvailability::NotOwned,
					TEXT("unowned weapon cannot be upgraded"));
			}
			else
			{
				const UknightMaintenanceCatalog* const Catalog = UknightEquipmentSettings::LoadCatalog();
				const auto Before = knightEquipmentStats::Calculate(knightEquipmentStats::ReadLoadout(Progression, Catalog), Catalog, Progression);
				const int64 MoneyBefore = Save->Money;
				const int32 GateBefore = Gate ? Save->ItemCounts.FindRef(Gate->GetPrimaryAssetId()) : 0;
				Progression->SetDebugNextWeaponUpgradeOutcome(Mode == TEXT("Failure") ? 0 : 1);
				FKnightWeaponUpgradeReceipt Receipt;
				const EKnightWeaponUpgradeOutcome Outcome = Progression->TryUpgradeWeapon(Weapon, Receipt);
				Check(Outcome == (Mode == TEXT("Failure") ? EKnightWeaponUpgradeOutcome::ChanceFailed : EKnightWeaponUpgradeOutcome::Success),
					TEXT("test-only forced roll produces requested outcome"));
				Check(Save->Money == MoneyBefore - Quote.MoneyCost && Receipt.MoneySpent == Quote.MoneyCost,
					TEXT("one accepted attempt charges exact money once"));
				Check(Save->ItemUpgradeLevels.FindRef(WeaponId) == (Mode == TEXT("Failure") ? Level : Level + 1)
					&& Save->WeaponUpgradeFailures.FindRef(WeaponId) == (Mode == TEXT("Failure") ? 3 : 0),
					TEXT("level and failure compensation follow outcome"));
				Check(!Gate || Save->ItemCounts.FindRef(Gate->GetPrimaryAssetId()) == GateBefore
					- (Mode == TEXT("Failure") ? 0 : Quote.GateMaterialNeeded),
					TEXT("gateway material is consumed only on success"));
				const auto After = knightEquipmentStats::Calculate(knightEquipmentStats::ReadLoadout(Progression, Catalog), Catalog, Progression);
				const float AttackDelta = After.Values.FindRef(UknightCoreAttributeSet::GetAttackPowerAttribute())
					- Before.Values.FindRef(UknightCoreAttributeSet::GetAttackPowerAttribute());
				const float ExpectedDelta = Mode == TEXT("Failure") || Mode == TEXT("SuccessUnequipped") ? 0.f : Quote.NextWeaponAttackIncrease;
				Check(Before.bComplete && After.bComplete && FMath::IsNearlyEqual(AttackDelta, ExpectedDelta, 0.01f),
					TEXT("equipped and unequipped upgrade stat changes follow shared calculation"));
				if (Mode == TEXT("Failure"))
				{
				FKnightWeaponUpgradeQuote Next;
					Progression->GetWeaponUpgradeQuote(Weapon, Next);
					Check(FMath::IsNearlyEqual(Next.EffectiveSuccessChance, FMath::Clamp(
						static_cast<double>(Step.BaseSuccessChance) + 3.0 * Step.FailureBonusPerAttempt, 0.0, 1.0)),
						TEXT("failed roll increases next effective chance"));
				}
				if (Mode == TEXT("SuccessFinal"))
				{
					FKnightWeaponUpgradeQuote Next;
					Progression->GetWeaponUpgradeQuote(Weapon, Next);
					Check(Next.Availability == EKnightWeaponUpgradeAvailability::MaxLevel,
						TEXT("final success reaches maximum level"));
				}
				Saves->OnSaveFinished.Broadcast(true);
				Check(Save->Money == MoneyBefore - Quote.MoneyCost && Save->ItemUpgradeLevels.FindRef(WeaponId)
					== (Mode == TEXT("Failure") ? Level : Level + 1), TEXT("save completion callback does not repeat attempt"));
			}
			if (Failures)
			{
				UE_LOG(LogknightDev, Error, TEXT("Phase7 UpgradeCases SUMMARY mode=%s failures=%d"), *Mode, Failures);
			}
			else
			{
				UE_LOG(LogknightDev, Log, TEXT("Phase7 UpgradeCases SUMMARY mode=%s failures=0"), *Mode);
			}
			return;
		}
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
		TEXT("knight.MaintenanceTest"), TEXT("Grant <asset> <count> | Snapshot | PureCases | AssetCases | MaterialSaleCases Partial|Final | MaterialWidgetCases | UpgradeCases Widget|Reject|Failure|SuccessEquipped|SuccessUnequipped|SuccessFinal | SocketCases Zero|Widget|Effect|Overflow|Reject|Equip|Replace|Unequip|RestoreEquip; requires editor test save namespace."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&MaintenanceTest), ECVF_Cheat);
}
#endif
