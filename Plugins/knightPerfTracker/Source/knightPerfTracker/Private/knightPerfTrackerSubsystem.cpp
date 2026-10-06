#include "knightPerfTrackerSubsystem.h"

#include "DynamicRHI.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformMemory.h"
#include "Misc/Paths.h"
#include "Misc/StringBuilder.h"
#include "ProfilingDebugging/MiscTrace.h"
#include "RenderTimer.h"
#include "UnrealClient.h"
#include "knightPerfCounters.h"
#include "knightPerfReport.h"
#include "knightPerfTrackerModule.h"

namespace knightPerfTracker
{
	static TAutoConsoleVariable<float> CVarHitchMs(
		TEXT("knight.Perf.HitchMs"),
		50.f,
		TEXT("Frames longer than this many milliseconds count as hitches in knight.Perf captures."));

	constexpr uint32 MemorySampleInterval = 30;
	constexpr uint64 OverlayMessageKey = 0x6B6E5066ull; // "knPf"
	constexpr int32 OverlayMaxCounters = 16;

	static double BytesToMB(uint64 Bytes)
	{
		return static_cast<double>(Bytes) / (1024.0 * 1024.0);
	}
}

UknightPerfTrackerSubsystem::UknightPerfTrackerSubsystem()
{
}

bool UknightPerfTrackerSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	return KNIGHT_PERF_ENABLED && Super::ShouldCreateSubsystem(Outer);
}

bool UknightPerfTrackerSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UknightPerfTrackerSubsystem::Deinitialize()
{
	// PIE를 끝내거나 다른 맵으로 이동해도 그때까지 기록한 내용은 저장한다.
	if (bCapturing)
	{
		StopCapture();
	}
	Super::Deinitialize();
}

TStatId UknightPerfTrackerSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UknightPerfTrackerSubsystem, STATGROUP_Tickables);
}

bool UknightPerfTrackerSubsystem::IsTickable() const
{
	return bCapturing || bOverlayEnabled;
}

void UknightPerfTrackerSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// 실제 시간을 쓰므로 히트 스톱, 슬로 모션, 일시정지가 프레임 비용을 왜곡하지 않는다.
	const double Now = FPlatformTime::Seconds();
	const float FrameMs = LastTickSeconds > 0.0 ? static_cast<float>((Now - LastTickSeconds) * 1000.0) : DeltaTime * 1000.f;
	LastTickSeconds = Now;
	SmoothedFrameMs = SmoothedFrameMs > 0.f ? FMath::Lerp(SmoothedFrameMs, FrameMs, 0.1f) : FrameMs;

	// GetStats는 OS에 질의하므로 매 프레임이 아니라 몇 프레임마다 한 번씩 실행한다.
	if (LastMemoryMB <= 0.f || ++FramesSinceMemorySample >= knightPerfTracker::MemorySampleInterval)
	{
		LastMemoryMB = static_cast<float>(knightPerfTracker::BytesToMB(FPlatformMemory::GetStats().UsedPhysical));
		FramesSinceMemorySample = 0;
	}

	if (bCapturing && Now >= SamplingStartSeconds)
	{
		SampleFrame(Now, FrameMs);
		if (CaptureDuration > 0.0 && Now - SamplingStartSeconds >= CaptureDuration)
		{
			StopCapture();
		}
	}

	if (bOverlayEnabled)
	{
		DrawOverlay(FrameMs);
	}

	FknightPerfCounters::EndFrame();
}

void UknightPerfTrackerSubsystem::StartCapture(const FString& Label, float DurationSeconds, float WarmupSeconds)
{
	if (bCapturing)
	{
		StopCapture();
	}
	if (!bOverlayEnabled)
	{
		// 지금까지 틱하지 않았으므로 프레임 시계를 다시 시작하고, 쉬는 동안 쌓인 카운터 값을 버린다.
		LastTickSeconds = 0.0;
		FknightPerfCounters::EndFrame();
	}

	if (!Capture.IsValid())
	{
		Capture = MakeShared<FknightPerfCapture>();
	}
	Capture->Reset();
	Capture->Label = FPaths::MakeValidFileName(Label.IsEmpty() ? FString(TEXT("Capture")) : Label, TEXT('_'));
	Capture->MapName = GetWorld() ? UWorld::RemovePIEPrefix(GetWorld()->GetMapName()) : FString();
	Capture->StartTime = FDateTime::Now();
	Capture->HitchThresholdMs = knightPerfTracker::CVarHitchMs.GetValueOnGameThread();
	Capture->Frames.Reserve(DurationSeconds > 0.f ? FMath::CeilToInt32(DurationSeconds * 144.f) : 8192);

	const double Now = FPlatformTime::Seconds();
	CaptureStartSeconds = Now;
	SamplingStartSeconds = Now + FMath::Max(0.f, WarmupSeconds);
	CaptureDuration = FMath::Max(0.f, DurationSeconds);
	bCapturing = true;

	TRACE_BOOKMARK(TEXT("knightPerf Start %s"), *Capture->Label);
	UE_LOG(LogknightPerf, Display, TEXT("Capture '%s' started on %s (duration %s, warm-up %.1f s)."),
		*Capture->Label, *Capture->MapName,
		CaptureDuration > 0.0 ? *FString::Printf(TEXT("%.1f s"), CaptureDuration) : TEXT("until Stop"), FMath::Max(0.f, WarmupSeconds));
}

