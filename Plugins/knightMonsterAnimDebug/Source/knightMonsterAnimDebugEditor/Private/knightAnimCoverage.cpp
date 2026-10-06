#include "knightAnimCoverage.h"

#include "Abilities/GameplayAbility.h"
#include "Animation/AnimMontage.h"
#include "Animation/Skeleton.h"
#include "Engine/SkeletalMesh.h"
#include "Misc/DateTime.h"
#include "knightAnimAbilityInfo.h"
#include "knightAnimDebugSettings.h"

namespace knightAnimCoverage
{
	using namespace knightAnimAbilityInfo;

	static bool AreSkeletonsCompatible(const USkeleton* MeshSkeleton, const USkeleton* MontageSkeleton)
	{
		return !MeshSkeleton || !MontageSkeleton || MeshSkeleton == MontageSkeleton || MeshSkeleton->IsCompatibleForEditor(MontageSkeleton);
	}

	static FString JoinSlots(const TSet<FName>& Slots)
	{
		TArray<FString> Names;
		for (const FName& Slot : Slots)
		{
			Names.Add(Slot.ToString());
		}
		return Names.IsEmpty() ? FString(TEXT("없음")) : FString::Join(Names, TEXT(", "));
	}

	/** 주 몽타주의 길이·판정 정렬 검사. 시간은 모두 어빌리티 시간(몽타주 시간 ÷ RateScale)으로 비교한다. */
	static void CheckPrimaryMontage(const FknightAbilityAnimInfo& Ability, const UAnimMontage& Montage, FknightAnimCoverageRow& Row)
	{
		const UknightAnimDebugSettings* const Settings = GetDefault<UknightAnimDebugSettings>();
		const float Rate = GetMontageRate(&Montage);
		const float Length = Montage.GetPlayLength() / Rate;

		TArray<FknightHitWindow> Windows;
		GetHitWindows(&Montage, Windows);
		const float FirstHitStart = Windows.IsEmpty() ? -1.f : Windows[0].Start / Rate;

		// 반응의 유지 몽타주는 루프하거나 마지막 포즈로 버티는 게 정상이라 길이 검사는 공격에만 한다.
		if (Ability.bIsAttack && Ability.PhaseTotal > 0.f && Length + 0.05f < Ability.PhaseTotal)
		{
			Row.AddIssue(EknightAnimCoverageSeverity::Warning,
				FString::Printf(TEXT("몽타주(%.2fs)가 어빌리티(%.2fs)보다 먼저 끝난다"), Length, Ability.PhaseTotal));
		}
		if (Ability.HitTime >= 0.f && FirstHitStart >= 0.f && FMath::Abs(FirstHitStart - Ability.HitTime) > Settings->HitAlignmentTolerance)
		{
			Row.AddIssue(EknightAnimCoverageSeverity::Warning,
				FString::Printf(TEXT("판정 어긋남: 어빌리티 %.2fs / 노티파이 %.2fs"), Ability.HitTime, FirstHitStart));
		}
		if (Ability.bIsAttack && Ability.HitTime < 0.f && FirstHitStart < 0.f)
		{
			Row.AddIssue(EknightAnimCoverageSeverity::Error, TEXT("판정 노티파이 없음: 몽타주가 판정을 맡는 공격이다"));
		}
		for (const FknightHitWindow& Window : Windows)
		{
			if (!Window.HasTraceShape())
			{
				Row.AddIssue(EknightAnimCoverageSeverity::Warning, FString::Printf(TEXT("%s의 소켓을 읽지 못해 미리보기 궤적을 그릴 수 없다"), *Window.Name));
			}
		}
	}

