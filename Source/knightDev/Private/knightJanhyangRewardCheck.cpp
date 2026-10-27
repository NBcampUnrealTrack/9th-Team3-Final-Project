// Isolated quest reward regression check. This invokes the result path without combat or UI input.
#if WITH_EDITOR
#include "Containers/Ticker.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Item/knightItemDefinitions.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Progression/knightProgressionSubsystem.h"
#include "Progression/knightSaveGame.h"
#include "Progression/knightSaveSubsystem.h"
#include "Quest/KnightQuestData.h"
#include "Raid/knightRaidSubsystem.h"

namespace
{
struct FJanhyangRewardCheck
{
	TWeakObjectPtr<UWorld> World;
	FString Mode;
	int32 Assertions = 0;
	int32 Failures = 0;
	int32 Step = 0;
	double Deadline = 0;
	FknightRaidResult FirstJanhyangResult;

	void Check(bool bPassed, const TCHAR* Label)
	{
		++Assertions;
		if (!bPassed) ++Failures;
		UE_LOG(LogTemp, Display, TEXT("JanhyangRewardCheck %s %s: %s"), *Mode,
			bPassed ? TEXT("PASS") : TEXT("FAIL"), Label);
	}

	bool Finish()
	{
		UE_LOG(LogTemp, Display, TEXT("JanhyangRewardCheck COMPLETE mode=%s assertions=%d failures=%d; result API only, no combat or interactive UI"),
			*Mode, Assertions, Failures);
		FPlatformMisc::RequestExit(Failures > 0);
		return false;
	}

	FknightRaidResult Record(UknightRaidSubsystem& Raid, const FString& Slot, FName QuestId,
		FName StageId, FName BossId, bool bSuccess, EknightRaidFailReason FailReason,
		const FGameplayTagContainer& BrokenParts = FGameplayTagContainer())
	{
		FknightRaidResult Result;
		Result.RunId = Raid.BeginRun(StageId, QuestId, BossId, EKnightQuestDifficulty::Normal, Slot);
		Result.StageId = StageId;
		Result.bSuccess = bSuccess;
		Result.FailReason = FailReason;
		Result.BrokenParts = BrokenParts;
		Check(Raid.RecordResult(Result), TEXT("result accepted by raid subsystem"));
		Raid.GetLastResult(Result);
		Raid.EndRun();
		return Result;
	}

