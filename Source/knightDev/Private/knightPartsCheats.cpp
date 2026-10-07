// 파츠 장착·해제 테스트용 콘솔 명령 (개발 빌드 전용, 치트 명령, 2026-10-07)
// 대장간·장비 화면이 연결되기 전에 파츠 효과(스탯·중량·적재 한도)를 확인하려고 만들었다.
// 세이브(UknightProgressionSubsystem)는 바꾸지 않는다 → 다음 플레이에서는 저장된 로드아웃으로 돌아간다.
//
// 사용법 (PIE 중 콘솔 ~ 키):
//   knight.Player.Parts                          슬롯별 장착 파츠, 중량 합/적재 한도, 장착할 수 있는 파츠 목록을 출력한다
//   knight.Player.EquipPart <이름|경로> [강화]     파츠를 장착한다 (예: knight.Player.EquipPart DA_Part_Test_Torso)
//                                                이름은 에셋 이름(대소문자 무시, 일부만 써도 하나로 정해지면 됨) 또는 /Game/... 경로
//   knight.Player.UnequipPart <슬롯>              슬롯을 비운다 (예: knight.Player.UnequipPart Torso)
//                                                슬롯: Head Torso Arms Legs Booster Generator Exterior Weapon
//
// 확인 흐름: EquipPart → 파츠 능력치(StatModifiers)로 만든 GE·AbilitySet으로 스탯 변화 + 중량 합이 Weight 스탯으로 → 이동 속도의 중량 계수가 바뀜
//           적재 한도를 넘는 파츠는 장착이 거부되고 이유(중량 합 → 장착 후 중량 / 한도)가 로그로 나온다.
// 결과는 LogknightDev 로그로 출력한다.

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "AbilitySystem/Attributes/knightAttributeSet.h"
#include "AbilitySystem/Attributes/knightCoreAttributeSet.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Character/knightMovementComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "GameplayTagsManager.h"
#include "HAL/IConsoleManager.h"
#include "Item/knightItemDefinitions.h"
#include "Item/knightPartsComponent.h"
#include "knightDev.h"

namespace knightPartsCheats
{
	// 첫 번째 플레이어 폰의 파츠 컴포넌트. 없으면 경고를 남기고 null.
	static UknightPartsComponent* GetPlayerParts(UWorld* World, const TCHAR* CommandName)
	{
		const APlayerController* const PC = World ? World->GetFirstPlayerController() : nullptr;
		const APawn* const Pawn = PC ? PC->GetPawn() : nullptr;
		UknightPartsComponent* const Parts = Pawn ? Pawn->FindComponentByClass<UknightPartsComponent>() : nullptr;
		if (!Parts)
		{
			UE_LOG(LogknightDev, Warning, TEXT("%s: no player pawn with a parts component."), CommandName);
		}
		return Parts;
	}

	// "Equipment.Slot.Torso" → "Torso". 로그를 짧게 하려고 마지막 부분만 쓴다.
	static FString SlotShortName(const FGameplayTag& Slot)
	{
		FString Name = Slot.ToString();
		int32 DotIndex = INDEX_NONE;
		return Name.FindLastChar(TEXT('.'), DotIndex) ? Name.RightChop(DotIndex + 1) : Name;
	}

	// 프로젝트에 있는 모든 파츠 정의(무기 포함). 에셋 레지스트리에서 찾아 불러온다. 슬롯 → 이름 순으로 정렬.
	static void GetAllParts(TArray<UknightPartItemDefinition*>& OutParts)
	{
		TArray<FAssetData> Assets;
		if (IAssetRegistry* const Registry = IAssetRegistry::Get())
		{
			Registry->GetAssetsByClass(UknightPartItemDefinition::StaticClass()->GetClassPathName(), Assets, true);
		}
		for (const FAssetData& Asset : Assets)
		{
			if (UknightPartItemDefinition* const Part = Cast<UknightPartItemDefinition>(Asset.GetAsset()))
			{
				OutParts.Add(Part);
			}
		}
		OutParts.Sort([](const UknightPartItemDefinition& A, const UknightPartItemDefinition& B)
		{
			return A.SlotTag != B.SlotTag ? A.SlotTag.ToString() < B.SlotTag.ToString() : A.GetName() < B.GetName();
		});
	}

