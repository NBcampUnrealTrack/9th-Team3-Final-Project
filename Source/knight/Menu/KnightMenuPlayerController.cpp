#include "KnightMenuPlayerController.h"
#include "Camera/knightPlayerCameraManager.h"

#include "KnightMenuGameMode.h"
#include "UI/KnightMainMenuWidget.h"
#include "UI/KnightMenuWidgetBase.h"
#include "UI/KnightTitleMenuWidget.h"
#include "GameFlow/KnightGameFlowSubsystem.h"
#include "Progression/knightSaveGame.h"
#include "Progression/knightSaveSubsystem.h"
#include "Quest/KnightQuestData.h"
#include "Raid/knightRaidSubsystem.h"
#include "knightItemDefinition.h"
#include "knightInventoryLibrary.h"
#include "Item/knightItemDefinitions.h"
#include "Item/knightEquipmentData.h"
#include "Item/knightEquipmentStats.h"
#include "AbilitySystem/Attributes/knightCoreAttributeSet.h"
#include "Progression/knightProgressionSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "TimerManager.h"
#include "knight.h"

AKnightMenuPlayerController::AKnightMenuPlayerController()
{
	PlayerCameraManagerClass = AknightPlayerCameraManager::StaticClass();
	bShowMouseCursor = true;
	bEnableClickEvents = true;
	bEnableMouseOverEvents = true;
}

void AKnightMenuPlayerController::BeginPlay()
{
	Super::BeginPlay();
	if (!IsLocalController() || MenuWidget)
	{
		return;
	}

	const AKnightMenuGameMode* const MenuMode = GetWorld() ? Cast<AKnightMenuGameMode>(GetWorld()->GetAuthGameMode()) : nullptr;
	if (!MenuMode)
	{
		UE_LOG(Logknight, Error, TEXT("Menu PlayerController started without a Knight menu GameMode."));
		return;
	}

	const EKnightFlowState MenuState = MenuMode->GetMenuFlowState();
	UKnightGameFlowSubsystem* const Flow = GetGameInstance() ? GetGameInstance()->GetSubsystem<UKnightGameFlowSubsystem>() : nullptr;
	if (!Flow || (MenuState != EKnightFlowState::Title && MenuState != EKnightFlowState::MainMenu)
		|| Flow->GetCurrentFlowState() != MenuState)
	{
		UE_LOG(Logknight, Error, TEXT("Menu PlayerController cannot create UI: GameMode flow state was not initialized correctly."));
		return;
	}

	UClass* const WidgetClass = MenuMode->GetMenuWidgetClass();
	if (!WidgetClass)
	{
		UE_LOG(Logknight, Warning, TEXT("%s has no MenuWidgetClass yet; no menu UI was created."), *MenuMode->GetClass()->GetName());
		return;
	}
	const bool bCorrectWidgetType = MenuState == EKnightFlowState::Title
		? WidgetClass->IsChildOf(UKnightTitleMenuWidget::StaticClass())
		: WidgetClass->IsChildOf(UKnightMainMenuWidget::StaticClass());
	if (!bCorrectWidgetType)
	{
		UE_LOG(Logknight, Error, TEXT("%s has an incompatible MenuWidgetClass: %s."),
			*MenuMode->GetClass()->GetName(), *WidgetClass->GetName());
		return;
	}

	MenuWidget = CreateWidget<UKnightMenuWidgetBase>(this, WidgetClass);
	if (!MenuWidget)
	{
		UE_LOG(Logknight, Error, TEXT("Failed to create menu widget %s."), *WidgetClass->GetName());
		return;
	}
	MenuWidget->AddToViewport();
	ApplyMenuInputMode();
	GetWorldTimerManager().SetTimerForNextTick(this, &ThisClass::ApplyMenuInputMode);
}

void AKnightMenuPlayerController::ApplyMenuInputMode()
{
	if (!IsLocalController() || !MenuWidget || !MenuWidget->IsInViewport()) return;
	FInputModeUIOnly InputMode;
	InputMode.SetWidgetToFocus(MenuWidget->TakeWidget());
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(InputMode);
	bShowMouseCursor = true;
	MenuWidget->FocusCurrentPage();
}

