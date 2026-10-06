#pragma once

#include "CoreMinimal.h"
#include "HAL/PlatformTime.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"

/** Counters and scopes compile to nothing in Shipping. */
#ifndef KNIGHT_PERF_ENABLED
	#define KNIGHT_PERF_ENABLED !UE_BUILD_SHIPPING
#endif

enum class EknightPerfCounterMode : uint8
{
	/** Summed over a frame and cleared after each sample: ticks, traces, activations. */
	PerFrame,
	/** Milliseconds summed over a frame and cleared after each sample. Written by KNIGHT_PERF_SCOPE. */
	TimeMs,
	/** Keeps its last value until set again: live monster count, candidate count. */
	Gauge,
};

/**
 * Named game counters that UknightPerfTrackerSubsystem samples once per frame into its capture.
 *
 * Use the macros below instead of calling this directly; they cache the index in a function-local static,
 * so a counter costs one registration on first use and an add per call after that.
 *
 * Game thread only. Values written from other threads are dropped, so keep scopes out of animation
 * worker threads and parallel tasks. At most MaxCounters names exist; extra names are ignored with a warning.
 */
class KNIGHTPERFTRACKER_API FknightPerfCounters
{
public:
	static constexpr int32 MaxCounters = 64;

	/** Index for Name, registering it on first use; INDEX_NONE when full. The first call fixes the mode. */
	static int32 Register(const TCHAR* Name, EknightPerfCounterMode Mode);

	static void Add(int32 Index, double Value);
	static void Set(int32 Index, double Value);

	static int32 Num();
	static FName GetName(int32 Index);
	static EknightPerfCounterMode GetMode(int32 Index);
	static double GetValue(int32 Index);

	/**
	 * Value at the last EndFrame, i.e. the whole previous frame for PerFrame and TimeMs counters.
	 * Lets other tools read a complete frame without depending on where they tick relative to the tracker.
	 */
	static double GetLastFrameValue(int32 Index);

	/** Snapshots every counter for GetLastFrameValue, then clears PerFrame and TimeMs counters. Called by the tracker after it samples a frame. */
	static void EndFrame();
};

/** Adds the elapsed milliseconds of its lifetime to a TimeMs counter. */
class FknightPerfScopeTimer
{
public:
	explicit FknightPerfScopeTimer(int32 InIndex)
		: Index(InIndex)
		, StartCycles(FPlatformTime::Cycles64())
	{
	}

	~FknightPerfScopeTimer()
	{
		FknightPerfCounters::Add(Index, FPlatformTime::ToMilliseconds64(FPlatformTime::Cycles64() - StartCycles));
	}

private:
	int32 Index;
	uint64 StartCycles;
};

#if KNIGHT_PERF_ENABLED

/** Adds Value to a per-frame counter. KNIGHT_PERF_COUNT("AI.StateTreeTicks", 1); */
#define KNIGHT_PERF_COUNT(Name, Value) \
	do \
	{ \
		static const int32 KnightPerfCounterIndex = FknightPerfCounters::Register(TEXT(Name), EknightPerfCounterMode::PerFrame); \
		FknightPerfCounters::Add(KnightPerfCounterIndex, (Value)); \
	} while (0)

/** Sets a counter that keeps its value across frames. KNIGHT_PERF_GAUGE("Monster.Alive", Count); */
#define KNIGHT_PERF_GAUGE(Name, Value) \
	do \
	{ \
		static const int32 KnightPerfCounterIndex = FknightPerfCounters::Register(TEXT(Name), EknightPerfCounterMode::Gauge); \
		FknightPerfCounters::Set(KnightPerfCounterIndex, (Value)); \
	} while (0)

/**
 * Times the rest of the enclosing scope into a TimeMs counter and emits an Unreal Insights CPU event of the
 * same name. KNIGHT_PERF_SCOPE("Monster.TargetEvaluator");
 */
#define KNIGHT_PERF_SCOPE(Name) \
	TRACE_CPUPROFILER_EVENT_SCOPE_STR(Name); \
	static const int32 UE_JOIN(KnightPerfScopeIndex_, __LINE__) = FknightPerfCounters::Register(TEXT(Name), EknightPerfCounterMode::TimeMs); \
	const FknightPerfScopeTimer UE_JOIN(KnightPerfScopeTimer_, __LINE__)(UE_JOIN(KnightPerfScopeIndex_, __LINE__))

#else

#define KNIGHT_PERF_COUNT(Name, Value) do {} while (0)
#define KNIGHT_PERF_GAUGE(Name, Value) do {} while (0)
#define KNIGHT_PERF_SCOPE(Name)

#endif