	// 이름(대소문자 무시, 정확히 같거나 일부만 써서 하나로 정해지는 경우) 또는 /Game/... 경로로 파츠를 찾는다.
	static UknightPartItemDefinition* FindPart(const FString& NameOrPath, const TCHAR* CommandName)
	{
		if (NameOrPath.Contains(TEXT("/")))
		{
			UknightPartItemDefinition* const Part = LoadObject<UknightPartItemDefinition>(nullptr, *NameOrPath);
			if (!Part)
			{
				UE_LOG(LogknightDev, Warning, TEXT("%s: no part at '%s'."), CommandName, *NameOrPath);
			}
			return Part;
		}

		TArray<UknightPartItemDefinition*> Parts;
		GetAllParts(Parts);
		TArray<UknightPartItemDefinition*> Partial;
		for (UknightPartItemDefinition* const Part : Parts)
		{
			if (Part->GetName().Equals(NameOrPath, ESearchCase::IgnoreCase))
			{
				return Part;
			}
			if (Part->GetName().Contains(NameOrPath, ESearchCase::IgnoreCase))
			{
				Partial.Add(Part);
			}
		}
		if (Partial.Num() == 1)
		{
			return Partial[0];
		}
		UE_LOG(LogknightDev, Warning, TEXT("%s: '%s' matches %d parts. Use knight.Player.Parts to list them."), CommandName, *NameOrPath, Partial.Num());
		return nullptr;
	}

	// 파츠 효과를 볼 때 확인할 스탯을 두 줄로 남긴다. 중량·한도와 이동 속도는 무브먼트 계수까지 같이 본다.
	static void LogPartStats(const UknightPartsComponent* Parts, const TCHAR* CommandName)
	{
		const AActor* const Owner = Parts->GetOwner();
		const UAbilitySystemComponent* const ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Owner);
		const UknightCoreAttributeSet* const Core = ASC ? ASC->GetSet<UknightCoreAttributeSet>() : nullptr;
		const UknightAttributeSet* const Mech = ASC ? ASC->GetSet<UknightAttributeSet>() : nullptr;
		if (!Core || !Mech)
		{
			return;
		}
		const ACharacter* const Character = Cast<ACharacter>(Owner);
		const UknightMovementComponent* const Movement = Character ? Cast<UknightMovementComponent>(Character->GetCharacterMovement()) : nullptr;