FString UknightPerfTrackerSubsystem::StopCapture()
{
	if (!bCapturing || !Capture.IsValid())
	{
		return FString();
	}
	bCapturing = false;
	TRACE_BOOKMARK(TEXT("knightPerf Stop %s"), *Capture->Label);

	if (Capture->Frames.IsEmpty())
	{
		UE_LOG(LogknightPerf, Warning, TEXT("Capture '%s' stopped before any frame was recorded (still in warm-up?)."), *Capture->Label);
		Capture->Reset();
		return FString();
	}

	const FString SummaryPath = knightPerfReport::Write(*Capture);
	Capture->Frames.Empty();
	Capture->CounterValues.Empty();
	return SummaryPath;
}

void UknightPerfTrackerSubsystem::SetOverlayEnabled(bool bEnabled)
{
	if (bEnabled && !IsTickable())
	{
		LastTickSeconds = 0.0;
		FknightPerfCounters::EndFrame();
	}
	bOverlayEnabled = bEnabled;
}

UknightPerfTrackerSubsystem* UknightPerfTrackerSubsystem::Get(const UWorld* World)
{
	return World ? World->GetSubsystem<UknightPerfTrackerSubsystem>() : nullptr;
}

void UknightPerfTrackerSubsystem::SampleFrame(double Now, float FrameMs)
{
	// Start 시점이 아니라 첫 샘플 프레임에서 읽는다. Standalone에서 BeginPlay에서 시작한 측정은
	// 게임 뷰포트 크기가 정해지기 전에 실행되기 때문이다.
	if (Capture->ViewportSize == FIntPoint::ZeroValue)
	{
		const UGameViewportClient* const GameViewport = GetWorld() ? GetWorld()->GetGameViewport() : nullptr;
		if (GameViewport && GameViewport->Viewport)
		{
			Capture->ViewportSize = GameViewport->Viewport->GetSizeXY();
		}
	}

	FknightPerfFrameSample& Sample = Capture->Frames.AddDefaulted_GetRef();
	Sample.Time = Now - SamplingStartSeconds;
	Sample.FrameMs = FrameMs;
	// 엔진 스레드 시간은 stat unit과 마찬가지로 가장 최근에 끝난 프레임의 값이다.
	Sample.GameMs = FPlatformTime::ToMilliseconds(GGameThreadTime);
	Sample.RenderMs = FPlatformTime::ToMilliseconds(GRenderThreadTime);
	Sample.RHIMs = FPlatformTime::ToMilliseconds(GRHIThreadTime);
	Sample.GpuMs = FPlatformTime::ToMilliseconds(RHIGetGPUFrameCycles(0));
	Sample.MemoryMB = LastMemoryMB;

	const int32 NumCounters = FknightPerfCounters::Num();
	Sample.CounterOffset = Capture->CounterValues.Num();
	Sample.CounterCount = NumCounters;
	for (int32 Index = 0; Index < NumCounters; ++Index)
	{
		Capture->CounterValues.Add(FknightPerfCounters::GetValue(Index));
	}
}

void UknightPerfTrackerSubsystem::DrawOverlay(float FrameMs) const
{
	if (!GEngine)
	{
		return;
	}

	TStringBuilder<1024> Text;
	Text.Appendf(TEXT("[knightPerf] %.1f fps  %.2f ms  (Game %.2f  Render %.2f  RHI %.2f  GPU %.2f)  Mem %.0f MB"),
		SmoothedFrameMs > 0.f ? 1000.f / SmoothedFrameMs : 0.f, SmoothedFrameMs,
		FPlatformTime::ToMilliseconds(GGameThreadTime), FPlatformTime::ToMilliseconds(GRenderThreadTime),
		FPlatformTime::ToMilliseconds(GRHIThreadTime), FPlatformTime::ToMilliseconds(RHIGetGPUFrameCycles(0)), LastMemoryMB);

	if (bCapturing && Capture.IsValid())
	{
		const double Now = FPlatformTime::Seconds();
		if (Now < SamplingStartSeconds)
		{
			Text.Appendf(TEXT("\nREC %s  warm-up %.1f s"), *Capture->Label, SamplingStartSeconds - Now);
		}
		else
		{
			Text.Appendf(TEXT("\nREC %s  %.1f s  %d frames"), *Capture->Label, Now - SamplingStartSeconds, Capture->Frames.Num());
		}
	}

	int32 Shown = 0;
	const int32 NumCounters = FknightPerfCounters::Num();
	for (int32 Index = 0; Index < NumCounters && Shown < knightPerfTracker::OverlayMaxCounters; ++Index)
	{
		const EknightPerfCounterMode Mode = FknightPerfCounters::GetMode(Index);
		const double Value = FknightPerfCounters::GetValue(Index);
		if (Mode != EknightPerfCounterMode::Gauge && Value == 0.0)
		{
			continue;
		}
		// Appendf는 리터럴 포맷 문자열만 받으므로 모드마다 따로 호출한다.
		const FString Name = FknightPerfCounters::GetName(Index).ToString();
		if (Mode == EknightPerfCounterMode::TimeMs)
		{
			Text.Appendf(TEXT("\n  %s  %.3f ms"), *Name, Value);
		}
		else
		{
			Text.Appendf(TEXT("\n  %s  %.0f"), *Name, Value);
		}
		++Shown;
	}

	const FColor Color = FrameMs > knightPerfTracker::CVarHitchMs.GetValueOnGameThread() ? FColor::Orange : FColor::Green;
	GEngine->AddOnScreenDebugMessage(knightPerfTracker::OverlayMessageKey, 0.f, Color, FString(Text.ToView()));
}

