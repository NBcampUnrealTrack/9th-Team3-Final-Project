#include "knightPerfReport.h"

#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Misc/App.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "knightPerfCounters.h"
#include "knightPerfTrackerModule.h"

namespace knightPerfReport
{
	static const TCHAR* SummarySuffix = TEXT(".summary.json");
	/** yyyyMMdd-HHmmss */
	static constexpr int32 StampLength = 15;

	struct FColumn
	{
		FString Name;
		FString Unit;
		/** INDEX_NONE for the built-in frame metrics. */
		int32 CounterIndex = INDEX_NONE;
		float FknightPerfFrameSample::* Member = nullptr;
	};

	struct FStats
	{
		double Avg = 0.0;
		double P50 = 0.0;
		double P95 = 0.0;
		double P99 = 0.0;
		double Max = 0.0;
	};

	static TArray<FColumn> MakeColumns()
	{
		TArray<FColumn> Columns;
		Columns.Add({TEXT("FrameMs"), TEXT("ms"), INDEX_NONE, &FknightPerfFrameSample::FrameMs});
		Columns.Add({TEXT("GameMs"), TEXT("ms"), INDEX_NONE, &FknightPerfFrameSample::GameMs});
		Columns.Add({TEXT("RenderMs"), TEXT("ms"), INDEX_NONE, &FknightPerfFrameSample::RenderMs});
		Columns.Add({TEXT("RHIMs"), TEXT("ms"), INDEX_NONE, &FknightPerfFrameSample::RHIMs});
		Columns.Add({TEXT("GpuMs"), TEXT("ms"), INDEX_NONE, &FknightPerfFrameSample::GpuMs});
		Columns.Add({TEXT("MemoryMB"), TEXT("MB"), INDEX_NONE, &FknightPerfFrameSample::MemoryMB});

		// Counters keep their registration order, so columns line up across frames.
		const int32 NumCounters = FknightPerfCounters::Num();
		for (int32 Index = 0; Index < NumCounters; ++Index)
		{
			const bool bTime = FknightPerfCounters::GetMode(Index) == EknightPerfCounterMode::TimeMs;
			Columns.Add({FknightPerfCounters::GetName(Index).ToString().Replace(TEXT(","), TEXT(";")), bTime ? TEXT("ms") : TEXT(""), Index, nullptr});
		}
		return Columns;
	}

	static double GetValue(const FknightPerfCapture& Capture, const FknightPerfFrameSample& Sample, const FColumn& Column)
	{
		if (Column.CounterIndex == INDEX_NONE)
		{
			return Sample.*Column.Member;
		}
		// A counter registered after this frame was sampled reads as 0.
		return Column.CounterIndex < Sample.CounterCount ? Capture.CounterValues[Sample.CounterOffset + Column.CounterIndex] : 0.0;
	}

	/** Sorts Values. Nearest-rank percentiles. */
	static FStats ComputeStats(TArray<double>& Values)
	{
		FStats Stats;
		if (Values.IsEmpty())
		{
			return Stats;
		}
		Values.Sort();
		double Sum = 0.0;
		for (const double Value : Values)
		{
			Sum += Value;
		}
		auto Percentile = [&Values](double Fraction)
		{
			const int32 Rank = FMath::Clamp(FMath::CeilToInt32(Fraction * Values.Num()) - 1, 0, Values.Num() - 1);
			return Values[Rank];
		};
		Stats.Avg = Sum / Values.Num();
		Stats.P50 = Percentile(0.50);
		Stats.P95 = Percentile(0.95);
		Stats.P99 = Percentile(0.99);
		Stats.Max = Values.Last();
		return Stats;
	}

	static FString FormatNumber(double Value)
	{
		return FString::Printf(TEXT("%.3f"), Value);
	}

	static TSharedPtr<FJsonObject> LoadSummary(const FString& Path)
	{
		FString Text;
		if (!FFileHelper::LoadFileToString(Text, *Path))
		{
			return nullptr;
		}
		TSharedPtr<FJsonObject> Root;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
		return FJsonSerializer::Deserialize(Reader, Root) ? Root : nullptr;
	}