		UE_LOG(LogknightDev, Log, TEXT("%s: Weight=%.1f / LoadCapacity=%.1f  WeightFactor=%.2f  MaxSpeed=%.1f  WalkSpeed=x%.2f  DodgeSpeed=x%.2f"),
			CommandName, Mech->GetWeight(), Mech->GetLoadCapacity(),
			Movement ? Movement->GetWeightSpeedFactor() : 1.f, Movement ? Movement->GetMaxSpeed() : 0.f,
			Mech->GetWalkSpeed(), Mech->GetDodgeSpeed());
		UE_LOG(LogknightDev, Log, TEXT("%s: MaxHealth=%.1f (Head %.0f Torso %.0f Arms %.0f Legs %.0f)  Defense=%.1f  AttackPower=%.1f  Crit=%.2f  MaxPoise=%.1f  MaxBoostGauge=%.1f  BoostRecovery=%.1f"),
			CommandName, Core->GetMaxHealth(), Mech->GetMaxHeadHealth(), Mech->GetMaxTorsoHealth(), Mech->GetMaxArmsHealth(), Mech->GetMaxLegsHealth(),
			Core->GetDefense(), Core->GetAttackPower(), Core->GetCriticalChance(), Core->GetMaxPoise(),
			Mech->GetMaxBoostGauge(), Mech->GetBoostRecoveryRate());
	}

	// knight.Player.Parts
	static void PartsCommand(const TArray<FString>& Args, UWorld* World)
	{
		const TCHAR* const CommandName = TEXT("knight.Player.Parts");
		const UknightPartsComponent* const Parts = GetPlayerParts(World, CommandName);
		if (!Parts)
		{
			return;
		}

		// 슬롯별 장착 상태
		for (const FGameplayTag& Slot : Parts->GetLoadoutSlots())
		{
			const UknightPartItemDefinition* const Part = Parts->GetEquippedPart(Slot);
			UE_LOG(LogknightDev, Log, TEXT("%s:   %-9s %s"), CommandName, *SlotShortName(Slot),
				Part ? *FString::Printf(TEXT("%s (weight %.1f)"), *Part->GetName(), Part->Weight) : TEXT("-"));
		}
		UE_LOG(LogknightDev, Log, TEXT("%s: total weight %.1f / load capacity %.1f"), CommandName, Parts->GetTotalWeight(), Parts->GetLoadCapacity());

		// 장착할 수 있는 파츠 목록. 'x'는 지금 장착하면 적재 한도를 넘는 파츠.
		TArray<UknightPartItemDefinition*> AllParts;
		GetAllParts(AllParts);
		UE_LOG(LogknightDev, Log, TEXT("%s: %d part asset(s):"), CommandName, AllParts.Num());
		for (const UknightPartItemDefinition* const Part : AllParts)
		{
			UE_LOG(LogknightDev, Log, TEXT("%s:   %s %-9s %s (weight %.1f)"), CommandName,
				Parts->CanEquipPart(Part) ? TEXT(" ") : TEXT("x"), *SlotShortName(Part->SlotTag), *Part->GetName(), Part->Weight);
		}
		LogPartStats(Parts, CommandName);
	}

	// knight.Player.EquipPart <이름|경로> [강화 레벨]
	static void EquipPartCommand(const TArray<FString>& Args, UWorld* World)
	{
		const TCHAR* const CommandName = TEXT("knight.Player.EquipPart");
		if (Args.IsEmpty())
		{
			UE_LOG(LogknightDev, Warning, TEXT("%s <PartName|/Game/Path> [UpgradeLevel]: e.g. knight.Player.EquipPart DA_Part_Test_Torso"), CommandName);
			return;
		}
		UknightPartsComponent* const Parts = GetPlayerParts(World, CommandName);
		UknightPartItemDefinition* const Part = Parts ? FindPart(Args[0], CommandName) : nullptr;
		if (!Part)
		{
			return;
		}

		const int32 UpgradeLevel = Args.IsValidIndex(1) ? FCString::Atoi(*Args[1]) : 0;
		const float WeightBefore = Parts->GetTotalWeight();
		if (!Parts->EquipPart(Part, UpgradeLevel))
		{
			const UknightPartItemDefinition* const Replaced = Parts->GetEquippedPart(Part->SlotTag);
			UE_LOG(LogknightDev, Warning, TEXT("%s: refused %s. Weight %.1f -> %.1f is over load capacity %.1f (or the part has no slot)."),
				CommandName, *Part->GetName(), WeightBefore, WeightBefore - (Replaced ? Replaced->Weight : 0.f) + Part->Weight, Parts->GetLoadCapacity());
			return;
		}
		UE_LOG(LogknightDev, Log, TEXT("%s: equipped %s in %s (level %d). Weight %.1f -> %.1f"),
			CommandName, *Part->GetName(), *SlotShortName(Part->SlotTag), UpgradeLevel, WeightBefore, Parts->GetTotalWeight());
		LogPartStats(Parts, CommandName);
	}

	// knight.Player.UnequipPart <슬롯>. 짧은 이름(Torso) 또는 전체 태그(Equipment.Slot.Torso).
	static void UnequipPartCommand(const TArray<FString>& Args, UWorld* World)
	{
		const TCHAR* const CommandName = TEXT("knight.Player.UnequipPart");
		if (Args.IsEmpty())
		{
			UE_LOG(LogknightDev, Warning, TEXT("%s <Slot>: Head Torso Arms Legs Booster Generator Exterior Weapon"), CommandName);
			return;
		}
		UknightPartsComponent* const Parts = GetPlayerParts(World, CommandName);
		if (!Parts)
		{
			return;
		}
		const FString TagName = Args[0].StartsWith(TEXT("Equipment.Slot.")) ? Args[0] : TEXT("Equipment.Slot.") + Args[0];
		const FGameplayTag Slot = UGameplayTagsManager::Get().RequestGameplayTag(FName(*TagName), false);
		if (!Slot.IsValid())
		{
			UE_LOG(LogknightDev, Warning, TEXT("%s: no slot tag '%s'."), CommandName, *TagName);
			return;
		}
		const UknightPartItemDefinition* const Part = Parts->GetEquippedPart(Slot);
		if (!Part)
		{
			UE_LOG(LogknightDev, Log, TEXT("%s: %s is already empty."), CommandName, *SlotShortName(Slot));
			return;
		}
		const FString PartName = Part->GetName();
		Parts->UnequipSlot(Slot);
		UE_LOG(LogknightDev, Log, TEXT("%s: removed %s from %s. Weight now %.1f"), CommandName, *PartName, *SlotShortName(Slot), Parts->GetTotalWeight());
		LogPartStats(Parts, CommandName);
	}

	// 콘솔 명령 등록. ECVF_Cheat라 개발 빌드에서만 동작한다.
	static FAutoConsoleCommandWithWorldAndArgs PartsCmd(
		TEXT("knight.Player.Parts"),
		TEXT("knight.Player.Parts. Logs equipped parts per slot, total weight and load capacity, and every part asset ('x' = over capacity)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&PartsCommand),
		ECVF_Cheat);

	static FAutoConsoleCommandWithWorldAndArgs EquipPartCmd(
		TEXT("knight.Player.EquipPart"),
		TEXT("knight.Player.EquipPart <PartName|/Game/Path> [UpgradeLevel]. Equips a part on the player (refused over load capacity). Does not change the save."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&EquipPartCommand),
		ECVF_Cheat);

	static FAutoConsoleCommandWithWorldAndArgs UnequipPartCmd(
		TEXT("knight.Player.UnequipPart"),
		TEXT("knight.Player.UnequipPart <Slot>. Empties a slot: Head Torso Arms Legs Booster Generator Exterior Weapon. Does not change the save."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&UnequipPartCommand),
		ECVF_Cheat);
}