#if KNIGHT_PERF_ENABLED

namespace knightPerfTracker
{
	/** 명령의 월드. PIE 중 에디터 Output Log에서 입력한 경우에는 아무 게임 월드나 찾는다. */
	static UknightPerfTrackerSubsystem* FindForCommand(UWorld* World)
	{
		if (UknightPerfTrackerSubsystem* const Tracker = UknightPerfTrackerSubsystem::Get(World))
		{
			return Tracker;
		}
		if (GEngine)
		{
			for (const FWorldContext& Context : GEngine->GetWorldContexts())
			{
				if (UknightPerfTrackerSubsystem* const Tracker = UknightPerfTrackerSubsystem::Get(Context.World()))
				{
					return Tracker;
				}
			}
		}
		UE_LOG(LogknightPerf, Warning, TEXT("No game world is running. Use knight.Perf commands during play (PIE or Standalone)."));
		return nullptr;
	}

	static void StartCommand(const TArray<FString>& Args, UWorld* World)
	{
		if (UknightPerfTrackerSubsystem* const Tracker = FindForCommand(World))
		{
			const FString Label = Args.IsValidIndex(0) ? Args[0] : FString();
			const float Duration = Args.IsValidIndex(1) ? FCString::Atof(*Args[1]) : 0.f;
			const float Warmup = Args.IsValidIndex(2) ? FCString::Atof(*Args[2]) : 0.f;
			Tracker->StartCapture(Label, Duration, Warmup);
		}
	}

	static void StopCommand(const TArray<FString>& Args, UWorld* World)
	{
		if (UknightPerfTrackerSubsystem* const Tracker = FindForCommand(World))
		{
			if (!Tracker->IsCapturing())
			{
				UE_LOG(LogknightPerf, Warning, TEXT("No capture is running."));
				return;
			}
			Tracker->StopCapture();
		}
	}

	static void OverlayCommand(const TArray<FString>& Args, UWorld* World)
	{
		if (UknightPerfTrackerSubsystem* const Tracker = FindForCommand(World))
		{
			Tracker->SetOverlayEnabled(Args.IsValidIndex(0) ? FCString::Atoi(*Args[0]) != 0 : !Tracker->IsOverlayEnabled());
		}
	}

	static void CompareCommand(const TArray<FString>& Args)
	{
		if (Args.Num() < 2)
		{
			UE_LOG(LogknightPerf, Warning, TEXT("Usage: knight.Perf.Compare <Before> <After>  (summary paths or capture labels)"));
			return;
		}
		const FString Before = knightPerfReport::ResolveSummary(Args[0]);
		const FString After = knightPerfReport::ResolveSummary(Args[1]);
		if (Before.IsEmpty() || After.IsEmpty())
		{
			UE_LOG(LogknightPerf, Warning, TEXT("No summary found for '%s'. Captures are in %s"),
				Before.IsEmpty() ? *Args[0] : *Args[1], *knightPerfReport::GetOutputDirectory());
			return;
		}
		knightPerfReport::Compare(Before, After);
	}

	static FAutoConsoleCommandWithWorldAndArgs StartCmd(
		TEXT("knight.Perf.Start"),
		TEXT("knight.Perf.Start [Label] [Seconds] [WarmupSeconds]. Seconds 0 records until knight.Perf.Stop."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&StartCommand));

	static FAutoConsoleCommandWithWorldAndArgs StopCmd(
		TEXT("knight.Perf.Stop"),
		TEXT("Stops the running capture and writes CSV + summary to Saved/Profiling/knightPerf."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&StopCommand));

	static FAutoConsoleCommandWithWorldAndArgs OverlayCmd(
		TEXT("knight.Perf.Overlay"),
		TEXT("knight.Perf.Overlay [0|1]. Shows frame timings and active counters on screen; no argument toggles."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&OverlayCommand));

	static FAutoConsoleCommand CompareCmd(
		TEXT("knight.Perf.Compare"),
		TEXT("knight.Perf.Compare <Before> <After>. Summary paths or labels (newest summary per label); writes a Markdown table."),
		FConsoleCommandWithArgsDelegate::CreateStatic(&CompareCommand));
}

#endif