	/** 몬스터 Blueprint 하나의 메시/AnimBP 행과 어빌리티별 몽타주 행. */
	static void InspectMonster(UClass* MonsterClass, TArray<TSharedPtr<FknightAnimCoverageRow>>& OutRows)
	{
		const FString MonsterName = CleanName(MonsterClass);
		const FknightMonsterMeshInfo MeshInfo = GetMonsterMeshInfo(MonsterClass);
		const USkeleton* const MeshSkeleton = MeshInfo.Mesh ? MeshInfo.Mesh->GetSkeleton() : nullptr;
		TSet<FName> AnimSlots;
		GetAnimBlueprintSlots(MeshInfo.AnimClass, AnimSlots);

		// 메시와 AnimBP: 이동 애니메이션과 몽타주 재생의 바탕이다.
		{
			TSharedPtr<FknightAnimCoverageRow> Row = MakeShared<FknightAnimCoverageRow>();
			Row->Monster = MonsterName;
			Row->Slot = TEXT("메시/AnimBP");
			Row->MontageName = MeshInfo.Mesh ? FString::Printf(TEXT("%s / %s"), *MeshInfo.Mesh->GetName(), *CleanName(MeshInfo.AnimClass)) : FString();
			Row->OpenTarget = GetBlueprintOrClass(MonsterClass);
			Row->MonsterClass = MonsterClass;
			if (!MeshInfo.Mesh)
			{
				Row->AddIssue(EknightAnimCoverageSeverity::Error, TEXT("스켈레탈 메시 없음"));
			}
			else if (!MeshInfo.AnimClass)
			{
				Row->AddIssue(EknightAnimCoverageSeverity::Warning, TEXT("AnimBP 없음: 이동 애니메이션과 몽타주 슬롯이 없다"));
			}
			OutRows.Add(Row);
		}

		TArray<FknightAbilityAnimInfo> Abilities;
		FindPlayableAbilities(MonsterClass, Abilities);

		for (const FknightAbilityAnimInfo& Ability : Abilities)
		{
			auto MakeRow = [&](const FString& Slot)
			{
				TSharedPtr<FknightAnimCoverageRow> Row = MakeShared<FknightAnimCoverageRow>();
				Row->Monster = MonsterName;
				Row->Ability = Ability.Name;
				Row->Kind = Ability.Kind;
				Row->Slot = Slot;
				Row->Timing = Ability.GetTimingText();
				Row->OpenTarget = GetBlueprintOrClass(Ability.AbilityClass);
				Row->MonsterClass = MonsterClass;
				Row->AbilityClass = Ability.AbilityClass;
				return Row;
			};

			if (Ability.MontageProperties.IsEmpty())
			{
				TSharedPtr<FknightAnimCoverageRow> Row = MakeRow(TEXT("-"));
				Row->AddIssue(EknightAnimCoverageSeverity::Warning, TEXT("몽타주 프로퍼티가 없는 어빌리티"));
				OutRows.Add(Row);
				continue;
			}

			const FObjectProperty* const PrimarySlot = Ability.GetPrimarySlot();
			for (const FObjectProperty* const Property : Ability.MontageProperties)
			{
				TSharedPtr<FknightAnimCoverageRow> Row = MakeRow(Property->GetName());
				UAnimMontage* const Montage = Ability.GetMontage(Property);
				const bool bOptional = Ability.IsOptionalSlot(Property);

				if (!Montage)
				{
					Row->AddIssue(bOptional ? EknightAnimCoverageSeverity::Warning : EknightAnimCoverageSeverity::Error,
						bOptional ? TEXT("비어 있음: 타이밍 값으로 대신 동작한다") : TEXT("몽타주 없음"));
					OutRows.Add(Row);
					continue;
				}

				Row->MontageName = Montage->GetName();
				Row->MontageLength = Montage->GetPlayLength();
				Row->OpenTarget = Montage;

				TArray<FknightHitWindow> Windows;
				GetHitWindows(Montage, Windows);
				for (const FknightHitWindow& Window : Windows)
				{
					Row->HitNotify += FString::Printf(TEXT("%s%s %.2f-%.2f"), Row->HitNotify.IsEmpty() ? TEXT("") : TEXT(", "), *Window.Name, Window.Start, Window.End);
				}

				if (!AreSkeletonsCompatible(MeshSkeleton, Montage->GetSkeleton()))
				{
					Row->AddIssue(EknightAnimCoverageSeverity::Error,
						FString::Printf(TEXT("스켈레톤 불일치 (몽타주 %s / 메시 %s)"), *GetNameSafe(Montage->GetSkeleton()), *GetNameSafe(MeshSkeleton)));
				}
				// 몽타주는 AnimBP의 같은 이름 Slot 노드를 거쳐야 보인다. 흔한 "넣었는데 안 보임"의 원인이다.
				if (MeshInfo.AnimClass && !AnimSlots.IsEmpty())
				{
					for (const FSlotAnimationTrack& Track : Montage->SlotAnimTracks)
					{
						if (!AnimSlots.Contains(Track.SlotName))
						{
							Row->AddIssue(EknightAnimCoverageSeverity::Warning, FString::Printf(TEXT("AnimBP에 '%s' 슬롯 노드가 없다 (있는 슬롯: %s)"),
								*Track.SlotName.ToString(), *JoinSlots(AnimSlots)));
						}
					}
				}
				else if (MeshInfo.AnimClass && AnimSlots.IsEmpty())
				{
					Row->AddIssue(EknightAnimCoverageSeverity::Warning, TEXT("AnimBP에서 Slot 노드를 찾지 못했다: 몽타주가 재생되지 않을 수 있다"));
				}
				if (Property == PrimarySlot)
				{
					CheckPrimaryMontage(Ability, *Montage, *Row);
				}
				OutRows.Add(Row);
			}
		}

		if (Abilities.IsEmpty())
		{
			TSharedPtr<FknightAnimCoverageRow> Row = MakeShared<FknightAnimCoverageRow>();
			Row->Monster = MonsterName;
			Row->Slot = TEXT("-");
			Row->OpenTarget = GetBlueprintOrClass(MonsterClass);
			Row->MonsterClass = MonsterClass;
			Row->AddIssue(EknightAnimCoverageSeverity::Error, TEXT("어빌리티를 찾지 못했다: MonsterDefinition과 AbilitySets를 확인한다"));
			OutRows.Add(Row);
		}
	}

