#include "Misc/AutomationTest.h"

#include "AbilitySystem/Attributes/knightCoreAttributeSet.h"
#include "Item/knightEquipmentData.h"
#include "Item/knightEquipmentStats.h"
#include "Item/knightItemDefinitions.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FknightMaintenanceQueriesTest, "Knight.Maintenance.Queries",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FknightMaintenanceQueriesTest::RunTest(const FString& Parameters)
{
	const UknightMaintenanceCatalog* Catalog = UknightEquipmentSettings::LoadCatalog();
	if (!TestNotNull(TEXT("maintenance catalog"), Catalog)) return false;
	auto LoadPart = [](const TCHAR* Set, const TCHAR* Name)
	{
		const FString Asset = FString::Printf(TEXT("DA_Equipment_%s_%s"), Set, Name);
		return LoadObject<UknightPartItemDefinition>(nullptr,
			*FString::Printf(TEXT("/Game/Knight/Data/Equipment/%s/%s.%s"), Set, *Asset, *Asset));
	};
	const UknightPartItemDefinition* Head = LoadPart(TEXT("Janyeol"), TEXT("HeadA"));
	const UknightPartItemDefinition* Body = LoadPart(TEXT("Janyeol"), TEXT("BodyA"));
	const UknightPartItemDefinition* Arms = LoadPart(TEXT("Janyeol"), TEXT("Arms"));
	const UknightPartItemDefinition* Legs = LoadPart(TEXT("Janyeol"), TEXT("Legs"));
	const UknightPartItemDefinition* OtherBody = LoadPart(TEXT("Janhyang"), TEXT("Body"));
	if (!TestNotNull(TEXT("Janyeol HeadA"), Head) || !TestNotNull(TEXT("Janyeol BodyA"), Body)
		|| !TestNotNull(TEXT("Janyeol Arms"), Arms) || !TestNotNull(TEXT("Janyeol Legs"), Legs)
		|| !TestNotNull(TEXT("Janhyang Body"), OtherBody)) return false;

	knightEquipmentStats::FLoadout Loadout;
	for (const UknightPartItemDefinition* Part : { Head, Body, Arms, Legs }) Loadout.Add(Part->SlotTag, Part);
	const FknightEquipmentComparison Preview = knightEquipmentStats::CompareReplacement(
		Loadout, Body->SlotTag, OtherBody, Catalog, nullptr);
	TestTrue(TEXT("same-slot replacement is valid"), Preview.bValidReplacement);
	TestTrue(TEXT("preview keeps original body"), Loadout.FindRef(Body->SlotTag) == Body);
	TestEqual(TEXT("loadout still contains four parts"), Loadout.Num(), 4);
	TestEqual(TEXT("current Janyeol count"), Preview.Current.SetCounts.FindRef(Body->SetTag), 4);
	TestEqual(TEXT("replacement Janyeol count"), Preview.Replacement.SetCounts.FindRef(Body->SetTag), 3);
	TestEqual(TEXT("replacement Janhyang count"), Preview.Replacement.SetCounts.FindRef(OtherBody->SetTag), 1);
	const auto BeforeTiers = knightEquipmentStats::GetActiveSetBonuses(Preview.Current, Catalog);
	const auto AfterTiers = knightEquipmentStats::GetActiveSetBonuses(Preview.Replacement, Catalog);
	const auto HasTier = [](const TArray<FknightActiveSetBonus>& Tiers, const FGameplayTag SetTag, bool bFourPiece)
	{
		for (const FknightActiveSetBonus& Tier : Tiers)
			if (Tier.Set && Tier.Set->SetTag == SetTag && Tier.bFourPiece == bFourPiece) return true;
		return false;
	};
	TestTrue(TEXT("four-piece tier was active"), HasTier(BeforeTiers, Body->SetTag, true));
	TestFalse(TEXT("four-piece tier drops on replacement"), HasTier(AfterTiers, Body->SetTag, true));
	TestTrue(TEXT("two-piece tier remains"), HasTier(AfterTiers, Body->SetTag, false));

	knightEquipmentStats::FLoadout ExpectedLoadout = Loadout;
	ExpectedLoadout.Add(Body->SlotTag, OtherBody);
	const FknightEquipmentStats Expected = knightEquipmentStats::Calculate(ExpectedLoadout, Catalog, nullptr);
	const FGameplayAttribute Defense = UknightCoreAttributeSet::GetDefenseAttribute();
	TestTrue(TEXT("replacement total matches a fresh calculation"), FMath::IsNearlyEqual(
		Preview.Replacement.Values.FindRef(Defense), Expected.Values.FindRef(Defense)));
	TestEqual(TEXT("exactness follows AbilitySet evaluation"), Preview.bExact,
		Preview.Current.bComplete && Preview.Replacement.bComplete);
	if (Preview.bExact)
		TestTrue(TEXT("delta equals replacement minus current"), FMath::IsNearlyEqual(
			Preview.Deltas.FindRef(Defense), Expected.Values.FindRef(Defense) - Preview.Current.Values.FindRef(Defense)));
	else
		TestTrue(TEXT("incomplete preview does not publish exact deltas"), Preview.Deltas.IsEmpty());

	const FknightEquipmentComparison Invalid = knightEquipmentStats::CompareReplacement(
		Loadout, Head->SlotTag, OtherBody, Catalog, nullptr);
	TestFalse(TEXT("mismatched replacement slot is rejected"), Invalid.bValidReplacement);
	TestTrue(TEXT("invalid replacement has no deltas"), Invalid.Deltas.IsEmpty());

	const FknightItemContribution ItemOnly = knightEquipmentStats::CalculateItemContribution(OtherBody, nullptr);
	knightEquipmentStats::FLoadout SingleLoadout;
	SingleLoadout.Add(OtherBody->SlotTag, OtherBody);
	const FknightEquipmentStats Single = knightEquipmentStats::Calculate(SingleLoadout, nullptr, nullptr);
	const FknightEquipmentStats Empty = knightEquipmentStats::Calculate({}, nullptr, nullptr);
	if (ItemOnly.bComplete)
		TestTrue(TEXT("item contribution excludes set tier and baseline"), FMath::IsNearlyEqual(
			ItemOnly.Values.FindRef(Defense), Single.Values.FindRef(Defense) - Empty.Values.FindRef(Defense)));
	else
		TestTrue(TEXT("incomplete item contribution stays unquantified"), ItemOnly.Values.IsEmpty());

	const UknightWeaponItemDefinition* Weapon = Cast<UknightWeaponItemDefinition>(
		LoadPart(TEXT("Janyeol"), TEXT("Weapon")));
	if (TestNotNull(TEXT("Janyeol weapon"), Weapon))
	{
		TestEqual(TEXT("weapon upgrade steps"), Weapon->UpgradeSteps.Num(), 15);
		for (int32 Level = 0; Level < Weapon->UpgradeSteps.Num(); ++Level)
		{
			// 1강당 2.6처럼 float으로 정확히 안 떨어지는 값은 누적 합의 차이가 1e-5쯤 어긋나서 허용 오차를 둔다.
			TestTrue(FString::Printf(TEXT("level %d quote attack gain uses step data"), Level + 1),
				FMath::IsNearlyEqual(Weapon->GetWeaponAttackAtLevel(Level + 1)
					- Weapon->GetWeaponAttackAtLevel(Level), Weapon->UpgradeSteps[Level].AttackIncrease, KINDA_SMALL_NUMBER));
		}
	}
	return true;
}

#endif
