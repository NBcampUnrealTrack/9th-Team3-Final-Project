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
//   knight.Player.PartsWatch [1|0]                (2026-10-13) 장착 이벤트(OnEquipmentChanged)를 순서대로 로그로 남긴다.
//                                                켤 때 슬롯별 현재 장착을 한 번 조회한다. 인자가 없으면 켜기/끄기를 바꾼다.
//   knight.Player.PartsCompare                    (2026-10-13) 정비 화면 계산(knightEquipmentStats::Calculate)과 실제 적용된 스탯,
//                                                저장된 로드아웃과 실제 장착을 나란히 찍고 다른 곳을 표시한다. 로비(폰 없음)에서는 계산만 찍는다.
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
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "GameplayTagsManager.h"
#include "HAL/IConsoleManager.h"
#include "Item/knightEquipmentData.h"
#include "Item/knightEquipmentStats.h"
#include "Item/knightItemDefinitions.h"
#include "Item/knightPartsComponent.h"
#include "Progression/knightProgressionSubsystem.h"
#include "knightDev.h"
#include "knightPartsWatchComponent.h"

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

	// (2026-10-13) 장착 이벤트 한 줄에 붙일 짧은 스탯: 중량, 방어력, 공격력, 현재/최대 체력, 걷기 속도 배율.
	// 현재 체력을 같이 보는 이유: 교체할 때 최대치가 잠깐 내려갔다 올라가면서 현재 체력이 깎이는지 확인하려고.
	static FString ShortStats(const AActor* Owner)
	{
		const UAbilitySystemComponent* const ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Owner);
		const UknightCoreAttributeSet* const Core = ASC ? ASC->GetSet<UknightCoreAttributeSet>() : nullptr;
		const UknightAttributeSet* const Mech = ASC ? ASC->GetSet<UknightAttributeSet>() : nullptr;
		if (!Core || !Mech)
		{
			return TEXT("(no stats)");
		}
		return FString::Printf(TEXT("Weight %.1f  Defense %.1f  Attack %.1f  Health %.1f/%.1f  Walk x%.3f"),
			Mech->GetWeight(), Core->GetDefense(), Core->GetAttackPower(), Core->GetHealth(), Core->GetMaxHealth(), Mech->GetWalkSpeed());
	}

	static FString PartNameOrDash(const UknightPartItemDefinition* Part)
	{
		return Part ? Part->GetName() : TEXT("-");
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

	// knight.Player.PartsWatch [1|0] (2026-10-13). 플레이어 폰에 UknightPartsWatchComponent를 붙이거나 뗀다.
	// 인자가 없으면 지금 상태를 뒤집는다. 다시 켜면 이벤트 번호를 1부터 다시 센다.
	static void PartsWatchCommand(const TArray<FString>& Args, UWorld* World)
	{
		const TCHAR* const CommandName = TEXT("knight.Player.PartsWatch");
		UknightPartsComponent* const Parts = GetPlayerParts(World, CommandName);
		AActor* const Owner = Parts ? Parts->GetOwner() : nullptr;
		if (!Owner)
		{
			return;
		}
		UknightPartsWatchComponent* Watch = Owner->FindComponentByClass<UknightPartsWatchComponent>();
		const bool bTurnOn = Args.IsEmpty() ? Watch == nullptr : Args[0] != TEXT("0");
		if (!bTurnOn)
		{
			if (Watch)
			{
				Watch->StopWatching();
				Watch->DestroyComponent();
			}
			UE_LOG(LogknightDev, Log, TEXT("%s: off."), CommandName);
			return;
		}
		if (!Watch)
		{
			Watch = NewObject<UknightPartsWatchComponent>(Owner, NAME_None, RF_Transient);
			Watch->RegisterComponent();
		}
		Watch->StartWatching(Parts);
	}

	// knight.Player.PartsCompare (2026-10-13).
	// 1) 슬롯별로 저장된 로드아웃(정비 화면이 보여 주는 것)과 실제 장착을 나란히 찍는다.
	// 2) 실제 장착을 정비 화면과 같은 계산(knightEquipmentStats::Calculate)에 넣은 값과, ASC에 실제로 적용된 값을 비교한다.
	//    둘이 다르면 정비 화면의 숫자(현재·예상)가 실제와 어긋난다는 뜻이다. 오차 0.01까지는 같은 값으로 본다.
	// 로비처럼 폰이 없으면 저장된 로드아웃의 계산만 찍는다.
	static void PartsCompareCommand(const TArray<FString>& Args, UWorld* World)
	{
		const TCHAR* const CommandName = TEXT("knight.Player.PartsCompare");
		const UGameInstance* const GameInstance = World ? World->GetGameInstance() : nullptr;
		const UknightProgressionSubsystem* const Progression = GameInstance ? GameInstance->GetSubsystem<UknightProgressionSubsystem>() : nullptr;
		const UknightMaintenanceCatalog* const Catalog = UknightEquipmentSettings::LoadCatalog();
		const knightEquipmentStats::FLoadout Saved = knightEquipmentStats::ReadLoadout(Progression, Catalog);

		const APlayerController* const PC = World ? World->GetFirstPlayerController() : nullptr;
		const APawn* const Pawn = PC ? PC->GetPawn() : nullptr;
		const UknightPartsComponent* const Parts = Pawn ? Pawn->FindComponentByClass<UknightPartsComponent>() : nullptr;
		const UAbilitySystemComponent* const ASC = Parts ? UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Pawn) : nullptr;

		// 1) 슬롯: 저장 vs 실제
		knightEquipmentStats::FLoadout Live;
		int32 SlotDiffs = 0;
		if (Parts)
		{
			UE_LOG(LogknightDev, Log, TEXT("%s: slot      save                                  live"), CommandName);
			for (const FGameplayTag& Slot : Parts->GetLoadoutSlots())
			{
				const UknightPartItemDefinition* const LivePart = Parts->GetEquippedPart(Slot);
				const UknightPartItemDefinition* const SavedPart = Saved.FindRef(Slot);
				if (LivePart)
				{
					Live.Add(Slot, LivePart);
				}
				SlotDiffs += LivePart != SavedPart ? 1 : 0;
				UE_LOG(LogknightDev, Log, TEXT("%s:   %-9s %-37s %s%s"), CommandName, *SlotShortName(Slot),
					*PartNameOrDash(SavedPart), *PartNameOrDash(LivePart), LivePart != SavedPart ? TEXT("  <- differs") : TEXT(""));
			}
		}
		else
		{
			UE_LOG(LogknightDev, Log, TEXT("%s: no player pawn (lobby?). Showing the maintenance screen calculation for the saved loadout only."), CommandName);
			for (const TPair<FGameplayTag, const UknightPartItemDefinition*>& Entry : Saved)
			{
				UE_LOG(LogknightDev, Log, TEXT("%s:   %-9s %s"), CommandName, *SlotShortName(Entry.Key), *PartNameOrDash(Entry.Value));
			}
		}

		// 2) 스탯: 계산 vs 실제
		const FknightEquipmentStats Calculated = knightEquipmentStats::Calculate(Parts ? Live : Saved, Catalog, Progression);
		if (!Calculated.bComplete)
		{
			UE_LOG(LogknightDev, Warning, TEXT("%s: calculation incomplete: %s"), CommandName, *Calculated.IncompleteReason);
		}
		FString SetText;
		for (const TPair<FGameplayTag, int32>& Entry : Calculated.SetCounts)
		{
			SetText += FString::Printf(TEXT(" %s x%d"), *SlotShortName(Entry.Key), Entry.Value);
		}
		UE_LOG(LogknightDev, Log, TEXT("%s: set counts:%s"), CommandName, SetText.IsEmpty() ? TEXT(" none") : *SetText);

		TArray<FGameplayAttribute> Attributes;
		Calculated.Values.GetKeys(Attributes);
		Attributes.Sort([](const FGameplayAttribute& A, const FGameplayAttribute& B) { return A.GetName() < B.GetName(); });
		int32 StatDiffs = 0;
		for (const FGameplayAttribute& Attribute : Attributes)
		{
			const float CalculatedValue = Calculated.Values.FindRef(Attribute);
			if (!ASC || !ASC->HasAttributeSetForAttribute(Attribute))
			{
				UE_LOG(LogknightDev, Log, TEXT("%s:   %-20s calc %9.3f"), CommandName, *Attribute.GetName(), CalculatedValue);
				continue;
			}
			const float LiveValue = ASC->GetNumericAttribute(Attribute);
			const bool bSame = FMath::IsNearlyEqual(CalculatedValue, LiveValue, 0.01f);
			StatDiffs += bSame ? 0 : 1;
			UE_LOG(LogknightDev, Log, TEXT("%s:   %-20s calc %9.3f  live %9.3f%s"), CommandName, *Attribute.GetName(),
				CalculatedValue, LiveValue, bSame ? TEXT("") : TEXT("  <- differs"));
		}
		if (Parts)
		{
			UE_LOG(LogknightDev, Log, TEXT("%s: result: %d slot(s) differ from the save, %d stat(s) differ from the calculation."), CommandName, SlotDiffs, StatDiffs);
		}
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

	static FAutoConsoleCommandWithWorldAndArgs PartsWatchCmd(
		TEXT("knight.Player.PartsWatch"),
		TEXT("knight.Player.PartsWatch [1|0]. Logs every OnEquipmentChanged event on the player in order, and the current loadout when turned on. No argument toggles."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&PartsWatchCommand),
		ECVF_Cheat);

	static FAutoConsoleCommandWithWorldAndArgs PartsCompareCmd(
		TEXT("knight.Player.PartsCompare"),
		TEXT("knight.Player.PartsCompare. Compares the saved loadout with the equipped parts, and the maintenance screen calculation with the stats actually applied."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&PartsCompareCommand),
		ECVF_Cheat);
}