	static FString FormatChange(double Before, double After)
	{
		if (FMath::IsNearlyZero(Before))
		{
			return FString::Printf(TEXT("%.2f → %.2f"), Before, After);
		}
		return FString::Printf(TEXT("%.2f → %.2f (%+.1f%%)"), Before, After, (After - Before) / Before * 100.0);
	}
}

FString knightPerfReport::GetOutputDirectory()
{
	return FPaths::ProfilingDir() / TEXT("knightPerf");
}

FString knightPerfReport::Write(const FknightPerfCapture& Capture)
{
	const FString Directory = GetOutputDirectory();
	IFileManager::Get().MakeDirectory(*Directory, true);
	const FString BasePath = Directory / FString::Printf(TEXT("%s_%s"), *Capture.Label, *Capture.StartTime.ToString(TEXT("%Y%m%d-%H%M%S")));
	const TArray<FColumn> Columns = MakeColumns();

	// CSV: one row per frame.
	FString Csv;
	Csv.Reserve((Capture.Frames.Num() + 1) * Columns.Num() * 10);
	Csv += TEXT("Time");
	for (const FColumn& Column : Columns)
	{
		Csv += TEXT(",");
		Csv += Column.CounterIndex != INDEX_NONE && !Column.Unit.IsEmpty() ? FString::Printf(TEXT("%s (%s)"), *Column.Name, *Column.Unit) : Column.Name;
	}
	Csv += LINE_TERMINATOR;
	for (const FknightPerfFrameSample& Sample : Capture.Frames)
	{
		Csv += FormatNumber(Sample.Time);
		for (const FColumn& Column : Columns)
		{
			Csv += TEXT(",");
			Csv += FormatNumber(GetValue(Capture, Sample, Column));
		}
		Csv += LINE_TERMINATOR;
	}
	const FString CsvPath = BasePath + TEXT(".csv");
	if (!FFileHelper::SaveStringToFile(Csv, *CsvPath))
	{
		UE_LOG(LogknightPerf, Error, TEXT("Could not write %s"), *CsvPath);
		return FString();
	}

	// Summary.
	double SampledSeconds = 0.0;
	int32 Hitches = 0;
	for (const FknightPerfFrameSample& Sample : Capture.Frames)
	{
		SampledSeconds += Sample.FrameMs / 1000.0;
		Hitches += Sample.FrameMs > Capture.HitchThresholdMs ? 1 : 0;
	}
	const double AverageFps = SampledSeconds > 0.0 ? Capture.Frames.Num() / SampledSeconds : 0.0;

	const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("label"), Capture.Label);
	Root->SetStringField(TEXT("map"), Capture.MapName);
	Root->SetStringField(TEXT("startTime"), Capture.StartTime.ToIso8601());
	Root->SetStringField(TEXT("build"), LexToString(FApp::GetBuildConfiguration()));
	Root->SetBoolField(TEXT("editor"), GIsEditor);
	Root->SetStringField(TEXT("viewport"), FString::Printf(TEXT("%dx%d"), Capture.ViewportSize.X, Capture.ViewportSize.Y));
	Root->SetNumberField(TEXT("frames"), Capture.Frames.Num());
	Root->SetNumberField(TEXT("durationSeconds"), SampledSeconds);
	Root->SetNumberField(TEXT("avgFps"), AverageFps);
	Root->SetNumberField(TEXT("hitches"), Hitches);
	Root->SetNumberField(TEXT("hitchThresholdMs"), Capture.HitchThresholdMs);

	UE_LOG(LogknightPerf, Display, TEXT("Capture '%s' on %s: %d frames, %.1f s, %.1f fps avg, %d hitches (> %.0f ms)"),
		*Capture.Label, *Capture.MapName, Capture.Frames.Num(), SampledSeconds, AverageFps, Hitches, Capture.HitchThresholdMs);
	UE_LOG(LogknightPerf, Display, TEXT("  %-36s %10s %10s %10s %10s %10s"), TEXT("Metric"), TEXT("avg"), TEXT("p50"), TEXT("p95"), TEXT("p99"), TEXT("max"));

	const TSharedRef<FJsonObject> Metrics = MakeShared<FJsonObject>();
	TArray<double> Values;
	Values.Reserve(Capture.Frames.Num());
	for (const FColumn& Column : Columns)
	{
		Values.Reset();
		for (const FknightPerfFrameSample& Sample : Capture.Frames)
		{
			Values.Add(GetValue(Capture, Sample, Column));
		}
		const FStats Stats = ComputeStats(Values);

		const TSharedRef<FJsonObject> Metric = MakeShared<FJsonObject>();
		Metric->SetStringField(TEXT("unit"), Column.Unit);
		Metric->SetNumberField(TEXT("avg"), Stats.Avg);
		Metric->SetNumberField(TEXT("p50"), Stats.P50);
		Metric->SetNumberField(TEXT("p95"), Stats.P95);
		Metric->SetNumberField(TEXT("p99"), Stats.P99);
		Metric->SetNumberField(TEXT("max"), Stats.Max);
		Metrics->SetObjectField(Column.Name, Metric);

		UE_LOG(LogknightPerf, Display, TEXT("  %-36s %10.3f %10.3f %10.3f %10.3f %10.3f"), *Column.Name, Stats.Avg, Stats.P50, Stats.P95, Stats.P99, Stats.Max);
	}
	Root->SetObjectField(TEXT("metrics"), Metrics);

	FString Json;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Json);
	FJsonSerializer::Serialize(Root, Writer);
	const FString SummaryPath = BasePath + SummarySuffix;
	if (!FFileHelper::SaveStringToFile(Json, *SummaryPath))
	{
		UE_LOG(LogknightPerf, Error, TEXT("Could not write %s"), *SummaryPath);
		return FString();
	}

	UE_LOG(LogknightPerf, Display, TEXT("Saved %s and %s"), *FPaths::ConvertRelativePathToFull(CsvPath), *FPaths::GetCleanFilename(SummaryPath));
	return SummaryPath;
}