	bool Tick(float)
	{
		if (FPlatformTime::Seconds() > Deadline)
		{
			Check(false, TEXT("bounded check timeout"));
			return Finish();
		}
		UGameInstance* const GI = World.IsValid() ? World->GetGameInstance() : nullptr;
		if (!GI) { Check(false, TEXT("game instance exists")); return Finish(); }
		auto* const Saves = GI->GetSubsystem<UknightSaveSubsystem>();
		auto* const Progress = GI->GetSubsystem<UknightProgressionSubsystem>();
		auto* const Raid = GI->GetSubsystem<UknightRaidSubsystem>();
		const UKnightQuestCatalog* const Catalog = UKnightQuestSettings::LoadCatalog();
		if (!Saves || !Progress || !Raid || !Catalog)
		{
			Check(false, TEXT("reward dependencies loaded"));
			return Finish();
		}
		if (Saves->IsSaveInFlight()) return true;
		if (Saves->IsSaveRecoveryPending())
		{
			Check(false, TEXT("isolated save completed"));
			return Finish();
		}
		const auto* const Janyeol = Catalog->FindQuest(TEXT("Quest_0001"));
		const auto* const Janhyang = Catalog->FindQuest(TEXT("Quest_0002"));
		const auto* const JanyeolHide = Cast<UknightMaterialItemDefinition>(
			Janyeol && !Janyeol->ClearRewards.IsEmpty() ? Janyeol->ClearRewards[0].Item : nullptr);
		const auto* const JanhyangHide = Cast<UknightMaterialItemDefinition>(
			Janhyang && !Janhyang->ClearRewards.IsEmpty() ? Janhyang->ClearRewards[0].Item : nullptr);
		const auto* const JanhyangBone = Cast<UknightMaterialItemDefinition>(
			Janhyang && Janhyang->ClearRewards.Num() > 1 ? Janhyang->ClearRewards[1].Item : nullptr);
		const auto* const ChestItem = Cast<UknightMaterialItemDefinition>(
			Janhyang && Janhyang->PartBreakRewards.Num() > 0 ? Janhyang->PartBreakRewards[0].Reward.Item : nullptr);
		const auto* const TailItem = Cast<UknightMaterialItemDefinition>(
			Janhyang && Janhyang->PartBreakRewards.Num() > 1 ? Janhyang->PartBreakRewards[1].Reward.Item : nullptr);
		if (!Janyeol || !Janhyang || !JanyeolHide || !JanhyangHide || !JanhyangBone || !ChestItem || !TailItem)
		{
			Check(false, TEXT("both quest reward definitions load"));
			return Finish();
		}
		if (Mode == TEXT("Restore"))
		{
			Check(Saves->DoesSlotExist(TEXT("Gate")) && Saves->SelectSlot(TEXT("Gate")), TEXT("isolated slot reloaded in a new process"));
			const UknightSaveGame* const Save = Saves->GetSaveGame();
			Check(Progress->IsQuestCompleted(TEXT("Quest_0001")) && Progress->IsQuestCompleted(TEXT("Quest_0002")), TEXT("both completion records persisted"));
			Check(Progress->GetItemCount(JanyeolHide) >= 2 && Progress->GetItemCount(JanhyangHide) >= 4
				&& Progress->GetItemCount(JanhyangBone) >= 2, TEXT("Janyeol and Janhyang clear inventory persisted"));
			Check(Progress->GetItemCount(ChestItem) == 1 && Progress->GetItemCount(TailItem) == 1,
				TEXT("distinct Janhyang chest and tail payouts persisted"));
			Check(Progress->GetMoney() == Janyeol->MoneyReward + 2 * Janhyang->MoneyReward,
				TEXT("repeat money persisted without replay"));
			Check(Save && Save->AppliedRaidRuns.Num() == 5 && Save->FirstDefeatedBossIds.Contains(TEXT("Janhyang")),
				TEXT("five run receipts and first boss flag persisted"));
			const int32 Before = Progress->GetItemCount(JanhyangHide);
			const int64 Money = Progress->GetMoney();
			if (Save && !Save->AppliedRaidRuns.IsEmpty())
			{
				FknightRaidResult Duplicate;
				Duplicate.RunId = *Save->AppliedRaidRuns.CreateConstIterator();
				Duplicate.StageId = TEXT("Stage_0002");
				Duplicate.bSuccess = true;
				Raid->OnRaidResultRecorded.Broadcast(Duplicate);
			}
			Check(Progress->GetItemCount(JanhyangHide) == Before && Progress->GetMoney() == Money,
				TEXT("persisted run receipt rejects late duplicate result"));
			return Finish();
		}
		if (Step++ == 0)
		{
			Check(!Saves->DoesSlotExist(TEXT("Gate")) && Saves->SelectSlot(TEXT("Gate")), TEXT("fresh isolated slot selected"));
			Check(Janhyang->QuestId == TEXT("Quest_0002") && Janhyang->StageId == TEXT("Stage_0002")
				&& Janhyang->BossId == TEXT("Janhyang") && Janhyang->bRepeatable,
				TEXT("Janhyang quest identity and repeat policy"));
			Check(Janhyang->PartBreakRewards.Num() == 2
				&& Janhyang->PartBreakRewards[0].Reward.RequiredBrokenPart == FGameplayTag::RequestGameplayTag(TEXT("Monster.Part.Janhyang.Chest"))
				&& Janhyang->PartBreakRewards[1].Reward.RequiredBrokenPart == FGameplayTag::RequestGameplayTag(TEXT("Monster.Part.Janhyang.Tail")),
				TEXT("actual breakable Janhyang chest and tail tags connected"));
			const FString Slot = Saves->GetActiveSlotId();
			Record(*Raid, Slot, Janyeol->QuestId, Janyeol->StageId, Janyeol->BossId, true, EknightRaidFailReason::None);
			Check(Progress->GetItemCount(JanyeolHide) >= 2 && Progress->IsQuestCompleted(Janyeol->QuestId),
				TEXT("working Janyeol quest still grants material and completion"));
			const int64 MoneyBefore = Progress->GetMoney();
			Record(*Raid, Slot, Janhyang->QuestId, Janhyang->StageId, Janhyang->BossId, false, EknightRaidFailReason::KnightDestroyed);
			Record(*Raid, Slot, Janhyang->QuestId, Janhyang->StageId, Janhyang->BossId, false, EknightRaidFailReason::Abandoned);
			Check(Progress->GetItemCount(JanhyangHide) == 0 && Progress->GetItemCount(JanhyangBone) == 0
				&& Progress->GetMoney() == MoneyBefore && !Progress->IsQuestCompleted(Janhyang->QuestId),
				TEXT("failure and abandonment grant no Janhyang rewards"));
			FirstJanhyangResult = Record(*Raid, Slot, Janhyang->QuestId, Janhyang->StageId, Janhyang->BossId, true, EknightRaidFailReason::None);
			Check(Progress->GetItemCount(JanhyangHide) >= 2 && Progress->GetItemCount(JanhyangBone) >= 1
				&& Progress->IsQuestCompleted(Janhyang->QuestId), TEXT("Janhyang successful clear grants its own guaranteed drops"));
			Check(FirstJanhyangResult.MoneyEarned == Janhyang->MoneyReward
				&& FirstJanhyangResult.FirstBossSocketSlotsEarned == Janhyang->FirstBossSocketSlots,
				TEXT("actual settled result has configured money and first boss slot"));
			bool bOnlyJanhyangItems = !FirstJanhyangResult.Rewards.IsEmpty();
			for (const FknightItemStack& Stack : FirstJanhyangResult.Rewards)
			{
				const auto* Material = Cast<UknightMaterialItemDefinition>(Stack.Item);
				bOnlyJanhyangItems &= Material && Material->SourceMonster == Janhyang->BossDefinition
					&& Progress->GetItemCount(Material) >= Stack.Count;
			}
			Check(bOnlyJanhyangItems, TEXT("settled result lists only actual granted Janhyang materials"));
			const auto Before = Saves->GetSaveGame()->ItemCounts;
			const int64 BeforeMoney = Progress->GetMoney();
			Check(!Raid->RecordResult(FirstJanhyangResult), TEXT("same-run result callback rejected"));
			Raid->OnRaidResultRecorded.Broadcast(FirstJanhyangResult);
			Check(Progress->GetMoney() == BeforeMoney && Before.OrderIndependentCompareEqual(Saves->GetSaveGame()->ItemCounts),
				TEXT("duplicate result broadcast does not reroll or pay again"));
			FGameplayTagContainer Parts;
			Parts.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Monster.Part.Janhyang.Chest")));
			Parts.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Monster.Part.Janhyang.Tail")));
			const int32 JanyeolCount = Progress->GetItemCount(JanyeolHide);
			const FknightRaidResult Repeat = Record(*Raid, Slot, Janhyang->QuestId, Janhyang->StageId, Janhyang->BossId,
				true, EknightRaidFailReason::None, Parts);
			Check(Progress->GetItemCount(ChestItem) == 1 && Progress->GetItemCount(TailItem) == 1,
				TEXT("repeat success with chest and tail records grants each configured part item once"));
			Check(Repeat.FirstBossSocketSlotsEarned == 0 && Repeat.MoneyEarned == Janhyang->MoneyReward
				&& Progress->GetItemCount(JanyeolHide) == JanyeolCount,
				TEXT("repeat clear pays normal reward without duplicate first slot or Janyeol items"));
			return true;
		}
		Check(Saves->SelectSlot(TEXT("Other")) && Progress->GetItemCount(JanhyangHide) == 0,
			TEXT("other profile has no Janhyang inventory"));
		Check(Saves->SelectSlot(TEXT("Gate")) && Progress->GetItemCount(JanhyangHide) >= 4,
			TEXT("original profile retains Janhyang inventory"));
		return Finish();
	}
};

void StartJanhyangRewardCheck(const TArray<FString>& Args, UWorld* World)
{
	FString Namespace, UserDir;
	if (!World || Args.Num() != 1 || (Args[0] != TEXT("Seed") && Args[0] != TEXT("Restore"))
		|| !FParse::Value(FCommandLine::Get(), TEXT("KnightSaveTestNamespace="), Namespace)
		|| !Namespace.StartsWith(TEXT("JanhyangDrop"))
		|| !FParse::Value(FCommandLine::Get(), TEXT("UserDir="), UserDir)
		|| !UserDir.Contains(TEXT("KnightJanhyangDrop")))
	{
		UE_LOG(LogTemp, Error, TEXT("JanhyangRewardCheck requires isolated KnightJanhyangDrop UserDir and JanhyangDrop namespace"));
		return;
	}
	auto Run = MakeShared<FJanhyangRewardCheck>();
	Run->World = World;
	Run->Mode = Args[0];
	Run->Deadline = FPlatformTime::Seconds() + 120;
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Run](float Delta) { return Run->Tick(Delta); }), .1f);
}

FAutoConsoleCommandWithWorldAndArgs Command(TEXT("knight.JanhyangRewardCheck"),
	TEXT("Isolated Janhyang quest reward regression check: Seed or Restore."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&StartJanhyangRewardCheck), ECVF_Cheat);
}
#endif
