#pragma once

#include "CoreMinimal.h"

/** One sampled frame. Counter values live in FknightPerfCapture::CounterValues at [CounterOffset, CounterOffset + CounterCount). */
struct FknightPerfFrameSample
{
	/** Seconds since sampling started (after warm-up). */
	double Time = 0.0;
	float FrameMs = 0.f;
	float GameMs = 0.f;
	float RenderMs = 0.f;
	float RHIMs = 0.f;
	float GpuMs = 0.f;
	float MemoryMB = 0.f;
	int32 CounterOffset = 0;
	int32 CounterCount = 0;
};

struct FknightPerfCapture
{
	FString Label;
	FString MapName;
	FDateTime StartTime;
	/** GPU and render costs scale with resolution, so comparisons warn when this differs. */
	FIntPoint ViewportSize = FIntPoint::ZeroValue;
	double HitchThresholdMs = 50.0;
	TArray<FknightPerfFrameSample> Frames;
	/** Flattened per-frame counter values. Counters registered mid-capture only appear in later frames. */
	TArray<double> CounterValues;

	void Reset()
	{
		Label.Reset();
		MapName.Reset();
		ViewportSize = FIntPoint::ZeroValue;
		Frames.Reset();
		CounterValues.Reset();
	}
};

/** File output for captures. Everything goes to <Project>/Saved/Profiling/knightPerf. */
namespace knightPerfReport
{
	FString GetOutputDirectory();

	/** Writes <Label>_<yyyyMMdd-HHmmss>.csv and .summary.json and logs the summary. Returns the summary path, empty on failure. */
	FString Write(const FknightPerfCapture& Capture);

	/** Accepts a summary file path, or a label (picks that label's newest summary). Empty when nothing matches. */
	FString ResolveSummary(const FString& PathOrLabel);

	/** Logs a before/after table and writes it as Markdown (ready for a PR description). Returns the Markdown path. */
	FString Compare(const FString& BeforeSummaryPath, const FString& AfterSummaryPath);
}