FString knightPerfReport::ResolveSummary(const FString& PathOrLabel)
{
	const FString Directory = GetOutputDirectory();
	for (const FString& Candidate : {PathOrLabel, Directory / PathOrLabel})
	{
		if (Candidate.EndsWith(TEXT(".json")) && FPaths::FileExists(Candidate))
		{
			return Candidate;
		}
	}

	// A label: newest <Label>_<stamp>.summary.json. The exact length check keeps "Before" from matching "Before_20Mobs".
	TArray<FString> Found;
	IFileManager::Get().FindFiles(Found, *(Directory / (PathOrLabel + TEXT("_*") + SummarySuffix)), true, false);
	const int32 ExpectedLength = PathOrLabel.Len() + 1 + StampLength + FCString::Strlen(SummarySuffix);
	Found.RemoveAll([ExpectedLength](const FString& Name) { return Name.Len() != ExpectedLength; });
	if (Found.IsEmpty())
	{
		return FString();
	}
	Found.Sort();
	return Directory / Found.Last();
}

FString knightPerfReport::Compare(const FString& BeforeSummaryPath, const FString& AfterSummaryPath)
{
	const TSharedPtr<FJsonObject> Before = LoadSummary(BeforeSummaryPath);
	const TSharedPtr<FJsonObject> After = LoadSummary(AfterSummaryPath);
	if (!Before.IsValid() || !After.IsValid())
	{
		UE_LOG(LogknightPerf, Error, TEXT("Could not read %s"), !Before.IsValid() ? *BeforeSummaryPath : *AfterSummaryPath);
		return FString();
	}

	const FString BeforeLabel = Before->GetStringField(TEXT("label"));
	const FString AfterLabel = After->GetStringField(TEXT("label"));

	TArray<FString> Lines;
	Lines.Add(FString::Printf(TEXT("## Perf: %s vs %s"), *BeforeLabel, *AfterLabel));
	Lines.Add(TEXT(""));
	Lines.Add(TEXT("| | Before | After |"));
	Lines.Add(TEXT("|---|---|---|"));
	for (const TCHAR* Field : {TEXT("label"), TEXT("startTime"), TEXT("map"), TEXT("build"), TEXT("viewport")})
	{
		Lines.Add(FString::Printf(TEXT("| %s | %s | %s |"), Field, *Before->GetStringField(Field), *After->GetStringField(Field)));
	}
	Lines.Add(FString::Printf(TEXT("| editor | %s | %s |"),
		Before->GetBoolField(TEXT("editor")) ? TEXT("yes") : TEXT("no"), After->GetBoolField(TEXT("editor")) ? TEXT("yes") : TEXT("no")));
	Lines.Add(FString::Printf(TEXT("| frames | %d | %d |"),
		static_cast<int32>(Before->GetNumberField(TEXT("frames"))), static_cast<int32>(After->GetNumberField(TEXT("frames")))));
	Lines.Add(FString::Printf(TEXT("| avg fps | %s |  |"), *FormatChange(Before->GetNumberField(TEXT("avgFps")), After->GetNumberField(TEXT("avgFps")))));
	Lines.Add(FString::Printf(TEXT("| hitches (> %.0f ms) | %d | %d |"), Before->GetNumberField(TEXT("hitchThresholdMs")),
		static_cast<int32>(Before->GetNumberField(TEXT("hitches"))), static_cast<int32>(After->GetNumberField(TEXT("hitches")))));

	// Results from different setups are not comparable; say so instead of silently printing deltas.
	for (const TCHAR* Field : {TEXT("map"), TEXT("build"), TEXT("viewport")})
	{
		if (Before->GetStringField(Field) != After->GetStringField(Field))
		{
			Lines.Add(FString::Printf(TEXT("\n> ⚠ %s differs between captures; the numbers are not directly comparable."), Field));
		}
	}
	if (Before->GetBoolField(TEXT("editor")) != After->GetBoolField(TEXT("editor")))
	{
		Lines.Add(TEXT("\n> ⚠ One capture ran inside the editor and the other did not."));
	}

	Lines.Add(TEXT(""));
	Lines.Add(TEXT("Lower is better. Values are before → after (change)."));
	Lines.Add(TEXT(""));
	Lines.Add(TEXT("| Metric | avg | p95 | p99 | max |"));
	Lines.Add(TEXT("|---|---|---|---|---|"));

	const TSharedPtr<FJsonObject>* BeforeMetrics = nullptr;
	const TSharedPtr<FJsonObject>* AfterMetrics = nullptr;
	if (Before->TryGetObjectField(TEXT("metrics"), BeforeMetrics) && After->TryGetObjectField(TEXT("metrics"), AfterMetrics))
	{
		for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : (*BeforeMetrics)->Values)
		{
			const TSharedPtr<FJsonObject>* AfterMetric = nullptr;
			if (!(*AfterMetrics)->TryGetObjectField(Pair.Key, AfterMetric))
			{
				continue;
			}
			const TSharedPtr<FJsonObject> BeforeMetric = Pair.Value->AsObject();
			const FString Unit = BeforeMetric->GetStringField(TEXT("unit"));
			auto Cell = [&BeforeMetric, AfterMetric](const TCHAR* Stat)
			{
				return FormatChange(BeforeMetric->GetNumberField(Stat), (*AfterMetric)->GetNumberField(Stat));
			};
			Lines.Add(FString::Printf(TEXT("| %s%s | %s | %s | %s | %s |"), *Pair.Key,
				Unit.IsEmpty() ? TEXT("") : *FString::Printf(TEXT(" (%s)"), *Unit),
				*Cell(TEXT("avg")), *Cell(TEXT("p95")), *Cell(TEXT("p99")), *Cell(TEXT("max"))));
		}
	}

	const FString Markdown = FString::Join(Lines, LINE_TERMINATOR) + LINE_TERMINATOR;
	for (const FString& Line : Lines)
	{
		UE_LOG(LogknightPerf, Display, TEXT("%s"), *Line);
	}

	const FString OutputPath = GetOutputDirectory() / FString::Printf(TEXT("Compare_%s_vs_%s_%s.md"),
		*BeforeLabel, *AfterLabel, *FDateTime::Now().ToString(TEXT("%Y%m%d-%H%M%S")));
	if (!FFileHelper::SaveStringToFile(Markdown, *OutputPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		UE_LOG(LogknightPerf, Error, TEXT("Could not write %s"), *OutputPath);
		return FString();
	}
	UE_LOG(LogknightPerf, Display, TEXT("Saved %s"), *FPaths::ConvertRelativePathToFull(OutputPath));
	return OutputPath;
}