void AKnightMenuPlayerController::KnightCoreUseFixedSlot(int32 SlotNumber)
{
#if WITH_EDITOR
	UKnightTitleMenuWidget* const TitleWidget = Cast<UKnightTitleMenuWidget>(MenuWidget);
	if (!TitleWidget || !TitleWidget->ShowPage(EKnightTitlePage::SaveSelect)) return;
	TitleWidget->SelectFixedSlot(SlotNumber);
	const UknightSaveSubsystem* const Save = GetGameInstance()->GetSubsystem<UknightSaveSubsystem>();
	UE_LOG(Logknight, Log, TEXT("KnightCoreUseFixedSlot slot=%d active=%s loadFailed=%d busy=%d"), SlotNumber,
		Save ? *Save->GetActiveSlotId() : TEXT("none"), Save && Save->DidLoadFail(), Save && Save->IsSaveInFlight());
#endif
}

void AKnightMenuPlayerController::KnightCoreReturnTitle()
{
#if WITH_EDITOR
	UKnightGameFlowSubsystem* const Flow = GetGameInstance()->GetSubsystem<UKnightGameFlowSubsystem>();
	UE_LOG(Logknight, Log, TEXT("KnightCoreReturnTitle accepted=%d"), Flow && Flow->RequestOpenTitle());
#endif
}

void AKnightMenuPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (MenuWidget)
	{
		MenuWidget->RemoveFromParent();
		MenuWidget = nullptr;
	}
	Super::EndPlay(EndPlayReason);
}

void AKnightMenuPlayerController::KnightCoreLaunchTwice(FName StageId)
{
#if WITH_EDITOR
	UKnightGameFlowSubsystem* const Flow = GetGameInstance()->GetSubsystem<UKnightGameFlowSubsystem>();
	const bool bFirst = Flow && Flow->RequestStartStage(StageId);
	const bool bSecond = Flow && Flow->RequestStartStage(StageId);
	UE_LOG(Logknight, Log, TEXT("KnightCoreLaunchTwice stage=%s first=%d second=%d pending=%d"),
		*StageId.ToString(), bFirst, bSecond, Flow && Flow->IsTravelPending());
#endif
}

void AKnightMenuPlayerController::KnightCoreLaunch(FName StageId)
{
#if WITH_EDITOR
	UKnightGameFlowSubsystem* const Flow = GetGameInstance()->GetSubsystem<UKnightGameFlowSubsystem>();
	UE_LOG(Logknight, Log, TEXT("KnightCoreLaunch stage=%s accepted=%d"),
		*StageId.ToString(), Flow && Flow->RequestStartStage(StageId));
#endif
}

void AKnightMenuPlayerController::KnightCoreLaunchQuestTwice(FName QuestId)
{
#if WITH_EDITOR
	UKnightGameFlowSubsystem* const Flow = GetGameInstance()->GetSubsystem<UKnightGameFlowSubsystem>();
	FText FirstReason;
	FText SecondReason;
	const bool bFirst = Flow && Flow->RequestStartQuest(QuestId, FirstReason);
	const bool bSecond = Flow && Flow->RequestStartQuest(QuestId, SecondReason);
	UE_LOG(Logknight, Log, TEXT("KnightCoreLaunchQuestTwice quest=%s first=%d second=%d pending=%d firstReason=%s secondReason=%s"),
		*QuestId.ToString(), bFirst, bSecond, Flow && Flow->IsTravelPending(),
		*FirstReason.ToString(), *SecondReason.ToString());
#endif
}

void AKnightMenuPlayerController::KnightCoreLaunchQuest(FName QuestId)
{
#if WITH_EDITOR
	UKnightGameFlowSubsystem* const Flow = GetGameInstance()->GetSubsystem<UKnightGameFlowSubsystem>();
	FText Reason;
	const bool bAccepted = Flow && Flow->RequestStartQuest(QuestId, Reason);
	UE_LOG(Logknight, Log, TEXT("KnightCoreLaunchQuest quest=%s accepted=%d pending=%d reason=%s"),
		*QuestId.ToString(), bAccepted, Flow && Flow->IsTravelPending(), *Reason.ToString());
#endif
}