	FknightAnimCoverageResult Gather()
	{
		FknightAnimCoverageResult Result;
		const TArray<UClass*> MonsterClasses = FindMonsterClasses(Result.Notice);
		for (UClass* const MonsterClass : MonsterClasses)
		{
			InspectMonster(MonsterClass, Result.Rows);
		}
		Result.MonsterCount = MonsterClasses.Num();
		return Result;
	}

	const TCHAR* SeverityLabel(EknightAnimCoverageSeverity Severity)
	{
		switch (Severity)
		{
		case EknightAnimCoverageSeverity::Ok:      return TEXT("정상");
		case EknightAnimCoverageSeverity::Warning: return TEXT("주의");
		case EknightAnimCoverageSeverity::Error:   return TEXT("문제");
		}
		return TEXT("?");
	}

	static FString Cell(const FString& Text)
	{
		return Text.IsEmpty() ? FString(TEXT("-")) : Text.Replace(TEXT("|"), TEXT("\\|"));
	}

	FString ToMarkdown(const FknightAnimCoverageResult& Result)
	{
		FString Md;
		Md += TEXT("# 몬스터 애니메이션 커버리지\n\n");
		Md += FString::Printf(TEXT("%s 기준. 몬스터 %d종, 항목 %d개: 정상 %d, 주의 %d, 문제 %d.\n\n"),
			*FDateTime::Now().ToString(TEXT("%Y-%m-%d %H:%M")), Result.MonsterCount, Result.Rows.Num(),
			Result.CountSeverity(EknightAnimCoverageSeverity::Ok), Result.CountSeverity(EknightAnimCoverageSeverity::Warning),
			Result.CountSeverity(EknightAnimCoverageSeverity::Error));
		if (!Result.Notice.IsEmpty())
		{
			Md += TEXT("> ") + Result.Notice + TEXT("\n\n");
		}

		Md += TEXT("## 애니메이터 할 일\n\n");
		bool bAnyTodo = false;
		for (const EknightAnimCoverageSeverity Severity : { EknightAnimCoverageSeverity::Error, EknightAnimCoverageSeverity::Warning })
		{
			for (const TSharedPtr<FknightAnimCoverageRow>& Row : Result.Rows)
			{
				if (Row->Severity == Severity)
				{
					Md += FString::Printf(TEXT("- [ ] **%s** %s / %s / %s: %s\n"), SeverityLabel(Severity), *Row->Monster,
						Row->Ability.IsEmpty() ? TEXT("(몬스터)") : *Row->Ability, *Row->Slot, *Row->GetIssueText());
					bAnyTodo = true;
				}
			}
		}
		if (!bAnyTodo)
		{
			Md += TEXT("없음.\n");
		}

		Md += TEXT("\n## 전체 표\n\n");
		Md += TEXT("| 상태 | 몬스터 | 어빌리티 | 종류 | 슬롯 | 몽타주 | 길이(s) | 타이밍(s) | 판정 노티파이 | 내용 |\n");
		Md += TEXT("|---|---|---|---|---|---|---:|---|---|---|\n");
		for (const TSharedPtr<FknightAnimCoverageRow>& Row : Result.Rows)
		{
			Md += FString::Printf(TEXT("| %s | %s | %s | %s | %s | %s | %s | %s | %s | %s |\n"), SeverityLabel(Row->Severity),
				*Cell(Row->Monster), *Cell(Row->Ability), *Cell(Row->Kind), *Cell(Row->Slot), *Cell(Row->MontageName),
				Row->MontageLength > 0.f ? *FString::Printf(TEXT("%.2f"), Row->MontageLength) : TEXT("-"),
				*Cell(Row->Timing), *Cell(Row->HitNotify), *Cell(Row->GetIssueText()));
		}
		return Md;
	}
}

void FknightAnimCoverageRow::AddIssue(EknightAnimCoverageSeverity InSeverity, const FString& Issue)
{
	Severity = FMath::Max(Severity, InSeverity);
	Issues.Add(Issue);
}

int32 FknightAnimCoverageResult::CountSeverity(EknightAnimCoverageSeverity Severity) const
{
	int32 Count = 0;
	for (const TSharedPtr<FknightAnimCoverageRow>& Row : Rows)
	{
		Count += Row->Severity == Severity ? 1 : 0;
	}
	return Count;
}
