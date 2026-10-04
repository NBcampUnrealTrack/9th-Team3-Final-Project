#include "knightPerfCounters.h"

#include "Misc/ScopeLock.h"
#include "knightPerfTrackerModule.h"

#include <atomic>

namespace knightPerfCounters
{
	struct FSlot
	{
		FName Name;
		EknightPerfCounterMode Mode = EknightPerfCounterMode::PerFrame;
		double Value = 0.0;
		double LastFrameValue = 0.0;
	};

	// Fixed storage so registering never moves a slot another call site is writing to.
	static FSlot Slots[FknightPerfCounters::MaxCounters];
	static std::atomic<int32> NumSlots{0};
	static FCriticalSection RegisterLock;

	static bool IsWritable(int32 Index)
	{
		return Index >= 0 && Index < NumSlots.load(std::memory_order_acquire) && IsInGameThread();
	}
}

int32 FknightPerfCounters::Register(const TCHAR* Name, EknightPerfCounterMode Mode)
{
	using namespace knightPerfCounters;

	const FName CounterName(Name);
	FScopeLock Lock(&RegisterLock);

	const int32 Count = NumSlots.load(std::memory_order_relaxed);
	for (int32 Index = 0; Index < Count; ++Index)
	{
		if (Slots[Index].Name == CounterName)
		{
			return Index;
		}
	}
	if (Count >= MaxCounters)
	{
		UE_LOG(LogknightPerf, Warning, TEXT("Counter limit (%d) reached; '%s' is not tracked."), MaxCounters, Name);
		return INDEX_NONE;
	}

	Slots[Count].Name = CounterName;
	Slots[Count].Mode = Mode;
	Slots[Count].Value = 0.0;
	Slots[Count].LastFrameValue = 0.0;
	NumSlots.store(Count + 1, std::memory_order_release);
	return Count;
}

void FknightPerfCounters::Add(int32 Index, double Value)
{
	if (knightPerfCounters::IsWritable(Index))
	{
		knightPerfCounters::Slots[Index].Value += Value;
	}
}

void FknightPerfCounters::Set(int32 Index, double Value)
{
	if (knightPerfCounters::IsWritable(Index))
	{
		knightPerfCounters::Slots[Index].Value = Value;
	}
}

int32 FknightPerfCounters::Num()
{
	return knightPerfCounters::NumSlots.load(std::memory_order_acquire);
}

FName FknightPerfCounters::GetName(int32 Index)
{
	return Index >= 0 && Index < Num() ? knightPerfCounters::Slots[Index].Name : NAME_None;
}

EknightPerfCounterMode FknightPerfCounters::GetMode(int32 Index)
{
	return Index >= 0 && Index < Num() ? knightPerfCounters::Slots[Index].Mode : EknightPerfCounterMode::PerFrame;
}

double FknightPerfCounters::GetValue(int32 Index)
{
	return Index >= 0 && Index < Num() ? knightPerfCounters::Slots[Index].Value : 0.0;
}

double FknightPerfCounters::GetLastFrameValue(int32 Index)
{
	return Index >= 0 && Index < Num() ? knightPerfCounters::Slots[Index].LastFrameValue : 0.0;
}

void FknightPerfCounters::EndFrame()
{
	const int32 Count = Num();
	for (int32 Index = 0; Index < Count; ++Index)
	{
		knightPerfCounters::FSlot& Slot = knightPerfCounters::Slots[Index];
		Slot.LastFrameValue = Slot.Value;
		if (Slot.Mode != EknightPerfCounterMode::Gauge)
		{
			Slot.Value = 0.0;
		}
	}
}