void AKnightMenuPlayerController::KnightCoreQuestStatus()
{
#if WITH_EDITOR
	const UKnightGameFlowSubsystem* const Flow = GetGameInstance()->GetSubsystem<UKnightGameFlowSubsystem>();
	const UknightRaidSubsystem* const Raid = GetGameInstance()->GetSubsystem<UknightRaidSubsystem>();
	FKnightQuestRunContext Context;
	const bool bHasContext = Raid && Raid->GetActiveQuestRunContext(Context);
	UE_LOG(Logknight, Log, TEXT("KnightCoreQuestStatus flow=%d pending=%d hasContext=%d quest=%s stage=%s boss=%s difficulty=%d run=%s"),
		Flow ? static_cast<int32>(Flow->GetCurrentFlowState()) : -1, Flow && Flow->IsTravelPending(), bHasContext,
		*Context.QuestId.ToString(), *Context.StageId.ToString(), *Context.BossId.ToString(),
		static_cast<int32>(Context.Difficulty), *Context.RunId.ToString());
#endif
}

void AKnightMenuPlayerController::KnightCoreFailNextRaidLoad()
{
#if WITH_EDITOR
	if (UKnightGameFlowSubsystem* const Flow = GetGameInstance()->GetSubsystem<UKnightGameFlowSubsystem>())
	{
		Flow->DebugFailNextRaidLoad();
		UE_LOG(Logknight, Log, TEXT("KnightCoreFailNextRaidLoad armed"));
	}
#endif
}

void AKnightMenuPlayerController::KnightCoreProfileStatus()
{
#if WITH_EDITOR
	const UknightSaveSubsystem* const Saves = GetGameInstance()->GetSubsystem<UknightSaveSubsystem>();
	const UknightSaveGame* const Save = Saves ? Saves->GetSaveGame() : nullptr;
	const int32 MaterialCount = Save ? Save->ItemCounts.FindRef(FPrimaryAssetId(TEXT("Item"), TEXT("DA_CoreTestMaterial"))) : -1;
	UE_LOG(Logknight, Log, TEXT("KnightCoreProfileStatus slot=%s exists=%d loadFailed=%d busy=%d material=%d upgrades=%d unlocks=%d runs=%d"),
		Saves ? *Saves->GetActiveSlotId() : TEXT("None"), Saves && Saves->DoesSlotExist(Saves->GetActiveSlotId()),
		Saves && Saves->DidLoadFail(), Saves && Saves->IsSaveInFlight(), MaterialCount,
		Save ? Save->ItemUpgradeLevels.Num() : -1, Save ? Save->UnlockedStages.Num() : -1,
		Save ? Save->AppliedRaidRuns.Num() : -1);
#endif
}

namespace
{
	bool IsIsolatedRewardProbe()
	{
#if WITH_EDITOR
		FString Namespace;
		return FParse::Value(FCommandLine::Get(), TEXT("KnightSaveTestNamespace="), Namespace) && !Namespace.IsEmpty();
#else
		return false;
#endif
	}
}

void AKnightMenuPlayerController::KnightCoreRewardTestSlot(int32 SlotNumber)
{
#if WITH_EDITOR
	if (!IsIsolatedRewardProbe()) return;
	UknightSaveSubsystem* const Saves = GetGameInstance()->GetSubsystem<UknightSaveSubsystem>();
	const bool bSelected = Saves && Saves->SelectFixedSlot(SlotNumber);
	UE_LOG(Logknight, Log, TEXT("KnightCoreRewardTestSlot requested=%d selected=%d active=%s"), SlotNumber,
		bSelected, Saves ? *Saves->GetActiveSlotId() : TEXT("None"));
#endif
}

void AKnightMenuPlayerController::KnightCoreRewardFailNextSaves(int32 Count)
{
#if WITH_EDITOR
	if (!IsIsolatedRewardProbe()) return;
	if (UknightSaveSubsystem* const Saves = GetGameInstance()->GetSubsystem<UknightSaveSubsystem>())
	{
		Saves->DebugFailNextSaves(Count);
		UE_LOG(Logknight, Log, TEXT("KnightCoreRewardFailNextSaves count=%d"), Count);
	}
#endif
}

