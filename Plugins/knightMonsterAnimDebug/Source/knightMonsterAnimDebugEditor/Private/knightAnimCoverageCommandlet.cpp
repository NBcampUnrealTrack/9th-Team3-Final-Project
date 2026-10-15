#include "knightAnimCoverageCommandlet.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "PreviewScene.h"
#include "knightAnimCoverage.h"
#include "knightAttackPreviewModel.h"

DEFINE_LOG_CATEGORY_STATIC(LogknightAnimCoverage, Log, All);

UknightAnimCoverageCommandlet::UknightAnimCoverageCommandlet()
{
	IsClient = false;
	IsEditor = true;
	IsServer = false;
	LogToConsole = true;
}

int32 UknightAnimCoverageCommandlet::Main(const FString& Params)
{
	// 커맨드렛에서는 에셋 레지스트리가 비동기로 채워지므로 다 읽을 때까지 기다린다.
	IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	AssetRegistry.SearchAllAssets(true);

	// -PreviewCheck: 공격 미리보기의 궤적·명중 계산을 화면 없이 점검한다(더미를 여러 거리에 세워 본다).
	if (FParse::Param(*Params, TEXT("PreviewCheck")))
	{
		FknightAttackPreviewModel Model(MakeShared<FPreviewScene>(FPreviewScene::ConstructionValues().SetCreatePhysicsScene(false)));
		for (int32 MonsterIndex = 0; MonsterIndex < Model.GetNumMonsters(); ++MonsterIndex)
		{
			Model.SelectMonster(MonsterIndex);
			for (int32 AbilityIndex = 0; AbilityIndex < Model.GetAbilities().Num(); ++AbilityIndex)
			{
				Model.SelectAbility(AbilityIndex);
				for (const float Distance : { 60.f, 200.f, 400.f })
				{
					Model.DummyDistance = Distance;
					Model.RecomputeHits();
					UE_LOG(LogknightAnimCoverage, Display, TEXT("PREVIEW %s"), *Model.DescribeForLog());
				}
			}
			Model.DummyDistance = 200.f;
			Model.RecomputeHits();
			Model.EngageDistance = 600.f;
			Model.BuildGapMap();
			UE_LOG(LogknightAnimCoverage, Display, TEXT("PREVIEW GAP %s: %s"), *Model.GetMonsterName(MonsterIndex), *Model.GetGapSummary().Replace(TEXT("\n"), TEXT(" | ")));
		}
	}

	// -RateEditCheck: 구간 속도를 추가·수정·삭제해 보며 시간축이 따라 바뀌는지 점검한다. 에셋은 저장하지 않는다.
	if (FParse::Param(*Params, TEXT("RateEditCheck")))
	{
		FknightAttackPreviewModel Model(MakeShared<FPreviewScene>(FPreviewScene::ConstructionValues().SetCreatePhysicsScene(false)));
		for (int32 MonsterIndex = 0; MonsterIndex < Model.GetNumMonsters(); ++MonsterIndex)
		{
			Model.SelectMonster(MonsterIndex);
			for (int32 AbilityIndex = 0; AbilityIndex < Model.GetAbilities().Num(); ++AbilityIndex)
			{
				Model.SelectAbility(AbilityIndex);
				if (!Model.GetMontage() || Model.GetRateWindows().IsEmpty())
				{
					continue;
				}
				auto Log = [&Model](const TCHAR* Step)
				{
					UE_LOG(LogknightAnimCoverage, Display, TEXT("RATE %s %s: windows=%d duration=%.3f hit=%.3f canAlign=%d"), *Model.GetAbility()->Name, Step,
						Model.GetRateWindows().Num(), Model.GetDuration(), Model.GetMontageHitTime(), Model.CanAlignHitPhase() ? 1 : 0);
				};
				Log(TEXT("before"));
				const FGuid Added = Model.AddRateWindow(0.1f, 0.5f, 2.f);
				Log(TEXT("added 0.1-0.5 x2"));
				Model.SetRateWindowRate(Added, 0.5f);
				Log(TEXT("rate x0.5"));
				Model.SetRateWindowRange(Added, 0.2f, 0.4f);
				Log(TEXT("range 0.2-0.4"));
				Model.RemoveRateWindow(Added);
				Log(TEXT("removed"));
			}
		}
	}

	const FknightAnimCoverageResult Result = knightAnimCoverage::Gather();

	FString OutPath;
	if (!FParse::Value(*Params, TEXT("Out="), OutPath))
	{
		OutPath = FPaths::ProfilingDir() / TEXT("knightAnimDebug") / FString::Printf(TEXT("AnimCoverage_%s.md"), *FDateTime::Now().ToString(TEXT("%Y%m%d-%H%M%S")));
	}
	OutPath = FPaths::ConvertRelativePathToFull(OutPath);
	if (!FFileHelper::SaveStringToFile(knightAnimCoverage::ToMarkdown(Result), *OutPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		UE_LOG(LogknightAnimCoverage, Error, TEXT("저장하지 못했다: %s"), *OutPath);
		return 2;
	}

	const int32 Errors = Result.CountSeverity(EknightAnimCoverageSeverity::Error);
	UE_LOG(LogknightAnimCoverage, Display, TEXT("몬스터 %d종, 항목 %d개, 주의 %d, 문제 %d. 저장: %s"), Result.MonsterCount, Result.Rows.Num(),
		Result.CountSeverity(EknightAnimCoverageSeverity::Warning), Errors, *OutPath);
	if (!Result.Notice.IsEmpty())
	{
		UE_LOG(LogknightAnimCoverage, Warning, TEXT("%s"), *Result.Notice);
	}
	return FParse::Param(*Params, TEXT("FailOnError")) && Errors > 0 ? 1 : 0;
}
