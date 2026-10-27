// Isolated default-weapon ownership, transaction and disk-reload regression check.
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "AbilitySystem/Attributes/knightCoreAttributeSet.h"
#include "Containers/Ticker.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "knightCharacter.h"
#include "HAL/IConsoleManager.h"
#include "Item/knightEquipmentData.h"
#include "Item/knightEquipmentStats.h"
#include "Item/knightItemDefinitions.h"
#include "Item/knightPartsComponent.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Progression/knightProgressionSubsystem.h"
#include "Progression/knightSaveGame.h"
#include "Progression/knightSaveSubsystem.h"
#include "System/knightGameplayTags.h"

#if WITH_EDITOR
namespace
{
struct FStarterWeaponCheck
{
	TWeakObjectPtr<UWorld> World;
	bool bReload = false;
	int32 Step = 0, Failures = 0;
	double Deadline = 0;
	float BaseAttack = 0.f;
	TWeakObjectPtr<AknightCharacter> TestPawn;
	void Check(bool OK, const TCHAR* Message)
	{
		Failures += OK ? 0 : 1;
		UE_LOG(LogTemp, Display, TEXT("StarterWeapon %s: %s"), OK ? TEXT("PASS") : TEXT("FAIL"), Message);
	}
	bool Finish()
	{
		UE_LOG(LogTemp, Display, TEXT("StarterWeapon COMPLETE mode=%s failures=%d"), bReload ? TEXT("Reload") : TEXT("Run"), Failures);
		if (FParse::Param(FCommandLine::Get(), TEXT("KnightStarterWeaponAutoExit"))) FPlatformMisc::RequestExit(false);
		return false;
	}
	bool Tick(float)
	{
		if (!World.IsValid() || FPlatformTime::Seconds() > Deadline) { Check(false, TEXT("timeout/world")); return Finish(); }
		auto* GI = World->GetGameInstance();
		auto* Saves = GI ? GI->GetSubsystem<UknightSaveSubsystem>() : nullptr;
		auto* Progression = GI ? GI->GetSubsystem<UknightProgressionSubsystem>() : nullptr;
		auto* PC = World->GetFirstPlayerController();
		if (PC && Saves && Progression && !TestPawn.IsValid())
		{
			// The lobby display robot deliberately has no ASC. Instantiate the real player
			// in this isolated, opt-in check, without entering a raid or activating combat.
			UClass* Class = LoadClass<AknightCharacter>(nullptr, TEXT("/Game/Knight/Blueprints/Character/BP_Knight.BP_Knight_C"));
			FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			TestPawn = Class ? World->SpawnActor<AknightCharacter>(Class, FVector(0, 0, 10000), FRotator::ZeroRotator, Params) : nullptr;
			if (!TestPawn.IsValid()) { Check(false, TEXT("real player fixture spawns")); return Finish(); }
			TestPawn->SetActorEnableCollision(false); TestPawn->GetCharacterMovement()->DisableMovement();
			PC->Possess(TestPawn.Get());
		}
		APawn* Pawn = PC ? PC->GetPawn() : nullptr;
		auto* Parts = Pawn ? Pawn->FindComponentByClass<UknightPartsComponent>() : nullptr;
		auto* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Pawn);
		if (!Saves || !Progression || !Parts || !ASC || Saves->IsSaveInFlight()) return true;
		const auto* Weapon = LoadObject<UknightWeaponItemDefinition>(nullptr, TEXT("/Game/Knight/Character/Knights/Weapons/Danjeol/DA_Weapon_Danjeol.DA_Weapon_Danjeol"));
		if (!Weapon) { Check(false, TEXT("starter definition loads")); return Finish(); }
		const auto Id = Weapon->GetPrimaryAssetId();
		const FGameplayTag Slot = knightGameplayTags::Equipment_Slot_Weapon;
		const auto Attack = UknightCoreAttributeSet::GetAttackPowerAttribute();
		if (Step == 0)
		{
			Check(Saves->SelectSlot(TEXT("DanjeolUpgrade")), TEXT("isolated profile selected"));
			if (!Saves->HasSelectedSlot()) return Finish();
		}
		auto* Save = Saves->GetSaveGame();
		if (!Save) return Finish();
		if (bReload)
		{
			Check(Progression->GetUpgradeLevel(Weapon) == 3 && Progression->GetWeaponUpgradeFailures(Weapon) == 1
				&& Save->Money == 9600, TEXT("fresh process restored level, failure compensation and money"));
			Check(Progression->GetItemCount(Weapon) == 1 && Save->ItemCounts.FindRef(Id) == 1
				&& Progression->GetEquippedPart(Slot) == Weapon, TEXT("fresh process restored one owned/equipped starter"));
			Parts->SyncLoadoutWithSave();
			const float Upgraded = ASC->GetNumericAttribute(Attack);
			Parts->RefreshWeaponUpgrade(Weapon, 0);
			Check(FMath::IsNearlyEqual(Upgraded - ASC->GetNumericAttribute(Attack), Weapon->GetUpgradeAttackBonus(3), .01f),
				TEXT("restored default weapon upgrade reaches real player ASC"));
			Parts->RefreshWeaponUpgrade(Weapon, 3);
			return Finish();
		}
		switch (Step++)
		{
		case 0:
		{
			Save->ItemCounts.Reset(); Save->EquippedItems.Reset(); Save->ExplicitlyEmptySlots.Reset();
			Save->ItemUpgradeLevels.Reset(); Save->WeaponUpgradeFailures.Reset(); Save->Money = 10000;
			Parts->SyncLoadoutWithSave(); BaseAttack = ASC->GetNumericAttribute(Attack);
			TArray<FknightItemStack> Owned;
			Check(Progression->GetOwnedWeapons(Owned) && Owned.Num() == 1 && Owned[0].Item == Weapon && Owned[0].Count == 1,
				TEXT("fresh profile owns its actual default weapon without a grant"));
			FKnightWeaponUpgradeQuote Quote; Progression->GetWeaponUpgradeQuote(Weapon, Quote);
			Check(Save->ItemCounts.IsEmpty() && Save->EquippedItems.IsEmpty() && !Saves->IsSaveInFlight()
				&& Quote.Availability == EKnightWeaponUpgradeAvailability::Ready && Quote.TargetLevel == 1,
				TEXT("queries and quote are read-only; implicit weapon is eligible"));
			Check(FMath::IsNearlyEqual(Quote.CurrentWeaponAttack, 100.f) && FMath::IsNearlyEqual(Quote.NextWeaponAttackIncrease, 1.f),
				TEXT("quote includes real fixed AbilitySet base 100 and upgrade increment 1"));
			FKnightWeaponUpgradeReceipt Receipt;
			Check(Progression->TryUpgradeWeapon(Weapon, Receipt) == EKnightWeaponUpgradeOutcome::Success
				&& Save->Money == 9800 && Progression->GetUpgradeLevel(Weapon) == 1,
				TEXT("real 100-percent first attempt charges once and advances"));
			Check(FMath::IsNearlyEqual(ASC->GetNumericAttribute(Attack) - BaseAttack, Quote.NextWeaponAttackIncrease, .01f),
				TEXT("implicit equipped weapon immediately receives attack increase in ASC"));
			FKnightWeaponUpgradeReceipt Duplicate;
			Check(Progression->TryUpgradeWeapon(Weapon, Duplicate) == EKnightWeaponUpgradeOutcome::Rejected && Save->Money == 9800,
				TEXT("in-flight duplicate request is rejected"));
			break;
		}
		case 1:
		{
			Save->ItemUpgradeLevels.Add(Id, 3); Parts->RefreshWeaponUpgrade(Weapon, 3);
			Progression->SetDebugNextWeaponUpgradeOutcome(0);
			FKnightWeaponUpgradeReceipt Receipt;
			Check(Progression->TryUpgradeWeapon(Weapon, Receipt) == EKnightWeaponUpgradeOutcome::ChanceFailed
				&& Save->Money == 9600 && Progression->GetUpgradeLevel(Weapon) == 3 && Progression->GetWeaponUpgradeFailures(Weapon) == 1,
				TEXT("editor-forced failure keeps level, charges once, records compensation"));
			break;
		}
		case 2:
		{
			Check(Saves->SelectSlot(TEXT("DanjeolOther")), TEXT("separate profile loads")); Parts->SyncLoadoutWithSave();
			Check(Progression->GetUpgradeLevel(Weapon) == 0 && Progression->GetWeaponUpgradeFailures(Weapon) == 0
				&& FMath::IsNearlyEqual(ASC->GetNumericAttribute(Attack), BaseAttack, .01f), TEXT("same default weapon has isolated profile level and combat stats"));
			Check(Saves->SelectSlot(TEXT("DanjeolUpgrade")), TEXT("upgraded profile reloads")); Parts->SyncLoadoutWithSave();
			Check(Progression->GetUpgradeLevel(Weapon) == 3 && Progression->GetWeaponUpgradeFailures(Weapon) == 1
				&& FMath::IsNearlyEqual(ASC->GetNumericAttribute(Attack), BaseAttack + Weapon->GetUpgradeAttackBonus(3), .01f),
				TEXT("profile reload reconciles unchanged weapon identity with saved level"));
			Progression->ClearEquippedSlot(Slot); Parts->SyncLoadoutWithSave();
			Check(Progression->GetItemCount(Weapon) == 1, TEXT("unequipping materializes exactly one existing starter"));
			break;
		}
		case 3:
			Check(Progression->SetEquippedPart(Weapon), TEXT("owned starter re-equips normally")); Parts->SyncLoadoutWithSave(); break;
		case 4:
			Check(Progression->SetEquippedPart(Weapon) && Progression->GetItemCount(Weapon) == 1,
				TEXT("repeated equip cannot duplicate starter")); break;
		default: return Finish();
		}
		return true;
	}
};
void RunStarterWeaponCheck(const TArray<FString>& Args, UWorld* World)
{
	FString Namespace;
	if (!World || !FParse::Value(FCommandLine::Get(), TEXT("KnightSaveTestNamespace="), Namespace)
		|| !Namespace.StartsWith(TEXT("StarterWeapon"))) { UE_LOG(LogTemp, Error, TEXT("StarterWeapon requires an isolated StarterWeapon save namespace")); return; }
	auto Run = MakeShared<FStarterWeaponCheck>(); Run->World = World; Run->bReload = Args.Contains(TEXT("Reload"));
	Run->Deadline = FPlatformTime::Seconds() + 90.;
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Run](float Delta) { return Run->Tick(Delta); }), .5f);
}
FAutoConsoleCommandWithWorldAndArgs StarterWeaponCommand(TEXT("knight.StarterWeaponCheck"), TEXT("Run | Reload; isolated editor-only starter enhancement checks"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&RunStarterWeaponCheck), ECVF_Cheat);
}
#endif