void AKnightMenuPlayerController::KnightCoreRewardResult(FName QuestId, int32 bSuccess, int32 bHead, int32 bTail)
{
#if WITH_EDITOR
	if (!IsIsolatedRewardProbe()) return;
	UknightSaveSubsystem* const Saves = GetGameInstance()->GetSubsystem<UknightSaveSubsystem>();
	UknightRaidSubsystem* const Raid = GetGameInstance()->GetSubsystem<UknightRaidSubsystem>();
	const UKnightQuestCatalog* const Catalog = UKnightQuestSettings::LoadCatalog();
	const UKnightQuestDefinition* const Quest = Catalog ? Catalog->FindQuest(QuestId) : nullptr;
	if (!Saves || !Saves->HasSelectedSlot() || Saves->IsSaveInFlight() || !Raid || !Quest
		|| !Quest->bContentEnabled || Quest->StageId.IsNone() || Quest->BossId.IsNone()
		|| Raid->GetCurrentRunId().IsValid())
	{
		UE_LOG(Logknight, Error, TEXT("KnightCoreRewardResult refused quest=%s"), *QuestId.ToString());
		return;
	}
	FknightRaidResult Result;
	Result.RunId = Raid->BeginRun(Quest->StageId, Quest->QuestId, Quest->BossId, Quest->Difficulty,
		Saves->GetActiveSlotId());
	const int32 OtherSlot = Saves->GetActiveFixedSlotNumber() == 1 ? 2 : 1;
	const bool bCrossSlotAccepted = Saves->SelectFixedSlot(OtherSlot);
	UE_LOG(Logknight, Log, TEXT("KnightCoreRewardResult active-run cross-slot switch accepted=%d"), bCrossSlotAccepted);
	Result.StageId = Quest->StageId;
	Result.bSuccess = bSuccess != 0;
	for (const FKnightQuestPartBreakReward& Entry : Quest->PartBreakRewards)
	{
		if ((Entry.Part == EKnightQuestBreakPart::Head && bHead != 0)
			|| (Entry.Part == EKnightQuestBreakPart::Tail && bTail != 0))
		{
			Result.BrokenParts.AddTag(Entry.Reward.RequiredBrokenPart);
		}
	}
	const bool bAccepted = Raid->RecordResult(Result);
	const bool bDuplicateAccepted = Raid->RecordResult(Result);
	FknightRaidResult Settled;
	Raid->GetLastResult(Settled);
	UE_LOG(Logknight, Log, TEXT("KnightCoreRewardResult quest=%s run=%s success=%d head=%d tail=%d accepted=%d duplicateAccepted=%d itemKinds=%d money=%lld sockets=%d"),
		*QuestId.ToString(), *Result.RunId.ToString(), Result.bSuccess, bHead, bTail, bAccepted, bDuplicateAccepted,
		Settled.Rewards.Num(), Settled.MoneyEarned, Settled.FirstBossSocketSlotsEarned);
	for (const FknightItemStack& Stack : Settled.Rewards)
	{
		UE_LOG(Logknight, Log, TEXT("KnightCoreRewardItem item=%s count=%d"),
			Stack.Item ? *Stack.Item->GetPrimaryAssetId().ToString() : TEXT("None"), Stack.Count);
	}
	Raid->EndRun();
#endif
}

void AKnightMenuPlayerController::KnightCoreRewardStatus()
{
#if WITH_EDITOR
	if (!IsIsolatedRewardProbe()) return;
	const UknightSaveSubsystem* const Saves = GetGameInstance()->GetSubsystem<UknightSaveSubsystem>();
	const UknightSaveGame* const Save = Saves && Saves->HasSelectedSlot() ? Saves->GetSaveGame() : nullptr;
	if (!Save) return;
	UE_LOG(Logknight, Log, TEXT("KnightCoreRewardStatus slot=%s money=%lld sockets=%d firstBosses=%d completedQuests=%d appliedRuns=%d saveBusy=%d"),
		*Saves->GetActiveSlotId(), Save->Money, Save->UnlockedSharedSocketSlots,
		Save->FirstDefeatedBossIds.Num(), Save->CompletedQuestIds.Num(), Save->AppliedRaidRuns.Num(), Saves->IsSaveInFlight());
	for (const TPair<FPrimaryAssetId, int32>& Entry : Save->ItemCounts)
	{
		UE_LOG(Logknight, Log, TEXT("KnightCoreRewardOwned item=%s count=%d"), *Entry.Key.ToString(), Entry.Value);
	}
#endif
}