void UknightPartsWatchComponent::StartWatching(UknightPartsComponent* InParts)
{
	StopWatching();
	Parts = InParts;
	EventCount = 0;
	if (!InParts)
	{
		return;
	}
	InParts->OnEquipmentChanged.AddUniqueDynamic(this, &ThisClass::HandleEquipmentChanged);

	// 늦게 구독한 쪽이 초기 외형을 맞추는 방법 그대로: 슬롯별로 한 번 조회한다.
	UE_LOG(LogknightDev, Log, TEXT("knight.Player.PartsWatch: on. Current loadout (GetLoadoutSlots + GetEquippedPart):"));
	for (const FGameplayTag& Slot : InParts->GetLoadoutSlots())
	{
		UE_LOG(LogknightDev, Log, TEXT("knight.Player.PartsWatch:   %-9s %s"), *knightPartsCheats::SlotShortName(Slot),
			*knightPartsCheats::PartNameOrDash(InParts->GetEquippedPart(Slot)));
	}
	UE_LOG(LogknightDev, Log, TEXT("knight.Player.PartsWatch:   %s"), *knightPartsCheats::ShortStats(GetOwner()));
}

void UknightPartsWatchComponent::StopWatching()
{
	if (UknightPartsComponent* const Watched = Parts.Get())
	{
		Watched->OnEquipmentChanged.RemoveDynamic(this, &ThisClass::HandleEquipmentChanged);
	}
	Parts.Reset();
}

void UknightPartsWatchComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopWatching();
	Super::EndPlay(EndPlayReason);
}

void UknightPartsWatchComponent::HandleEquipmentChanged(FGameplayTag SlotTag, UknightPartItemDefinition* Part)
{
	// 이벤트를 받은 순간의 GetEquippedPart가 받은 파츠와 다르면 MISMATCH로 표시한다(해제 이벤트면 둘 다 비어 있어야 함).
	const UknightPartsComponent* const Watched = Parts.Get();
	const UknightPartItemDefinition* const Queried = Watched ? Watched->GetEquippedPart(SlotTag) : nullptr;
	++EventCount;
	UE_LOG(LogknightDev, Log, TEXT("knight.Player.PartsWatch: [#%d frame %llu] %s <- %s  (GetEquippedPart: %s%s)  %s"),
		EventCount, static_cast<unsigned long long>(GFrameCounter), *knightPartsCheats::SlotShortName(SlotTag),
		Part ? *Part->GetName() : TEXT("null"), *knightPartsCheats::PartNameOrDash(Queried),
		Queried == Part ? TEXT("") : TEXT(", MISMATCH"), *knightPartsCheats::ShortStats(GetOwner()));
}