void AKnightMenuPlayerController::KnightCoreSaleGrant(FName ItemName, int32 Count)
{
#if WITH_EDITOR
	if (!IsIsolatedRewardProbe() || Count <= 0) return;
	const FPrimaryAssetId ItemId(UknightItemDefinition::ItemAssetType, ItemName);
	const UknightMaterialItemDefinition* const Material = Cast<UknightMaterialItemDefinition>(
		UknightInventoryLibrary::LoadItemDefinition(ItemId));
	UknightSaveSubsystem* const Saves = GetGameInstance()->GetSubsystem<UknightSaveSubsystem>();
	UknightProgressionSubsystem* const Progression = GetGameInstance()->GetSubsystem<UknightProgressionSubsystem>();
	if (!Material || !Saves || !Saves->CanStartAtomicTransaction() || !Progression) return;
	Progression->AddItem(Material, Count);
	UE_LOG(Logknight, Log, TEXT("KnightCoreSaleGrant item=%s count=%d owned=%d"),
		*ItemId.ToString(), Count, Progression->GetItemCount(Material));
#endif
}

void AKnightMenuPlayerController::KnightCoreSaleAttempt(FName ItemName, int32 Count)
{
#if WITH_EDITOR
	if (!IsIsolatedRewardProbe()) return;
	UknightProgressionSubsystem* const Progression = GetGameInstance()->GetSubsystem<UknightProgressionSubsystem>();
	FKnightMaterialSaleReceipt Receipt;
	const FPrimaryAssetId ItemId(UknightItemDefinition::ItemAssetType, ItemName);
	const bool bSold = Progression && Progression->SellMaterial(ItemId, Count, Receipt);
	UE_LOG(Logknight, Log, TEXT("KnightCoreSaleAttempt item=%s requested=%d accepted=%d count=%d unit=%d total=%lld money=%lld"),
		*ItemId.ToString(), Count, bSold, Receipt.Count, Receipt.UnitPrice, Receipt.TotalMoney,
		Progression ? Progression->GetMoney() : 0);
#endif
}

void AKnightMenuPlayerController::KnightCoreSaleRetrySave()
{
#if WITH_EDITOR
	if (!IsIsolatedRewardProbe()) return;
	if (UknightSaveSubsystem* const Saves = GetGameInstance()->GetSubsystem<UknightSaveSubsystem>())
	{
		Saves->RequestSave();
		UE_LOG(Logknight, Log, TEXT("KnightCoreSaleRetrySave slot=%s busy=%d recovery=%d"),
			*Saves->GetActiveSlotId(), Saves->IsSaveInFlight(), Saves->IsSaveRecoveryPending());
	}
#endif
}

void AKnightMenuPlayerController::KnightCoreUpgradeGrant(FName ItemName, int32 Count)
{
#if WITH_EDITOR
	if (!IsIsolatedRewardProbe() || Count <= 0) return;
	UknightSaveSubsystem* Saves = GetGameInstance()->GetSubsystem<UknightSaveSubsystem>();
	UknightProgressionSubsystem* Progression = GetGameInstance()->GetSubsystem<UknightProgressionSubsystem>();
	const FPrimaryAssetId Id(UknightItemDefinition::ItemAssetType, ItemName);
	const UknightItemDefinition* Item = UknightInventoryLibrary::LoadItemDefinition(Id);
	if (!Saves || !Saves->CanStartAtomicTransaction() || !Progression || !Item) return;
	Progression->AddItem(Item, Count);
	UE_LOG(Logknight, Log, TEXT("KnightCoreUpgradeGrant item=%s count=%d owned=%d"),
		*Id.ToString(), Count, Progression->GetItemCount(Item));
#endif
}

void AKnightMenuPlayerController::KnightCoreUpgradeMoney(int32 Amount)
{
#if WITH_EDITOR
	if (!IsIsolatedRewardProbe() || Amount <= 0) return;
	UknightSaveSubsystem* Saves = GetGameInstance()->GetSubsystem<UknightSaveSubsystem>();
	UknightProgressionSubsystem* Progression = GetGameInstance()->GetSubsystem<UknightProgressionSubsystem>();
	const bool bAdded = Saves && Saves->CanStartAtomicTransaction() && Progression && Progression->AddMoney(Amount);
	UE_LOG(Logknight, Log, TEXT("KnightCoreUpgradeMoney amount=%d added=%d money=%lld"), Amount, bAdded,
		Progression ? Progression->GetMoney() : 0);
#endif
}

void AKnightMenuPlayerController::KnightCoreUpgradeForceNext(int32 bSuccess)
{
#if WITH_EDITOR
	if (!IsIsolatedRewardProbe()) return;
	if (UknightProgressionSubsystem* Progression = GetGameInstance()->GetSubsystem<UknightProgressionSubsystem>())
		Progression->SetDebugNextWeaponUpgradeOutcome(bSuccess != 0 ? 1 : 0);
#endif
}

void AKnightMenuPlayerController::KnightCoreUpgradeAttempt(FName WeaponName)
{
#if WITH_EDITOR
	if (!IsIsolatedRewardProbe()) return;
	UknightProgressionSubsystem* Progression = GetGameInstance()->GetSubsystem<UknightProgressionSubsystem>();
	const FPrimaryAssetId Id(UknightItemDefinition::ItemAssetType, WeaponName);
	const UknightWeaponItemDefinition* Weapon = Cast<UknightWeaponItemDefinition>(UknightInventoryLibrary::LoadItemDefinition(Id));
	FKnightWeaponUpgradeReceipt Receipt;
	const EKnightWeaponUpgradeOutcome Outcome = Progression && Weapon
		? Progression->TryUpgradeWeapon(Weapon, Receipt) : EKnightWeaponUpgradeOutcome::Rejected;
	UE_LOG(Logknight, Log, TEXT("KnightCoreUpgradeAttempt item=%s outcome=%d level=%d->%d moneySpent=%lld materialSpent=%d money=%lld failures=%d"),
		*Id.ToString(), static_cast<int32>(Outcome), Receipt.PreviousLevel, Receipt.NewLevel, Receipt.MoneySpent,
		Receipt.MaterialConsumed, Progression ? Progression->GetMoney() : 0,
		Progression && Weapon ? Progression->GetWeaponUpgradeFailures(Weapon) : -1);
#endif
}

void AKnightMenuPlayerController::KnightCoreUpgradeStatus(FName WeaponName)
{
#if WITH_EDITOR
	if (!IsIsolatedRewardProbe()) return;
	UknightSaveSubsystem* Saves = GetGameInstance()->GetSubsystem<UknightSaveSubsystem>();
	UknightProgressionSubsystem* Progression = GetGameInstance()->GetSubsystem<UknightProgressionSubsystem>();
	const FPrimaryAssetId Id(UknightItemDefinition::ItemAssetType, WeaponName);
	const UknightWeaponItemDefinition* Weapon = Cast<UknightWeaponItemDefinition>(UknightInventoryLibrary::LoadItemDefinition(Id));
	FKnightWeaponUpgradeQuote Quote;
	if (Progression && Weapon) Progression->GetWeaponUpgradeQuote(Weapon, Quote);
	UE_LOG(Logknight, Log, TEXT("KnightCoreUpgradeStatus slot=%s item=%s owned=%d level=%d failures=%d available=%d target=%d cost=%lld gate=%s %d/%d money=%lld attack=%.1f->%.1f busy=%d recovery=%d"),
		Saves ? *Saves->GetActiveSlotId() : TEXT("None"), *Id.ToString(),
		Progression && Weapon ? Progression->GetItemCount(Weapon) : 0,
		Progression && Weapon ? Progression->GetUpgradeLevel(Weapon) : 0,
		Progression && Weapon ? Progression->GetWeaponUpgradeFailures(Weapon) : 0,
		static_cast<int32>(Quote.Availability), Quote.TargetLevel, Quote.MoneyCost,
		Quote.GateMaterial ? *Quote.GateMaterial->GetPrimaryAssetId().ToString() : TEXT("None"),
		Quote.GateMaterialOwned, Quote.GateMaterialNeeded, Progression ? Progression->GetMoney() : 0,
		Quote.CurrentWeaponAttack, Quote.NextWeaponAttack,
		Saves ? Saves->IsSaveInFlight() : 0, Saves ? Saves->IsSaveRecoveryPending() : 0);
#endif
}

void AKnightMenuPlayerController::KnightCoreUpgradeEquip(FName WeaponName)
{
#if WITH_EDITOR
	if (!IsIsolatedRewardProbe()) return;
	UknightProgressionSubsystem* Progression = GetGameInstance()->GetSubsystem<UknightProgressionSubsystem>();
	const FPrimaryAssetId Id(UknightItemDefinition::ItemAssetType, WeaponName);
	const UknightWeaponItemDefinition* Weapon = Cast<UknightWeaponItemDefinition>(UknightInventoryLibrary::LoadItemDefinition(Id));
	const bool bEquipped = Progression && Weapon && Progression->SetEquippedPart(Weapon);
	UE_LOG(Logknight, Log, TEXT("KnightCoreUpgradeEquip item=%s equipped=%d level=%d"),
		*Id.ToString(), bEquipped, Progression && Weapon ? Progression->GetUpgradeLevel(Weapon) : 0);
#endif
}

void AKnightMenuPlayerController::KnightCoreUpgradeCompare(FName WeaponName)
{
#if WITH_EDITOR
	if (!IsIsolatedRewardProbe()) return;
	const UknightProgressionSubsystem* Progression = GetGameInstance()->GetSubsystem<UknightProgressionSubsystem>();
	const UknightMaintenanceCatalog* Catalog = UknightEquipmentSettings::LoadCatalog();
	const FPrimaryAssetId Id(UknightItemDefinition::ItemAssetType, WeaponName);
	const UknightWeaponItemDefinition* Weapon = Cast<UknightWeaponItemDefinition>(UknightInventoryLibrary::LoadItemDefinition(Id));
	if (!Progression || !Catalog || !Weapon) return;
	const auto CurrentLoadout = knightEquipmentStats::ReadLoadout(Progression, Catalog);
	const FknightEquipmentStats Current = knightEquipmentStats::Calculate(CurrentLoadout, Catalog, Progression);
	auto CandidateLoadout = CurrentLoadout;
	CandidateLoadout.Add(Weapon->SlotTag, Weapon);
	const FknightEquipmentStats Preview = knightEquipmentStats::Calculate(CandidateLoadout, Catalog, Progression);
	const FGameplayAttribute Attack = UknightCoreAttributeSet::GetAttackPowerAttribute();
	UE_LOG(Logknight, Log, TEXT("KnightCoreUpgradeCompare item=%s current=%.1f preview=%.1f delta=%.1f complete=%d/%d"),
		*Id.ToString(), Current.Values.FindRef(Attack), Preview.Values.FindRef(Attack),
		Preview.Values.FindRef(Attack) - Current.Values.FindRef(Attack), Current.bComplete, Preview.bComplete);
#endif
}

void AKnightMenuPlayerController::KnightCoreSocketUnlock(int32 Count)
{
#if WITH_EDITOR
	if (!IsIsolatedRewardProbe() || Count <= 0 || Count > 3) return;
	UknightSaveSubsystem* const Saves = GetGameInstance()->GetSubsystem<UknightSaveSubsystem>();
	UknightProgressionSubsystem* const Progression = GetGameInstance()->GetSubsystem<UknightProgressionSubsystem>();
	UknightSaveGame* const Save = Saves && Saves->CanStartAtomicTransaction() ? Saves->GetSaveGame() : nullptr;
	if (!Save || !Progression || Save->UnlockedSharedSocketSlots > 3 - Count) return;
	Save->UnlockedSharedSocketSlots += Count;
	Saves->RequestSave();
	Progression->OnProgressionChanged.Broadcast();
	UE_LOG(Logknight, Log, TEXT("KnightCoreSocketUnlock slot=%s slots=%d"), *Saves->GetActiveSlotId(), Save->UnlockedSharedSocketSlots);
#endif
}

void AKnightMenuPlayerController::KnightCoreSocketEquip(int32 SlotIndex, FName ItemName)
{
#if WITH_EDITOR
	if (!IsIsolatedRewardProbe()) return;
	UknightProgressionSubsystem* const Progression = GetGameInstance()->GetSubsystem<UknightProgressionSubsystem>();
	const FPrimaryAssetId Id(UknightItemDefinition::ItemAssetType, ItemName);
	const UknightSocketItemDefinition* const Item = Cast<UknightSocketItemDefinition>(UknightInventoryLibrary::LoadItemDefinition(Id));
	const bool bAccepted = Progression && Progression->EquipSocketItem(SlotIndex, Item);
	UE_LOG(Logknight, Log, TEXT("KnightCoreSocketEquip slot=%d item=%s accepted=%d owned=%d"), SlotIndex,
		*Id.ToString(), bAccepted, Progression && Item ? Progression->GetItemCount(Item) : 0);
#endif
}

void AKnightMenuPlayerController::KnightCoreSocketUnequip(int32 SlotIndex)
{
#if WITH_EDITOR
	if (!IsIsolatedRewardProbe()) return;
	UknightProgressionSubsystem* const Progression = GetGameInstance()->GetSubsystem<UknightProgressionSubsystem>();
	const bool bAccepted = Progression && Progression->UnequipSocketItem(SlotIndex);
	UE_LOG(Logknight, Log, TEXT("KnightCoreSocketUnequip slot=%d accepted=%d"), SlotIndex, bAccepted);
#endif
}

void AKnightMenuPlayerController::KnightCoreSocketConsume(FName ItemName, int32 Count)
{
#if WITH_EDITOR
	if (!IsIsolatedRewardProbe()) return;
	UknightProgressionSubsystem* const Progression = GetGameInstance()->GetSubsystem<UknightProgressionSubsystem>();
	const FPrimaryAssetId Id(UknightItemDefinition::ItemAssetType, ItemName);
	const UknightSocketItemDefinition* const Item = Cast<UknightSocketItemDefinition>(UknightInventoryLibrary::LoadItemDefinition(Id));
	const bool bAccepted = Progression && Progression->ConsumeItem(Item, Count);
	UE_LOG(Logknight, Log, TEXT("KnightCoreSocketConsume item=%s count=%d accepted=%d owned=%d"), *Id.ToString(), Count,
		bAccepted, Progression && Item ? Progression->GetItemCount(Item) : 0);
#endif
}

void AKnightMenuPlayerController::KnightCoreSocketStatus()
{
#if WITH_EDITOR
	if (!IsIsolatedRewardProbe()) return;
	UknightSaveSubsystem* const Saves = GetGameInstance()->GetSubsystem<UknightSaveSubsystem>();
	UknightProgressionSubsystem* const Progression = GetGameInstance()->GetSubsystem<UknightProgressionSubsystem>();
	if (!Saves || !Progression) return;
	UE_LOG(Logknight, Log, TEXT("KnightCoreSocketStatus slot=%s unlocked=%d kitBonus=%d attackCost=%.3f gaugeCharge=%.3f busy=%d recovery=%d"),
		*Saves->GetActiveSlotId(), Progression->GetUnlockedSharedSocketSlots(), Progression->GetRepairKitCapacityBonus(),
		Progression->GetAttackStaminaCostMultiplier(), Progression->GetSocketGaugeChargeMultiplier(),
		Saves->IsSaveInFlight(), Saves->IsSaveRecoveryPending());
	for (int32 Index = 0; Index < Progression->GetUnlockedSharedSocketSlots(); ++Index)
	{
		const UknightSocketItemDefinition* const Item = Progression->GetEquippedSocketItem(Index);
		UE_LOG(Logknight, Log, TEXT("KnightCoreSocketSlot index=%d item=%s owned=%d available=%d"), Index,
			Item ? *Item->GetPrimaryAssetId().ToString() : TEXT("None"),
			Item ? Progression->GetItemCount(Item) : 0, Item ? Progression->GetAvailableSocketItemCount(Item) : 0);
	}
#endif
}
